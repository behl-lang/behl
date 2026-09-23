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

    static constexpr int32_t kFrameSize = 48;
    static constexpr int32_t kSavedLinkSlot = 16;
    static constexpr int32_t kEntryDepthSlot = 24;
    static constexpr int32_t kSpillItems = 32;
    static constexpr int32_t kSpillBase = 40;

    static constexpr int32_t kCallFrameStride = static_cast<int32_t>(sizeof(CallFrame));
    static constexpr int32_t kCallHeaderTop = 0;
    static constexpr int32_t kCallHeaderCallPos = 4;
    static constexpr int32_t kCallHeaderNumVarargs = 8;
    static constexpr int32_t kCallHeaderDeferMask = 12;
    static constexpr int32_t kCallHeaderRetBase = 16;
    static constexpr int32_t kCallHeaderNResults = 20;

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

    static_assert(sizeof(CallFrame) == 16, "call frame stride must stay a power of two for the shift below");
    static constexpr uint8_t kCallFrameShift = 4;

    void CodegenAArch64::emit_base_refresh()
    {
        e_.ldr(A64Reg::x0, mem(kStateReg, State::call_stack_data_offset()));
        e_.ldr(A64Reg::x1, mem(kStateReg, State::call_stack_size_offset()));
        e_.sub(A64Reg::x1, A64Reg::x1, 1);
        e_.lsl(A64Reg::x1, A64Reg::x1, kCallFrameShift);
        e_.add(A64Reg::x0, A64Reg::x0, A64Reg::x1);
        e_.ldrw(A64Reg::x1, mem(A64Reg::x0, CallFrame::base_offset()));
        e_.lsl(A64Reg::x1, A64Reg::x1, 4);
        e_.ldr(kFrameBase, mem(kStateReg, State::stack_data_offset()));
        e_.add(kFrameBase, kFrameBase, A64Reg::x1);
    }

    void CodegenAArch64::emit_prologue()
    {
        e_.stp_pre(kStateReg, kFrameBase, A64Reg::sp, -kFrameSize);
        e_.str(A64Reg::x30, mem(A64Reg::sp, kSavedLinkSlot));
        e_.mov(kStateReg, A64Reg::x0);
        e_.ldr(A64Reg::x0, mem(kStateReg, State::call_stack_size_offset()));
        e_.str(A64Reg::x0, mem(A64Reg::sp, kEntryDepthSlot));
        base_valid_ = false;
    }

    void CodegenAArch64::emit_epilogue(uint32_t result_code)
    {
        e_.mov32(A64Reg::x0, result_code);
        e_.ldr(A64Reg::x30, mem(A64Reg::sp, kSavedLinkSlot));
        e_.ldp_post(kStateReg, kFrameBase, A64Reg::sp, kFrameSize);
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
        if (!op.flag)
        {
            base_valid_ = false;
        }
        alloc_result(op.var);
    }

    void CodegenAArch64::emit_tail_jump_native(const CgOp& op)
    {
        constexpr A64Reg kTarget = A64Reg::x17;

        e_.ldr(A64Reg::x0, mem(kStateReg, State::call_stack_size_offset()));
        e_.ldr(A64Reg::x1, mem(A64Reg::sp, kEntryDepthSlot));
        e_.cmp(A64Reg::x0, A64Reg::x1);
        e_.bcond(A64Cond::ne, label(op.label));

        e_.ldr(A64Reg::x1, mem(kStateReg, State::call_stack_data_offset()));
        e_.sub(A64Reg::x0, A64Reg::x0, 1);
        e_.lsl(A64Reg::x0, A64Reg::x0, kCallFrameShift);
        e_.add(A64Reg::x1, A64Reg::x1, A64Reg::x0);
        e_.ldr(kTarget, mem(A64Reg::x1, CallFrame::proto_offset()));
        e_.ldr(kTarget, mem(kTarget, GCProto::jit_code_offset()));
        e_.cmp(kTarget, 0u);
        e_.bcond(A64Cond::eq, label(op.label));

        e_.mov(A64Reg::x0, kStateReg);
        e_.ldr(A64Reg::x30, mem(A64Reg::sp, kSavedLinkSlot));
        e_.ldp_post(kStateReg, kFrameBase, A64Reg::sp, kFrameSize);
        e_.br(kTarget);
    }

    void CodegenAArch64::emit_return_dispatch(const CgOp& op)
    {
        e_.ldr(A64Reg::x0, mem(kStateReg, State::call_stack_size_offset()));
        e_.ldr(A64Reg::x1, mem(A64Reg::sp, kEntryDepthSlot));
        e_.cmp(A64Reg::x0, A64Reg::x1);
        e_.bcond(A64Cond::hs, label(op.label));
        e_.b(label(op.label2));
    }

    void CodegenAArch64::emit_return_fast(const CgOp& op)
    {
        const int32_t a = op.slot;
        const int32_t moved = static_cast<int32_t>(op.var);
        const A64Label slow = label(op.label);

        constexpr A64Reg kIdx = A64Reg::x0;
        constexpr A64Reg kDest = A64Reg::x1;
        constexpr A64Reg kSize = A64Reg::x2;
        constexpr A64Reg kTmpA = A64Reg::x9;
        constexpr A64Reg kTmpB = A64Reg::x10;

        ensure_base();

        e_.ldr(kIdx, mem(kStateReg, State::call_stack_size_offset()));
        e_.cmp(kIdx, 2u);
        e_.bcond(A64Cond::lo, slow);

        e_.sub(kSize, kIdx, 1);
        e_.ldr(kTmpA, mem(A64Reg::sp, kEntryDepthSlot));
        e_.cmp(kSize, kTmpA);
        e_.bcond(A64Cond::lo, slow);

        e_.ldr(kTmpA, mem(kStateReg, State::call_headers_data_offset()));
        e_.lsl(kTmpB, kSize, 1);
        e_.add(kTmpB, kTmpB, kSize);
        e_.lsl(kTmpB, kTmpB, 3);
        e_.add(kTmpA, kTmpA, kTmpB);

        const A64Label nresults_ok = e_.new_label();
        e_.ldrw(kTmpB, mem(kTmpA, kCallHeaderNResults));
        e_.cmpw(kTmpB, static_cast<uint32_t>(moved));
        e_.bcond(A64Cond::eq, nresults_ok);
        e_.cmpw(kTmpB, 255u);
        e_.bcond(A64Cond::ne, slow);
        e_.bind(nresults_ok);
        e_.ldrw(kDest, mem(kTmpA, kCallHeaderCallPos));

        if (moved == 1)
        {
            e_.ldr(kTmpA, mem(kStateReg, State::stack_data_offset()));
            e_.lsl(kTmpB, kDest, 4);
            e_.add(kTmpA, kTmpA, kTmpB);
            e_.ldr(kTmpB, mem(kFrameBase, a * Value::size()));
            e_.str(kTmpB, mem(kTmpA, 0));
            e_.ldr(kTmpB, mem(kFrameBase, a * Value::size() + 8));
            e_.str(kTmpB, mem(kTmpA, 8));
        }

        e_.ldr(kTmpA, mem(kStateReg, State::call_stack_data_offset()));
        e_.sub(kTmpB, kIdx, 2);
        e_.lsl(kTmpB, kTmpB, kCallFrameShift);
        e_.add(kTmpA, kTmpA, kTmpB);
        e_.ldr(kTmpB, mem(kTmpA, CallFrame::proto_offset()));
        e_.cmp(kTmpB, 0u);
        e_.bcond(A64Cond::eq, slow);
        e_.ldrw(kTmpB, mem(kTmpB, GCProto::max_stack_size_offset()));
        e_.ldrw(kTmpA, mem(kTmpA, CallFrame::base_offset()));
        e_.add(kTmpB, kTmpB, kTmpA);

        e_.add(kTmpA, kDest, static_cast<uint32_t>(moved));

        const A64Label have_size = e_.new_label();
        e_.cmp(kTmpB, kTmpA);
        e_.bcond(A64Cond::hs, have_size);
        e_.mov(kTmpB, kTmpA);
        e_.bind(have_size);

        e_.ldr(kIdx, mem(kStateReg, State::stack_size_offset()));
        e_.cmp(kTmpB, kIdx);
        e_.bcond(A64Cond::hi, slow);
        e_.str(kTmpB, mem(kStateReg, State::stack_size_offset()));

        e_.str(kSize, mem(kStateReg, State::call_stack_size_offset()));
        e_.str(kSize, mem(kStateReg, State::call_headers_size_offset()));

        e_.ldr(kTmpB, mem(kStateReg, State::call_headers_data_offset()));
        e_.sub(kIdx, kSize, 1);
        e_.lsl(kDest, kIdx, 1);
        e_.add(kDest, kDest, kIdx);
        e_.lsl(kDest, kDest, 3);
        e_.add(kTmpB, kTmpB, kDest);
        e_.strw(kTmpA, mem(kTmpB, kCallHeaderTop));

        base_valid_ = false;
    }

    void CodegenAArch64::emit_frame_push_fast(const CgOp& op)
    {
        const auto* proto = reinterpret_cast<const GCProto*>(static_cast<uintptr_t>(op.imm));
        const int32_t a = op.slot;
        const int32_t num_args = static_cast<int32_t>(op.var);
        const int32_t nresults = static_cast<int32_t>(op.var2);
        const int32_t window = static_cast<int32_t>(proto->max_stack_size);
        const int32_t items = num_args + 1;
        const int32_t needed = (items > window) ? items : window;

        const A64Label slow = label(op.label);
        constexpr A64Reg kPos = A64Reg::x2;
        constexpr A64Reg kIdx = A64Reg::x0;
        constexpr A64Reg kReq = A64Reg::x1;
        constexpr A64Reg kTmpA = A64Reg::x9;
        constexpr A64Reg kTmpB = A64Reg::x10;

        ensure_base();

        e_.ldr(kTmpA, mem(kStateReg, State::call_stack_data_offset()));
        e_.ldr(kTmpB, mem(kStateReg, State::call_stack_size_offset()));
        e_.sub(kPos, kTmpB, 1);
        e_.lsl(kPos, kPos, kCallFrameShift);
        e_.add(kTmpA, kTmpA, kPos);
        e_.ldrw(kPos, mem(kTmpA, CallFrame::base_offset()));
        emit_add_imm(kPos, a);

        e_.ldr(kTmpA, mem(kStateReg, State::gc_debt_offset()));
        e_.cmp(kTmpA, 0u);
        e_.bcond(A64Cond::gt, slow);

        e_.ldr(kIdx, mem(kStateReg, State::call_stack_size_offset()));
        e_.ldr(kTmpA, mem(kStateReg, State::call_stack_capacity_offset()));
        e_.cmp(kIdx, kTmpA);
        e_.bcond(A64Cond::hs, slow);
        e_.ldr(kTmpA, mem(kStateReg, State::call_headers_size_offset()));
        e_.ldr(kTmpB, mem(kStateReg, State::call_headers_capacity_offset()));
        e_.cmp(kTmpA, kTmpB);
        e_.bcond(A64Cond::hs, slow);

        e_.mov(kReq, kPos);
        emit_add_imm(kReq, needed);
        e_.ldr(kTmpA, mem(kStateReg, State::stack_capacity_offset()));
        e_.cmp(kReq, kTmpA);
        e_.bcond(A64Cond::hi, slow);

        if (proto->has_upvalues)
        {
            e_.ldr(kTmpA, mem(kFrameBase, 0));
            e_.ldr(kTmpB, mem(kFrameBase, 8));
            e_.str(kTmpA, mem(kFrameBase, a * Value::size()));
            e_.str(kTmpB, mem(kFrameBase, a * Value::size() + 8));
        }

        if (num_args < static_cast<int32_t>(proto->num_params))
        {
            e_.mov32(kScratch, 0);
            for (int32_t i = num_args; i < static_cast<int32_t>(proto->num_params); ++i)
            {
                e_.strw(kScratch, mem(kFrameBase, (a + 1 + i) * Value::size() + Value::type_offset()));
            }
        }

        e_.ldr(kTmpA, mem(kStateReg, State::call_stack_data_offset()));
        e_.lsl(kTmpB, kIdx, kCallFrameShift);
        e_.add(kTmpA, kTmpA, kTmpB);
        e_.sub(kTmpB, kTmpA, static_cast<uint32_t>(kCallFrameStride));
        e_.mov32(kScratch, static_cast<uint32_t>(op.pcn));
        e_.strw(kScratch, mem(kTmpB, CallFrame::pc_offset()));

        e_.mov(kScratch, reinterpret_cast<uint64_t>(proto));
        e_.str(kScratch, mem(kTmpA, CallFrame::proto_offset()));
        e_.mov32(kScratch, 0);
        e_.strw(kScratch, mem(kTmpA, CallFrame::pc_offset()));
        e_.strw(kPos, mem(kTmpA, CallFrame::base_offset()));

        e_.ldr(kTmpA, mem(kStateReg, State::call_headers_data_offset()));
        e_.lsl(kTmpB, kIdx, 1);
        e_.add(kTmpB, kTmpB, kIdx);
        e_.lsl(kTmpB, kTmpB, 3);
        e_.add(kTmpA, kTmpA, kTmpB);
        e_.mov(kTmpB, kPos);
        emit_add_imm(kTmpB, items);
        e_.strw(kTmpB, mem(kTmpA, kCallHeaderTop));
        e_.strw(kPos, mem(kTmpA, kCallHeaderCallPos));
        e_.mov32(kScratch, 0);
        e_.strw(kScratch, mem(kTmpA, kCallHeaderNumVarargs));
        e_.strw(kScratch, mem(kTmpA, kCallHeaderDeferMask));
        e_.strw(kScratch, mem(kTmpA, kCallHeaderRetBase));
        e_.mov32(kScratch, static_cast<uint32_t>(nresults));
        e_.strw(kScratch, mem(kTmpA, kCallHeaderNResults));

        e_.add(kTmpB, kIdx, 1);
        e_.str(kTmpB, mem(kStateReg, State::call_stack_size_offset()));
        e_.str(kTmpB, mem(kStateReg, State::call_headers_size_offset()));

        const A64Label done = e_.new_label();
        e_.ldr(kIdx, mem(kStateReg, State::stack_size_offset()));
        e_.cmp(kIdx, kReq);
        e_.bcond(A64Cond::hs, done);

        const A64Label fill = e_.new_label();
        e_.ldr(kTmpA, mem(kStateReg, State::stack_data_offset()));
        e_.lsl(kTmpB, kIdx, 4);
        e_.add(kTmpA, kTmpA, kTmpB);
        e_.mov32(kScratch, 0);
        e_.bind(fill);
        e_.strw(kScratch, mem(kTmpA, Value::type_offset()));
        e_.add(kTmpA, kTmpA, static_cast<uint32_t>(Value::size()));
        e_.add(kIdx, kIdx, 1);
        e_.cmp(kIdx, kReq);
        e_.bcond(A64Cond::lo, fill);
        e_.str(kReq, mem(kStateReg, State::stack_size_offset()));
        e_.bind(done);

        emit_add_imm(kFrameBase, a * Value::size());
        base_valid_ = true;
    }

    void CodegenAArch64::emit_tail_frame_fast(const CgOp& op)
    {
        const auto* proto = reinterpret_cast<const GCProto*>(static_cast<uintptr_t>(op.imm));
        const int32_t a = op.slot;
        const int32_t num_args = static_cast<int32_t>(op.var);
        const int32_t window = static_cast<int32_t>(proto->max_stack_size);
        const int32_t items = num_args + 1;
        const A64Label slow = label(op.label);

        constexpr A64Reg kBase = A64Reg::x2;
        constexpr A64Reg kReq = A64Reg::x1;
        constexpr A64Reg kTmpA = A64Reg::x9;
        constexpr A64Reg kTmpB = A64Reg::x10;
        constexpr A64Reg kCur = A64Reg::x0;

        ensure_base();

        e_.ldr(kTmpA, mem(kStateReg, State::call_stack_data_offset()));
        e_.ldr(kTmpB, mem(kStateReg, State::call_stack_size_offset()));
        e_.sub(kCur, kTmpB, 1);
        e_.lsl(kCur, kCur, kCallFrameShift);
        e_.add(kTmpA, kTmpA, kCur);
        e_.ldrw(kBase, mem(kTmpA, CallFrame::base_offset()));

        if (op.flag)
        {
            e_.ldr(kCur, mem(kStateReg, State::call_headers_data_offset()));
            e_.sub(kReq, kTmpB, 1);
            e_.lsl(kScratch, kReq, 1);
            e_.add(kReq, kScratch, kReq);
            e_.lsl(kReq, kReq, 3);
            e_.add(kCur, kCur, kReq);

            e_.ldrw(kReq, mem(kCur, kCallHeaderTop));
            e_.sub(kReq, kReq, kBase);
            e_.sub(kReq, kReq, static_cast<uint32_t>(a));
            e_.cmp(kReq, 1u);
            e_.bcond(A64Cond::lo, slow);
            emit_cmp_imm(kReq, window);
            e_.bcond(A64Cond::hi, slow);
            e_.str(kReq, mem(A64Reg::sp, kSpillItems));
            e_.str(kBase, mem(A64Reg::sp, kSpillBase));

            e_.mov(kReq, kBase);
            emit_add_imm(kReq, window);
            e_.ldr(kScratch, mem(kStateReg, State::stack_capacity_offset()));
            e_.cmp(kReq, kScratch);
            e_.bcond(A64Cond::hi, slow);

            e_.mov32(kScratch, 0);
            e_.strw(kScratch, mem(kTmpA, CallFrame::pc_offset()));

            e_.ldr(kTmpB, mem(A64Reg::sp, kSpillItems));
            e_.add(kTmpB, kTmpB, kBase);
            e_.strw(kTmpB, mem(kCur, kCallHeaderTop));

            e_.mov(kTmpA, kFrameBase);
            emit_add_imm(kTmpA, (a + 1) * Value::size());
            e_.mov(kTmpB, kFrameBase);
            e_.add(kTmpB, kTmpB, static_cast<uint32_t>(Value::size()));
            e_.ldr(kCur, mem(A64Reg::sp, kSpillItems));
            e_.sub(kCur, kCur, 1);

            const A64Label copy_done = e_.new_label();
            const A64Label copy_loop = e_.new_label();
            e_.cmp(kCur, 0u);
            e_.bcond(A64Cond::ls, copy_done);
            e_.bind(copy_loop);
            e_.ldr(kBase, mem(kTmpA, 0));
            e_.str(kBase, mem(kTmpB, 0));
            e_.ldr(kBase, mem(kTmpA, 8));
            e_.str(kBase, mem(kTmpB, 8));
            e_.add(kTmpA, kTmpA, static_cast<uint32_t>(Value::size()));
            e_.add(kTmpB, kTmpB, static_cast<uint32_t>(Value::size()));
            e_.sub(kCur, kCur, 1);
            e_.cmp(kCur, 0u);
            e_.bcond(A64Cond::hi, copy_loop);
            e_.bind(copy_done);

            e_.ldr(kCur, mem(A64Reg::sp, kSpillItems));
            e_.mov(kTmpA, kFrameBase);
            e_.lsl(kTmpB, kCur, 4);
            e_.add(kTmpA, kTmpA, kTmpB);

            const A64Label pad_done = e_.new_label();
            const A64Label pad_loop = e_.new_label();
            emit_cmp_imm(kCur, window);
            e_.bcond(A64Cond::hs, pad_done);
            e_.mov32(kBase, 0);
            e_.bind(pad_loop);
            e_.strw(kBase, mem(kTmpA, Value::type_offset()));
            e_.add(kTmpA, kTmpA, static_cast<uint32_t>(Value::size()));
            e_.add(kCur, kCur, 1);
            emit_cmp_imm(kCur, window);
            e_.bcond(A64Cond::lo, pad_loop);
            e_.bind(pad_done);

            e_.ldr(kReq, mem(A64Reg::sp, kSpillBase));
            emit_add_imm(kReq, window);

            const A64Label grow_done = e_.new_label();
            e_.ldr(kCur, mem(kStateReg, State::stack_size_offset()));
            e_.cmp(kCur, kReq);
            e_.bcond(A64Cond::hs, grow_done);

            const A64Label grow_fill = e_.new_label();
            e_.ldr(kTmpA, mem(kStateReg, State::stack_data_offset()));
            e_.lsl(kTmpB, kCur, 4);
            e_.add(kTmpA, kTmpA, kTmpB);
            e_.mov32(kBase, 0);
            e_.bind(grow_fill);
            e_.strw(kBase, mem(kTmpA, Value::type_offset()));
            e_.add(kTmpA, kTmpA, static_cast<uint32_t>(Value::size()));
            e_.add(kCur, kCur, 1);
            e_.cmp(kCur, kReq);
            e_.bcond(A64Cond::lo, grow_fill);
            e_.str(kReq, mem(kStateReg, State::stack_size_offset()));
            e_.bind(grow_done);

            base_valid_ = false;
            return;
        }

        e_.mov(kReq, kBase);
        emit_add_imm(kReq, window);
        e_.ldr(kCur, mem(kStateReg, State::stack_capacity_offset()));
        e_.cmp(kReq, kCur);
        e_.bcond(A64Cond::hi, slow);

        for (int32_t i = 0; i < num_args; ++i)
        {
            const int32_t src = (a + 1 + i) * Value::size();
            const int32_t dst = (1 + i) * Value::size();
            e_.ldr(kTmpB, mem(kFrameBase, src));
            e_.str(kTmpB, mem(kFrameBase, dst));
            e_.ldr(kTmpB, mem(kFrameBase, src + 8));
            e_.str(kTmpB, mem(kFrameBase, dst + 8));
        }

        e_.mov32(kScratch, 0);
        for (int32_t i = items; i < window; ++i)
        {
            e_.strw(kScratch, mem(kFrameBase, i * Value::size() + Value::type_offset()));
        }

        e_.strw(kScratch, mem(kTmpA, CallFrame::pc_offset()));

        e_.ldr(kTmpA, mem(kStateReg, State::call_headers_data_offset()));
        e_.ldr(kTmpB, mem(kStateReg, State::call_stack_size_offset()));
        e_.sub(kTmpB, kTmpB, 1);
        e_.lsl(kCur, kTmpB, 1);
        e_.add(kCur, kCur, kTmpB);
        e_.lsl(kCur, kCur, 3);
        e_.add(kTmpA, kTmpA, kCur);
        e_.mov(kTmpB, kBase);
        emit_add_imm(kTmpB, items);
        e_.strw(kTmpB, mem(kTmpA, kCallHeaderTop));

        const A64Label done = e_.new_label();
        e_.ldr(kCur, mem(kStateReg, State::stack_size_offset()));
        e_.cmp(kCur, kReq);
        e_.bcond(A64Cond::hs, done);

        const A64Label fill = e_.new_label();
        e_.ldr(kTmpA, mem(kStateReg, State::stack_data_offset()));
        e_.lsl(kTmpB, kCur, 4);
        e_.add(kTmpA, kTmpA, kTmpB);
        e_.mov32(kBase, 0);
        e_.bind(fill);
        e_.strw(kBase, mem(kTmpA, Value::type_offset()));
        e_.add(kTmpA, kTmpA, static_cast<uint32_t>(Value::size()));
        e_.add(kCur, kCur, 1);
        e_.cmp(kCur, kReq);
        e_.bcond(A64Cond::lo, fill);
        e_.bind(done);
        e_.str(kReq, mem(kStateReg, State::stack_size_offset()));

        base_valid_ = false;
    }

    void CodegenAArch64::emit_call_fast(const CgOp& op)
    {
        assert(gp_used_ == 0 && fp_used_ == 0 && "call fast with live variables");

        constexpr A64Reg kTarget = A64Reg::x17;

        e_.mov(A64Reg::x0, kStateReg);
        e_.mov32(A64Reg::x1, op.raw);
        e_.mov32(A64Reg::x2, op.pcn);
        e_.call(reinterpret_cast<uintptr_t>(&jit_call_setup));

        base_valid_ = false;

        e_.cmp(A64Reg::x0, static_cast<uint32_t>(kJitSetupDecline));
        e_.bcond(A64Cond::eq, label(op.label));
        e_.cmp(A64Reg::x0, static_cast<uint32_t>(kJitSetupError));
        e_.bcond(A64Cond::eq, label(op.var));
        e_.cmp(A64Reg::x0, static_cast<uint32_t>(kJitSetupPushedOther));
        e_.bcond(A64Cond::eq, label(op.var2));

        if (op.flag)
        {
            e_.b(label(static_cast<uint32_t>(op.slot)));
            return;
        }

        e_.mov(kTarget, A64Reg::x0);
        e_.mov(A64Reg::x0, kStateReg);
        e_.blr(kTarget);

        e_.cmpw(A64Reg::x0, kJitResultOk);
        e_.bcond(A64Cond::eq, label(op.label2));
        e_.cmpw(A64Reg::x0, kJitResultError);
        e_.bcond(A64Cond::eq, label(op.var));
        e_.b(label(op.var2));
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
                    e_.cmp(gp(op.var2), 63u);
                    e_.bcond(A64Cond::hi, label(op.label));
                    e_.lslv(gp(op.var), gp(op.var), gp(op.var2));
                }
                break;

            case CgOpKind::kShrI64:
                if (!failed_)
                {
                    e_.cmp(gp(op.var2), 63u);
                    e_.bcond(A64Cond::hi, label(op.label));
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
                    e_.lsl(A64Reg::x1, A64Reg::x1, kCallFrameShift);
                    e_.add(A64Reg::x0, A64Reg::x0, A64Reg::x1);
                    e_.ldrw(gp(op.var), mem(A64Reg::x0, CallFrame::pc_offset()));
                }
                break;

            case CgOpKind::kTailJumpNative:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                emit_tail_jump_native(op);
                break;

            case CgOpKind::kCallFast:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                emit_call_fast(op);
                break;

            case CgOpKind::kFramePushFast:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                emit_frame_push_fast(op);
                break;

            case CgOpKind::kTailFrameFast:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                emit_tail_frame_fast(op);
                break;

            case CgOpKind::kReturnFast:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                if (!failed_)
                {
                    emit_return_fast(op);
                }
                break;

            case CgOpKind::kReturnSelfSite:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                e_.b(label(op.label));
                break;

            case CgOpKind::kReturnDispatch:
                if (cache_enabled_)
                {
                    cache_drop_all();
                }
                emit_return_dispatch(op);
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
                if (op.flag)
                {
                    emit_add_imm(kFrameBase, op.imm);
                }
                else
                {
                    emit_base_refresh();
                }
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

        e_.reserve(program.ops.size(), program.num_labels);

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

        label_states_.clear();
        guard_slots_.clear();
        label_states_.reserve(program.num_labels);
        guard_slots_.reserve(program.num_labels);
        for (uint32_t i = 0; i < program.num_labels; ++i)
        {
            label_states_.emplace_back(state_);
            guard_slots_.emplace_back(AutoVector<int32_t>(state_));
        }

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
