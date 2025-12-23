#pragma once

#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"
#include "behl/export.hpp"

namespace behl
{

    struct SemanticsPass
    {
        static constexpr std::string_view kName = "Semantics";

        BEHL_API static AstProgram* apply(State* state, AstHolder& holder, AstProgram* program);
    };

} // namespace behl
