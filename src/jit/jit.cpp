#include "jit.hpp"

#include "jit_helpers.hpp"
#include "state.hpp"
#include "vm/vm.hpp"

#include <cstring>
#include <utility>
#include <vector>

#if BEHL_JIT_SUPPORTED
#    include "jit_compiler.hpp"
#endif
#if BEHL_JIT_X86
#    include "x86/codegen_x86.hpp"
#elif BEHL_JIT_AARCH64
#    include "aarch64/codegen_aarch64.hpp"
#endif

#if BEHL_PLATFORM_WINDOWS
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <Windows.h>
#else
#    include <sys/mman.h>
#    if defined(__APPLE__)
#        include <pthread.h>
#    endif
#endif

namespace behl
{
    struct JitChunk
    {
        uint8_t* base;
        size_t size;
        size_t used;
    };

    struct JitFreeBlock
    {
        uint8_t* ptr;
        size_t size;
    };

    struct JitArena
    {
        std::vector<JitChunk> chunks;
        std::vector<JitFreeBlock> free_blocks;
    };

    static constexpr size_t kJitChunkSize = 64 * 1024;
    static constexpr size_t kJitAllocAlign = 16;
    static constexpr size_t kJitHeaderSize = 16;
    static constexpr size_t kJitMinSplit = 64;

#if BEHL_PLATFORM_WINDOWS && defined(_WIN64)
#    if BEHL_JIT_AARCH64
    static constexpr uintptr_t kNearWindow = uintptr_t{ 128 } << 20;
#    else
    static constexpr uintptr_t kNearWindow = uintptr_t{ 2 } << 30;
#    endif

    static uintptr_t jit_image_base() noexcept
    {
        HMODULE image = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&jit_image_base), &image)
            || image == nullptr)
        {
            return 0;
        }
        return reinterpret_cast<uintptr_t>(image);
    }

    static void* jit_alloc_via_valloc2(uintptr_t base, size_t size) noexcept
    {
        using VirtualAlloc2Fn = PVOID(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);

        static const VirtualAlloc2Fn virtual_alloc2 = []() noexcept -> VirtualAlloc2Fn {
            const HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
            if (kernelbase == nullptr)
            {
                return nullptr;
            }
            return reinterpret_cast<VirtualAlloc2Fn>(GetProcAddress(kernelbase, "VirtualAlloc2"));
        }();

        if (virtual_alloc2 == nullptr)
        {
            return nullptr;
        }

        MEM_ADDRESS_REQUIREMENTS requirements{};
        requirements.LowestStartingAddress = reinterpret_cast<PVOID>(base);
        requirements.HighestEndingAddress = reinterpret_cast<PVOID>(base + kNearWindow - 1);

        MEM_EXTENDED_PARAMETER param{};
        param.Type = MemExtendedParameterAddressRequirements;
        param.Pointer = &requirements;

        return virtual_alloc2(
            GetCurrentProcess(), nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE, &param, 1);
    }

    static void* jit_alloc_via_query_walk(uintptr_t base, size_t size) noexcept
    {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        const uintptr_t granularity = si.dwAllocationGranularity;
        const uintptr_t limit = base + kNearWindow;

        uintptr_t addr = (base + granularity - 1) & ~(granularity - 1);

        while (addr < limit)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0)
            {
                break;
            }

            const auto region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            const uintptr_t region_end = region_base + mbi.RegionSize;

            if (mbi.State == MEM_FREE)
            {
                const uintptr_t candidate = (region_base + granularity - 1) & ~(granularity - 1);
                if (candidate >= addr && candidate + size <= region_end && candidate + size <= limit)
                {
                    void* mem = VirtualAlloc(
                        reinterpret_cast<LPVOID>(candidate), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                    if (mem != nullptr)
                    {
                        return mem;
                    }
                }
            }

            const uintptr_t next = (region_end + granularity - 1) & ~(granularity - 1);
            if (next <= addr)
            {
                break;
            }
            addr = next;
        }

        return nullptr;
    }

    static void* jit_alloc_near_image(size_t size) noexcept
    {
        const uintptr_t base = jit_image_base();
        if (base == 0)
        {
            return nullptr;
        }

        if (void* mem = jit_alloc_via_valloc2(base, size); mem != nullptr)
        {
            return mem;
        }

        return jit_alloc_via_query_walk(base, size);
    }
#endif

    static void* jit_os_alloc(size_t size) noexcept
    {
#if BEHL_PLATFORM_WINDOWS
#    if defined(_WIN64)
        void* near_mem = jit_alloc_near_image(size);
        if (near_mem != nullptr)
        {
            return near_mem;
        }
#    endif
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#    if defined(__APPLE__)
        flags |= MAP_JIT;
#    endif
        void* mem = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);
        return (mem == MAP_FAILED) ? nullptr : mem;
