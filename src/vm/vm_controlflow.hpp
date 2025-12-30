#pragma once

#include "bytecode.hpp"
#include "common/format.hpp"
#include "frame.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_detail.hpp"
#include "vm_metatable.hpp"
#include "vm_operands.hpp"
#include "vm_upvalues.hpp"

#include <algorithm>
#include <behl/exceptions.hpp>
#include <cassert>

namespace behl
{
    // Forward declarations for functions used by control flow handlers
    [[noreturn]] BEHL_FORCEINLINE void throw_bad_call(const Value& val, const CallFrame& frame, State* S)
    {
        const auto loc = get_current_location(frame);
        std::string msg;
        switch (val.get_type())
        {
            case Type::kNil:
                msg = "attempt to call nil value";
                break;
            case Type::kBoolean:
                msg = "attempt to call boolean value";
                break;
            case Type::kInteger:
                msg = "attempt to call integer value";
                break;
            case Type::kNumber:
                msg = "attempt to call number value";
                break;
            case Type::kString:
                msg = "attempt to call string value";
                break;
            case Type::kTable:
                msg = "attempt to call table value";
                break;
            case Type::kUserdata:
                msg = "attempt to call userdata value";
                break;
            default:
                msg = "attempt to call unknown value";
                break;
        }

        std::string stacktrace = build_stacktrace_internal(S);
        std::string full_msg = behl::format<"{}\n{}">(msg, stacktrace);
        throw TypeError(full_msg, loc);
    }

    // Setup a new call frame for any function call
    BEHL_FORCEINLINE
    void setup_call_frame(
        State* S, const GCProto* proto, uint32_t new_base, uint32_t actual_num_args, uint32_t call_pos, uint8_t nresults)
    {
        CallFrame& new_frame = S->call_stack.emplace_back(S);
        new_frame.proto = proto;
        new_frame.pc = 0;
        new_frame.base = new_base;
        new_frame.top = new_base + actual_num_args + 1;
        new_frame.call_pos = call_pos;
        new_frame.nresults = nresults;

        if (proto && proto->is_vararg && actual_num_args > proto->num_params)
        {
            new_frame.num_varargs = actual_num_args - proto->num_params;
        }
        else
        {
            new_frame.num_varargs = 0;
        }
    }

    // Prepare stack for a function call (ensure enough space)
    BEHL_FORCEINLINE
    void prepare_call(State* S, uint32_t frame_size, uint32_t new_base, uint32_t actual_num_args)
    {
        const auto items_to_move = actual_num_args + 1;
        const auto items_count = new_base + items_to_move;
        const auto proto_size = new_base + frame_size;
        const auto required_size = (items_count > proto_size) ? items_count : proto_size;

        S->stack.resize(S, required_size);
    }

    // Move arguments for tail call (handles forward and backward moves)
    BEHL_FORCEINLINE
    void move_tail_call_args(State* S, uint32_t func_abs_pos, uint32_t frame_base, uint32_t items_to_move) noexcept
    {
        auto& stack = S->stack;
        if (func_abs_pos > frame_base)
        {
            std::move(stack.begin() + func_abs_pos, stack.begin() + func_abs_pos + items_to_move, stack.begin() + frame_base);
        }
        else if (func_abs_pos < frame_base)
        {
            std::copy_backward(stack.begin() + func_abs_pos, stack.begin() + func_abs_pos + items_to_move,
                stack.begin() + frame_base + items_to_move);
        }
    }

    // Clear local variables above arguments for tail call
    BEHL_FORCEINLINE
    void clear_tail_call_locals(State* S, uint32_t first_local_reg) noexcept
    {
        auto& stack = S->stack;
        if (first_local_reg < stack.size())
        {
            for (size_t i = first_local_reg; i < stack.size(); ++i)
            {
                stack[i].set_nil();
            }
        }
    }

