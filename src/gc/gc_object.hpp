#pragma once

#include "gc_types.hpp"

#include <cstdint>

namespace behl
{

    struct GCObject
    {
        GCType type{};
        GCColor color{};

        // Only strings use this; it sits in padding that already existed, so
        // GCObject stays 32 bytes and the pointer offsets below are unchanged.
        uint32_t str_hash{};

        GCObject* next{};
        GCObject* prev{};
        GCObject* gray_next{};

        constexpr GCObject() = default;
        constexpr explicit GCObject(GCType t)
            : type(t)
        {
        }

        constexpr bool is_string() const
        {
            return type == GCType::kString;
        }

        constexpr bool is_table() const
        {
            return type == GCType::kTable;
        }

        constexpr bool is_closure() const
        {
            return type == GCType::kClosure;
        }

        constexpr bool is_proto() const
        {
            return type == GCType::kProto;
        }

        constexpr bool is_userdata() const
        {
            return type == GCType::kUserdata;
        }
    };

} // namespace behl
