#include "bytecode.hpp"

#include "bytecode_meta.hpp"
#include "common/format.hpp"

#include <string>

namespace behl
{

    std::string instruction_to_string(const Instruction& instr, [[maybe_unused]] size_t pc)
    {
        OpCode op = instr.op();
        const auto& meta = get_opcode_meta(op);
        std::string opcode_str;
        switch (op)
        {
            case OpCode::kOpMove:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpLoadS:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.a(), instr.const_or_proto_index());
                break;
            case OpCode::kOpLoadI:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.a(), instr.const_or_proto_index());
                break;
            case OpCode::kOpLoadF:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.a(), instr.const_or_proto_index());
                break;
            case OpCode::kOpLoadBool:
                opcode_str = format(
                    "{:<9} R{} {}{}", meta.name, instr.a(), (instr.b() ? "true" : "false"), (instr.c() ? " skip" : ""));
                break;
            case OpCode::kOpLoadNil:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpLoadImm:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.signed_immediate());
                break;
            case OpCode::kOpGetUpval:
                opcode_str = format("{:<9} R{} U{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpGetGlobal:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.a(), instr.const_or_proto_index());
                break;
            case OpCode::kOpGetField:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpGetFieldI:
            {
                opcode_str = format("{:<9} R{} R{} {}", meta.name, instr.a(), instr.b(), instr.small_const_index());
                break;
            }
            case OpCode::kOpGetFieldS:
            {
                opcode_str = format("{:<9} R{} R{} K{}", meta.name, instr.a(), instr.b(), instr.small_const_index());
                break;
            }
            case OpCode::kOpSetUpval:
                opcode_str = format("{:<9} U{} R{}", meta.name, instr.b(), instr.a());
                break;
            case OpCode::kOpSetGlobal:
                opcode_str = format("{:<9} K{} R{}", meta.name, instr.const_or_proto_index(), instr.a());
                break;
            case OpCode::kOpSetField:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpSetFieldI:
            {
                opcode_str = format("{:<9} R{} R{} {}", meta.name, instr.a(), instr.b(), instr.small_const_index());
                break;
            }
            case OpCode::kOpSetFieldS:
            {
                opcode_str = format("{:<9} R{} R{} K{}", meta.name, instr.a(), instr.b(), instr.small_const_index());
                break;
            }
            case OpCode::kOpNewTable:
                opcode_str = format("{:<9} R{} {} {}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpSelf:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpIncLocal:
                opcode_str = format("{:<9} R{}", meta.name, instr.a());
                break;
            case OpCode::kOpDecLocal:
                opcode_str = format("{:<9} R{}", meta.name, instr.a());
                break;
            case OpCode::kOpAddLocal:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpAddImm:
                opcode_str = format("{:<9} R{} R{} {}", meta.name, instr.a(), instr.b(), instr.signed_immediate_9bit());
                break;
            case OpCode::kOpSubImm:
                opcode_str = format("{:<9} R{} R{} {}", meta.name, instr.a(), instr.b(), instr.signed_immediate_9bit());
                break;
            case OpCode::kOpAddKI:
                opcode_str = format("{:<9} R{} R{} K{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpAddKF:
                opcode_str = format("{:<9} R{} R{} K{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpSubKI:
                opcode_str = format("{:<9} R{} R{} K{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpSubKF:
                opcode_str = format("{:<9} R{} R{} K{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpIncGlobal:
                opcode_str = format("{:<9} K{}", meta.name, instr.large_const_index());
                break;
            case OpCode::kOpDecGlobal:
                opcode_str = format("{:<9} K{}", meta.name, instr.large_const_index());
                break;
            case OpCode::kOpIncUpvalue:
                opcode_str = format("{:<9} U{}", meta.name, instr.a());
                break;
            case OpCode::kOpDecUpvalue:
                opcode_str = format("{:<9} U{}", meta.name, instr.a());
                break;
            case OpCode::kOpAdd:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpSub:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpMul:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpDiv:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpMod:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpPow:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpBand:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpBor:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpBxor:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpShl:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpShr:
                opcode_str = format("{:<9} R{} R{} R{}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpUnm:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpBnot:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpLen:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpToString:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpToNumber:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpJmp:
                opcode_str = format("{:<9} {}", meta.name, instr.jump_offset());
                break;
            case OpCode::kOpEq:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpNe:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpLt:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpGe:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpLe:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpGt:
                opcode_str = format("{:<9} R{} R{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpLTI:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpGEI:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpLTF:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpGEF:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpLEI:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpGTI:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpLEF:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpGTF:
                opcode_str = format("{:<9} R{} K{}", meta.name, instr.b(), instr.c());
                break;
            case OpCode::kOpLTImm:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.b_at_8(), instr.signed_immediate());
                break;
            case OpCode::kOpGeImm:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.b_at_8(), instr.signed_immediate());
                break;
            case OpCode::kOpLEImm:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.b_at_8(), instr.signed_immediate());
                break;
            case OpCode::kOpGtImm:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.b_at_8(), instr.signed_immediate());
                break;
            case OpCode::kOpEqImm:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.b_at_8(), instr.signed_immediate());
                break;
            case OpCode::kOpNeImm:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.b_at_8(), instr.signed_immediate());
                break;
            case OpCode::kOpTest:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), (instr.b() ? "invert" : "normal"));
                break;
            case OpCode::kOpTestSet:
                opcode_str = format("{:<9} R{} R{} {}", meta.name, instr.a(), instr.b(), (instr.c() ? "invert" : "normal"));
                break;
            case OpCode::kOpCall:
                opcode_str = format("{:<9} R{} {} {}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpTailCall:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpReturn:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpForPrep:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.signed_offset());
                break;
            case OpCode::kOpForLoop:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.signed_offset());
                break;
            case OpCode::kOpSetList:
                // Assuming setlist has num_fields in b, extra in c or something
                opcode_str = format("{:<9} R{} {} {}", meta.name, instr.a(), instr.b(), instr.c());
                break;
            case OpCode::kOpClosure:
                opcode_str = format("{:<9} R{} P{}", meta.name, instr.a(), instr.const_or_proto_index());
                break;
            case OpCode::kOpVararg:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.b());
                break;
            case OpCode::kOpVarargPrep:
                opcode_str = format("{:<9} {}", meta.name, instr.a());
                break;
            case OpCode::kOpVarargExpand:
                opcode_str = format("{:<9} R{} {}", meta.name, instr.a(), instr.b());
                break;
            default:
                opcode_str = "UNKNOWN";
                break;
        }
        return opcode_str;
    }

} // namespace behl
