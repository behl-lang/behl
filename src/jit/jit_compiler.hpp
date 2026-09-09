#pragma once

#include "jit/jit.hpp"
#include "jit/jit_helpers.hpp"
#include "vm/bytecode.hpp"

#include <behl/types.hpp>
#include <cstdint>
#include <vector>

namespace behl
{
    enum class CgCmp : uint8_t
    {
        kEq,
        kNe,
        kLt,
        kLe,
        kGt,
        kGe,
    };

    enum class CgOpKind : uint8_t
    {
        kBind,
        kJump,
        kGuardTag,
        kCopySlot,
        kStoreTag,
        kLoadI64,
        kLoadF64,
        kConstI64,
        kConstF64,
        kStoreI64,
        kStoreF64,
        kAddI64,
        kSubI64,
        kAddI64Imm,
        kMulI64,
        kModI64,
        kDivU64,
        kShlI64,
        kShrI64,
        kAndI64,
        kOrI64,
        kXorI64,
        kAddF64,
        kSubF64,
        kMulF64,
        kDivF64,
        kCvtSlotToF64,
        kBranchI64Imm,
        kBranchI64,
        kBranchF64,
        kBranchTruthy,
        kBranchVarEqU32,
        kHelperCall,
        kSyncFrame,
        kReturnResult,
    };

    struct CgOp
    {
        CgOpKind kind;
        uint8_t tag;
        CgCmp cmp;
        bool flag;
        uint32_t label;
        uint32_t label2;
        uint32_t var;
        uint32_t var2;
        int32_t slot;
        int64_t imm;
        JitOpFn fn;
        uint32_t raw;
        uint32_t pcn;
    };

    constexpr int32_t kClobberAll = -2;
    constexpr int32_t kClobberNone = -1;

    struct CgProgram
    {
        std::vector<CgOp> ops;
        uint32_t num_labels{};
        uint32_t num_vars{};
        bool allow_slot_cache{};
    };

    bool jit_compile_proto(const GCProto* proto, CgProgram& out);

} // namespace behl
