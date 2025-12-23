#pragma once

#include "common/string.hpp"
#include "value.hpp"

#include <cstdint>

namespace behl
{
    struct State;

    // Perform call, enters VM interpreter.
    bool perform_call(State* S, int nargs, int nresults, size_t func_pos);

    // Debug utilities - internal version returns String
    std::string build_stacktrace_internal(State* S);

} // namespace behl
