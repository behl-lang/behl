#pragma once

#include "gc_object.hpp"
#include "vm/upvalue.hpp"

#include <vector>

namespace behl
{
    struct GCProto;

    struct GCClosure : GCObject
    {
        static constexpr auto kObjectType = GCType::kClosure;

        GCProto* proto{};
        UpvalueIndexVector upvalue_indices{};

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
        static constexpr int32_t proto_offset()
        {
            return static_cast<int32_t>(offsetof(GCClosure, proto));
        }
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
    };

} // namespace behl
