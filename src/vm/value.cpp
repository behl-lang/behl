#include "value.hpp"

#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gc_types.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"

#include <bit>
#include <limits>
#include <utility>
#include <variant>

namespace behl
{

    static_assert(std::is_trivially_copyable_v<Value>, "Value must be trivially copyable");
    static_assert(std::is_trivially_destructible_v<Value>, "Value must be trivially destructible");
    static_assert(std::is_trivially_copy_constructible_v<Value>, "Value must be trivially copy constructible");
    static_assert(std::is_trivially_move_constructible_v<Value>, "Value must be trivially move constructible");
    static_assert(std::is_trivially_copy_assignable_v<Value>, "Value must be trivially copy assignable");
    static_assert(std::is_trivially_move_assignable_v<Value>, "Value must be trivially move assignable");

    size_t ValueHash::operator()(const Value& v) const noexcept
    {
        return v.hash();
    }

    size_t ValueHash::operator()(const std::string_view key) const noexcept
    {
        StringHash str_hash;
        return str_hash(key);
    }

    static inline uint64_t fmix64(uint64_t k) noexcept
    {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return k;
    }

    BEHL_FORCEINLINE static size_t fold_to_size_t(uint64_t x) noexcept
    {
        if constexpr (sizeof(size_t) == 8)
        {
            return static_cast<size_t>(x);
        }
        else
        {
            return static_cast<size_t>(static_cast<uint32_t>(x) ^ static_cast<uint32_t>(x >> 32));
        }
    }

    BEHL_FORCEINLINE static size_t hash_float(FP x) noexcept
    {
        using bits_type = std::conditional_t<sizeof(FP) == 4, uint32_t, uint64_t>;

        constexpr bits_type exp_mask = sizeof(FP) == 4 ? 0x7F800000u : 0x7FF0000000000000ULL;
        constexpr bits_type nan_canonical = sizeof(FP) == 4 ? 0x7FC00000u : 0x7FF8000000000000ULL;

        // IEEE754
        bits_type u = std::bit_cast<bits_type>(x);

        // normalize ±0.0
        if ((u << 1) == 0)
        {
            u = 0;
        }

        // normalize NaN
        if ((u & exp_mask) == exp_mask && (u & ~exp_mask))
        {
            u = nan_canonical;
        }

        return fold_to_size_t(fmix64(u));
    }

    size_t Value::hash() const noexcept
    {
        switch (type_)
        {
            case Type::kNil:
                return 0;

            case Type::kInteger:
                return fold_to_size_t(fmix64(static_cast<uint64_t>(get_integer())));

            case Type::kNumber:
            {
                // For integer-valued floats, hash as integer to match equality semantics
                // (e.g., 1000.0 == 1000, so they must have the same hash)
                FP f = get_fp();
                Integer i = static_cast<Integer>(f);

                // If the round-trip matches, it's an integer-valued float in range
                if (static_cast<FP>(i) == f)
                {
                    return fold_to_size_t(fmix64(static_cast<uint64_t>(i)));
                }
                return hash_float(f);
            }

            case Type::kBoolean:
                return get_bool() ? 0xA5A5A5A5u : 0;

            case Type::kString:
            {
                auto* val = get_string();
                StringHash str_hash;
                return str_hash(val->view());
            }

            case Type::kClosure:
            case Type::kTable:
            case Type::kUserdata:
            case Type::kCFunction:
            {
                return std::bit_cast<uintptr_t>(gc_object_);
            }

            default:
                return 0;
        }
    }

} // namespace behl
