#include "codegen_x86.hpp"

#if BEHL_JIT_X86

#    include "state.hpp"
#    include "vm/frame.hpp"

#    include <cassert>
#    include <cstddef>
#    include <type_traits>

namespace behl
{
    static_assert(sizeof(Vector<Value>) == 3 * sizeof(void*));
    static_assert(std::is_same_v<FP, double>);

    static constexpr bool kMode64 = BEHL_JIT_X86_64 != 0;

    static constexpr GpReg kScratchA = GpReg::r0;
    static constexpr GpReg kScratchB = GpReg::r1;
    static constexpr GpReg kScratchC = GpReg::r2;
    static constexpr GpReg kStateReg = GpReg::r3;
    static constexpr GpReg kStackPtr = GpReg::r4;
    static constexpr GpReg kFrameBase = GpReg::r6;

    static constexpr int32_t kValueSize = 16;
    static constexpr int32_t kPayloadOffset = 8;
    static constexpr int32_t kOffStackData = static_cast<int32_t>(offsetof(State, stack));
    static constexpr int32_t kOffCallStackData = static_cast<int32_t>(offsetof(State, call_stack));
    static constexpr int32_t kOffCallStackSize = static_cast<int32_t>(offsetof(State, call_stack) + sizeof(void*));
    static constexpr int32_t kOffFrameBase = static_cast<int32_t>(offsetof(CallFrame, base));

#    if BEHL_JIT_X86_64
    static constexpr GpReg kGpPool[] = { kScratchA, kScratchB, kScratchC, GpReg::r9, GpReg::r10 };
#    else
    static constexpr GpReg kGpPool[] = { kScratchA, kScratchB, kScratchC, GpReg::r7 };
#    endif
    static constexpr XmmReg kXmmPool[] = { XmmReg::xmm0, XmmReg::xmm1, XmmReg::xmm2, XmmReg::xmm3 };
    static constexpr size_t kGpPoolSize = sizeof(kGpPool) / sizeof(kGpPool[0]);
    static constexpr size_t kXmmPoolSize = sizeof(kXmmPool) / sizeof(kXmmPool[0]);
    static constexpr uint8_t kNoReg = 0xFF;

    static Mem slot_tag(int32_t reg) noexcept
    {
        return mem(kFrameBase, kValueSize * reg);
    }

    static Mem slot_payload(int32_t reg) noexcept
    {
        return mem(kFrameBase, kValueSize * reg + kPayloadOffset);
    }

    [[maybe_unused]] static Mem slot_payload_hi(int32_t reg) noexcept
    {
        return mem(kFrameBase, kValueSize * reg + kPayloadOffset + 4);
    }

    [[maybe_unused]] static Cond cond_signed(CgCmp cmp) noexcept
    {
        switch (cmp)
        {
            case CgCmp::kEq:
                return Cond::e;
            case CgCmp::kNe:
                return Cond::ne;
            case CgCmp::kLt:
                return Cond::l;
            case CgCmp::kLe:
                return Cond::le;
            case CgCmp::kGt:
                return Cond::g;
            case CgCmp::kGe:
                return Cond::ge;
        }
        return Cond::e;
    }

    Label CodegenX86::label(uint32_t id) const noexcept
    {
        return Label{ id };
    }

    GpReg CodegenX86::gp(uint32_t var) const
    {
        assert(var < var_reg_.size() && var_reg_[var] != kNoReg && !var_f64_[var]);
        return kGpPool[var_reg_[var]];
    }

    GpReg CodegenX86::gp_hi(uint32_t var) const
    {
        assert(var < var_reg2_.size() && var_reg2_[var] != kNoReg && !var_f64_[var]);
        return kGpPool[var_reg2_[var]];
    }

    XmmReg CodegenX86::xmm(uint32_t var) const
    {
        assert(var < var_reg_.size() && var_reg_[var] != kNoReg && var_f64_[var]);
        return kXmmPool[var_reg_[var]];
    }

    uint8_t CodegenX86::alloc_gp_slot()
    {
        for (size_t i = 0; i < kGpPoolSize; ++i)
        {
            if ((gp_used_ & (1u << i)) == 0)
            {
                gp_used_ |= (1u << i);
                return static_cast<uint8_t>(i);
            }
        }
        failed_ = true;
        return 0;
    }

