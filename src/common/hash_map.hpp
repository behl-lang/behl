#pragma once

#include "memory.hpp"
#include "platform.hpp"
#include "vm/value.hpp"

#include <bit>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

namespace behl
{
    struct State;

    template<typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
    struct HashMap
    {
        // Load factor threshold: rehash when size > capacity * load_factor
        static constexpr double kLoadFactor = 0.75;
        static constexpr size_t kMinCapacity = 8;

        // Control byte values
        static constexpr int8_t kEmpty = -128; // 0b10000000
        static constexpr int8_t kDeleted = -2; // 0b11111110

        using KeyValue = std::pair<K, V>;

        int8_t* ctrl_{};
        KeyValue* slots_{};
        size_t size_{};     // Number of occupied entries
        size_t capacity_{}; // Total number of slots
        [[no_unique_address]] Hash hasher_{};
        [[no_unique_address]] Eq eq_{};

        // Helper to extract 7-bit hash from full hash
        static constexpr int8_t h2(size_t hash)
        {
            // Use appropriate shift based on size_t width
            constexpr int shift = sizeof(size_t) == 8 ? 57 : 25;
            return static_cast<int8_t>((hash >> shift) & 0x7F);
        }

        struct iterator
        {
            int8_t* ctrl_;
            int8_t* ctrl_end_;
            KeyValue* slot_;

            iterator(int8_t* ctrl, int8_t* ctrl_end, KeyValue* slot)
                : ctrl_(ctrl)
                , ctrl_end_(ctrl_end)
                , slot_(slot)
            {
                // Skip to first occupied slot
                while (ctrl_ < ctrl_end_)
                {
                    if (*ctrl_ != kEmpty && *ctrl_ != kDeleted)
                    {
                        break;
                    }
                    ++ctrl_;
                    ++slot_;
                }
            }

            iterator& operator++()
            {
                do
                {
                    ++ctrl_;
                    ++slot_;
                } while (ctrl_ < ctrl_end_ && (*ctrl_ == kEmpty || *ctrl_ == kDeleted));
                return *this;
            }

            KeyValue& operator*() const
            {
                return *slot_;
            }

            KeyValue* operator->() const
            {
                return slot_;
            }

            bool operator==(const iterator& other) const
            {
                return ctrl_ == other.ctrl_;
            }

            bool operator!=(const iterator& other) const
            {
                return ctrl_ != other.ctrl_;
            }
        };

        struct const_iterator
        {
            const int8_t* ctrl_;
            const int8_t* ctrl_end_;
            const KeyValue* slot_;

            const_iterator(const int8_t* ctrl, const int8_t* ctrl_end, const KeyValue* slot)
                : ctrl_(ctrl)
                , ctrl_end_(ctrl_end)
                , slot_(slot)
            {
                // Skip to first occupied slot
                while (ctrl_ < ctrl_end_)
                {
                    if (*ctrl_ != kEmpty && *ctrl_ != kDeleted)
                    {
                        break;
                    }
                    ++ctrl_;
                    ++slot_;
                }
            }

            const_iterator& operator++()
            {
                do
                {
                    ++ctrl_;
                    ++slot_;
                } while (ctrl_ < ctrl_end_ && (*ctrl_ == kEmpty || *ctrl_ == kDeleted));
                return *this;
            }

            const KeyValue& operator*() const
            {
                return *slot_;
            }

            const KeyValue* operator->() const
            {
                return slot_;
            }

            bool operator==(const const_iterator& other) const
            {
                return ctrl_ == other.ctrl_;
            }

            bool operator!=(const const_iterator& other) const
            {
                return ctrl_ != other.ctrl_;
            }
        };

        HashMap() = default;

#ifndef NDEBUG
        ~HashMap()
        {
            assert(ctrl_ == nullptr && slots_ == nullptr && "HashMap not destroyed before destruction");
        }
#endif

