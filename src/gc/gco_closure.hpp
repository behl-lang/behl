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
    };

} // namespace behl
