#pragma once

#include "gc/gc.hpp"
#include "gc/gco_proto.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_detail.hpp"

#include <cassert>

namespace behl
{

    template<size_t I, typename... Args>
    BEHL_FORCEINLINE static decltype(auto) operand_arg(Args&&... args)
    {
        return std::get<I>(std::forward_as_tuple(std::forward<Args>(args)...));
    }

    BEHL_FORCEINLINE
    const Value& operand_reg(State* S, CallFrame& frame, Reg r)
    {
        return get_register(S, frame, r);
    }

    BEHL_FORCEINLINE
    const Value& operand_const_int(State*, CallFrame& frame, ConstIndex k)
    {
        return get_integer_constant(frame.proto, k);
    }

    BEHL_FORCEINLINE
    const Value& operand_const_fp(State*, CallFrame& frame, ConstIndex k)
    {
        return get_fp_constant(frame.proto, k);
    }

    BEHL_FORCEINLINE
    Value operand_imm(State*, CallFrame&, int32_t imm)
    {
        return Value(static_cast<Integer>(imm));
    }

} // namespace behl
