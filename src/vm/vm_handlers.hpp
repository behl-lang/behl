#pragma once

#include "bytecode.hpp"
#include "common/arithmetic.hpp"
#include "frame.hpp"
#include "gc/gc.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "platform/platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_arithmetic.hpp"
#include "vm_controlflow.hpp"
#include "vm_detail.hpp"
#include "vm_metatable.hpp"
#include "vm_operands.hpp"
#include "vm_table.hpp"
#include "vm_upvalues.hpp"

#include <behl/exceptions.hpp>
#include <cassert>
#include <functional>

namespace behl
{
    BEHL_INLINE
    void handler_varargprep(State* S, CallFrame& frame, uint8_t num_params)
    {
        // Calculate how many extra args were passed
        const auto total_args = frame_header(S, frame).top - frame.base - 1;
        const auto num_varargs = (total_args > num_params) ? (total_args - num_params) : 0;

        frame_header(S, frame).num_varargs = num_varargs;

        if (num_varargs == 0)
        {
            return;
        }

        // Before: [func, p0, p1, v0, v1, v2]
        // After:  [func, p0, p1, v0, v1, v2, func_copy, p0_copy, p1_copy]
        //                                    ^
        //                                    new base
        // Varargs are still at their original positions, accessible at base - num_varargs

        const auto old_base = frame.base;
        const auto new_base = old_base + total_args + 1; // Move base past all args

        // Ensure stack has room for copies
        const auto required_size = new_base + 1 + num_params + frame.proto->max_stack_size;
        if (S->stack.size() < required_size)
        {
            S->stack.resize(S, required_size, Value{});
        }

        // Copy function to new position (at frame.top, which is after all args)
        S->stack[new_base] = S->stack[old_base];

        // Copy fixed params to new positions
        for (uint32_t i = 0; i < num_params; ++i)
        {
            S->stack[new_base + 1 + i] = S->stack[old_base + 1 + i];
        }

        // Update frame pointers. call_pos is left untouched: it marks where the caller
        // expects results, which stays at the original call site even though the locals
        // base moves past the varargs.
        frame.base = new_base;
        frame_header(S, frame).top = new_base + 1 + num_params;
    }

    BEHL_INLINE
    void handler_vararg(State* S, CallFrame& frame, Reg a, uint8_t num)
    {
        const auto num_varargs = frame_header(S, frame).num_varargs;

        // Varargs are at: base - num_varargs ... base - 1
        const auto vararg_start = frame.base - num_varargs;
        const auto dest = frame.base + a;

        // num == 0 requests all varargs (multret) and extends top so a following call
        // or table constructor can consume them. num > 0 requests exactly that many
        // values, nil-padding when fewer were passed and leaving top untouched.
        if (num == 0)
        {
            const auto target_end = dest + num_varargs;
            if (target_end > S->stack.size())
            {
                S->stack.resize(S, target_end);
            }

            for (uint32_t i = 0; i < num_varargs; ++i)
            {
                S->stack[dest + i] = S->stack[vararg_start + i];
            }

            frame_header(S, frame).top = target_end;
            return;
        }

        const auto want = static_cast<uint32_t>(num);
        const auto target_end = dest + want;
        if (target_end > S->stack.size())
        {
            S->stack.resize(S, target_end);
        }

        const auto copy_count = (num_varargs < want) ? num_varargs : want;
        for (uint32_t i = 0; i < copy_count; ++i)
        {
            S->stack[dest + i] = S->stack[vararg_start + i];
        }
        for (uint32_t i = copy_count; i < want; ++i)
        {
            S->stack[dest + i].set_nil();
        }
    }

    BEHL_INLINE
    void handler_varargexpand(State* S, CallFrame& frame, Reg table_reg, uint32_t start_idx)
    {
        const auto num_varargs = frame_header(S, frame).num_varargs;

        // Get the table
        Value& table = get_register(S, frame, table_reg);
        assert(table.is_table() && "VARARGEXPAND: table_reg must contain a table");

        // Varargs are at: base - num_varargs ... base - 1
        const auto vararg_start = frame.base - num_varargs;

        // Copy each vararg directly into the table array
        for (uint32_t i = 0; i < num_varargs; ++i)
        {
            const Value key = Value(static_cast<int64_t>(start_idx + i));
            const Value& val = S->stack[vararg_start + i];
            setfield_impl(S, frame, table, key, val);
        }
    }

