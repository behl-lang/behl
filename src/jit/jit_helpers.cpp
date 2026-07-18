#include "jit_helpers.hpp"

#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "jit.hpp"
#include "state.hpp"
#include "vm/bytecode.hpp"
#include "vm/frame.hpp"
#include "vm/vm.hpp"
#include "vm/vm_arithmetic.hpp"
#include "vm/vm_bitwise.hpp"
#include "vm/vm_controlflow.hpp"
#include "vm/vm_detail.hpp"
#include "vm/vm_handlers.hpp"
#include "vm/vm_load.hpp"
#include "vm/vm_metatable.hpp"
#include "vm/vm_operands.hpp"
#include "vm/vm_table.hpp"
#include "vm/vm_upvalues.hpp"

#include <exception>

namespace behl
{
#define BEHL_JIT_WRAP(NAME, ...)                                                                                               \
    uint32_t NAME(State* S, uint32_t raw, uint32_t pc_next) noexcept                                                           \
    {                                                                                                                          \
        const Instruction instr{ raw };                                                                                        \
        (void)instr;                                                                                                           \
        try                                                                                                                    \
        {                                                                                                                      \
            CallFrame& frame = S->call_stack.back();                                                                           \
            frame.pc = pc_next;                                                                                                \
            __VA_ARGS__;                                                                                                       \
            return S->call_stack.back().pc;                                                                                    \
        }                                                                                                                      \
        catch (...)                                                                                                            \
        {                                                                                                                      \
            S->jit_exception = std::current_exception();                                                                       \
            return kJitError;                                                                                                  \
        }                                                                                                                      \
    }