#endif
    }

    static void jit_write_protect([[maybe_unused]] bool executable) noexcept
    {
#if defined(__APPLE__) && defined(__aarch64__)
        pthread_jit_write_protect_np(executable ? 1 : 0);
#endif
    }

    static void jit_os_free(void* mem, [[maybe_unused]] size_t size) noexcept
    {
#if BEHL_PLATFORM_WINDOWS
        VirtualFree(mem, 0, MEM_RELEASE);
#else
        munmap(mem, size);
#endif
    }

    bool jit_supported() noexcept
    {
        return BEHL_JIT_SUPPORTED != 0;
    }

    JitEntry jit_compile(State* S, const GCProto* proto)
    {
#if BEHL_JIT_SUPPORTED
        CgProgram program;
        if (!jit_compile_proto(proto, program))
        {
            return nullptr;
        }
#    if BEHL_JIT_X86
        CodegenX86 backend;
#    else
        CodegenAArch64 backend;
#    endif
        return backend.generate(S, program);
#else
        (void)S;
        (void)proto;
        return nullptr;
#endif
    }

    void jit_release(State* S, JitEntry entry) noexcept
    {
        if (entry == nullptr || S->jit_arena == nullptr)
        {
            return;
        }

        auto* base = reinterpret_cast<uint8_t*>(entry) - kJitHeaderSize;
        size_t total = 0;
        std::memcpy(&total, base, sizeof(total));
        try
        {
            S->jit_arena->free_blocks.push_back(JitFreeBlock{ base, total });
        }
        catch (...)
        {
        }
    }

    void* jit_exec_alloc(State* S, size_t size)
    {
        if (S->jit_arena == nullptr)
        {
            S->jit_arena = new JitArena();
        }
        auto& arena = *S->jit_arena;

        const size_t total = (size + kJitHeaderSize + (kJitAllocAlign - 1)) & ~(kJitAllocAlign - 1);

        for (size_t i = 0; i < arena.free_blocks.size(); ++i)
        {
            JitFreeBlock& block = arena.free_blocks[i];
            if (block.size < total)
            {
                continue;
            }

            uint8_t* base = block.ptr;
            size_t block_total = total;
            if (block.size - total >= kJitMinSplit)
            {
                block.ptr += total;
                block.size -= total;
            }
            else
            {
                block_total = block.size;
                arena.free_blocks[i] = arena.free_blocks.back();
                arena.free_blocks.pop_back();
            }

            jit_write_protect(false);
            std::memcpy(base, &block_total, sizeof(block_total));
            return base + kJitHeaderSize;
        }

        if (arena.chunks.empty() || arena.chunks.back().size - arena.chunks.back().used < total)
        {
            if (!arena.chunks.empty())
            {
                JitChunk& last = arena.chunks.back();
                const size_t tail = last.size - last.used;
                if (tail >= kJitMinSplit)
                {
                    arena.free_blocks.push_back(JitFreeBlock{ last.base + last.used, tail });
                    last.used = last.size;
                }
            }

            const size_t chunk_size = (total > kJitChunkSize) ? total : kJitChunkSize;
            void* mem = jit_os_alloc(chunk_size);
            if (mem == nullptr)
            {
                return nullptr;
            }
            arena.chunks.push_back(JitChunk{ static_cast<uint8_t*>(mem), chunk_size, 0 });
        }

        JitChunk& chunk = arena.chunks.back();
        uint8_t* base = chunk.base + chunk.used;
        chunk.used += total;
        jit_write_protect(false);
        std::memcpy(base, &total, sizeof(total));

        return base + kJitHeaderSize;
    }

    void jit_exec_commit(void* mem, size_t size)
    {
        jit_write_protect(true);
#if BEHL_PLATFORM_WINDOWS
        FlushInstructionCache(GetCurrentProcess(), mem, size);
#else
        __builtin___clear_cache(static_cast<char*>(mem), static_cast<char*>(mem) + size);
#endif
    }

    void jit_shutdown(State* S) noexcept
    {
        if (S->jit_arena == nullptr)
        {
            return;
        }

        for (JitChunk& chunk : S->jit_arena->chunks)
        {
            jit_os_free(chunk.base, chunk.size);
        }

        delete S->jit_arena;
        S->jit_arena = nullptr;
    }

    bool jit_run_or_compile(State* S, const GCProto* proto)
    {
#if BEHL_JIT_SUPPORTED
        if (S->call_stack.size() > kJitMaxCallDepth)
        {
            return false;
        }

        if (proto->jit_code == nullptr)
        {
            if (proto->jit_declined)
            {
                return false;
            }
            JitEntry entry = jit_compile(S, proto);
            if (entry == nullptr)
            {
                proto->jit_declined = true;
                return false;
            }
            proto->jit_code = entry;
        }

        for (;;)
        {
            const uint32_t result = proto->jit_code(S);
            if (result == kJitResultOk)
            {
                return true;
            }
            if (result == kJitResultError)
            {
                std::rethrow_exception(std::exchange(S->jit_exception, nullptr));
            }

            proto = S->call_stack.back().proto;
            if (proto->jit_code != nullptr)
            {
                continue;
            }
            if (!proto->jit_declined)
            {
                if (JitEntry entry = jit_compile(S, proto); entry != nullptr)
                {
                    proto->jit_code = entry;
                    continue;
                }
                proto->jit_declined = true;
            }

            const auto size = static_cast<uint32_t>(S->call_stack.size());
            run_interpreter(S, jit_return_entry_depth(S, S->call_stack.back()), size - 1);
            return true;
        }
#else
        (void)S;
        (void)proto;
        return false;
#endif
    }

} // namespace behl
