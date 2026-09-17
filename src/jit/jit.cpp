#include "jit.hpp"

#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "jit_helpers.hpp"
#include "memory.hpp"
#include "state.hpp"
#include "vm/vm.hpp"

#include <cstring>
#include <utility>

#if BEHL_JIT_SUPPORTED
#    include "jit_compiler.hpp"
#endif
#if BEHL_JIT_X86
#    include "x86/codegen_x86.hpp"
#elif BEHL_JIT_AARCH64
#    include "aarch64/codegen_aarch64.hpp"
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
        AutoVector<JitChunk> chunks;
        AutoVector<JitFreeBlock> free_blocks;

        explicit JitArena(State* state)
            : chunks(state)
            , free_blocks(state)
        {
        }
    };

    static constexpr size_t kJitChunkSize = 64 * 1024;
    static constexpr size_t kJitAllocAlign = 16;
    static constexpr size_t kJitHeaderSize = 16;
    static constexpr size_t kJitMinSplit = 64;

    bool jit_supported() noexcept
    {
        return BEHL_JIT_SUPPORTED != 0;
    }

    JitEntry jit_compile(State* S, const GCProto* proto)
    {
#if BEHL_JIT_SUPPORTED
        CgProgram program(S);
        if (!jit_compile_proto(S, proto, program))
        {
            return nullptr;
        }
#    if BEHL_JIT_X86
        CodegenX86 backend(S);
#    else
        CodegenAArch64 backend(S);
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
            S->jit_arena = mem_create<JitArena>(S, S);
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

            platform::exec_write_protect(false);
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
            void* mem = platform::exec_alloc(chunk_size);
            if (mem == nullptr)
            {
                return nullptr;
            }
            arena.chunks.push_back(JitChunk{ static_cast<uint8_t*>(mem), chunk_size, 0 });
        }

        JitChunk& chunk = arena.chunks.back();
        uint8_t* base = chunk.base + chunk.used;
        chunk.used += total;
        platform::exec_write_protect(false);
        std::memcpy(base, &total, sizeof(total));

        return base + kJitHeaderSize;
    }

    void jit_exec_commit(void* mem, size_t size)
    {
        platform::exec_write_protect(true);
        platform::exec_flush_icache(mem, size);
    }

    void jit_clear_cache(State* S) noexcept
    {
#if BEHL_JIT_SUPPORTED
        for (GCObject* obj = S->gc.gc_all_objects.head(); obj != nullptr; obj = obj->get_header().next)
        {
            if (obj->is_proto())
            {
                auto* proto = static_cast<GCProto*>(obj);
                proto->jit_code = nullptr;
                proto->jit_declined = false;
            }
        }

        if (S->jit_depth == 0)
        {
            jit_shutdown(S);
        }
        else
        {
            S->jit_pending_clear = true;
        }
#else
        (void)S;
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
            platform::exec_free(chunk.base, chunk.size);
        }

        mem_destroy(S, S->jit_arena);
        S->jit_arena = nullptr;
    }

    struct JitDepthGuard
    {
        State* state;

        explicit JitDepthGuard(State* S) noexcept
            : state(S)
        {
            ++S->jit_depth;
        }

        ~JitDepthGuard()
        {
            if (--state->jit_depth == 0 && state->jit_pending_clear)
            {
                state->jit_pending_clear = false;
                jit_clear_cache(state);
            }
        }
    };

    bool jit_drive(State* S, const GCProto* proto, uint32_t entry_depth)
    {
#if BEHL_JIT_SUPPORTED
        for (;;)
        {
            const uint32_t result = proto->jit_code(S);
            if (result == kJitResultError)
            {
                std::rethrow_exception(std::exchange(S->jit_exception, nullptr));
            }
            if (result == kJitResultOk && S->call_stack.size() < entry_depth)
            {
                return true;
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
            if (S->call_stack.size() < entry_depth)
            {
                return true;
            }
            proto = S->call_stack.back().proto;
            if (proto->jit_code == nullptr)
            {
                const auto caller_size = static_cast<uint32_t>(S->call_stack.size());
                run_interpreter(S, jit_return_entry_depth(S, S->call_stack.back()), caller_size - 1);
                if (S->call_stack.size() < entry_depth)
                {
                    return true;
                }
            }
        }
#else
        (void)S;
        (void)proto;
        (void)entry_depth;
        return false;
#endif
    }

    bool jit_run_or_compile(State* S, const GCProto* proto)
    {
#if BEHL_JIT_SUPPORTED
        if (S->call_stack.size() > kJitMaxCallDepth)
        {
            return false;
        }

        const JitDepthGuard depth_guard{ S };

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

        return jit_drive(S, proto, static_cast<uint32_t>(S->call_stack.size()));
#else
        (void)S;
        (void)proto;
        return false;
#endif
    }

} // namespace behl
