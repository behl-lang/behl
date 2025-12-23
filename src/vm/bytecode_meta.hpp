#pragma once

#include "bytecode.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace behl
{
    // Operand access mode
    enum class OpMode : uint8_t
    {
        kNone,  // Operand not used
        kRead,  // Register is read
        kWrite, // Register is written
        kRW,    // Register is both read and written
    };

    // Metadata for an opcode describing its operands
    struct OpCodeMeta
    {
        OpCode opcode;         // OpCode enum value (for validation)
        OpMode a;              // Access mode for operand A
        OpMode b;              // Access mode for operand B
        OpMode c;              // Access mode for operand C
        bool has_side_effects; // Affects memory, globals, or control flow
        bool is_terminator;    // Ends a basic block (branch, return, etc.)
        bool is_branch;        // Conditional or unconditional branch
        std::string_view name; // Human-readable name for debugging
    };

    inline constexpr std::array<OpCodeMeta, kOpCount> kOpCodeMetadata = { {
        // kOpCall - R(A) = function, R(A+1..A+nargs) = args, results stored at R(A..A+nresults-1)
        { OpCode::kOpCall, OpMode::kRW, OpMode::kRead, OpMode::kNone, true, false, false, "CALL" },
        // kOpReturn - Return R(A..A+num-1)
        { OpCode::kOpReturn, OpMode::kRead, OpMode::kNone, OpMode::kNone, true, true, false, "RETURN" },
        // kOpJmp - Unconditional jump
        { OpCode::kOpJmp, OpMode::kNone, OpMode::kNone, OpMode::kNone, false, true, true, "JMP" },
        // kOpBand - R(A) = R(B) & R(C)
        { OpCode::kOpBand, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "BAND" },
        // kOpBnot - R(A) = ~R(B)
        { OpCode::kOpBnot, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "BNOT" },
        // kOpBor - R(A) = R(B) | R(C)
        { OpCode::kOpBor, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "BOR" },
        // kOpBxor - R(A) = R(B) ^ R(C)
        { OpCode::kOpBxor, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "BXOR" },
        // kOpClosure - R(A) = closure from proto index
        { OpCode::kOpClosure, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "CLOSURE" },
        // kOpDecGlobal - Decrement global variable
        { OpCode::kOpDecGlobal, OpMode::kNone, OpMode::kNone, OpMode::kNone, true, false, false, "DECGLOBAL" },
        // kOpDecLocal - R(A)--
        { OpCode::kOpDecLocal, OpMode::kRW, OpMode::kNone, OpMode::kNone, false, false, false, "DECLOCAL" },
        // kOpDecUpvalue - Decrement upvalue
        { OpCode::kOpDecUpvalue, OpMode::kNone, OpMode::kNone, OpMode::kNone, true, false, false, "DECUPVALUE" },
        // kOpDiv - R(A) = R(B) / R(C)
        { OpCode::kOpDiv, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "DIV" },
        // kOpForLoop - Numeric for loop
        { OpCode::kOpForLoop, OpMode::kRW, OpMode::kNone, OpMode::kNone, false, true, true, "FORLOOP" },
        // kOpForPrep - Prepare numeric for loop
        { OpCode::kOpForPrep, OpMode::kRW, OpMode::kNone, OpMode::kNone, false, true, true, "FORPREP" },
        // kOpIncGlobal - Increment global variable
        { OpCode::kOpIncGlobal, OpMode::kNone, OpMode::kNone, OpMode::kNone, true, false, false, "INCGLOBAL" },
        // kOpIncLocal - R(A)++
        { OpCode::kOpIncLocal, OpMode::kRW, OpMode::kNone, OpMode::kNone, false, false, false, "INCLOCAL" },
        // kOpIncUpvalue - Increment upvalue
        { OpCode::kOpIncUpvalue, OpMode::kNone, OpMode::kNone, OpMode::kNone, true, false, false, "INCUPVALUE" },
        // kOpAdd - R(A) = R(B) + R(C)
        { OpCode::kOpAdd, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "ADD" },
        // kOpAddImm - R(A) = R(B) + imm
        { OpCode::kOpAddImm, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "ADDIMM" },
        // kOpAddKF - R(A) = R(B) + KF(C)
        { OpCode::kOpAddKF, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "ADDKF" },
        // kOpAddKI - R(A) = R(B) + KI(C)
        { OpCode::kOpAddKI, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "ADDKI" },
        // kOpAddLocal - R(A) += R(B)
        { OpCode::kOpAddLocal, OpMode::kRW, OpMode::kRead, OpMode::kNone, false, false, false, "ADDLOCAL" },
        // kOpSub - R(A) = R(B) - R(C)
        { OpCode::kOpSub, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "SUB" },
        // kOpSubImm - R(A) = R(B) - imm
        { OpCode::kOpSubImm, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "SUBIMM" },
        // kOpSubKF - R(A) = R(B) - KF(C)
        { OpCode::kOpSubKF, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "SUBKF" },
        // kOpSubKI - R(A) = R(B) - KI(C)
        { OpCode::kOpSubKI, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "SUBKI" },
        // kOpEqImm - Compare R(B) == imm (test instruction)
        { OpCode::kOpEqImm, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "EQIMM" },
        // kOpGEF - Compare R(B) >= KF(C) (test instruction)
        { OpCode::kOpGEF, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "GEF" },
        // kOpGEI - Compare R(B) >= KI(C) (test instruction)
        { OpCode::kOpGEI, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "GEI" },
        // kOpGTF - Compare R(B) > KF(C) (test instruction)
        { OpCode::kOpGTF, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "GTF" },
        // kOpGTI - Compare R(B) > KI(C) (test instruction)
        { OpCode::kOpGTI, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "GTI" },
        // kOpEq - Compare R(B) == R(C) (test instruction)
        { OpCode::kOpEq, OpMode::kNone, OpMode::kRead, OpMode::kRead, false, false, false, "EQ" },
        // kOpGe - Compare R(B) >= R(C) (test instruction)
        { OpCode::kOpGe, OpMode::kNone, OpMode::kRead, OpMode::kRead, false, false, false, "GE" },
        // kOpGeImm - Compare R(B) >= imm (test instruction)
        { OpCode::kOpGeImm, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "GEIMM" },
        // kOpGt - Compare R(B) > R(C) (test instruction)
        { OpCode::kOpGt, OpMode::kNone, OpMode::kRead, OpMode::kRead, false, false, false, "GT" },
        // kOpGtImm - Compare R(B) > imm (test instruction)
        { OpCode::kOpGtImm, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "GTIMM" },
        // kOpLEF - Compare R(B) <= KF(C) (test instruction)
        { OpCode::kOpLEF, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "LEF" },
        // kOpLEI - Compare R(B) <= KI(C) (test instruction)
        { OpCode::kOpLEI, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "LEI" },
        // kOpLEImm - Compare R(B) <= imm (test instruction)
        { OpCode::kOpLEImm, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "LEIMM" },
        // kOpLTF - Compare R(B) < KF(C) (test instruction)
        { OpCode::kOpLTF, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "LTF" },
        // kOpLTI - Compare R(B) < KI(C) (test instruction)
        { OpCode::kOpLTI, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "LTI" },
        // kOpLTImm - Compare R(B) < imm (test instruction)
        { OpCode::kOpLTImm, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "LTIMM" },
        // kOpLe - Compare R(B) <= R(C) (test instruction)
        { OpCode::kOpLe, OpMode::kNone, OpMode::kRead, OpMode::kRead, false, false, false, "LE" },
        // kOpLt - Compare R(B) < R(C) (test instruction)
        { OpCode::kOpLt, OpMode::kNone, OpMode::kRead, OpMode::kRead, false, false, false, "LT" },
        // kOpNeImm - Compare R(B) != imm (test instruction)
        { OpCode::kOpNeImm, OpMode::kNone, OpMode::kRead, OpMode::kNone, false, false, false, "NEIMM" },
        // kOpLen - R(A) = #R(B)
        { OpCode::kOpLen, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "LEN" },
        // kOpLoadBool - R(A) = bool
        { OpCode::kOpLoadBool, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "LOADBOOL" },
        // kOpLoadF - R(A) = KF(const_index)
        { OpCode::kOpLoadF, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "LOADF" },
        // kOpLoadI - R(A) = KI(const_index)
        { OpCode::kOpLoadI, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "LOADI" },
        // kOpLoadImm - R(A) = immediate
        { OpCode::kOpLoadImm, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "LOADIMM" },
        // kOpLoadNil - R(A..A+B) = nil
        { OpCode::kOpLoadNil, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "LOADNIL" },
        // kOpLoadS - R(A) = KS(const_index)
        { OpCode::kOpLoadS, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "LOADS" },
        // kOpMod - R(A) = R(B) % R(C)
        { OpCode::kOpMod, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "MOD" },
        // kOpMove - R(A) = R(B)
        { OpCode::kOpMove, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "MOVE" },
        // kOpMul - R(A) = R(B) * R(C)
        { OpCode::kOpMul, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "MUL" },
        // kOpNe - Compare R(B) != R(C) (test instruction)
        { OpCode::kOpNe, OpMode::kNone, OpMode::kRead, OpMode::kRead, false, false, false, "NE" },
        // kOpNewTable - R(A) = new table
        { OpCode::kOpNewTable, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "NEWTABLE" },
        // kOpPow - R(A) = R(B) ** R(C)
        { OpCode::kOpPow, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "POW" },
        // kOpSelf - R(A) = R(B), R(A+1) = R(B)[R(C)]
        { OpCode::kOpSelf, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "SELF" },
        // kOpGetField - R(A) = R(B)[R(C)]
        { OpCode::kOpGetField, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "GETFIELD" },
        // kOpGetFieldI - R(A) = R(B)[imm]
        { OpCode::kOpGetFieldI, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "GETFIELDI" },
        // kOpGetFieldS - R(A) = R(B)[KS(C)]
        { OpCode::kOpGetFieldS, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "GETFIELDS" },
        // kOpGetGlobal - R(A) = _G[KS(const_index)]
        { OpCode::kOpGetGlobal, OpMode::kWrite, OpMode::kNone, OpMode::kNone, true, false, false, "GETGLOBAL" },
        // kOpGetUpval - R(A) = upvalue[B]
        { OpCode::kOpGetUpval, OpMode::kWrite, OpMode::kNone, OpMode::kNone, true, false, false, "GETUPVAL" },
        // kOpSetField - R(A)[R(B)] = R(C)
        { OpCode::kOpSetField, OpMode::kRead, OpMode::kRead, OpMode::kRead, true, false, false, "SETFIELD" },
        // kOpSetFieldI - R(A)[imm] = R(B)
        { OpCode::kOpSetFieldI, OpMode::kRead, OpMode::kRead, OpMode::kNone, true, false, false, "SETFIELDI" },
        // kOpSetFieldS - R(A)[KS(C)] = R(B)
        { OpCode::kOpSetFieldS, OpMode::kRead, OpMode::kRead, OpMode::kNone, true, false, false, "SETFIELDS" },
        // kOpSetGlobal - _G[KS(const_index)] = R(A)
        { OpCode::kOpSetGlobal, OpMode::kRead, OpMode::kNone, OpMode::kNone, true, false, false, "SETGLOBAL" },
        // kOpSetList - Set array portion of table from registers
        { OpCode::kOpSetList, OpMode::kRead, OpMode::kNone, OpMode::kNone, true, false, false, "SETLIST" },
        // kOpSetUpval - upvalue[B] = R(A)
        { OpCode::kOpSetUpval, OpMode::kRead, OpMode::kNone, OpMode::kNone, true, false, false, "SETUPVAL" },
        // kOpShl - R(A) = R(B) << R(C)
        { OpCode::kOpShl, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "SHL" },
        // kOpShr - R(A) = R(B) >> R(C)
        { OpCode::kOpShr, OpMode::kWrite, OpMode::kRead, OpMode::kRead, false, false, false, "SHR" },
        // kOpTailCall - Tail call with R(A) = function, R(A+1..A+nargs) = args
        { OpCode::kOpTailCall, OpMode::kRead, OpMode::kRead, OpMode::kNone, true, true, false, "TAILCALL" },
        // kOpTest - Test R(A) for truthiness
        { OpCode::kOpTest, OpMode::kRead, OpMode::kNone, OpMode::kNone, false, false, false, "TEST" },
        // kOpTestSet - R(A) = R(B) if R(B) passes test
        { OpCode::kOpTestSet, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "TESTSET" },
        // kOpToNumber - R(A) = tonumber(R(B))
        { OpCode::kOpToNumber, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "TONUMBER" },
        // kOpToString - R(A) = tostring(R(B))
        { OpCode::kOpToString, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "TOSTRING" },
        // kOpUnm - R(A) = -R(B)
        { OpCode::kOpUnm, OpMode::kWrite, OpMode::kRead, OpMode::kNone, false, false, false, "UNM" },
        // kOpVararg - R(A..A+num-1) = varargs
        { OpCode::kOpVararg, OpMode::kWrite, OpMode::kNone, OpMode::kNone, false, false, false, "VARARG" },
        // kOpVarargPrep - Prepare varargs
        { OpCode::kOpVarargPrep, OpMode::kNone, OpMode::kNone, OpMode::kNone, false, false, false, "VARARGPREP" },
        // kOpVarargExpand - Expand varargs into table array
        { OpCode::kOpVarargExpand, OpMode::kRead, OpMode::kNone, OpMode::kNone, true, false, false, "VARARGEXPAND" },
    } };

    // Helper function to get metadata for an opcode
    inline constexpr const OpCodeMeta& get_opcode_meta(OpCode op) noexcept
    {
        return kOpCodeMetadata[static_cast<size_t>(op)];
    }

    // Compile-time validation that metadata table order matches OpCode enum
    namespace detail
    {
        template<size_t I>
        constexpr bool validate_entry()
        {
            return kOpCodeMetadata[I].opcode == static_cast<OpCode>(I);
        }

        template<size_t... Is>
        constexpr bool validate_all_entries(std::index_sequence<Is...>)
        {
            return (validate_entry<Is>() && ...);
        }
    } // namespace detail

    // Validate the indexing of the metadata table at compile-time.
    static_assert(detail::validate_all_entries(std::make_index_sequence<std::size(kOpCodeMetadata)>{}),
        "Metadata table order does not match OpCode enum - check bytecode_meta.hpp");

} // namespace behl
