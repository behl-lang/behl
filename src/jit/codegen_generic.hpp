#pragma once

#include "state.hpp"
#include "vm/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace behl
{
    static_assert(sizeof(Vector<Value>) == 3 * sizeof(void*));
    static_assert(std::is_same_v<FP, double>);

    inline constexpr int32_t kValueSize = 16;
    inline constexpr int32_t kPayloadOffset = 8;

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    inline constexpr int32_t kOffStackData = static_cast<int32_t>(offsetof(State, stack));
    inline constexpr int32_t kOffCallStackData = static_cast<int32_t>(offsetof(State, call_stack));
    inline constexpr int32_t kOffCallStackSize = static_cast<int32_t>(offsetof(State, call_stack) + sizeof(void*));
    inline constexpr int32_t kOffFrameBase = static_cast<int32_t>(offsetof(CallFrame, base));
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

} // namespace behl
