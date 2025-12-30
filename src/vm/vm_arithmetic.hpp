#pragma once

#include "bytecode.hpp"
#include "common/format.hpp"
#include "exceptions.hpp"
#include "frame.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "types.hpp"
#include "value.hpp"
#include "vm/integer_ops.hpp"
#include "vm_detail.hpp"
#include "vm_metatable.hpp"
#include "vm_upvalues.hpp"

#include <cassert>
#include <cmath>

namespace behl
{

    //////////////////////////////////////////////////////////////////////////
    // Error Throwing Functions

    [[noreturn]] BEHL_FORCEINLINE void throw_bad_arith(
        const Value& a, const Value& b, const CallFrame& frame, const char* op_name = nullptr)
    {
        const auto loc = get_current_location(frame);
        std::string msg;

        if (op_name)
        {
            msg = behl::format<"attempt to {} a '{}' with a '{}'">(op_name, a.get_type_string(), b.get_type_string());
        }
        else
        {
            msg = behl::format<"attempt to perform arithmetic on a '{}' value and a '{}' value">(
                a.get_type_string(), b.get_type_string());
        }

        throw TypeError(msg, loc);
    }

    [[noreturn]] BEHL_FORCEINLINE void throw_bad_arith(const Value& a, const CallFrame& frame)
    {
        const auto loc = get_current_location(frame);
        const auto msg = behl::format<"attempt to perform arithmetic on a {} value">(a.get_type_string());

        throw TypeError(msg, loc);
    }

    //////////////////////////////////////////////////////////////////////////
    // Metamethod Helpers

    // Try to call arithmetic metamethod
    template<MetaMethodType MMIndex>
    BEHL_FORCEINLINE Value try_arith_metamethod(State* S, const Value& a, const Value& b)
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

    // Try to call unary metamethod
    template<MetaMethodType MMIndex>
    BEHL_FORCEINLINE Value try_unary_metamethod(State* S, const Value& a)
    {
        Value mm = metatable_get_method<MMIndex>(a);
        if (!mm.has_value())
        {
            return Value::NullOpt{};
        }

        const Value lhs = a;

        return metatable_call_method_result(S, mm, lhs);
    }

    //////////////////////////////////////////////////////////////////////////
    // Operation Functors

    // Comparison operation functors (wrap functions for use as template params)
    struct CmpEqOp
    {
        BEHL_FORCEINLINE bool operator()(const Value& l, const Value& r) const
        {
            return l == r;
        }
    };
    struct CmpNeOp
    {
        BEHL_FORCEINLINE bool operator()(const Value& l, const Value& r) const
        {
            return l != r;
        }
    };
    struct CmpLtOp
    {
        BEHL_FORCEINLINE bool operator()(const Value& l, const Value& r) const
        {
            return l < r;
        }
    };
    struct CmpLeOp
    {
        BEHL_FORCEINLINE bool operator()(const Value& l, const Value& r) const
        {
            return l <= r;
        }
    };
    struct CmpGtOp
    {
        BEHL_FORCEINLINE bool operator()(const Value& l, const Value& r) const
        {
            return l > r;
        }
    };
    struct CmpGeOp
    {
        BEHL_FORCEINLINE bool operator()(const Value& l, const Value& r) const
        {
            return l >= r;
        }
    };

    // Numeric operation functors
    struct NumericAddOp
    {
        template<typename T>
        BEHL_FORCEINLINE auto operator()(T a, T b) const
        {
            if constexpr (std::is_same_v<T, Integer>)
            {
                return int_op::add(a, b);
            }
            else
            {
                return a + b;
            }
        }
    };

    struct NumericSubOp
    {
        template<typename T>
        BEHL_FORCEINLINE auto operator()(T a, T b) const
        {
            if constexpr (std::is_same_v<T, Integer>)
            {
                return int_op::sub(a, b);
            }
            else
            {
                return a - b;
            }
        }
    };

