#pragma once

#include "config_internal.hpp"
#include "gc/gco_string.hpp"
#include "platform.hpp"

#include <behl/config.hpp>
#include <behl/types.hpp>
#include <cassert>
#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace behl
{
    using CFunction = int (*)(struct State* S);

    struct GCObject;
    struct GCTable;
    struct GCClosure;
    struct GCProto;
    struct UserdataData;

    constexpr uint16_t kTypePairNilNil = (static_cast<uint16_t>(Type::kNil) << 8) | static_cast<uint16_t>(Type::kNil);
    constexpr uint16_t kTypePairIntInt = (static_cast<uint16_t>(Type::kInteger) << 8) | static_cast<uint16_t>(Type::kInteger);
    constexpr uint16_t kTypePairIntFloat = (static_cast<uint16_t>(Type::kInteger) << 8) | static_cast<uint16_t>(Type::kNumber);
    constexpr uint16_t kTypePairFloatInt = (static_cast<uint16_t>(Type::kNumber) << 8) | static_cast<uint16_t>(Type::kInteger);
    constexpr uint16_t kTypePairFloatFloat = (static_cast<uint16_t>(Type::kNumber) << 8) | static_cast<uint16_t>(Type::kNumber);
    constexpr uint16_t kTypePairStringString = (static_cast<uint16_t>(Type::kString) << 8)
        | static_cast<uint16_t>(Type::kString);

    struct Value
    {
        struct Nil
        {
        };

        struct NullOpt
        {
        };

        constexpr Value() noexcept
            : type_{ Type::kNil }
            , gc_object_{}
        {
        }

        constexpr Value(Nil) noexcept
            : type_{ Type::kNil }
            , gc_object_{}
        {
        }

        // To avoid wrapping Value in std::optional<Value>, we provide a NullOpt type.
        constexpr Value(NullOpt) noexcept
            : type_{ Type::kNullOpt }
            , gc_object_{}
        {
        }

        template<typename T, typename = std::enable_if_t<std::is_same_v<T, bool>>>
        explicit constexpr Value(T val) noexcept
            : type_{ Type::kBoolean }
            , bool_{ val }
        {
        }

        constexpr explicit Value(int64_t val) noexcept
            : type_{ Type::kInteger }
            , int_{ val }
        {
        }
        constexpr explicit Value(FP val) noexcept
            : type_{ Type::kNumber }
            , float_{ val }
        {
        }
        constexpr explicit Value(CFunction val) noexcept
            : type_{ Type::kCFunction }
            , cfunction_{ val }
        {
        }
        constexpr explicit Value(GCString* val) noexcept
            : type_{ Type::kString }
            , string_{ val }
        {
        }
        constexpr explicit Value(GCTable* val) noexcept
            : type_{ Type::kTable }
            , table_{ val }
        {
        }
        constexpr explicit Value(GCClosure* val) noexcept
            : type_{ Type::kClosure }
            , closure_{ val }
        {
        }
        constexpr explicit Value(UserdataData* val) noexcept
            : type_{ Type::kUserdata }
            , userdata_{ val }
        {
        }

        BEHL_FORCEINLINE
        std::partial_ordering operator<=>(const Value& other) const noexcept
        {
            const Type lhs_t = type_;
            const Type rhs_t = other.type_;

            if (lhs_t == rhs_t)
            {
                if (lhs_t == Type::kInteger)
                {
                    return get_integer() <=> other.get_integer();
                }

                if (lhs_t == Type::kNumber)
                {
                    return get_fp() <=> other.get_fp();
                }

                if (lhs_t == Type::kString)
                {
                    const auto* l = get_string();
                    const auto* r = other.get_string();
                    return GCString::compare(l, r);
                }

                if (lhs_t == Type::kBoolean)
                {
                    return get_bool() <=> other.get_bool();
                }

                if (lhs_t == Type::kNil) [[unlikely]]
                {
                    return std::partial_ordering::equivalent;
                }

                // Everything else is a GC object → pointer comparison
                assert(is_gcobject() && other.is_gcobject());

                return get_gcobject_ptr() <=> other.get_gcobject_ptr();
            }

            const bool lhs_numeric = (lhs_t == Type::kInteger || lhs_t == Type::kNumber);
            const bool rhs_numeric = (rhs_t == Type::kInteger || rhs_t == Type::kNumber);

            if (lhs_numeric && rhs_numeric)
            {
                FP lhs_fp = lhs_t == Type::kInteger ? static_cast<FP>(get_integer()) : get_fp();
                FP rhs_fp = rhs_t == Type::kInteger ? static_cast<FP>(other.get_integer()) : other.get_fp();
                return lhs_fp <=> rhs_fp;
            }

            // Everything else is unordered
            return std::partial_ordering::unordered;
        }

        BEHL_FORCEINLINE
        bool operator==(const Value& other) const noexcept
        {
            return (*this <=> other) == std::partial_ordering::equivalent;
        }

        BEHL_FORCEINLINE
        bool operator==(const std::string_view str) const noexcept
        {
            if (!is_string())
            {
                return false;
            }

            GCStringEq str_eq;
            return str_eq(get_string(), str);
        }

        template<typename StoredType, typename U>
        BEHL_FORCEINLINE auto emplace(U&& new_value) noexcept
        {
            if constexpr (std::is_same_v<StoredType, bool>)
            {
                type_ = Type::kBoolean;
                bool_ = std::forward<U>(new_value);
            }
            else if constexpr (std::is_same_v<StoredType, Integer>)
            {
                type_ = Type::kInteger;
                int_ = std::forward<U>(new_value);
            }
            else if constexpr (std::is_same_v<StoredType, FP>)
            {
                type_ = Type::kNumber;
                float_ = std::forward<U>(new_value);
            }
            else if constexpr (std::is_same_v<StoredType, GCString*>)
            {
                type_ = Type::kString;
                string_ = std::forward<U>(new_value);
            }
            else if constexpr (std::is_same_v<StoredType, GCTable*>)
            {
                type_ = Type::kTable;
                table_ = std::forward<U>(new_value);
            }
            else if constexpr (std::is_same_v<StoredType, GCClosure*>)
            {
                type_ = Type::kClosure;
                closure_ = std::forward<U>(new_value);
            }
            else if constexpr (std::is_same_v<StoredType, CFunction>)
            {
                type_ = Type::kCFunction;
                cfunction_ = std::forward<U>(new_value);
            }
            else if constexpr (std::is_same_v<StoredType, UserdataData*>)
            {
                type_ = Type::kUserdata;
                userdata_ = std::forward<U>(new_value);
            }
        }

        // Update value without changing type (avoids redundant type assignment)
        template<typename T>
        BEHL_FORCEINLINE constexpr void update(T new_value) noexcept
        {
            if constexpr (std::is_same_v<T, Integer>)
            {
                assert(is_integer() && "update<Integer>: value is not an integer");
                int_ = new_value;
            }
            else if constexpr (std::is_same_v<T, FP>)
            {
                assert(is_fp() && "update<Float>: value is not a float");
                float_ = new_value;
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                assert(is_bool() && "update<bool>: value is not a boolean");
                bool_ = new_value;
            }
            else
            {
                static_assert(std::is_same_v<T, Integer> || std::is_same_v<T, FP> || std::is_same_v<T, bool>,
                    "update() only supports Integer, Float, and bool types");
            }
        }

        BEHL_FORCEINLINE
        constexpr bool has_value() const noexcept
        {
            return type_ != Type::kNullOpt;
        }

        BEHL_FORCEINLINE
        constexpr bool is_nil() const noexcept
        {
            return type_ == Type::kNil;
        }

        BEHL_FORCEINLINE
        constexpr bool is_gcobject() const noexcept
        {
            return (static_cast<uint8_t>(type_) & kGCTypeBit) != 0;
        }

        BEHL_FORCEINLINE
        GCObject* get_gcobject() noexcept
        {
            return gc_object_;
        }

        BEHL_FORCEINLINE
        GCObject* get_gcobject() const noexcept
        {
            return gc_object_;
        }

        BEHL_FORCEINLINE
        uintptr_t get_gcobject_ptr() const noexcept
        {
            return reinterpret_cast<uintptr_t>(gc_object_);
        }

        // Boolean
        BEHL_FORCEINLINE
        constexpr bool is_bool() const noexcept
        {
            return type_ == Type::kBoolean;
        }

        BEHL_FORCEINLINE
        constexpr bool get_bool() const noexcept
        {
            return bool_;
        }

        // Integer
        BEHL_FORCEINLINE
        constexpr bool is_integer() const noexcept
        {
            return type_ == Type::kInteger;
        }
        BEHL_FORCEINLINE
        constexpr Integer get_integer() const noexcept
        {
            return int_;
        }

        // FP
        BEHL_FORCEINLINE
        constexpr bool is_fp() const noexcept
        {
            return type_ == Type::kNumber;
        }

        BEHL_FORCEINLINE
        constexpr FP get_fp() const noexcept
        {
            return float_;
        }

        // String
        BEHL_FORCEINLINE
        constexpr bool is_string() const noexcept
        {
            return type_ == Type::kString;
        }

        BEHL_FORCEINLINE
        constexpr GCString* get_string() noexcept
        {
            return string_;
        }
        BEHL_FORCEINLINE
        constexpr GCString* get_string() const noexcept
        {
            return string_;
        }

        // Table
        BEHL_FORCEINLINE
        constexpr bool is_table() const noexcept
        {
            return type_ == Type::kTable;
        }
        BEHL_FORCEINLINE
        constexpr GCTable* get_table() noexcept
        {
            return table_;
        }
        BEHL_FORCEINLINE
        constexpr GCTable* get_table() const noexcept
        {
            return table_;
        }

        // Closure
        BEHL_FORCEINLINE
        constexpr bool is_closure() const noexcept
        {
            return type_ == Type::kClosure;
        }
        BEHL_FORCEINLINE
        constexpr GCClosure* get_closure() noexcept
        {
            return closure_;
        }
        BEHL_FORCEINLINE
        constexpr GCClosure* get_closure() const noexcept
        {
            return closure_;
        }

        // CFunction
        BEHL_FORCEINLINE
        constexpr bool is_cfunction() const noexcept
        {
            return type_ == Type::kCFunction;
        }
        BEHL_FORCEINLINE
        constexpr CFunction get_cfunction() const
        {
            return cfunction_;
        }

        // Userdata
        BEHL_FORCEINLINE
        constexpr bool is_userdata() const noexcept
        {
            return type_ == Type::kUserdata;
        }
        BEHL_FORCEINLINE
        constexpr UserdataData* get_userdata() noexcept
        {
            return userdata_;
        }
        BEHL_FORCEINLINE
        constexpr UserdataData* get_userdata() const noexcept
        {
            return userdata_;
        }

        BEHL_FORCEINLINE
        constexpr bool is_truthy() const noexcept
        {
            if (is_nil())
            {
                return false;
            }
            if (is_bool())
            {
                return bool_;
            }
            return true;
        }

        BEHL_FORCEINLINE
        constexpr bool is_numeric() const noexcept
        {
            return (static_cast<uint8_t>(type_) & kNumericTypeBit) != 0;
        }

        BEHL_FORCEINLINE
        constexpr bool is_callable() const noexcept
        {
            return (static_cast<uint8_t>(type_) & kCallableTypeBit) != 0;
        }

        BEHL_FORCEINLINE
        constexpr bool is_table_like() const noexcept
        {
            return (static_cast<uint8_t>(type_) & kTableLikeTypeBit) != 0;
        }

        BEHL_FORCEINLINE
        constexpr Type get_type() const noexcept
        {
            return type_;
        }

        BEHL_FORCEINLINE
        constexpr void set_nil() noexcept
        {
            type_ = Type::kNil;
        }

        constexpr std::string_view get_type_string() const noexcept
        {
            return get_type_string(get_type());
        }

        static constexpr std::string_view get_type_string(Type type) noexcept
        {
            switch (type)
            {
                case Type::kNil:
                    return "nil";
                case Type::kBoolean:
                    return "boolean";
                case Type::kInteger:
                    return "integer";
                case Type::kNumber:
                    return "number";
                case Type::kString:
                    return "string";
                case Type::kTable:
                    return "table";
                case Type::kClosure:
                    return "function";
                case Type::kCFunction:
                    return "function";
                case Type::kUserdata:
                    return "userdata";
                case Type::kNullOpt:
                    return "nullopt";
            }
            assert(false && "Unreachable type in Value::get_type_string");
            BEHL_UNREACHABLE();
        }

        size_t hash() const noexcept;

    private:
        Type type_;
        union
        {
            bool bool_;
            Integer int_;
            FP float_;
            GCObject* gc_object_; // All GC types share this
            GCString* string_;
            GCTable* table_;
            GCClosure* closure_;
            UserdataData* userdata_;
            CFunction cfunction_;
        };
    };

    struct ValueHash
    {
        // Enable transparent
        using is_transparent = void;

        size_t operator()(const Value& v) const noexcept;
        size_t operator()(const std::string_view key) const noexcept;
    };

    struct ValueEq
    {
        // Enable transparent
        using is_transparent = void;

        bool operator()(const auto& a, const auto& b) const noexcept
        {
            return a == b;
        }
    };

    // Combine type information into a single value for efficient switching
    // Format: (a_type << 8) | b_type
    BEHL_FORCEINLINE
    constexpr uint16_t make_type_pair(const Value& a, const Value& b) noexcept
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(a.get_type()) << 8) | static_cast<uint16_t>(b.get_type()));
    }

} // namespace behl
