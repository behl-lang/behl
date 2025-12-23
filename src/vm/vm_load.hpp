#pragma once

#include "bytecode.hpp"
#include "frame.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_detail.hpp"

namespace behl
{

    BEHL_FORCEINLINE
    void handler_loadk(State* S, CallFrame& frame, Reg a, uint32_t k)
    {
        get_register(S, frame, a) = get_string_constant(frame.proto, k);
    }

    BEHL_FORCEINLINE
    void handler_loadi(State* S, CallFrame& frame, Reg a, ConstIndex k)
    {
        get_register(S, frame, a) = get_integer_constant(frame.proto, k);
    }

    BEHL_FORCEINLINE
    void handler_loadf(State* S, CallFrame& frame, Reg a, ConstIndex k)
    {
        get_register(S, frame, a) = get_fp_constant(frame.proto, k);
    }

    BEHL_FORCEINLINE
    void handler_loadbool(State* S, CallFrame& frame, Reg a, bool b, bool skip)
    {
        get_register(S, frame, a).emplace<bool>(b);
        if (skip)
        {
            frame.pc++;
        }
    }

    BEHL_FORCEINLINE
    void handler_loadnil(State* S, CallFrame& frame, Reg a, Reg b)
    {
        for (uint8_t i = a; i <= a + b; ++i)
        {
            get_register(S, frame, i).set_nil();
        }
    }

    BEHL_FORCEINLINE
    void handler_load_imm(State* S, CallFrame& frame, Reg a, int32_t imm)
    {
        get_register(S, frame, a) = Value(static_cast<Integer>(imm));
    }

    BEHL_FORCEINLINE
    void handler_move(State* S, CallFrame& frame, const Reg a, const Reg b)
    {
        const Value& src = get_register(S, frame, b);
        get_register(S, frame, a) = src;
    }

} // namespace behl
