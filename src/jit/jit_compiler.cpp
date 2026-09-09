#include "jit_compiler.hpp"

#include <bit>
#include <cassert>
#include <type_traits>

namespace behl
{
    static_assert(std::is_same_v<FP, double>);

    static JitOpFn plain_helper(OpCode op) noexcept
    {
        switch (op)
        {
            case OpCode::kOpLoadI:
                return jit_op_loadi;
            case OpCode::kOpLoadF:
                return jit_op_loadf;
            case OpCode::kOpLoadS:
                return jit_op_loads;
            case OpCode::kOpLoadNil:
                return jit_op_loadnil;
            case OpCode::kOpGetGlobal:
                return jit_op_getglobal;
            case OpCode::kOpSetGlobal:
                return jit_op_setglobal;
            case OpCode::kOpGetUpval:
                return jit_op_getupval;
            case OpCode::kOpSetUpval:
                return jit_op_setupval;
            case OpCode::kOpGetField:
                return jit_op_getfield;
            case OpCode::kOpGetFieldI:
                return jit_op_getfieldi;
            case OpCode::kOpGetFieldS:
                return jit_op_getfields;
            case OpCode::kOpSetField:
                return jit_op_setfield;
            case OpCode::kOpSetFieldI:
                return jit_op_setfieldi;
            case OpCode::kOpSetFieldS:
                return jit_op_setfields;
            case OpCode::kOpNewTable:
                return jit_op_newtable;
            case OpCode::kOpSetList:
                return jit_op_setlist;
            case OpCode::kOpSelf:
                return jit_op_self;
            case OpCode::kOpAdd:
                return jit_op_add;
            case OpCode::kOpSub:
                return jit_op_sub;
            case OpCode::kOpMul:
                return jit_op_mul;
            case OpCode::kOpDiv:
                return jit_op_div;
            case OpCode::kOpMod:
                return jit_op_mod;
            case OpCode::kOpBand:
                return jit_op_band;
            case OpCode::kOpBor:
                return jit_op_bor;
            case OpCode::kOpBxor:
                return jit_op_bxor;
            case OpCode::kOpShl:
                return jit_op_shl;
            case OpCode::kOpShr:
                return jit_op_shr;
            case OpCode::kOpUnm:
                return jit_op_unm;
            case OpCode::kOpBnot:
                return jit_op_bnot;
            case OpCode::kOpLen:
                return jit_op_len;
            case OpCode::kOpToString:
                return jit_op_tostring;
            case OpCode::kOpToNumber:
                return jit_op_tonumber;
            case OpCode::kOpAddKI:
                return jit_op_addki;
            case OpCode::kOpSubKI:
                return jit_op_subki;
            case OpCode::kOpAddKF:
                return jit_op_addkf;
            case OpCode::kOpAddKS:
                return jit_op_addks;
            case OpCode::kOpMMAdd:
                return jit_op_mmadd;
            case OpCode::kOpMMSub:
                return jit_op_mmsub;
            case OpCode::kOpMMMul:
                return jit_op_mmmul;
            case OpCode::kOpMMDiv:
                return jit_op_mmdiv;
            case OpCode::kOpMMMod:
                return jit_op_mmmod;
            case OpCode::kOpMMPow:
                return jit_op_mmpow;
            case OpCode::kOpMMBand:
                return jit_op_mmband;
            case OpCode::kOpMMBor:
                return jit_op_mmbor;
            case OpCode::kOpMMBxor:
                return jit_op_mmbxor;
            case OpCode::kOpMMShl:
                return jit_op_mmshl;
            case OpCode::kOpMMShr:
                return jit_op_mmshr;
            case OpCode::kOpSubKF:
                return jit_op_subkf;
            case OpCode::kOpIncGlobal:
                return jit_op_incglobal;
            case OpCode::kOpDecGlobal:
                return jit_op_decglobal;
            case OpCode::kOpIncUpvalue:
                return jit_op_incupvalue;
            case OpCode::kOpDecUpvalue:
                return jit_op_decupvalue;
            case OpCode::kOpAddLocal:
                return jit_op_addlocal;
            default:
                return nullptr;
        }
    }

    static JitOpFn branch_helper(OpCode op) noexcept
    {
        switch (op)
        {
            case OpCode::kOpEq:
                return jit_op_eq;
            case OpCode::kOpNe:
                return jit_op_ne;
            case OpCode::kOpLt:
                return jit_op_lt;
            case OpCode::kOpGe:
                return jit_op_ge;
            case OpCode::kOpLe:
                return jit_op_le;
            case OpCode::kOpGt:
                return jit_op_gt;
            case OpCode::kOpLTI:
                return jit_op_lti;
            case OpCode::kOpGEI:
                return jit_op_gei;
            case OpCode::kOpLEI:
                return jit_op_lei;
            case OpCode::kOpGTI:
                return jit_op_gti;
            case OpCode::kOpLTF:
                return jit_op_ltf;
            case OpCode::kOpGEF:
                return jit_op_gef;
            case OpCode::kOpLEF:
                return jit_op_lef;
            case OpCode::kOpGTF:
                return jit_op_gtf;
            case OpCode::kOpPow:
                return jit_op_pow;
            case OpCode::kOpTest:
                return jit_op_test;
            case OpCode::kOpTestSet:
                return jit_op_testset;
            default:
                return nullptr;
        }
    }

    // A kOpMM* whose fast half the backend compiles natively is unreachable in
    // compiled code: both the fast path and its cold block jump past it. Pow has
    // no native path, so its follower stays live.
    static bool mm_unreachable(OpCode mm, OpCode prev) noexcept
    {
        switch (mm)
        {
            case OpCode::kOpMMAdd:
                return prev == OpCode::kOpAdd;
            case OpCode::kOpMMSub:
                return prev == OpCode::kOpSub;
            case OpCode::kOpMMMul:
                return prev == OpCode::kOpMul;
            case OpCode::kOpMMDiv:
                return prev == OpCode::kOpDiv;
            case OpCode::kOpMMMod:
                return prev == OpCode::kOpMod;
            case OpCode::kOpMMBand:
                return prev == OpCode::kOpBand;
            case OpCode::kOpMMBor:
                return prev == OpCode::kOpBor;
            case OpCode::kOpMMBxor:
                return prev == OpCode::kOpBxor;
            case OpCode::kOpMMShl:
                return prev == OpCode::kOpShl;
            case OpCode::kOpMMShr:
                return prev == OpCode::kOpShr;
            default:
                return false;
        }
    }

    static bool can_fall_off_end(OpCode op) noexcept
    {
        switch (op)
        {
            case OpCode::kOpReturn:
            case OpCode::kOpReturn0:
            case OpCode::kOpReturn1:
            case OpCode::kOpJmp:
            case OpCode::kOpTailCall:
                return false;
            default:
                return true;
        }
    }

