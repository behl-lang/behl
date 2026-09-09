#include "codegen_aarch64.hpp"

#if BEHL_JIT_AARCH64

#    include "state.hpp"
#    include "vm/frame.hpp"

#    include <cassert>

namespace behl
{
    static constexpr A64Reg kStateReg = A64Reg::x19;
    static constexpr A64Reg kFrameBase = A64Reg::x20;
    static constexpr A64Reg kScratch = A64Reg::x16;
    static constexpr A64Vec kCopyVec = A64Vec::d16;

    static constexpr A64Reg kGpPool[] = { A64Reg::x0, A64Reg::x1, A64Reg::x2, A64Reg::x9, A64Reg::x10 };
    static constexpr A64Vec kFpPool[] = { A64Vec::d0, A64Vec::d1, A64Vec::d2, A64Vec::d3 };
    static constexpr size_t kGpPoolSize = sizeof(kGpPool) / sizeof(kGpPool[0]);
    static constexpr size_t kFpPoolSize = sizeof(kFpPool) / sizeof(kFpPool[0]);
    static constexpr uint8_t kNoReg = 0xFF;

    static A64Mem slot_tag(int32_t reg) noexcept
    {
        return mem(kFrameBase, Value::size() * reg);
    }

    static A64Mem slot_payload(int32_t reg) noexcept
    {
        return mem(kFrameBase, Value::size() * reg + Value::payload_offset());
    }

    static A64Cond cond_signed(CgCmp cmp) noexcept
    {
        switch (cmp)
        {
            case CgCmp::kEq:
                return A64Cond::eq;
            case CgCmp::kNe:
                return A64Cond::ne;
            case CgCmp::kLt:
                return A64Cond::lt;
            case CgCmp::kLe:
                return A64Cond::le;
            case CgCmp::kGt:
                return A64Cond::gt;
            case CgCmp::kGe:
                return A64Cond::ge;
        }
        return A64Cond::eq;
    }

    static A64Cond cond_f64(CgCmp cmp) noexcept
    {
        switch (cmp)
        {
            case CgCmp::kEq:
                return A64Cond::eq;
            case CgCmp::kNe:
                return A64Cond::ne;
            case CgCmp::kLt:
                return A64Cond::mi;
            case CgCmp::kLe:
                return A64Cond::ls;
            case CgCmp::kGt:
                return A64Cond::gt;
            case CgCmp::kGe:
                return A64Cond::ge;
        }
        return A64Cond::eq;
    }

    A64Label CodegenAArch64::label(uint32_t id) const noexcept
    {
        return A64Label{ id };
    }

    A64Reg CodegenAArch64::gp(uint32_t var) const
    {
        assert(var < var_reg_.size() && var_reg_[var] != kNoReg && !var_f64_[var]);
        return kGpPool[var_reg_[var]];
    }

    A64Vec CodegenAArch64::fp(uint32_t var) const
    {
        assert(var < var_reg_.size() && var_reg_[var] != kNoReg && var_f64_[var]);
        return kFpPool[var_reg_[var]];
    }

    void CodegenAArch64::alloc_i64(uint32_t var)
    {
        var_f64_[var] = false;
        for (size_t i = 0; i < kGpPoolSize; ++i)
        {
            if ((gp_used_ & (1u << i)) == 0)
            {
                gp_used_ |= (1u << i);
                var_reg_[var] = static_cast<uint8_t>(i);
                return;
            }
        }

        if (cache_enabled_ && evict_one(false))
        {
            for (size_t i = 0; i < kGpPoolSize; ++i)
            {
                if ((gp_used_ & (1u << i)) == 0)
                {
                    gp_used_ |= (1u << i);
                    var_reg_[var] = static_cast<uint8_t>(i);
                    return;
                }
            }
        }

        failed_ = true;
        var_reg_[var] = 0;
    }

    void CodegenAArch64::alloc_f64(uint32_t var)
    {
        var_f64_[var] = true;
        for (size_t i = 0; i < kFpPoolSize; ++i)
        {
            if ((fp_used_ & (1u << i)) == 0)
            {
                fp_used_ |= (1u << i);
                var_reg_[var] = static_cast<uint8_t>(i);
                return;
            }
        }

        if (cache_enabled_ && evict_one(true))
        {
            for (size_t i = 0; i < kFpPoolSize; ++i)
            {
                if ((fp_used_ & (1u << i)) == 0)
                {
                    fp_used_ |= (1u << i);
                    var_reg_[var] = static_cast<uint8_t>(i);
                    return;
                }
            }
        }

        failed_ = true;
        var_reg_[var] = 0;
    }

