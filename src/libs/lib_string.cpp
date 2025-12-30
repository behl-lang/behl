#include "behl.hpp"
#include "common/format.hpp"
#include "common/vector.hpp"
#include "gc/gc.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "state.hpp"
#include "vm/vm_detail.hpp"

#include <algorithm>
#include <cctype>

namespace behl
{

    // string.len(s) - returns length of string
    static int str_len(State* S)
    {
        auto str = check_string(S, 0);

        push_integer(S, static_cast<Integer>(str.length()));

        return 1;
    }

    // string.sub(s, start, end) - returns substring from start to end (0-based, inclusive)
    static int str_sub(State* S)
    {
        auto str = check_string(S, 0);
        auto start = check_integer(S, 1);

        const Integer len = static_cast<Integer>(str.length());
        Integer end = len - 1;

        if (get_top(S) > 2)
        {
            end = check_integer(S, 2);
        }

        // Handle negative indices
        if (start < 0)
        {
            start = len + start;
        }
        if (end < 0)
        {
            end = len + end;
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

        if (start > end || start >= len)
        {
            push_string(S, "");
            return 1;
        }

        const size_t substr_len = static_cast<size_t>(end - start + 1);
        const std::string_view substr = str.substr(static_cast<size_t>(start), substr_len);

        auto* result = gc_new_string(S, substr);
        S->stack.push_back(S, Value(result));

        return 1;
    }

    // string.upper(s) - converts string to uppercase
    static int str_upper(State* S)
    {
        auto str = check_string(S, 0);

        auto* result = gc_new_string(S, str);
        char* data = result->data();
        const size_t len = result->size();

        for (size_t i = 0; i < len; ++i)
        {
            data[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(data[i])));
        }

        S->stack.push_back(S, Value(result));
        return 1;
    }

    // string.lower(s) - converts string to lowercase
    static int str_lower(State* S)
    {
        auto str = check_string(S, 0);

        auto* result = gc_new_string(S, str);
        char* data = result->data();
        const size_t len = result->size();

        for (size_t i = 0; i < len; ++i)
        {
            data[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(data[i])));
        }