    BEHL_FORCEINLINE
    void move_results(Vector<Value>& stack, State* S, uint32_t src_base, uint32_t dest, uint32_t count, uint32_t available)
    {
        const uint32_t final_size = dest + count;

        if (count == 0)
        {
            // No results, resize and return
            stack.resize(S, final_size);
            return;
        }

        // Move available results first (before resize in case src and dest overlap)
        const auto to_move = std::min(count, available);

        if (count == 1 && available >= 1)
        {
            // Single result - direct assignment
            stack[dest] = std::move(stack[src_base]);
        }
        else if (count == 2 && available >= 2)
        {
            // Two results - unrolled moves
            stack[dest] = std::move(stack[src_base]);
            stack[dest + 1] = std::move(stack[src_base + 1]);
        }
        else if (count == 3 && available >= 3)
        {
            // Three results - unrolled moves (common for iterator returns)
            stack[dest] = std::move(stack[src_base]);
            stack[dest + 1] = std::move(stack[src_base + 1]);
            stack[dest + 2] = std::move(stack[src_base + 2]);
        }
        else
        {
            // General case - move what's available
            if (to_move > 0)
            {
                std::move(stack.begin() + src_base, stack.begin() + src_base + to_move, stack.begin() + dest);
            }
        }

        // Resize to final size
        stack.resize(S, final_size);

        // Pad remaining slots with nil if we need more than available
        for (uint32_t i = to_move; i < count; ++i)
        {
            stack[dest + i].set_nil();
        }
    }

    // Count the actual number of arguments passed to a function.
    BEHL_FORCEINLINE
    static uint32_t count_actual_args(const CallFrame& frame, uint32_t func_pos, uint8_t num_args) noexcept
    {
        if (num_args == static_cast<uint8_t>(kMultArgs))
        {
            // kMultArgs: use all values on stack from func_pos + 1 to frame.top
            return (frame.top > func_pos + 1) ? (frame.top - func_pos - 1) : 0;
        }
        return static_cast<uint32_t>(num_args - 1);
    }

    // Core return implementation
    BEHL_FORCEINLINE
    static bool return_from_function(
        State* S, const CallFrame& frame, uint8_t a, uint8_t num_results, uint32_t entry_call_depth)
    {
        auto& call_stack = S->call_stack;
        auto& stack = S->stack;

        const auto result_base = frame.base + a;

        // num_results from RETURN instruction tells us how many the function is returning
        uint32_t num_available;
        if (num_results == static_cast<uint8_t>(kMultRet))
        {
            // kMultRet (-1 as uint8_t = 255): return all available values from frame.top
            num_available = (frame.top > result_base) ? (frame.top - result_base) : 0;
        }
        else
        {
            // Explicit count: function is returning exactly num_results values
            num_available = num_results;
        }

        // frame.nresults tells us how many the CALLER expects (from CALL instruction)
        const uint8_t wanted = frame.nresults;
        uint32_t num_to_move;
        if (wanted == static_cast<uint8_t>(kMultRet))
        {
            // Caller wants all results
            num_to_move = num_available;
        }
        else
        {
            // Caller wants specific number - adjust if needed
            num_to_move = wanted;
        }

        const uint32_t dest = frame.call_pos;
        const uint32_t stack_size = static_cast<uint32_t>(stack.size());

        if (frame.proto->has_upvalues) [[unlikely]]
        {
            close_upvalues(S, frame.base);
        }

        // Move/pad results to match what caller expects
        const auto available = (result_base < stack_size) ? std::min(num_available, stack_size - result_base) : 0;
        move_results(stack, S, result_base, dest, num_to_move, available);

        call_stack.pop_back();

        // Check if we've returned to the entry depth or the stack is empty
        if (call_stack.empty() || call_stack.size() <= entry_call_depth)
        {
            return false;
        }

        // Set top to what we're claiming to return
        auto& next_frame = call_stack.back();
        next_frame.top = dest + num_to_move;

        const auto* next_proto = next_frame.proto;
        if (next_proto != nullptr)
        {
            const auto caller_needs = next_frame.base + next_proto->max_stack_size;
            if (stack.size() < caller_needs)
            {
                stack.resize(S, caller_needs);
            }

            return true;
        }

        return false;
    }

    // Execute C function implementation
    BEHL_FORCEINLINE
    static uint32_t execute_native_impl(State* S, CFunction cfunc, uint32_t func_pos, uint32_t num_args)
    {
        auto& stack = S->stack;
        auto& call_stack = S->call_stack;

        setup_call_frame(S, nullptr, func_pos, num_args, func_pos, static_cast<uint8_t>(kMultRet));
        prepare_call(S, 0, func_pos, num_args);

        const auto res = cfunc(S);
        const auto result_count = std::max(0, res);

        call_stack.pop_back();

        const auto returned_count = static_cast<uint32_t>(result_count);
        const auto current_stack_size = static_cast<uint32_t>(stack.size());

        uint32_t results_start = (current_stack_size >= returned_count) ? (current_stack_size - returned_count)
                                                                        : current_stack_size;
        move_results(stack, S, results_start, func_pos, returned_count, returned_count);

        return returned_count;
    }

