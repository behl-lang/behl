#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace behl
{
    struct format_spec
    {
        enum class type : char
        {
            none = '\0',
            decimal = 'd',
            hex_lower = 'x',
            hex_upper = 'X',
            floating = 'f',
            string = 's',
            pointer = 'p',
            character = 'c'
        };

        type spec_type = type::none;
        int width = -1;
        int precision = -1;
        char fill = ' ';
        char align = '\0';
    };

    constexpr format_spec parse_spec(std::string_view spec)
    {
        format_spec result;
        if (spec.empty())
        {
            return result;
        }

        size_t pos = 0;

        // Check for alignment and fill: [[fill]align]
        // align can be '<' (left), '>' (right), '^' (center)
        if (pos + 1 < spec.size() && (spec[pos + 1] == '<' || spec[pos + 1] == '>' || spec[pos + 1] == '^'))
        {
            result.fill = spec[pos];
            result.align = spec[pos + 1];
            pos += 2;
        }
        else if (pos < spec.size() && (spec[pos] == '<' || spec[pos] == '>' || spec[pos] == '^'))
        {
            result.align = spec[pos];
            pos++;
        }

        // Check for width
        if (pos < spec.size() && spec[pos] >= '0' && spec[pos] <= '9')
        {
            int w = 0;
            while (pos < spec.size() && spec[pos] >= '0' && spec[pos] <= '9')
            {
                w = w * 10 + (spec[pos] - '0');
                pos++;
            }
            result.width = w;
        }

        // Check for precision
        if (pos < spec.size() && spec[pos] == '.')
        {
            pos++;
            int p = 0;
            while (pos < spec.size() && spec[pos] >= '0' && spec[pos] <= '9')
            {
                p = p * 10 + (spec[pos] - '0');
                pos++;
            }
            result.precision = p;
        }

        // Check for type (last char or next char)
        if (pos < spec.size())
        {
            char type_char = spec[pos];
            switch (type_char)
            {
                case 'd':
                    result.spec_type = format_spec::type::decimal;
                    break;
                case 'x':
                    result.spec_type = format_spec::type::hex_lower;
                    break;
                case 'X':
                    result.spec_type = format_spec::type::hex_upper;
                    break;
                case 'f':
                    result.spec_type = format_spec::type::floating;
                    break;
                case 's':
                    result.spec_type = format_spec::type::string;
                    break;
                case 'p':
                    result.spec_type = format_spec::type::pointer;
                    break;
                case 'c':
                    result.spec_type = format_spec::type::character;
                    break;
            }
        }

        return result;
    }

    template<typename T, int Base = 10>
    inline void int_to_string(T value, std::string& out, bool uppercase = false)
    {
        if (value == 0)
        {
            out += '0';
            return;
        }

        bool negative = false;
        if constexpr (std::is_signed_v<T>)
        {
            if (value < 0)
            {
                negative = true;
                value = -value;
            }
        }

        // Fast path for base 10 using std::to_chars
        if constexpr (Base == 10)
        {
            char buffer[32];
            using SignedT = std::make_signed_t<T>;
            auto result = std::to_chars(
                buffer, buffer + sizeof(buffer), negative ? static_cast<T>(-static_cast<SignedT>(value)) : value);
            if (result.ec == std::errc{})
            {
                out.append(buffer, static_cast<size_t>(result.ptr - buffer));
                return;
            }
        }

        // Fast path for hex using std::to_chars
        if constexpr (Base == 16)
        {
            char buffer[32];
            auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, 16);
            if (result.ec == std::errc{})
            {
                if (negative)
                {
                    out += '-';
                }
                // Handle uppercase if needed
                if (uppercase)
                {
                    for (char* p = buffer; p < result.ptr; ++p)
                    {
                        if (*p >= 'a' && *p <= 'f')
                        {
                            *p = *p - 'a' + 'A';
                        }
                    }
                }
                out.append(buffer, static_cast<size_t>(result.ptr - buffer));
                return;
            }
        }

        // Fallback for other bases
        constexpr size_t buf_size = 32;
        char buffer[buf_size];
        char* ptr = buffer + buf_size;

        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

        while (value > 0)
        {
            *--ptr = digits[value % Base];
            value /= Base;
        }

        if (negative)
        {
            *--ptr = '-';
        }

        out.append(ptr, static_cast<size_t>(buffer + buf_size - ptr));
    }

    inline void double_to_string(double value, std::string& out, int precision = -1)
    {
        // Special values
        if (std::isnan(value))
        {
            out += "nan";
            return;
        }
        if (std::isinf(value))
        {
            out += value < 0 ? "-inf" : "inf";
            return;
        }
        if (value == 0.0)
        {
            out += "0";
            return;
        }

        // Use std::to_chars - it's already highly optimized (uses Ryu internally in modern implementations)
        char buffer[64];
        std::to_chars_result result;

        if (precision < 0)
        {
            result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        }
        else
        {
            result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, precision);
        }

        if (result.ec == std::errc{})
        {
            out.append(buffer, static_cast<size_t>(result.ptr - buffer));
        }
        else
        {
            out += "error";
        }
    }

    // Forward declaration for formatter template
    template<typename T, typename CharT = char>
    struct formatter;

    namespace detail
    {
        // Concept to check if a formatter<T> specialization exists and is usable
        template<typename T>
        concept has_formatter = requires(formatter<T> f, std::string& out, const T& value, const format_spec& spec) {
            { f.format(value, out, spec) } -> std::same_as<void>;
        };

        template<typename T>
        inline void format_one(std::string& out, const T& value, const format_spec& spec)
        {
            // Format the value first into a temporary string if we need to apply width/alignment
            std::string temp;
            std::string* target = (spec.width > 0) ? &temp : &out;

            // Check if a custom formatter<T> exists
            if constexpr (has_formatter<T>)
            {
                formatter<T> f;
                f.format(value, *target, spec);
            }
            else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>)
            {
                if (spec.spec_type == format_spec::type::hex_lower)
                {
                    int_to_string<T, 16>(value, *target, false);
                }
                else if (spec.spec_type == format_spec::type::hex_upper)
                {
                    int_to_string<T, 16>(value, *target, true);
                }
                else
                {
                    int_to_string<T, 10>(value, *target, false);
                }
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                int precision = spec.precision >= 0 ? spec.precision : -1;
                double_to_string(static_cast<double>(value), *target, precision);
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                *target += value ? "true" : "false";
            }
            else if constexpr (std::is_same_v<T, char>)
            {
                *target += value;
            }
            else if constexpr (std::is_convertible_v<T, const char*>)
            {
                const char* str = value;
                *target += str ? str : "(null)";
            }
            else if constexpr (std::is_same_v<T, std::string_view>)
            {
                *target += value;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                *target += value;
            }
            else if constexpr (std::is_pointer_v<T>)
            {
                *target += "0x";
                int_to_string<uintptr_t, 16>(reinterpret_cast<uintptr_t>(value), *target, false);
            }
            else
            {
                *target += "(unsupported)";
            }

            // Apply width and alignment if needed
            if (spec.width > 0 && target == &temp)
            {
                size_t len = temp.size();
                if (len < static_cast<size_t>(spec.width))
                {
                    size_t padding = static_cast<size_t>(spec.width) - len;
                    char fill_char = spec.fill != '\0' ? spec.fill : ' ';
                    char align = spec.align != '\0' ? spec.align : '<'; // Default to left align

                    if (align == '<') // Left align
                    {
                        out += temp;
                        out.append(padding, fill_char);
                    }
                    else if (align == '>') // Right align
                    {
                        out.append(padding, fill_char);
                        out += temp;
                    }
                    else if (align == '^') // Center align
                    {
                        size_t left_pad = padding / 2;
                        size_t right_pad = padding - left_pad;
                        out.append(left_pad, fill_char);
                        out += temp;
                        out.append(right_pad, fill_char);
                    }
                    else
                    {
                        out += temp; // Fallback
                    }
                }
                else
                {
                    out += temp;
                }
            }
        }

        template<typename Tuple, size_t... Is>
        inline void format_arg_at_index(
            std::string& result, Tuple&& args, size_t index, const format_spec& spec, std::index_sequence<Is...>)
        {
            (void)((index == Is ? (format_one(result, std::get<Is>(args), spec), true) : false) || ...);
        }
    } // namespace detail

    template<typename... TArgs>
    struct basic_format_string
    {
    private:
        struct Segment
        {
            enum class Type
            {
                Text,
                Arg
            } type
                = Type::Text;

            int text_start = 0;
            int text_length = 0;

            uint16_t arg_index = 0;
            int16_t arg_width = 0;
            int16_t arg_precision = 0;
            char arg_type = 0;
            char arg_fill = 0;
            char arg_align = 0;
        };

        static constexpr size_t MAX_SEGMENTS = 64;
        std::array<Segment, MAX_SEGMENTS> segments{};
        size_t segment_count = 0;
        std::string_view str;

    public:
        template<size_t N>
        consteval basic_format_string(const char (&s)[N])
            : str(s, N - 1)
        {
            parse();
        }

        basic_format_string(const basic_format_string&) = default;
        basic_format_string& operator=(const basic_format_string&) = default;

        constexpr void parse()
        {
            size_t pos = 0;
            size_t arg_idx = 0;

            while (pos < str.size())
            {
                size_t brace = str.find_first_of("{}", pos);
                if (brace == std::string_view::npos)
                {
                    add_literal(pos, str.size() - pos);
                    break;
                }

                if (brace > pos)
                {
                    add_literal(pos, brace - pos);
                }

                char ch = str[brace];
                if (brace + 1 < str.size() && str[brace + 1] == ch)
                {
                    add_literal(brace, 1);
                    pos = brace + 2;
                    continue;
                }

                if (ch == '}')
                {
                    throw "Unmatched closing brace '}'";
                }

                // Found '{'
                size_t close = str.find('}', brace);
                if (close == std::string_view::npos)
                {
                    throw "Unclosed format placeholder";
                }

                std::string_view content = str.substr(brace + 1, close - brace - 1);
                format_spec spec;
                if (!content.empty() && content[0] == ':')
                {
                    spec = parse_spec(content.substr(1));
                }

                if (arg_idx >= sizeof...(TArgs))
                {
                    throw "Too many format placeholders for provided arguments";
                }

                add_arg(arg_idx, spec);
                arg_idx++;
                pos = close + 1;
            }
        }

        constexpr void add_literal(size_t start, size_t length)
        {
            if (segment_count >= MAX_SEGMENTS)
            {
                throw "Format string too complex (too many segments)";
            }
            if (start > std::numeric_limits<uint16_t>::max() || length > std::numeric_limits<uint16_t>::max())
            {
                throw "Format string too long";
            }

            segments[segment_count] = Segment{
                .type = Segment::Type::Text, .text_start = static_cast<int>(start), .text_length = static_cast<int>(length)
            };
            segment_count++;
        }

        constexpr void add_arg(size_t index, format_spec spec)
        {
            if (segment_count >= MAX_SEGMENTS)
            {
                throw "Format string too complex (too many segments)";
            }
            if (index > std::numeric_limits<uint16_t>::max())
            {
                throw "Too many arguments";
            }

            segments[segment_count] = Segment{ .type = Segment::Type::Arg,
                .arg_index = static_cast<uint16_t>(index),
                .arg_width = static_cast<int16_t>(spec.width),
                .arg_precision = static_cast<int16_t>(spec.precision),
                .arg_type = static_cast<char>(spec.spec_type),
                .arg_fill = spec.fill,
                .arg_align = spec.align };
            segment_count++;
        }

        template<typename... As>
        friend std::string format(basic_format_string<std::type_identity_t<As>...> fmt, As&&... Args);

        template<typename OutputIt, typename... As>
        friend OutputIt format_to(OutputIt out, basic_format_string<std::type_identity_t<As>...> fmt, As&&... Args);
    };

    template<typename... TArgs>
    using format_string = basic_format_string<std::type_identity_t<TArgs>...>;

    template<typename... TArgs>
    inline std::string format(format_string<TArgs...> fmt, TArgs&&... args)
    {
        using FmtType = format_string<TArgs...>;

        // Fast path for single argument with simple format
        if constexpr (sizeof...(TArgs) == 1)
        {
            if (fmt.segment_count == 2 && fmt.segments[1].type == FmtType::Segment::Type::Arg
                && fmt.segments[1].arg_precision < 0)
            {
                std::string result;
                result.reserve(static_cast<size_t>(fmt.segments[0].text_length) + 16);
                result.append(fmt.str.data() + fmt.segments[0].text_start, static_cast<size_t>(fmt.segments[0].text_length));
                format_spec spec{};
                spec.spec_type = static_cast<format_spec::type>(fmt.segments[1].arg_type);
                detail::format_one(result, std::forward<TArgs>(args)..., spec);
                return result;
            }
        }

        std::string result;

        // Better size estimation
        size_t estimated_size = 0;
        for (size_t i = 0; i < fmt.segment_count; ++i)
        {
            if (fmt.segments[i].type == FmtType::Segment::Type::Text)
            {
                estimated_size += static_cast<size_t>(fmt.segments[i].text_length);
            }
            else
            {
                // More accurate estimates based on type
                estimated_size += 16; // Reasonable default for most types
            }
        }
        result.reserve(estimated_size);

        auto TArgs_tuple = std::forward_as_tuple(args...);

        for (size_t i = 0; i < fmt.segment_count; ++i)
        {
            const auto& seg = fmt.segments[i];
            if (seg.type == FmtType::Segment::Type::Text)
            {
                result.append(fmt.str.data() + seg.text_start, static_cast<size_t>(seg.text_length));
            }
            else
            {
                format_spec spec;
                spec.spec_type = static_cast<format_spec::type>(seg.arg_type);
                spec.width = seg.arg_width;
                spec.precision = seg.arg_precision;
                spec.fill = seg.arg_fill;
                spec.align = seg.arg_align;
                detail::format_arg_at_index(result, TArgs_tuple, seg.arg_index, spec, std::index_sequence_for<TArgs...>{});
            }
        }

        return result;
    }

    template<typename OutputIt, typename... TArgs>
    inline OutputIt format_to(OutputIt out, format_string<TArgs...> fmt, TArgs&&... args)
    {
        std::string result = format(fmt, std::forward<TArgs>(args)...);
        return std::copy(result.begin(), result.end(), out);
    }

} // namespace behl