    void CodegenAArch64::alloc_result(uint32_t var)
    {
        assert((gp_used_ & 1u) == 0 && "result register already allocated");
        var_f64_[var] = false;
        gp_used_ |= 1u;
        var_reg_[var] = 0;
    }

    void CodegenAArch64::release_var(uint32_t var)
    {
        if (var_reg_[var] != kNoReg)
        {
            uint32_t& used = var_f64_[var] ? fp_used_ : gp_used_;
            used &= ~(1u << var_reg_[var]);
            var_reg_[var] = kNoReg;
        }
    }

    void CodegenAArch64::ensure_base()
    {
        if (!base_valid_)
        {
            assert(gp_used_ == 0 && fp_used_ == 0 && "base refresh with live variables");
            emit_base_refresh();
            base_valid_ = true;
        }
    }

    void CodegenAArch64::emit_base_refresh()
    {
        e_.ldr(A64Reg::x0, mem(kStateReg, State::call_stack_data_offset()));
        e_.ldr(A64Reg::x1, mem(kStateReg, State::call_stack_size_offset()));
        e_.sub(A64Reg::x1, A64Reg::x1, 1);
        e_.mov32(A64Reg::x2, static_cast<uint32_t>(sizeof(CallFrame)));
        e_.madd(A64Reg::x0, A64Reg::x1, A64Reg::x2, A64Reg::x0);
        e_.ldrw(A64Reg::x1, mem(A64Reg::x0, CallFrame::base_offset()));
        e_.lsl(A64Reg::x1, A64Reg::x1, 4);
        e_.ldr(kFrameBase, mem(kStateReg, State::stack_data_offset()));
        e_.add(kFrameBase, kFrameBase, A64Reg::x1);
    }

    void CodegenAArch64::emit_prologue()
    {
        e_.stp_pre(kStateReg, kFrameBase, A64Reg::sp, -32);
        e_.str(A64Reg::x30, mem(A64Reg::sp, 16));
        e_.mov(kStateReg, A64Reg::x0);
        base_valid_ = false;
    }

    void CodegenAArch64::emit_epilogue(uint32_t result_code)
    {
        e_.mov32(A64Reg::x0, result_code);
        e_.ldr(A64Reg::x30, mem(A64Reg::sp, 16));
        e_.ldp_post(kStateReg, kFrameBase, A64Reg::sp, 32);
        e_.ret();
    }

    void CodegenAArch64::emit_helper_call(const CgOp& op)
    {
        assert(gp_used_ == 0 && fp_used_ == 0 && "helper call with live variables");

        e_.mov(A64Reg::x0, kStateReg);
        e_.mov32(A64Reg::x1, op.raw);
        e_.mov32(A64Reg::x2, op.pcn);
        e_.call(reinterpret_cast<uintptr_t>(op.fn));

        e_.cmnw(A64Reg::x0, 1);
        e_.bcond(A64Cond::eq, label(op.label));
        base_valid_ = false;
        alloc_result(op.var);
    }

    void CodegenAArch64::emit_cmp_imm(A64Reg reg, int64_t imm)
    {
        if (imm >= 0 && imm < 4096)
        {
            e_.cmp(reg, static_cast<uint32_t>(imm));
        }
        else if (imm < 0 && imm > -4096)
        {
            e_.cmn(reg, static_cast<uint32_t>(-imm));
        }
        else
        {
            e_.mov(kScratch, static_cast<uint64_t>(imm));
            e_.cmp(reg, kScratch);
        }
    }

    void CodegenAArch64::emit_add_imm(A64Reg reg, int64_t imm)
    {
        if (imm >= 0 && imm < 4096)
        {
            e_.add(reg, reg, static_cast<uint32_t>(imm));
        }
        else if (imm < 0 && imm > -4096)
        {
            e_.sub(reg, reg, static_cast<uint32_t>(-imm));
        }
        else
        {
            e_.mov(kScratch, static_cast<uint64_t>(imm));
            e_.add(reg, reg, kScratch);
        }
    }