    // Main function call logic
    BEHL_FORCEINLINE
    void call_function(State* S, uint8_t a, uint8_t num_args, uint8_t num_results)
    {
        // Manual loop for the metamethod case to avoid C++ stack frame growth
        for (;;)
        {
            auto& call_stack = S->call_stack;
            assert(!call_stack.empty() && "Call stack should not be empty in call_function");

            CallFrame& caller_frame = call_stack.back();
            assert(caller_frame.proto != nullptr && "Caller frame should have valid proto in call_function");

            const Value& func = get_register(S, caller_frame, a);

            if (func.is_closure())
            {
                const auto caller_base = caller_frame.base;
                const auto call_pos = caller_base + a;
                const auto new_base = call_pos;
                const uint32_t actual_num_args = count_actual_args(caller_frame, call_pos, num_args);

                const auto* closure_data = func.get_closure();
                const auto* proto = closure_data->proto;
                setup_call_frame(S, proto, new_base, actual_num_args, call_pos, num_results);
                prepare_call(S, proto->max_stack_size, new_base, actual_num_args);

                return;
            }

            if (func.is_cfunction())
            {
                const auto caller_base = caller_frame.base;
                const auto call_pos = caller_base + a;
                const auto new_base = call_pos;
                const uint32_t actual_num_args = count_actual_args(caller_frame, call_pos, num_args);

                auto* cfunc = func.get_cfunction();

                const uint32_t returned_count = execute_native_impl(S, cfunc, new_base, actual_num_args);
                const uint32_t wanted = (num_results == static_cast<uint8_t>(kMultRet)) ? returned_count
                                                                                        : static_cast<uint32_t>(num_results);

                if (!call_stack.empty())
                {
                    // Ensure stack is large enough before padding
                    if (call_pos + wanted > S->stack.size())
                    {
                        S->stack.resize(S, call_pos + wanted);
                    }

                    call_stack.back().top = call_pos + wanted;

                    // Pad with nils if needed
                    for (uint32_t i = returned_count; i < wanted; ++i)
                    {
                        S->stack[call_pos + i].set_nil();
                    }
                }

                return;
            }

            // Try __call metamethod for tables/userdata
            if (func.is_table_like())
            {
                Value call_mm = metatable_get_method<MetaMethodType::kCall>(func);
                if (call_mm.is_callable())
                {
                    // The table with __call metamethod becomes the first argument
                    const auto func_pos = caller_frame.base + a;

                    // Insert the actual function and shift the table to be first arg
                    // Stack before: [table, arg1, arg2, ...]
                    // Stack after: [function, table, arg1, arg2, ...]
                    S->stack.insert(S, S->stack.begin() + func_pos, call_mm);

                    // Adjust top if it was fixed
                    if (num_args != static_cast<uint8_t>(kMultArgs))
                    {
                        call_stack.back().top++;
                    }

                    // Loop back with updated num_args (simulates tail call)
                    num_args = (num_args == static_cast<uint8_t>(kMultArgs)) ? static_cast<uint8_t>(kMultArgs) : (num_args + 1);

                    continue;
                }
            }

            throw_bad_call(func, caller_frame, S);
        }
    }

    // Jump instruction handler
    BEHL_FORCEINLINE
    void handler_jmp(CallFrame& frame, int32_t offset)
    {
        frame.pc += static_cast<uint32_t>(offset);
    }

    // Call instruction handler
    BEHL_FORCEINLINE
    void handler_call(State* S, CallFrame& frame, Reg a, uint8_t num_args, uint8_t num_results, bool is_self_call)
    {
        if (S->gc.gc_debt > 0)
        {
            gc_step(S);
        }

        const auto call_pos = frame.base + a;

        // Only set frame.top for fixed argument counts. kMultArgs means use current frame.top
        if (num_args != static_cast<uint8_t>(kMultArgs))
        {
            frame.top = call_pos + num_args;
        }

        if (is_self_call)
        {
            // Optimize self-recursive calls - we already know we're calling the current function
            const auto new_base = call_pos;
            const uint32_t actual_num_args = count_actual_args(frame, static_cast<uint32_t>(call_pos), num_args);

            const auto* proto = frame.proto;
            setup_call_frame(S, proto, new_base, actual_num_args, call_pos, num_results);
            prepare_call(S, proto->max_stack_size, new_base, actual_num_args);
        }
        else
        {
            call_function(S, a, num_args, num_results);

            auto& new_frame = S->call_stack.back();
            const auto stack_after_call = new_frame.base + new_frame.proto->max_stack_size;

            S->stack.resize(S, stack_after_call);
        }
    }

