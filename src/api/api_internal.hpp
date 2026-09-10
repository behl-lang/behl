#pragma once

#include <cstddef>
#include <cstdint>

namespace behl
{
    struct State;

    ptrdiff_t resolve_index(const State* S, int32_t idx);

} // namespace behl