    struct NumericMulOp
    {
        template<typename T>
        BEHL_FORCEINLINE auto operator()(T a, T b) const
        {
            if constexpr (std::is_same_v<T, Integer>)
            {
                return int_op::mul(a, b);
            }
            else
            {
                return a * b;
            }
        }
    };

    struct NumericDivOp
    {
        template<typename T>
        BEHL_FORCEINLINE auto operator()(T a, T b) const
        {
            return a / b;
        }
    };

    struct NumericModOp
    {
        CallFrame& frame;
        template<typename T>
        BEHL_FORCEINLINE auto operator()(T a, T b) const
        {
            if constexpr (std::is_same_v<T, FP>)
            {
                return std::fmod(a, b);
            }
            else
            {
                if (b == 0) [[unlikely]]
                {
                    throw TypeError("attempt to perform 'n%0'", get_current_location(frame));
                }
                return a % b;
            }
        }
    };

    struct NumericPowOp
    {
        template<typename T>
        BEHL_FORCEINLINE auto operator()(T a, T b) const
        {
            if constexpr (std::is_same_v<T, FP>)
            {
                return std::pow(a, b);
            }
            else
            {
                return static_cast<Integer>(std::pow(static_cast<FP>(a), static_cast<FP>(b)));
            }
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Core Arithmetic Operations

    template<MetaMethodType MMIndex, bool AsFloat, typename Op>
    BEHL_FORCEINLINE void numeric_binop(State* S, Reg dst_reg, const Value& a, const Value& b, CallFrame& frame, Op op)
    {
        const uint16_t type_pair = make_type_pair(a, b);

        switch (type_pair)
        {
            case kTypePairIntInt:
            {
                if constexpr (AsFloat)
                {
                    const FP ai = static_cast<FP>(a.get_integer());
                    const FP bi = static_cast<FP>(b.get_integer());
                    Value& dst = get_register(S, frame, dst_reg);
                    dst.emplace<FP>(op(ai, bi));
                    return;
                }
                else
                {
                    const Integer ai = a.get_integer();
                    const Integer bi = b.get_integer();
                    Value& dst = get_register(S, frame, dst_reg);
                    dst.emplace<Integer>(op(ai, bi));
                    return;
                }
            }

            case kTypePairIntFloat:
            {
                const Integer ai = a.get_integer();
                const FP bf = b.get_fp();
                Value& dst = get_register(S, frame, dst_reg);
                dst.emplace<FP>(op(static_cast<FP>(ai), bf));
                return;
            }

            case kTypePairFloatInt:
            {
                const FP af = a.get_fp();
                const Integer bi = b.get_integer();
                Value& dst = get_register(S, frame, dst_reg);
                dst.emplace<FP>(op(af, static_cast<FP>(bi)));
                return;
            }

            case kTypePairFloatFloat:
            {
                const FP af = a.get_fp();
                const FP bf = b.get_fp();
                Value& dst = get_register(S, frame, dst_reg);
                dst.emplace<FP>(op(af, bf));
                return;
            }

            default:
                break;
        }

        // Try metamethod before throwing error
        auto result = try_arith_metamethod<MMIndex>(S, a, b);

        // Re-fetch frame, might have changed during metamethod call.
        CallFrame& current_frame = S->call_stack.back();

        if (result.has_value())
        {
            Value& dst = get_register(S, current_frame, dst_reg);
            dst = result;
            return;
        }

        throw_bad_arith(a, b, current_frame);
    }

    //////////////////////////////////////////////////////////////////////////
    // Increment/Decrement Helpers

    BEHL_FORCEINLINE
    bool increment_value(State* S, Value& value)
    {
        if (value.is_integer()) [[likely]]
        {
            value.update(int_op::inc(value.get_integer()));
            return true;
        }

        if (value.is_fp())
        {
            value.update(value.get_fp() + static_cast<FP>(1.0));
            return true;
        }

        // Try __add metamethod for tables
        if (value.is_table())
        {
            Value one(static_cast<Integer>(1));
            auto result = try_arith_metamethod<MetaMethodType::kAdd>(S, value, one);
            if (result.has_value())
            {
                value = result;
                return true;
            }
        }

        return false;
    }

    BEHL_FORCEINLINE
    bool decrement_value(State* S, Value& value)
    {
        if (value.is_integer())
        {
            value.update(int_op::dec(value.get_integer()));
            return true;
        }

        if (value.is_fp())
        {
            value.update(value.get_fp() - static_cast<FP>(1.0));
            return true;
        }

        // Try __sub metamethod for tables
        if (value.is_table())
        {
            Value one(static_cast<Integer>(1));
            auto result = try_arith_metamethod<MetaMethodType::kSub>(S, value, one);
            if (result.has_value())
            {
                value = result;
                return true;
            }
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    // Arithmetic Handlers

    // Generic numeric handler template
    template<MetaMethodType MMIndex, bool DivByZeroCheck, typename NumericOp, auto GetLhs, auto GetRhs, typename... Args>
    BEHL_FORCEINLINE void handler_numeric(State* S, CallFrame& frame, Reg dst, Args&&... args)
    {
        const auto& lhs = GetLhs(S, frame, operand_arg<0>(args...));
        const auto& rhs = GetRhs(S, frame, operand_arg<1>(args...));
        numeric_binop<MMIndex, DivByZeroCheck>(S, dst, lhs, rhs, frame, NumericOp{});
    }

    // Special numeric handler for functions that need CallFrame (like mod)
    template<MetaMethodType MMIndex, bool DivByZeroCheck, typename NumericOp, auto GetLhs, auto GetRhs, typename... Args>
    BEHL_FORCEINLINE void handler_numeric_with_frame(State* S, CallFrame& frame, Reg dst, Args&&... args)
    {
        const auto& lhs = GetLhs(S, frame, operand_arg<0>(args...));
        const auto& rhs = GetRhs(S, frame, operand_arg<1>(args...));
        numeric_binop<MMIndex, DivByZeroCheck>(S, dst, lhs, rhs, frame, NumericOp{ frame });
    }

    BEHL_FORCEINLINE
    void handler_add(State* S, CallFrame& frame, Reg a, Reg b, Reg c)
    {
        const Value& lhs = get_register(S, frame, b);
        const Value& rhs = get_register(S, frame, c);

        if (lhs.is_string())
        {
            const auto* ls = lhs.get_string();
            if (rhs.is_string())
            {
                const auto* rs = rhs.get_string();
                auto* obj = gc_new_string(S, { ls->view(), rs->view() });
                get_register(S, frame, a) = Value(obj);
                gc_validate_on_stack(S, obj);
                gc_step(S);
                return;
            }
            throw TypeError(behl::format("can only concatenate string with string, not with {}", rhs.get_type_string()),
                get_current_location(frame));
        }

        if (rhs.is_string())
        {
            throw TypeError(behl::format("can only concatenate string with string, not with {}", lhs.get_type_string()),
                get_current_location(frame));
        }

        numeric_binop<MetaMethodType::kAdd, false>(S, a, lhs, rhs, frame, NumericAddOp{});
    }

    BEHL_FORCEINLINE
    void handler_add_imm(State* S, CallFrame& frame, Reg a, Reg b, int32_t imm)
    {
        const Value& lhs = get_register(S, frame, b);

        if (lhs.is_integer())
        {
            Value& dst = get_register(S, frame, a);
            dst.emplace<Integer>(int_op::add(lhs.get_integer(), imm));
        }
        else if (lhs.is_fp())
        {
            Value& dst = get_register(S, frame, a);
            dst.emplace<FP>(lhs.get_fp() + static_cast<FP>(imm));
        }
        else
        {
            handler_numeric<MetaMethodType::kAdd, false, NumericAddOp, operand_reg, operand_imm>(S, frame, a, b, imm);
        }
    }

    BEHL_FORCEINLINE
    void handler_mod(State* S, CallFrame& frame, Reg a, Reg b, Reg c)
    {
        const Value& lhs = get_register(S, frame, b);
        const Value& rhs = get_register(S, frame, c);

        return numeric_binop<MetaMethodType::kMod, false>(S, a, lhs, rhs, frame, NumericModOp{ frame });
    }

    BEHL_FORCEINLINE
    void handler_unm(State* S, CallFrame& frame, Reg a, Reg b)
    {
        Value& val = get_register(S, frame, b);
        Value& dst = get_register(S, frame, a);

        if (val.is_integer())
        {
            auto i = val.get_integer();
            dst.emplace<Integer>(int_op::neg(i));
            return;
        }
        if (val.is_fp())
        {
            auto f = val.get_fp();
            dst.emplace<FP>(-f);
            return;
        }

        // Try __unm metamethod
        auto result = try_unary_metamethod<MetaMethodType::kUnm>(S, val);
        if (result.has_value())
        {
            get_register(S, S->call_stack.back(), a) = result;
            return;
        }

        throw_bad_arith(val, frame);
    }

    BEHL_FORCEINLINE
    void handler_inc_local(State* S, CallFrame& frame, Reg a)
    {
        Value* base = S->stack.data() + frame.base;
        Value& reg = base[a];

        if (increment_value(S, reg))
        {
            return;
        }

        throw_bad_arith(reg, frame);
    }

    BEHL_FORCEINLINE
    void handler_inc_global(State* S, CallFrame& frame, uint32_t k)
    {
        const Value& key = get_string_constant(frame.proto, k);

        Value& globals = S->globals_table;
        assert(globals.is_table());

        auto* table = globals.get_table();
        assert(table != nullptr);

        auto it = table->hash.find(key);
        if (it != table->hash.end())
        {
            Value& global = it->second;

            if (increment_value(S, global))
            {
                return;
            }

            throw_bad_arith(global, frame);
        }

        throw TypeError("attempt to perform arithmetic on a nil value", get_current_location(frame));
    }

    BEHL_FORCEINLINE
    void handler_inc_upvalue(State* S, CallFrame& frame, Reg a)
    {
        const auto& upvalue_indices = S->stack[frame.base].get_closure()->upvalue_indices;
        assert(a < upvalue_indices.size() && "handler_incupvalue: upvalue index out of bounds");
        const auto upvalue_idx = upvalue_indices[a];
        Value& upval = upvalue_ref(S, upvalue_idx);

        if (increment_value(S, upval))
        {
            return;
        }

        throw_bad_arith(upval, frame);
    }

    BEHL_FORCEINLINE
    void handler_dec_local(State* S, CallFrame& frame, Reg a)
    {
        Value& reg = get_register(S, frame, a);

        if (decrement_value(S, reg))
        {
            return;
        }

        throw_bad_arith(reg, frame);
    }

    BEHL_FORCEINLINE
    void handler_dec_global(State* S, CallFrame& frame, uint32_t k)
    {
        const Value& key = get_string_constant(frame.proto, k);

        Value& globals = S->globals_table;
        assert(globals.is_table());

        auto* table = globals.get_table();
        assert(table != nullptr);

        auto it = table->hash.find(key);
        if (it != table->hash.end())
        {
            Value& global = it->second;

            if (decrement_value(S, global))
            {
                return;
            }

            throw_bad_arith(global, frame);
        }

        throw TypeError("attempt to perform arithmetic on a nil value", get_current_location(frame));
    }

    BEHL_FORCEINLINE
    void handler_dec_upvalue(State* S, CallFrame& frame, Reg a)
    {
        const auto& upvalue_indices = S->stack[frame.base].get_closure()->upvalue_indices;
        assert(a < upvalue_indices.size() && "handler_decupvalue: upvalue index out of bounds");
        const auto upvalue_idx = upvalue_indices[a];
        Value& upval = upvalue_ref(S, upvalue_idx);

        if (decrement_value(S, upval))
        {
            return;
        }

        throw_bad_arith(upval, frame);
    }

} // namespace behl