        void init(State* state, size_t initial_capacity)
        {
            size_ = 0;
            capacity_ = 0;
            ctrl_ = nullptr;
            slots_ = nullptr;

            if (initial_capacity > 0)
            {
                size_t actual_capacity = std::bit_ceil(std::max(initial_capacity, kMinCapacity));
                assert(actual_capacity > 0 && "Capacity overflow in BasicMap initialization");

                capacity_ = actual_capacity;
                ctrl_ = mem_alloc_array<int8_t>(state, capacity_);
                slots_ = mem_alloc_array<KeyValue>(state, capacity_);
                assert(ctrl_ && slots_ && "Memory allocation failed for BasicMap");

                // Initialize all control bytes as empty
                std::memset(ctrl_, kEmpty, capacity_);
                // Note: slots are not constructed yet - they will be constructed on insert
            }
        }

        void destroy(State* state)
        {
            if (slots_)
            {
                // Destroy all constructed elements
                for (size_t i = 0; i < capacity_; ++i)
                {
                    if (ctrl_[i] != kEmpty && ctrl_[i] != kDeleted)
                    {
                        std::destroy_at(&slots_[i].first);
                        std::destroy_at(&slots_[i].second);
                    }
                }
                mem_free_array<KeyValue>(state, slots_, capacity_);
            }
            if (ctrl_)
            {
                mem_free_array<int8_t>(state, ctrl_, capacity_);
            }
            ctrl_ = nullptr;
            slots_ = nullptr;
            size_ = 0;
            capacity_ = 0;
        }

        BEHL_FORCEINLINE size_t size() const
        {
            return size_;
        }

        BEHL_FORCEINLINE size_t capacity() const
        {
            return capacity_;
        }

        BEHL_FORCEINLINE void reserve(State* state, size_t min_capacity)
        {
            if (capacity_ >= min_capacity)
            {
                return;
            }

            size_t new_capacity = std::bit_ceil(std::max(min_capacity, kMinCapacity));
            rehash(state, new_capacity);
        }

        BEHL_FORCEINLINE bool empty() const
        {
            return size_ == 0;
        }

        template<typename KeyType>
        bool contains(KeyType&& key) const
        {
            return find_internal_impl(*this, std::forward<KeyType>(key)) != nullptr;
        }

        template<typename KeyType>
        iterator find(KeyType&& key)
        {
            return find_impl(*this, std::forward<KeyType>(key));
        }

        template<typename KeyType>
        const_iterator find(KeyType&& key) const
        {
            return find_impl(*this, std::forward<KeyType>(key));
        }

        // Insert or update a key-value pair
        // Returns iterator to the inserted/updated element
        template<typename KeyType, typename ValueType>
        iterator insert_or_assign(State* state, KeyType&& key, ValueType&& value)
        {
            // Check if we need to rehash
            if (needs_rehash())
            {
                size_t new_capacity = capacity_ == 0 ? kMinCapacity : capacity_ * 2;
                rehash(state, new_capacity);
            }

            size_t hash = hasher_(key);
            int8_t h2_val = h2(hash);
            size_t mask = capacity_ - 1;
            size_t index = hash & mask;
            size_t start = index;
            size_t first_deleted = capacity_; // Track first deleted slot

            do
            {
                int8_t ctrl = ctrl_[index];

                if (ctrl == kEmpty)
                {
                    // Empty slot - insert here
                    size_t insert_index = (first_deleted < capacity_) ? first_deleted : index;
                    ctrl_[insert_index] = h2_val;
                    std::construct_at(&slots_[insert_index].first, std::forward<KeyType>(key));
                    std::construct_at(&slots_[insert_index].second, std::forward<ValueType>(value));
                    size_++;
                    return iterator(ctrl_ + insert_index, ctrl_ + capacity_, slots_ + insert_index);
                }

                if (ctrl == kDeleted)
                {
                    // Track first deleted slot
                    if (first_deleted == capacity_)
                    {
                        first_deleted = index;
                    }
                }
                else if (ctrl == h2_val && eq_(slots_[index].first, key))
                {
                    // Key exists - update value
                    slots_[index].second = std::forward<ValueType>(value);
                    return iterator(ctrl_ + index, ctrl_ + capacity_, slots_ + index);
                }

                index = (index + 1) & mask;
            } while (index != start);

            // Table is full - shouldn't happen with load factor management
            // Force rehash and retry
            rehash(state, capacity_ * 2);
            return insert_or_assign(state, std::forward<KeyType>(key), std::forward<ValueType>(value));
        }

