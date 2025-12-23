#pragma once

#include "ast_context.hpp"

namespace behl
{
    // Constant folding optimization pass
    // Evaluates binary operations on constant operands at compile time
    struct ConstantFoldingPass
    {
        static constexpr std::string_view kName = "ConstantFolding";

        static bool apply(AstOptimizationContext& context);
    };

} // namespace behl
