#pragma once

#include "platform.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace behl
{

    template<typename T>
    concept StringViewLike = requires(T t) {
        { t.data() } -> std::convertible_to<const char*>;
        { t.size() } -> std::convertible_to<size_t>;
    };

    struct StringHash
    {
        using is_transparent = void;

        template<StringViewLike T>
        constexpr size_t operator()(T&& str) const noexcept
        {
            // FNV-1a hash
            size_t FNV_OFFSET;
            size_t FNV_PRIME;

            if constexpr (sizeof(size_t) == 8)
            {
                FNV_OFFSET = 14695981039346656037ULL;
                FNV_PRIME = 1099511628211ULL;
            }
            else
            {
                FNV_OFFSET = 2166136261U;
                FNV_PRIME = 16777619U;
            }

            const char* ptr = str.data();
            const size_t len = str.size();

            size_t h = FNV_OFFSET;
            for (size_t i = 0; i < len; ++i)
            {
                h ^= static_cast<unsigned char>(ptr[i]);
                h *= FNV_PRIME;
            }

            return h;
        }
    };

    struct StringEq
    {
        using is_transparent = void;

        template<StringViewLike T1, StringViewLike T2>
        constexpr bool operator()(T1&& lhs, T2&& rhs) const noexcept
        {
            const size_t lhs_size = lhs.size();
            const size_t rhs_size = rhs.size();
            if (lhs_size != rhs_size)
            {
                return false;
            }
            return std::memcmp(lhs.data(), rhs.data(), lhs_size) == 0;
        }
    };

} // namespace behl
