#pragma once

#include "gc_object.hpp"

#include <cstddef>
#include <type_traits>

namespace behl
{
    struct GCTable;

    struct UserdataData : GCObject
    {
        static constexpr auto kObjectType = GCType::kUserdata;

        GCOHeader header{};

        GCTable* metatable = nullptr;

        void* data = nullptr;
        size_t size = 0;
        uint32_t uid = 0;

        UserdataData()
            : header(kObjectType)
        {
        }
    };

    static_assert(std::is_standard_layout_v<UserdataData>);
    static_assert(offsetof(UserdataData, header) == 0);

} // namespace behl
