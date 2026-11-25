#include "platform.hpp"

#if BEHL_PLATFORM_POSIX

#    include <sys/mman.h>

#    if defined(__APPLE__)
#        include <pthread.h>
#    endif

namespace behl::platform
{
    void* exec_alloc(size_t size) noexcept
    {
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#    if defined(__APPLE__)
        flags |= MAP_JIT;
#    endif
        void* mem = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);
        return (mem == MAP_FAILED) ? nullptr : mem;
    }

    void exec_free(void* mem, size_t size) noexcept
    {
        munmap(mem, size);
    }

    void exec_write_protect(bool executable) noexcept
    {
#    if defined(__APPLE__) && defined(__aarch64__)
        pthread_jit_write_protect_np(executable ? 1 : 0);
#    else
        (void)executable;
#    endif
    }

    void exec_flush_icache(void* mem, size_t size) noexcept
    {
        __builtin___clear_cache(static_cast<char*>(mem), static_cast<char*>(mem) + size);
    }
} // namespace behl::platform

#endif
