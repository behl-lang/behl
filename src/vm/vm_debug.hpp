#pragma once

#include "bytecode.hpp"
#include "common/format.hpp"
#include "common/print.hpp"
#include "frame.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "state_debug.hpp"
#include "value.hpp"
#include "vm_detail.hpp"

#include <cassert>

namespace behl
{

    static constexpr auto kTraceInstructions = false;

    //////////////////////////////////////////////////////////////////////////
    // Debug Helpers

    // Check if execution should pause for debugging
    BEHL_FORCEINLINE
    bool should_break_for_debug(State* S, const CallFrame& frame, DebugEvent& out_event)
    {
        if (!S->debug.enabled)
        {
            return false;
        }

        // Get current location (using current PC before it's incremented)
        if (!frame.proto || frame.pc >= frame.proto->line_info.size())
        {
            return false;
        }

        const int current_line = frame.proto->line_info[frame.pc];

        // Skip instructions with no line info (e.g., implicit returns)
        if (current_line <= 0)
        {
            return false;
        }

        const std::string& current_file = !frame.proto->source_name || frame.proto->source_name->size() == 0
            ? std::string("<script>")
            : std::string(frame.proto->source_name->view());
        const int current_depth = static_cast<int>(S->call_stack.size());

        // Check explicit breakpoints
        for (const auto& bp : S->debug.breakpoints)
        {
            if (bp.line == current_line)
            {
                // If breakpoint has no file specified, or file matches
                if (bp.file.empty() || bp.file == current_file)
                {
                    out_event = DebugEvent::BreakpointHit;
                    return true;
                }
            }
        }

        // Check step modes (only trigger on line changes)
        if (S->debug.step_mode != StepMode::None)
        {
            const bool line_changed = (current_line != S->debug.last_line || current_file != S->debug.last_file);

            if (line_changed)
            {
                switch (S->debug.step_mode)
                {
                    case StepMode::Pause:
                        // Stop at next line change (for debug_pause)
                        out_event = DebugEvent::Paused;
                        S->debug.step_completed = true;
                        S->debug.step_mode = StepMode::None;
                        return true;

                    case StepMode::StepInto:
                        // Stop at any line change
                        out_event = DebugEvent::SteppedIn;
                        S->debug.step_completed = true;
                        S->debug.step_mode = StepMode::None;
                        return true;

                    case StepMode::StepOver:
                        // Stop if at same or shallower depth
                        if (current_depth <= S->debug.step_target_depth)
                        {
                            out_event = DebugEvent::SteppedOver;
                            S->debug.step_completed = true;
                            S->debug.step_mode = StepMode::None;
                            return true;
                        }
                        break;

                    case StepMode::StepOut:
                        // Stop if shallower depth (returned from function)
                        if (current_depth < S->debug.step_target_depth)
                        {
                            out_event = DebugEvent::SteppedOut;
                            S->debug.step_completed = true;
                            S->debug.step_mode = StepMode::None;
                            return true;
                        }
                        break;

                    case StepMode::None:
                        break;
                }
            }
        }

        // Update last location for next check
        S->debug.last_line = current_line;
        S->debug.last_file = current_file;

        return false;
    }

    // Emit debug event and set paused state
    BEHL_FORCEINLINE
    void emit_debug_event(State* S, DebugEvent dv)
    {
        S->debug.paused = true;
        if (S->debug.on_event)
        {
            S->debug.on_event(S, dv);
        }
    }

