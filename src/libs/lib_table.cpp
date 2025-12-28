#include "behl.hpp"
#include "common/format.hpp"
#include "common/print.hpp"
#include "common/vector.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace behl
{

    static void dump_value_to_string(State* S, int idx, int indent_level, int indent_size, bool use_newlines, int max_depth,
        std::set<const void*>& visited, std::string& out);

    static const void* get_value_ptr(State* S, int idx)
    {
        return reinterpret_cast<const void*>(static_cast<uintptr_t>(idx >= 0 ? idx : get_top(S) + idx));
    }

    static void append_indent(std::string& out, int indent_level, int indent_size)
    {
        out.append(static_cast<size_t>(indent_level * indent_size), ' ');
    }

    static void dump_table_to_string(State* S, int idx, int indent_level, int indent_size, bool use_newlines, int max_depth,
        std::set<const void*>& visited, std::string& out)
    {
        if (indent_level >= max_depth)
        {
            out += "{...}";
            return;
        }

        const void* table_id = get_value_ptr(S, idx);
        if (visited.count(table_id))
        {
            out += "{...}";
            return;
        }
        visited.insert(table_id);

        dup(S, idx);
        int table_idx = -1;

        push_nil(S);
        bool has_non_integer = false;
        AutoVector<Integer> indices(S);

        while (table_next(S, table_idx - 1))
        {
            int key_idx = -2;

            if (type(S, key_idx) == Type::kInteger)
            {
                Integer key = to_integer(S, key_idx);
                if (key >= 0)
                {
                    indices.push_back(key);
                }
                else
                {
                    has_non_integer = true;
                }
            }
            else
            {
                has_non_integer = true;
            }

            pop(S, 1);
        }

        std::ranges::sort(indices);
        bool is_sequential = !has_non_integer && !indices.empty();
        if (is_sequential)
        {
            for (size_t i = 0; i < indices.size(); ++i)
            {
                if (indices[i] != static_cast<Integer>(i))
                {
                    is_sequential = false;
                    break;
                }
            }
        }

        out += "{";

        if (is_sequential)
        {
            if (use_newlines && !indices.empty())
            {
                out += "\n";
            }

            for (size_t i = 0; i < indices.size(); ++i)
            {
                if (use_newlines)
                {
                    append_indent(out, indent_level + 1, indent_size);
                }
                else if (i > 0)
                {
                    out += " ";
                }

                push_integer(S, static_cast<Integer>(i));
                table_rawget(S, table_idx - 1);
                dump_value_to_string(S, -1, indent_level + 1, indent_size, use_newlines, max_depth, visited, out);
                pop(S, 1);

                if (i < indices.size() - 1)
                {
                    out += ",";
                }

                if (use_newlines)
                {
                    out += "\n";
                }
            }

            if (use_newlines && !indices.empty())
            {
                append_indent(out, indent_level, indent_size);
            }
        }
        else
        {
            if (use_newlines)
            {
                out += "\n";
            }

            push_nil(S);
            bool first = true;

            while (table_next(S, table_idx - 1))
            {
                if (!first)
                {
                    out += ",";
                    if (use_newlines)
                    {
                        out += "\n";
                    }
                }
                first = false;

                if (use_newlines)
                {
                    append_indent(out, indent_level + 1, indent_size);
                }
                else if (!first)
                {
                    out += " ";
                }

                out += "[";
                dump_value_to_string(S, -2, indent_level + 1, indent_size, use_newlines, max_depth, visited, out);
                out += "] = ";
                dump_value_to_string(S, -1, indent_level + 1, indent_size, use_newlines, max_depth, visited, out);

                pop(S, 1);
            }

            if (use_newlines && !first)
            {
                out += "\n";
                append_indent(out, indent_level, indent_size);
            }
        }

        out += "}";

        pop(S, 1);

        visited.erase(table_id);
    }

    static void dump_value_to_string(State* S, int idx, int indent_level, int indent_size, bool use_newlines, int max_depth,
        std::set<const void*>& visited, std::string& out)
    {
        switch (type(S, idx))
        {
            case Type::kNil:
                out += "nil";
                break;
            case Type::kBoolean:
                out += (to_boolean(S, idx) ? "true" : "false");
                break;
            case Type::kInteger:
                out += behl::format("{}", to_integer(S, idx));
                break;
            case Type::kNumber:
                out += behl::format("{}", to_number(S, idx));
                break;
            case Type::kString:
                out += behl::format("\"{}\"", to_string(S, idx));
                break;
            case Type::kTable:
                dump_table_to_string(S, idx, indent_level, indent_size, use_newlines, max_depth, visited, out);
                break;
            default:
                out += "nil";
                break;
        }
    }

    static int tbl_rawlen(State* S)
    {
        if (get_top(S) < 1)
        {
            push_integer(S, 0);
            return 1;
        }

        if (type(S, 0) != Type::kTable)
        {
            push_integer(S, 0);
            return 1;
        }

        table_rawlen(S, 0);
        return 1;
    }

    static int tbl_rawget(State* S)
    {
        if (get_top(S) < 2)
        {
            push_nil(S);
            return 1;
        }

        if (type(S, 0) != Type::kTable)
        {
            push_nil(S);
            return 1;
        }

        dup(S, 1);
        table_rawget(S, 0);

        return 1;
    }

    static int tbl_rawset(State* S)
    {
        if (get_top(S) < 3)
        {
            push_nil(S);
            return 1;
        }

        if (type(S, 0) != Type::kTable)
        {
            push_nil(S);
            return 1;
        }

        table_rawset(S, 0);

        dup(S, 0);

        return 1;
    }

    static int tbl_insert(State* S)
    {
        check_type(S, 0, Type::kTable);

        table_rawlen(S, 0);
        const auto l = to_integer(S, -1);
        pop(S, 1);

        const auto idx = l;

        push_integer(S, idx);

        dup(S, 1);
        table_rawset(S, 0);

        return 0;
    }

    static int tbl_dump(State* S)
    {
        check_type(S, 0, Type::kTable);

        int indent_size = 4;
        bool use_newlines = true;
        int max_depth = 100;

        if (get_top(S) > 1 && type(S, 1) == Type::kTable)
        {
            table_getfield(S, 1, "indent");
            if (type(S, -1) == Type::kInteger)
            {
                indent_size = static_cast<int>(to_integer(S, -1));
            }
            pop(S, 1);

            table_getfield(S, 1, "compact");
            if (type(S, -1) == Type::kBoolean)
            {
                use_newlines = !to_boolean(S, -1);
            }
            pop(S, 1);

            table_getfield(S, 1, "max_recursion");
            if (type(S, -1) == Type::kInteger)
            {
                max_depth = static_cast<int>(to_integer(S, -1));
            }
            pop(S, 1);
        }

        std::string result;
        std::set<const void*> visited;
        dump_table_to_string(S, 0, 0, indent_size, use_newlines, max_depth, visited, result);

        push_string(S, result.c_str());
        return 1;
    }

    static int tbl_print(State* S)
    {
        check_type(S, 0, Type::kTable);

        int indent_size = 4;
        bool use_newlines = true;
        int max_depth = 100;

        if (get_top(S) > 1 && type(S, 1) == Type::kTable)
        {
            table_getfield(S, 1, "indent");
            if (type(S, -1) == Type::kInteger)
            {
                indent_size = static_cast<int>(to_integer(S, -1));
            }
            pop(S, 1);

            table_getfield(S, 1, "compact");
            if (type(S, -1) == Type::kBoolean)
            {
                use_newlines = !to_boolean(S, -1);
            }
            pop(S, 1);

            table_getfield(S, 1, "max_recursion");
            if (type(S, -1) == Type::kInteger)
            {
                max_depth = static_cast<int>(to_integer(S, -1));
            }
            pop(S, 1);
        }

        std::string result;
        std::set<const void*> visited;
        dump_table_to_string(S, 0, 0, indent_size, use_newlines, max_depth, visited, result);
        println(S, "{}", result);

        return 0;
    }

    static int tbl_unpack(State* S)
    {
        check_type(S, 0, Type::kTable);

        // Get the table length
        table_rawlen(S, 0);
        Integer len = to_integer(S, -1);
        pop(S, 1);

        // Default range: [0, len)
        Integer start = 0;
        Integer end = len - 1;

        // Optional start index (arg 1)
        if (get_top(S) > 1 && type(S, 1) == Type::kInteger)
        {
            start = to_integer(S, 1);
        }

        // Optional end index (arg 2)
        if (get_top(S) > 2 && type(S, 2) == Type::kInteger)
        {
            end = to_integer(S, 2);
        }

        // Clamp to valid range
        if (start < 0)
        {
            start = 0;
        }
        if (end >= len)
        {
            end = len - 1;
        }
        if (end < start)
        {
            // Return nothing if invalid range
            return 0;
        }

        // Push all values from table[start] to table[end] onto the stack
        int result_count = 0;
        for (Integer i = start; i <= end; ++i)
        {
            push_integer(S, i);
            table_rawget(S, 0);
            ++result_count;
        }

        return result_count;
    }

    static int tbl_setname(State* S)
    {
        check_type(S, 0, Type::kTable);

        auto name_opt = check_string(S, 1);
        table_setname(S, 0, name_opt);

        return 0;
    }

    void load_lib_table(State* S)
    {
        static constexpr ModuleReg table_funcs[] = {
            { "rawlen", tbl_rawlen },
            { "rawget", tbl_rawget },
            { "rawset", tbl_rawset },
            { "insert", tbl_insert },
            { "dump", tbl_dump },
            { "print", tbl_print },
            { "unpack", tbl_unpack },
            { "set_name", tbl_setname },
        };

        ModuleDef table_module = { .funcs = table_funcs };

        create_module(S, "table", table_module);
    }

} // namespace behl
