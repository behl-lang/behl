#pragma once

#include <cstddef>
#include <cstdint>

namespace behl
{
    struct State;

    // Turns a public API stack index, which may be negative to count back from
    // the top, into an absolute index.
    ptrdiff_t resolve_index(const State* S, int32_t idx);

} // namespace behl
