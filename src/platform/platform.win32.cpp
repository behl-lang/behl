#include "platform.hpp"

#if BEHL_PLATFORM_WINDOWS

#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <Windows.h>
#    include <cstdint>

namespace behl::platform
{
#    if defined(_WIN64)
#        if defined(_M_ARM64)
    static constexpr uintptr_t kNearWindow = uintptr_t{ 128 } << 20;
#        else
    static constexpr uintptr_t kNearWindow = uintptr_t{ 2 } << 30;
#        endif

    static uintptr_t image_base() noexcept
    {
        HMODULE image = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&image_base), &image)
            || image == nullptr)
        {
            return 0;
        }
        return reinterpret_cast<uintptr_t>(image);
    }

    using VirtualAlloc2Fn = PVOID(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);

    static const VirtualAlloc2Fn virtual_alloc2 = []() noexcept -> VirtualAlloc2Fn {
        const HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
        if (kernelbase == nullptr)
        {
            return nullptr;
        }
        return reinterpret_cast<VirtualAlloc2Fn>(reinterpret_cast<void (*)()>(GetProcAddress(kernelbase, "VirtualAlloc2")));
    }();

    static void* alloc_via_valloc2(uintptr_t base, size_t size) noexcept
    {
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

        return virtual_alloc2(GetCurrentProcess(), nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE, &param, 1);
    }

    static void* alloc_via_query_walk(uintptr_t base, size_t size) noexcept
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

    static void* alloc_near_image(size_t size) noexcept
    {
        const uintptr_t base = image_base();
        if (base == 0)
        {
            return nullptr;
        }

        if (void* mem = alloc_via_valloc2(base, size); mem != nullptr)
        {
            return mem;
        }

        return alloc_via_query_walk(base, size);
    }
#    endif

    void* exec_alloc(size_t size) noexcept
    {
#    if defined(_WIN64)
        void* near_mem = alloc_near_image(size);
        if (near_mem != nullptr)
        {
            return near_mem;
        }
#    endif
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }

    void exec_free(void* mem, size_t size) noexcept
    {
        (void)size;
        VirtualFree(mem, 0, MEM_RELEASE);
    }

    void exec_write_protect(bool executable) noexcept
    {
        (void)executable;
    }

    void exec_flush_icache(void* mem, size_t size) noexcept
    {
        FlushInstructionCache(GetCurrentProcess(), mem, size);
    }
} // namespace behl::platform

#endif