    // Tail call instruction handler
    BEHL_FORCEINLINE
    static bool handler_tailcall(
        State* S, CallFrame& frame, Reg a, uint8_t num_args, bool is_self_call, uint32_t entry_call_depth)
    {
        const auto func_abs_pos = frame.base + a;
        const uint32_t actual_num_args = count_actual_args(frame, static_cast<uint32_t>(func_abs_pos), num_args);
        const auto items_to_move = actual_num_args + 1;
        auto& stack = S->stack;

        // Optimize self-tail-recursive calls
        if (is_self_call)
        {
            if (frame.proto->has_upvalues)
            {
                close_upvalues(S, frame.base);
            }

            // Move only arguments to frame.base (overwriting old args)
            move_tail_call_args(S, func_abs_pos, frame.base, items_to_move);
            clear_tail_call_locals(S, frame.base + actual_num_args + 1);

            // Reset PC to beginning - proto and upvalues stay the same
            frame.pc = 0;
            frame.top = frame.base + actual_num_args + 1;
            stack.resize(S, frame.base + frame.proto->max_stack_size);

            return true;
        }

        const Value& func = get_register(S, frame, a);

        if (func.is_closure())
        {
            auto* closure_data = func.get_closure();
            auto* proto = closure_data->proto;

            if (frame.proto->has_upvalues)
            {
                close_upvalues(S, frame.base);
            }

            // Move only arguments to frame.base (overwriting old args)
            move_tail_call_args(S, func_abs_pos, frame.base, items_to_move);
            clear_tail_call_locals(S, frame.base + actual_num_args + 1);

            frame.proto = proto;
            frame.pc = 0;
            frame.top = frame.base + actual_num_args + 1;

            const auto items_count = frame.top;
            const auto proto_size = frame.base + proto->max_stack_size;
            const auto required_size = (items_count > proto_size) ? items_count : proto_size;

            if (required_size > stack.size())
            {
                stack.resize(S, required_size);
            }

            return true;
        }
        else if (func.is_cfunction())
        {
            call_function(S, a, num_args, static_cast<uint8_t>(kMultRet));

            // For tail call to C function, return with what the caller expects
            // Use kMultRet to return all values from the C function
            return return_from_function(S, frame, a, static_cast<uint8_t>(kMultRet), entry_call_depth);
        }
        else if (func.is_table_like())
        {
            // Try __call metamethod for tables/userdata
            Value call_mm = metatable_get_method<MetaMethodType::kCall>(func);

            // Check if the metamethod is a C function or a closure
            if (call_mm.is_cfunction())
            {
                // For C function metamethods, call and return immediately
                call_function(S, a, num_args, static_cast<uint8_t>(kMultRet));

                return return_from_function(S, frame, a, static_cast<uint8_t>(kMultRet), entry_call_depth);
            }
            else if (call_mm.is_closure())
            {
                // For closure metamethods, handle like a tail call to a closure
                // The __call metamethod takes the object as the first argument
                // Stack before: [object, arg1, arg2, ...]
                // Stack after (at frame.base): [closure, object, arg1, arg2, ...]
                auto* closure_data = call_mm.get_closure();

                // Close upvalues before modifying the stack
                close_upvalues(S, frame.base);

                // Move [object, arg1, arg2, ...] to [frame.base + 1, frame.base + 2, ...]
                const auto src_start = func_abs_pos;
                const auto dest_start = frame.base + 1;
                const auto mm_items_to_move = actual_num_args + 1; // object + args

                if (src_start != dest_start && mm_items_to_move > 0)
                {
                    std::move(
                        stack.begin() + src_start, stack.begin() + src_start + mm_items_to_move, stack.begin() + dest_start);
                }

                // Place the closure at frame.base
                stack[frame.base] = call_mm;

                // Clear any locals above the arguments
                const auto new_top = frame.base + mm_items_to_move + 1; // closure + object + args
                clear_tail_call_locals(S, new_top);

                frame.proto = closure_data->proto;
                frame.pc = 0;
                frame.top = new_top;
                stack.resize(S, frame.base + closure_data->proto->max_stack_size);

                return true;
            }
            else
            {
                throw_bad_call(func, frame, S);
            }
        }

        throw_bad_call(func, frame, S);
    }