    void CodegenAArch64::compute_liveness(const CgProgram& program)
    {
        last_pos_.assign(program.num_vars, 0);
        var_reg_.assign(program.num_vars, kNoReg);
        var_f64_.assign(program.num_vars, false);

        for (uint32_t i = 0; i < program.ops.size(); ++i)
        {
            const CgOp& op = program.ops[i];
            switch (op.kind)
            {
                case CgOpKind::kLoadI64:
                case CgOpKind::kLoadF64:
                case CgOpKind::kConstI64:
                case CgOpKind::kConstF64:
                case CgOpKind::kHelperCall:
                case CgOpKind::kStoreI64:
                case CgOpKind::kStoreF64:
                case CgOpKind::kAddI64Imm:
                case CgOpKind::kBranchI64Imm:
                case CgOpKind::kBranchVarEqU32:
                case CgOpKind::kLoadFramePc:
                    last_pos_[op.var] = i;
                    break;
                case CgOpKind::kAddI64:
                case CgOpKind::kSubI64:
                case CgOpKind::kMulI64:
                case CgOpKind::kModI64:
                case CgOpKind::kDivU64:
                case CgOpKind::kShlI64:
                case CgOpKind::kShrI64:
                case CgOpKind::kAndI64:
                case CgOpKind::kOrI64:
                case CgOpKind::kXorI64:
                case CgOpKind::kAddF64:
                case CgOpKind::kSubF64:
                case CgOpKind::kMulF64:
                case CgOpKind::kDivF64:
                case CgOpKind::kBranchI64:
                case CgOpKind::kBranchF64:
                    last_pos_[op.var] = i;
                    last_pos_[op.var2] = i;
                    break;
                case CgOpKind::kCvtSlotToF64:
                    last_pos_[op.var] = i;
                    break;
                default:
                    break;
            }
        }
    }

    void CodegenAArch64::release_dead(const CgOp& op, uint32_t index)
    {
        switch (op.kind)
        {
            case CgOpKind::kLoadI64:
            case CgOpKind::kLoadF64:
            case CgOpKind::kConstI64:
            case CgOpKind::kConstF64:
            case CgOpKind::kHelperCall:
            case CgOpKind::kStoreI64:
            case CgOpKind::kStoreF64:
            case CgOpKind::kAddI64Imm:
            case CgOpKind::kBranchI64Imm:
            case CgOpKind::kBranchVarEqU32:
            case CgOpKind::kLoadFramePc:
            case CgOpKind::kCvtSlotToF64:
                if (last_pos_[op.var] == index)
                {
                    release_var(op.var);
                }
                break;
            case CgOpKind::kAddI64:
            case CgOpKind::kSubI64:
            case CgOpKind::kMulI64:
            case CgOpKind::kModI64:
            case CgOpKind::kDivU64:
            case CgOpKind::kShlI64:
            case CgOpKind::kShrI64:
            case CgOpKind::kAndI64:
            case CgOpKind::kOrI64:
            case CgOpKind::kXorI64:
            case CgOpKind::kAddF64:
            case CgOpKind::kSubF64:
            case CgOpKind::kMulF64:
            case CgOpKind::kDivF64:
            case CgOpKind::kBranchI64:
            case CgOpKind::kBranchF64:
                if (last_pos_[op.var] == index)
                {
                    release_var(op.var);
                }
                if (last_pos_[op.var2] == index)
                {
                    release_var(op.var2);
                }
                break;
            default:
                break;
        }
    }


    static constexpr uint8_t kTagUnknown = 0xFF;

    bool CodegenAArch64::slot_in_reg(int32_t slot, bool want_f64) const
    {
        if (slot < 0 || static_cast<size_t>(slot) >= slots_.size())
        {
            return false;
        }
        const SlotState& st = slots_[static_cast<size_t>(slot)];
        return st.reg != kNoReg && st.is_f64 == want_f64;
    }

    void CodegenAArch64::cache_reset()
    {
        for (SlotState& st : slots_)
        {
            st.reg = kNoReg;
            st.is_f64 = false;
            st.tag = kTagUnknown;
        }
    }

