#include "api/api_internal.hpp"
#include "behl.hpp"
#include "common/format.hpp"
#include "common/hash_map.hpp"
#include "common/print.hpp"
#include "common/vector.hpp"
#include "gc/gco_table.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm_error.hpp"

#include <algorithm>
#include <bit>
#include <string>
#include <string_view>
#include <utility>

namespace behl
{

    struct DumpOptions
    {
        size_t indent_size{ 4 };
        bool use_newlines{ true };
        Integer max_depth{ 100 };
    };

    enum class DumpTaskKind : uint8_t
    {
        kText,
        kIndent,
        kValue,
        kLeave,
    };

    struct DumpTask
    {
        DumpTaskKind kind{ DumpTaskKind::kText };
        std::string_view text{};
        Value value{};
        size_t level{ 0 };
    };

    struct DumpEntry
    {
        Value key{};
        Value value{};
    };

    static constexpr Integer kMaxDumpIndent = 1024;

    static DumpTask text_task(std::string_view text)
    {
        return DumpTask{ DumpTaskKind::kText, text, Value{}, 0 };
    }

    static DumpTask indent_task(size_t level)
    {
        return DumpTask{ DumpTaskKind::kIndent, std::string_view{}, Value{}, level };
    }

    static DumpTask value_task(const Value& value, size_t level)
    {
        return DumpTask{ DumpTaskKind::kValue, std::string_view{}, value, level };
    }

    static DumpTask leave_task(GCTable* t)
    {
        return DumpTask{ DumpTaskKind::kLeave, std::string_view{}, Value(t), 0 };
    }

    static DumpOptions read_dump_options(State* S, std::string_view fn_name)
    {
        DumpOptions options{};
        if (get_top(S) <= 1 || type(S, 1) != Type::kTable)
        {
            return options;
        }

        table_getfield(S, 1, "indent");
        if (type(S, -1) == Type::kInteger)
        {
            const Integer indent = to_integer(S, -1);
            if (indent < 0)
            {
                error(S, behl::format("bad argument #2 to '{}' (indent must not be negative)", fn_name));
            }
            if (indent > kMaxDumpIndent)
            {
                error(S, behl::format("bad argument #2 to '{}' (indent must not exceed {})", fn_name, kMaxDumpIndent));
            }
            options.indent_size = static_cast<size_t>(indent);
        }
        pop(S, 1);

        table_getfield(S, 1, "compact");
        if (type(S, -1) == Type::kBoolean)
        {
            options.use_newlines = !to_boolean(S, -1);
        }
        pop(S, 1);

        table_getfield(S, 1, "max_recursion");
        if (type(S, -1) == Type::kInteger)
        {
            options.max_depth = to_integer(S, -1);
        }
        pop(S, 1);

        return options;
    }

    static void append_scalar(std::string& out, const Value& v)
    {
        switch (v.get_type())
        {
            case Type::kBoolean:
                out += v.get_bool() ? "true" : "false";
                break;
            case Type::kInteger:
                out += behl::format("{}", v.get_integer());
                break;
            case Type::kNumber:
                out += behl::format("{}", v.get_fp());
                break;
            case Type::kString:
                out += behl::format("\"{}\"", v.get_string()->view());
                break;
            case Type::kClosure:
                out += behl::format<"function:{:p}">(static_cast<const void*>(v.get_closure()));
                break;
            case Type::kCFunction:
                out += behl::format<"cfunction:{:p}">(std::bit_cast<const void*>(v.get_cfunction()));
                break;
            case Type::kUserdata:
                out += behl::format<"userdata:{:p}">(static_cast<const void*>(v.get_userdata()));
                break;
            case Type::kBuffer:
                out += behl::format<"buffer:{:p}">(static_cast<const void*>(v.get_buffer()));
                break;
            default:
                out += "nil";
                break;
        }
    }

    static void collect_entries(const GCTable* t, AutoVector<DumpEntry>& entries)
    {
        for (size_t i = 0; i < t->array.size(); ++i)
        {
            if (!t->array[i].is_nil())
            {
                entries.push_back(DumpEntry{ Value(static_cast<Integer>(i)), t->array[i] });
            }
        }
        for (const auto& [key, value] : t->hash)
        {
            if (!value.is_nil())
            {
                entries.push_back(DumpEntry{ key, value });
            }
        }
    }

