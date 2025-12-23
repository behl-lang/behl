#pragma once

#include <behl/export.hpp>
#include <cstddef>
#include <memory>
#include <utility>

namespace behl
{
    struct State;

    BEHL_API void* mem_alloc(State* S, size_t size);

    template<typename T>
    T* mem_alloc_array(State* S, size_t count)
    {
        size_t total_size = sizeof(T) * count;
        T* ptr = static_cast<T*>(mem_alloc(S, total_size));
        return ptr;
    }

    template<typename T>
    T* mem_realloc_array(State* S, T* ptr, size_t old_count, size_t new_count)
    {
        size_t old_size = sizeof(T) * old_count;
        size_t new_size = sizeof(T) * new_count;
        T* new_ptr = static_cast<T*>(mem_realloc(S, ptr, old_size, new_size));
        return new_ptr;
    }

    BEHL_API void* mem_realloc(State* S, void* ptr, size_t old_size, size_t new_size);

    BEHL_API void mem_free(State* S, void* ptr, size_t size);

    template<typename T>
    void mem_free_array(State* S, void* ptr, size_t count)
    {
        size_t total_size = sizeof(T) * count;
        mem_free(S, ptr, total_size);
    }

    template<typename T, typename... Args>
    T* mem_create(State* S, Args&&... args)
    {
        void* mem = mem_alloc(S, sizeof(T));
        return std::construct_at(static_cast<T*>(mem), std::forward<Args>(args)...);
    }

    template<typename T>
    void mem_destroy(State* S, T* ptr)
    {
        std::destroy_at(ptr);
        mem_free(S, ptr, sizeof(T));
    }

} // namespace behl
