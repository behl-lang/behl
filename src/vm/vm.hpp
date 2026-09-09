#pragma once

#include "common/string.hpp"
#include "value.hpp"

#include <cstdint>
#include <exception>

namespace behl
{
    struct State;

    // Perform call, enters VM interpreter.
    bool perform_call(State* S, int nargs, int nresults, size_t func_pos);

    void run_interpreter(State* S, uint32_t entry_call_depth, uint32_t stop_depth);

    void unwind_call_frames(State* S, size_t target_depth, std::exception_ptr& pending);

    // Drops every frame above count, keeping call_stack and call_headers in step.
    void truncate_call_frames(State* S, size_t count);

    // Debug utilities - internal version returns String
    std::string build_stacktrace_internal(State* S);

} // namespace behl