    void CodegenX86::alloc_i64(uint32_t var)
    {
        var_f64_[var] = false;
        var_reg_[var] = alloc_gp_slot();
        if constexpr (!kMode64)
        {
            var_reg2_[var] = alloc_gp_slot();
        }
    }

    void CodegenX86::alloc_f64(uint32_t var)
    {
        var_f64_[var] = true;
        for (size_t i = 0; i < kXmmPoolSize; ++i)
        {
            if ((xmm_used_ & (1u << i)) == 0)
            {
                xmm_used_ |= (1u << i);
                var_reg_[var] = static_cast<uint8_t>(i);
                return;
            }
        }
        failed_ = true;
        var_reg_[var] = 0;
    }

    void CodegenX86::alloc_result(uint32_t var)
    {
        assert((gp_used_ & 1u) == 0 && "result register already allocated");
        var_f64_[var] = false;
        gp_used_ |= 1u;
        var_reg_[var] = 0;
    }

    void CodegenX86::release_var(uint32_t var)
    {
        if (var_reg_[var] != kNoReg)
        {
            uint32_t& used = var_f64_[var] ? xmm_used_ : gp_used_;
            used &= ~(1u << var_reg_[var]);
            var_reg_[var] = kNoReg;
        }
        if (var_reg2_[var] != kNoReg)
        {
            gp_used_ &= ~(1u << var_reg2_[var]);
            var_reg2_[var] = kNoReg;
        }
    }

    void CodegenX86::ensure_base()
    {
        if (!base_valid_)
        {
            assert(gp_used_ == 0 && xmm_used_ == 0 && "base refresh with live variables");
            emit_base_refresh();
            base_valid_ = true;
        }
    }

    void CodegenX86::emit_base_refresh()
    {
        e_.mov(kScratchA, mem(kStateReg, kOffCallStackData));
        e_.mov(kScratchB, mem(kStateReg, kOffCallStackSize));
        e_.sub(kScratchB, 1);
        e_.imul(kScratchB, kScratchB, static_cast<int32_t>(sizeof(CallFrame)));
        e_.add(kScratchA, kScratchB);
        e_.mov32(kScratchB, mem(kScratchA, kOffFrameBase));
        e_.shl(kScratchB, 4);
        e_.mov(kFrameBase, mem(kStateReg, kOffStackData));
        e_.add(kFrameBase, kScratchB);
    }

    void CodegenX86::emit_prologue()
    {
        if constexpr (kMode64)
        {
            e_.push(kStateReg);
            e_.push(kFrameBase);
            e_.sub(kStackPtr, 40);
            e_.mov(kStateReg, kScratchB);
        }
        else
        {
            e_.push(kStateReg);
            e_.push(kFrameBase);
            e_.push(GpReg::r7);
            e_.mov(kStateReg, mem(kStackPtr, 16));
            e_.sub(kStackPtr, 16);
        }
        base_valid_ = false;
    }

    void CodegenX86::emit_epilogue(uint32_t result_code)
    {
        e_.mov(kScratchA, static_cast<uint64_t>(result_code));
        if constexpr (kMode64)
        {
            e_.add(kStackPtr, 40);
            e_.pop(kFrameBase);
            e_.pop(kStateReg);
        }
        else
        {
            e_.add(kStackPtr, 16);
            e_.pop(GpReg::r7);
            e_.pop(kFrameBase);
            e_.pop(kStateReg);
        }
        e_.ret();
    }

    void CodegenX86::emit_helper_call(const CgOp& op)
    {
        assert(gp_used_ == 0 && xmm_used_ == 0 && "helper call with live variables");

        if constexpr (kMode64)
        {
            e_.mov(kScratchB, kStateReg);
            e_.mov32(kScratchC, op.raw);
            e_.mov32(GpReg::r8, op.pcn);
            e_.call(reinterpret_cast<uintptr_t>(op.fn));
        }
        else
        {
            e_.push_imm(op.pcn);
            e_.push_imm(op.raw);
            e_.push(kStateReg);
            e_.call(reinterpret_cast<uintptr_t>(op.fn));
            e_.add(kStackPtr, 12);
        }

        e_.cmp32(kScratchA, kJitError);
        e_.jcc(Cond::e, label(op.label));
        base_valid_ = false;
        alloc_result(op.var);
    }

