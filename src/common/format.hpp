#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace behl
{
    // Compile-time string literal wrapper (C++20 NTTP)
    template<size_t N>
    struct format_string_literal
    {
        char data[N];

        constexpr format_string_literal(const char (&str)[N])
        {
            for (size_t i = 0; i < N; ++i)
            {
                data[i] = str[i];
            }
        }

        constexpr std::string_view view() const
        {
            return std::string_view(data, N - 1);
        }
    };

    struct format_spec
    {
        enum class type : char
        {
            none = 0,
            decimal = 'd',
            hex_lower = 'x',
            hex_upper = 'X',
            float_fixed = 'f',
            float_exp = 'e',
            float_general = 'g'
        };

        int width = 0;
        int precision = -1;
        int width_arg_index = -1;
        int precision_arg_index = -1;
        type spec_type = type::none;
        char fill = ' ';
        char align = '<';
        bool dynamic_width = false;
        bool dynamic_precision = false;
        bool explicit_align = false;

        constexpr format_spec() = default;
    };

    struct format_part
    {
        std::string_view literal;
        size_t arg_index = static_cast<size_t>(-1);
        format_spec spec;
        bool is_literal = true;

        constexpr format_part() = default;
        constexpr format_part(std::string_view lit)
            : literal(lit)
            , is_literal(true)
        {
        }
    };

    template<size_t N>
    struct format_parts
    {
        std::array<format_part, N> parts{};
        size_t count = 0;
        size_t arg_count = 0;

        constexpr format_parts() = default;
    };

    constexpr format_spec parse_format_spec(std::string_view spec_str)
    {
        format_spec spec;
        if (spec_str.empty())
        {
            return spec;
        }

        size_t i = 0;

        if (i < spec_str.size() && (spec_str[i] == '<' || spec_str[i] == '>' || spec_str[i] == '^'))
        {
            spec.align = spec_str[i++];
            spec.explicit_align = true;
        }

        // Check for dynamic width: {} or {N}
        if (i < spec_str.size() && spec_str[i] == '{')
        {
            ++i; // consume '{'
            if (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
            {
                // Parse indexed dynamic width: {N}
                int index = 0;
                while (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
                {
                    index = index * 10 + (spec_str[i++] - '0');
                }
                spec.width_arg_index = index;
                spec.dynamic_width = true;
            }
            else if (i < spec_str.size() && spec_str[i] == '}')
            {
                // Sequential dynamic width: {}
                spec.dynamic_width = true;
            }

            if (i < spec_str.size() && spec_str[i] == '}')
            {
                ++i; // consume '}'
            }
        }
        else if (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
        {
            int width = 0;
            while (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
            {
                width = width * 10 + (spec_str[i++] - '0');
            }
            spec.width = width;
        }

        // Check for dynamic precision: .{} or .{N}
        if (i < spec_str.size() && spec_str[i] == '.')
        {
            ++i;
            if (i < spec_str.size() && spec_str[i] == '{')
            {
                ++i; // consume '{'
                if (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
                {
                    // Parse indexed dynamic precision: .{N}
                    int index = 0;
                    while (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
                    {
                        index = index * 10 + (spec_str[i++] - '0');
                    }
                    spec.precision_arg_index = index;
                    spec.dynamic_precision = true;
                }
                else if (i < spec_str.size() && spec_str[i] == '}')
                {
                    // Sequential dynamic precision: .{}
                    spec.dynamic_precision = true;
                }

                if (i < spec_str.size() && spec_str[i] == '}')
                {
                    ++i; // consume '}'
                }
            }
            else if (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
            {
                int precision = 0;
                while (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
                {
                    precision = precision * 10 + (spec_str[i++] - '0');
                }
                spec.precision = precision;
            }
        }

        if (i < spec_str.size())
        {
            char type_char = spec_str[i];
            switch (type_char)
            {
                case 'd':
                    spec.spec_type = format_spec::type::decimal;
                    break;
                case 'x':
                    spec.spec_type = format_spec::type::hex_lower;
                    break;
                case 'X':
                    spec.spec_type = format_spec::type::hex_upper;
                    break;
                case 'f':
                    spec.spec_type = format_spec::type::float_fixed;
                    break;
                case 'e':
                    spec.spec_type = format_spec::type::float_exp;
                    break;
                case 'g':
                    spec.spec_type = format_spec::type::float_general;
                    break;
            }
        }

        return spec;
    }

    constexpr size_t count_format_parts(std::string_view fmt)
    {
        size_t count = 0;
        size_t i = 0;
        size_t literal_start = 0;

        while (i < fmt.size())
        {
            if (fmt[i] == '{')
            {
                if (i + 1 < fmt.size() && fmt[i + 1] == '{')
                {
                    i += 2;
                    continue;
                }

                if (i > literal_start)
                {
                    ++count;
                }

                while (i < fmt.size() && fmt[i] != '}')
                {
                    ++i;
                }

                if (i >= fmt.size())
                {
                    throw std::runtime_error("unmatched '{' in format string");
                }

                ++count;
                ++i;
                literal_start = i;
            }
            else if (fmt[i] == '}')
            {
                if (i + 1 < fmt.size() && fmt[i + 1] == '}')
                {
                    i += 2;
                    continue;
                }
                throw std::runtime_error("unmatched '}' in format string");
            }
            else
            {
                ++i;
            }
        }

        if (i > literal_start)
        {
            ++count;
        }

        return count > 0 ? count : 1;
    }

    template<size_t N>
    constexpr format_parts<N> parse_format_string_sized(std::string_view fmt)
    {
        format_parts<N> result;
        size_t i = 0;
        size_t literal_start = 0;
        size_t arg_index = 0;

        while (i < fmt.size())
        {
            if (fmt[i] == '{')
            {
                if (i + 1 < fmt.size() && fmt[i + 1] == '{')
                {
                    i += 2;
                    continue;
                }

                if (i > literal_start)
                {
                    result.parts[result.count++] = format_part(fmt.substr(literal_start, i - literal_start));
                }

                size_t spec_start = i + 1;
                while (i < fmt.size() && fmt[i] != '}')
                {
                    ++i;
                }

                if (i >= fmt.size())
                {
                    throw std::runtime_error("unmatched '{' in format string");
                }

                std::string_view spec_str = fmt.substr(spec_start, i - spec_start);

                // Parse argument index if present
                size_t current_arg_index = arg_index;
                size_t colon_pos = spec_str.find(':');

                if (!spec_str.empty())
                {
                    std::string_view index_part = (colon_pos != std::string_view::npos) ? spec_str.substr(0, colon_pos)
                                                                                        : spec_str;

                    // Check if it's a numeric index
                    if (!index_part.empty() && index_part[0] >= '0' && index_part[0] <= '9')
                    {
                        size_t parsed_index = 0;
                        for (char c : index_part)
                        {
                            if (c >= '0' && c <= '9')
                            {
                                parsed_index = parsed_index * 10 + static_cast<size_t>(c - '0');
                            }
                            else
                            {
                                break;
                            }
                        }
                        current_arg_index = parsed_index;

                        // Extract format spec after colon (if any)
                        if (colon_pos != std::string_view::npos && colon_pos + 1 < spec_str.size())
                        {
                            spec_str = spec_str.substr(colon_pos + 1);
                        }
                        else
                        {
                            spec_str = std::string_view();
                        }
                    }
                    else if (colon_pos != std::string_view::npos)
                    {
                        // No index, just spec after colon (e.g., "{:.2f}")
                        spec_str = spec_str.substr(colon_pos + 1);
                        current_arg_index = arg_index++;
                    }
                    else
                    {
                        // No colon, no numeric index - auto-increment
                        current_arg_index = arg_index++;
                        spec_str = std::string_view();
                    }
                }
                else
                {
                    // Empty spec - auto-increment
                    current_arg_index = arg_index++;
                }

                format_part part;
                part.is_literal = false;
                part.arg_index = current_arg_index;
                part.spec = parse_format_spec(spec_str);
                result.parts[result.count++] = part;

                ++i;
                literal_start = i;
            }
            else if (fmt[i] == '}')
            {
                if (i + 1 < fmt.size() && fmt[i + 1] == '}')
                {
                    i += 2;
                    continue;
                }
                throw std::runtime_error("unmatched '}' in format string");
            }
            else
            {
                ++i;
            }
        }

        if (i > literal_start)
        {
            result.parts[result.count++] = format_part(fmt.substr(literal_start, i - literal_start));
        }

        result.arg_count = arg_index;
        return result;
    }

    class format_arg
    {
    public:
        using value_type = std::variant<std::monostate, bool, long long, double, std::string_view, std::string, const void*>;

        constexpr format_arg()
            : value_(std::monostate{})
        {
        }
        constexpr format_arg(bool v)
            : value_(v)
        {
        }
        constexpr format_arg(int v)
            : value_(static_cast<long long>(v))
        {
        }
        constexpr format_arg(long v)
            : value_(static_cast<long long>(v))
        {
        }
        constexpr format_arg(long long v)
            : value_(v)
        {
        }
        constexpr format_arg(unsigned int v)
            : value_(static_cast<long long>(v))
        {
        }
        constexpr format_arg(unsigned long v)
            : value_(static_cast<long long>(v))
        {
        }
        constexpr format_arg(unsigned long long v)
            : value_(static_cast<long long>(v))
        {
        }
        constexpr format_arg(float v)
            : value_(static_cast<double>(v))
        {
        }
        constexpr format_arg(double v)
            : value_(v)
        {
        }
        constexpr format_arg(const char* v)
            : value_(std::string_view(v))
        {
        }
        constexpr format_arg(std::string_view v)
            : value_(v)
        {
        }
        format_arg(std::string v)
            : value_(std::move(v))
        {
        }
        constexpr format_arg(const void* v)
            : value_(v)
        {
        }
        constexpr format_arg(void* v)
            : value_(static_cast<const void*>(v))
        {
        }

        constexpr const value_type& value() const
        {
            return value_;
        }

    private:
        value_type value_;
    };

    namespace detail
    {
        inline std::string to_hex(long long value, bool uppercase)
        {
            if (value == 0)
            {
                return "0";
            }

            const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
            std::string result;
            auto uvalue = static_cast<unsigned long long>(value);

            while (uvalue > 0)
            {
                result += digits[uvalue % 16];
                uvalue /= 16;
            }

            std::reverse(result.begin(), result.end());
            return result;
        }

        inline std::string constexpr_double_to_string(double value, size_t precision)
        {
            // Simple double to string for fixed precision
            bool negative = value < 0;
            if (negative)
            {
                value = -value;
            }

            // Round to precision
            double multiplier = 1.0;
            for (size_t i = 0; i < precision; ++i)
            {
                multiplier *= 10.0;
            }
            value = (value * multiplier + 0.5); // Round
            long long rounded = static_cast<long long>(value);

            // Split into integer and fractional parts
            long long int_part = rounded;
            for (size_t i = 0; i < precision; ++i)
            {
                int_part /= 10;
            }
            long long frac_part = rounded - static_cast<long long>(static_cast<double>(int_part) * multiplier);

            std::string result;
            if (negative)
            {
                result += '-';
            }

            // Convert integer part
            if (int_part == 0)
            {
                result += '0';
            }
            else
            {
                std::string int_str;
                long long temp = int_part;
                while (temp > 0)
                {
                    int_str += static_cast<char>('0' + (temp % 10));
                    temp /= 10;
                }
                std::reverse(int_str.begin(), int_str.end());
                result += int_str;
            }

            // Only add decimal point and fraction if precision > 0
            if (precision > 0)
            {
                result += '.';

                // Convert fractional part with leading zeros
                std::string frac_str;
                for (size_t i = 0; i < precision; ++i)
                {
                    frac_str += static_cast<char>('0' + (frac_part % 10));
                    frac_part /= 10;
                }
                std::reverse(frac_str.begin(), frac_str.end());
                result += frac_str;
            }

            return result;
        }

        inline std::string to_string_with_precision(double value, int precision)
        {
            return constexpr_double_to_string(value, static_cast<size_t>(precision));
        }

        inline std::string apply_width_and_align(std::string str, int width, char align, char fill)
        {
            if (width <= 0 || static_cast<size_t>(width) <= str.size())
            {
                return str;
            }

            size_t padding = static_cast<size_t>(width) - str.size();
            switch (align)
            {
                case '<':
                    return str + std::string(padding, fill);
                case '>':
                    return std::string(padding, fill) + str;
                case '^':
                {
                    size_t left_pad = padding / 2;
                    size_t right_pad = padding - left_pad;
                    return std::string(left_pad, fill) + str + std::string(right_pad, fill);
                }
                default:
                    return str;
            }
        }
    } // namespace detail

    inline std::string format_value(const format_arg& arg, const format_spec& spec)
    {
        return std::visit(
            [&spec](auto&& value) -> std::string {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    return "";
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    std::string result = value ? "true" : "false";
                    char align = spec.align == '<' && spec.width > 0 ? '<' : spec.align;
                    return detail::apply_width_and_align(result, spec.width, align, spec.fill);
                }
                else if constexpr (std::is_same_v<T, long long>)
                {
                    std::string result;

                    switch (spec.spec_type)
                    {
                        case format_spec::type::hex_lower:
                            result = detail::to_hex(value, false);
                            break;
                        case format_spec::type::hex_upper:
                            result = detail::to_hex(value, true);
                            break;
                        case format_spec::type::decimal:
                        case format_spec::type::none:
                        default:
                            result = std::to_string(value);
                            break;
                    }

                    char align = spec.align;
                    if (!spec.explicit_align && align == '<' && spec.width > 0)
                    {
                        align = '>';
                    }
                    return detail::apply_width_and_align(result, spec.width, align, spec.fill);
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    std::string result;

                    if (spec.precision != -1)
                    {
                        result = detail::to_string_with_precision(value, spec.precision);
                    }
                    else
                    {
                        result = std::to_string(value);
                        while (!result.empty() && result.back() == '0')
                        {
                            result.pop_back();
                        }
                        if (!result.empty() && result.back() == '.')
                        {
                            result.pop_back();
                        }
                        if (result.empty())
                        {
                            result = "0";
                        }
                    }

                    char align = spec.align;
                    if (!spec.explicit_align && align == '<' && spec.width > 0)
                    {
                        align = '>';
                    }
                    return detail::apply_width_and_align(result, spec.width, align, spec.fill);
                }
                else if constexpr (std::is_same_v<T, std::string_view>)
                {
                    std::string result(value);
                    return detail::apply_width_and_align(result, spec.width, spec.align, spec.fill);
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    return detail::apply_width_and_align(value, spec.width, spec.align, spec.fill);
                }
                else if constexpr (std::is_same_v<T, const void*>)
                {
                    // Pointer formatting - convert to hex address
                    auto addr = reinterpret_cast<uintptr_t>(value);
                    return "0x" + detail::to_hex(static_cast<long long>(addr), false);
                }
                else
                {
                    return "";
                }
            },
            arg.value());
    }

    template<typename... Args>
    struct format_string
    {
        const char* str;
        size_t len;

        template<size_t N>
        constexpr format_string(const char (&s)[N])
            : str(s)
            , len(N - 1)
        {
        }

        constexpr std::string_view view() const
        {
            return std::string_view(str, len);
        }
        constexpr operator std::string_view() const
        {
            return view();
        }
    };

    namespace detail
    {
        inline std::string process_literal(std::string_view lit)
        {
            std::string result;
            result.reserve(lit.size());
            for (size_t i = 0; i < lit.size(); ++i)
            {
                if (lit[i] == '{' && i + 1 < lit.size() && lit[i + 1] == '{')
                {
                    result += '{';
                    ++i;
                }
                else if (lit[i] == '}' && i + 1 < lit.size() && lit[i + 1] == '}')
                {
                    result += '}';
                    ++i;
                }
                else
                {
                    result += lit[i];
                }
            }
            return result;
        }

        template<typename T>
        inline std::string format_arg_value(T&& arg, const format_spec& spec)
        {
            return format_value(format_arg(std::forward<T>(arg)), spec);
        }

        template<typename Tuple, size_t... Is>
        inline int get_integer_arg(const Tuple& args, size_t index, std::index_sequence<Is...>)
        {
            int result = 0;
            [[maybe_unused]] auto extract = [&]<size_t I>() {
                if (index == I)
                {
                    auto&& arg = std::get<I>(args);
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_integral_v<T>)
                    {
                        result = static_cast<int>(arg);
                    }
                    else if constexpr (std::is_floating_point_v<T>)
                    {
                        result = static_cast<int>(arg);
                    }
                }
            };
            (extract.template operator()<Is>(), ...);
            return result;
        }

        // Dynamic array-based format impl (determines size at runtime)
        template<typename Tuple, size_t... Is>
        std::string format_impl_dynamic(std::string_view fmt, const Tuple& args, std::index_sequence<Is...>)
        {
            std::string result;
            size_t i = 0;
            size_t arg_index = 0;

            while (i < fmt.size())
            {
                if (fmt[i] == '{')
                {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '{')
                    {
                        result += '{';
                        i += 2;
                        continue;
                    }

                    size_t spec_start = i + 1;
                    size_t brace_end = i + 1;
                    int brace_depth = 0;
                    while (brace_end < fmt.size())
                    {
                        if (fmt[brace_end] == '{')
                        {
                            ++brace_depth;
                        }
                        else if (fmt[brace_end] == '}')
                        {
                            if (brace_depth == 0)
                            {
                                break;
                            }
                            --brace_depth;
                        }
                        ++brace_end;
                    }

                    if (brace_end >= fmt.size())
                    {
                        throw std::runtime_error("unmatched '{' in format string");
                    }

                    std::string_view spec_str = fmt.substr(spec_start, brace_end - spec_start);

                    // Parse argument index if present
                    size_t current_arg_index = arg_index;
                    size_t colon_pos = spec_str.find(':');

                    if (!spec_str.empty())
                    {
                        std::string_view index_part = (colon_pos != std::string_view::npos) ? spec_str.substr(0, colon_pos)
                                                                                            : spec_str;

                        // Check if it's a numeric index
                        if (!index_part.empty() && index_part[0] >= '0' && index_part[0] <= '9')
                        {
                            size_t parsed_index = 0;
                            for (char c : index_part)
                            {
                                if (c >= '0' && c <= '9')
                                {
                                    parsed_index = parsed_index * 10 + static_cast<size_t>(c - '0');
                                }
                                else
                                {
                                    break;
                                }
                            }
                            current_arg_index = parsed_index;

                            // Extract format spec after colon (if any)
                            if (colon_pos != std::string_view::npos && colon_pos + 1 < spec_str.size())
                            {
                                spec_str = spec_str.substr(colon_pos + 1);
                            }
                            else
                            {
                                spec_str = std::string_view();
                            }
                        }
                        else if (colon_pos != std::string_view::npos)
                        {
                            // No index, just spec after colon (e.g., "{:.2f}")
                            spec_str = spec_str.substr(colon_pos + 1);
                            current_arg_index = arg_index++;
                        }
                        else
                        {
                            // No colon, no numeric index - auto-increment
                            current_arg_index = arg_index++;
                            spec_str = std::string_view();
                        }
                    }
                    else
                    {
                        // Empty spec - auto-increment
                        current_arg_index = arg_index++;
                    }

                    format_spec spec = parse_format_spec(spec_str);

                    // Handle dynamic width and precision
                    if (spec.dynamic_width)
                    {
                        constexpr size_t arg_count = sizeof...(Is);
                        size_t width_index;

                        // Use indexed or sequential width
                        if (spec.width_arg_index != -1)
                        {
                            width_index = static_cast<size_t>(spec.width_arg_index);
                        }
                        else
                        {
                            width_index = arg_index++;
                        }

                        if (width_index >= arg_count)
                        {
                            throw std::runtime_error("width argument index out of range");
                        }
                        spec.width = get_integer_arg(args, width_index, std::index_sequence<Is...>{});
                    }

                    if (spec.dynamic_precision)
                    {
                        constexpr size_t arg_count = sizeof...(Is);
                        size_t precision_index;

                        // Use indexed or sequential precision
                        if (spec.precision_arg_index != -1)
                        {
                            precision_index = static_cast<size_t>(spec.precision_arg_index);
                        }
                        else
                        {
                            precision_index = arg_index++;
                        }

                        if (precision_index >= arg_count)
                        {
                            throw std::runtime_error("precision argument index out of range");
                        }
                        spec.precision = get_integer_arg(args, precision_index, std::index_sequence<Is...>{});
                    }

                    // Bounds check
                    constexpr size_t arg_count = sizeof...(Is);
                    if (current_arg_index >= arg_count)
                    {
                        throw std::runtime_error("not enough arguments for format string");
                    }

                    (void)((current_arg_index == Is ? (result += format_arg_value(std::get<Is>(args), spec), true) : false)
                        || ...);

                    i = brace_end + 1;
                }
                else if (fmt[i] == '}')
                {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '}')
                    {
                        result += '}';
                        i += 2;
                        continue;
                    }
                    throw std::runtime_error("unmatched '}' in format string");
                }
                else
                {
                    result += fmt[i++];
                }
            }

            return result;
        }

        template<typename ArgsTuple, size_t... Is>
        std::string format_parts_tuple(const auto& parts_tuple, const ArgsTuple& args, std::index_sequence<Is...>)
        {
            std::string result;
            auto process_part = [&](const auto& part) {
                if (part.is_literal)
                {
                    result += process_literal(part.literal);
                }
                else
                {
                    ((part.arg_index == Is ? (result += format_arg_value(std::get<Is>(args), part.spec), true) : false) || ...);
                }
            };
            (process_part(std::get<Is>(parts_tuple)), ...);
            return result;
        }

        template<size_t N, typename Tuple, size_t... Is>
        std::string format_impl(const format_parts<N>& parts, const Tuple& args, std::index_sequence<Is...>)
        {
            std::string result;
            for (size_t i = 0; i < parts.count; ++i)
            {
                const auto& part = parts.parts[i];
                if (part.is_literal)
                {
                    result += process_literal(part.literal);
                }
                else
                {
                    (void)((part.arg_index == Is ? (result += format_arg_value(std::get<Is>(args), part.spec), true) : false)
                        || ...);
                }
            }
            return result;
        }

        template<size_t N>
        consteval auto parse_to_tuple(format_string_literal<N> fmt)
        {
            constexpr size_t part_count = count_format_parts(fmt.view());
            constexpr auto parsed = parse_format_string_sized<part_count>(fmt.view());

            // Convert array to tuple
            return [&]<size_t... Is>(std::index_sequence<Is...>) { return std::make_tuple(parsed.parts[Is]...); }(
                       std::make_index_sequence<parsed.count>{});
        }

    } // namespace detail

    template<format_string_literal Fmt, typename... Args>
    std::string format(Args&&... args)
    {
        constexpr size_t part_count = count_format_parts(Fmt.view());
        constexpr auto parts = parse_format_string_sized<part_count>(Fmt.view());
        auto args_tuple = std::forward_as_tuple(args...);
        return detail::format_impl(parts, args_tuple, std::index_sequence_for<Args...>{});
    }

    template<typename... Args>
    std::string format(format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        auto args_tuple = std::forward_as_tuple(args...);
        return detail::format_impl_dynamic(fmt.view(), args_tuple, std::index_sequence_for<Args...>{});
    }

    std::string vformat(std::string_view fmt, const std::vector<format_arg>& args);

} // namespace behl
