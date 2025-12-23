#pragma once

#include "gc_object.hpp"

namespace behl
{
    struct GCTable;

    struct UserdataData : GCObject
    {
        static constexpr auto kObjectType = GCType::kUserdata;

        GCTable* metatable = nullptr;

        void* data = nullptr;
        size_t size = 0;
        uint32_t uid = 0;

        UserdataData()
            : GCObject(kObjectType)
        {
        }
    };

} // namespace behl