    void CodegenX86::emit_branch_i64_imm(const CgOp& op)
    {
        assert(op.imm >= INT32_MIN && op.imm <= INT32_MAX);
        const Label target = label(op.label);

        if constexpr (kMode64)
        {
            e_.cmp(gp(op.var), static_cast<int32_t>(op.imm));
            e_.jcc(cond_signed(op.cmp), target);
            return;
        }
        else
        {
            const int32_t lo_imm = static_cast<int32_t>(op.imm);
            const int32_t hi_imm = op.imm < 0 ? -1 : 0;
            const GpReg lo = gp(op.var);
            const GpReg hi = gp_hi(op.var);

            switch (op.cmp)
            {
                case CgCmp::kEq:
                {
                    const Label skip = e_.new_label();
                    e_.cmp(hi, hi_imm);
                    e_.jcc(Cond::ne, skip);
                    e_.cmp(lo, lo_imm);
                    e_.jcc(Cond::e, target);
                    e_.bind(skip);
                    break;
                }
                case CgCmp::kNe:
                    e_.cmp(hi, hi_imm);
                    e_.jcc(Cond::ne, target);
                    e_.cmp(lo, lo_imm);
                    e_.jcc(Cond::ne, target);
                    break;
                case CgCmp::kLt:
                {
                    const Label skip = e_.new_label();
                    e_.cmp(hi, hi_imm);
                    e_.jcc(Cond::l, target);
                    e_.jcc(Cond::g, skip);
                    e_.cmp(lo, lo_imm);
                    e_.jcc(Cond::b, target);
                    e_.bind(skip);
                    break;
                }
                case CgCmp::kGe:
                {
                    const Label skip = e_.new_label();
                    e_.cmp(hi, hi_imm);
                    e_.jcc(Cond::g, target);
                    e_.jcc(Cond::l, skip);
                    e_.cmp(lo, lo_imm);
                    e_.jcc(Cond::ae, target);
                    e_.bind(skip);
                    break;
                }
                case CgCmp::kGt:
                {
                    const Label skip = e_.new_label();
                    e_.cmp(hi, hi_imm);
                    e_.jcc(Cond::g, target);
                    e_.jcc(Cond::l, skip);
                    e_.cmp(lo, lo_imm);
                    e_.jcc(Cond::a, target);
                    e_.bind(skip);
                    break;
                }
                case CgCmp::kLe:
                {
                    const Label skip = e_.new_label();
                    e_.cmp(hi, hi_imm);
                    e_.jcc(Cond::l, target);
                    e_.jcc(Cond::g, skip);
                    e_.cmp(lo, lo_imm);
                    e_.jcc(Cond::be, target);
                    e_.bind(skip);
                    break;
                }
            }
        }
    }

    void CodegenX86::emit_const_f64(const CgOp& op)
    {
        alloc_f64(op.var);
        if (failed_)
        {
            return;
        }

        if constexpr (kMode64)
        {
            e_.mov(GpReg::r11, static_cast<uint64_t>(op.imm));
            e_.movq(xmm(op.var), GpReg::r11);
        }
        else
        {
            const auto bits = static_cast<uint64_t>(op.imm);
            e_.mov32(mem(kStackPtr, 0), static_cast<uint32_t>(bits));
            e_.mov32(mem(kStackPtr, 4), static_cast<uint32_t>(bits >> 32));
            e_.movsd(xmm(op.var), mem(kStackPtr, 0));
        }
    }

