#include "behl.hpp"
#include "common/format.hpp"
#include "common/vector.hpp"
#include "gc/gc.hpp"
#include "gc/gco_string.hpp"
#include "state.hpp"

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

        auto* result = gc_new_string(S, std::string_view(buffer.data(), buffer.size()));
        S->stack.push_back(S, Value(result));

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

        auto* result = gc_new_string(S, std::string_view(buffer.data(), buffer.size()));
        S->stack.push_back(S, Value(result));

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

    // string.format(fmt, ...) - basic string formatting using format
    static int str_format(State* S)
    {
        check_type(S, 0, Type::kString);

        auto fmt = check_string(S, 0);
        const int nargs = get_top(S) - 1;

        // Build result in a temporary buffer
        AutoVector<char> result_buffer(S);
        result_buffer.reserve(fmt.length() * 2);

        size_t fmt_pos = 0;
        int arg_idx = 0;

        while (fmt_pos < fmt.length())
        {
            // Look for {}
            if (fmt_pos + 1 < fmt.length() && fmt[fmt_pos] == '{' && fmt[fmt_pos + 1] == '}')
            {
                if (arg_idx < nargs)
                {
                    // Format the argument
                    const int stack_idx = arg_idx + 1;
                    char temp_buffer[64];
                    const char* replacement = nullptr;
                    size_t replacement_len = 0;

                    switch (type(S, stack_idx))
                    {
                        case Type::kNil:
                            replacement = "nil";
                            replacement_len = 3;
                            break;
                        case Type::kBoolean:
                            if (to_boolean(S, stack_idx))
                            {
                                replacement = "true";
                                replacement_len = 4;
                            }
                            else
                            {
                                replacement = "false";
                                replacement_len = 5;
                            }
                            break;
                        case Type::kInteger:
                        {
                            auto formatted = format("{}", to_integer(S, stack_idx));
                            replacement_len = formatted.length();
                            std::memcpy(temp_buffer, formatted.data(), replacement_len);
                            replacement = temp_buffer;
                            break;
                        }
                        case Type::kNumber:
                        {
                            auto formatted = format("{}", to_number(S, stack_idx));
                            replacement_len = formatted.length();
                            std::memcpy(temp_buffer, formatted.data(), replacement_len);
                            replacement = temp_buffer;
                            break;
                        }
                        case Type::kString:
                        {
                            auto str_val = to_string(S, stack_idx);
                            replacement = str_val.data();
                            replacement_len = str_val.length();
                            break;
                        }
                        default:
                            replacement = "?";
                            replacement_len = 1;
                            break;
                    }

                    if (replacement)
                    {
                        for (size_t i = 0; i < replacement_len; ++i)
                        {
                            result_buffer.push_back(replacement[i]);
                        }
                    }

                    arg_idx++;
                }
                fmt_pos += 2;
            }
            else
            {
                result_buffer.push_back(fmt[fmt_pos]);
                fmt_pos++;
            }
        }

        auto* result = gc_new_string(S, std::string_view(result_buffer.data(), result_buffer.size()));
        S->stack.push_back(S, Value(result));
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

    void load_lib_string(State* S, bool make_global)
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

        create_module(S, "string", string_module, make_global);
    }

} // namespace behl
