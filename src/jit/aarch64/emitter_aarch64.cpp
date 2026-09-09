#include "emitter_aarch64.hpp"

#include <cassert>
#include <cstring>

namespace behl
{
    static constexpr uint32_t kCallLongWords = 5;

    static uint8_t rn(A64Reg reg) noexcept
    {
        return static_cast<uint8_t>(reg);
    }

    static uint8_t vn(A64Vec reg) noexcept
    {
        return static_cast<uint8_t>(reg);
    }

    void A64Emitter::emit(uint32_t word)
    {
        buffer_.push_back(word);
    }

    void A64Emitter::mov(A64Reg dst, A64Reg src)
    {
        emit(0xAA0003E0u | (uint32_t{ rn(src) } << 16) | rn(dst));
    }

    void A64Emitter::mov(A64Reg dst, uint64_t imm)
    {
        uint16_t chunk[4];
        int nonzero = 0;
        int nonones = 0;
        for (int i = 0; i < 4; ++i)
        {
            chunk[i] = static_cast<uint16_t>(imm >> (i * 16));
            nonzero += chunk[i] != 0;
            nonones += chunk[i] != 0xFFFF;
        }

        const uint32_t d = rn(dst);
        if (nonones < nonzero)
        {
            int first = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (chunk[i] != 0xFFFF)
                {
                    first = i;
                    break;
                }
            }
            const uint32_t inv = static_cast<uint16_t>(~chunk[first]);
            emit(0x92800000u | (uint32_t(first) << 21) | (inv << 5) | d);
            for (int i = 0; i < 4; ++i)
            {
                if (i != first && chunk[i] != 0xFFFF)
                {
                    emit(0xF2800000u | (uint32_t(i) << 21) | (uint32_t{ chunk[i] } << 5) | d);
                }
            }
        }
        else
        {
            int first = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (chunk[i] != 0)
                {
                    first = i;
                    break;
                }
            }
            emit(0xD2800000u | (uint32_t(first) << 21) | (uint32_t{ chunk[first] } << 5) | d);
            for (int i = 0; i < 4; ++i)
            {
                if (i != first && chunk[i] != 0)
                {
                    emit(0xF2800000u | (uint32_t(i) << 21) | (uint32_t{ chunk[i] } << 5) | d);
                }
            }
        }
    }

    void A64Emitter::mov32(A64Reg dst, uint32_t imm)
    {
        const uint32_t d = rn(dst);
        const uint32_t lo = imm & 0xFFFFu;
        const uint32_t hi = imm >> 16;
        if (hi != 0 && lo == 0)
        {
            emit(0x52800000u | (1u << 21) | (hi << 5) | d);
            return;
        }
        emit(0x52800000u | (lo << 5) | d);
        if (hi != 0)
        {
            emit(0x72800000u | (1u << 21) | (hi << 5) | d);
        }
    }

    void A64Emitter::emit_ls(uint32_t base_op, uint8_t scale_log, uint8_t rt, A64Mem addr)
    {
        assert(addr.disp >= 0);
        assert((addr.disp & ((1 << scale_log) - 1)) == 0);
        const uint32_t imm = static_cast<uint32_t>(addr.disp) >> scale_log;
        assert(imm < 4096);
        emit(base_op | (imm << 10) | (uint32_t{ rn(addr.base) } << 5) | rt);
    }

    void A64Emitter::ldr(A64Reg dst, A64Mem src)
    {
        emit_ls(0xF9400000u, 3, rn(dst), src);
    }

    void A64Emitter::str(A64Reg src, A64Mem dst)
    {
        emit_ls(0xF9000000u, 3, rn(src), dst);
    }

    void A64Emitter::ldrw(A64Reg dst, A64Mem src)
    {
        emit_ls(0xB9400000u, 2, rn(dst), src);
    }

    void A64Emitter::strw(A64Reg src, A64Mem dst)
    {
        emit_ls(0xB9000000u, 2, rn(src), dst);
    }

    void A64Emitter::ldrb(A64Reg dst, A64Mem src)
    {
        emit_ls(0x39400000u, 0, rn(dst), src);
    }

    void A64Emitter::ldr_d(A64Vec dst, A64Mem src)
    {
        emit_ls(0xFD400000u, 3, vn(dst), src);
    }

    void A64Emitter::str_d(A64Vec src, A64Mem dst)
    {
        emit_ls(0xFD000000u, 3, vn(src), dst);
    }

    void A64Emitter::ldr_q(A64Vec dst, A64Mem src)
    {
        emit_ls(0x3DC00000u, 4, vn(dst), src);
    }

    void A64Emitter::str_q(A64Vec src, A64Mem dst)
    {
        emit_ls(0x3D800000u, 4, vn(src), dst);
    }

    void A64Emitter::stp_pre(A64Reg r1, A64Reg r2, A64Reg base, int32_t imm)
    {
        assert((imm & 7) == 0 && imm >= -512 && imm <= 504);
        const uint32_t imm7 = (static_cast<uint32_t>(imm >> 3)) & 0x7Fu;
        emit(0xA9800000u | (imm7 << 15) | (uint32_t{ rn(r2) } << 10) | (uint32_t{ rn(base) } << 5) | rn(r1));
    }

    void A64Emitter::ldp_post(A64Reg r1, A64Reg r2, A64Reg base, int32_t imm)
    {
        assert((imm & 7) == 0 && imm >= -512 && imm <= 504);
        const uint32_t imm7 = (static_cast<uint32_t>(imm >> 3)) & 0x7Fu;
        emit(0xA8C00000u | (imm7 << 15) | (uint32_t{ rn(r2) } << 10) | (uint32_t{ rn(base) } << 5) | rn(r1));
    }

    void A64Emitter::add(A64Reg dst, A64Reg src, uint32_t imm)
    {
        assert(imm < 4096);
        emit(0x91000000u | (imm << 10) | (uint32_t{ rn(src) } << 5) | rn(dst));
    }

    void A64Emitter::sub(A64Reg dst, A64Reg src, uint32_t imm)
    {
        assert(imm < 4096);
        emit(0xD1000000u | (imm << 10) | (uint32_t{ rn(src) } << 5) | rn(dst));
    }

    void A64Emitter::add(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0x8B000000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::sub(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0xCB000000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::and_(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0x8A000000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::orr(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0xAA000000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::eor(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0xCA000000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::mul(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0x9B007C00u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::madd(A64Reg dst, A64Reg lhs, A64Reg rhs, A64Reg addend)
    {
        emit(0x9B000000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(addend) } << 10)
            | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::msub(A64Reg dst, A64Reg lhs, A64Reg rhs, A64Reg minuend)
    {
        emit(0x9B008000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(minuend) } << 10)
            | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::sdiv(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0x9AC00C00u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::udiv(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0x9AC00800u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::lslv(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0x9AC02000u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::asrv(A64Reg dst, A64Reg lhs, A64Reg rhs)
    {
        emit(0x9AC02800u | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5) | rn(dst));
    }

    void A64Emitter::lsl(A64Reg dst, A64Reg src, uint8_t shift)
    {
        assert(shift > 0 && shift < 64);
        const uint32_t immr = (64u - shift) & 63u;
        const uint32_t imms = 63u - shift;
        emit(0xD3400000u | (immr << 16) | (imms << 10) | (uint32_t{ rn(src) } << 5) | rn(dst));
    }

    void A64Emitter::cmp(A64Reg lhs, A64Reg rhs)
    {
        emit(0xEB00001Fu | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5));
    }

    void A64Emitter::cmp(A64Reg reg, uint32_t imm)
    {
        assert(imm < 4096);
        emit(0xF100001Fu | (imm << 10) | (uint32_t{ rn(reg) } << 5));
    }

    void A64Emitter::cmn(A64Reg reg, uint32_t imm)
    {
        assert(imm < 4096);
        emit(0xB100001Fu | (imm << 10) | (uint32_t{ rn(reg) } << 5));
    }

    void A64Emitter::cmpw(A64Reg lhs, A64Reg rhs)
    {
        emit(0x6B00001Fu | (uint32_t{ rn(rhs) } << 16) | (uint32_t{ rn(lhs) } << 5));
    }

    void A64Emitter::cmpw(A64Reg reg, uint32_t imm)
    {
        assert(imm < 4096);
        emit(0x7100001Fu | (imm << 10) | (uint32_t{ rn(reg) } << 5));
    }

    void A64Emitter::cmnw(A64Reg reg, uint32_t imm)
    {
        assert(imm < 4096);
        emit(0x3100001Fu | (imm << 10) | (uint32_t{ rn(reg) } << 5));
    }

    void A64Emitter::fadd(A64Vec dst, A64Vec lhs, A64Vec rhs)
    {
        emit(0x1E602800u | (uint32_t{ vn(rhs) } << 16) | (uint32_t{ vn(lhs) } << 5) | vn(dst));
    }

    void A64Emitter::fsub(A64Vec dst, A64Vec lhs, A64Vec rhs)
    {
        emit(0x1E603800u | (uint32_t{ vn(rhs) } << 16) | (uint32_t{ vn(lhs) } << 5) | vn(dst));
    }

    void A64Emitter::fmul(A64Vec dst, A64Vec lhs, A64Vec rhs)
    {
        emit(0x1E600800u | (uint32_t{ vn(rhs) } << 16) | (uint32_t{ vn(lhs) } << 5) | vn(dst));
    }

    void A64Emitter::fdiv(A64Vec dst, A64Vec lhs, A64Vec rhs)
    {
        emit(0x1E601800u | (uint32_t{ vn(rhs) } << 16) | (uint32_t{ vn(lhs) } << 5) | vn(dst));
    }

    void A64Emitter::fcmp(A64Vec lhs, A64Vec rhs)
    {
        emit(0x1E602000u | (uint32_t{ vn(rhs) } << 16) | (uint32_t{ vn(lhs) } << 5));
    }

    void A64Emitter::fmov_d(A64Vec dst, A64Vec src)
    {
        // FMOV Dd, Dn
        emit(0x1E604000u | (uint32_t{ vn(src) } << 5) | vn(dst));
    }

    void A64Emitter::fmov(A64Vec dst, A64Reg src)
    {
        emit(0x9E670000u | (uint32_t{ rn(src) } << 5) | vn(dst));
    }

    void A64Emitter::scvtf(A64Vec dst, A64Reg src)
    {
        emit(0x9E620000u | (uint32_t{ rn(src) } << 5) | vn(dst));
    }

    void A64Emitter::call(uintptr_t target)
    {
        nodes_.push_back(PatchNode{ static_cast<uint32_t>(buffer_.size()), 0, target, NodeType::kCall, 0, 0 });
    }

    void A64Emitter::ret()
    {
        emit(0xD65F03C0u);
    }

    A64Label A64Emitter::new_label()
    {
        const uint32_t id = static_cast<uint32_t>(labels_.size());
        labels_.push_back(LabelState{ 0, 0, false });
        return A64Label{ id };
    }

    void A64Emitter::bind(A64Label label)
    {
        assert(label.id < labels_.size() && !labels_[label.id].bound);
        labels_[label.id] = LabelState{ static_cast<uint32_t>(buffer_.size()),
            static_cast<uint32_t>(nodes_.size()), true };
    }

    void A64Emitter::b(A64Label label)
    {
        nodes_.push_back(
            PatchNode{ static_cast<uint32_t>(buffer_.size()), label.id, 0, NodeType::kBranch, 0, 0 });
    }

    void A64Emitter::bcond(A64Cond cond, A64Label label)
    {
        nodes_.push_back(PatchNode{ static_cast<uint32_t>(buffer_.size()), label.id, 0, NodeType::kBranchCond,
            static_cast<uint8_t>(cond), 0 });
    }

    size_t A64Emitter::size()
    {
        size_t words = buffer_.size();
        for (const PatchNode& node : nodes_)
        {
            words += (node.type == NodeType::kCall) ? kCallLongWords : 1;
        }
        return words * 4;
    }

    uint32_t A64Emitter::node_offset(uint32_t index) const noexcept
    {
        return nodes_[index].lit_pos + node_prefix_[index];
    }

    uint32_t A64Emitter::label_offset(uint32_t id) const noexcept
    {
        const LabelState& state = labels_[id];
        const uint32_t before = (state.node_index < nodes_.size())
            ? node_prefix_[state.node_index]
            : (nodes_.empty() ? 0
                              : node_prefix_[nodes_.size() - 1] + nodes_.back().words);
        return state.lit_offset + before;
    }

    void A64Emitter::relax(uintptr_t address)
    {
        for (PatchNode& node : nodes_)
        {
            node.words = 1;
        }
        node_prefix_.assign(nodes_.size(), 0);

        bool changed = true;
        while (changed)
        {
            changed = false;

            uint32_t acc = 0;
            for (size_t i = 0; i < nodes_.size(); ++i)
            {
                node_prefix_[i] = acc;
                acc += nodes_[i].words;
            }

            for (size_t i = 0; i < nodes_.size(); ++i)
            {
                PatchNode& node = nodes_[i];
                if (node.type != NodeType::kCall || node.words != 1)
                {
                    continue;
                }
                const uintptr_t pc = address + uintptr_t{ node_offset(static_cast<uint32_t>(i)) } * 4;
                const int64_t delta = static_cast<int64_t>(node.target - pc);
                if (delta < -(int64_t{ 1 } << 27) || delta >= (int64_t{ 1 } << 27))
                {
                    node.words = kCallLongWords;
                    changed = true;
                }
            }
        }
    }

    size_t A64Emitter::finalize(uintptr_t address, uint8_t* buf, size_t buf_size)
    {
        relax_failed_ = false;
        relax(address);

        size_t total_words = buffer_.size();
        for (const PatchNode& node : nodes_)
        {
            total_words += node.words;
        }
        if (total_words * 4 > buf_size)
        {
            return 0;
        }

        auto* out = reinterpret_cast<uint32_t*>(buf);
        size_t out_pos = 0;
        uint32_t lit_cursor = 0;

        auto copy_literals = [&](uint32_t end) {
            if (end > lit_cursor)
            {
                std::memcpy(out + out_pos, buffer_.data() + lit_cursor, (end - lit_cursor) * 4);
                out_pos += end - lit_cursor;
                lit_cursor = end;
            }
        };

        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            const PatchNode& node = nodes_[i];
            copy_literals(node.lit_pos);

            const uint32_t own_offset = node_offset(static_cast<uint32_t>(i));

            switch (node.type)
            {
                case NodeType::kBranch:
                case NodeType::kBranchCond:
                {
                    const int64_t delta =
                        (int64_t{ label_offset(node.label) } - int64_t{ own_offset });
                    if (node.type == NodeType::kBranch)
                    {
                        if (delta < -(int64_t{ 1 } << 25) || delta >= (int64_t{ 1 } << 25))
                        {
                            relax_failed_ = true;
                        }
                        out[out_pos++] = 0x14000000u | (static_cast<uint32_t>(delta) & 0x03FFFFFFu);
                    }
                    else
                    {
                        if (delta < -(int64_t{ 1 } << 18) || delta >= (int64_t{ 1 } << 18))
                        {
                            relax_failed_ = true;
                        }
                        out[out_pos++] = 0x54000000u | ((static_cast<uint32_t>(delta) & 0x0007FFFFu) << 5) | node.cc;
                    }
                    break;
                }
                case NodeType::kCall:
                {
                    if (node.words == 1)
                    {
                        const uintptr_t pc = address + uintptr_t{ own_offset } * 4;
                        const int64_t delta = static_cast<int64_t>(node.target - pc) >> 2;
                        out[out_pos++] = 0x94000000u | (static_cast<uint32_t>(delta) & 0x03FFFFFFu);
                    }
                    else
                    {
                        const auto target = static_cast<uint64_t>(node.target);
                        out[out_pos++] = 0xD2800000u | ((static_cast<uint32_t>(target) & 0xFFFFu) << 5) | 16u;
                        out[out_pos++] = 0xF2800000u | (1u << 21) | ((static_cast<uint32_t>(target >> 16) & 0xFFFFu) << 5) | 16u;
                        out[out_pos++] = 0xF2800000u | (2u << 21) | ((static_cast<uint32_t>(target >> 32) & 0xFFFFu) << 5) | 16u;
                        out[out_pos++] = 0xF2800000u | (3u << 21) | ((static_cast<uint32_t>(target >> 48) & 0xFFFFu) << 5) | 16u;
                        out[out_pos++] = 0xD63F0000u | (16u << 5);
                    }
                    break;
                }
            }
        }

        copy_literals(static_cast<uint32_t>(buffer_.size()));

        if (relax_failed_)
        {
            return 0;
        }
        assert(out_pos == total_words);
        return out_pos * 4;
    }

} // namespace behl
