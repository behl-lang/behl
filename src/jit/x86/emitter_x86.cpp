#include "emitter_x86.hpp"

#include <cassert>
#include <cstring>

namespace behl
{
    static constexpr uint8_t reg_low(GpReg reg) noexcept
    {
        return static_cast<uint8_t>(reg) & 0x7;
    }

    static constexpr bool reg_ext(GpReg reg) noexcept
    {
        return static_cast<uint8_t>(reg) >= 8;
    }

    static constexpr uint8_t xmm_low(XmmReg reg) noexcept
    {
        return static_cast<uint8_t>(reg) & 0x7;
    }

    static bool mem_index_ext(const Mem& mem_op) noexcept
    {
        return mem_op.scale != 0 && (static_cast<uint8_t>(mem_op.index) & 0x8) != 0;
    }

    void X86Emitter::emit_rex_natural(bool r, bool x, bool b)
    {
        if (mode64_)
        {
            emit_rex(true, r, x, b);
        }
        else
        {
            assert(!r && !x && !b && "extended registers unavailable in 32 bit mode");
        }
    }

    void X86Emitter::mov(GpReg dst, GpReg src)
    {
        if (dst == src)
        {
            return;
        }

        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x89);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::mov(GpReg dst, uint64_t imm)
    {
        if (imm == 0)
        {
            xor_(dst, dst);
            return;
        }

        if (!mode64_)
        {
            assert(imm <= 0xFFFFFFFFu && "immediate exceeds 32 bit mode width");
            mov32(dst, static_cast<uint32_t>(imm));
            return;
        }

        if (imm <= 0xFFFFFFFFu)
        {
            mov32(dst, static_cast<uint32_t>(imm));
            return;
        }

        if (static_cast<int64_t>(imm) >= INT32_MIN && static_cast<int64_t>(imm) <= INT32_MAX)
        {
            emit_rex(true, false, false, reg_ext(dst));
            emit8(0xC7);
            emit_modrm(0b11, 0, reg_low(dst));
            emit32(static_cast<uint32_t>(static_cast<int32_t>(imm)));
            return;
        }

        emit_rex(true, false, false, reg_ext(dst));
        emit8(static_cast<uint8_t>(0xB8 + reg_low(dst)));
        emit64(imm);
    }

