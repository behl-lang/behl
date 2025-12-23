#pragma once

#include <behl/types.hpp>

namespace behl
{
    // Debug events sent from VM to debugger
    enum class DebugEvent
    {
        BreakpointHit, // Stopped at a breakpoint
        Paused,        // Paused via debug_pause()
        SteppedIn,     // Completed step into
        SteppedOver,   // Completed step over
        SteppedOut,    // Completed step out
        ScriptFinished // Script execution completed
    };

    // Debug event callback type
    // Called by VM when debug events occur
    // The callback should issue a command (continue, step, etc.) to resume execution
    using DebugEventCallback = void (*)(State* S, DebugEvent event);

    // Enable/disable debugging for a State
    void debug_enable(State* S, bool enable = true);

    // Set event callback - called when VM hits breakpoint, completes step, etc.
    void debug_set_event_callback(State* S, DebugEventCallback callback);

    // Issue commands (called from event callback to control execution)
    // These set pending commands that the VM will process

    // Continue normal execution until next breakpoint
    void debug_continue(State* S);

    // Step to the next source line, entering function calls
    void debug_step_into(State* S);

    // Step to the next source line at the same or shallower call depth
    void debug_step_over(State* S);

    // Step out of the current function (continue until return)
    void debug_step_out(State* S);

    // Breakpoint management
    void debug_set_breakpoint(State* S, const char* file, int line);
    void debug_remove_breakpoint(State* S, const char* file, int line);
    void debug_clear_breakpoints(State* S);

    // Pause execution at the next opportunity (issues pause command)
    void debug_pause(State* S);

    // Query current debug state
    bool debug_is_enabled(State* S);

    // Get current source location (file, line, column)
    // Returns false if location information is not available
    bool debug_get_location(State* S, const char** file, int* line, int* column);

} // namespace behl