    static bool imm_cmp_info(OpCode op, CgCmp& cmp, JitOpFn& fn) noexcept
    {
        switch (op)
        {
            case OpCode::kOpLTImm:
                cmp = CgCmp::kLt;
                fn = jit_op_ltimm;
                return true;
            case OpCode::kOpGeImm:
                cmp = CgCmp::kGe;
                fn = jit_op_geimm;
                return true;
            case OpCode::kOpLEImm:
                cmp = CgCmp::kLe;
                fn = jit_op_leimm;
                return true;
            case OpCode::kOpGtImm:
                cmp = CgCmp::kGt;
                fn = jit_op_gtimm;
                return true;
            case OpCode::kOpEqImm:
                cmp = CgCmp::kEq;
                fn = jit_op_eqimm;
                return true;
            case OpCode::kOpNeImm:
                cmp = CgCmp::kNe;
                fn = jit_op_neimm;
                return true;
            default:
                return false;
        }
    }

    static bool reg_cmp_info(OpCode op, CgCmp& out) noexcept
    {
        switch (op)
        {
            case OpCode::kOpEq:
                out = CgCmp::kEq;
                return true;
            case OpCode::kOpNe:
                out = CgCmp::kNe;
                return true;
            case OpCode::kOpLt:
                out = CgCmp::kLt;
                return true;
            case OpCode::kOpGe:
                out = CgCmp::kGe;
                return true;
            case OpCode::kOpLe:
                out = CgCmp::kLe;
                return true;
            case OpCode::kOpGt:
                out = CgCmp::kGt;
                return true;
            default:
                return false;
        }
    }

    static bool const_int_cmp_info(OpCode op, CgCmp& out) noexcept
    {
        switch (op)
        {
            case OpCode::kOpLTI:
                out = CgCmp::kLt;
                return true;
            case OpCode::kOpGEI:
                out = CgCmp::kGe;
                return true;
            case OpCode::kOpLEI:
                out = CgCmp::kLe;
                return true;
            case OpCode::kOpGTI:
                out = CgCmp::kGt;
                return true;
            default:
                return false;
        }
    }

    static bool const_fp_cmp_info(OpCode op, CgCmp& out) noexcept
    {
        switch (op)
        {
            case OpCode::kOpLTF:
                out = CgCmp::kLt;
                return true;
            case OpCode::kOpGEF:
                out = CgCmp::kGe;
                return true;
            case OpCode::kOpLEF:
                out = CgCmp::kLe;
                return true;
            case OpCode::kOpGTF:
                out = CgCmp::kGt;
                return true;
            default:
                return false;
        }
    }

    static CgCmp invert(CgCmp cmp) noexcept
    {
        switch (cmp)
        {
            case CgCmp::kEq:
                return CgCmp::kNe;
            case CgCmp::kNe:
                return CgCmp::kEq;
            case CgCmp::kLt:
                return CgCmp::kGe;
            case CgCmp::kLe:
                return CgCmp::kGt;
            case CgCmp::kGt:
                return CgCmp::kLe;
            case CgCmp::kGe:
                return CgCmp::kLt;
        }
        return CgCmp::kEq;
    }

    namespace
    {
        class AbstractCompiler
        {
        public:
            explicit AbstractCompiler(const GCProto* proto, CgProgram& out)
                : proto_(proto)
                , out_(out)
                , n_(proto->code.size())
            {
            }

            bool compile();

        private:
            enum class ColdKind : uint8_t
            {
                kIncDec,
                kAddSubImm,
                kCmpImm,
                kForLoop,
                kForPrep,
                kArithF64,
                kHelperOnly,
                kAddSubK,
                kUnmF64,
                kCmpRegs,
                kCmpK,
            };

            struct ColdBlock
            {
                uint32_t entry;
                uint32_t resume;
                JitOpFn fn;
                uint32_t raw;
                uint32_t pcn;
                ColdKind kind;
                int32_t offset;
                int64_t k{};
            };

            CgOp& push(CgOpKind kind)
            {
                out_.ops.push_back(CgOp{});
                CgOp& op = out_.ops.back();
                op.kind = kind;
                return op;
            }

            uint32_t new_label()
            {
                return out_.num_labels++;
            }

            uint32_t new_var()
            {
                return out_.num_vars++;
            }

            bool valid_pc(int64_t pc) const noexcept
            {
                return pc >= 0 && pc < static_cast<int64_t>(n_);
            }

            void bind(uint32_t label, bool is_join)
            {
                CgOp& op = push(CgOpKind::kBind);
                op.label = label;
                op.flag = is_join;
                op.slot = kClobberAll;
            }

            // The split arithmetic opcodes skip their kOpMM* follower on the
            // numeric path, so both the fast path and the cold block resume at
            // the instruction after it.
            bool set_mm_resume(ColdBlock& cb, uint32_t pcn)
            {
                if (!valid_pc(static_cast<int64_t>(pcn) + 1))
                {
                    return false;
                }
                cb.resume = pc_labels_[static_cast<size_t>(pcn) + 1];
                return true;
            }

            void bind_resume(uint32_t label, int32_t clobbered_slot, uint32_t cold_entry)
            {
                CgOp& op = push(CgOpKind::kBind);
                op.label = label;
                op.flag = false;
                op.slot = clobbered_slot;
                op.label2 = cold_entry;
            }

            void jump(uint32_t label)
            {
                push(CgOpKind::kJump).label = label;
            }

            void guard_tag(int32_t slot, Type tag, uint32_t on_fail)
            {
                CgOp& op = push(CgOpKind::kGuardTag);
                op.slot = slot;
                op.tag = static_cast<uint8_t>(tag);
                op.label = on_fail;
            }

            void store_tag(int32_t slot, Type tag)
            {
                CgOp& op = push(CgOpKind::kStoreTag);
                op.slot = slot;
                op.tag = static_cast<uint8_t>(tag);
            }

            uint32_t load(CgOpKind kind, int32_t slot)
            {
                CgOp& op = push(kind);
                op.slot = slot;
                op.var = new_var();
                return op.var;
            }

            uint32_t const_i64(int64_t value)
            {
                CgOp& op = push(CgOpKind::kConstI64);
                op.imm = value;
                op.var = new_var();
                return op.var;
            }

            uint32_t const_f64(double value)
            {
                CgOp& op = push(CgOpKind::kConstF64);
                op.imm = static_cast<int64_t>(std::bit_cast<uint64_t>(value));
                op.var = new_var();
                return op.var;
            }

            void store(CgOpKind kind, int32_t slot, uint32_t var)
            {
                CgOp& op = push(kind);
                op.slot = slot;
                op.var = var;
            }

            void arith(CgOpKind kind, uint32_t dst, uint32_t src)
            {
                CgOp& op = push(kind);
                op.var = dst;
                op.var2 = src;
            }

            void add_i64_imm(uint32_t var, int64_t imm)
            {
                CgOp& op = push(CgOpKind::kAddI64Imm);
                op.var = var;
                op.imm = imm;
            }

