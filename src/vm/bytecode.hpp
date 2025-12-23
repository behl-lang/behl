#pragma once

#include "common/string.hpp"
#include "config_internal.hpp"
#include "platform.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace behl
{
    using Reg = uint8_t;
    using ConstIndex = uint32_t;
    using ProtoIndex = uint32_t;

    enum class OpCode : uint8_t
    {
        kOpCall,
        kOpReturn,
        kOpJmp,
        kOpBand,
        kOpBnot,
        kOpBor,
        kOpBxor,
        kOpClosure,
        kOpDecGlobal,
        kOpDecLocal,
        kOpDecUpvalue,
        kOpDiv,
        kOpForLoop,
        kOpForPrep,
        kOpIncGlobal,
        kOpIncLocal,
        kOpIncUpvalue,
        kOpAdd,
        kOpAddImm,
        kOpAddKF,
        kOpAddKI,
        kOpAddLocal,
        kOpSub,
        kOpSubImm,
        kOpSubKF,
        kOpSubKI,
        kOpEqImm,
        kOpGEF,
        kOpGEI,
        kOpGTF,
        kOpGTI,
        kOpEq,
        kOpGe,
        kOpGeImm,
        kOpGt,
        kOpGtImm,
        kOpLEF,
        kOpLEI,
        kOpLEImm,
        kOpLTF,
        kOpLTI,
        kOpLTImm,
        kOpLe,
        kOpLt,
        kOpNeImm,
        kOpLen,
        kOpLoadBool,
        kOpLoadF,
        kOpLoadI,
        kOpLoadImm,
        kOpLoadNil,
        kOpLoadS,
        kOpMod,
        kOpMove,
        kOpMul,
        kOpNe,
        kOpNewTable,
        kOpPow,
        kOpSelf,
        kOpGetField,
        kOpGetFieldI,
        kOpGetFieldS,
        kOpGetGlobal,
        kOpGetUpval,
        kOpSetField,
        kOpSetFieldI,
        kOpSetFieldS,
        kOpSetGlobal,
        kOpSetList,
        kOpSetUpval,
        kOpShl,
        kOpShr,
        kOpTailCall,
        kOpTest,
        kOpTestSet,
        kOpToNumber,
        kOpToString,
        kOpUnm,
        kOpVararg,
        kOpVarargPrep,
        kOpVarargExpand,
    };

    // Total number of opcodes - computed from last enum value
    static constexpr auto kOpCount = static_cast<size_t>(OpCode::kOpVarargExpand) + 1;

    struct Instruction
    {
        uint32_t raw;

        BEHL_FORCEINLINE
        constexpr OpCode op() const noexcept
        {
            return static_cast<OpCode>((raw >> 25) & 0x7F);
        }

        BEHL_FORCEINLINE
        constexpr Reg a() const noexcept
        {
            return raw & 0xFF;
        }

        BEHL_FORCEINLINE
        constexpr Reg b() const noexcept
        {
            return (raw >> 8) & 0xFF;
        }

        BEHL_FORCEINLINE
        constexpr Reg b_at_8() const noexcept
        {
            return (raw >> 8) & 0xFF;
        }

        BEHL_FORCEINLINE
        constexpr bool invert_flag() const noexcept
        {
            return raw & 1;
        }

        BEHL_FORCEINLINE
        constexpr Reg c() const noexcept
        {
            return (raw >> 16) & 0xFF;
        }

        BEHL_FORCEINLINE
        constexpr bool flag_bit() const noexcept
        {
            return (raw >> 24) & 1;
        }

        BEHL_FORCEINLINE
        constexpr bool bool_value() const noexcept
        {
            return (raw >> 8) & 1;
        }

        BEHL_FORCEINLINE
        constexpr bool skip_next() const noexcept
        {
            return (raw >> 9) & 1;
        }

        BEHL_FORCEINLINE
        constexpr ConstIndex const_or_proto_index() const noexcept
        {
            return static_cast<ConstIndex>((raw >> 8) & 0x1FFFF);
        }

        BEHL_FORCEINLINE
        constexpr ConstIndex small_const_index() const noexcept
        {
            return static_cast<ConstIndex>((raw >> 16) & 0x1FF);
        }

        BEHL_FORCEINLINE
        constexpr int32_t signed_offset() const noexcept
        {
            return static_cast<int32_t>((raw >> 8) & 0x1FFFF) - 65536;
        }

        BEHL_FORCEINLINE
        constexpr int32_t signed_immediate() const noexcept
        {
            return static_cast<int32_t>((raw >> 8) & 0x1FFFF) - 65536;
        }

        BEHL_FORCEINLINE
        constexpr int32_t signed_immediate_9bit() const noexcept
        {
            return static_cast<int32_t>((raw >> 16) & 0x1FF) - 256;
        }

        BEHL_FORCEINLINE
        constexpr ConstIndex large_const_index() const noexcept
        {
            return raw & 0x1FFFFFF;
        }

        BEHL_FORCEINLINE
        constexpr int32_t jump_offset() const noexcept
        {
            return static_cast<int32_t>(raw & 0x1FFFFFF) - 8388608;
        }
    };

    constexpr Instruction make_op_move(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpMove) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_loads(Reg a, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLoadS) << 25) | static_cast<uint32_t>(a) | (k << 8);
        return i;
    }

    constexpr Instruction make_op_loadi(Reg a, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLoadI) << 25) | static_cast<uint32_t>(a) | (k << 8);
        return i;
    }

    constexpr Instruction make_op_loadf(Reg a, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLoadF) << 25) | static_cast<uint32_t>(a) | (k << 8);
        return i;
    }

    constexpr Instruction make_op_loadbool(Reg a, bool b, bool skip) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLoadBool) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(skip) << 9);
        return i;
    }

    constexpr Instruction make_op_loadnil(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLoadNil) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_loadimm(Reg a, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 65536);
        i.raw = (static_cast<uint32_t>(OpCode::kOpLoadImm) << 25) | static_cast<uint32_t>(a) | (encoded_imm << 8);
        return i;
    }

    constexpr Instruction make_op_getupval(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGetUpval) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_getglobal(Reg a, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGetGlobal) << 25) | static_cast<uint32_t>(a) | (k << 8);
        return i;
    }

    constexpr Instruction make_op_getfield(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i;
        i.raw = (static_cast<uint32_t>(OpCode::kOpGetField) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }
    constexpr Instruction make_op_setfieldi(Reg a, Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        assert(imm >= 0 && imm <= 511 && "SetFieldI immediate out of 9-bit unsigned range");
        const uint32_t imm_bits = static_cast<uint32_t>(imm & 0x1FF) << 16;
        i.raw = (static_cast<uint32_t>(OpCode::kOpSetFieldI) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | imm_bits;
        return i;
    }

    constexpr Instruction make_op_setfields(Reg a, Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        assert(k <= 511 && "SetFieldS constant index out of 9-bit unsigned range");
        i.raw = (static_cast<uint32_t>(OpCode::kOpSetFieldS) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (k << 16);
        return i;
    }
    constexpr Instruction make_op_getfieldi(Reg a, Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        assert(imm >= 0 && imm <= 511 && "GetFieldI immediate out of 9-bit unsigned range");
        const uint32_t imm_bits = static_cast<uint32_t>(imm & 0x1FF) << 16;
        i.raw = (static_cast<uint32_t>(OpCode::kOpGetFieldI) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | imm_bits;
        return i;
    }

    constexpr Instruction make_op_getfields(Reg a, Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        assert(k <= 511 && "GetFieldS constant index out of 9-bit unsigned range");
        i.raw = (static_cast<uint32_t>(OpCode::kOpGetFieldS) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (k << 16);
        return i;
    }

    constexpr Instruction make_op_setupval(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSetUpval) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_setglobal(Reg a, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSetGlobal) << 25) | static_cast<uint32_t>(a) | (k << 8);
        return i;
    }

    constexpr Instruction make_op_setfield(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSetField) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_newtable(Reg a, uint8_t array_size, uint8_t hash_size) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpNewTable) << 25) | static_cast<uint32_t>(a)
            | (static_cast<uint32_t>(array_size) << 8) | (static_cast<uint32_t>(hash_size) << 16);
        return i;
    }

    constexpr Instruction make_op_setlist(Reg a, uint8_t num_fields, uint8_t extra) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSetList) << 25) | static_cast<uint32_t>(a)
            | (static_cast<uint32_t>(num_fields) << 8) | (static_cast<uint32_t>(extra) << 16);
        return i;
    }

    constexpr Instruction make_op_self(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSelf) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_add(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpAdd) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_sub(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSub) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_mul(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpMul) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_div(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpDiv) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_mod(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpMod) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_pow(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpPow) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_band(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpBand) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_bor(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpBor) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_bxor(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpBxor) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_shl(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpShl) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_shr(Reg a, Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpShr) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_unm(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpUnm) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_bnot(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpBnot) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_len(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLen) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_tostring(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpToString) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_tonumber(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpToNumber) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_jmp(int32_t offset) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpJmp) << 25) | (static_cast<uint32_t>(offset + 8388608) & 0x1FFFFFF);
        return i;
    }

    constexpr Instruction make_op_closure(Reg a, ProtoIndex proto_idx) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpClosure) << 25) | static_cast<uint32_t>(a) | (proto_idx << 8);
        return i;
    }

    constexpr Instruction make_op_test(Reg a, bool invert) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpTest) << 25) | static_cast<uint32_t>(a)
            | (static_cast<uint32_t>(invert) << 8);
        return i;
    }

    constexpr Instruction make_op_call(Reg a, uint8_t num_args, uint8_t num_results, bool is_self_call) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpCall) << 25) | static_cast<uint32_t>(a)
            | (static_cast<uint32_t>(num_args) << 8) | (static_cast<uint32_t>(num_results) << 16)
            | (static_cast<uint32_t>(is_self_call) << 24);
        return i;
    }

    constexpr Instruction make_op_tailcall(Reg a, uint8_t num_args, bool is_self_call) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpTailCall) << 25) | static_cast<uint32_t>(a)
            | (static_cast<uint32_t>(num_args) << 8) | (static_cast<uint32_t>(is_self_call) << 16);
        return i;
    }

    constexpr Instruction make_op_return(Reg a, uint8_t num_results) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpReturn) << 25) | static_cast<uint32_t>(a)
            | (static_cast<uint32_t>(num_results) << 8);
        return i;
    }

    constexpr Instruction make_op_forprep(Reg a, int32_t offset) noexcept
    {
        uint32_t encoded_offset = static_cast<uint32_t>(offset + 65536);
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpForPrep) << 25) | static_cast<uint32_t>(a) | (encoded_offset << 8);
        return i;
    }

    constexpr Instruction make_op_forloop(Reg a, int32_t offset) noexcept
    {
        uint32_t encoded_offset = static_cast<uint32_t>(offset + 65536);
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpForLoop) << 25) | static_cast<uint32_t>(a) | (encoded_offset << 8);
        return i;
    }

    constexpr Instruction make_op_eq(Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpEq) << 25) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_ne(Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpNe) << 25) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_lt(Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLt) << 25) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_ge(Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGe) << 25) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_le(Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLe) << 25) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_gt(Reg b, Reg c) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGt) << 25) | (static_cast<uint32_t>(b) << 8)
            | (static_cast<uint32_t>(c) << 16);
        return i;
    }

    constexpr Instruction make_op_lti(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLTI) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_gei(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGEI) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_lei(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLEI) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_gti(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGTI) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_ltf(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLTF) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_gef(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGEF) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_lef(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpLEF) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_gtf(Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpGTF) << 25) | (static_cast<uint32_t>(b) << 8) | (k << 16);
        return i;
    }

    constexpr Instruction make_op_addki(Reg a, Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpAddKI) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (k << 16);
        return i;
    }

    constexpr Instruction make_op_addkf(Reg a, Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpAddKF) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (k << 16);
        return i;
    }

    constexpr Instruction make_op_subki(Reg a, Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSubKI) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (k << 16);
        return i;
    }

    constexpr Instruction make_op_subkf(Reg a, Reg b, ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpSubKF) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (k << 16);
        return i;
    }

    constexpr Instruction make_op_addlocal(Reg a, Reg b) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpAddLocal) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8);
        return i;
    }

    constexpr Instruction make_op_addimm(Reg a, Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 256);
        i.raw = (static_cast<uint32_t>(OpCode::kOpAddImm) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (encoded_imm << 16);
        return i;
    }

    constexpr Instruction make_op_subimm(Reg a, Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 256);
        i.raw = (static_cast<uint32_t>(OpCode::kOpSubImm) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8)
            | (encoded_imm << 16);
        return i;
    }

    constexpr Instruction make_op_ltimm(Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 65536);
        i.raw = (static_cast<uint32_t>(OpCode::kOpLTImm) << 25) | static_cast<uint32_t>(b) | (encoded_imm << 8);
        return i;
    }

    constexpr Instruction make_op_geimm(Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 65536);
        i.raw = (static_cast<uint32_t>(OpCode::kOpGeImm) << 25) | static_cast<uint32_t>(b) | (encoded_imm << 8);
        return i;
    }

    constexpr Instruction make_op_leimm(Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 65536);
        i.raw = (static_cast<uint32_t>(OpCode::kOpLEImm) << 25) | static_cast<uint32_t>(b) | (encoded_imm << 8);
        return i;
    }

    constexpr Instruction make_op_gtimm(Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 65536);
        i.raw = (static_cast<uint32_t>(OpCode::kOpGtImm) << 25) | static_cast<uint32_t>(b) | (encoded_imm << 8);
        return i;
    }

    constexpr Instruction make_op_eqimm(Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 65536);
        i.raw = (static_cast<uint32_t>(OpCode::kOpEqImm) << 25) | static_cast<uint32_t>(b) | (encoded_imm << 8);
        return i;
    }

    constexpr Instruction make_op_neimm(Reg b, int32_t imm) noexcept
    {
        Instruction i{};
        uint32_t encoded_imm = static_cast<uint32_t>(imm + 65536);
        i.raw = (static_cast<uint32_t>(OpCode::kOpNeImm) << 25) | static_cast<uint32_t>(b) | (encoded_imm << 8);
        return i;
    }

    constexpr Instruction make_op_inclocal(Reg a) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpIncLocal) << 25) | a;
        return i;
    }

    constexpr Instruction make_op_declocal(Reg a) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpDecLocal) << 25) | a;
        return i;
    }

    constexpr Instruction make_op_incglobal(ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpIncGlobal) << 25) | k;
        return i;
    }

    constexpr Instruction make_op_decglobal(ConstIndex k) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpDecGlobal) << 25) | k;
        return i;
    }

    constexpr Instruction make_op_incupvalue(Reg a) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpIncUpvalue) << 25) | a;
        return i;
    }

    constexpr Instruction make_op_decupvalue(Reg a) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpDecUpvalue) << 25) | a;
        return i;
    }

    constexpr Instruction make_op_vararg(Reg a, uint8_t num) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpVararg) << 25) | static_cast<uint32_t>(a) | (static_cast<uint32_t>(num) << 8);
        return i;
    }

    constexpr Instruction make_op_varargprep(uint8_t num_params) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpVarargPrep) << 25) | num_params;
        return i;
    }

    constexpr Instruction make_op_varargexpand(Reg table_reg, uint32_t start_idx) noexcept
    {
        Instruction i{};
        i.raw = (static_cast<uint32_t>(OpCode::kOpVarargExpand) << 25) | table_reg | (start_idx << 8);
        return i;
    }

    std::string instruction_to_string(const Instruction& instr, size_t pc);

} // namespace behl