    BEHL_JIT_WRAP(jit_op_move, handler_move(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_loadi, handler_loadi(S, frame, instr.a(), instr.const_or_proto_index()))
    BEHL_JIT_WRAP(jit_op_loadf, handler_loadf(S, frame, instr.a(), instr.const_or_proto_index()))
    BEHL_JIT_WRAP(jit_op_loads, handler_loadk(S, frame, instr.a(), instr.const_or_proto_index()))
    BEHL_JIT_WRAP(jit_op_loadbool, handler_loadbool(S, frame, instr.a(), instr.bool_value(), instr.skip_next()))
    BEHL_JIT_WRAP(jit_op_loadnil, handler_loadnil(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_loadimm, handler_load_imm(S, frame, instr.a(), instr.signed_immediate()))
    BEHL_JIT_WRAP(jit_op_getglobal, handler_getglobal(S, frame, instr.a(), instr.const_or_proto_index()))
    BEHL_JIT_WRAP(jit_op_setglobal, handler_setglobal(S, frame, instr.a(), instr.const_or_proto_index()))
    BEHL_JIT_WRAP(jit_op_getupval, handler_getupval(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_setupval, handler_setupval(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_getfield, handler_getfield(S, frame, instr.a(), instr.b(), instr.c()))
    BEHL_JIT_WRAP(
        jit_op_getfieldi, handler_getfieldi(S, frame, instr.a(), instr.b(), static_cast<int32_t>(instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_getfields, handler_getfields(S, frame, instr.a(), instr.b(), instr.small_const_index()))
    BEHL_JIT_WRAP(jit_op_setfield, handler_setfield(S, frame, instr.a(), instr.b(), instr.c()))
    BEHL_JIT_WRAP(
        jit_op_setfieldi, handler_setfieldi(S, frame, instr.a(), instr.b(), static_cast<int32_t>(instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_setfields, handler_setfields(S, frame, instr.a(), instr.b(), instr.small_const_index()))
    BEHL_JIT_WRAP(jit_op_newtable, handler_newtable(S, frame, instr.a(), instr.b(), instr.c()))
    BEHL_JIT_WRAP(jit_op_setlist, handler_setlist(S, frame, instr.a(), instr.b(), instr.c()))
    BEHL_JIT_WRAP(jit_op_self, handler_self(S, frame, instr.a(), instr.b(), instr.c()))
    BEHL_JIT_WRAP(jit_op_add, handler_add(S, frame, instr.a(), instr.b(), instr.c()))
    BEHL_JIT_WRAP(jit_op_sub,
        (handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_mul,
        (handler_numeric<MetaMethodType::kMul, false, NumericMulOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_div,
        (handler_numeric<MetaMethodType::kDiv, true, NumericDivOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_mod, handler_mod(S, frame, instr.a(), instr.b(), instr.c()))
    BEHL_JIT_WRAP(jit_op_pow,
        (handler_numeric<MetaMethodType::kPow, false, NumericPowOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_band,
        (handler_bitwise<MetaMethodType::kBAnd, BitwiseAndOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_bor,
        (handler_bitwise<MetaMethodType::kBOr, BitwiseOrOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_bxor,
        (handler_bitwise<MetaMethodType::kBXor, BitwiseXorOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_shl,
        (handler_bitwise<MetaMethodType::kBShl, BitwiseShlOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_shr,
        (handler_bitwise<MetaMethodType::kBShr, BitwiseShrOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_unm, handler_unm(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_bnot, handler_bnot(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_len, handler_len(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_tostring, handler_tostring(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_tonumber, handler_tonumber(S, frame, instr.a(), instr.b()))
    BEHL_JIT_WRAP(jit_op_addimm, handler_add_imm(S, frame, instr.a(), instr.b(), instr.signed_immediate_9bit()))
    BEHL_JIT_WRAP(jit_op_subimm,
        (handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_imm>(
            S, frame, instr.a(), instr.b(), instr.signed_immediate_9bit())))
    BEHL_JIT_WRAP(jit_op_addki,
        (handler_numeric<MetaMethodType::kAdd, false, NumericAddOp, operand_reg, operand_const_int>(
            S, frame, instr.a(), instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_subki,
        (handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_const_int>(
            S, frame, instr.a(), instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_addkf,
        (handler_numeric<MetaMethodType::kAdd, false, NumericAddOp, operand_reg, operand_const_fp>(
            S, frame, instr.a(), instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_subkf,
        (handler_numeric<MetaMethodType::kSub, false, NumericSubOp, operand_reg, operand_const_fp>(
            S, frame, instr.a(), instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_inclocal, handler_inc_local(S, frame, instr.a()))
    BEHL_JIT_WRAP(jit_op_declocal, handler_dec_local(S, frame, instr.a()))
    BEHL_JIT_WRAP(jit_op_incglobal, handler_inc_global(S, frame, instr.large_const_index()))
    BEHL_JIT_WRAP(jit_op_decglobal, handler_dec_global(S, frame, instr.large_const_index()))
    BEHL_JIT_WRAP(jit_op_incupvalue, handler_inc_upvalue(S, frame, instr.a()))
    BEHL_JIT_WRAP(jit_op_decupvalue, handler_dec_upvalue(S, frame, instr.a()))
    BEHL_JIT_WRAP(jit_op_addlocal,
        (handler_numeric<MetaMethodType::kAdd, false, NumericAddOp, operand_reg, operand_reg>(
            S, frame, instr.a(), instr.a(), instr.b())))
    BEHL_JIT_WRAP(
        jit_op_eq, (handler_cmp<MetaMethodType::kEq, CmpEqOp, operand_reg, operand_reg>(S, frame, instr.b(), instr.c())))
    BEHL_JIT_WRAP(
        jit_op_ne, (handler_cmp<MetaMethodType::kEq, CmpNeOp, operand_reg, operand_reg>(S, frame, instr.b(), instr.c())))
    BEHL_JIT_WRAP(
        jit_op_lt, (handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_reg>(S, frame, instr.b(), instr.c())))
    BEHL_JIT_WRAP(
        jit_op_ge, (handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_reg>(S, frame, instr.b(), instr.c())))
    BEHL_JIT_WRAP(
        jit_op_le, (handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_reg>(S, frame, instr.b(), instr.c())))
    BEHL_JIT_WRAP(
        jit_op_gt, (handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_reg>(S, frame, instr.b(), instr.c())))
    BEHL_JIT_WRAP(jit_op_lti,
        (handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_const_int>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_gei,
        (handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_const_int>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_lei,
        (handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_const_int>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_gti,
        (handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_const_int>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_ltf,
        (handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_const_fp>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_gef,
        (handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_const_fp>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_lef,
        (handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_const_fp>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_gtf,
        (handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_const_fp>(
            S, frame, instr.b(), instr.small_const_index())))
    BEHL_JIT_WRAP(jit_op_ltimm,
        (handler_cmp<MetaMethodType::kLt, CmpLtOp, operand_reg, operand_imm>(S, frame, instr.a(), instr.signed_immediate())))
    BEHL_JIT_WRAP(jit_op_geimm,
        (handler_cmp<MetaMethodType::kLt, CmpGeOp, operand_reg, operand_imm>(S, frame, instr.a(), instr.signed_immediate())))
    BEHL_JIT_WRAP(jit_op_leimm,
        (handler_cmp<MetaMethodType::kLe, CmpLeOp, operand_reg, operand_imm>(S, frame, instr.a(), instr.signed_immediate())))
    BEHL_JIT_WRAP(jit_op_gtimm,
        (handler_cmp<MetaMethodType::kLt, CmpGtOp, operand_reg, operand_imm>(S, frame, instr.a(), instr.signed_immediate())))
    BEHL_JIT_WRAP(jit_op_eqimm,
        (handler_cmp<MetaMethodType::kEq, CmpEqOp, operand_reg, operand_imm>(S, frame, instr.a(), instr.signed_immediate())))
    BEHL_JIT_WRAP(jit_op_neimm,
        (handler_cmp<MetaMethodType::kEq, CmpNeOp, operand_reg, operand_imm>(S, frame, instr.a(), instr.signed_immediate())))
    BEHL_JIT_WRAP(jit_op_test, handler_test(S, frame, instr.a(), instr.b() != 0))
    BEHL_JIT_WRAP(jit_op_testset, handler_testset(S, frame, instr.a(), instr.b(), instr.c() != 0))
    BEHL_JIT_WRAP(jit_op_forprep, handler_forprep(S, frame, instr.a(), instr.signed_offset()))
    BEHL_JIT_WRAP(jit_op_forloop, handler_forloop(S, frame, instr.a(), instr.signed_offset()))
    BEHL_JIT_WRAP(jit_op_closure, handler_closure(S, frame, instr.a(), instr.const_or_proto_index()))

#undef BEHL_JIT_WRAP

    int64_t jit_i64_mod(int64_t a, int64_t b) noexcept
    {
        if (b == -1)
        {
            return 0;
        }
        return a % b;
    }

    uint32_t jit_return_entry_depth(State* S, const CallFrame& frame) noexcept
    {
        const auto size = S->call_stack.size();
        if (size >= 2)
        {
            const CallFrame& caller = S->call_stack[size - 2];
            if (caller.proto != nullptr && frame.call_pos < caller.base + caller.proto->max_stack_size)
            {
                return static_cast<uint32_t>(size - 2);
            }
        }
        return static_cast<uint32_t>(size - 1);
    }

    uint32_t jit_op_return(State* S, uint32_t raw, uint32_t pc_next) noexcept
    {
        const Instruction instr{ raw };
        try
        {
            CallFrame& frame = S->call_stack.back();
            frame.pc = pc_next;
            handler_return(S, frame, instr.a(), instr.b(), jit_return_entry_depth(S, frame));
            return 0;
        }
        catch (...)
        {
            S->jit_exception = std::current_exception();
            return kJitError;
        }
    }

    uint32_t jit_op_return0(State* S, uint32_t raw, uint32_t pc_next) noexcept
    {
        (void)raw;
        try
        {
            CallFrame& frame = S->call_stack.back();
            frame.pc = pc_next;
            handler_return0(S, frame, jit_return_entry_depth(S, frame));
            return 0;
        }
        catch (...)
        {
            S->jit_exception = std::current_exception();
            return kJitError;
        }
    }

    uint32_t jit_op_return1(State* S, uint32_t raw, uint32_t pc_next) noexcept
    {
        const Instruction instr{ raw };
        try
        {
            CallFrame& frame = S->call_stack.back();
            frame.pc = pc_next;
            handler_return1(S, frame, instr.a(), jit_return_entry_depth(S, frame));
            return 0;
        }
        catch (...)
        {
            S->jit_exception = std::current_exception();
            return kJitError;
        }
    }

    uint32_t jit_op_call(State* S, uint32_t raw, uint32_t pc_next) noexcept
    {
        const Instruction instr{ raw };
        try
        {
            auto& callstack = S->call_stack;
            CallFrame& frame = callstack.back();
            frame.pc = pc_next;

            const auto depth = static_cast<uint32_t>(callstack.size());
            handler_call(S, frame, instr.a(), instr.b(), instr.c(), instr.flag_bit());

            if (callstack.size() > depth)
            {
                run_interpreter(S, depth - 1, depth);
            }
            return callstack.back().pc;
        }
        catch (...)
        {
            S->jit_exception = std::current_exception();
            return kJitError;
        }
    }

    uint32_t jit_op_tailcall(State* S, uint32_t raw, uint32_t pc_next) noexcept
    {
        const Instruction instr{ raw };
        try
        {
            CallFrame& frame = S->call_stack.back();
            frame.pc = pc_next;

            const GCProto* old_proto = frame.proto;
            const auto entry_depth = jit_return_entry_depth(S, frame);
            const auto depth_before = S->call_stack.size();
            if (!handler_tailcall(S, frame, instr.a(), instr.b(), !!instr.c(), entry_depth))
            {
                return kJitTailReturned;
            }
            if (S->call_stack.size() < depth_before)
            {
                return kJitTailReturned;
            }

            const CallFrame& top = S->call_stack.back();
            if (top.proto == old_proto)
            {
                return top.pc;
            }
            return kJitTailReplaced;
        }
        catch (...)
        {
            S->jit_exception = std::current_exception();
            return kJitError;
        }
    }

} // namespace behl
