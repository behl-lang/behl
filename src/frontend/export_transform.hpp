#pragma once

#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"
#include "state.hpp"

namespace behl
{
    // Transform module exports into explicit assignments to __EXPORTS__ table
    // This pass rewrites the AST to inject:
    //   1. let __EXPORTS__ = {} at the start
    //   2. __EXPORTS__["name"] = name after each export
    //   3. return __EXPORTS__ at the end
    AstProgram* transform_exports(State* S, AstHolder& holder, AstProgram* program);

} // namespace behl