    // Return instruction handler
    BEHL_FORCEINLINE
    static bool handler_return(State* S, const CallFrame& frame, Reg a, uint8_t num_results, uint32_t entry_call_depth)
    {
        return return_from_function(S, frame, a, num_results, entry_call_depth);
    }

    //////////////////////////////////////////////////////////////////////////
    // Comparison and Test Handlers

    // Try comparison metamethod
    template<MetaMethodType MMIndex>
    BEHL_FORCEINLINE static bool try_comparison_metamethod(State* S, const Value first, const Value second, bool& result)
    {
        Value mm1 = metatable_get_method<MMIndex>(first);
        Value mm2 = metatable_get_method<MMIndex>(second);

        if constexpr (MMIndex == MetaMethodType::kEq)
        {
            // Both operands must be tables OR userdata with metatables for __eq to trigger
            bool a_has_metatable = first.is_table_like();
            bool b_has_metatable = second.is_table_like();

            if (!a_has_metatable || !b_has_metatable)
            {
                return false;
            }

            // BOTH must have the __eq metamethod, otherwise use default comparison
            if (!mm1.has_value() || !mm2.has_value())
            {
                return false;
            }

            // Both have eq, must be the same metamethod for __eq to trigger
            if (mm1 != mm2)
            {
                return false;
            }
        }
        else
        {
            // For __lt and __le, use if either has the metamethod
            if (!mm1.has_value() && !mm2.has_value())
            {
                return false;
            }
        }

        result = metatable_call_method_result(S, !mm1.has_value() ? mm2 : mm1, first, second).is_truthy();

        return true;
    }

    // General comparison operation
    template<MetaMethodType MMIndex, typename CmpFunc>
    BEHL_FORCEINLINE void comparison_op_general(State* S, CallFrame& frame, auto&& lhs, auto&& rhs, CmpFunc&& cmp)
    {
        // Try metamethod first
        bool result = false;
        if (try_comparison_metamethod<MMIndex>(S, lhs, rhs, result))
        {
            frame.pc += !result ? 1 : 0;
            return;
        }

        // No metamethod - check if we can do direct comparison
        const uint16_t type_pair = make_type_pair(lhs, rhs);

        switch (type_pair)
        {
            case kTypePairIntInt:
            case kTypePairIntFloat:
            case kTypePairFloatInt:
            case kTypePairFloatFloat:
            case kTypePairStringString:
            {
                // Numbers and strings - do direct comparison
                result = cmp(lhs, rhs);
                frame.pc += !result ? 1 : 0;
                return;
            }
        }

        // For equality on other types, fall back to identity comparison
        // For ordering, throw error
        if constexpr (MMIndex == MetaMethodType::kEq)
        {
            result = cmp(lhs, rhs);
            frame.pc += !result ? 1 : 0;
        }
        else
        {
            // Ordering comparison on incompatible types
            throw TypeError(behl::format("attempt to compare {} with {}", lhs.get_type_string(), rhs.get_type_string()),
                get_current_location(frame));
        }
    }

    // Generic comparison handler template
    template<MetaMethodType MMIndex, typename CmpOp, auto TGetLhs, auto TGetRhs, typename... Args>
    BEHL_FORCEINLINE void handler_cmp(State* S, CallFrame& frame, Args&&... args)
    {
        const auto& lhs = TGetLhs(S, frame, operand_arg<0>(args...));
        const auto& rhs = TGetRhs(S, frame, operand_arg<1>(args...));
        comparison_op_general<MMIndex>(S, frame, lhs, rhs, CmpOp{});
    }

    // Test instruction handler
    BEHL_FORCEINLINE
    void handler_test(State* S, CallFrame& frame, Reg a, bool invert)
    {
        bool cond = get_register(S, frame, a).is_truthy();
        if (invert)
        {
            cond = !cond;
        }
        if (!cond)
        {
            frame.pc++;
        }
    }

    // Test and set instruction handler
    BEHL_FORCEINLINE
    void handler_testset(State* S, CallFrame& frame, Reg a, Reg b, bool invert)
    {
        bool cond = get_register(S, frame, b).is_truthy();
        if (invert)
        {
            cond = !cond;
        }
        if (cond)
        {
            get_register(S, frame, a) = get_register(S, frame, b);
        }
        if (!cond)
        {
            frame.pc++;
        }
    }

} // namespace behl
