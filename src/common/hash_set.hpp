#pragma once

#include "hash_map.hpp"

namespace behl
{
    namespace detail
    {
        struct EmptyValue
        {
        };
    } // namespace detail

    template<typename K, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
    struct HashSet
    {
        using Map = HashMap<K, detail::EmptyValue, Hash, Eq>;

        void init(State* state, size_t initial_capacity)
        {
            map_.init(state, initial_capacity);
        }

        void destroy(State* state)
        {
            map_.destroy(state);
        }

        BEHL_FORCEINLINE size_t size() const
        {
            return map_.size();
        }

        BEHL_FORCEINLINE bool empty() const
        {
            return map_.empty();
        }

        template<typename KeyType>
        bool contains(KeyType&& key) const
        {
            return map_.contains(std::forward<KeyType>(key));
        }

        template<typename KeyType>
        void insert(State* state, KeyType&& key)
        {
            map_.insert_or_assign(state, std::forward<KeyType>(key), detail::EmptyValue{});
        }

        template<typename KeyType>
        void erase(KeyType&& key)
        {
            map_.erase(std::forward<KeyType>(key));
        }

        void clear()
        {
            map_.clear();
        }

        struct const_iterator
        {
            typename Map::const_iterator inner;

            const_iterator& operator++()
            {
                ++inner;
                return *this;
            }

            const K& operator*() const
            {
                return inner->first;
            }

            const K* operator->() const
            {
                return &inner->first;
            }

            bool operator==(const const_iterator& other) const
            {
                return inner == other.inner;
            }

            bool operator!=(const const_iterator& other) const
            {
                return inner != other.inner;
            }
        };

        const_iterator begin() const
        {
            return const_iterator{ map_.begin() };
        }

        const_iterator end() const
        {
            return const_iterator{ map_.end() };
        }

    private:
        Map map_;
    };

} // namespace behl
