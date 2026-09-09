#pragma once

#include "bytecode.hpp"
#include "frame.hpp"
#include "gc/gc.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm/integer_ops.hpp"
#include "vm_detail.hpp"
#include "vm_metatable.hpp"
#include "vm_operands.hpp"
#include "vm_upvalues.hpp"

#include <behl/exceptions.hpp>
#include <cassert>
#include <functional>

namespace behl
{
    BEHL_FORCEINLINE
    static void handler_closure(State* S, CallFrame& frame, Reg a, uint32_t proto_idx)
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
    BEHL_FORCEINLINE
    static void handler_len(State* S, CallFrame& frame, Reg a, Reg b)
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

    BEHL_FORCEINLINE
    static void handler_tostring(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const Value& val = get_register(S, frame, b);

        Value result = vm_tostring(S, val, frame);
        get_register(S, frame, a) = result;

        // TOSTRING produces exactly 1 result in register a, so top = base + a + 1
        frame_header(S, frame).top = frame.base + a + 1;

        gc_step(S);
    }

    BEHL_FORCEINLINE
    static void handler_tonumber(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const Value& val = get_register(S, frame, b);

        Value result = vm_tonumber(S, val);
        get_register(S, frame, a) = result;

        // TONUMBER produces exactly 1 result in register a, so top = base + a + 1
        frame_header(S, frame).top = frame.base + a + 1;
    }

    BEHL_FORCEINLINE
    static void handler_forprep(State* S, CallFrame& frame, Reg a, int32_t offset)
    {
        Value& init = get_register(S, frame, a);
        Value& limit = get_register(S, frame, a + 1);
        Value& step = get_register(S, frame, a + 2);

        if (init.is_integer() && limit.is_integer() && step.is_integer())
        {
            // Counted loop: prove the types and compute the trip count once so
            // FORLOOP only decrements a counter per iteration. The count lives
            // in the internal register the compiler reserves at a+3.
            const auto i = init.get_integer();
            const auto l = limit.get_integer();
            const auto s = step.get_integer();

            if ((s > 0) ? (i > l) : (i < l))
            {
                // Zero iterations: skip past the FORLOOP instruction
                frame.pc += static_cast<uint32_t>(offset) + 1;
                return;
            }

            using UInt = std::make_unsigned_t<Integer>;
            UInt remaining;
            if (s > 0)
            {
                remaining = (static_cast<UInt>(l) - static_cast<UInt>(i)) / static_cast<UInt>(s);
            }
            else if (s < 0)
            {
                remaining = (static_cast<UInt>(i) - static_cast<UInt>(l)) / (0 - static_cast<UInt>(s));
            }
            else
            {
                // A zero step never advances; mirror the old endless behavior
                remaining = ~static_cast<UInt>(0);
            }

            get_register(S, frame, a + 3).emplace<Integer>(static_cast<Integer>(remaining));

            // Fall through into the body with the loop variable at its start value
            return;
        }

        if (init.is_numeric() && step.is_numeric())
        {
            const FP i = init.is_integer() ? static_cast<FP>(init.get_integer()) : init.get_fp();
            const FP s = step.is_integer() ? static_cast<FP>(step.get_integer()) : step.get_fp();
            init.emplace<FP>(i - s);

            // FORLOOP distinguishes counted loops by an integer step, so float
            // loops always carry a float step
            step.emplace<FP>(s);
            if (limit.is_integer())
            {
                limit.emplace<FP>(static_cast<FP>(limit.get_integer()));
            }

            frame.pc += static_cast<uint32_t>(offset);
            return;
        }

        throw TypeError("numeric for-loop requires number initial and step values", get_current_location(frame));
    }

    BEHL_FORCEINLINE
    static void handler_forloop(State* S, CallFrame& frame, Reg a, int32_t offset)
    {
        Value& idx = get_register(S, frame, a);
        const Value& limit = get_register(S, frame, a + 1);
        const Value& step = get_register(S, frame, a + 2);

        if (step.is_integer())
        {
            // Counted loop: FORPREP proved idx/limit/step are integers and left
            // the remaining iteration count in the internal register at a+3
            Value& count = get_register(S, frame, a + 3);
            using UInt = std::make_unsigned_t<Integer>;
            const auto remaining = static_cast<UInt>(count.get_integer());

            // The index keeps advancing on the final iteration so it ends on
            // the first failing value, same as the generic path
            idx.update(int_op::add(idx.get_integer(), step.get_integer()));

            if (remaining != 0)
            {
                count.update(static_cast<Integer>(remaining - 1));
                frame.pc += static_cast<uint32_t>(offset - 1);
            }
            return;
        }

        if (idx.is_numeric() && limit.is_numeric() && step.is_numeric())
        {
            const FP i = idx.is_integer() ? static_cast<FP>(idx.get_integer()) : idx.get_fp();
            const FP l = limit.is_integer() ? static_cast<FP>(limit.get_integer()) : limit.get_fp();
            const FP s = step.is_integer() ? static_cast<FP>(step.get_integer()) : step.get_fp();

            const FP new_idx = i + s;
            idx.emplace<FP>(new_idx);

            const bool continue_loop = (s > 0) ? (new_idx <= l) : (new_idx >= l);
            if (continue_loop)
            {
                frame.pc += static_cast<uint32_t>(offset - 1);
            }

            return;
        }

        throw TypeError("numeric for-loop requires number index/limit/step values", get_current_location(frame));
    }

} // namespace behl
