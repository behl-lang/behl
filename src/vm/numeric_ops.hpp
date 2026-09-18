#pragma once

#include "platform/platform.hpp"

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

    BEHL_FORCEINLINE
    constexpr bool try_from_fp(FP value, Integer& out) noexcept
    {
        if (value >= static_cast<FP>(INT64_MIN) && value < -static_cast<FP>(INT64_MIN))
        {
            out = static_cast<Integer>(value);
            return true;
        }
        return false;
    }

    BEHL_FORCEINLINE
    constexpr Integer shl(Integer a, Integer count) noexcept
    {
        if (count >= 64)
        {
            return 0;
        }
        if (count >= 0)
        {
            return static_cast<Integer>(static_cast<UnsignedInteger>(a) << count);
        }
        if (count <= -64)
        {
            return (a < 0) ? -1 : 0;
        }
        return a >> -count;
    }

    BEHL_FORCEINLINE
    constexpr Integer shr(Integer a, Integer count) noexcept
    {
        if (count >= 63)
        {
            return (a < 0) ? -1 : 0;
        }
        if (count >= 0)
        {
            return a >> count;
        }
        if (count <= -64)
        {
            return 0;
        }
        return static_cast<Integer>(static_cast<UnsignedInteger>(a) << -count);
    }

    BEHL_FORCEINLINE
    constexpr Integer mod(Integer a, Integer b) noexcept
    {
        if (b == 0 || b == -1)
        {
            return 0;
        }
        return a % b;
    }

    constexpr Integer pow(Integer base, Integer exp) noexcept
    {
        if (exp < 0)
        {
            if (base == 1)
            {
                return 1;
            }
            if (base == -1)
            {
                return ((exp & 1) != 0) ? -1 : 1;
            }
            return 0;
        }

        Integer result = 1;
        Integer acc = base;
        auto e = static_cast<UnsignedInteger>(exp);

        while (e != 0)
        {
            if ((e & 1) != 0)
            {
                result = mul(result, acc);
            }

            e >>= 1;

            if (e != 0)
            {
                acc = mul(acc, acc);
            }
        }

        return result;
    }

} // namespace behl::int_op

namespace behl::fp_op
{

    FP pow(FP base, FP exp) noexcept;

} // namespace behl::fp_op