    // Cached values always match memory, so dropping one never needs a
    // write-back: the store that produced it was already emitted.
    void CodegenAArch64::discard_payload(int32_t slot)
    {
        if (slot < 0 || static_cast<size_t>(slot) >= slots_.size())
        {
            return;
        }

        SlotState& st = slots_[static_cast<size_t>(slot)];
        if (st.reg != kNoReg)
        {
            if (st.is_f64)
            {
                fp_used_ &= ~(1u << st.reg);
            }
            else
            {
                gp_used_ &= ~(1u << st.reg);
            }
            st.reg = kNoReg;
        }
    }

    void CodegenAArch64::cache_drop_slot(int32_t slot)
    {
        if (slot < 0 || static_cast<size_t>(slot) >= slots_.size())
        {
            return;
        }
        discard_payload(slot);
        slots_[static_cast<size_t>(slot)].tag = kTagUnknown;
    }

    void CodegenAArch64::cache_drop_all()
    {
        for (size_t i = 0; i < slots_.size(); ++i)
        {
            cache_drop_slot(static_cast<int32_t>(i));
        }
    }

    uint32_t CodegenAArch64::next_slot_use(int32_t slot, uint32_t from) const
    {
        const size_t count = program_->ops.size();
        const size_t limit = (count - from > 256) ? from + 256 : count;

        for (size_t i = from; i < limit; ++i)
        {
            const CgOp& op = program_->ops[i];
            switch (op.kind)
            {
                case CgOpKind::kGuardTag:
                case CgOpKind::kStoreTag:
                case CgOpKind::kLoadI64:
                case CgOpKind::kLoadF64:
                case CgOpKind::kStoreI64:
                case CgOpKind::kStoreF64:
                case CgOpKind::kCvtSlotToF64:
                case CgOpKind::kBranchTruthy:
                    if (op.slot == slot)
                    {
                        return static_cast<uint32_t>(i);
                    }
                    break;
                case CgOpKind::kCopySlot:
                    if (op.slot == slot || op.imm == slot)
                    {
                        return static_cast<uint32_t>(i);
                    }
                    break;
                default:
                    break;
            }
        }

        return UINT32_MAX;
    }

    int32_t CodegenAArch64::pick_victim(bool want_f64) const
    {
        int32_t best = -1;
        uint32_t best_use = 0;

        for (size_t i = 0; i < slots_.size(); ++i)
        {
            const SlotState& st = slots_[i];
            if (st.reg == kNoReg || st.is_f64 != want_f64)
            {
                continue;
            }

            const uint32_t use = next_slot_use(static_cast<int32_t>(i), cur_index_ + 1);
            if (best < 0 || use > best_use)
            {
                best = static_cast<int32_t>(i);
                best_use = use;
            }
        }

        return best;
    }

    bool CodegenAArch64::evict_one(bool want_f64)
    {
        const int32_t victim = pick_victim(want_f64);
        if (victim < 0)
        {
            return false;
        }

        discard_payload(victim);
        return true;
    }

    void CodegenAArch64::take_ownership(int32_t slot, uint32_t var, bool is_f64)
    {
        discard_payload(slot);

        SlotState& st = slots_[static_cast<size_t>(slot)];
        st.reg = var_reg_[var];
        st.is_f64 = is_f64;

        var_reg_[var] = kNoReg;
    }

    void CodegenAArch64::record_label_state(uint32_t label)
    {
        LabelState& ls = label_states_[label];
        ls.recorded = true;
        ls.entries.clear();

        for (size_t i = 0; i < slots_.size(); ++i)
        {
            if (slots_[i].reg != kNoReg)
            {
                ls.entries.emplace_back(static_cast<int32_t>(i), slots_[i]);
            }
        }
    }

    // A cold block reaches its resume label with no cached registers, so it has
    // to reload whatever the fast path was holding before merging back.
    void CodegenAArch64::restore_label_state(uint32_t label)
    {
        const LabelState& ls = label_states_[label];

        cache_drop_all();

        if (gp_used_ != 0 || fp_used_ != 0)
        {
            failed_ = true;
            return;
        }

        ensure_base();

        for (const auto& entry : ls.entries)
        {
            const int32_t slot = entry.first;
            const SlotState& want = entry.second;

            if (want.is_f64)
            {
                e_.ldr_d(kFpPool[want.reg], slot_payload(slot));
                fp_used_ |= (1u << want.reg);
            }
            else
            {
                e_.ldr(kGpPool[want.reg], slot_payload(slot));
                gp_used_ |= (1u << want.reg);
            }

            SlotState& st = slots_[static_cast<size_t>(slot)];
            st.reg = want.reg;
            st.is_f64 = want.is_f64;
            st.tag = kTagUnknown;
        }
    }

