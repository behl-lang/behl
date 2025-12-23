#include "backend/proto.hpp"

#include "common/format.hpp"
#include "common/print.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "vm/bytecode.hpp"
#include "vm/bytecode_meta.hpp"

#include <span>
#include <string>

namespace behl
{

    static std::string get_annotation(const GCProto& proto, size_t i)
    {
        const auto& instr = proto.code[i];

        const OpCode op = instr.op();
        std::string comment;
        std::string constant_info;

        switch (op)
        {
            case OpCode::kOpLoadS:
            {
                const auto k = instr.const_or_proto_index();
                if (k < proto.str_constants.size() && proto.str_constants[k].is_string())
                {
                    auto* str = proto.str_constants[k].get_string();
                    if (str)
                    {
                        constant_info = behl::format("K{} = \"{}\"", k, str->view());
                    }
                }
                break;
            }
            case OpCode::kOpLoadI:
            {
                const auto k = instr.const_or_proto_index();
                if (k < proto.int_constants.size() && proto.int_constants[k].is_integer())
                {
                    constant_info = behl::format("K{} = {}", k, proto.int_constants[k].get_integer());
                }
                break;
            }
            case OpCode::kOpLoadF:
            {
                const auto k = instr.const_or_proto_index();
                if (k < proto.fp_constants.size() && proto.fp_constants[k].is_fp())
                {
                    constant_info = behl::format("K{} = {}", k, proto.fp_constants[k].get_fp());
                }
                break;
            }
            case OpCode::kOpGetGlobal:
            case OpCode::kOpSetGlobal:
            {
                const auto k = instr.const_or_proto_index();
                if (k < proto.str_constants.size() && proto.str_constants[k].is_string())
                {
                    auto* str = proto.str_constants[k].get_string();
                    if (str)
                    {
                        constant_info = behl::format("K{} = \"{}\"", k, str->view());
                    }
                }
                break;
            }
            case OpCode::kOpGetFieldS:
            case OpCode::kOpSetFieldS:
            {
                const auto k = instr.small_const_index();
                if (k < proto.str_constants.size() && proto.str_constants[k].is_string())
                {
                    auto* str = proto.str_constants[k].get_string();
                    if (str)
                    {
                        constant_info = behl::format("K{} = \"{}\"", k, str->view());
                    }
                }
                break;
            }
            case OpCode::kOpAddKI:
            case OpCode::kOpSubKI:
            case OpCode::kOpLTI:
            case OpCode::kOpGEI:
            case OpCode::kOpLEI:
            case OpCode::kOpGTI:
            {
                const auto k = instr.small_const_index();
                if (k < proto.int_constants.size() && proto.int_constants[k].is_integer())
                {
                    constant_info = behl::format("K{} = {}", k, proto.int_constants[k].get_integer());
                }
                break;
            }
            case OpCode::kOpAddKF:
            case OpCode::kOpSubKF:
            case OpCode::kOpLTF:
            case OpCode::kOpGEF:
            case OpCode::kOpLEF:
            case OpCode::kOpGTF:
            {
                const auto k = instr.small_const_index();
                if (k < proto.fp_constants.size() && proto.fp_constants[k].is_fp())
                {
                    constant_info = behl::format("K{} = {}", k, proto.fp_constants[k].get_fp());
                }
                break;
            }
            case OpCode::kOpIncGlobal:
            case OpCode::kOpDecGlobal:
            {
                const auto k = instr.large_const_index();
                if (k < proto.str_constants.size() && proto.str_constants[k].is_string())
                {
                    auto* str = proto.str_constants[k].get_string();
                    if (str)
                    {
                        constant_info = behl::format("K{} = \"{}\"", k, str->view());
                    }
                }
                break;
            }
            case OpCode::kOpClosure:
            {
                const auto p = instr.const_or_proto_index();
                comment = behl::format("proto #{}", p);
                break;
            }
            case OpCode::kOpJmp:
            {
                const auto offset = instr.jump_offset();
                const auto target = static_cast<int>(i) + offset + 1;
                comment = behl::format("to {}", target);
                break;
            }
            case OpCode::kOpForPrep:
            case OpCode::kOpForLoop:
            {
                const auto offset = instr.signed_offset();
                const auto target = static_cast<int>(i) + offset + 1;
                comment = behl::format("to {}", target);
                break;
            }
            default:
                break;
        }

        if (!constant_info.empty())
        {
            return constant_info;
        }
        else if (!comment.empty())
        {
            return comment;
        }
        return "";
    };

