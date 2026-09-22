#pragma once

#include "platform/platform.hpp"

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

    struct StringHash32
    {
        using is_transparent = void;

        static constexpr uint32_t kFnvOffset = 2166136261U;
        static constexpr uint32_t kFnvPrime = 16777619U;

        template<StringViewLike T>
        constexpr uint32_t operator()(T&& str) const noexcept
        {
            // FNV-1a hash
            const char* ptr = str.data();
            const size_t len = str.size();

            auto h = kFnvOffset;
            for (size_t i = 0; i < len; ++i)
            {
                h ^= static_cast<unsigned char>(ptr[i]);
                h *= kFnvPrime;
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
            if (lhs_size == 0)
            {
                return true;
            }
            return std::memcmp(lhs.data(), rhs.data(), lhs_size) == 0;
        }
    };

} // namespace behl
