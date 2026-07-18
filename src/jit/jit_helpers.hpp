#pragma once

#include <cstdint>

namespace behl
{
    struct State;
    struct CallFrame;

    using JitOpFn = uint32_t (*)(State* S, uint32_t raw, uint32_t pc_next);

    uint32_t jit_return_entry_depth(State* S, const CallFrame& frame) noexcept;

    int64_t jit_i64_mod(int64_t a, int64_t b) noexcept;

    uint32_t jit_op_move(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_loadi(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_loadf(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_loads(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_loadbool(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_loadnil(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_loadimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_getglobal(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_setglobal(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_getupval(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_setupval(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_getfield(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_getfieldi(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_getfields(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_setfield(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_setfieldi(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_setfields(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_newtable(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_setlist(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_self(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_add(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_sub(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_mul(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_div(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_mod(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_pow(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_band(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_bor(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_bxor(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_shl(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_shr(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_unm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_bnot(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_len(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_tostring(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_tonumber(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_addimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_subimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_addki(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_subki(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_addkf(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_subkf(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_inclocal(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_declocal(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_incglobal(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_decglobal(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_incupvalue(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_decupvalue(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_addlocal(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_eq(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_ne(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_lt(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_ge(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_le(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_gt(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_lti(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_gei(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_lei(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_gti(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_ltf(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_gef(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_lef(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_gtf(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_ltimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_geimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_leimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_gtimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_eqimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_neimm(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_test(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_testset(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_forprep(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_forloop(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_call(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_tailcall(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_closure(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_return(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_return0(State* S, uint32_t raw, uint32_t pc_next) noexcept;
    uint32_t jit_op_return1(State* S, uint32_t raw, uint32_t pc_next) noexcept;

} // namespace behl