        // Insert a new key-value pair (does not update if key exists)
        // Returns iterator to the element and bool indicating if insertion took place
        template<typename KeyType, typename ValueType>
        std::pair<iterator, bool> insert(State* state, KeyType&& key, ValueType&& value)
        {
            auto it = find(key);
            if (it != end())
            {
                // Key already exists, don't insert
                return { it, false };
            }

            // Key doesn't exist, insert it
            it = insert_or_assign(state, std::forward<KeyType>(key), std::forward<ValueType>(value));
            return { it, true };
        }

        // Erase by key
        template<typename KeyType>
        void erase(KeyType&& key)
        {
            if (capacity_ == 0 || size_ == 0)
            {
                return;
            }

            size_t hash = hasher_(key);
            int8_t h2_val = h2(hash);
            size_t mask = capacity_ - 1;
            size_t index = hash & mask;
            size_t start = index;

            do
            {
                int8_t ctrl = ctrl_[index];

                if (ctrl == kEmpty)
                {
                    return; // Not found
                }

                if (ctrl == h2_val && eq_(slots_[index].first, key))
                {
                    // Found - destroy and mark as deleted
                    std::destroy_at(&slots_[index].first);
                    std::destroy_at(&slots_[index].second);
                    ctrl_[index] = kDeleted;
                    size_--;
                    return;
                }

                index = (index + 1) & mask;
            } while (index != start);
        }

        void clear()
        {
            if (ctrl_)
            {
                // Destroy all constructed elements
                for (size_t i = 0; i < capacity_; ++i)
                {
                    if (ctrl_[i] != kEmpty && ctrl_[i] != kDeleted)
                    {
                        std::destroy_at(&slots_[i].first);
                        std::destroy_at(&slots_[i].second);
                    }
                }
                std::memset(ctrl_, kEmpty, capacity_);
            }
            size_ = 0;
        }

        BEHL_FORCEINLINE iterator begin()
        {
            return iterator(ctrl_, ctrl_ + capacity_, slots_);
        }

        BEHL_FORCEINLINE iterator end()
        {
            return iterator(ctrl_ + capacity_, ctrl_ + capacity_, slots_ + capacity_);
        }

        BEHL_FORCEINLINE const_iterator begin() const
        {
            return const_iterator(ctrl_, ctrl_ + capacity_, slots_);
        }

        BEHL_FORCEINLINE const_iterator end() const
        {
            return const_iterator(ctrl_ + capacity_, ctrl_ + capacity_, slots_ + capacity_);
        }

        // Rehash when load factor exceeds threshold
        void rehash(State* state, size_t new_capacity)
        {
            assert(new_capacity > 0 && "New capacity must be greater than zero for rehashing");

            int8_t* old_ctrl = ctrl_;
            KeyValue* old_slots = slots_;
            size_t old_capacity = capacity_;

            // Allocate new table
            ctrl_ = mem_alloc_array<int8_t>(state, new_capacity);
            slots_ = mem_alloc_array<KeyValue>(state, new_capacity);
            assert(ctrl_ && slots_ && "Memory allocation failed during BasicMap rehash");

            std::memset(ctrl_, kEmpty, new_capacity);
            capacity_ = new_capacity;
            size_ = 0;

            // Reinsert all entries
            if (old_ctrl)
            {
                for (size_t i = 0; i < old_capacity; ++i)
                {
                    if (old_ctrl[i] != kEmpty && old_ctrl[i] != kDeleted)
                    {
                        // Move the key-value pair to new table
                        const K& key = old_slots[i].first;
                        size_t hash = hasher_(key);
                        int8_t h2_val = h2(hash);
                        size_t mask = capacity_ - 1;
                        size_t index = hash & mask;

                        // Linear probe for empty slot
                        while (ctrl_[index] != kEmpty)
                        {
                            index = (index + 1) & mask;
                        }

                        ctrl_[index] = h2_val;
                        // Move construct into new location
                        std::construct_at(&slots_[index].first, std::move(old_slots[i].first));
                        std::construct_at(&slots_[index].second, std::move(old_slots[i].second));
                        size_++;

                        // Destroy old elements
                        std::destroy_at(&old_slots[i].first);
                        std::destroy_at(&old_slots[i].second);
                    }
                }

                mem_free_array<int8_t>(state, old_ctrl, old_capacity);
                mem_free_array<KeyValue>(state, old_slots, old_capacity);
            }
        }

