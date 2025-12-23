#include "ast/ast_holder.hpp"

#include "ast/ast.hpp"

#include <algorithm>

namespace behl
{
    AstHolder::AstHolder(State* state)
        : m_state(state)
    {
        // Start with one pool
        m_pools.emplace_back(m_state);
    }

    AstHolder::~AstHolder()
    {
        destroy_all_nodes();
        m_nodes.destroy(m_state);
        m_pools.destroy(m_state);
    }

    AstHolder::AstHolder(AstHolder&& other) noexcept
        : m_state(other.m_state)
        , m_pools(std::move(other.m_pools))
        , m_nodes(std::move(other.m_nodes))
    {
        other.m_state = nullptr;
    }

    AstHolder& AstHolder::operator=(AstHolder&& other) noexcept
    {
        if (this != &other)
        {
            destroy_all_nodes();
            m_nodes.destroy(m_state);
            m_pools.destroy(m_state);

            m_state = other.m_state;
            m_pools = std::move(other.m_pools);
            m_nodes = std::move(other.m_nodes);

            other.m_state = nullptr;
        }
        return *this;
    }

    void* AstHolder::allocate(size_t size, size_t alignment)
    {
        // Align the current offset
        auto& pool = m_pools.back();
        size_t aligned_offset = (pool.offset + alignment - 1) & ~(alignment - 1);

        // Check if we need a new pool
        if (aligned_offset + size > Pool::POOL_SIZE)
        {
            m_pools.emplace_back(m_state);
            return allocate(size, alignment);
        }

        void* ptr = pool.memory.get() + aligned_offset;
        pool.offset = aligned_offset + size;
        return ptr;
    }

    void AstHolder::track_node(AstNode* node)
    {
        m_nodes.push_back(m_state, node);
    }

    AstString* AstHolder::make_string(std::string_view str)
    {
        // Allocate memory for the string data
        char* data = static_cast<char*>(allocate(str.size(), 1));
        std::copy(str.begin(), str.end(), data);

        // Allocate and construct the AstString node
        void* mem = allocate(sizeof(AstString), alignof(AstString));
        AstString* node = new (mem) AstString(data, str.size());

        // Track for destruction
        track_node(node);

        return node;
    }

    void AstHolder::destroy_all_nodes()
    {
        // Destroy in reverse order (similar to stack unwinding)
        for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it)
        {
            (*it)->~AstNode();
        }
    }

    size_t AstHolder::memory_used() const
    {
        size_t total = 0;
        for (const auto& pool : m_pools)
        {
            total += pool.offset;
        }
        return total;
    }

} // namespace behl