    BEHL_INLINE
    void handler_closure(State* S, CallFrame& frame, Reg a, uint32_t proto_idx)
    {
        assert(proto_idx < frame.proto->protos.size() && "handler_closure: proto index out of bounds");
        GCProto* nested_proto = frame.proto->protos[proto_idx];
        assert(nested_proto != nullptr && "handler_closure: nested proto is null");

        auto* obj = gc_new_closure(S, nested_proto);
        assert(obj != nullptr);

        get_register(S, frame, a).emplace<GCClosure*>(obj);

        auto& upvalue_indices = obj->upvalue_indices;

        for (size_t i = 0; i < nested_proto->upvalue_names.size(); ++i)
        {
            const Instruction& cap = frame.proto->code[frame.pc++];

            std::invoke([&]() {
                if (cap.op() == OpCode::kOpMove)
                {
                    const auto stack_idx = frame.base + cap.b();
                    const auto uv_idx = find_or_create_upvalue(S, stack_idx);

                    upvalue_indices.push_back(S, uv_idx);
                    return;
                }

                if (cap.op() == OpCode::kOpGetUpval)
                {
                    const auto& parent_upvalue_indices = S->stack[frame.base].get_closure()->upvalue_indices;
                    assert(cap.b() < parent_upvalue_indices.size() && "handler_closure : upvalue index out of bounds");
                    const auto uv_idx = parent_upvalue_indices[cap.b()];
                    upvalue_indices.push_back(S, uv_idx);
                    return;
                }

                assert(false && "Invalid upvalue capture instruction");
            });
        }

        gc_validate_on_stack(S, obj);
        gc_step(S);
    }

    BEHL_INLINE
    void handler_len(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const Value& val = get_register(S, frame, b);

        // Try __len metamethod first for tables
        if (val.is_table_like())
        {
            auto result = try_unary_metamethod<MetaMethodType::kLen>(S, val);
            if (result.has_value())
            {
                get_register(S, frame, a) = result;
                return;
            }
        }

        // No metamethod, use default length
        if (val.is_table())
        {
            const auto* table_data = val.get_table();

            size_t len = 0;
            for (; len < table_data->array.size(); ++len)
            {
                if (table_data->array[len].is_nil())
                {
                    break;
                }
            }
            get_register(S, frame, a).emplace<Integer>(static_cast<Integer>(len));
        }
        else if (val.is_string())
        {
            auto* str_data = val.get_string();
            get_register(S, frame, a).emplace<Integer>(static_cast<Integer>(str_data->size()));
        }
        else
        {
            throw TypeError("attempt to get length of a non-table/non-string value", get_current_location(frame));
        }
    }

    BEHL_INLINE
    void handler_tostring(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const Value& val = get_register(S, frame, b);

        Value result = vm_tostring(S, val, frame);
        get_register(S, frame, a) = result;

        // TOSTRING produces exactly 1 result in register a, so top = base + a + 1
        frame_header(S, frame).top = frame.base + a + 1;

        gc_step(S);
    }

    BEHL_INLINE
    void handler_tonumber(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const Value& val = get_register(S, frame, b);

        Value result = vm_tonumber(S, val);
        get_register(S, frame, a) = result;

        // TONUMBER produces exactly 1 result in register a, so top = base + a + 1
        frame_header(S, frame).top = frame.base + a + 1;
    }

    BEHL_INLINE
    bool for_loop_condition(State* S, const Value idx, const Value limit, Integer mode)
    {
        const bool descending = (mode & kForModeDescending) != 0;
        const bool inclusive = (mode & kForModeInclusive) != 0;

        bool result = false;
        if (descending)
        {
            const bool has_mm = inclusive ? try_comparison_metamethod<MetaMethodType::kLt>(S, idx, limit, result)
                                          : try_comparison_metamethod<MetaMethodType::kLe>(S, idx, limit, result);
            if (has_mm)
            {
                return !result;
            }
        }
        else
        {
            const bool has_mm = inclusive ? try_comparison_metamethod<MetaMethodType::kLe>(S, idx, limit, result)
                                          : try_comparison_metamethod<MetaMethodType::kLt>(S, idx, limit, result);
            if (has_mm)
            {
                return result;
            }
        }

        switch (make_type_pair(idx, limit))
        {
            case kTypePairIntInt:
            case kTypePairIntFloat:
            case kTypePairFloatInt:
            case kTypePairFloatFloat:
            case kTypePairStringString:
                if (descending)
                {
                    return inclusive ? idx >= limit : idx > limit;
                }
                return inclusive ? idx <= limit : idx < limit;
            default:
                break;
        }

        throw TypeError(behl::format("attempt to compare {} with {}", idx.get_type_string(), limit.get_type_string()),
            get_current_location(S->call_stack.back()));
    }

