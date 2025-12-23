#pragma once

#include "common/format.hpp"

#include <iostream>
#include <string_view>

namespace behl
{

    template<typename... TArgs>
    void print(format_string<TArgs...> fmt, TArgs&&... args)
    {
        const auto msg = format(fmt, std::forward<TArgs>(args)...);
        std::cout << msg;
    }

    template<typename... TArgs>
    void println(format_string<TArgs...> fmt, TArgs&&... args)
    {
        const auto msg = format(fmt, std::forward<TArgs>(args)...);
        std::cout << msg << std::endl;
    }

    struct State;

    namespace detail
    {
        void print(State* S, std::string_view msg);
    }

    template<typename... TArgs>
    void print(State* S, format_string<TArgs...> fmt, TArgs&&... args)
    {
        const auto msg = format(fmt, std::forward<TArgs>(args)...);
        detail::print(S, msg);
    }

    template<typename... TArgs>
    void println(State* S, format_string<TArgs...> fmt, TArgs&&... args)
    {
        const auto msg = format(fmt, std::forward<TArgs>(args)...);
        print(S, "{}\n", msg);
    }

} // namespace behl
