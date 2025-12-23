#pragma once

#include "memory.hpp"
#include "platform.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace behl
{
    struct State;

    template<typename T>
    struct Vector
    {
#ifndef NDEBUG
        // Avoid realloc optimizations in debug builds to help catch pointer invalidation bugs.
        static constexpr bool kVectorPointerInvalidation = true;
#else
        static constexpr bool kVectorPointerInvalidation = false;
#endif

        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        // Default constructor
        Vector() = default;

        // Move constructor
        BEHL_FORCEINLINE
        Vector(Vector&& other) noexcept
            : data_(other.data_)
            , size_(other.size_)
            , capacity_(other.capacity_)
        {
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }

        // Move assignment
        BEHL_FORCEINLINE
        Vector& operator=(Vector&& other) noexcept
        {
            if (this != &other)
            {
                data_ = other.data_;
                size_ = other.size_;
                capacity_ = other.capacity_;

                other.data_ = nullptr;
                other.size_ = 0;
                other.capacity_ = 0;
            }
            return *this;
        }

        // Delete copy operations since we manage memory manually
        Vector(const Vector&) = delete;
        Vector& operator=(const Vector&) = delete;

#ifndef NDEBUG
        ~Vector()
        {
            assert(data_ == nullptr && "Vector destroyed without calling destroy()");
        }
#endif

        BEHL_FORCEINLINE
        void init(State* state, size_t initial_capacity)
        {
            assert(data_ == nullptr && "Vector already initialized");

            size_ = 0;
            capacity_ = 0;
            data_ = nullptr;

            if (initial_capacity > 0)
            {
                reserve(state, initial_capacity);
            }
        }

        BEHL_FORCEINLINE
        void destroy(State* state)
        {
            assert(state != nullptr && "State can not be null");

            // Call destructors for non-trivial types
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_t i = 0; i < size_; ++i)
                {
                    std::destroy_at(&data_[i]);
                }
            }

            mem_free_array<T>(state, data_, capacity_);
            data_ = nullptr;
            size_ = 0;
            capacity_ = 0;
        }

        BEHL_FORCEINLINE
        size_t size() const
        {
            return size_;
        }

        BEHL_FORCEINLINE
        size_t capacity() const
        {
            return capacity_;
        }

        BEHL_FORCEINLINE
        void reserve(State* state, size_t min_capacity)
        {
            assert(state != nullptr && "State can not be null");

            if (capacity_ >= min_capacity)
            {
                return;
            }

            grow(state, min_capacity);
        }

        BEHL_FORCEINLINE
        void push_back(State* state, const T& value)
        {
            assert(state != nullptr && "State can not be null");

            if (size_ >= capacity_)
            {
                grow(state, capacity_ == 0 ? 4 : capacity_ * 2);
            }

            std::construct_at(&data_[size_++], value);
        }

        BEHL_FORCEINLINE
        void push_back(State* state, T&& value)
        {
            assert(state != nullptr && "State can not be null");

            if (size_ >= capacity_)
            {
                grow(state, capacity_ == 0 ? 4 : capacity_ * 2);
            }

            std::construct_at(&data_[size_++], std::move(value));
        }

        template<typename... Args>
        BEHL_FORCEINLINE T& emplace_back(State* state, Args&&... args)
        {
            assert(state != nullptr && "State can not be null");

            if (size_ >= capacity_)
            {
                grow(state, capacity_ == 0 ? 4 : capacity_ * 2);
            }

            if constexpr (sizeof...(args) > 0 || !std::is_trivially_default_constructible_v<T>)
            {
                std::construct_at(&data_[size_], std::forward<Args>(args)...);
            }
            return data_[size_++];
        }

        BEHL_FORCEINLINE
        void pop_back()
        {
            assert(size_ > 0 && "pop_back on empty vector");
            --size_;

            // Call destructor for non-trivial types
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                std::destroy_at(&data_[size_]);
            }
        }

        BEHL_FORCEINLINE
        void clear()
        {
            // Call destructors for non-trivial types
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_t i = 0; i < size_; ++i)
                {
                    std::destroy_at(&data_[i]);
                }
            }

            size_ = 0;
        }

        BEHL_FORCEINLINE
        bool empty() const
        {
            return size_ == 0;
        }

        BEHL_FORCEINLINE
        T& operator[](size_t index)
        {
            assert(index < size_ && "Index out of bounds");
            return data_[index];
        }

        BEHL_FORCEINLINE
        const T& operator[](size_t index) const
        {
            assert(index < size_ && "Index out of bounds");
            return data_[index];
        }

        BEHL_FORCEINLINE
        T& back()
        {
            assert(size_ > 0 && "back() on empty vector");
            return data_[size_ - 1];
        }

        BEHL_FORCEINLINE
        const T& back() const
        {
            assert(size_ > 0 && "back() on empty vector");
            return data_[size_ - 1];
        }

        BEHL_FORCEINLINE
        T& front()
        {
            assert(size_ > 0 && "front() on empty vector");
            return data_[0];
        }

        BEHL_FORCEINLINE
        const T& front() const
        {
            assert(size_ > 0 && "front() on empty vector");
            return data_[0];
        }

        BEHL_FORCEINLINE
        void resize(State* state, size_t new_size, const T& value = T{})
        {
            if (new_size > capacity_)
            {
                reserve(state, new_size);
            }

            if (new_size > size_)
            {
                // Construct new elements
                for (size_t i = size_; i < new_size; ++i)
                {
                    std::construct_at(&data_[i], value);
                }
            }
            else if (new_size < size_)
            {
                // Destroy removed elements for non-trivial types
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (size_t i = new_size; i < size_; ++i)
                    {
                        std::destroy_at(&data_[i]);
                    }
                }
            }

            size_ = new_size;
        }

        BEHL_FORCEINLINE
        iterator insert(State* state, const_iterator pos, const T& value)
        {
            size_t index = static_cast<size_t>(pos - data_);
            assert(index <= size_ && "insert position out of range");

            if (size_ >= capacity_)
            {
                grow(state, capacity_ == 0 ? 4 : capacity_ * 2);
            }

            // Shift elements right
            for (size_t i = size_; i > index; --i)
            {
                data_[i] = data_[i - 1];
            }

            data_[index] = value;
            ++size_;

            return data_ + index;
        }

        BEHL_FORCEINLINE
        iterator erase(const_iterator pos)
        {
            size_t index = static_cast<size_t>(pos - data_);
            assert(index < size_ && "erase position out of range");

            // Destroy the element being erased for non-trivial types
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                std::destroy_at(&data_[index]);
            }

            // Shift elements left
            for (size_t i = index; i < size_ - 1; ++i)
            {
                if constexpr (std::is_trivially_copyable_v<T>)
                {
                    data_[i] = data_[i + 1];
                }
                else
                {
                    std::construct_at(&data_[i], std::move(data_[i + 1]));
                    if constexpr (!std::is_trivially_destructible_v<T>)
                    {
                        std::destroy_at(&data_[i + 1]);
                    }
                }
            }

            --size_;
            return data_ + index;
        }

        BEHL_FORCEINLINE
        iterator erase(const_iterator first, const_iterator last)
        {
            size_t start_index = static_cast<size_t>(first - data_);
            size_t end_index = static_cast<size_t>(last - data_);
            assert(start_index <= end_index && "invalid erase range");
            assert(end_index <= size_ && "erase range out of bounds");

            if (start_index == end_index)
            {
                return data_ + start_index;
            }

            size_t erase_count = end_index - start_index;

            // Destroy elements being erased for non-trivial types
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_t i = start_index; i < end_index; ++i)
                {
                    std::destroy_at(&data_[i]);
                }
            }

            // Shift remaining elements left
            for (size_t i = start_index; i < size_ - erase_count; ++i)
            {
                if constexpr (std::is_trivially_copyable_v<T>)
                {
                    data_[i] = data_[i + erase_count];
                }
                else
                {
                    std::construct_at(&data_[i], std::move(data_[i + erase_count]));
                    if constexpr (!std::is_trivially_destructible_v<T>)
                    {
                        std::destroy_at(&data_[i + erase_count]);
                    }
                }
            }

            size_ -= erase_count;
            return data_ + start_index;
        }

        BEHL_FORCEINLINE
        T* data() noexcept
        {
            return data_;
        }

        BEHL_FORCEINLINE
        const T* data() const noexcept
        {
            return data_;
        }

        BEHL_FORCEINLINE
        iterator begin() noexcept
        {
            return data_;
        }

        BEHL_FORCEINLINE
        iterator end() noexcept
        {
            return data_ + size_;
        }

        BEHL_FORCEINLINE
        const_iterator begin() const noexcept
        {
            return data_;
        }

        BEHL_FORCEINLINE
        const_iterator end() const noexcept
        {
            return data_ + size_;
        }

        BEHL_FORCEINLINE
        reverse_iterator rbegin() noexcept
        {
            return reverse_iterator(end());
        }

        BEHL_FORCEINLINE
        reverse_iterator rend() noexcept
        {
            return reverse_iterator(begin());
        }

        BEHL_FORCEINLINE
        const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(end());
        }

        BEHL_FORCEINLINE
        const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator(begin());
        }

    private:
        BEHL_FORCEINLINE
        void grow(State* state, size_t min_capacity)
        {
            assert(state != nullptr && "State can not be null");
            assert(capacity_ < min_capacity && "grow called when capacity is sufficient");

            // Calculate next power of 2, with minimum of 4
            size_t new_capacity = std::max(size_t(4), std::bit_ceil(min_capacity));

            T* new_data;

            if constexpr (std::is_trivially_copyable_v<T> && !kVectorPointerInvalidation)
            {
                // For POD types, we can use realloc which is more efficient
                new_data = mem_realloc_array<T>(state, data_, capacity_, new_capacity);
            }
            else
            {
                // For non-POD types, allocate new memory, move/copy construct, and destroy old
                new_data = mem_alloc_array<T>(state, new_capacity);

                // Move or copy existing elements
                if constexpr (std::is_nothrow_move_constructible_v<T>)
                {
                    for (size_t i = 0; i < size_; ++i)
                    {
                        std::construct_at(&new_data[i], std::move(data_[i]));
                    }
                }
                else
                {
                    for (size_t i = 0; i < size_; ++i)
                    {
                        std::construct_at(&new_data[i], data_[i]);
                    }
                }

                // Destroy old elements
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (size_t i = 0; i < size_; ++i)
                    {
                        std::destroy_at(&data_[i]);
                    }
                }

                // Free old memory
                mem_free_array<T>(state, data_, capacity_);
            }

            data_ = new_data;
            capacity_ = new_capacity;
        }

    private:
        T* data_{ nullptr };
        size_t size_{ 0 };
        size_t capacity_{ 0 };
    };

    template<typename T>
    struct AutoVector
    {
        using iterator = typename Vector<T>::iterator;
        using const_iterator = typename Vector<T>::const_iterator;
        using reverse_iterator = typename Vector<T>::reverse_iterator;
        using const_reverse_iterator = typename Vector<T>::const_reverse_iterator;

        AutoVector(State* state, size_t initial_capacity = 0)
            : state_(state)
        {
            vec_.init(state_, initial_capacity);
        }

        AutoVector(AutoVector&& other) noexcept
            : state_(other.state_)
            , vec_(std::move(other.vec_))
        {
            other.state_ = nullptr;
        }

        ~AutoVector()
        {
            if (state_ != nullptr)
            {
                vec_.destroy(state_);
            }
        }

        size_t size() const
        {
            return vec_.size();
        }

        size_t capacity() const
        {
            return vec_.capacity();
        }

        void reserve(size_t min_capacity)
        {
            return vec_.reserve(state_, min_capacity);
        }

        void push_back(const T& value)
        {
            return vec_.push_back(state_, value);
        }

        void push_back(T&& value)
        {
            return vec_.push_back(state_, std::move(value));
        }

        template<typename... Args>
        T& emplace_back(Args&&... args)
        {
            return vec_.emplace_back(state_, std::forward<Args>(args)...);
        }

        void pop_back()
        {
            return vec_.pop_back();
        }

        void clear()
        {
            return vec_.clear();
        }

        bool empty() const
        {
            return vec_.empty();
        }

        T& operator[](size_t index)
        {
            return vec_[index];
        }

        const T& operator[](size_t index) const
        {
            return vec_[index];
        }

        T& back()
        {
            return vec_.back();
        }

        const T& back() const
        {
            return vec_.back();
        }

        T& front()
        {
            return vec_.front();
        }

        const T& front() const
        {
            return vec_.front();
        }

        void resize(size_t new_size, const T& value = T{})
        {
            return vec_.resize(state_, new_size, value);
        }

        auto insert(const_iterator pos, const T& value)
        {
            return vec_.insert(state_, pos, value);
        }

        auto erase(const_iterator pos)
        {
            return vec_.erase(pos);
        }

        auto erase(const_iterator first, const_iterator last)
        {
            return vec_.erase(first, last);
        }

        T* data()
        {
            return vec_.data();
        }

        const T* data() const
        {
            return vec_.data();
        }

        auto begin()
        {
            return vec_.begin();
        }

        auto end()
        {
            return vec_.end();
        }

        auto begin() const
        {
            return vec_.begin();
        }

        auto end() const
        {
            return vec_.end();
        }

        auto rbegin()
        {
            return vec_.rbegin();
        }

        auto rend()
        {
            return vec_.rend();
        }

        auto rbegin() const
        {
            return vec_.rbegin();
        }

        auto rend() const
        {
            return vec_.rend();
        }

    private:
        State* state_{ nullptr };
        Vector<T> vec_;
    };

} // namespace behl
