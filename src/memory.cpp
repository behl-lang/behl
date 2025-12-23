#include "memory.hpp"

#include "state.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>

namespace behl
{
    static constexpr size_t kMemoryLimitBytes = ((1024u * 1024u) * 1024u) * 2; // 2 GB

    BEHL_FORCEINLINE static void adjust_gc_bytes(State* S, ptrdiff_t delta)
    {
        assert(S != nullptr);

        int64_t updated = static_cast<int64_t>(S->gc.gc_total_bytes) + delta;
        if (updated < 0)
        {
            updated = 0;
        }

        S->gc.gc_total_bytes = static_cast<size_t>(updated);
        S->gc.gc_debt = static_cast<int64_t>(S->gc.gc_total_bytes) - static_cast<int64_t>(S->gc.gc_threshold);
    }

    BEHL_FORCEINLINE void check_memory_limit(State* S, ptrdiff_t delta)
    {
        assert(S != nullptr);

        if (S->gc.gc_total_bytes + static_cast<size_t>(delta) > kMemoryLimitBytes)
        {
            throw std::bad_alloc();
        }
    }

    void* mem_alloc(State* S, size_t size)
    {
        assert(S != nullptr);

        check_memory_limit(S, static_cast<ptrdiff_t>(size));

        void* ptr = std::malloc(size);
        if (ptr)
        {
            adjust_gc_bytes(S, static_cast<ptrdiff_t>(size));
        }
        return ptr;
    }

    void* mem_realloc(State* S, void* ptr, size_t old_size, size_t new_size)
    {
        assert(S != nullptr);

        if (ptr == nullptr)
        {
            return mem_alloc(S, new_size);
        }

        ptrdiff_t delta = static_cast<ptrdiff_t>(new_size) - static_cast<ptrdiff_t>(old_size);
        check_memory_limit(S, delta);

        void* new_ptr = std::realloc(ptr, new_size);
        if (new_ptr || new_size == 0)
        {
            adjust_gc_bytes(S, delta);
        }

        return new_ptr;
    }

    void mem_free(State* S, void* ptr, size_t size)
    {
        assert(S != nullptr);

        if (ptr)
        {
            std::free(ptr);
            adjust_gc_bytes(S, -static_cast<ptrdiff_t>(size));
        }
    }

} // namespace behl
