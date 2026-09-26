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

#include <algorithm>
#include <span>
#include <string>

namespace behl
{

    static void append_annotation(format_buffer& out, const GCProto& proto, size_t i)
    {
        const auto& instr = proto.code[i];

        const OpCode op = instr.op();

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
                        format_to(out, "K{} = \"{}\"", k, str->view());
                    }
                }
                break;
            }
            case OpCode::kOpLoadI:
            {
                const auto k = instr.const_or_proto_index();
                if (k < proto.int_constants.size() && proto.int_constants[k].is_integer())
                {
                    format_to(out, "K{} = {}", k, proto.int_constants[k].get_integer());
                }
                break;
            }
            case OpCode::kOpLoadF:
            {
                const auto k = instr.const_or_proto_index();
                if (k < proto.fp_constants.size() && proto.fp_constants[k].is_fp())
                {
                    format_to(out, "K{} = {}", k, proto.fp_constants[k].get_fp());
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
                        format_to(out, "K{} = \"{}\"", k, str->view());
                    }
                }
                break;
            }
            case OpCode::kOpGetFieldS:
            case OpCode::kOpSetFieldS:
            case OpCode::kOpAddKS:
            {
                const auto k = instr.small_const_index();
                if (k < proto.str_constants.size() && proto.str_constants[k].is_string())
                {
                    auto* str = proto.str_constants[k].get_string();
                    if (str)
                    {
                        format_to(out, "K{} = \"{}\"", k, str->view());
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
                    format_to(out, "K{} = {}", k, proto.int_constants[k].get_integer());
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
                    format_to(out, "K{} = {}", k, proto.fp_constants[k].get_fp());
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
                        format_to(out, "K{} = \"{}\"", k, str->view());
                    }
                }
                break;
            }
            case OpCode::kOpClosure:
            {
                const auto p = instr.const_or_proto_index();
                format_to(out, "proto #{}", p);
                break;
            }
            case OpCode::kOpJmp:
            {
                const auto offset = instr.jump_offset();
                const auto target = static_cast<int>(i) + offset + 1;
                format_to(out, "to {}", target);
                break;
            }
            case OpCode::kOpForPrep:
            case OpCode::kOpForLoop:
            {
                const auto offset = instr.signed_offset();
                const auto target = static_cast<int>(i) + offset + 1;
                format_to(out, "to {}", target);
                break;
            }
            default:
                break;
        }
    }

    static void append_operand_modes(format_buffer& out, const OpCodeMeta& meta)
    {
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

        for (const OpMode mode : { meta.a, meta.b, meta.c })
        {
            const std::string_view str = mode_to_str(mode);
            if (str.empty())
            {
                continue;
            }
            if (out.size() != 0)
            {
                out += ' ';
            }
            out.append(str);
        }
    }

    void dump_proto(const GCProto& proto, int32_t indent)
    {
        format_buffer indent_buffer;
        indent_buffer.append(static_cast<size_t>(indent * 2), ' ');
        const std::string_view ind = indent_buffer.view();

        println("{}Proto: {} params, {}, max_stack_size: {}, source: {}", ind, proto.num_params,
            (proto.is_vararg ? "vararg" : "fixed"), proto.max_stack_size,
            proto.source_path ? proto.source_path->view() : std::string_view{ "<unknown>" });

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

        size_t max_annotation_width = 0;
        size_t max_operand_width = 0;
        for (size_t i = 0; i < proto.code.size(); ++i)
        {
            format_buffer annotation;
            append_annotation(annotation, proto, i);
            max_annotation_width = std::max(max_annotation_width, annotation.size());

            format_buffer operand_info;
            append_operand_modes(operand_info, get_opcode_meta(proto.code[i].op()));
            max_operand_width = std::max(max_operand_width, operand_info.size());
        }

        for (size_t i = 0; i < proto.code.size(); ++i)
        {
            std::string instr_str = instruction_to_string(proto.code[i], i);

            format_buffer annotation;
            append_annotation(annotation, proto, i);
            annotation.append(max_annotation_width - annotation.size(), ' ');

            format_buffer operand_info;
            append_operand_modes(operand_info, get_opcode_meta(proto.code[i].op()));
            operand_info.append(max_operand_width - operand_info.size(), ' ');

            print("{}{:>4} | {:<22} | {} | {}", ind, i, instr_str, operand_info.view(), annotation.view());

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
