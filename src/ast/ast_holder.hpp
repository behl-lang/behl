#pragma once

#include "common/vector.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

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
            // Allocate memory from pool
            void* mem = allocate(sizeof(T), alignof(T));

            // Construct in-place
            T* node = new (mem) T(std::forward<Args>(args)...);

            // Track for destruction
            track_node(node);

            return node;
        }

        // Allocate string node with string data in pool
        AstString* make_string(std::string_view str);

        // Get statistics
        size_t node_count() const
        {
            return m_nodes.size();
        }
        size_t memory_used() const;

        State* state() const
        {
            return m_state;
        }

    private:
        struct Pool
        {
            static constexpr size_t POOL_SIZE = 64 * 1024; // 64KB
            std::unique_ptr<std::byte[]> memory;
            size_t offset = 0;

            Pool()
                : memory(new std::byte[POOL_SIZE])
            {
            }
        };

        void* allocate(size_t size, size_t alignment);
        void track_node(AstNode* node);
        void destroy_all_nodes();

        State* m_state;
        Vector<Pool> m_pools;
        Vector<AstNode*> m_nodes; // Tracked for destruction in reverse order
    };

} // namespace behl
