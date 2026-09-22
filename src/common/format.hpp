#pragma once

#include "common/ascii.hpp"
#include "common/charconv.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#if defined(__cpp_lib_constexpr_string) && __cpp_lib_constexpr_string >= 201907L
#    define BEHL_CONSTEXPR_STRING constexpr
#else
#    define BEHL_CONSTEXPR_STRING
#endif

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

    inline constexpr int kMaxFormatNumber = 1 << 16;

    constexpr int parse_bounded_number(std::string_view spec_str, size_t& i, const char* message)
    {
        int value = 0;
        while (i < spec_str.size() && spec_str[i] >= '0' && spec_str[i] <= '9')
        {
            const int digit = spec_str[i] - '0';
            if (value > (kMaxFormatNumber - digit) / 10)
            {
                throw std::runtime_error(message);
            }
            value = value * 10 + digit;
            ++i;
        }
        return value;
    }

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
                spec.width_arg_index = parse_bounded_number(spec_str, i, "format width argument index out of range");
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
            spec.width = parse_bounded_number(spec_str, i, "format width out of range");
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
                    spec.precision_arg_index = parse_bounded_number(
                        spec_str, i, "format precision argument index out of range");
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
                spec.precision = parse_bounded_number(spec_str, i, "format precision out of range");
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
        using value_type = std::variant<std::monostate, bool, long long, double, std::string_view, const void*>;

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
        template<typename Out, typename T>
        constexpr void append_integer(Out& out, T value, int base)
        {
            std::array<char, 72> buffer{};
            const auto converted = behl::to_chars(buffer.data(), buffer.data() + buffer.size(), value, base);
            out.append(buffer.data(), static_cast<size_t>(converted.ptr - buffer.data()));
        }

        template<typename Out>
        constexpr void append_hex(Out& out, unsigned long long value, bool uppercase)
        {
            const size_t start = out.size();
            append_integer(out, value, 16);
            if (uppercase)
            {
                for (size_t i = start; i < out.size(); ++i)
                {
                    out[i] = ascii_to_upper(out[i]);
                }
            }
        }

        template<typename Out>
        constexpr void append_shortest(Out& out, double value)
        {
            std::array<char, 64> buffer{};
            const auto converted = behl::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            out.append(buffer.data(), static_cast<size_t>(converted.ptr - buffer.data()));
        }

        template<typename Out>
        constexpr void append_fixed(Out& out, double value, size_t precision)
        {
            constexpr double kInt64Limit = 9223372036854775808.0;
            constexpr size_t kMaxExactPowerOfTen = 19;

            const bool negative = value < 0;
            const double magnitude = negative ? -value : value;

            if (!(magnitude < kInt64Limit))
            {
                append_shortest(out, value);
                return;
            }

            // Round to precision
            double multiplier = 1.0;
            size_t scaled_digits = 0;
            while (scaled_digits < precision && magnitude * (multiplier * 10.0) + 0.5 < kInt64Limit)
            {
                multiplier *= 10.0;
                ++scaled_digits;
            }
            const auto rounded = static_cast<unsigned long long>(magnitude * multiplier + 0.5);

            // Split into integer and fractional parts
            unsigned long long int_part = 0;
            unsigned long long frac_part = rounded;
            if (scaled_digits < kMaxExactPowerOfTen)
            {
                unsigned long long power = 1;
                for (size_t i = 0; i < scaled_digits; ++i)
                {
                    power *= 10;
                }
                int_part = rounded / power;
                frac_part = rounded % power;
            }

            if (negative)
            {
                out += '-';
            }

            // Convert integer part
            append_integer(out, int_part, 10);

            // Only add decimal point and fraction if precision > 0
            if (precision > 0)
            {
                out += '.';

                // Convert fractional part with leading zeros
                if (scaled_digits > 0)
                {
                    std::array<char, 24> digits{};
                    const auto converted = behl::to_chars(digits.data(), digits.data() + digits.size(), frac_part);
                    const auto length = static_cast<size_t>(converted.ptr - digits.data());
                    out.append(scaled_digits - length, '0');
                    out.append(digits.data(), length);
                }
                out.append(precision - scaled_digits, '0');
            }
        }

        template<typename Out>
        constexpr void pad_to_width(Out& out, size_t start, int width, char align, char fill)
        {
            const size_t length = out.size() - start;
            if (width <= 0 || static_cast<size_t>(width) <= length)
            {
                return;
            }

            const size_t padding = static_cast<size_t>(width) - length;
            switch (align)
            {
                case '<':
                    out.append(padding, fill);
                    break;
                case '>':
                    out.insert(start, padding, fill);
                    break;
                case '^':
                {
                    const size_t left_pad = padding / 2;
                    out.insert(start, left_pad, fill);
                    out.append(padding - left_pad, fill);
                    break;
                }
                default:
                    break;
            }
        }

        constexpr char numeric_align(const format_spec& spec)
        {
            if (!spec.explicit_align && spec.align == '<' && spec.width > 0)
            {
                return '>';
            }
            return spec.align;
        }

        template<typename Out>
        constexpr void append_literal(Out& out, std::string_view lit)
        {
            size_t run_start = 0;
            for (size_t i = 0; i < lit.size(); ++i)
            {
                if ((lit[i] == '{' || lit[i] == '}') && i + 1 < lit.size() && lit[i + 1] == lit[i])
                {
                    out.append(lit.substr(run_start, i + 1 - run_start));
                    ++i;
                    run_start = i + 1;
                }
            }
            out.append(lit.substr(run_start));
        }
    } // namespace detail

    template<typename Out>
    constexpr void format_value_to(Out& out, const format_arg& arg, const format_spec& spec)
    {
        std::visit(
            [&out, &spec](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                const size_t start = out.size();

                if constexpr (std::is_same_v<T, bool>)
                {
                    out.append(value ? std::string_view("true") : std::string_view("false"));
                    detail::pad_to_width(out, start, spec.width, spec.align, spec.fill);
                }
                else if constexpr (std::is_same_v<T, long long>)
                {
                    switch (spec.spec_type)
                    {
                        case format_spec::type::hex_lower:
                            detail::append_hex(out, static_cast<unsigned long long>(value), false);
                            break;
                        case format_spec::type::hex_upper:
                            detail::append_hex(out, static_cast<unsigned long long>(value), true);
                            break;
                        case format_spec::type::decimal:
                        case format_spec::type::none:
                        default:
                            detail::append_integer(out, value, 10);
                            break;
                    }
                    detail::pad_to_width(out, start, spec.width, detail::numeric_align(spec), spec.fill);
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    if (spec.precision != -1)
                    {
                        detail::append_fixed(out, value, static_cast<size_t>(spec.precision));
                    }
                    else
                    {
                        detail::append_shortest(out, value);
                    }
                    detail::pad_to_width(out, start, spec.width, detail::numeric_align(spec), spec.fill);
                }
                else if constexpr (std::is_same_v<T, std::string_view>)
                {
                    out.append(value);
                    detail::pad_to_width(out, start, spec.width, spec.align, spec.fill);
                }
                else if constexpr (std::is_same_v<T, const void*>)
                {
                    // Pointer formatting - convert to hex address
                    out.append(std::string_view("0x"));
                    detail::append_hex(out, static_cast<unsigned long long>(std::bit_cast<uintptr_t>(value)), false);
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
        template<typename Tuple, size_t... Is>
        constexpr int get_integer_arg(const Tuple& args, size_t index, std::index_sequence<Is...>)
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
        BEHL_CONSTEXPR_STRING std::string format_impl_dynamic(std::string_view fmt, const Tuple& args, std::index_sequence<Is...>)
        {
            std::string result;
            result.reserve(fmt.size());
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

                    (void)((current_arg_index == Is ? (format_value_to(result, format_arg(std::get<Is>(args)), spec), true)
                                                    : false)
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
        BEHL_CONSTEXPR_STRING std::string format_parts_tuple(const auto& parts_tuple, const ArgsTuple& args, std::index_sequence<Is...>)
        {
            std::string result;
            auto process_part = [&](const auto& part) {
                if (part.is_literal)
                {
                    append_literal(result, part.literal);
                }
                else
                {
                    ((part.arg_index == Is ? (format_value_to(result, format_arg(std::get<Is>(args)), part.spec), true) : false)
                        || ...);
                }
            };
            (process_part(std::get<Is>(parts_tuple)), ...);
            return result;
        }

        template<format_string_literal Fmt>
        struct compiled_format
        {
            static constexpr size_t part_count = count_format_parts(Fmt.view());
            static constexpr auto parts = parse_format_string_sized<part_count>(Fmt.view());
        };

        template<format_string_literal Fmt, size_t I, typename Out, typename Tuple>
        constexpr void append_part(Out& out, const Tuple& args)
        {
            constexpr const format_part& part = compiled_format<Fmt>::parts.parts[I];
            if constexpr (part.is_literal)
            {
                append_literal(out, part.literal);
            }
            else if constexpr (part.arg_index < std::tuple_size_v<Tuple>)
            {
                format_value_to(out, format_arg(std::get<part.arg_index>(args)), part.spec);
            }
        }

        template<format_string_literal Fmt, typename Tuple, size_t... Ps>
        BEHL_CONSTEXPR_STRING std::string format_impl(const Tuple& args, std::index_sequence<Ps...>)
        {
            std::string result;
            result.reserve(Fmt.view().size());
            (append_part<Fmt, Ps>(result, args), ...);
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
    BEHL_CONSTEXPR_STRING std::string format(Args&&... args)
    {
        auto args_tuple = std::forward_as_tuple(args...);
        return detail::format_impl<Fmt>(args_tuple, std::make_index_sequence<detail::compiled_format<Fmt>::parts.count>{});
    }

    template<typename... Args>
    BEHL_CONSTEXPR_STRING std::string format(format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        auto args_tuple = std::forward_as_tuple(args...);
        return detail::format_impl_dynamic(fmt.view(), args_tuple, std::index_sequence_for<Args...>{});
    }

    std::string vformat(std::string_view fmt, const std::vector<format_arg>& args);

} // namespace behl