            void branch_i64_imm(uint32_t var, int64_t imm, CgCmp cmp, uint32_t on_true)
            {
                CgOp& op = push(CgOpKind::kBranchI64Imm);
                op.var = var;
                op.imm = imm;
                op.cmp = cmp;
                op.label = on_true;
            }

            void branch_f64(uint32_t lhs, uint32_t rhs, CgCmp cmp, uint32_t on_true, uint32_t on_false)
            {
                CgOp& op = push(CgOpKind::kBranchF64);
                op.var = lhs;
                op.var2 = rhs;
                op.cmp = cmp;
                op.label = on_true;
                op.label2 = on_false;
            }

            void branch_i64(uint32_t lhs, uint32_t rhs, CgCmp cmp, uint32_t on_true)
            {
                CgOp& op = push(CgOpKind::kBranchI64);
                op.var = lhs;
                op.var2 = rhs;
                op.cmp = cmp;
                op.label = on_true;
            }

            void branch_truthy(int32_t slot, uint32_t on_truthy, uint32_t on_not)
            {
                CgOp& op = push(CgOpKind::kBranchTruthy);
                op.slot = slot;
                op.label = on_truthy;
                op.label2 = on_not;
            }

            void branch_var_eq_u32(uint32_t var, uint32_t value, uint32_t on_eq)
            {
                CgOp& op = push(CgOpKind::kBranchVarEqU32);
                op.var = var;
                op.imm = value;
                op.label = on_eq;
            }

            uint32_t helper_call(JitOpFn fn, uint32_t raw, uint32_t pcn)
            {
                CgOp& op = push(CgOpKind::kHelperCall);
                op.fn = fn;
                op.raw = raw;
                op.pcn = pcn;
                op.label = err_;
                op.var = new_var();
                return op.var;
            }

            void pc_dispatch(uint32_t result, std::initializer_list<int64_t> candidates);
            bool compile_op(uint32_t& pc, const Instruction& ins);
            void emit_cold_blocks();
            bool collect_jump_targets();

            const GCProto* proto_;
            CgProgram& out_;
            size_t n_;
            std::vector<bool> jump_targets_;
            std::vector<uint32_t> pc_labels_;
            std::vector<ColdBlock> cold_blocks_;
            uint32_t err_{};
            uint32_t ret_stub_{};
            uint32_t tail_stub_{};
            bool failed_{};
        };

        bool AbstractCompiler::collect_jump_targets()
        {
            jump_targets_.assign(n_, false);
            const auto mark = [&](int64_t t) {
                if (valid_pc(t))
                {
                    jump_targets_[static_cast<size_t>(t)] = true;
                }
            };

            for (uint32_t pc = 0; pc < n_; ++pc)
            {
                const Instruction ins = proto_->code[pc];
                const uint32_t pcn = pc + 1;

                switch (ins.op())
                {
                    case OpCode::kOpJmp:
                        mark(static_cast<int64_t>(pcn) + ins.jump_offset());
                        break;
                    case OpCode::kOpForPrep:
                    {
                        const int32_t off = ins.signed_offset();
                        mark(pcn);
                        mark(static_cast<int64_t>(pcn) + off);
                        mark(static_cast<int64_t>(pcn) + off + 1);
                        break;
                    }
                    case OpCode::kOpForLoop:
                    {
                        const int32_t off = ins.signed_offset();
                        mark(static_cast<int64_t>(pcn) + off - 1);
                        mark(pcn);
                        break;
                    }
                    case OpCode::kOpLoadBool:
                        if (ins.skip_next())
                        {
                            mark(static_cast<int64_t>(pc) + 2);
                        }
                        break;
                    case OpCode::kOpTailCall:
                        mark(0);
                        break;
                    case OpCode::kOpClosure:
                    {
                        const uint32_t proto_idx = ins.const_or_proto_index();
                        if (proto_idx >= proto_->protos.size())
                        {
                            return false;
                        }
                        pc += static_cast<uint32_t>(proto_->protos[proto_idx]->upvalue_names.size());
                        break;
                    }
                    default:
                    {
                        CgCmp cmp{};
                        JitOpFn fn = nullptr;
                        if (branch_helper(ins.op()) != nullptr || imm_cmp_info(ins.op(), cmp, fn))
                        {
                            mark(pcn);
                            mark(static_cast<int64_t>(pcn) + 1);
                        }
                        break;
                    }
                }
            }
            return true;
        }

        void AbstractCompiler::pc_dispatch(uint32_t result, std::initializer_list<int64_t> candidates)
        {
            int64_t targets[4];
            size_t count = 0;

            for (const int64_t c : candidates)
            {
                if (!valid_pc(c))
                {
                    failed_ = true;
                    return;
                }

                bool seen = false;
                for (size_t i = 0; i < count; ++i)
                {
                    if (targets[i] == c)
                    {
                        seen = true;
                        break;
                    }
                }
                if (!seen)
                {
                    targets[count++] = c;
                }
            }

            for (size_t i = 0; i + 1 < count; ++i)
            {
                branch_var_eq_u32(result, static_cast<uint32_t>(targets[i]), pc_labels_[static_cast<size_t>(targets[i])]);
            }
            jump(pc_labels_[static_cast<size_t>(targets[count - 1])]);
        }

