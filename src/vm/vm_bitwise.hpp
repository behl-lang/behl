#pragma once

#include "bytecode.hpp"
#include "common/format.hpp"
#include "exceptions.hpp"
#include "frame.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "types.hpp"
#include "value.hpp"
#include "vm_detail.hpp"
#include "vm_metatable.hpp"

namespace behl
{

    //////////////////////////////////////////////////////////////////////////
    // Error Throwing Functions

    [[noreturn]] BEHL_FORCEINLINE static void throw_bad_bitwise(const Value& a, const Value& b, const CallFrame& frame)
    {
        const auto loc = get_current_location(frame);
        const auto msg = behl::format(
            "attempt to perform bitwise operation on a '{}' value and a '{}' value", a.get_type_string(), b.get_type_string());

        throw TypeError(msg, loc);
    }

    [[noreturn]] BEHL_FORCEINLINE static void throw_bad_bitwise(const Value& a, const CallFrame& frame)
    {
        const auto loc = get_current_location(frame);
        const auto msg = behl::format<"attempt to perform bitwise operation on a {} value">(a.get_type_string());

        throw TypeError(msg, loc);
    }

    //////////////////////////////////////////////////////////////////////////
    // Metamethod Helpers

    // Try bitwise metamethod
    template<MetaMethodType MMIndex>
    static Value try_bitwise_metamethod(State* S, const Value& a, const Value& b)
    {
        // Check left operand first
        Value mm = metatable_get_method<MMIndex>(a);
        if (!mm.has_value())
        {
            // Check right operand
            mm = metatable_get_method<MMIndex>(b);
            if (!mm.has_value())
            {
                return Value::NullOpt{};
            }
        }

        const Value lhs = a;
        const Value rhs = b;

        return metatable_call_method_result(S, mm, lhs, rhs);
    }

    //////////////////////////////////////////////////////////////////////////
    // Operation Functors

    struct BitwiseAndOp
    {
        BEHL_FORCEINLINE Integer operator()(Integer a, Integer b) const
        {
            return a & b;
        }
    };
    struct BitwiseOrOp
    {
        BEHL_FORCEINLINE Integer operator()(Integer a, Integer b) const
        {
            return a | b;
        }
    };
    struct BitwiseXorOp
    {
        BEHL_FORCEINLINE Integer operator()(Integer a, Integer b) const
        {
            return a ^ b;
        }
    };
    struct BitwiseShlOp
    {
        BEHL_FORCEINLINE Integer operator()(Integer a, Integer b) const
        {
            return a << b;
        }
    };
    struct BitwiseShrOp
    {
        BEHL_FORCEINLINE Integer operator()(Integer a, Integer b) const
        {
            return a >> b;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Core Bitwise Operations

    template<MetaMethodType MMIndex, typename Op>
    BEHL_FORCEINLINE static void bitwise_binop(State* S, Reg dst_reg, const Value& a, const Value& b, CallFrame& frame, Op op)
    {
        const uint16_t type_pair = make_type_pair(a, b);

        switch (type_pair)
        {
            case kTypePairIntInt:
            {
                const Integer ai = a.get_integer();
                const Integer bi = b.get_integer();
                Value& dst = get_register(S, frame, dst_reg);
                dst.emplace<Integer>(op(ai, bi));
                return;
            }

            case kTypePairIntFloat:
            {
                const Integer ai = a.get_integer();
                const Integer bf = static_cast<Integer>(b.get_fp());
                Value& dst = get_register(S, frame, dst_reg);
                dst.emplace<Integer>(op(ai, bf));
                return;
            }

            case kTypePairFloatInt:
            {
                const Integer af = static_cast<Integer>(a.get_fp());
                const Integer bi = b.get_integer();
                Value& dst = get_register(S, frame, dst_reg);
                dst.emplace<Integer>(op(af, bi));
                return;
            }

            case kTypePairFloatFloat:
            {
                const Integer af = static_cast<Integer>(a.get_fp());
                const Integer bf = static_cast<Integer>(b.get_fp());
                Value& dst = get_register(S, frame, dst_reg);
                dst.emplace<Integer>(op(af, bf));
                return;
            }

            default:
                break;
        }

        // Try metamethod before throwing error
        const auto result = try_bitwise_metamethod<MMIndex>(S, a, b);

        // Re-fetch frame, might have changed during metamethod call.
        CallFrame& current_frame = S->call_stack.back();

        if (result.has_value())
        {
            Value& dst = get_register(S, current_frame, dst_reg);
            dst = result;
            return;
        }

        throw_bad_bitwise(a, b, current_frame);
    }

    //////////////////////////////////////////////////////////////////////////
    // Bitwise Handlers

    // Generic bitwise handler template
    template<MetaMethodType MMIndex, typename BitwiseOp, auto GetLhs, auto GetRhs, typename... Args>
    BEHL_FORCEINLINE static void handler_bitwise(State* S, CallFrame& frame, Reg dst, Args&&... args)
    {
        const auto& lhs = GetLhs(S, frame, operand_arg<0>(args...));
        const auto& rhs = GetRhs(S, frame, operand_arg<1>(args...));
        bitwise_binop<MMIndex>(S, dst, lhs, rhs, frame, BitwiseOp{});
    }

    BEHL_FORCEINLINE static void handler_bnot(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const Value& val = get_register(S, frame, b);

        if (val.is_integer())
        {
            auto i = val.get_integer();
            get_register(S, frame, a).emplace<Integer>(~i);
            return;
        }

        // Try __bnot metamethod
        const auto result = try_unary_metamethod<MetaMethodType::kBNot>(S, val);

        // Re-fetch frame reference after metamethod call
        CallFrame& current_frame = S->call_stack.back();

        if (result.has_value())
        {
            get_register(S, current_frame, a) = result;
            return;
        }

        throw_bad_bitwise(val, current_frame);
    }

} // namespace behl
