#pragma once

#include <charconv>
#include <system_error>
#include <type_traits>

namespace behl
{

    std::from_chars_result from_chars(const char* first, const char* last, double& value) noexcept;
    std::from_chars_result from_chars(const char* first, const char* last, float& value) noexcept;

    template<typename T>
    std::enable_if_t<std::is_integral_v<T>, std::from_chars_result> from_chars(
        const char* first, const char* last, T& value, int base = 10) noexcept
    {
        return std::from_chars(first, last, value, base);
    }

    std::to_chars_result to_chars(char* first, char* last, double value) noexcept;
    std::to_chars_result to_chars(char* first, char* last, float value) noexcept;

    template<typename T>
    std::enable_if_t<std::is_integral_v<T>, std::to_chars_result> to_chars(
        char* first, char* last, T value, int base = 10) noexcept
    {
        return std::to_chars(first, last, value, base);
    }

} // namespace behl
