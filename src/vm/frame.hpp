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

#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4324)
#endif
    struct alignas(8) CallFrame
    {
        const GCProto* proto;
        uint32_t pc;
        uint32_t base;

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
        static constexpr int32_t base_offset()
        {
            return static_cast<int32_t>(offsetof(CallFrame, base));
        }

        static constexpr int32_t pc_offset()
        {
            return static_cast<int32_t>(offsetof(CallFrame, pc));
        }
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
    };
#if defined(_MSC_VER)
#    pragma warning(pop)
#endif

    struct CallFrameHeader
    {
        uint32_t top;
        uint32_t call_pos;
        uint32_t num_varargs;
        uint32_t defer_mask;
        uint32_t ret_base;
        uint8_t nresults;
    };

    static_assert(std::is_standard_layout_v<CallFrame>);
    static_assert(std::is_standard_layout_v<CallFrameHeader>);
    static_assert(sizeof(CallFrame) == 16);

} // namespace behl
