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

    class BEHL_API AstHolder
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
            // Allocate memory from pool
            void* mem = allocate(sizeof(T), alignof(T));

            // Construct in-place
            T* node = std::construct_at(static_cast<T*>(mem), std::forward<Args>(args)...);

            // Track for destruction
            track_node(node);

            return node;
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
            static constexpr size_t POOL_SIZE = 64 * 1024; // 64KB

            std::byte* memory;
            size_t offset = 0;
            State* state;

            Pool(State* s)
                : memory(mem_alloc_array<std::byte>(s, POOL_SIZE))
                , state(s)
            {
            }

            Pool(Pool&& other) noexcept
                : memory(other.memory)
                , offset(other.offset)
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
                        mem_free_array<std::byte>(state, memory, POOL_SIZE);
                    }
                    memory = other.memory;
                    offset = other.offset;
                    state = other.state;
                    other.memory = nullptr;
                }
                return *this;
            }

            ~Pool()
            {
                if (memory)
                {
                    mem_free_array<std::byte>(state, memory, POOL_SIZE);
                }
            }
        };

        void* allocate(size_t size, size_t alignment);
        void track_node(AstNode* node);
        void destroy_all_nodes();

        State* m_state;
        Vector<Pool> m_pools;
        Vector<AstNode*> m_nodes;
    };

} // namespace behl
