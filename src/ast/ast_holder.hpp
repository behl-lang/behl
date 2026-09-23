#pragma once

#include "common/vector.hpp"
#include "memory.hpp"

#include <behl/export.hpp>
#include <cstddef>
#include <memory>
#include <string_view>

namespace behl
{
    struct AstNode;
    struct AstString;
    struct State;

    class AstHolder
    {
    public:
        explicit AstHolder(State* state);
        ~AstHolder();

        // Non-copyable, movable
        AstHolder(const AstHolder&) = delete;
        AstHolder& operator=(const AstHolder&) = delete;
        AstHolder(AstHolder&&) noexcept;
        AstHolder& operator=(AstHolder&&) noexcept;

        // Allocate a new node of type T
        template<typename T, typename... Args>
        T* make(Args&&... args)
        {
            static_assert(std::is_trivially_destructible_v<T>,
                "AST nodes must be trivially destructible: AstHolder releases pool memory without running destructors");

            // Allocate memory from pool
            void* mem = allocate(sizeof(T), alignof(T));

            // Construct in-place
            return std::construct_at(static_cast<T*>(mem), std::forward<Args>(args)...);
        }

        // Allocate string node with string data (untracked memory)
        AstString* make_string(std::string_view str);

        State* state() const
        {
            return m_state;
        }

    private:
        struct Pool
        {
            static constexpr size_t kDefaultPoolSize = 64 * 1024; // 64KB

            std::byte* memory;
            size_t offset = 0;
            size_t capacity;
            State* state;

            Pool(State* s, size_t bytes = kDefaultPoolSize)
                : memory(mem_alloc_array<std::byte>(s, bytes))
                , capacity(bytes)
                , state(s)
            {
            }

            Pool(Pool&& other) noexcept
                : memory(other.memory)
                , offset(other.offset)
                , capacity(other.capacity)
                , state(other.state)
            {
                other.memory = nullptr;
            }

            Pool& operator=(Pool&& other) noexcept
            {
                if (this != &other)
                {
                    if (memory)
                    {
                        mem_free_array<std::byte>(state, memory, capacity);
                    }
                    memory = other.memory;
                    offset = other.offset;
                    capacity = other.capacity;
                    state = other.state;
                    other.memory = nullptr;
                }
                return *this;
            }

            ~Pool()
            {
                if (memory)
                {
                    mem_free_array<std::byte>(state, memory, capacity);
                }
            }
        };

        void* allocate(size_t size, size_t alignment);

        State* m_state;
        Vector<Pool> m_pools;
    };

} // namespace behl
