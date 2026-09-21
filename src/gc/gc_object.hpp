#pragma once

#include "gc_types.hpp"
#include "platform/platform.hpp"

#include <cstdint>

namespace behl
{

    struct GCObject;

    enum class GCOFlags : uint8_t
    {
        kNone = 0,
        kFinalized = 1u << 0,
        kHeapString = 1u << 1,
    };

    struct GCOHeader
    {
        GCType type{};
        GCColor color{};
        GCOFlags flags{};

        uint32_t object_hash{};

        GCObject* next{};
        GCObject* prev{};
        GCObject* gray_next{};

        constexpr GCOHeader() = default;
        constexpr explicit GCOHeader(GCType t)
            : type(t)
        {
        }

        constexpr bool has_flag(GCOFlags flag) const
        {
            return static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag);
        }

        constexpr void add_flag(GCOFlags flag)
        {
            flags = static_cast<GCOFlags>(static_cast<uint8_t>(flags) | static_cast<uint8_t>(flag));
        }

        constexpr void remove_flag(GCOFlags flag)
        {
            flags = static_cast<GCOFlags>(static_cast<uint8_t>(flags) & ~static_cast<uint8_t>(flag));
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