        bool AbstractCompiler::compile_op(uint32_t& pc, const Instruction& ins)
        {
            const uint32_t pcn = pc + 1;

            if (pc > 0 && !jump_targets_[pc] && mm_unreachable(ins.op(), proto_->code[pc - 1].op()))
            {
                return true;
            }

            switch (ins.op())
            {
                case OpCode::kOpJmp:
                {
                    const int64_t target = static_cast<int64_t>(pcn) + ins.jump_offset();
                    if (!valid_pc(target))
                    {
                        return false;
                    }
                    jump(pc_labels_[static_cast<size_t>(target)]);
                    break;
                }

                case OpCode::kOpMove:
                {
                    CgOp& op = push(CgOpKind::kCopySlot);
                    op.slot = ins.a();
                    op.imm = ins.b();
                    break;
                }

                case OpCode::kOpLoadImm:
                {
                    store_tag(ins.a(), Type::kInteger);
                    const uint32_t v = const_i64(ins.signed_immediate());
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    break;
                }

                case OpCode::kOpIncLocal:
                case OpCode::kOpDecLocal:
                {
                    const bool is_inc = ins.op() == OpCode::kOpIncLocal;
                    ColdBlock cb{ new_label(), new_label(), is_inc ? jit_op_inclocal : jit_op_declocal, ins.raw, pcn,
                        ColdKind::kIncDec, 0 };
                    guard_tag(ins.a(), Type::kInteger, cb.entry);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.a());
                    add_i64_imm(v, is_inc ? 1 : -1);
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    bind_resume(cb.resume, ins.a(), cb.entry);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpAddImm:
                case OpCode::kOpSubImm:
                {
                    const bool is_add = ins.op() == OpCode::kOpAddImm;
                    const int32_t imm = ins.signed_immediate_9bit();
                    ColdBlock cb{ new_label(), new_label(), is_add ? jit_op_addimm : jit_op_subimm, ins.raw, pcn,
                        ColdKind::kAddSubImm, 0 };
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.b());
                    add_i64_imm(v, is_add ? static_cast<int64_t>(imm) : -static_cast<int64_t>(imm));
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    store_tag(ins.a(), Type::kInteger);
                    bind_resume(cb.resume, ins.a(), cb.entry);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpLTImm:
                case OpCode::kOpGeImm:
                case OpCode::kOpLEImm:
                case OpCode::kOpGtImm:
                case OpCode::kOpEqImm:
                case OpCode::kOpNeImm:
                {
                    CgCmp cmp{};
                    JitOpFn fn = nullptr;
                    if (!imm_cmp_info(ins.op(), cmp, fn) || static_cast<size_t>(pcn) + 1 >= n_)
                    {
                        return false;
                    }
                    ColdBlock cb{ new_label(), 0, fn, ins.raw, pcn, ColdKind::kCmpImm, 0 };
                    cb.resume = cb.entry;
                    guard_tag(ins.a(), Type::kInteger, cb.entry);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.a());
                    branch_i64_imm(v, ins.signed_immediate(), invert(cmp), pc_labels_[pcn + 1]);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpLoadI:
                {
                    const uint32_t kidx = ins.const_or_proto_index();
                    if (kidx >= proto_->int_constants.size())
                    {
                        return false;
                    }
                    store_tag(ins.a(), Type::kInteger);
                    const uint32_t v = const_i64(proto_->int_constants[kidx].get_integer());
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    break;
                }

                case OpCode::kOpLoadF:
                {
                    const uint32_t kidx = ins.const_or_proto_index();
                    if (kidx >= proto_->fp_constants.size())
                    {
                        return false;
                    }
                    store_tag(ins.a(), Type::kNumber);
                    const uint32_t v = const_f64(proto_->fp_constants[kidx].get_fp());
                    store(CgOpKind::kStoreF64, ins.a(), v);
                    break;
                }

                case OpCode::kOpLoadS:
                {
                    const uint32_t kidx = ins.const_or_proto_index();
                    if (kidx >= proto_->str_constants.size())
                    {
                        return false;
                    }
                    const auto ptr_bits =
                        static_cast<int64_t>(reinterpret_cast<uintptr_t>(proto_->str_constants[kidx].get_string()));
                    store_tag(ins.a(), Type::kString);
                    const uint32_t v = const_i64(ptr_bits);
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    break;
                }

                case OpCode::kOpLoadNil:
                {
                    const int32_t first = ins.a();
                    const int32_t last = first + ins.b();
                    if (last > 255)
                    {
                        return false;
                    }
                    if (ins.b() < 8)
                    {
                        for (int32_t i = first; i <= last; ++i)
                        {
                            store_tag(i, Type::kNil);
                        }
                    }
                    else
                    {
                        helper_call(jit_op_loadnil, ins.raw, pcn);
                    }
                    break;
                }

                case OpCode::kOpAddKI:
                case OpCode::kOpSubKI:
                {
                    const uint32_t kidx = ins.small_const_index();
                    if (kidx >= proto_->int_constants.size())
                    {
                        return false;
                    }
                    const bool is_add = ins.op() == OpCode::kOpAddKI;
                    const int64_t k = proto_->int_constants[kidx].get_integer();
                    ColdBlock cb{ new_label(), new_label(), is_add ? jit_op_addki : jit_op_subki, ins.raw, pcn,
                        ColdKind::kAddSubK, 0, k };
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.b());
                    const int64_t eff = is_add ? k : static_cast<int64_t>(uint64_t{ 0 } - static_cast<uint64_t>(k));
                    if (eff >= INT32_MIN && eff <= INT32_MAX)
                    {
                        add_i64_imm(v, eff);
                    }
                    else
                    {
                        const uint32_t c = const_i64(eff);
                        arith(CgOpKind::kAddI64, v, c);
                    }
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    store_tag(ins.a(), Type::kInteger);
                    bind_resume(cb.resume, ins.a(), cb.entry);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpAddKF:
                case OpCode::kOpSubKF:
                {
                    const uint32_t kidx = ins.small_const_index();
                    if (kidx >= proto_->fp_constants.size())
                    {
                        return false;
                    }
                    const bool is_add = ins.op() == OpCode::kOpAddKF;
                    const double kd = proto_->fp_constants[kidx].get_fp();
                    ColdBlock cb{ new_label(), new_label(), is_add ? jit_op_addkf : jit_op_subkf, ins.raw, pcn,
                        ColdKind::kAddSubK, 0, static_cast<int64_t>(std::bit_cast<uint64_t>(kd)) };
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    const uint32_t t = load(CgOpKind::kCvtSlotToF64, ins.b());
                    const uint32_t c = const_f64(kd);
                    arith(is_add ? CgOpKind::kAddF64 : CgOpKind::kSubF64, t, c);
                    store(CgOpKind::kStoreF64, ins.a(), t);
                    store_tag(ins.a(), Type::kNumber);
                    bind_resume(cb.resume, ins.a(), cb.entry);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpAdd:
                case OpCode::kOpSub:
                case OpCode::kOpAddLocal:
                {
                    const bool is_addlocal = ins.op() == OpCode::kOpAddLocal;
                    const bool is_sub = ins.op() == OpCode::kOpSub;
                    const uint8_t dst = ins.a();
                    const uint8_t lhs = is_addlocal ? ins.a() : ins.b();
                    const uint8_t rhs = is_addlocal ? ins.b() : ins.c();
                    JitOpFn fn = is_sub ? jit_op_sub : (is_addlocal ? jit_op_addlocal : jit_op_add);
                    const bool has_mm = !is_addlocal;
                    ColdBlock cb{ new_label(), new_label(), fn, ins.raw, pcn, ColdKind::kArithF64, 0 };
                    if (has_mm && !set_mm_resume(cb, pcn))
                    {
                        return false;
                    }
                    guard_tag(lhs, Type::kInteger, cb.entry);
                    guard_tag(rhs, Type::kInteger, cb.entry);
                    const uint32_t v1 = load(CgOpKind::kLoadI64, lhs);
                    const uint32_t v2 = load(CgOpKind::kLoadI64, rhs);
                    arith(is_sub ? CgOpKind::kSubI64 : CgOpKind::kAddI64, v1, v2);
                    store(CgOpKind::kStoreI64, dst, v1);
                    store_tag(dst, Type::kInteger);
                    if (has_mm)
                    {
                        jump(cb.resume);
                    }
                    else
                    {
                        bind_resume(cb.resume, ins.a(), cb.entry);
                    }
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpMul:
                {
                    ColdBlock cb{ new_label(), new_label(), jit_op_mul, ins.raw, pcn, ColdKind::kArithF64, 0 };
                    if (!set_mm_resume(cb, pcn))
                    {
                        return false;
                    }
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    guard_tag(ins.c(), Type::kInteger, cb.entry);
                    const uint32_t v1 = load(CgOpKind::kLoadI64, ins.b());
                    const uint32_t v2 = load(CgOpKind::kLoadI64, ins.c());
                    arith(CgOpKind::kMulI64, v1, v2);
                    store(CgOpKind::kStoreI64, ins.a(), v1);
                    store_tag(ins.a(), Type::kInteger);
                    jump(cb.resume);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpDiv:
                {
                    ColdBlock cb{ new_label(), new_label(), jit_op_div, ins.raw, pcn, ColdKind::kArithF64, 0 };
                    if (!set_mm_resume(cb, pcn))
                    {
                        return false;
                    }
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    guard_tag(ins.c(), Type::kInteger, cb.entry);
                    const uint32_t t1 = load(CgOpKind::kCvtSlotToF64, ins.b());
                    const uint32_t t2 = load(CgOpKind::kCvtSlotToF64, ins.c());
                    arith(CgOpKind::kDivF64, t1, t2);
                    store(CgOpKind::kStoreF64, ins.a(), t1);
                    store_tag(ins.a(), Type::kNumber);
                    jump(cb.resume);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpShl:
                case OpCode::kOpShr:
                {
                    const bool is_shl = ins.op() == OpCode::kOpShl;
                    ColdBlock cb{ new_label(), new_label(), is_shl ? jit_op_shl : jit_op_shr, ins.raw, pcn,
                        ColdKind::kHelperOnly, 0 };
                    if (!set_mm_resume(cb, pcn))
                    {
                        return false;
                    }
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    guard_tag(ins.c(), Type::kInteger, cb.entry);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.b());
                    const uint32_t cnt = load(CgOpKind::kLoadI64, ins.c());
                    arith(is_shl ? CgOpKind::kShlI64 : CgOpKind::kShrI64, v, cnt);
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    store_tag(ins.a(), Type::kInteger);
                    jump(cb.resume);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpBand:
                case OpCode::kOpBor:
                case OpCode::kOpBxor:
                {
                    CgOpKind bop = CgOpKind::kAndI64;
                    JitOpFn fn = jit_op_band;
                    if (ins.op() == OpCode::kOpBor)
                    {
                        bop = CgOpKind::kOrI64;
                        fn = jit_op_bor;
                    }
                    else if (ins.op() == OpCode::kOpBxor)
                    {
                        bop = CgOpKind::kXorI64;
                        fn = jit_op_bxor;
                    }
                    ColdBlock cb{ new_label(), new_label(), fn, ins.raw, pcn, ColdKind::kHelperOnly, 0 };
                    if (!set_mm_resume(cb, pcn))
                    {
                        return false;
                    }
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    guard_tag(ins.c(), Type::kInteger, cb.entry);
                    const uint32_t v1 = load(CgOpKind::kLoadI64, ins.b());
                    const uint32_t v2 = load(CgOpKind::kLoadI64, ins.c());
                    arith(bop, v1, v2);
                    store(CgOpKind::kStoreI64, ins.a(), v1);
                    store_tag(ins.a(), Type::kInteger);
                    jump(cb.resume);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpBnot:
                {
                    ColdBlock cb{ new_label(), new_label(), jit_op_bnot, ins.raw, pcn, ColdKind::kHelperOnly, 0 };
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.b());
                    const uint32_t c = const_i64(-1);
                    arith(CgOpKind::kXorI64, v, c);
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    store_tag(ins.a(), Type::kInteger);
                    bind_resume(cb.resume, ins.a(), cb.entry);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpUnm:
                {
                    ColdBlock cb{ new_label(), new_label(), jit_op_unm, ins.raw, pcn, ColdKind::kUnmF64, 0 };
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    const uint32_t z = const_i64(0);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.b());
                    arith(CgOpKind::kSubI64, z, v);
                    store(CgOpKind::kStoreI64, ins.a(), z);
                    store_tag(ins.a(), Type::kInteger);
                    bind_resume(cb.resume, ins.a(), cb.entry);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpMod:
                {
                    ColdBlock cb{ new_label(), new_label(), jit_op_mod, ins.raw, pcn, ColdKind::kHelperOnly, 0 };
                    if (!set_mm_resume(cb, pcn))
                    {
                        return false;
                    }
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    guard_tag(ins.c(), Type::kInteger, cb.entry);
                    const uint32_t v1 = load(CgOpKind::kLoadI64, ins.b());
                    const uint32_t v2 = load(CgOpKind::kLoadI64, ins.c());
                    CgOp& mod_op = push(CgOpKind::kModI64);
                    mod_op.var = v1;
                    mod_op.var2 = v2;
                    mod_op.label = cb.entry;
                    store(CgOpKind::kStoreI64, ins.a(), v1);
                    store_tag(ins.a(), Type::kInteger);
                    jump(cb.resume);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpTest:
                {
                    if (static_cast<size_t>(pcn) + 1 >= n_)
                    {
                        return false;
                    }
                    const bool inv = ins.b() != 0;
                    const uint32_t taken = pc_labels_[pcn];
                    const uint32_t skip = pc_labels_[pcn + 1];
                    branch_truthy(ins.a(), inv ? skip : taken, inv ? taken : skip);
                    break;
                }

                case OpCode::kOpTestSet:
                {
                    if (static_cast<size_t>(pcn) + 1 >= n_)
                    {
                        return false;
                    }
                    const bool inv = ins.c() != 0;
                    const uint32_t copy_label = new_label();
                    const uint32_t skip = pc_labels_[pcn + 1];
                    branch_truthy(ins.b(), inv ? skip : copy_label, inv ? copy_label : skip);
                    bind(copy_label, false);
                    CgOp& cp = push(CgOpKind::kCopySlot);
                    cp.slot = ins.a();
                    cp.imm = ins.b();
                    jump(pc_labels_[pcn]);
                    break;
                }

                case OpCode::kOpEq:
                case OpCode::kOpNe:
                case OpCode::kOpLt:
                case OpCode::kOpGe:
                case OpCode::kOpLe:
                case OpCode::kOpGt:
                {
                    CgCmp cmp{};
                    if (!reg_cmp_info(ins.op(), cmp) || static_cast<size_t>(pcn) + 1 >= n_)
                    {
                        return false;
                    }
                    ColdBlock cb{ new_label(), 0, branch_helper(ins.op()), ins.raw, pcn, ColdKind::kCmpRegs, 0 };
                    cb.resume = cb.entry;
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    guard_tag(ins.c(), Type::kInteger, cb.entry);
                    const uint32_t v1 = load(CgOpKind::kLoadI64, ins.b());
                    const uint32_t v2 = load(CgOpKind::kLoadI64, ins.c());
                    branch_i64(v1, v2, invert(cmp), pc_labels_[pcn + 1]);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpLTI:
                case OpCode::kOpGEI:
                case OpCode::kOpLEI:
                case OpCode::kOpGTI:
                {
                    CgCmp cmp{};
                    const uint32_t kidx = ins.small_const_index();
                    if (!const_int_cmp_info(ins.op(), cmp) || static_cast<size_t>(pcn) + 1 >= n_
                        || kidx >= proto_->int_constants.size())
                    {
                        return false;
                    }
                    const int64_t k = proto_->int_constants[kidx].get_integer();
                    ColdBlock cb{ new_label(), 0, branch_helper(ins.op()), ins.raw, pcn, ColdKind::kCmpK, 0, k };
                    cb.resume = cb.entry;
                    guard_tag(ins.b(), Type::kInteger, cb.entry);
                    const uint32_t v = load(CgOpKind::kLoadI64, ins.b());
                    if (k >= INT32_MIN && k <= INT32_MAX)
                    {
                        branch_i64_imm(v, k, invert(cmp), pc_labels_[pcn + 1]);
                    }
                    else
                    {
                        const uint32_t c = const_i64(k);
                        branch_i64(v, c, invert(cmp), pc_labels_[pcn + 1]);
                    }
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpLTF:
                case OpCode::kOpGEF:
                case OpCode::kOpLEF:
                case OpCode::kOpGTF:
                {
                    CgCmp cmp{};
                    const uint32_t kidx = ins.small_const_index();
                    if (!const_fp_cmp_info(ins.op(), cmp) || static_cast<size_t>(pcn) + 1 >= n_
                        || kidx >= proto_->fp_constants.size())
                    {
                        return false;
                    }
                    const double kd = proto_->fp_constants[kidx].get_fp();
                    ColdBlock cb{ new_label(), 0, branch_helper(ins.op()), ins.raw, pcn, ColdKind::kCmpK, 0,
                        static_cast<int64_t>(std::bit_cast<uint64_t>(kd)) };
                    cb.resume = cb.entry;
                    guard_tag(ins.b(), Type::kNumber, cb.entry);
                    const uint32_t f = load(CgOpKind::kLoadF64, ins.b());
                    const uint32_t c = const_f64(kd);
                    branch_f64(f, c, cmp, pc_labels_[pcn], pc_labels_[pcn + 1]);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpCall:
                    helper_call(jit_op_call, ins.raw, pcn);
                    break;

                case OpCode::kOpClosure:
                {
                    const uint32_t proto_idx = ins.const_or_proto_index();
                    if (proto_idx >= proto_->protos.size())
                    {
                        return false;
                    }
                    const GCProto* nested = proto_->protos[proto_idx];
                    const auto num_captures = static_cast<uint32_t>(nested->upvalue_names.size());
                    if (static_cast<size_t>(pc) + 1 + num_captures > n_)
                    {
                        return false;
                    }

                    helper_call(jit_op_closure, ins.raw, pcn);

                    for (uint32_t j = 1; j <= num_captures; ++j)
                    {
                        bind(pc_labels_[pc + j], false);
                    }
                    pc += num_captures;
                    break;
                }

                case OpCode::kOpTailCall:
                {
                    const uint32_t r = helper_call(jit_op_tailcall, ins.raw, pcn);
                    branch_var_eq_u32(r, kJitTailReturned, ret_stub_);
                    branch_var_eq_u32(r, kJitTailReplaced, tail_stub_);
                    jump(pc_labels_[0]);
                    break;
                }

                case OpCode::kOpReturn:
                    helper_call(jit_op_return, ins.raw, pcn);
                    jump(ret_stub_);
                    break;

                case OpCode::kOpReturn0:
                    helper_call(jit_op_return0, ins.raw, pcn);
                    jump(ret_stub_);
                    break;

                case OpCode::kOpReturn1:
                    helper_call(jit_op_return1, ins.raw, pcn);
                    jump(ret_stub_);
                    break;

                case OpCode::kOpLoadBool:
                {
                    store_tag(ins.a(), Type::kBoolean);
                    const uint32_t v = const_i64(ins.bool_value() ? 1 : 0);
                    store(CgOpKind::kStoreI64, ins.a(), v);
                    if (ins.skip_next())
                    {
                        if (!valid_pc(static_cast<int64_t>(pc) + 2))
                        {
                            return false;
                        }
                        jump(pc_labels_[pc + 2]);
                    }
                    break;
                }

                case OpCode::kOpForPrep:
                {
                    const int32_t off = ins.signed_offset();
                    const int64_t exit_pc = static_cast<int64_t>(pcn) + off + 1;
                    if (!valid_pc(static_cast<int64_t>(pcn)) || !valid_pc(static_cast<int64_t>(pcn) + off) || !valid_pc(exit_pc))
                    {
                        return false;
                    }

                    const int32_t a = ins.a();
                    ColdBlock cb{ new_label(), 0, jit_op_forprep, ins.raw, pcn, ColdKind::kForPrep, off };
                    cb.resume = cb.entry;

                    guard_tag(a, Type::kInteger, cb.entry);
                    guard_tag(a + 1, Type::kInteger, cb.entry);
                    guard_tag(a + 2, Type::kInteger, cb.entry);

                    const uint32_t neg_step = new_label();
                    const uint32_t zero_step = new_label();
                    const uint32_t zero_trip = new_label();
                    const uint32_t prepared = new_label();

                    {
                        const uint32_t s = load(CgOpKind::kLoadI64, a + 2);
                        branch_i64_imm(s, 0, CgCmp::kEq, zero_step);
                    }
                    {
                        const uint32_t s = load(CgOpKind::kLoadI64, a + 2);
                        branch_i64_imm(s, 0, CgCmp::kLt, neg_step);
                    }

                    {
                        const uint32_t i = load(CgOpKind::kLoadI64, a);
                        const uint32_t l = load(CgOpKind::kLoadI64, a + 1);
                        branch_i64(i, l, CgCmp::kGt, zero_trip);
                    }
                    {
                        const uint32_t t = load(CgOpKind::kLoadI64, a + 1);
                        const uint32_t i = load(CgOpKind::kLoadI64, a);
                        arith(CgOpKind::kSubI64, t, i);
                        const uint32_t s = load(CgOpKind::kLoadI64, a + 2);
                        arith(CgOpKind::kDivU64, t, s);
                        store(CgOpKind::kStoreI64, a + 3, t);
                        store_tag(a + 3, Type::kInteger);
                        jump(prepared);
                    }

                    bind(neg_step, false);
                    {
                        const uint32_t i = load(CgOpKind::kLoadI64, a);
                        const uint32_t l = load(CgOpKind::kLoadI64, a + 1);
                        branch_i64(i, l, CgCmp::kLt, zero_trip);
                    }
                    {
                        const uint32_t t = load(CgOpKind::kLoadI64, a);
                        const uint32_t l = load(CgOpKind::kLoadI64, a + 1);
                        arith(CgOpKind::kSubI64, t, l);
                        const uint32_t d = const_i64(0);
                        const uint32_t s = load(CgOpKind::kLoadI64, a + 2);
                        arith(CgOpKind::kSubI64, d, s);
                        arith(CgOpKind::kDivU64, t, d);
                        store(CgOpKind::kStoreI64, a + 3, t);
                        store_tag(a + 3, Type::kInteger);
                        jump(prepared);
                    }

                    bind(zero_step, false);
                    {
                        const uint32_t v = const_i64(-1);
                        store(CgOpKind::kStoreI64, a + 3, v);
                        store_tag(a + 3, Type::kInteger);
                        jump(prepared);
                    }

                    bind(zero_trip, false);
                    jump(pc_labels_[static_cast<size_t>(exit_pc)]);

                    bind(prepared, false);
                    cold_blocks_.push_back(cb);
                    break;
                }

                case OpCode::kOpForLoop:
                {
                    const int32_t off = ins.signed_offset();
                    const int64_t loop_target = static_cast<int64_t>(pcn) + off - 1;
                    if (!valid_pc(loop_target) || static_cast<size_t>(pcn) >= n_)
                    {
                        return false;
                    }
                    const int32_t a = ins.a();
                    ColdBlock cb{ new_label(), 0, jit_op_forloop, ins.raw, pcn, ColdKind::kForLoop, off };
                    cb.resume = cb.entry;
                    guard_tag(a + 2, Type::kInteger, cb.entry);
                    const uint32_t step = load(CgOpKind::kLoadI64, a + 2);
                    const uint32_t idx = load(CgOpKind::kLoadI64, a);
                    arith(CgOpKind::kAddI64, idx, step);
                    store(CgOpKind::kStoreI64, a, idx);
                    const uint32_t count = load(CgOpKind::kLoadI64, a + 3);
                    branch_i64_imm(count, 0, CgCmp::kEq, pc_labels_[pcn]);
                    add_i64_imm(count, -1);
                    store(CgOpKind::kStoreI64, a + 3, count);
                    jump(pc_labels_[static_cast<size_t>(loop_target)]);
                    cold_blocks_.push_back(cb);
                    break;
                }

                default:
                {
                    if (JitOpFn branch_fn = branch_helper(ins.op()); branch_fn != nullptr)
                    {
                        const uint32_t r = helper_call(branch_fn, ins.raw, pcn);
                        pc_dispatch(r, { static_cast<int64_t>(pcn), static_cast<int64_t>(pcn) + 1 });
                        break;
                    }

                    JitOpFn fn = plain_helper(ins.op());
                    if (fn == nullptr)
                    {
                        return false;
                    }
                    helper_call(fn, ins.raw, pcn);
                    break;
                }
            }

            return !failed_;
        }

        void AbstractCompiler::emit_cold_blocks()
        {
            for (const ColdBlock& cb : cold_blocks_)
            {
                const Instruction cins{ cb.raw };
                bind(cb.entry, true);

                switch (cb.kind)
                {
                    case ColdKind::kIncDec:
                    {
                        const uint32_t help = new_label();
                        const bool is_inc = cins.op() == OpCode::kOpIncLocal;
                        guard_tag(cins.a(), Type::kNumber, help);
                        const uint32_t v = load(CgOpKind::kLoadF64, cins.a());
                        const uint32_t one = const_f64(1.0);
                        arith(is_inc ? CgOpKind::kAddF64 : CgOpKind::kSubF64, v, one);
                        store(CgOpKind::kStoreF64, cins.a(), v);
                        jump(cb.resume);

                        bind(help, true);
                        helper_call(cb.fn, cb.raw, cb.pcn);
                        push(CgOpKind::kSyncFrame);
                        jump(cb.resume);
                        break;
                    }

                    case ColdKind::kAddSubImm:
                    {
                        const uint32_t help = new_label();
                        const bool is_add = cins.op() == OpCode::kOpAddImm;
                        guard_tag(cins.b(), Type::kNumber, help);
                        const uint32_t v = load(CgOpKind::kLoadF64, cins.b());
                        const uint32_t rhs = const_f64(static_cast<double>(cins.signed_immediate_9bit()));
                        arith(is_add ? CgOpKind::kAddF64 : CgOpKind::kSubF64, v, rhs);
                        store(CgOpKind::kStoreF64, cins.a(), v);
                        store_tag(cins.a(), Type::kNumber);
                        jump(cb.resume);

                        bind(help, true);
                        helper_call(cb.fn, cb.raw, cb.pcn);
                        push(CgOpKind::kSyncFrame);
                        jump(cb.resume);
                        break;
                    }

                    case ColdKind::kCmpImm:
                    {
                        const uint32_t help = new_label();
                        CgCmp cmp{};
                        JitOpFn fn = nullptr;
                        if (imm_cmp_info(cins.op(), cmp, fn) && static_cast<size_t>(cb.pcn) + 1 < n_)
                        {
                            guard_tag(cins.a(), Type::kNumber, help);
                            const uint32_t lhs = load(CgOpKind::kLoadF64, cins.a());
                            const uint32_t rhs = const_f64(static_cast<double>(cins.signed_immediate()));
                            branch_f64(lhs, rhs, cmp, pc_labels_[cb.pcn], pc_labels_[cb.pcn + 1]);
                        }
                        bind(help, true);
                        const uint32_t r = helper_call(cb.fn, cb.raw, cb.pcn);
                        pc_dispatch(r, { static_cast<int64_t>(cb.pcn), static_cast<int64_t>(cb.pcn) + 1 });
                        break;
                    }

                    case ColdKind::kForPrep:
                    {
                        const uint32_t r = helper_call(cb.fn, cb.raw, cb.pcn);
                        pc_dispatch(r,
                            { static_cast<int64_t>(cb.pcn), static_cast<int64_t>(cb.pcn) + cb.offset,
                                static_cast<int64_t>(cb.pcn) + cb.offset + 1 });
                        break;
                    }

                    case ColdKind::kForLoop:
                    {
                        const uint32_t r = helper_call(cb.fn, cb.raw, cb.pcn);
                        pc_dispatch(r, { static_cast<int64_t>(cb.pcn) + cb.offset - 1, static_cast<int64_t>(cb.pcn) });
                        break;
                    }

                    case ColdKind::kArithF64:
                    {
                        const uint32_t help = new_label();
                        const bool is_addlocal = cins.op() == OpCode::kOpAddLocal;
                        const uint8_t lhs_slot = is_addlocal ? cins.a() : cins.b();
                        const uint8_t rhs_slot = is_addlocal ? cins.b() : cins.c();
                        CgOpKind fop = CgOpKind::kAddF64;
                        switch (cins.op())
                        {
                            case OpCode::kOpSub:
                                fop = CgOpKind::kSubF64;
                                break;
                            case OpCode::kOpMul:
                                fop = CgOpKind::kMulF64;
                                break;
                            case OpCode::kOpDiv:
                                fop = CgOpKind::kDivF64;
                                break;
                            default:
                                break;
                        }
                        guard_tag(lhs_slot, Type::kNumber, help);
                        guard_tag(rhs_slot, Type::kNumber, help);
                        const uint32_t lhs = load(CgOpKind::kLoadF64, lhs_slot);
                        const uint32_t rhs = load(CgOpKind::kLoadF64, rhs_slot);
                        arith(fop, lhs, rhs);
                        store(CgOpKind::kStoreF64, cins.a(), lhs);
                        store_tag(cins.a(), Type::kNumber);
                        jump(cb.resume);

                        bind(help, true);
                        helper_call(cb.fn, cb.raw, cb.pcn);
                        push(CgOpKind::kSyncFrame);
                        jump(cb.resume);
                        break;
                    }

                    case ColdKind::kHelperOnly:
                        helper_call(cb.fn, cb.raw, cb.pcn);
                        push(CgOpKind::kSyncFrame);
                        jump(cb.resume);
                        break;

                    case ColdKind::kAddSubK:
                    {
                        const uint32_t help = new_label();
                        const bool is_add = cins.op() == OpCode::kOpAddKI || cins.op() == OpCode::kOpAddKF;
                        const bool is_int_k = cins.op() == OpCode::kOpAddKI || cins.op() == OpCode::kOpSubKI;
                        const double kd = is_int_k ? static_cast<double>(cb.k)
                                                   : std::bit_cast<double>(static_cast<uint64_t>(cb.k));
                        guard_tag(cins.b(), Type::kNumber, help);
                        const uint32_t lhs = load(CgOpKind::kLoadF64, cins.b());
                        const uint32_t rhs = const_f64(kd);
                        arith(is_add ? CgOpKind::kAddF64 : CgOpKind::kSubF64, lhs, rhs);
                        store(CgOpKind::kStoreF64, cins.a(), lhs);
                        store_tag(cins.a(), Type::kNumber);
                        jump(cb.resume);

                        bind(help, true);
                        helper_call(cb.fn, cb.raw, cb.pcn);
                        push(CgOpKind::kSyncFrame);
                        jump(cb.resume);
                        break;
                    }

                    case ColdKind::kUnmF64:
                    {
                        const uint32_t help = new_label();
                        guard_tag(cins.b(), Type::kNumber, help);
                        const uint32_t v = load(CgOpKind::kLoadI64, cins.b());
                        const uint32_t c = const_i64(INT64_MIN);
                        arith(CgOpKind::kXorI64, v, c);
                        store(CgOpKind::kStoreI64, cins.a(), v);
                        store_tag(cins.a(), Type::kNumber);
                        jump(cb.resume);

                        bind(help, true);
                        helper_call(cb.fn, cb.raw, cb.pcn);
                        push(CgOpKind::kSyncFrame);
                        jump(cb.resume);
                        break;
                    }

                    case ColdKind::kCmpRegs:
                    {
                        const uint32_t help = new_label();
                        CgCmp cmp{};
                        if (reg_cmp_info(cins.op(), cmp) && static_cast<size_t>(cb.pcn) + 1 < n_)
                        {
                            guard_tag(cins.b(), Type::kNumber, help);
                            guard_tag(cins.c(), Type::kNumber, help);
                            const uint32_t f1 = load(CgOpKind::kLoadF64, cins.b());
                            const uint32_t f2 = load(CgOpKind::kLoadF64, cins.c());
                            branch_f64(f1, f2, cmp, pc_labels_[cb.pcn], pc_labels_[cb.pcn + 1]);
                        }
                        bind(help, true);
                        const uint32_t r = helper_call(cb.fn, cb.raw, cb.pcn);
                        pc_dispatch(r, { static_cast<int64_t>(cb.pcn), static_cast<int64_t>(cb.pcn) + 1 });
                        break;
                    }

                    case ColdKind::kCmpK:
                    {
                        const uint32_t help = new_label();
                        CgCmp cmp{};
                        if (static_cast<size_t>(cb.pcn) + 1 < n_)
                        {
                            if (const_int_cmp_info(cins.op(), cmp))
                            {
                                guard_tag(cins.b(), Type::kNumber, help);
                                const uint32_t f = load(CgOpKind::kLoadF64, cins.b());
                                const uint32_t c = const_f64(static_cast<double>(cb.k));
                                branch_f64(f, c, cmp, pc_labels_[cb.pcn], pc_labels_[cb.pcn + 1]);
                            }
                            else if (const_fp_cmp_info(cins.op(), cmp))
                            {
                                guard_tag(cins.b(), Type::kInteger, help);
                                const uint32_t t = load(CgOpKind::kCvtSlotToF64, cins.b());
                                const uint32_t c = const_f64(std::bit_cast<double>(static_cast<uint64_t>(cb.k)));
                                branch_f64(t, c, cmp, pc_labels_[cb.pcn], pc_labels_[cb.pcn + 1]);
                            }
                        }
                        bind(help, true);
                        const uint32_t r = helper_call(cb.fn, cb.raw, cb.pcn);
                        pc_dispatch(r, { static_cast<int64_t>(cb.pcn), static_cast<int64_t>(cb.pcn) + 1 });
                        break;
                    }
                }

                if (failed_)
                {
                    return;
                }
            }
        }

        bool AbstractCompiler::compile()
        {
            if (proto_->is_vararg || n_ == 0)
            {
                return false;
            }

            for (size_t i = 0; i < n_; ++i)
            {
                switch (proto_->code[i].op())
                {
                    case OpCode::kOpVararg:
                    case OpCode::kOpVarargPrep:
                    case OpCode::kOpVarargExpand:
                        return false;
                    default:
                        break;
                }
            }

            {
                const OpCode last_op = proto_->code[n_ - 1].op();
                CgCmp cmp{};
                JitOpFn fn = nullptr;
                switch (last_op)
                {
                    case OpCode::kOpForPrep:
                    case OpCode::kOpForLoop:
                        break;
                    default:
                        if (can_fall_off_end(last_op) && branch_helper(last_op) == nullptr && !imm_cmp_info(last_op, cmp, fn))
                        {
                            return false;
                        }
                        break;
                }
            }

            if (!collect_jump_targets())
            {
                return false;
            }

            pc_labels_.reserve(n_);
            for (size_t i = 0; i < n_; ++i)
            {
                pc_labels_.push_back(new_label());
            }
            err_ = new_label();
            ret_stub_ = new_label();
            tail_stub_ = new_label();

            for (uint32_t pc = 0; pc < n_; ++pc)
            {
                bind(pc_labels_[pc], jump_targets_[pc]);
                const Instruction ins = proto_->code[pc];
                if (!compile_op(pc, ins))
                {
                    return false;
                }
            }

            emit_cold_blocks();
            if (failed_)
            {
                return false;
            }

            bind(err_, true);
            push(CgOpKind::kReturnResult).imm = kJitResultError;
            bind(ret_stub_, true);
            push(CgOpKind::kReturnResult).imm = kJitResultOk;
            bind(tail_stub_, true);
            push(CgOpKind::kReturnResult).imm = kJitResultTailCall;

            return true;
        }

    } // namespace

    bool jit_compile_proto(const GCProto* proto, CgProgram& out)
    {
        AbstractCompiler compiler(proto, out);
        if (!compiler.compile())
        {
            return false;
        }

        out.allow_slot_cache = proto->protos.empty();
        return true;
    }

} // namespace behl
