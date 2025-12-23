#pragma once

#include "ast_context.hpp"

namespace behl
{
    // Dead store elimination optimization pass
    // Removes assignments to variables that are never read
    struct DeadStoreEliminationPass
    {
        static constexpr std::string_view kName = "DeadStoreElimination";

        static bool apply(AstOptimizationContext& context);
    };

} // namespace behl
