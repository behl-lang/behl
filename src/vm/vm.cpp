#include "vm.hpp"

#include "bytecode.hpp"
#include "config_internal.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "state_debug.hpp"
#include "value.hpp"
#include "vm/integer_ops.hpp"
#include "vm_arithmetic.hpp"
#include "vm_bitwise.hpp"
#include "vm_controlflow.hpp"
#include "vm_debug.hpp"
#include "vm_detail.hpp"
#include "vm_handlers.hpp"
#include "vm_load.hpp"
#include "vm_metatable.hpp"
#include "vm_operands.hpp"
#include "vm_table.hpp"
#include "vm_upvalues.hpp"

#include "jit/jit.hpp"

#include <behl/exceptions.hpp>
#include <cassert>

namespace behl
{
    BEHL_FORCEINLINE
    static void handler_varargprep(State* S, CallFrame& frame, uint8_t num_params)
    {
        // Calculate how many extra args were passed
        const auto total_args = frame.top - frame.base - 1;
        const auto num_varargs = (total_args > num_params) ? (total_args - num_params) : 0;

        frame.num_varargs = num_varargs;

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
        frame.top = new_base + 1 + num_params;
    }

    BEHL_FORCEINLINE
    static void handler_vararg(State* S, CallFrame& frame, Reg a, uint8_t num)
    {
        const auto num_varargs = frame.num_varargs;

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

            frame.top = target_end;
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

    BEHL_FORCEINLINE
    static void handler_varargexpand(State* S, CallFrame& frame, Reg table_reg, uint32_t start_idx)
    {
        const auto num_varargs = frame.num_varargs;

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

    inline static void execute_native(State* S, const Value& func_value, int num_args, int nresults)
    {
        auto& stack = S->stack;

        auto* cfunc = func_value.get_cfunction();
        assert(cfunc != nullptr && "execute_native: function is not a C function");

        const auto func_pos = static_cast<uint32_t>(stack.size()) - static_cast<uint32_t>(num_args) - 1;
        assert(func_pos < stack.size() && "Function position out of range");

        const auto returned_count = execute_native_impl(S, cfunc, func_pos, static_cast<uint32_t>(num_args));

        // Pad with nils if needed
        const auto wanted = (nresults == kMultRet) ? returned_count : static_cast<uint32_t>(nresults);
        stack.resize(S, func_pos + wanted);

        for (uint32_t i = returned_count; i < wanted; ++i)
        {
            stack[func_pos + i].set_nil();
        }
    }

    template<bool TDebugMode>
    inline static void interpreter_loop(State* S, uint32_t entry_call_depth, uint32_t stop_depth)
    {
        auto& callstack = S->call_stack;

        CallFrame* frame = &callstack.back();
        const Instruction* code = frame->proto->code.data();

        for (;;)
        {
            if constexpr (TDebugMode)
            {
                DebugEvent dv = DebugEvent::Paused;
                if (should_break_for_debug(S, *frame, dv))
                {
                    emit_debug_event(S, dv);
                }

                // Process any pending debug commands
                process_debug_commands(S);

                // If still paused, skip instruction execution and keep looping
                if (S->debug.paused)
                {
                    continue;
                }

                // Refresh frame pointer, may have invalidated due to debug functions.
                frame = &callstack.back();
                code = frame->proto->code.data();
            }

            const Instruction instr = code[frame->pc];

            trace_instruction(S, *frame, instr);

            frame->pc++;

            // Execute the instruction
            switch (instr.op())
            {
                case OpCode::kOpMove:
                    handler_move(S, *frame, instr.a(), instr.b());
                    break;
                case OpCode::kOpLoadI:
                    handler_loadi(S, *frame, instr.a(), instr.const_or_proto_index());
                    break;
                case OpCode::kOpLoadF:
                    handler_loadf(S, *frame, instr.a(), instr.const_or_proto_index());
                    break;
                case OpCode::kOpLoadS:
                    handler_loadk(S, *frame, instr.a(), instr.const_or_proto_index());
                    break;
                case OpCode::kOpLoadBool:
                    handler_loadbool(S, *frame, instr.a(), instr.bool_value(), instr.skip_next());
                    break;
                case OpCode::kOpLoadNil:
                    handler_loadnil(S, *frame, instr.a(), instr.b());
                    break;
                case OpCode::kOpLoadImm:
                    handler_load_imm(S, *frame, instr.a(), instr.signed_immediate());
                    break;

                case OpCode::kOpGetGlobal:
                    handler_getglobal(S, *frame, instr.a(), instr.const_or_proto_index());
                    break;
                case OpCode::kOpSetGlobal:
                    handler_setglobal(S, *frame, instr.a(), instr.const_or_proto_index());
                    break;

                case OpCode::kOpGetUpval:
                    handler_getupval(S, *frame, instr.a(), instr.b());
                    break;
                case OpCode::kOpSetUpval:
                    handler_setupval(S, *frame, instr.a(), instr.b());
                    break;

                case OpCode::kOpGetField:
                    handler_getfield(S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpGetFieldI:
                    handler_getfieldi(S, *frame, instr.a(), instr.b(), static_cast<int32_t>(instr.small_const_index()));
                    break;
                case OpCode::kOpGetFieldS:
                    handler_getfields(S, *frame, instr.a(), instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpSetField:
                    handler_setfield(S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpSetFieldI:
                    handler_setfieldi(S, *frame, instr.a(), instr.b(), static_cast<int32_t>(instr.small_const_index()));
                    break;
                case OpCode::kOpSetFieldS:
                    handler_setfields(S, *frame, instr.a(), instr.b(), instr.small_const_index());
                    break;

                case OpCode::kOpNewTable:
                    handler_newtable(S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpSetList:
                    handler_setlist(S, *frame, instr.a(), instr.b(), instr.c());
                    break;

                case OpCode::kOpSelf:
                    handler_self(S, *frame, instr.a(), instr.b(), instr.c());
                    break;

                case OpCode::kOpAdd:
                    handler_add_fast(S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpSub:
                    handler_numeric_fast<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMul:
                    handler_numeric_fast<MetaMethodType::kMul, false, NumericMulOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpDiv:
                    handler_numeric_fast<MetaMethodType::kDiv, true, NumericDivOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMod:
                    handler_mod_fast(S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpPow:
                    handler_numeric_fast<MetaMethodType::kPow, false, NumericPowOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;

                case OpCode::kOpBand:
                    handler_bitwise_fast<MetaMethodType::kBAnd, BitwiseAndOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpBor:
                    handler_bitwise_fast<MetaMethodType::kBOr, BitwiseOrOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpBxor:
                    handler_bitwise_fast<MetaMethodType::kBXor, BitwiseXorOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpShl:
                    handler_bitwise_fast<MetaMethodType::kBShl, BitwiseShlOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpShr:
                    handler_bitwise_fast<MetaMethodType::kBShr, BitwiseShrOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;

                case OpCode::kOpUnm:
                    handler_unm(S, *frame, instr.a(), instr.b());
                    break;
                case OpCode::kOpBnot:
                    handler_bnot(S, *frame, instr.a(), instr.b());
                    break;
                case OpCode::kOpLen:
                    handler_len(S, *frame, instr.a(), instr.b());
                    break;
                case OpCode::kOpToString:
                    handler_tostring(S, *frame, instr.a(), instr.b());
                    break;

                case OpCode::kOpToNumber:
                    handler_tonumber(S, *frame, instr.a(), instr.b());
                    break;

                case OpCode::kOpAddImm:
                    handler_add_imm(S, *frame, instr.a(), instr.b(), instr.signed_immediate_9bit());
                    break;
                case OpCode::kOpSubImm:
                    handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_imm>(
                        S, *frame, instr.a(), instr.b(), instr.signed_immediate_9bit());
                    break;

                case OpCode::kOpMMSub:
                    handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMMul:
                    handler_numeric<MetaMethodType::kMul, false, NumericMulOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMDiv:
                    handler_numeric<MetaMethodType::kDiv, true, NumericDivOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMMod:
                    handler_mod(S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMPow:
                    handler_numeric<MetaMethodType::kPow, false, NumericPowOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMBand:
                    handler_bitwise<MetaMethodType::kBAnd, BitwiseAndOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMBor:
                    handler_bitwise<MetaMethodType::kBOr, BitwiseOrOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMBxor:
                    handler_bitwise<MetaMethodType::kBXor, BitwiseXorOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMShl:
                    handler_bitwise<MetaMethodType::kBShl, BitwiseShlOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpMMShr:
                    handler_bitwise<MetaMethodType::kBShr, BitwiseShrOp, operand_reg, operand_reg>(
                        S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpDefer:
                    handler_defer(*frame, instr.a());
                    break;
                case OpCode::kOpDeferCall:
                    handler_defercall(S, *frame, instr.a());
                    break;
                case OpCode::kOpEndDefer:
                    handler_enddefer(S, *frame, instr.a());
                    break;
                case OpCode::kOpSaveRet:
                    handler_saveret(S, *frame, instr.a(), instr.b());
                    break;
                case OpCode::kOpRetSaved:
                    if (!handler_retsaved(S, *frame, entry_call_depth))
                    {
                        return;
                    }
                    if (callstack.size() <= stop_depth)
                    {
                        return;
                    }
                    --frame;
                    code = frame->proto->code.data();
                    break;
                case OpCode::kOpEndUnwind:
                    return;
                case OpCode::kOpMMAdd:
                    handler_add(S, *frame, instr.a(), instr.b(), instr.c());
                    break;
                case OpCode::kOpAddKS:
                    handler_add_ks(S, *frame, instr.a(), instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpAddKI:
                    handler_numeric<MetaMethodType::kAdd, false, NumericAddOp, operand_reg, operand_const_int>(
                        S, *frame, instr.a(), instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpSubKI:
                    handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_const_int>(
                        S, *frame, instr.a(), instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpAddKF:
                    handler_numeric<MetaMethodType::kAdd, false, NumericAddOp, operand_reg, operand_const_fp>(
                        S, *frame, instr.a(), instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpSubKF:
                    handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_const_fp>(
                        S, *frame, instr.a(), instr.b(), instr.small_const_index());
                    break;

                case OpCode::kOpIncLocal:
                    handler_inc_local(S, *frame, instr.a());
                    break;
                case OpCode::kOpDecLocal:
                    handler_dec_local(S, *frame, instr.a());
                    break;
                case OpCode::kOpIncGlobal:
                    handler_inc_global(S, *frame, instr.large_const_index());
                    break;
                case OpCode::kOpDecGlobal:
                    handler_dec_global(S, *frame, instr.large_const_index());
                    break;
                case OpCode::kOpIncUpvalue:
                    handler_inc_upvalue(S, *frame, instr.a());
                    break;
                case OpCode::kOpDecUpvalue:
                    handler_dec_upvalue(S, *frame, instr.a());
                    break;
                case OpCode::kOpAddLocal:
                    handler_add(S, *frame, instr.a(), instr.a(), instr.b());
                    break;

                case OpCode::kOpEq:
                    handler_cmp<MetaMethodType::kEq, CmpEqOp, operand_reg, operand_reg>(S, *frame, instr.b(), instr.c());
                    break;
                case OpCode::kOpNe:
                    handler_cmp<MetaMethodType::kEq, CmpNeOp, operand_reg, operand_reg>(S, *frame, instr.b(), instr.c());
                    break;
                case OpCode::kOpLt:
                    handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_reg>(S, *frame, instr.b(), instr.c());
                    break;
                case OpCode::kOpGe:
                    handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_reg>(S, *frame, instr.b(), instr.c());
                    break;
                case OpCode::kOpLe:
                    handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_reg>(S, *frame, instr.b(), instr.c());
                    break;
                case OpCode::kOpGt:
                    handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_reg>(S, *frame, instr.b(), instr.c());
                    break;

                case OpCode::kOpLTI:
                    handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_const_int>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpGEI:
                    handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_const_int>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpLEI:
                    handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_const_int>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpGTI:
                    handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_const_int>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpLTF:
                    handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_const_fp>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpGEF:
                    handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_const_fp>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpLEF:
                    handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_const_fp>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;
                case OpCode::kOpGTF:
                    handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_const_fp>(
                        S, *frame, instr.b(), instr.small_const_index());
                    break;

                case OpCode::kOpLTImm:
                    handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_imm>(
                        S, *frame, instr.a(), instr.signed_immediate());
                    break;
                case OpCode::kOpGeImm:
                    handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_imm>(
                        S, *frame, instr.a(), instr.signed_immediate());
                    break;
                case OpCode::kOpLEImm:
                    handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_imm>(
                        S, *frame, instr.a(), instr.signed_immediate());
                    break;
                case OpCode::kOpGtImm:
                    handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_imm>(
                        S, *frame, instr.a(), instr.signed_immediate());
                    break;
                case OpCode::kOpEqImm:
                    handler_cmp<MetaMethodType::kEq, CmpEqOp, operand_reg, operand_imm>(
                        S, *frame, instr.a(), instr.signed_immediate());
                    break;
                case OpCode::kOpNeImm:
                    handler_cmp<MetaMethodType::kEq, CmpNeOp, operand_reg, operand_imm>(
                        S, *frame, instr.a(), instr.signed_immediate());
                    break;

                case OpCode::kOpTest:
                    handler_test(S, *frame, instr.a(), instr.b() != 0);
                    break;
                case OpCode::kOpTestSet:
                    handler_testset(S, *frame, instr.a(), instr.b(), instr.c() != 0);
                    break;

                case OpCode::kOpJmp:
                    handler_jmp(*frame, instr.jump_offset());
                    break;

                case OpCode::kOpForPrep:
                    handler_forprep(S, *frame, instr.a(), instr.signed_offset());
                    break;
                case OpCode::kOpForLoop:
                    handler_forloop(S, *frame, instr.a(), instr.signed_offset());
                    break;

                case OpCode::kOpClosure:
                    handler_closure(S, *frame, instr.a(), instr.const_or_proto_index());
                    break;

                case OpCode::kOpCall:
                {
                    const bool self_call = instr.flag_bit();
                    frame = handler_call(S, *frame, instr.a(), instr.b(), instr.c(), self_call);
                    code = frame->proto->code.data();
                    break;
                }

                case OpCode::kOpTailCall:
                    if (!handler_tailcall(S, *frame, instr.a(), instr.b(), !!instr.c(), entry_call_depth))
                    {
                        return;
                    }
                    if (callstack.size() <= stop_depth)
                    {
                        return;
                    }
                    frame = &callstack.back();
                    code = frame->proto->code.data();
                    break;

                case OpCode::kOpReturn:
                    if (!handler_return(S, *frame, instr.a(), instr.b(), entry_call_depth))
                    {
                        return;
                    }
                    if (callstack.size() <= stop_depth)
                    {
                        return;
                    }
                    --frame;
                    code = frame->proto->code.data();
                    break;

                case OpCode::kOpReturn0:
                    if (!handler_return0(S, *frame, entry_call_depth))
                    {
                        return;
                    }
                    if (callstack.size() <= stop_depth)
                    {
                        return;
                    }
                    --frame;
                    code = frame->proto->code.data();
                    break;

                case OpCode::kOpReturn1:
                    if (!handler_return1(S, *frame, instr.a(), entry_call_depth))
                    {
                        return;
                    }
                    if (callstack.size() <= stop_depth)
                    {
                        return;
                    }
                    --frame;
                    code = frame->proto->code.data();
                    break;

                case OpCode::kOpVararg:
                    handler_vararg(S, *frame, instr.a(), instr.b());
                    break;

                case OpCode::kOpVarargPrep:
                    handler_varargprep(S, *frame, instr.a());
                    break;

                case OpCode::kOpVarargExpand:
                    handler_varargexpand(S, *frame, instr.a(), instr.b());
                    break;

#ifndef NDEBUG
                default:
                    assert(false && "Unknown opcode");
                    break;
#endif
            }
        }
    }

    template<bool TDebugMode>
    inline static void execute_closure(State* S, const Value& func_value, int args, int nresults)
    {
        auto& callstack = S->call_stack;

        auto* closure_data = func_value.get_closure();
        assert(closure_data != nullptr);
        assert(closure_data->proto != nullptr && "Closure proto should not be nullptr");
        assert(!closure_data->proto->code.empty() && "Empty function proto (compiler bug)");

        const auto entry_call_depth = static_cast<uint32_t>(callstack.size());
        const auto num_args = static_cast<uint32_t>(args);
        const auto new_base = static_cast<uint32_t>(S->stack.size()) - num_args - 1;

        assert(new_base < S->stack.size() && "Frame base out of range");

        const auto* proto = closure_data->proto;
        const auto nres = (nresults == kMultRet) ? static_cast<uint8_t>(kMultRet) : static_cast<uint8_t>(nresults);
        setup_call_frame(S, proto, new_base, num_args, new_base, nres);
        prepare_call(S, proto->max_stack_size, new_base, num_args);

#if BEHL_JIT_SUPPORTED
        if constexpr (!TDebugMode)
        {
            if (jit_try_execute(S, proto))
            {
                return;
            }
        }
#endif

        interpreter_loop<TDebugMode>(S, entry_call_depth, entry_call_depth);
    }

    bool perform_call(State* S, int nargs, int nresults, size_t func_pos)
    {
        auto& stack = S->stack;

        assert(func_pos < stack.size() && "perform_call: function position out of range");
        const Value& func = stack[func_pos];

        gc_step(S);

        if (func.is_closure())
        {
            if (S->debug.enabled) [[unlikely]]
            {
                execute_closure<true>(S, func, nargs, nresults);
            }
            else
            {
                execute_closure<false>(S, func, nargs, nresults);
            }
        }
        else if (func.is_cfunction())
        {
            execute_native(S, func, nargs, nresults);
        }
        else
        {
            throw_bad_call(func, S->call_stack.empty() ? CallFrame{} : S->call_stack.back(), S);
        }

        return true;
    }

    void run_interpreter(State* S, uint32_t entry_call_depth, uint32_t stop_depth)
    {
        interpreter_loop<false>(S, entry_call_depth, stop_depth);
    }

    void unwind_call_frames(State* S, size_t target_depth, std::exception_ptr& pending)
    {
        while (S->call_stack.size() > target_depth)
        {
            const auto index = static_cast<uint32_t>(S->call_stack.size() - 1);

            for (;;)
            {
                CallFrame& frame = S->call_stack[index];

                if (frame.proto == nullptr || frame.defer_mask == 0 || frame.proto->defer_unwind_pc == 0)
                {
                    break;
                }

                frame.pc = frame.proto->defer_unwind_pc;
                S->stack.resize(S, frame.base + frame.proto->max_stack_size);

                try
                {
                    if (S->debug.enabled) [[unlikely]]
                    {
                        interpreter_loop<true>(S, index, index);
                    }
                    else
                    {
                        interpreter_loop<false>(S, index, index);
                    }
                    break;
                }
                catch (...)
                {
                    pending = std::current_exception();
                    unwind_call_frames(S, static_cast<size_t>(index) + 1, pending);
                }
            }

            S->call_stack.resize(S, index);
        }
    }

} // namespace behl
