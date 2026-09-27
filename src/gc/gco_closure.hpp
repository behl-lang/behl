#pragma once

#include "gc_object.hpp"
#include "vm/upvalue.hpp"

#include <cstddef>
#include <type_traits>
#include <vector>

namespace behl
{
    struct GCProto;

    struct GCClosure : GCObject
    {
        static constexpr auto kObjectType = GCType::kClosure;

        GCOHeader header{};

        GCProto* proto{};
        UpvalueIndexVector upvalue_indices{};

        static constexpr int32_t proto_offset()
        {
            return static_cast<int32_t>(offsetof(GCClosure, proto));
        }
    };

    static_assert(std::is_standard_layout_v<GCClosure>);
    static_assert(offsetof(GCClosure, header) == 0);

} // namespace behl
