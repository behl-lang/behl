#include "gc/gco_proto.hpp"
#include "state.hpp"
#include "state_debug.hpp"
#include "vm/vm.hpp"

#include <behl/debug.hpp>
#include <cassert>

namespace behl
{
    void debug_enable(State* S, bool enable)
    {
        assert(S && "State cannot be null");
        S->debug.enabled = enable;
    }

    void debug_set_event_callback(State* S, DebugEventCallback callback)
    {
        assert(S && "State cannot be null");
        S->debug.on_event = callback;
    }

    void debug_continue(State* S)
    {
        assert(S && "State cannot be null");
        S->debug.pending_command = DebugCommand::Continue;
    }

    void debug_step_into(State* S)
    {
        assert(S && "State cannot be null");
        S->debug.pending_command = DebugCommand::StepInto;
    }

    void debug_step_over(State* S)
    {
        assert(S && "State cannot be null");
        S->debug.pending_command = DebugCommand::StepOver;
    }

    void debug_step_out(State* S)
    {
        assert(S && "State cannot be null");
        S->debug.pending_command = DebugCommand::StepOut;
    }

    void debug_pause(State* S)
    {
        assert(S && "State cannot be null");
        if (!S->debug.enabled)
        {
            return;
        }

        // Set pause mode to break at next instruction
        S->debug.step_mode = StepMode::Pause;
        S->debug.last_line = -1; // Force break on next line
    }

    void debug_set_breakpoint(State* S, const char* file, int32_t line)
    {
        assert(S && "State cannot be null");
        assert(line > 0 && "Line number must be positive");

        Breakpoint bp;
        bp.file = file ? std::string(file) : std::string();
        bp.line = line;

        S->debug.breakpoints.insert(bp);
    }

    void debug_remove_breakpoint(State* S, const char* file, int32_t line)
    {
        assert(S && "State cannot be null");

        Breakpoint bp;
        bp.file = file ? std::string(file) : std::string();
        bp.line = line;

        S->debug.breakpoints.erase(bp);
    }

    void debug_clear_breakpoints(State* S)
    {
        assert(S && "State cannot be null");
        S->debug.breakpoints.clear();
    }

    bool debug_is_enabled(State* S)
    {
        assert(S && "State cannot be null");
        return S->debug.enabled;
    }

    bool debug_get_location(State* S, const char** file, int* line, int* column)
    {
        assert(S && "State cannot be null");

        if (S->call_stack.empty())
        {
            return false;
        }

        const auto& frame = S->call_stack.back();
        if (!frame.proto || frame.pc >= frame.proto->line_info.size())
        {
            return false;
        }

        // PC points at the current instruction (not yet executed during debug callback)
        if (line)
        {
            *line = frame.proto->line_info[frame.pc];
        }

        if (column && frame.pc < frame.proto->column_info.size())
        {
            *column = frame.proto->column_info[frame.pc];
        }

        if (file)
        {
            *file = !frame.proto->source_name || frame.proto->source_name->size() == 0 ? "<script>"
                                                                                       : frame.proto->source_name->data();
        }

        return true;
    }

} // namespace behl