    private:
        // Internal find that returns KeyValue*
        template<typename TSelf, typename KeyType>
        static auto find_internal_impl(TSelf&& self, KeyType&& key)
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<TSelf>>, const KeyValue*, KeyValue*>
        {
            if (self.capacity_ == 0 || self.size_ == 0)
            {
                return nullptr;
            }

            size_t hash = self.hasher_(key);
            int8_t h2_val = h2(hash);
            size_t mask = self.capacity_ - 1;
            size_t index = hash & mask;
            size_t start = index;

            do
            {
                int8_t ctrl = self.ctrl_[index];

                if (ctrl == kEmpty)
                {
                    // Empty slot - key not found
                    return nullptr;
                }

                if (ctrl == h2_val && self.eq_(self.slots_[index].first, key))
                {
                    // Key found
                    return &self.slots_[index];
                }

                index = (index + 1) & mask;
            } while (index != start);

            return nullptr;
        }

        template<typename TSelf, typename KeyType>
        static auto find_impl(TSelf&& self, KeyType&& key)
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<TSelf>>, const_iterator, iterator>
        {
            auto* kv = find_internal_impl(self, std::forward<KeyType>(key));
            if (kv)
            {
                size_t index = static_cast<size_t>(kv - self.slots_);
                return { self.ctrl_ + index, self.ctrl_ + self.capacity_, self.slots_ + index };
            }

            return self.end();
        }

        BEHL_FORCEINLINE bool needs_rehash() const
        {
            if (capacity_ == 0)
            {
                return true; // Always need to rehash if capacity is 0
            }
            return static_cast<double>(size_) > static_cast<double>(capacity_) * kLoadFactor;
        }
    };

    // Wrapper that automatically passes State* to HashMap operations
    template<typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
    struct AutoHashMap
    {
        using iterator = typename HashMap<K, V, Hash, Eq>::iterator;
        using const_iterator = typename HashMap<K, V, Hash, Eq>::const_iterator;
        using KeyValue = typename HashMap<K, V, Hash, Eq>::KeyValue;

        AutoHashMap(State* state, size_t initial_capacity = 0)
            : state_(state)
        {
            map_.init(state_, initial_capacity);
        }

        AutoHashMap(AutoHashMap&& other) noexcept
            : state_(other.state_)
            , map_(std::move(other.map_))
        {
            other.state_ = nullptr;
        }

        ~AutoHashMap()
        {
            map_.destroy(state_);
        }

        size_t size() const
        {
            return map_.size();
        }

        size_t capacity() const
        {
            return map_.capacity();
        }

        void reserve(size_t min_capacity)
        {
            return map_.reserve(state_, min_capacity);
        }

        bool empty() const
        {
            return map_.empty();
        }

        template<typename KeyType>
        bool contains(KeyType&& key) const
        {
            return map_.contains(std::forward<KeyType>(key));
        }

        template<typename KeyType>
        iterator find(KeyType&& key)
        {
            return map_.find(std::forward<KeyType>(key));
        }

        template<typename KeyType>
        const_iterator find(KeyType&& key) const
        {
            return map_.find(std::forward<KeyType>(key));
        }

        template<typename KeyType, typename ValueType>
        iterator insert_or_assign(KeyType&& key, ValueType&& value)
        {
            return map_.insert_or_assign(state_, std::forward<KeyType>(key), std::forward<ValueType>(value));
        }

        template<typename KeyType, typename ValueType>
        std::pair<iterator, bool> insert(KeyType&& key, ValueType&& value)
        {
            return map_.insert(state_, std::forward<KeyType>(key), std::forward<ValueType>(value));
        }

        template<typename KeyType>
        void erase(KeyType&& key)
        {
            return map_.erase(std::forward<KeyType>(key));
        }

        void clear()
        {
            return map_.clear();
        }

        iterator begin()
        {
            return map_.begin();
        }

        iterator end()
        {
            return map_.end();
        }

        const_iterator begin() const
        {
            return map_.begin();
        }

        const_iterator end() const
        {
            return map_.end();
        }

    private:
        State* state_{ nullptr };
        HashMap<K, V, Hash, Eq> map_;
    };

} // namespace behl
