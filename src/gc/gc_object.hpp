#pragma once

#include "gc_types.hpp"
#include "platform/platform.hpp"

#include <cstdint>

namespace behl
{

    struct GCObject;

    struct GCOHeader
    {
        GCType type{};
        GCColor color{};
        bool finalized{};

        uint32_t object_hash{};

        GCObject* next{};
        GCObject* prev{};
        GCObject* gray_next{};

        constexpr GCOHeader() = default;
        constexpr explicit GCOHeader(GCType t)
            : type(t)
        {
        }
    };

    struct GCObject
    {
        BEHL_FORCEINLINE
        GCOHeader& get_header() noexcept
        {
            return *reinterpret_cast<GCOHeader*>(this);
        }

        BEHL_FORCEINLINE
        const GCOHeader& get_header() const noexcept
        {
            return *reinterpret_cast<const GCOHeader*>(this);
        }

        BEHL_FORCEINLINE
        bool is_string() const
        {
            return get_header().type == GCType::kString;
        }

        BEHL_FORCEINLINE
        bool is_table() const
        {
            return get_header().type == GCType::kTable;
        }

        BEHL_FORCEINLINE
        bool is_closure() const
        {
            return get_header().type == GCType::kClosure;
        }

        BEHL_FORCEINLINE
        bool is_proto() const
        {
            return get_header().type == GCType::kProto;
        }

        BEHL_FORCEINLINE
        bool is_userdata() const
        {
            return get_header().type == GCType::kUserdata;
        }
    };

} // namespace behl
