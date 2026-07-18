#pragma once

#include "common/vector.hpp"
#include "upvalue.hpp"
#include "value.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
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

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
        static constexpr int32_t base_offset()
        {
            return static_cast<int32_t>(offsetof(CallFrame, base));
        }
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
    };

    static_assert(std::is_standard_layout_v<CallFrame>);

} // namespace behl
