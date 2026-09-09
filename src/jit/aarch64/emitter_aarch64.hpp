#pragma once

#include <cstddef>
#include "common/vector.hpp"

#include <cstdint>

namespace behl
{
    struct State;
    enum class A64Reg : uint8_t
    {
        x0 = 0,
        x1 = 1,
        x2 = 2,
        x3 = 3,
        x4 = 4,
        x5 = 5,
        x6 = 6,
        x7 = 7,
        x8 = 8,
        x9 = 9,
        x10 = 10,
        x11 = 11,
        x12 = 12,
        x13 = 13,
        x14 = 14,
        x15 = 15,
        x16 = 16,
        x17 = 17,
        x19 = 19,
        x20 = 20,
        x21 = 21,
        x29 = 29,
        x30 = 30,
        sp = 31,
    };

    enum class A64Vec : uint8_t
    {
        d0 = 0,
        d1 = 1,
        d2 = 2,
        d3 = 3,
        d4 = 4,
        d5 = 5,
        d16 = 16,
    };

    enum class A64Cond : uint8_t
    {
        eq = 0x0,
        ne = 0x1,
        hs = 0x2,
        lo = 0x3,
        mi = 0x4,
        pl = 0x5,
        vs = 0x6,
        vc = 0x7,
        hi = 0x8,
        ls = 0x9,
        ge = 0xA,
        lt = 0xB,
        gt = 0xC,
        le = 0xD,
    };

    struct A64Label
    {
        uint32_t id;
    };

    struct A64Mem
    {
        A64Reg base;
        int32_t disp;
    };

    constexpr A64Mem mem(A64Reg base, int32_t disp = 0) noexcept
    {
        return A64Mem{ base, disp };
    }

    struct A64Emitter
    {
        explicit A64Emitter(State* state)
            : buffer_(state)
            , labels_(state)
            , nodes_(state)
            , node_prefix_(state)
        {
        }

        void mov(A64Reg dst, A64Reg src);
        void mov(A64Reg dst, uint64_t imm);
        void mov32(A64Reg dst, uint32_t imm);

        void ldr(A64Reg dst, A64Mem src);
        void str(A64Reg src, A64Mem dst);
        void ldrw(A64Reg dst, A64Mem src);
        void strw(A64Reg src, A64Mem dst);
        void ldrb(A64Reg dst, A64Mem src);
        void ldr_d(A64Vec dst, A64Mem src);
        void str_d(A64Vec src, A64Mem dst);
        void ldr_q(A64Vec dst, A64Mem src);
        void str_q(A64Vec src, A64Mem dst);

        void stp_pre(A64Reg r1, A64Reg r2, A64Reg base, int32_t imm);
        void ldp_post(A64Reg r1, A64Reg r2, A64Reg base, int32_t imm);

        void add(A64Reg dst, A64Reg src, uint32_t imm);
        void sub(A64Reg dst, A64Reg src, uint32_t imm);
        void add(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void sub(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void and_(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void orr(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void eor(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void mul(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void madd(A64Reg dst, A64Reg lhs, A64Reg rhs, A64Reg addend);
        void msub(A64Reg dst, A64Reg lhs, A64Reg rhs, A64Reg minuend);
        void sdiv(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void udiv(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void lslv(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void asrv(A64Reg dst, A64Reg lhs, A64Reg rhs);
        void lsl(A64Reg dst, A64Reg src, uint8_t shift);

        void cmp(A64Reg lhs, A64Reg rhs);
        void cmp(A64Reg reg, uint32_t imm);
        void cmn(A64Reg reg, uint32_t imm);
        void cmpw(A64Reg lhs, A64Reg rhs);
        void cmpw(A64Reg reg, uint32_t imm);
        void cmnw(A64Reg reg, uint32_t imm);

        void fadd(A64Vec dst, A64Vec lhs, A64Vec rhs);
        void fsub(A64Vec dst, A64Vec lhs, A64Vec rhs);
        void fmul(A64Vec dst, A64Vec lhs, A64Vec rhs);
        void fdiv(A64Vec dst, A64Vec lhs, A64Vec rhs);
        void fcmp(A64Vec lhs, A64Vec rhs);
        void fmov(A64Vec dst, A64Reg src);
        void fmov_d(A64Vec dst, A64Vec src);
        void scvtf(A64Vec dst, A64Reg src);

        void call(uintptr_t target);
        void ret();

        A64Label new_label();
        void bind(A64Label label);

        void b(A64Label label);
        void bcond(A64Cond cond, A64Label label);

        size_t size();

        size_t finalize(uintptr_t address, uint8_t* buf, size_t buf_size);

    private:
        enum class NodeType : uint8_t
        {
            kBranch,
            kBranchCond,
            kCall,
        };

        struct PatchNode
        {
            uint32_t lit_pos;
            uint32_t label;
            uintptr_t target;
            NodeType type;
            uint8_t cc;
            uint8_t words;
        };

        struct LabelState
        {
            uint32_t lit_offset;
            uint32_t node_index;
            bool bound;
        };

        void emit(uint32_t word);
        void emit_ls(uint32_t base_op, uint8_t scale_log, uint8_t rt, A64Mem addr);
        void relax(uintptr_t address);
        uint32_t node_offset(uint32_t index) const noexcept;
        uint32_t label_offset(uint32_t id) const noexcept;

        AutoVector<uint32_t> buffer_;
        AutoVector<LabelState> labels_;
        AutoVector<PatchNode> nodes_;
        AutoVector<uint32_t> node_prefix_;
        bool relax_failed_{};
    };

} // namespace behl
