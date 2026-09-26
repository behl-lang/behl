#pragma once

#include "gc_object.hpp"

#include <behl/config.hpp>
#include <cstddef>
#include <span>
#include <type_traits>

namespace behl
{
    struct GCBuffer : GCObject
    {
        static constexpr auto kObjectType = GCType::kBuffer;

        GCOHeader header{};

        GCBuffer* owner = nullptr;
        std::byte* data = nullptr;
        SysInt offset = 0;
        SysInt len = 0;

        GCBuffer()
            : header(kObjectType)
        {
        }

        bool is_root() const noexcept
        {
            return owner == this;
        }

        SysInt size() const noexcept
        {
            const SysInt owner_len = owner->len;
            if (offset >= owner_len)
            {
                return 0;
            }
            const SysInt available = owner_len - offset;
            return len < available ? len : available;
        }

        std::span<std::byte> bytes() noexcept
        {
            return { owner->data + offset, size() };
        }
    };

    static_assert(std::is_standard_layout_v<GCBuffer>);
    static_assert(offsetof(GCBuffer, header) == 0);

} // namespace behl
