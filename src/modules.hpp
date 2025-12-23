#pragma once

#include "common/string.hpp"

#include <string_view>
#include <vector>

namespace behl
{
    struct State;

    std::string resolve_module_path(State* S, std::string_view module_name, std::string_view importing_file);

} // namespace behl