    void CodegenX86::compute_liveness(const CgProgram& program)
    {
        last_pos_.assign(program.num_vars, 0);
        var_reg_.assign(program.num_vars, kNoReg);
        var_reg2_.assign(program.num_vars, kNoReg);
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
                    last_pos_[op.var] = i;
                    break;
                case CgOpKind::kAddI64:
                case CgOpKind::kSubI64:
                case CgOpKind::kMulI64:
                case CgOpKind::kModI64:
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

    void CodegenX86::release_dead(const CgOp& op, uint32_t index)
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

    void CodegenX86::lower(const CgOp& op, uint32_t index)
    {
        switch (op.kind)
        {
            case CgOpKind::kBind:
                e_.bind(label(op.label));
                if (op.flag)
                {
                    base_valid_ = false;
                }
                break;

            case CgOpKind::kJump:
                e_.jmp(label(op.label));
                break;

            case CgOpKind::kGuardTag:
                ensure_base();
                e_.cmp8(slot_tag(op.slot), op.tag);
                e_.jcc(Cond::ne, label(op.label));
                break;

            case CgOpKind::kCopySlot:
                ensure_base();
                e_.movups(XmmReg::xmm5, slot_tag(static_cast<int32_t>(op.imm)));
                e_.movups(slot_tag(op.slot), XmmReg::xmm5);
                break;

            case CgOpKind::kStoreTag:
                ensure_base();
                e_.mov32(slot_tag(op.slot), op.tag);
                break;

            case CgOpKind::kLoadI64:
                ensure_base();
                alloc_i64(op.var);
                if (!failed_)
                {
                    e_.mov(gp(op.var), slot_payload(op.slot));
                    if constexpr (!kMode64)
                    {
                        e_.mov(gp_hi(op.var), slot_payload_hi(op.slot));
                    }
                }
                break;

            case CgOpKind::kLoadF64:
                ensure_base();
                alloc_f64(op.var);
                if (!failed_)
                {
                    e_.movsd(xmm(op.var), slot_payload(op.slot));
                }
                break;

            case CgOpKind::kConstI64:
                alloc_i64(op.var);
                if (!failed_)
                {
                    if constexpr (kMode64)
                    {
                        e_.mov(gp(op.var), static_cast<uint64_t>(op.imm));
                    }
                    else
                    {
                        e_.mov32(gp(op.var), static_cast<uint32_t>(op.imm));
                        e_.mov32(gp_hi(op.var), static_cast<uint32_t>(static_cast<uint64_t>(op.imm) >> 32));
                    }
                }
                break;

            case CgOpKind::kConstF64:
                emit_const_f64(op);
                break;

            case CgOpKind::kStoreI64:
                ensure_base();
                if (!failed_)
                {
                    e_.mov(slot_payload(op.slot), gp(op.var));
                    if constexpr (!kMode64)
                    {
                        e_.mov(slot_payload_hi(op.slot), gp_hi(op.var));
                    }
                }
                break;

            case CgOpKind::kStoreF64:
                ensure_base();
                if (!failed_)
                {
                    e_.movsd(slot_payload(op.slot), xmm(op.var));
                }
                break;

            case CgOpKind::kAddI64:
                if (!failed_)
                {
                    e_.add(gp(op.var), gp(op.var2));
                    if constexpr (!kMode64)
                    {
                        e_.adc(gp_hi(op.var), gp_hi(op.var2));
                    }
                }
                break;

            case CgOpKind::kSubI64:
                if (!failed_)
                {
                    e_.sub(gp(op.var), gp(op.var2));
                    if constexpr (!kMode64)
                    {
                        e_.sbb(gp_hi(op.var), gp_hi(op.var2));
                    }
                }
                break;

            case CgOpKind::kAndI64:
                if (!failed_)
                {
                    e_.and_(gp(op.var), gp(op.var2));
                    if constexpr (!kMode64)
                    {
                        e_.and_(gp_hi(op.var), gp_hi(op.var2));
                    }
                }
                break;

            case CgOpKind::kOrI64:
                if (!failed_)
                {
                    e_.or_(gp(op.var), gp(op.var2));
                    if constexpr (!kMode64)
                    {
                        e_.or_(gp_hi(op.var), gp_hi(op.var2));
                    }
                }
                break;

            case CgOpKind::kXorI64:
                if (!failed_)
                {
                    e_.xor_(gp(op.var), gp(op.var2));
                    if constexpr (!kMode64)
                    {
                        e_.xor_(gp_hi(op.var), gp_hi(op.var2));
                    }
                }
                break;

            case CgOpKind::kModI64:
                if (!failed_)
                {
                    if constexpr (kMode64)
                    {
                        const Label neg1 = e_.new_label();
                        const Label done = e_.new_label();
                        e_.mov(mem(kStackPtr, 0), gp(op.var));
                        e_.mov(mem(kStackPtr, 8), gp(op.var2));
                        e_.mov(kScratchB, mem(kStackPtr, 8));
                        e_.cmp(kScratchB, 0);
                        e_.jcc(Cond::e, label(op.label));
                        e_.cmp(kScratchB, -1);
                        e_.jcc(Cond::e, neg1);
                        e_.mov(kScratchA, mem(kStackPtr, 0));
                        e_.cqo();
                        e_.idiv(kScratchB);
                        e_.mov(gp(op.var), kScratchC);
                        e_.jmp(done);
                        e_.bind(neg1);
                        e_.xor_(gp(op.var), gp(op.var));
                        e_.bind(done);
                    }
                    else
                    {
                        const Label nonzero = e_.new_label();
                        e_.cmp(gp(op.var2), 0);
                        e_.jcc(Cond::ne, nonzero);
                        e_.cmp(gp_hi(op.var2), 0);
                        e_.jcc(Cond::e, label(op.label));
                        e_.bind(nonzero);
                        e_.push(gp_hi(op.var2));
                        e_.push(gp(op.var2));
                        e_.push(gp_hi(op.var));
                        e_.push(gp(op.var));
                        e_.call(reinterpret_cast<uintptr_t>(&jit_i64_mod));
                        e_.add(kStackPtr, 16);
                        e_.mov(mem(kStackPtr, 0), kScratchA);
                        e_.mov(gp_hi(op.var), kScratchC);
                        e_.mov(gp(op.var), mem(kStackPtr, 0));
                    }
                }
                break;

            case CgOpKind::kBranchI64:
                if (!failed_)
                {
                    if constexpr (kMode64)
                    {
                        e_.cmp(gp(op.var), gp(op.var2));
                        e_.jcc(cond_signed(op.cmp), label(op.label));
                    }
                    else
                    {
                        const Label target = label(op.label);
                        const GpReg lo = gp(op.var);
                        const GpReg hi = gp_hi(op.var);
                        const GpReg lo2 = gp(op.var2);
                        const GpReg hi2 = gp_hi(op.var2);

                        switch (op.cmp)
                        {
                            case CgCmp::kEq:
                            {
                                const Label skip = e_.new_label();
                                e_.cmp(hi, hi2);
                                e_.jcc(Cond::ne, skip);
                                e_.cmp(lo, lo2);
                                e_.jcc(Cond::e, target);
                                e_.bind(skip);
                                break;
                            }
                            case CgCmp::kNe:
                                e_.cmp(hi, hi2);
                                e_.jcc(Cond::ne, target);
                                e_.cmp(lo, lo2);
                                e_.jcc(Cond::ne, target);
                                break;
                            case CgCmp::kLt:
                            {
                                const Label skip = e_.new_label();
                                e_.cmp(hi, hi2);
                                e_.jcc(Cond::l, target);
                                e_.jcc(Cond::g, skip);
                                e_.cmp(lo, lo2);
                                e_.jcc(Cond::b, target);
                                e_.bind(skip);
                                break;
                            }
                            case CgCmp::kGe:
                            {
                                const Label skip = e_.new_label();
                                e_.cmp(hi, hi2);
                                e_.jcc(Cond::g, target);
                                e_.jcc(Cond::l, skip);
                                e_.cmp(lo, lo2);
                                e_.jcc(Cond::ae, target);
                                e_.bind(skip);
                                break;
                            }
                            case CgCmp::kGt:
                            {
                                const Label skip = e_.new_label();
                                e_.cmp(hi, hi2);
                                e_.jcc(Cond::g, target);
                                e_.jcc(Cond::l, skip);
                                e_.cmp(lo, lo2);
                                e_.jcc(Cond::a, target);
                                e_.bind(skip);
                                break;
                            }
                            case CgCmp::kLe:
                            {
                                const Label skip = e_.new_label();
                                e_.cmp(hi, hi2);
                                e_.jcc(Cond::l, target);
                                e_.jcc(Cond::g, skip);
                                e_.cmp(lo, lo2);
                                e_.jcc(Cond::be, target);
                                e_.bind(skip);
                                break;
                            }
                        }
                    }
                }
                break;

            case CgOpKind::kBranchTruthy:
            {
                ensure_base();
                const Label truthy = label(op.label);
                const Label falsy = label(op.label2);
                e_.cmp8(slot_tag(op.slot), static_cast<uint8_t>(Type::kNil));
                e_.jcc(Cond::e, falsy);
                e_.cmp8(slot_tag(op.slot), static_cast<uint8_t>(Type::kBoolean));
                e_.jcc(Cond::ne, truthy);
                e_.cmp8(slot_payload(op.slot), 0);
                e_.jcc(Cond::e, falsy);
                e_.jmp(truthy);
                break;
            }

            case CgOpKind::kAddI64Imm:
                assert(op.imm >= INT32_MIN && op.imm <= INT32_MAX);
                if (!failed_)
                {
                    e_.add(gp(op.var), static_cast<int32_t>(op.imm));
                    if constexpr (!kMode64)
                    {
                        e_.adc(gp_hi(op.var), op.imm < 0 ? -1 : 0);
                    }
                }
                break;

            case CgOpKind::kAddF64:
                if (!failed_)
                {
                    e_.addsd(xmm(op.var), xmm(op.var2));
                }
                break;

            case CgOpKind::kSubF64:
                if (!failed_)
                {
                    e_.subsd(xmm(op.var), xmm(op.var2));
                }
                break;

            case CgOpKind::kMulF64:
                if (!failed_)
                {
                    e_.mulsd(xmm(op.var), xmm(op.var2));
                }
                break;

            case CgOpKind::kDivF64:
                if (!failed_)
                {
                    e_.divsd(xmm(op.var), xmm(op.var2));
                }
                break;

            case CgOpKind::kCvtSlotToF64:
                ensure_base();
                alloc_f64(op.var);
                if (!failed_)
                {
                    if constexpr (kMode64)
                    {
                        e_.cvtsi2sd(xmm(op.var), slot_payload(op.slot));
                    }
                    else
                    {
                        e_.fild_qword(slot_payload(op.slot));
                        e_.fstp_qword(mem(kStackPtr, 0));
                        e_.movsd(xmm(op.var), mem(kStackPtr, 0));
                    }
                }
                break;

            case CgOpKind::kMulI64:
                if (!failed_)
                {
                    if constexpr (kMode64)
                    {
                        e_.imul(gp(op.var), gp(op.var2));
                    }
                    else
                    {
                        e_.mov(mem(kStackPtr, 0), gp(op.var));
                        e_.mov(mem(kStackPtr, 4), gp_hi(op.var));
                        e_.mov(mem(kStackPtr, 8), gp(op.var2));
                        e_.mov(mem(kStackPtr, 12), gp_hi(op.var2));
                        e_.mov(kScratchA, mem(kStackPtr, 0));
                        e_.mul(mem(kStackPtr, 8));
                        e_.mov(kScratchB, mem(kStackPtr, 4));
                        e_.imul(kScratchB, mem(kStackPtr, 8));
                        e_.add(kScratchC, kScratchB);
                        e_.mov(kScratchB, mem(kStackPtr, 12));
                        e_.imul(kScratchB, mem(kStackPtr, 0));
                        e_.add(kScratchC, kScratchB);
                        e_.mov(gp_hi(op.var), kScratchC);
                        e_.mov(gp(op.var), kScratchA);
                    }
                }
                break;

            case CgOpKind::kShlI64:
            case CgOpKind::kShrI64:
                if (!failed_)
                {
                    const bool is_shl = op.kind == CgOpKind::kShlI64;
                    if constexpr (kMode64)
                    {
                        e_.mov(mem(kStackPtr, 0), gp(op.var));
                        e_.mov(mem(kStackPtr, 8), gp(op.var2));
                        e_.mov(kScratchA, mem(kStackPtr, 0));
                        e_.mov(kScratchB, mem(kStackPtr, 8));
                        if (is_shl)
                        {
                            e_.shl_cl(kScratchA);
                        }
                        else
                        {
                            e_.sar_cl(kScratchA);
                        }
                        e_.mov(gp(op.var), kScratchA);
                    }
                    else
                    {
                        e_.mov(mem(kStackPtr, 0), gp(op.var));
                        e_.mov(mem(kStackPtr, 4), gp_hi(op.var));
                        e_.mov(mem(kStackPtr, 8), gp(op.var2));
                        e_.mov(kScratchA, mem(kStackPtr, 0));
                        e_.mov(kScratchC, mem(kStackPtr, 4));
                        e_.mov(kScratchB, mem(kStackPtr, 8));
                        e_.and_(kScratchB, 255);

                        const Label upper = e_.new_label();
                        const Label big = e_.new_label();
                        const Label done = e_.new_label();

                        e_.cmp(kScratchB, 64);
                        e_.jcc(Cond::ae, big);
                        e_.cmp(kScratchB, 32);
                        e_.jcc(Cond::ae, upper);
                        if (is_shl)
                        {
                            e_.shld_cl(kScratchC, kScratchA);
                            e_.shl_cl(kScratchA);
                        }
                        else
                        {
                            e_.shrd_cl(kScratchA, kScratchC);
                            e_.sar_cl(kScratchC);
                        }
                        e_.jmp(done);

                        e_.bind(upper);
                        if (is_shl)
                        {
                            e_.mov(kScratchC, kScratchA);
                            e_.shl_cl(kScratchC);
                            e_.xor_(kScratchA, kScratchA);
                        }
                        else
                        {
                            e_.mov(kScratchA, kScratchC);
                            e_.sar_cl(kScratchA);
                            e_.sar(kScratchC, 31);
                        }
                        e_.jmp(done);

                        e_.bind(big);
                        if (is_shl)
                        {
                            e_.xor_(kScratchA, kScratchA);
                            e_.xor_(kScratchC, kScratchC);
                        }
                        else
                        {
                            e_.sar(kScratchC, 31);
                            e_.mov(kScratchA, kScratchC);
                        }

                        e_.bind(done);
                        e_.mov(gp(op.var), kScratchA);
                        e_.mov(gp_hi(op.var), kScratchC);
                    }
                }
                break;

            case CgOpKind::kBranchI64Imm:
                if (!failed_)
                {
                    emit_branch_i64_imm(op);
                }
                break;

            case CgOpKind::kBranchF64:
            {
                if (failed_)
                {
                    break;
                }
                const Label true_label = label(op.label);
                const Label false_label = label(op.label2);
                const XmmReg lhs = xmm(op.var);
                const XmmReg rhs = xmm(op.var2);

                switch (op.cmp)
                {
                    case CgCmp::kLt:
                        e_.ucomisd(rhs, lhs);
                        e_.jcc(Cond::a, true_label);
                        e_.jmp(false_label);
                        break;
                    case CgCmp::kLe:
                        e_.ucomisd(rhs, lhs);
                        e_.jcc(Cond::ae, true_label);
                        e_.jmp(false_label);
                        break;
                    case CgCmp::kGt:
                        e_.ucomisd(lhs, rhs);
                        e_.jcc(Cond::a, true_label);
                        e_.jmp(false_label);
                        break;
                    case CgCmp::kGe:
                        e_.ucomisd(lhs, rhs);
                        e_.jcc(Cond::ae, true_label);
                        e_.jmp(false_label);
                        break;
                    case CgCmp::kEq:
                        e_.ucomisd(lhs, rhs);
                        e_.jcc(Cond::p, false_label);
                        e_.jcc(Cond::e, true_label);
                        e_.jmp(false_label);
                        break;
                    case CgCmp::kNe:
                        e_.ucomisd(lhs, rhs);
                        e_.jcc(Cond::p, true_label);
                        e_.jcc(Cond::ne, true_label);
                        e_.jmp(false_label);
                        break;
                }
                break;
            }

            case CgOpKind::kBranchVarEqU32:
                if (!failed_)
                {
                    e_.cmp32(gp(op.var), static_cast<uint32_t>(op.imm));
                    e_.jcc(Cond::e, label(op.label));
                }
                break;

            case CgOpKind::kHelperCall:
                emit_helper_call(op);
                break;

            case CgOpKind::kSyncFrame:
                assert(gp_used_ == 0 && xmm_used_ == 0 && "frame sync with live variables");
                emit_base_refresh();
                base_valid_ = true;
                break;

            case CgOpKind::kReturnResult:
                emit_epilogue(static_cast<uint32_t>(op.imm));
                break;
        }

        release_dead(op, index);
    }

    JitEntry CodegenX86::generate(State* S, const CgProgram& program)
    {
        compute_liveness(program);

        for (uint32_t i = 0; i < program.num_labels; ++i)
        {
            e_.new_label();
        }

        emit_prologue();

        for (uint32_t i = 0; i < program.ops.size(); ++i)
        {
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
