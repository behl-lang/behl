#pragma once

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <system_error>

namespace behl
{

// macOS libc++ has incomplete std::from_chars support for floating-point, silly Apple.
// Provide fallback using strtod/strtof for compatibility
#if defined(__APPLE__) && defined(__MACH__)

    inline std::from_chars_result from_chars_float_fallback(const char* first, const char* last, float& value) noexcept
    {
        // Reject hex strings to match std::from_chars behavior
        if (last - first >= 2 && first[0] == '0' && (first[1] == 'x' || first[1] == 'X'))
        {
            return { first, std::errc::invalid_argument };
        }

        char* end;
        errno = 0;
        value = std::strtof(first, &end);

        if (errno == ERANGE)
        {
            return { end, std::errc::result_out_of_range };
        }
        if (end == first)
        {
            return { first, std::errc::invalid_argument };
        }
        if (end > last)
        {
            end = const_cast<char*>(last);
        }

        return { end, std::errc{} };
    }

    inline std::from_chars_result from_chars_double_fallback(const char* first, const char* last, double& value) noexcept
    {
        // Reject hex strings to match std::from_chars behavior
        if (last - first >= 2 && first[0] == '0' && (first[1] == 'x' || first[1] == 'X'))
        {
            return { first, std::errc::invalid_argument };
        }

        char* end;
        errno = 0;
        value = std::strtod(first, &end);

        if (errno == ERANGE)
        {
            return { end, std::errc::result_out_of_range };
        }
        if (end == first)
        {
            return { first, std::errc::invalid_argument };
        }
        if (end > last)
        {
            end = const_cast<char*>(last);
        }

        return { end, std::errc{} };
    }

    inline std::to_chars_result to_chars_float_fallback(
        char* first, char* last, float value, std::chars_format fmt = std::chars_format::general, int precision = 6) noexcept
    {
        char buffer[64];
        int len;
        if (fmt == std::chars_format::fixed)
        {
            len = std::snprintf(buffer, sizeof(buffer), "%.*f", precision, static_cast<double>(value));
        }
        else
        {
            len = std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(value));
        }

        if (len < 0 || static_cast<size_t>(len) >= sizeof(buffer))
        {
            return { last, std::errc::value_too_large };
        }
        if (last - first < len)
        {
            return { last, std::errc::value_too_large };
        }

        std::memcpy(first, buffer, static_cast<size_t>(len));
        return { first + len, std::errc{} };
    }

    inline std::to_chars_result to_chars_double_fallback(
        char* first, char* last, double value, std::chars_format fmt = std::chars_format::general, int precision = 6) noexcept
    {
        char buffer[64];
        int len;
        if (fmt == std::chars_format::fixed)
        {
            len = std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
        }
        else
        {
            len = std::snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        }

        if (len < 0 || static_cast<size_t>(len) >= sizeof(buffer))
        {
            return { last, std::errc::value_too_large };
        }
        if (last - first < len)
        {
            return { last, std::errc::value_too_large };
        }

        std::memcpy(first, buffer, static_cast<size_t>(len));
        return { first + len, std::errc{} };
    }

    // Wrapper that uses fallback on macOS
    template<typename T>
    std::from_chars_result from_chars(const char* first, const char* last, T& value, int base = 10)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if constexpr (std::is_same_v<T, float>)
            {
                return from_chars_float_fallback(first, last, value);
            }
            else
            {
                return from_chars_double_fallback(first, last, value);
            }
        }
        else
        {
            // Integer types work fine with std::from_chars on macOS
            return std::from_chars(first, last, value, base);
        }
    }

    // to_chars overloads for floating-point
    inline std::to_chars_result to_chars(char* first, char* last, float value) noexcept
    {
        return to_chars_float_fallback(first, last, value);
    }

    inline std::to_chars_result to_chars(char* first, char* last, float value, std::chars_format fmt) noexcept
    {
        return to_chars_float_fallback(first, last, value, fmt);
    }

    inline std::to_chars_result to_chars(char* first, char* last, float value, std::chars_format fmt, int precision) noexcept
    {
        return to_chars_float_fallback(first, last, value, fmt, precision);
    }

    inline std::to_chars_result to_chars(char* first, char* last, double value) noexcept
    {
        return to_chars_double_fallback(first, last, value);
    }

    inline std::to_chars_result to_chars(char* first, char* last, double value, std::chars_format fmt) noexcept
    {
        return to_chars_double_fallback(first, last, value, fmt);
    }

    inline std::to_chars_result to_chars(char* first, char* last, double value, std::chars_format fmt, int precision) noexcept
    {
        return to_chars_double_fallback(first, last, value, fmt, precision);
    }

    template<typename T>
    std::enable_if_t<std::is_integral_v<T>, std::to_chars_result> to_chars(
        char* first, char* last, T value, int base = 10) noexcept
    {
        return std::to_chars(first, last, value, base);
    }

#else

    // Other platforms - use standard library directly.
    using std::from_chars;
    using std::to_chars;

#endif

} // namespace behl
