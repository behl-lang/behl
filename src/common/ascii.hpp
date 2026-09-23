#pragma once

#include <concepts>

namespace behl
{

    template<typename T>
    concept AsciiChar = std::same_as<T, char> || std::same_as<T, unsigned char> || std::same_as<T, char32_t>;

    template<AsciiChar T>
    [[nodiscard]] constexpr char32_t ascii_code(T c) noexcept
    {
        if constexpr (std::same_as<T, char>)
        {
            return static_cast<char32_t>(static_cast<unsigned char>(c));
        }
        else
        {
            return static_cast<char32_t>(c);
        }
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr bool is_ascii_space(T c) noexcept
    {
        const char32_t u = ascii_code(c);
        return u == U' ' || u == U'\t' || u == U'\n' || u == U'\r' || u == U'\v' || u == U'\f';
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr bool is_ascii_digit(T c) noexcept
    {
        const char32_t u = ascii_code(c);
        return u >= U'0' && u <= U'9';
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr bool is_ascii_upper(T c) noexcept
    {
        const char32_t u = ascii_code(c);
        return u >= U'A' && u <= U'Z';
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr bool is_ascii_lower(T c) noexcept
    {
        const char32_t u = ascii_code(c);
        return u >= U'a' && u <= U'z';
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr bool is_ascii_alpha(T c) noexcept
    {
        return is_ascii_upper(c) || is_ascii_lower(c);
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr bool is_ascii_hex_digit(T c) noexcept
    {
        const char32_t u = ascii_code(c);
        return is_ascii_digit(c) || (u >= U'a' && u <= U'f') || (u >= U'A' && u <= U'F');
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr T ascii_to_upper(T c) noexcept
    {
        return is_ascii_lower(c) ? static_cast<T>(ascii_code(c) - (U'a' - U'A')) : c;
    }

    template<AsciiChar T>
    [[nodiscard]] constexpr T ascii_to_lower(T c) noexcept
    {
        return is_ascii_upper(c) ? static_cast<T>(ascii_code(c) + (U'a' - U'A')) : c;
    }

} // namespace behl
