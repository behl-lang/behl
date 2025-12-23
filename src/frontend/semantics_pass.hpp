#pragma once

#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"

namespace behl
{

    struct SemanticsPass
    {
        static constexpr std::string_view kName = "Semantics";

        static AstProgram* apply(State* state, AstHolder& holder, AstProgram* program);
    };

} // namespace behl
