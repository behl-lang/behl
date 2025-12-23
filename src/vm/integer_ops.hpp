#pragma once

#include "platform.hpp"

#include <behl/types.hpp>
#include <cstdint>
#include <type_traits>

namespace behl::int_op
{

    using UnsignedInteger = std::make_unsigned_t<Integer>;

    BEHL_FORCEINLINE
    constexpr Integer add(Integer a, Integer b) noexcept
    {
        return static_cast<Integer>(static_cast<UnsignedInteger>(a) + static_cast<UnsignedInteger>(b));
    }

    BEHL_FORCEINLINE
    constexpr Integer sub(Integer a, Integer b) noexcept
    {
        return static_cast<Integer>(static_cast<UnsignedInteger>(a) - static_cast<UnsignedInteger>(b));
    }

    BEHL_FORCEINLINE
    constexpr Integer mul(Integer a, Integer b) noexcept
    {
        return static_cast<Integer>(static_cast<UnsignedInteger>(a) * static_cast<UnsignedInteger>(b));
    }

    BEHL_FORCEINLINE
    constexpr Integer neg(Integer a) noexcept
    {
        return static_cast<Integer>(UnsignedInteger{ 0 } - static_cast<UnsignedInteger>(a));
    }

    BEHL_FORCEINLINE
    constexpr Integer inc(Integer a) noexcept
    {
        return static_cast<Integer>(static_cast<UnsignedInteger>(a) + UnsignedInteger{ 1 });
    }

    BEHL_FORCEINLINE
    constexpr Integer dec(Integer a) noexcept
    {
        return static_cast<Integer>(static_cast<UnsignedInteger>(a) - UnsignedInteger{ 1 });
    }

} // namespace behl::int_op
