#pragma once

#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"

namespace behl
{

    struct AstOptimizationContext
    {
        AstHolder& holder;
        AstProgram* program;

        AstOptimizationContext(AstHolder& h, AstProgram* p)
            : holder(h)
            , program(p)
        {
        }

        AstProgram* result() const
        {
            return program;
        }
    };

} // namespace behl