    void dump_proto(const GCProto& proto, int32_t indent)
    {
        std::string ind(static_cast<size_t>(indent * 2), ' ');

        println("{}Proto: {} params, {}, max_stack_size: {}, source: {}", ind, proto.num_params,
            (proto.is_vararg ? "vararg" : "fixed"), proto.max_stack_size, proto.source_path->view());

        const auto print_constants = [&](const std::span<const Value> constants) {
            for (size_t i = 0; i < constants.size(); ++i)
            {
                print("{}  {:>3}: ", ind, i);
                const auto& v = constants[i];

                if (v.is_nil())
                {
                    println("nil");
                    continue;
                }
                else if (v.is_bool())
                {
                    println("{}", v.get_bool() ? "true" : "false");
                    continue;
                }
                else if (v.is_integer())
                {
                    println("{}", v.get_integer());
                    continue;
                }
                else if (v.is_fp())
                {
                    println("{}", v.get_fp());
                    continue;
                }
                else if (v.is_string())
                {
                    auto* str_ptr = v.get_string();
                    if (str_ptr)
                    {
                        println("\"{}\"", str_ptr->view());
                    }
                    else
                    {
                        println("<null string>");
                    }
                    continue;
                }
                else if (v.is_table())
                {
                    println("<table>");
                }
                else if (v.is_closure())
                {
                    println("<closure>");
                }
                else if (v.is_cfunction())
                {
                    println("<cfunction>");
                }
            }
        };

        if (!proto.str_constants.empty())
        {
            println("{}String Consts:", ind);
            print_constants(proto.str_constants);
        }
        if (!proto.int_constants.empty())
        {
            println("{}Int Consts:", ind);
            print_constants(proto.int_constants);
        }
        if (!proto.fp_constants.empty())
        {
            println("{}FP Consts:", ind);
            print_constants(proto.fp_constants);
        }

        println("{}Code:", ind);

        const auto mode_to_str = [](OpMode mode) -> std::string_view {
            switch (mode)
            {
                case OpMode::kRead:
                    return "R";
                case OpMode::kWrite:
                    return "W";
                case OpMode::kRW:
                    return "RW";
                case OpMode::kNone:
                    return "";
                default:
                    return "?";
            }
        };

        size_t max_annotation_width = 0;
        size_t max_operand_width = 0;
        for (size_t i = 0; i < proto.code.size(); ++i)
        {
            std::string annotation = get_annotation(proto, i);
            max_annotation_width = std::max(max_annotation_width, annotation.size());

            const OpCode op = proto.code[i].op();
            const auto& meta = get_opcode_meta(op);

            std::string operand_info;
            auto a_str = mode_to_str(meta.a);
            auto b_str = mode_to_str(meta.b);
            auto c_str = mode_to_str(meta.c);

            if (!a_str.empty())
            {
                operand_info += std::string(a_str);
            }
            if (!b_str.empty())
            {
                if (!operand_info.empty())
                {
                    operand_info += " ";
                }
                operand_info += std::string(b_str);
            }
            if (!c_str.empty())
            {
                if (!operand_info.empty())
                {
                    operand_info += " ";
                }
                operand_info += std::string(c_str);
            }

            max_operand_width = std::max(max_operand_width, operand_info.size());
        }

        for (size_t i = 0; i < proto.code.size(); ++i)
        {
            std::string instr_str = instruction_to_string(proto.code[i], i);
            std::string annotation = get_annotation(proto, i);

            if (annotation.size() < max_annotation_width)
            {
                annotation += std::string(max_annotation_width - annotation.size(), ' ');
            }

            const OpCode op = proto.code[i].op();
            const auto& meta = get_opcode_meta(op);

            std::string operand_info;
            auto a_str = mode_to_str(meta.a);
            auto b_str = mode_to_str(meta.b);
            auto c_str = mode_to_str(meta.c);

            if (!a_str.empty())
            {
                operand_info += std::string(a_str);
            }
            if (!b_str.empty())
            {
                if (!operand_info.empty())
                {
                    operand_info += " ";
                }
                operand_info += std::string(b_str);
            }
            if (!c_str.empty())
            {
                if (!operand_info.empty())
                {
                    operand_info += " ";
                }
                operand_info += std::string(c_str);
            }

            if (operand_info.size() < max_operand_width)
            {
                operand_info += std::string(max_operand_width - operand_info.size(), ' ');
            }

            print("{}{:>4} | {:<22} | {} | {}", ind, i, instr_str, operand_info, annotation);

            if (i < proto.line_info.size() && i < proto.column_info.size())
            {
                println(" | line {:>3}, col {:>2}", proto.line_info[i], proto.column_info[i]);
            }
            else
            {
                println("");
            }
        }

        for (size_t i = 0; i < proto.protos.size(); ++i)
        {
            println("{}Nested proto {}:", ind, i);
            dump_proto(*proto.protos[i], indent + 1);
        }
    }
} // namespace behl
