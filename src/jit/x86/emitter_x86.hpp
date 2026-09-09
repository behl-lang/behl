#pragma once

#include "common/vector.hpp"

#include <cstddef>
#include <cstdint>

namespace behl
{
    struct State;
    enum class GpReg : uint8_t
    {
        r0 = 0,
        r1 = 1,
        r2 = 2,
        r3 = 3,
        r4 = 4,
        r5 = 5,
        r6 = 6,
        r7 = 7,
        r8 = 8,
        r9 = 9,
        r10 = 10,
        r11 = 11,
        r12 = 12,
        r13 = 13,
        r14 = 14,
        r15 = 15,
    };

    enum class XmmReg : uint8_t
    {
        xmm0 = 0,
        xmm1 = 1,
        xmm2 = 2,
        xmm3 = 3,
        xmm4 = 4,
        xmm5 = 5,
    };

    enum class Cond : uint8_t
    {
        o = 0x0,
        no = 0x1,
        b = 0x2,
        ae = 0x3,
        e = 0x4,
        ne = 0x5,
        be = 0x6,
        a = 0x7,
        s = 0x8,
        ns = 0x9,
        p = 0xA,
        np = 0xB,
        l = 0xC,
        ge = 0xD,
        le = 0xE,
        g = 0xF,
    };

    struct Label
    {
        uint32_t id;
    };

    // scale of 0 means there is no index register.
    struct Mem
    {
        GpReg base;
        int32_t disp;
        GpReg index;
        uint8_t scale;
    };

    constexpr Mem mem(GpReg base, int32_t disp = 0) noexcept
    {
        return Mem{ base, disp, GpReg::r0, 0 };
    }

    constexpr Mem mem(GpReg base, GpReg index, uint8_t scale, int32_t disp = 0) noexcept
    {
        return Mem{ base, disp, index, scale };
    }

    struct X86Emitter
    {
        explicit X86Emitter(State* state, bool mode64 = sizeof(void*) == 8) noexcept
            : buffer_(state)
            , labels_(state)
            , nodes_(state)
            , node_prefix_(state)
            , mode64_(mode64)
        {
        }

        void mov(GpReg dst, GpReg src);
        void mov(GpReg dst, uint64_t imm);
        void mov(GpReg dst, Mem src);
        void lea(GpReg dst, Mem src);
        void mov(Mem dst, GpReg src);
        void mov(Mem dst, int32_t imm);
        void mov32(GpReg dst, uint32_t imm);
        void mov32(GpReg dst, Mem src);
        void mov32(Mem dst, uint32_t imm);

        void movups(XmmReg dst, Mem src);
        void movups(Mem dst, XmmReg src);
        void movsd(XmmReg dst, Mem src);
        void movsd(Mem dst, XmmReg src);
        void movsd_reg(XmmReg dst, XmmReg src);
        void movq(XmmReg dst, GpReg src);
        void cvtsi2sd(XmmReg dst, Mem src);
        void fild_qword(Mem src);
        void fstp_qword(Mem dst);
        void addsd(XmmReg dst, XmmReg src);
        void subsd(XmmReg dst, XmmReg src);
        void mulsd(XmmReg dst, XmmReg src);
        void divsd(XmmReg dst, XmmReg src);
        void addsd(XmmReg dst, Mem src);
        void subsd(XmmReg dst, Mem src);
        void ucomisd(XmmReg lhs, XmmReg rhs);
        void ucomisd(XmmReg lhs, Mem rhs);

        void add(GpReg dst, int32_t imm);
        void add(GpReg dst, GpReg src);
        void add(Mem dst, GpReg src);
        void add(Mem dst, int32_t imm);
        void adc(GpReg dst, GpReg src);
        void adc(GpReg dst, int32_t imm);
        void sub(GpReg dst, GpReg src);
        void sbb(GpReg dst, GpReg src);
        void sub(GpReg dst, int32_t imm);
        void sub(Mem dst, int32_t imm);
        void cmp32(GpReg reg, uint32_t imm);
        void cmp(GpReg reg, int32_t imm);
        void cmp(Mem mem_op, int32_t imm);
        void cmp8(Mem mem_op, uint8_t imm);
        void shl(GpReg reg, uint8_t imm);
        void shl32(GpReg reg, uint8_t imm);
        void test(GpReg lhs, GpReg rhs);
        void dec(GpReg reg);
        void sar(GpReg reg, uint8_t imm);
        void shl_cl(GpReg reg);
        void sar_cl(GpReg reg);
        void shld_cl(GpReg dst, GpReg src);
        void shrd_cl(GpReg dst, GpReg src);
        void imul(GpReg dst, GpReg src, int32_t imm);
        void imul(GpReg dst, GpReg src);
        void imul(GpReg dst, Mem src);
        void mul(Mem src);
        void div(Mem src);
        void cqo();
        void idiv(GpReg divisor);
        void cmp(GpReg lhs, GpReg rhs);
        void add(GpReg dst, Mem src);
        void sub(GpReg dst, Mem src);
        void and_(GpReg dst, Mem src);
        void or_(GpReg dst, Mem src);
        void xor_(GpReg dst, Mem src);
        void cmp(GpReg lhs, Mem rhs);
        void xor_(GpReg dst, GpReg src);
        void and_(GpReg dst, GpReg src);
        void or_(GpReg dst, GpReg src);
        void and_(GpReg dst, int32_t imm);

        void push(GpReg reg);
        void push_imm(uint32_t imm);
        void pop(GpReg reg);

        void call(GpReg reg);
        void call(uintptr_t target);
        void ret();

        Label new_label();
        void bind(Label label);
        void align(uint8_t boundary);

        void jmp(Label label);
        void jcc(Cond cond, Label label);

        size_t size();

        size_t finalize(uintptr_t address, uint8_t* buf, size_t buf_size);

    private:
        struct PatchNode
        {
            uint32_t lit_pos;
            uint32_t label;
            uintptr_t target;
            uint8_t cc;
            bool is_jcc;
            bool is_call;
            uint8_t encoded_size;
            bool is_align{};
            uint8_t boundary{};
        };

        struct LabelState
        {
            uint32_t lit_offset;
            uint32_t node_index;
            bool bound;
        };

        void emit_branch(bool is_jcc, uint8_t cc, Label label);
        void relax(bool exact, uintptr_t address);

        void emit8(uint8_t value);
        void emit32(uint32_t value);
        void emit64(uint64_t value);
        void emit_rex(bool w, bool r, bool x, bool b);
        void emit_rex_opt(bool r, bool b);
        void emit_rex_opt(bool r, bool x, bool b);
        void emit_rex_natural(bool r, bool x, bool b);
        void emit_modrm(uint8_t mod, uint8_t reg, uint8_t rm);
        void emit_modrm_mem(uint8_t reg, const Mem& mem);

        AutoVector<uint8_t> buffer_;
        AutoVector<LabelState> labels_;
        AutoVector<PatchNode> nodes_;
        AutoVector<uint32_t> node_prefix_;
        uint8_t relax_mode_{};
        bool relax_failed_{};
        bool mode64_;
    };

} // namespace behl
