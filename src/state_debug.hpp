#pragma once

#include "common/string.hpp"

#include <behl/debug.hpp>
#include <functional>
#include <unordered_set>

namespace behl
{
    struct State;

    // Commands issued FROM debugger TO VM
    enum class DebugCommand
    {
        None,
        Continue,
        StepInto,
        StepOver,
        StepOut
    };

    enum class StepMode
    {
        None,
        Pause,    // Pause at next instruction (for debug_pause)
        StepInto, // Stop at next line
        StepOver, // Stop at next line at same or shallower depth
        StepOut   // Stop when function returns (shallower depth)
    };

    struct Breakpoint
    {
        std::string file; // Empty means any file
        int line;

        bool operator==(const Breakpoint& other) const
        {
            return line == other.line && file == other.file;
        }
    };

    struct BreakpointHash
    {
        size_t operator()(const Breakpoint& bp) const noexcept
        {
            size_t h1 = std::hash<int>{}(bp.line);
            size_t h2 = std::hash<std::string>{}(std::string(bp.file));
            return h1 ^ (h2 << 1);
        }
    };

    struct DebugState
    {
        bool enabled = false;
        bool paused = false;

        // Breakpoints
        std::unordered_set<Breakpoint, BreakpointHash> breakpoints;

        // Pending command from debugger
        DebugCommand pending_command = DebugCommand::None;

        // Event callback - VM calls this when events occur
        DebugEventCallback on_event = nullptr;

        // Stepping state (internal)
        StepMode step_mode = StepMode::None;
        int step_target_depth = -1;
        int last_line = -1;
        std::string last_file;

        // Track if we just completed a step (to emit event)
        bool step_completed = false;
        DebugEvent pending_event = DebugEvent::Paused;
    };

} // namespace behl