    static bool sort_if_sequential(AutoVector<DumpEntry>& entries)
    {
        if (entries.empty())
        {
            return false;
        }
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (!entries[i].key.is_integer() || entries[i].key.get_integer() < 0)
            {
                return false;
            }
        }
        std::ranges::sort(
            entries, [](const DumpEntry& a, const DumpEntry& b) { return a.key.get_integer() < b.key.get_integer(); });
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].key.get_integer() != static_cast<Integer>(i))
            {
                return false;
            }
        }
        return true;
    }

    static void plan_table(
        GCTable* t, size_t level, const DumpOptions& options, AutoVector<DumpEntry>& entries, AutoVector<DumpTask>& plan)
    {
        const bool nl = options.use_newlines;
        const size_t inner = level + 1;
        const bool sequential = sort_if_sequential(entries);

        plan.push_back(text_task("{"));
        if (nl && (!sequential || !entries.empty()))
        {
            plan.push_back(text_task("\n"));
        }

        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i > 0 && !sequential)
            {
                plan.push_back(text_task(nl ? ",\n" : ","));
            }

            if (nl)
            {
                plan.push_back(indent_task(inner));
            }
            else if (i > 0)
            {
                plan.push_back(text_task(" "));
            }

            if (sequential)
            {
                plan.push_back(value_task(entries[i].value, inner));
                if (i + 1 < entries.size())
                {
                    plan.push_back(text_task(","));
                }
                if (nl)
                {
                    plan.push_back(text_task("\n"));
                }
            }
            else
            {
                plan.push_back(text_task("["));
                plan.push_back(value_task(entries[i].key, inner));
                plan.push_back(text_task("] = "));
                plan.push_back(value_task(entries[i].value, inner));
            }
        }

        if (nl && !entries.empty())
        {
            if (!sequential)
            {
                plan.push_back(text_task("\n"));
            }
            plan.push_back(indent_task(level));
        }

        plan.push_back(text_task("}"));
        plan.push_back(leave_task(t));
    }

    static std::string dump_table(State* S, int32_t idx, const DumpOptions& options)
    {
        const Value root = S->stack[static_cast<size_t>(resolve_index(S, idx))];

        std::string out;
        AutoVector<DumpTask> work(S);
        AutoVector<DumpTask> plan(S);
        AutoVector<DumpEntry> entries(S);
        AutoHashMap<const GCTable*, bool> on_path(S);

        work.push_back(value_task(root, 0));
        while (!work.empty())
        {
            const DumpTask task = work.back();
            work.pop_back();

            switch (task.kind)
            {
                case DumpTaskKind::kText:
                    out += task.text;
                    break;
                case DumpTaskKind::kIndent:
                    out.append(task.level * options.indent_size, ' ');
                    break;
                case DumpTaskKind::kLeave:
                    on_path.erase(task.value.get_table());
                    break;
                case DumpTaskKind::kValue:
                {
                    if (!task.value.is_table())
                    {
                        append_scalar(out, task.value);
                        break;
                    }

                    GCTable* t = task.value.get_table();
                    if (static_cast<Integer>(task.level) >= options.max_depth || on_path.contains(t))
                    {
                        out += "{...}";
                        break;
                    }
                    on_path.insert_or_assign(t, true);

                    entries.clear();
                    collect_entries(t, entries);
                    plan.clear();
                    plan_table(t, task.level, options, entries, plan);
                    for (size_t i = plan.size(); i > 0; --i)
                    {
                        work.push_back(plan[i - 1]);
                    }
                    break;
                }
            }
        }

        return out;
    }

    static int tbl_rawlen(State* S)
    {
        if (is_string(S, 0))
        {
            push_integer(S, static_cast<Integer>(to_string(S, 0).size()));
            return 1;
        }
        if (is_buffer(S, 0))
        {
            push_integer(S, static_cast<Integer>(buffer_len(S, 0)));
            return 1;
        }
        if (!is_table(S, 0))
        {
            raise_type_error(
                S, SourceLocation{}, "bad argument #1 (expected table, string or buffer, got {})", value_typename(S, 0));
        }

        table_rawlen(S, 0);
        return 1;
    }

    static int tbl_rawget(State* S)
    {
        check_type(S, 0, Type::kTable);
        if (get_top(S) < 2)
        {
            error(S, "bad argument #2 to 'rawget' (value expected)");
        }

        set_top(S, 2);
        table_rawget(S, 0);

        return 1;
    }

    static int tbl_rawset(State* S)
    {
        check_type(S, 0, Type::kTable);
        if (get_top(S) < 2)
        {
            error(S, "bad argument #2 to 'rawset' (value expected)");
        }
        if (get_top(S) < 3)
        {
            error(S, "bad argument #3 to 'rawset' (value expected)");
        }

        set_top(S, 3);
        table_rawset(S, 0);

        dup(S, 0);

        return 1;
    }

    static int tbl_insert(State* S)
    {
        check_type(S, 0, Type::kTable);

        const int32_t nargs = get_top(S);
        if (nargs != 2 && nargs != 3)
        {
            error(S, "wrong number of arguments to 'insert'");
        }

        table_rawlen(S, 0);
        const Integer len = to_integer(S, -1);
        pop(S, 1);

        if (nargs == 2)
        {
            push_integer(S, len);
            dup(S, 1);
            table_rawset(S, 0);
            return 0;
        }

        const Integer pos = check_integer(S, 1);
        if (pos < 0 || pos > len)
        {
            error(S, "bad argument #2 to 'insert' (position out of bounds)");
        }

        for (Integer i = len; i > pos; --i)
        {
            push_integer(S, i);
            push_integer(S, i - 1);
            table_rawget(S, 0);
            table_rawset(S, 0);
        }

        push_integer(S, pos);
        dup(S, 2);
        table_rawset(S, 0);

        return 0;
    }

    static int tbl_dump(State* S)
    {
        check_type(S, 0, Type::kTable);

        const DumpOptions options = read_dump_options(S, "dump");
        const std::string result = dump_table(S, 0, options);

        push_string(S, result);
        return 1;
    }

    static int tbl_print(State* S)
    {
        check_type(S, 0, Type::kTable);

        const DumpOptions options = read_dump_options(S, "print");
        const std::string result = dump_table(S, 0, options);
        println(S, "{}", result);

        return 0;
    }

    static Integer optional_bound(State* S, int32_t idx, Integer fallback)
    {
        if (get_top(S) <= idx || type(S, idx) == Type::kNil)
        {
            return fallback;
        }
        return check_integer(S, idx);
    }

    static int tbl_unpack(State* S)
    {
        check_type(S, 0, Type::kTable);

        // Get the table length
        table_rawlen(S, 0);
        Integer len = to_integer(S, -1);
        pop(S, 1);

        // Default range: [0, len)
        Integer start = optional_bound(S, 1, 0);
        Integer end = optional_bound(S, 2, len - 1);

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
