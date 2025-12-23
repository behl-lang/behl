#pragma once

#include "ast/ast_context.hpp"
#include "ast/constant_folding.hpp"
#include "ast/dead_store_elimination.hpp"
#include "ast/loop_optimization.hpp"
#include "optimization_pass.hpp"

namespace behl
{
    // Standard AST optimization pipeline (does not include semantics pass - that always runs)
    // Loop optimization should run before other optimizations to expose more optimization opportunities
    using ASTOptimizationPipeline = OptimizationPipeline<AstOptimizationContext, LoopOptimizationPass, ConstantFoldingPass,
        DeadStoreEliminationPass>;

} // namespace behl