    void X86Emitter::lea(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x8D);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::mov(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x8B);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::mov(Mem dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), mem_index_ext(dst), reg_ext(dst.base));
        emit8(0x89);
        emit_modrm_mem(reg_low(src), dst);
    }

    void X86Emitter::mov(Mem dst, int32_t imm)
    {
        emit_rex_natural(false, mem_index_ext(dst), reg_ext(dst.base));
        emit8(0xC7);
        emit_modrm_mem(0, dst);
        emit32(static_cast<uint32_t>(imm));
    }

    void X86Emitter::mov32(GpReg dst, uint32_t imm)
    {
        emit_rex_opt(false, reg_ext(dst));
        emit8(static_cast<uint8_t>(0xB8 + reg_low(dst)));
        emit32(imm);
    }

    void X86Emitter::mov32(GpReg dst, Mem src)
    {
        emit_rex_opt(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x8B);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::mov32(Mem dst, uint32_t imm)
    {
        emit_rex_opt(false, mem_index_ext(dst), reg_ext(dst.base));
        emit8(0xC7);
        emit_modrm_mem(0, dst);
        emit32(imm);
    }

    void X86Emitter::movups(XmmReg dst, Mem src)
    {
        emit_rex_opt(false, mem_index_ext(src), reg_ext(src.base));
        emit8(0x0F);
        emit8(0x10);
        emit_modrm_mem(xmm_low(dst), src);
    }

    void X86Emitter::movups(Mem dst, XmmReg src)
    {
        emit_rex_opt(false, mem_index_ext(dst), reg_ext(dst.base));
        emit8(0x0F);
        emit8(0x11);
        emit_modrm_mem(xmm_low(src), dst);
    }

    void X86Emitter::movsd(XmmReg dst, Mem src)
    {
        emit8(0xF2);
        emit_rex_opt(false, mem_index_ext(src), reg_ext(src.base));
        emit8(0x0F);
        emit8(0x10);
        emit_modrm_mem(xmm_low(dst), src);
    }

    void X86Emitter::movsd(Mem dst, XmmReg src)
    {
        emit8(0xF2);
        emit_rex_opt(false, mem_index_ext(dst), reg_ext(dst.base));
        emit8(0x0F);
        emit8(0x11);
        emit_modrm_mem(xmm_low(src), dst);
    }

    void X86Emitter::movsd_reg(XmmReg dst, XmmReg src)
    {
        emit8(0xF2);
        emit8(0x0F);
        emit8(0x10);
        emit_modrm(0b11, xmm_low(dst), xmm_low(src));
    }

    void X86Emitter::movq(XmmReg dst, GpReg src)
    {
        assert(mode64_ && "movq xmm, r64 requires 64 bit mode");
        emit8(0x66);
        emit_rex(true, false, false, reg_ext(src));
        emit8(0x0F);
        emit8(0x6E);
        emit_modrm(0b11, xmm_low(dst), reg_low(src));
    }

    void X86Emitter::cvtsi2sd(XmmReg dst, Mem src)
    {
        assert(mode64_ && "cvtsi2sd from m64 requires 64 bit mode");
        emit8(0xF2);
        emit_rex(true, false, mem_index_ext(src), reg_ext(src.base));
        emit8(0x0F);
        emit8(0x2A);
        emit_modrm_mem(xmm_low(dst), src);
    }

    void X86Emitter::fild_qword(Mem src)
    {
        emit_rex_opt(false, mem_index_ext(src), reg_ext(src.base));
        emit8(0xDF);
        emit_modrm_mem(5, src);
    }

    void X86Emitter::fstp_qword(Mem dst)
    {
        emit_rex_opt(false, mem_index_ext(dst), reg_ext(dst.base));
        emit8(0xDD);
        emit_modrm_mem(3, dst);
    }

    void X86Emitter::addsd(XmmReg dst, XmmReg src)
    {
        emit8(0xF2);
        emit8(0x0F);
        emit8(0x58);
        emit_modrm(0b11, xmm_low(dst), xmm_low(src));
    }

    void X86Emitter::subsd(XmmReg dst, XmmReg src)
    {
        emit8(0xF2);
        emit8(0x0F);
        emit8(0x5C);
        emit_modrm(0b11, xmm_low(dst), xmm_low(src));
    }

    void X86Emitter::addsd(XmmReg dst, Mem src)
    {
        emit8(0xF2);
        emit_rex_opt(false, mem_index_ext(src), reg_ext(src.base));
        emit8(0x0F);
        emit8(0x58);
        emit_modrm_mem(xmm_low(dst), src);
    }

    void X86Emitter::subsd(XmmReg dst, Mem src)
    {
        emit8(0xF2);
        emit_rex_opt(false, mem_index_ext(src), reg_ext(src.base));
        emit8(0x0F);
        emit8(0x5C);
        emit_modrm_mem(xmm_low(dst), src);
    }

    void X86Emitter::mulsd(XmmReg dst, XmmReg src)
    {
        emit8(0xF2);
        emit8(0x0F);
        emit8(0x59);
        emit_modrm(0b11, xmm_low(dst), xmm_low(src));
    }

    void X86Emitter::divsd(XmmReg dst, XmmReg src)
    {
        emit8(0xF2);
        emit8(0x0F);
        emit8(0x5E);
        emit_modrm(0b11, xmm_low(dst), xmm_low(src));
    }

    void X86Emitter::ucomisd(XmmReg lhs, XmmReg rhs)
    {
        emit8(0x66);
        emit8(0x0F);
        emit8(0x2E);
        emit_modrm(0b11, xmm_low(lhs), xmm_low(rhs));
    }

    void X86Emitter::ucomisd(XmmReg lhs, Mem rhs)
    {
        emit8(0x66);
        emit_rex_opt(false, mem_index_ext(rhs), reg_ext(rhs.base));
        emit8(0x0F);
        emit8(0x2E);
        emit_modrm_mem(xmm_low(lhs), rhs);
    }

    void X86Emitter::add(GpReg dst, int32_t imm)
    {
        emit_rex_natural(false, false, reg_ext(dst));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm(0b11, 0, reg_low(dst));
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm(0b11, 0, reg_low(dst));
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::add(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x01);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::add(Mem dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), mem_index_ext(dst), reg_ext(dst.base));
        emit8(0x01);
        emit_modrm_mem(reg_low(src), dst);
    }

    void X86Emitter::add(Mem dst, int32_t imm)
    {
        emit_rex_natural(false, mem_index_ext(dst), reg_ext(dst.base));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm_mem(0, dst);
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm_mem(0, dst);
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::adc(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x11);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::adc(GpReg dst, int32_t imm)
    {
        emit_rex_natural(false, false, reg_ext(dst));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm(0b11, 2, reg_low(dst));
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm(0b11, 2, reg_low(dst));
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::sub(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x29);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::sbb(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x19);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::sub(GpReg dst, int32_t imm)
    {
        emit_rex_natural(false, false, reg_ext(dst));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm(0b11, 5, reg_low(dst));
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm(0b11, 5, reg_low(dst));
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::sub(Mem dst, int32_t imm)
    {
        emit_rex_natural(false, mem_index_ext(dst), reg_ext(dst.base));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm_mem(5, dst);
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm_mem(5, dst);
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::cmp32(GpReg reg, uint32_t imm)
    {
        emit_rex_opt(false, reg_ext(reg));
        emit8(0x81);
        emit_modrm(0b11, 7, reg_low(reg));
        emit32(imm);
    }

    void X86Emitter::cmp(GpReg reg, int32_t imm)
    {
        if (imm == 0)
        {
            test(reg, reg);
            return;
        }

        emit_rex_natural(false, false, reg_ext(reg));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm(0b11, 7, reg_low(reg));
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm(0b11, 7, reg_low(reg));
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::cmp(Mem mem_op, int32_t imm)
    {
        emit_rex_natural(false, mem_index_ext(mem_op), reg_ext(mem_op.base));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm_mem(7, mem_op);
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm_mem(7, mem_op);
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::cmp8(Mem mem_op, uint8_t imm)
    {
        emit_rex_opt(false, mem_index_ext(mem_op), reg_ext(mem_op.base));
        emit8(0x80);
        emit_modrm_mem(7, mem_op);
        emit8(imm);
    }

    void X86Emitter::test(GpReg lhs, GpReg rhs)
    {
        emit_rex_natural(reg_ext(rhs), false, reg_ext(lhs));
        emit8(0x85);
        emit_modrm(0b11, reg_low(rhs), reg_low(lhs));
    }

    void X86Emitter::dec(GpReg reg)
    {
        emit_rex_natural(false, false, reg_ext(reg));
        emit8(0xFF);
        emit_modrm(0b11, 1, reg_low(reg));
    }

    void X86Emitter::shl32(GpReg reg, uint8_t imm)
    {
        emit_rex_opt(false, reg_ext(reg));
        emit8(0xC1);
        emit_modrm(0b11, 4, reg_low(reg));
        emit8(imm);
    }

    void X86Emitter::shl(GpReg reg, uint8_t imm)
    {
        emit_rex_natural(false, false, reg_ext(reg));
        emit8(0xC1);
        emit_modrm(0b11, 4, reg_low(reg));
        emit8(imm);
    }

    void X86Emitter::imul(GpReg dst, GpReg src, int32_t imm)
    {
        emit_rex_natural(reg_ext(dst), false, reg_ext(src));
        emit8(0x69);
        emit_modrm(0b11, reg_low(dst), reg_low(src));
        emit32(static_cast<uint32_t>(imm));
    }

    void X86Emitter::imul(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(dst), false, reg_ext(src));
        emit8(0x0F);
        emit8(0xAF);
        emit_modrm(0b11, reg_low(dst), reg_low(src));
    }

    void X86Emitter::imul(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x0F);
        emit8(0xAF);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::mul(Mem src)
    {
        emit_rex_natural(false, mem_index_ext(src), reg_ext(src.base));
        emit8(0xF7);
        emit_modrm_mem(4, src);
    }

    void X86Emitter::div(Mem src)
    {
        emit_rex_natural(false, mem_index_ext(src), reg_ext(src.base));
        emit8(0xF7);
        emit_modrm_mem(6, src);
    }

    void X86Emitter::sar(GpReg reg, uint8_t imm)
    {
        emit_rex_natural(false, false, reg_ext(reg));
        emit8(0xC1);
        emit_modrm(0b11, 7, reg_low(reg));
        emit8(imm);
    }

    void X86Emitter::shl_cl(GpReg reg)
    {
        emit_rex_natural(false, false, reg_ext(reg));
        emit8(0xD3);
        emit_modrm(0b11, 4, reg_low(reg));
    }

    void X86Emitter::sar_cl(GpReg reg)
    {
        emit_rex_natural(false, false, reg_ext(reg));
        emit8(0xD3);
        emit_modrm(0b11, 7, reg_low(reg));
    }

    void X86Emitter::shld_cl(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x0F);
        emit8(0xA5);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::shrd_cl(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x0F);
        emit8(0xAD);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::cqo()
    {
        assert(mode64_ && "cqo requires 64 bit mode");
        emit_rex(true, false, false, false);
        emit8(0x99);
    }

    void X86Emitter::idiv(GpReg divisor)
    {
        emit_rex_natural(false, false, reg_ext(divisor));
        emit8(0xF7);
        emit_modrm(0b11, 7, reg_low(divisor));
    }

    void X86Emitter::cmp(GpReg lhs, GpReg rhs)
    {
        emit_rex_natural(reg_ext(rhs), false, reg_ext(lhs));
        emit8(0x39);
        emit_modrm(0b11, reg_low(rhs), reg_low(lhs));
    }

    void X86Emitter::and_(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x21);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::or_(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x09);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::add(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x03);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::sub(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x2B);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::and_(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x23);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::or_(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x0B);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::xor_(GpReg dst, Mem src)
    {
        emit_rex_natural(reg_ext(dst), mem_index_ext(src), reg_ext(src.base));
        emit8(0x33);
        emit_modrm_mem(reg_low(dst), src);
    }

    void X86Emitter::cmp(GpReg lhs, Mem rhs)
    {
        emit_rex_natural(reg_ext(lhs), mem_index_ext(rhs), reg_ext(rhs.base));
        emit8(0x3B);
        emit_modrm_mem(reg_low(lhs), rhs);
    }

    void X86Emitter::xor_(GpReg dst, GpReg src)
    {
        emit_rex_natural(reg_ext(src), false, reg_ext(dst));
        emit8(0x31);
        emit_modrm(0b11, reg_low(src), reg_low(dst));
    }

    void X86Emitter::and_(GpReg dst, int32_t imm)
    {
        emit_rex_natural(false, false, reg_ext(dst));
        if (imm >= -128 && imm <= 127)
        {
            emit8(0x83);
            emit_modrm(0b11, 4, reg_low(dst));
            emit8(static_cast<uint8_t>(imm));
        }
        else
        {
            emit8(0x81);
            emit_modrm(0b11, 4, reg_low(dst));
            emit32(static_cast<uint32_t>(imm));
        }
    }

    void X86Emitter::push(GpReg reg)
    {
        if (reg_ext(reg))
        {
            assert(mode64_ && "extended registers unavailable in 32 bit mode");
            emit8(0x41);
        }
        emit8(static_cast<uint8_t>(0x50 + reg_low(reg)));
    }

    void X86Emitter::push_imm(uint32_t imm)
    {
        emit8(0x68);
        emit32(imm);
    }

    void X86Emitter::pop(GpReg reg)
    {
        if (reg_ext(reg))
        {
            assert(mode64_ && "extended registers unavailable in 32 bit mode");
            emit8(0x41);
        }
        emit8(static_cast<uint8_t>(0x58 + reg_low(reg)));
    }

    void X86Emitter::call(GpReg reg)
    {
        if (reg_ext(reg))
        {
            assert(mode64_ && "extended registers unavailable in 32 bit mode");
            emit8(0x41);
        }
        emit8(0xFF);
        emit_modrm(0b11, 2, reg_low(reg));
    }

    void X86Emitter::ret()
    {
        emit8(0xC3);
    }

    Label X86Emitter::new_label()
    {
        labels_.push_back(LabelState{ 0, 0, false });
        return Label{ static_cast<uint32_t>(labels_.size() - 1) };
    }

    void X86Emitter::bind(Label label)
    {
        assert(relax_mode_ == 0 && "bind after relaxation");
        assert(label.id < labels_.size() && "bind: invalid label");
        LabelState& state = labels_[label.id];
        assert(!state.bound && "bind: label already bound");

        state.lit_offset = static_cast<uint32_t>(buffer_.size());
        state.node_index = static_cast<uint32_t>(nodes_.size());
        state.bound = true;
    }

    void X86Emitter::emit_branch(bool is_jcc, uint8_t cc, Label label)
    {
        assert(relax_mode_ == 0 && "branch after relaxation");
        assert(label.id < labels_.size() && "branch: invalid label");
        nodes_.push_back(PatchNode{ static_cast<uint32_t>(buffer_.size()), label.id, 0, cc, is_jcc, false, 2 });
    }

    void X86Emitter::jmp(Label label)
    {
        emit_branch(false, 0, label);
    }

    void X86Emitter::jcc(Cond cond, Label label)
    {
        emit_branch(true, static_cast<uint8_t>(cond), label);
    }

    void X86Emitter::call(uintptr_t target)
    {
        assert(relax_mode_ == 0 && "call after relaxation");
        nodes_.push_back(PatchNode{ static_cast<uint32_t>(buffer_.size()), 0, target, 0, false, true, 5 });
    }

    void X86Emitter::align(uint8_t boundary)
    {
        assert(relax_mode_ == 0 && "align after relaxation");
        assert(boundary != 0 && (boundary & (boundary - 1)) == 0 && "align: boundary must be a power of two");
        nodes_.push_back(PatchNode{ static_cast<uint32_t>(buffer_.size()), 0, 0, 0, false, false, 0, true, boundary });
    }

    void X86Emitter::relax(bool exact, uintptr_t address)
    {
        if (relax_mode_ == (exact ? 2 : 1) || (!exact && relax_mode_ == 2))
        {
            return;
        }

        static constexpr int64_t kRel32Slack = INT32_MAX - (1 << 20);

        for (PatchNode& node : nodes_)
        {
            if (node.is_align)
            {
                node.encoded_size = exact ? 0 : static_cast<uint8_t>(node.boundary - 1);
            }
            else if (node.is_call)
            {
                if (!mode64_)
                {
                    node.encoded_size = 5;
                }
                else if (exact)
                {
                    const int64_t disp = static_cast<int64_t>(static_cast<uint64_t>(node.target))
                        - static_cast<int64_t>(static_cast<uint64_t>(address) + node.lit_pos);
                    node.encoded_size = (disp >= -kRel32Slack && disp <= kRel32Slack) ? 5 : 12;
                }
                else
                {
                    node.encoded_size = 12;
                }
            }
            else
            {
                node.encoded_size = 2;
            }
        }

        node_prefix_.assign(nodes_.size() + 1, 0);

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (size_t i = 0; i < nodes_.size(); ++i)
            {
                PatchNode& node = nodes_[i];
                if (node.is_align && exact)
                {
                    const uint32_t mask = node.boundary - 1u;
                    const auto pad = static_cast<uint8_t>((node.boundary - ((node.lit_pos + node_prefix_[i]) & mask)) & mask);
                    if (pad != node.encoded_size)
                    {
                        node.encoded_size = pad;
                        changed = true;
                    }
                }
                node_prefix_[i + 1] = node_prefix_[i] + node.encoded_size;
            }

            for (size_t i = 0; i < nodes_.size(); ++i)
            {
                PatchNode& node = nodes_[i];
                if (node.is_align || node.is_call || node.encoded_size != 2)
                {
                    continue;
                }

                const LabelState& target = labels_[node.label];
                if (!target.bound)
                {
                    relax_failed_ = true;
                    continue;
                }

                const int64_t target_pos = static_cast<int64_t>(target.lit_offset) + node_prefix_[target.node_index];
                const int64_t branch_end = static_cast<int64_t>(node.lit_pos) + node_prefix_[i] + 2;
                const int64_t disp = target_pos - branch_end;
                if (disp < -128 || disp > 127)
                {
                    node.encoded_size = node.is_jcc ? 6 : 5;
                    changed = true;
                }
            }
        }

        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            node_prefix_[i + 1] = node_prefix_[i] + nodes_[i].encoded_size;
        }
        relax_mode_ = exact ? 2 : 1;
    }

    size_t X86Emitter::size()
    {
        relax(false, 0);
        return buffer_.size() + node_prefix_.back();
    }

    static const uint8_t* nop_sequence(size_t len) noexcept
    {
        static constexpr uint8_t kNops[9][9] = {
            { 0x90 },
            { 0x66, 0x90 },
            { 0x0F, 0x1F, 0x00 },
            { 0x0F, 0x1F, 0x40, 0x00 },
            { 0x0F, 0x1F, 0x44, 0x00, 0x00 },
            { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 },
            { 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00 },
            { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
        };
        return kNops[len - 1];
    }

    size_t X86Emitter::finalize(uintptr_t address, uint8_t* buf, size_t buf_size)
    {
        assert(buf != nullptr && "finalize: null output buffer");

        relax(true, address);
        if (relax_failed_)
        {
            return 0;
        }

        const size_t total = buffer_.size() + node_prefix_.back();
        if (total > buf_size)
        {
            return 0;
        }

        size_t out = 0;
        uint32_t lit_cursor = 0;
        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            const PatchNode& node = nodes_[i];
            const size_t lit_chunk = node.lit_pos - lit_cursor;
            std::memcpy(buf + out, buffer_.data() + lit_cursor, lit_chunk);
            out += lit_chunk;
            lit_cursor = node.lit_pos;

            if (node.is_align)
            {
                size_t remaining = node.encoded_size;
                while (remaining > 0)
                {
                    const size_t chunk = remaining < 9 ? remaining : 9;
                    std::memcpy(buf + out, nop_sequence(chunk), chunk);
                    out += chunk;
                    remaining -= chunk;
                }
                assert(((address + out) & (node.boundary - 1u)) == 0);
                continue;
            }

            if (node.is_call)
            {
                if (node.encoded_size == 5)
                {
                    const uintptr_t call_end = address + out + 5;
                    const auto disp32 = static_cast<uint32_t>(node.target - call_end);
                    buf[out++] = 0xE8;
                    buf[out++] = static_cast<uint8_t>(disp32);
                    buf[out++] = static_cast<uint8_t>(disp32 >> 8);
                    buf[out++] = static_cast<uint8_t>(disp32 >> 16);
                    buf[out++] = static_cast<uint8_t>(disp32 >> 24);
                }
                else
                {
                    const auto target64 = static_cast<uint64_t>(node.target);
                    buf[out++] = 0x48;
                    buf[out++] = 0xB8;
                    for (int b = 0; b < 8; ++b)
                    {
                        buf[out++] = static_cast<uint8_t>(target64 >> (b * 8));
                    }
                    buf[out++] = 0xFF;
                    buf[out++] = 0xD0;
                }
                continue;
            }

            const LabelState& target = labels_[node.label];
            const int64_t target_pos = static_cast<int64_t>(target.lit_offset) + node_prefix_[target.node_index];
            const int64_t branch_end = static_cast<int64_t>(node.lit_pos) + node_prefix_[i] + node.encoded_size;
            const int64_t disp = target_pos - branch_end;

            if (node.encoded_size == 2)
            {
                assert(disp >= -128 && disp <= 127);
                buf[out++] = node.is_jcc ? static_cast<uint8_t>(0x70 + node.cc) : static_cast<uint8_t>(0xEB);
                buf[out++] = static_cast<uint8_t>(disp);
            }
            else
            {
                if (node.is_jcc)
                {
                    buf[out++] = 0x0F;
                    buf[out++] = static_cast<uint8_t>(0x80 + node.cc);
                }
                else
                {
                    buf[out++] = 0xE9;
                }
                const auto disp32 = static_cast<uint32_t>(disp);
                buf[out++] = static_cast<uint8_t>(disp32);
                buf[out++] = static_cast<uint8_t>(disp32 >> 8);
                buf[out++] = static_cast<uint8_t>(disp32 >> 16);
                buf[out++] = static_cast<uint8_t>(disp32 >> 24);
            }
        }

        std::memcpy(buf + out, buffer_.data() + lit_cursor, buffer_.size() - lit_cursor);
        out += buffer_.size() - lit_cursor;

        assert(out <= total);
        return out;
    }

    void X86Emitter::emit8(uint8_t value)
    {
        assert(relax_mode_ == 0 && "emit after relaxation");
        buffer_.push_back(value);
    }

    void X86Emitter::emit32(uint32_t value)
    {
        emit8(static_cast<uint8_t>(value));
        emit8(static_cast<uint8_t>(value >> 8));
        emit8(static_cast<uint8_t>(value >> 16));
        emit8(static_cast<uint8_t>(value >> 24));
    }

    void X86Emitter::emit64(uint64_t value)
    {
        emit32(static_cast<uint32_t>(value));
        emit32(static_cast<uint32_t>(value >> 32));
    }

    void X86Emitter::emit_rex(bool w, bool r, bool x, bool b)
    {
        assert(mode64_ && "REX prefix unavailable in 32 bit mode");
        emit8(static_cast<uint8_t>(0x40 | (static_cast<uint8_t>(w) << 3) | (static_cast<uint8_t>(r) << 2)
            | (static_cast<uint8_t>(x) << 1) | static_cast<uint8_t>(b)));
    }

    void X86Emitter::emit_rex_opt(bool r, bool b)
    {
        if (r || b)
        {
            assert(mode64_ && "extended registers unavailable in 32 bit mode");
            emit8(static_cast<uint8_t>(0x40 | (static_cast<uint8_t>(r) << 2) | static_cast<uint8_t>(b)));
        }
    }

    void X86Emitter::emit_rex_opt(bool r, bool x, bool b)
    {
        if (r || x || b)
        {
            assert(mode64_ && "extended registers unavailable in 32 bit mode");
            emit8(static_cast<uint8_t>(0x40 | (static_cast<uint8_t>(r) << 2) | (static_cast<uint8_t>(x) << 1)
                | static_cast<uint8_t>(b)));
        }
    }

    void X86Emitter::emit_modrm(uint8_t mod, uint8_t reg, uint8_t rm)
    {
        emit8(static_cast<uint8_t>((mod << 6) | (reg << 3) | rm));
    }

    void X86Emitter::emit_modrm_mem(uint8_t reg, const Mem& mem_op)
    {
        const uint8_t base = reg_low(mem_op.base);
        const bool indexed = mem_op.scale != 0;
        const bool needs_sib = indexed || base == 0b100;
        const bool disp_fits8 = mem_op.disp >= -128 && mem_op.disp <= 127;

        uint8_t mod;
        if (mem_op.disp == 0 && base != 0b101)
        {
            mod = 0b00;
        }
        else if (disp_fits8)
        {
            mod = 0b01;
        }
        else
        {
            mod = 0b10;
        }

        emit_modrm(mod, reg, needs_sib ? uint8_t{ 0b100 } : base);

        if (indexed)
        {
            assert(reg_low(mem_op.index) != 0b100 && "rsp cannot be a SIB index");
            uint8_t scale_bits = 0;
            switch (mem_op.scale)
            {
                case 1:
                    scale_bits = 0;
                    break;
                case 2:
                    scale_bits = 1;
                    break;
                case 4:
                    scale_bits = 2;
                    break;
                case 8:
                    scale_bits = 3;
                    break;
                default:
                    assert(false && "SIB scale must be 1, 2, 4 or 8");
                    break;
            }
            emit8(static_cast<uint8_t>((scale_bits << 6) | (reg_low(mem_op.index) << 3) | base));
        }
        else if (needs_sib)
        {
            emit8(0x24);
        }

        if (mod == 0b01)
        {
            emit8(static_cast<uint8_t>(mem_op.disp));
        }
        else if (mod == 0b10)
        {
            emit32(static_cast<uint32_t>(mem_op.disp));
        }
    }

} // namespace behl