    // Process pending debug commands
    BEHL_FORCEINLINE
    void process_debug_commands(State* S)
    {
        if (S->debug.pending_command == DebugCommand::None)
        {
            return;
        }

        switch (S->debug.pending_command)
        {
            case DebugCommand::Continue:
                S->debug.paused = false;
                S->debug.step_mode = StepMode::None;
                break;

            case DebugCommand::StepInto:
                S->debug.paused = false;
                S->debug.step_mode = StepMode::StepInto;
                S->debug.last_line = -1;
                break;

            case DebugCommand::StepOver:
                S->debug.paused = false;
                S->debug.step_mode = StepMode::StepOver;
                S->debug.step_target_depth = static_cast<int>(S->call_stack.size());
                break;

            case DebugCommand::StepOut:
                S->debug.paused = false;
                S->debug.step_mode = StepMode::StepOut;
                S->debug.step_target_depth = static_cast<int>(S->call_stack.size());
                break;

            case DebugCommand::None:
                break;
        }

        S->debug.pending_command = DebugCommand::None;
        S->debug.step_completed = false;
    }

    inline std::string get_function_name(State* S, const CallFrame& frame)
    {
        // Get function name
        if (frame.proto && frame.proto->name && frame.proto->name->size() > 0)
        {
            return std::string{ frame.proto->name->view() };
        }

        const Value& closure = S->stack[frame.base];
        if (closure.is_cfunction())
        {
            CFunction called_func = closure.get_cfunction();

            // Traverse globals to find matching C function.
            assert(S->globals_table.is_table());

            auto* globals = S->globals_table.get_table();
            for (const auto& [key, val] : globals->hash)
            {
                if (val.is_cfunction())
                {
                    CFunction candidate = val.get_cfunction();
                    if (candidate == called_func)
                    {
                        if (!key.is_string())
                        {
                            continue;
                        }
                        auto* str = key.get_string();
                        return std::string{ str->view() };
                    }
                }
            }

            // Traverse modules.
            for (const auto& [mod_name, mod_table_val] : S->module_cache)
            {
                if (!mod_table_val.is_table())
                {
                    continue;
                }

                auto* mod_table = mod_table_val.get_table();
                for (const auto& [key, val] : mod_table->hash)
                {
                    if (val.is_cfunction())
                    {
                        CFunction candidate = val.get_cfunction();
                        if (candidate == called_func)
                        {
                            if (!key.is_string())
                            {
                                continue;
                            }
                            auto* str = key.get_string();
                            return behl::format("{}.{}", mod_name->view(), str->view());
                        }
                    }
                }
            }
        }

        return "<unknown>";
    }

    inline std::string build_stacktrace_internal(State* S)
    {
        std::string result;
        result.reserve(512);

        if (S->call_stack.empty())
        {
            result = "<empty call stack>";
            return result;
        }

        result = "Stack trace:\n";

        // Walk the call stack from most recent to oldest
        for (int i = static_cast<int>(S->call_stack.size()) - 1; i >= 0; --i)
        {
            const auto& frame = S->call_stack[static_cast<size_t>(i)];
            const auto loc = get_current_location(frame);

            // Get function name
            const auto func_name = get_function_name(S, frame);

            // Format: filename(line,col): at function
            if (!loc.filename.empty() && loc.line > 0 && loc.column > 0)
            {
                result += behl::format("  {}({},{}): at {}", loc.filename, loc.line, loc.column, func_name);
            }
            else if (!loc.filename.empty() && loc.line > 0)
            {
                result += behl::format("  {}({}): at {}", loc.filename, loc.line, func_name);
            }
            else if (!loc.filename.empty())
            {
                result += behl::format<"  {}: at {}">(loc.filename, func_name);
            }
            else
            {
                result += behl::format<"  at {}">(func_name);
            }

            if (i > 0)
            {
                result += "\n";
            }
        }

        return result;
    }

    BEHL_FORCEINLINE
    void trace_instruction(State*, const CallFrame& frame, const Instruction& instr)
    {
        if constexpr (kTraceInstructions)
        {
            const std::string instr_str = instruction_to_string(instr, frame.pc);
            const auto loc = get_current_location(frame);
            println("[TRACE] {}:{}:{} - {}", loc.filename, loc.line, loc.column, instr_str);
        }
    }

} // namespace behl