        S->stack.push_back(S, Value(result));
        return 1;
    }

    // string.reverse(s) - reverses a string
    static int str_reverse(State* S)
    {
        auto str = check_string(S, 0);

        auto* result = gc_new_string(S, str);
        char* data = result->data();
        const size_t len = result->size();

        for (size_t i = 0; i < len / 2; ++i)
        {
            std::swap(data[i], data[len - 1 - i]);
        }

        S->stack.push_back(S, Value(result));
        return 1;
    }

    // string.char(code1, code2, ...) - converts character codes to string
    static int str_char(State* S)
    {
        const int n = get_top(S);

        AutoVector<char> buffer(S);
        buffer.reserve(static_cast<size_t>(n));

        for (int i = 0; i < n; ++i)
        {
            const Integer code = check_integer(S, i);
            if (code < 0 || code > 255)
            {
                push_string(S, "");
                return 1;
            }

            buffer.push_back(static_cast<char>(code));
        }

        push_string(S, std::string_view(buffer.data(), buffer.size()));

        return 1;
    }

    // string.byte(s, index) - returns byte value at index (0-based)
    static int str_byte(State* S)
    {
        auto str = check_string(S, 0);

        if (str.empty())
        {
            push_nil(S);
            return 1;
        }

        Integer idx = 0;
        if (get_top(S) > 1 && type(S, 1) == Type::kInteger)
        {
            idx = to_integer(S, 1);
        }

        const Integer len = static_cast<Integer>(str.length());

        // Handle negative index
        if (idx < 0)
        {
            idx = len + idx;
        }

        if (idx < 0 || idx >= len)
        {
            push_nil(S);
            return 1;
        }

        push_integer(S, static_cast<Integer>(static_cast<unsigned char>(str[static_cast<size_t>(idx)])));
        return 1;
    }

    // string.rep(s, n) - repeats string n times
    static int str_rep(State* S)
    {
        auto str = check_string(S, 0);
        const Integer n = check_integer(S, 1);

        if (n <= 0)
        {
            push_string(S, "");
            return 1;
        }

        const size_t str_len = str.length();
        const size_t total_len = str_len * static_cast<size_t>(n);

        AutoVector<char> buffer(S);
        buffer.reserve(total_len);

        for (Integer i = 0; i < n; ++i)
        {
            for (size_t j = 0; j < str_len; ++j)
            {
                buffer.push_back(str[j]);
            }
        }

        push_string(S, std::string_view(buffer.data(), buffer.size()));

        return 1;
    }

    // string.find(s, substr, start) - simple substring search (no patterns)
    static int str_find(State* S)
    {
        auto str = check_string(S, 0);
        auto substr = check_string(S, 1);

        Integer start = 0;
        if (get_top(S) > 2)
        {
            start = check_integer(S, 2);
        }

        const Integer len = static_cast<Integer>(str.length());

        // Handle negative start
        if (start < 0)
        {
            start = len + start;
        }

        if (start < 0 || start >= len)
        {
            push_integer(S, -1);
            return 1;
        }

        const size_t pos = str.find(substr, static_cast<size_t>(start));
        if (pos == std::string_view::npos)
        {
            push_integer(S, -1);
        }
        else
        {
            push_integer(S, static_cast<Integer>(pos));
        }

        return 1;
    }

    static int str_format(State* S)
    {
        const auto format_str = check_string(S, 0);
        const int nargs = get_top(S) - 1;

        std::vector<format_arg> args;
        args.reserve(static_cast<size_t>(nargs));

        for (int i = 0; i < nargs; ++i)
        {
            const int stack_idx = i + 1;
            switch (type(S, stack_idx))
            {
                case Type::kNil:
                    args.emplace_back("nil");
                    break;
                case Type::kBoolean:
                    args.emplace_back(to_boolean(S, stack_idx));
                    break;
                case Type::kInteger:
                    args.emplace_back(to_integer(S, stack_idx));
                    break;
                case Type::kNumber:
                    args.emplace_back(to_number(S, stack_idx));
                    break;
                case Type::kString:
                    args.emplace_back(to_string(S, stack_idx));
                    break;
                case Type::kUserdata:
                case Type::kTable:
                case Type::kClosure:
                case Type::kCFunction:
                {
                    const auto value_idx = resolve_index(S, stack_idx);
                    const auto& val = S->stack[static_cast<size_t>(value_idx)];
                    const auto str_val = vm_tostring(S, val, S->call_stack.back());
                    if (str_val.is_nil())
                    {
                        args.emplace_back("nil");
                    }
                    else if (str_val.is_string())
                    {
                        args.emplace_back(str_val.get_string()->view());
                    }
                    else
                    {
                        args.emplace_back("<invalid>");
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        try
        {
            std::string result = vformat(format_str, args);
            push_string(S, result);
        }
        catch (const std::exception& e)
        {
            error(S, e.what());
        }

        return 1;
    }

    // string.split(s, sep) - splits string by separator (simple, no patterns)
    static int str_split(State* S)
    {
        auto str = check_string(S, 0);
        auto sep = check_string(S, 1);

        table_new(S);
        const int table_abs_idx = get_top(S) - 1;

        if (sep.empty())
        {
            // If no separator, return table with original string
            push_integer(S, 0);
            push_string(S, str);
            table_rawset(S, table_abs_idx);
            return 1;
        }

        size_t start = 0;
        size_t pos;
        Integer idx = 0;

        while ((pos = str.find(sep, start)) != std::string_view::npos)
        {
            const std::string_view part = str.substr(start, pos - start);
            auto* part_str = gc_new_string(S, part);

            push_integer(S, idx++);
            S->stack.push_back(S, Value(part_str));
            table_rawset(S, table_abs_idx);

            start = pos + sep.length();
        }

        // Add remaining part
        const std::string_view part = str.substr(start);
        auto* part_str = gc_new_string(S, part);

        push_integer(S, idx);
        S->stack.push_back(S, Value(part_str));
        table_rawset(S, table_abs_idx);

        return 1;
    }

    void load_lib_string(State* S)
    {
        static constexpr ModuleReg string_funcs[] = {
            { "len", str_len },
            { "sub", str_sub },
            { "upper", str_upper },
            { "lower", str_lower },
            { "reverse", str_reverse },
            { "char", str_char },
            { "byte", str_byte },
            { "rep", str_rep },
            { "find", str_find },
            { "format", str_format },
            { "split", str_split },
        };

        ModuleDef string_module = { .funcs = string_funcs };

        create_module(S, "string", string_module);
    }

} // namespace behl
