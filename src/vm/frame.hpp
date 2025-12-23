#pragma once

#include "common/vector.hpp"
#include "upvalue.hpp"
#include "value.hpp"

#include <cstdint>
#include <vector>

namespace behl
{
    struct GCProto;

    struct CallFrame
    {
        const GCProto* proto;
        uint32_t pc;
        uint32_t base;
        uint32_t top;
        uint32_t call_pos;
        uint8_t nresults;
        uint32_t num_varargs;
    };

} // namespace behl
