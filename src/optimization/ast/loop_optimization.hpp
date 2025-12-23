#pragma once

#include "ast_context.hpp"

namespace behl
{
    // Loop optimization pass
    // Transforms C-style numeric for loops into optimized ForCNumeric nodes
    // Pattern: for(let i = start; i </<=/>/>= end; i++ / i-- / i += step / i -= step)
    struct LoopOptimizationPass
    {
        static constexpr std::string_view kName = "LoopOptimization";

        static bool apply(AstOptimizationContext& context);
    };

} // namespace behl