    BEHL_INLINE
    void handler_forprep(State* S, CallFrame& frame, Reg a, int32_t offset)
    {
        Value& init = get_register(S, frame, a);
        const Value& limit = get_register(S, frame, a + 1);
        Value& step = get_register(S, frame, a + 2);
        const Integer mode = get_register(S, frame, a + 4).get_integer();

        if (init.is_integer() && limit.is_integer() && step.is_integer())
        {
            // Counted loop: prove the types and compute the trip count once so
            // FORLOOP only decrements a counter per iteration. The count lives
            // in the internal register the compiler reserves at a+3.
            const auto i = init.get_integer();
            const auto l = limit.get_integer();
            const auto s = step.get_integer();
            const bool descending = (mode & kForModeDescending) != 0;
            const bool inclusive = (mode & kForModeInclusive) != 0;

            const bool enter = descending ? (inclusive ? i >= l : i > l) : (inclusive ? i <= l : i < l);
            if (!enter)
            {
                // Zero iterations: skip past the FORLOOP instruction
                frame.pc += static_cast<uint32_t>(offset) + 1;
                return;
            }

            if (s > 0)
            {
                using UInt = std::make_unsigned_t<Integer>;
                UInt span = descending ? static_cast<UInt>(i) - static_cast<UInt>(l)
                                       : static_cast<UInt>(l) - static_cast<UInt>(i);
                if (!inclusive)
                {
                    span -= 1;
                }
                get_register(S, frame, a + 3).emplace<Integer>(static_cast<Integer>(span / static_cast<UInt>(s)));
                if (descending)
                {
                    step.update(arithmetic::sub(Integer{ 0 }, s));
                }

                // Fall through into the body with the loop variable at its start value
                return;
            }

            get_register(S, frame, a + 3) = Value{};
            return;
        }

        if (!for_loop_condition(S, init, limit, mode))
        {
            S->call_stack.back().pc += static_cast<uint32_t>(offset) + 1;
            return;
        }

        CallFrame& current = S->call_stack.back();
        get_register(S, current, a + 3) = Value{};
    }

    BEHL_INLINE
    void handler_forloop(State* S, CallFrame& frame, Reg a, int32_t offset)
    {
        Value& count = get_register(S, frame, a + 3);

        if (count.is_integer())
        {
            // Counted loop: FORPREP proved idx/limit/step are integers and left
            // the remaining iteration count in the internal register at a+3
            Value& idx = get_register(S, frame, a);
            const Value& step = get_register(S, frame, a + 2);
            using UInt = std::make_unsigned_t<Integer>;
            const auto remaining = static_cast<UInt>(count.get_integer());

            // The index keeps advancing on the final iteration so it ends on
            // the first failing value, same as the generic path
            idx.update(arithmetic::add(idx.get_integer(), step.get_integer()));

            if (remaining != 0)
            {
                count.update(static_cast<Integer>(remaining - 1));
                frame.pc += static_cast<uint32_t>(offset - 1);
            }
            return;
        }

        const Integer mode = get_register(S, frame, a + 4).get_integer();
        const auto step_reg = static_cast<Reg>(a + 2);
        if ((mode & kForModeDescending) != 0)
        {
            handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_reg>(S, frame, a, a, step_reg);
        }
        else
        {
            handler_add(S, frame, a, a, step_reg);
        }

        CallFrame& current = S->call_stack.back();
        if (for_loop_condition(S, get_register(S, current, a), get_register(S, current, a + 1), mode))
        {
            S->call_stack.back().pc += static_cast<uint32_t>(offset - 1);
        }
    }

} // namespace behl