    void CodegenAArch64::lower(const CgOp& op, uint32_t index)
    {
        switch (op.kind)
        {
            case CgOpKind::kBind:
                if (cache_enabled_)
                {
                    if (op.slot < 0)
                    {
                        cache_drop_all();
                    }
                    else
                    {
                        if (static_cast<size_t>(op.slot) < slots_.size())
                        {
                            slots_[static_cast<size_t>(op.slot)].tag = kTagUnknown;
                        }
                        for (const int32_t guarded : guard_slots_[op.label2])
                        {
                            if (static_cast<size_t>(guarded) < slots_.size())
                            {
                                slots_[static_cast<size_t>(guarded)].tag = kTagUnknown;
                            }
                        }
                        record_label_state(op.label);
                    }
                }
                e_.bind(label(op.label));
                if (op.flag)
                {
                    base_valid_ = false;
                }
                break;

            case CgOpKind::kJump:
                if (cache_enabled_)
                {
                    if (label_states_[op.label].recorded)
                    {
                        restore_label_state(op.label);
                        if (failed_)
                        {
                            break;
                        }
                    }
                    else
                    {
                        cache_drop_all();
                    }
                }
                e_.b(label(op.label));
                break;

            case CgOpKind::kGuardTag:
                if (cache_enabled_)
                {
                    if (slots_[static_cast<size_t>(op.slot)].tag == op.tag)
                    {
                        break;
                    }
                    guard_slots_[op.label].push_back(op.slot);
                }
                ensure_base();
                e_.ldrb(kScratch, slot_tag(op.slot));
                e_.cmpw(kScratch, op.tag);
                e_.bcond(A64Cond::ne, label(op.label));
                if (cache_enabled_)
                {
                    slots_[static_cast<size_t>(op.slot)].tag = op.tag;
                }
                break;

            case CgOpKind::kCopySlot:
            {
                const int32_t src_slot = static_cast<int32_t>(op.imm);
                if (cache_enabled_)
                {
                    cache_drop_slot(op.slot);
                }
                ensure_base();
                e_.ldr_q(kCopyVec, slot_tag(src_slot));
                e_.str_q(kCopyVec, slot_tag(op.slot));
                if (cache_enabled_)
                {
                    slots_[static_cast<size_t>(op.slot)].tag = slots_[static_cast<size_t>(src_slot)].tag;
                }
                break;
            }

            case CgOpKind::kStoreTag:
                if (cache_enabled_ && slots_[static_cast<size_t>(op.slot)].tag == op.tag)
                {
                    break;
                }
                ensure_base();
                e_.mov32(kScratch, op.tag);
                e_.strw(kScratch, slot_tag(op.slot));
                if (cache_enabled_)
                {
                    slots_[static_cast<size_t>(op.slot)].tag = op.tag;
                }
                break;

            case CgOpKind::kLoadI64:
                if (cache_enabled_ && slot_in_reg(op.slot, false))
                {
                    const A64Reg src = kGpPool[slots_[static_cast<size_t>(op.slot)].reg];
                    alloc_i64(op.var);
                    if (!failed_)
                    {
                        e_.mov(gp(op.var), src);
                    }
                    break;
                }
                ensure_base();
                alloc_i64(op.var);
                if (!failed_)
                {
                    e_.ldr(gp(op.var), slot_payload(op.slot));
                }
                break;

            case CgOpKind::kLoadF64:
                if (cache_enabled_ && slot_in_reg(op.slot, true))
                {
                    const A64Vec src = kFpPool[slots_[static_cast<size_t>(op.slot)].reg];
                    alloc_f64(op.var);
                    if (!failed_)
                    {
                        e_.fmov_d(fp(op.var), src);
                    }
                    break;
                }
                ensure_base();
                alloc_f64(op.var);
                if (!failed_)
                {
                    e_.ldr_d(fp(op.var), slot_payload(op.slot));
                }
                break;

            case CgOpKind::kConstI64:
                alloc_i64(op.var);
                if (!failed_)
                {
                    e_.mov(gp(op.var), static_cast<uint64_t>(op.imm));
                }
                break;

            case CgOpKind::kConstF64:
                alloc_f64(op.var);
                if (!failed_)
                {
                    e_.mov(kScratch, static_cast<uint64_t>(op.imm));
                    e_.fmov(fp(op.var), kScratch);
                }
                break;

            case CgOpKind::kStoreI64:
                if (cache_enabled_)
                {
                    discard_payload(op.slot);
                }
                ensure_base();
                if (!failed_)
                {
                    e_.str(gp(op.var), slot_payload(op.slot));
                    if (cache_enabled_ && last_pos_[op.var] == index)
                    {
                        take_ownership(op.slot, op.var, false);
                    }
                }
                break;

            case CgOpKind::kStoreF64:
                if (cache_enabled_)
                {
                    discard_payload(op.slot);
                }
                ensure_base();
                if (!failed_)
                {
                    e_.str_d(fp(op.var), slot_payload(op.slot));
                    if (cache_enabled_ && last_pos_[op.var] == index)
                    {
                        take_ownership(op.slot, op.var, true);
                    }
                }
                break;

            case CgOpKind::kAddI64:
                if (!failed_)
                {
                    e_.add(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kSubI64:
                if (!failed_)
                {
                    e_.sub(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kAndI64:
                if (!failed_)
                {
                    e_.and_(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kOrI64:
                if (!failed_)
                {
                    e_.orr(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kXorI64:
                if (!failed_)
                {
                    e_.eor(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kMulI64:
                if (!failed_)
                {
                    e_.mul(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kModI64:
                if (!failed_)
                {
                    e_.cmp(gp(op.var2), 0u);
                    e_.bcond(A64Cond::eq, label(op.label));
                    e_.sdiv(kScratch, gp(op.var), gp(op.var2));
                    e_.msub(gp(op.var), kScratch, gp(op.var2), gp(op.var));
                }
                break;

            case CgOpKind::kDivU64:
                if (!failed_)
                {
                    e_.udiv(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kShlI64:
                if (!failed_)
                {
                    e_.lslv(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kShrI64:
                if (!failed_)
                {
                    e_.asrv(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kAddI64Imm:
                if (!failed_)
                {
                    emit_add_imm(gp(op.var), op.imm);
                }
                break;

            case CgOpKind::kAddF64:
                if (!failed_)
                {
                    e_.fadd(fp(op.var), fp(op.var), fp(op.var2));
                }
                break;

            case CgOpKind::kSubF64:
                if (!failed_)
                {
                    e_.fsub(fp(op.var), fp(op.var), fp(op.var2));
                }
                break;

            case CgOpKind::kMulF64:
                if (!failed_)
                {
                    e_.fmul(fp(op.var), fp(op.var), fp(op.var2));
                }
                break;

            case CgOpKind::kDivF64:
                if (!failed_)
                {
                    e_.fdiv(fp(op.var), fp(op.var), fp(op.var2));
                }
                break;

            case CgOpKind::kCvtSlotToF64:
                ensure_base();
                alloc_f64(op.var);
                if (!failed_)
                {
                    e_.ldr(kScratch, slot_payload(op.slot));
                    e_.scvtf(fp(op.var), kScratch);
                }
                break;

            case CgOpKind::kBranchI64:
                if (!failed_)
                {
                    e_.cmp(gp(op.var), gp(op.var2));
                    e_.bcond(cond_signed(op.cmp), label(op.label));
                }
                break;

            case CgOpKind::kBranchI64Imm:
                if (!failed_)
                {
                    emit_cmp_imm(gp(op.var), op.imm);
                    e_.bcond(cond_signed(op.cmp), label(op.label));
                }
                break;

            case CgOpKind::kBranchF64:
                if (!failed_)
                {
                    e_.fcmp(fp(op.var), fp(op.var2));
                    e_.bcond(cond_f64(op.cmp), label(op.label));
                    e_.b(label(op.label2));
                }
                break;

            case CgOpKind::kBranchTruthy:
            {
                ensure_base();
                const A64Label truthy = label(op.label);
                const A64Label falsy = label(op.label2);
                e_.ldrb(kScratch, slot_tag(op.slot));
                e_.cmpw(kScratch, static_cast<uint32_t>(Type::kNil));
                e_.bcond(A64Cond::eq, falsy);
                e_.cmpw(kScratch, static_cast<uint32_t>(Type::kBoolean));
                e_.bcond(A64Cond::ne, truthy);
                e_.ldrb(kScratch, slot_payload(op.slot));
                e_.cmpw(kScratch, 0u);
                e_.bcond(A64Cond::eq, falsy);
                e_.b(truthy);
                break;
            }

            case CgOpKind::kBranchVarEqU32:
                if (!failed_)
                {
                    const auto imm = static_cast<uint32_t>(op.imm);
                    if (imm < 4096)
                    {
                        e_.cmpw(gp(op.var), imm);
                    }
                    else
                    {
                        e_.mov32(kScratch, imm);
                        e_.cmpw(gp(op.var), kScratch);
                    }
                    e_.bcond(A64Cond::eq, label(op.label));
                }
                break;

            case CgOpKind::kLoadFramePc:
                alloc_i64(op.var);
                if (!failed_)
                {
                    e_.ldr(A64Reg::x0, mem(kStateReg, State::call_stack_data_offset()));
                    e_.ldr(A64Reg::x1, mem(kStateReg, State::call_stack_size_offset()));
                    e_.sub(A64Reg::x1, A64Reg::x1, 1);
                    e_.mov32(A64Reg::x2, static_cast<uint32_t>(sizeof(CallFrame)));
                    e_.madd(A64Reg::x0, A64Reg::x1, A64Reg::x2, A64Reg::x0);
                    e_.ldrw(gp(op.var), mem(A64Reg::x0, CallFrame::pc_offset()));
                }
                break;

            case CgOpKind::kHelperCall:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                emit_helper_call(op);
                break;

            case CgOpKind::kSyncFrame:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                assert(gp_used_ == 0 && fp_used_ == 0 && "frame sync with live variables");
                emit_base_refresh();
                base_valid_ = true;
                break;

            case CgOpKind::kReturnResult:
                emit_epilogue(static_cast<uint32_t>(op.imm));
                break;
        }

        release_dead(op, index);
    }

    JitEntry CodegenAArch64::generate(State* S, const CgProgram& program)
    {
        compute_liveness(program);

        program_ = &program;
        cache_enabled_ = program.allow_slot_cache;

        {
            int32_t max_slot = -1;
            for (const CgOp& op : program.ops)
            {
                switch (op.kind)
                {
                    case CgOpKind::kGuardTag:
                    case CgOpKind::kStoreTag:
                    case CgOpKind::kLoadI64:
                    case CgOpKind::kLoadF64:
                    case CgOpKind::kStoreI64:
                    case CgOpKind::kStoreF64:
                    case CgOpKind::kCvtSlotToF64:
                    case CgOpKind::kBranchTruthy:
                    case CgOpKind::kBind:
                        max_slot = (op.slot > max_slot) ? op.slot : max_slot;
                        break;
                    case CgOpKind::kCopySlot:
                        max_slot = (op.slot > max_slot) ? op.slot : max_slot;
                        max_slot = (static_cast<int32_t>(op.imm) > max_slot) ? static_cast<int32_t>(op.imm) : max_slot;
                        break;
                    default:
                        break;
                }
            }

            slots_.assign(static_cast<size_t>(max_slot + 1), SlotState{});
            cache_reset();
        }

        label_states_.assign(program.num_labels, LabelState{});
        guard_slots_.assign(program.num_labels, std::vector<int32_t>{});

        for (uint32_t i = 0; i < program.num_labels; ++i)
        {
            e_.new_label();
        }

        emit_prologue();

        for (uint32_t i = 0; i < program.ops.size(); ++i)
        {
            cur_index_ = i;
            lower(program.ops[i], i);
            if (failed_)
            {
                return nullptr;
            }
        }

        const size_t size = e_.size();
        void* code_mem = jit_exec_alloc(S, size);
        if (code_mem == nullptr)
        {
            return nullptr;
        }
        const size_t emitted = e_.finalize(reinterpret_cast<uintptr_t>(code_mem), static_cast<uint8_t*>(code_mem), size);
        jit_exec_commit(code_mem, size);
        if (emitted == 0 || emitted > size)
        {
            return nullptr;
        }

        return reinterpret_cast<JitEntry>(code_mem);
    }

} // namespace behl

#endif
