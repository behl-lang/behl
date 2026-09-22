#include "ast/ast_holder.hpp"

#include "ast/ast.hpp"
#include "memory.hpp"

#include <algorithm>

namespace behl
{
    AstHolder::AstHolder(State* state)
        : m_state(state)
    {
        m_pools.emplace_back(m_state, m_state);
    }

    AstHolder::~AstHolder()
    {
        m_pools.destroy(m_state);
    }

    AstHolder::AstHolder(AstHolder&& other) noexcept
        : m_state(other.m_state)
        , m_pools(std::move(other.m_pools))
    {
        other.m_state = nullptr;
    }

    AstHolder& AstHolder::operator=(AstHolder&& other) noexcept
    {
        if (this != &other)
        {
            m_pools.destroy(m_state);

            m_state = other.m_state;
            m_pools = std::move(other.m_pools);

            other.m_state = nullptr;
        }
        return *this;
    }

    void* AstHolder::allocate(size_t size, size_t alignment)
    {
        // Align the current offset
        Pool* pool = &m_pools.back();
        size_t aligned_offset = (pool->offset + alignment - 1) & ~(alignment - 1);

        // Check if we need a new pool
        if (aligned_offset + size > pool->capacity)
        {
            const size_t needed = size + alignment - 1;
            m_pools.emplace_back(m_state, m_state, std::max(needed, Pool::kDefaultPoolSize));
            pool = &m_pools.back();
            aligned_offset = (pool->offset + alignment - 1) & ~(alignment - 1);
        }

        void* ptr = pool->memory + aligned_offset;
        pool->offset = aligned_offset + size;
        return ptr;
    }

    AstString* AstHolder::make_string(std::string_view str)
    {
        // Allocate memory for the string data
        char* data = static_cast<char*>(allocate(str.size(), 1));
        std::copy(str.begin(), str.end(), data);

        // Allocate and construct the AstString node
        void* mem = allocate(sizeof(AstString), alignof(AstString));
        AstString* node = std::construct_at(static_cast<AstString*>(mem), data, str.size());

        return node;
    }

} // namespace behl
