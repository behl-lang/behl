#include "loop_optimization.hpp"

#include "ast/ast_transformer.hpp"
#include "frontend/lexer.hpp"

namespace behl
{
    class LoopOptimizer : public AstTransformer
    {
    private:
        // Pattern: for(let i = start; i </<=/>/>= end; i++ / i-- / i += step / i -= step)
        AstForCNumeric* try_optimize_for_c(AstForC* for_c)
        {
            if (!for_c->init || !for_c->condition || !for_c->update)
            {
                return nullptr; // Need all three parts
            }

            // Check init: must be "let i = start" (LocalDecl with single variable)
            AstString* loop_var = nullptr;
            AstNode* start_expr = nullptr;

            if (auto* local_decl = for_c->init->try_as<AstLocalDecl>())
            {
                // Check it's a single variable declaration
                if (!local_decl->first_name || local_decl->first_name->next_child)
                {
                    return nullptr; // Multiple variables
                }
                // Cannot optimize const loop variables (they can't be incremented)
                if (local_decl->is_const)
                {
                    return nullptr;
                }
                loop_var = static_cast<AstString*>(local_decl->first_name);
                if (!local_decl->first_init || local_decl->first_init->next_child)
                {
                    return nullptr; // Need exactly one initializer
                }
                start_expr = local_decl->first_init;
            }
            else
            {
                return nullptr; // Must be a local declaration
            }

            // Check condition: must be "i </<=/>/>= end"
            auto* cond_binop = for_c->condition->try_as<AstBinOp>();
            if (!cond_binop)
            {
                return nullptr;
            }

            // Left side must be the loop variable
            auto* left_ident = cond_binop->left->try_as<AstIdent>();
            if (!left_ident || left_ident->name->view() != loop_var->view())
            {
                return nullptr;
            }

            // Check operator type
            bool ascending = false;
            bool inclusive = false;
            switch (cond_binop->op)
            {
                case TokenType::kLt:
                    ascending = true;
                    inclusive = false;
                    break;
                case TokenType::kLe:
                    ascending = true;
                    inclusive = true;
                    break;
                case TokenType::kGt:
                    ascending = false;
                    inclusive = false;
                    break;
                case TokenType::kGe:
                    ascending = false;
                    inclusive = true;
                    break;
                default:
                    return nullptr; // Not a simple comparison
            }

            AstNode* end_expr = cond_binop->right;

            // Check update: must be i++, i--, i += step, i -= step, or i = i +/- step
            AstNode* step_expr = nullptr;
            bool step_ascending = ascending;

            // Check post-transformation nodes first
            if (auto* inc_local = for_c->update->try_as<AstIncLocal>())
            {
                if (inc_local->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                step_ascending = true;
            }
            else if (auto* dec_local = for_c->update->try_as<AstDecLocal>())
            {
                if (dec_local->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                step_ascending = false;
            }
            else if (auto* compound_local = for_c->update->try_as<AstCompoundLocal>())
            {
                if (compound_local->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                if (compound_local->op == TokenType::kPlus)
                {
                    step_ascending = true;
                    step_expr = compound_local->expr;
                }
                else if (compound_local->op == TokenType::kMinus)
                {
                    step_ascending = false;
                    step_expr = compound_local->expr;
                }
                else
                {
                    return nullptr;
                }
            }
            else if (auto* assign_local = for_c->update->try_as<AstAssignLocal>())
            {
                if (assign_local->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                auto* rhs_binop = assign_local->expr->try_as<AstBinOp>();
                if (!rhs_binop)
                {
                    return nullptr;
                }
                auto* rhs_left = rhs_binop->left->try_as<AstIdent>();
                if (!rhs_left || rhs_left->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                if (rhs_binop->op == TokenType::kPlus)
                {
                    step_ascending = true;
                    step_expr = rhs_binop->right;
                }
                else if (rhs_binop->op == TokenType::kMinus)
                {
                    step_ascending = false;
                    step_expr = rhs_binop->right;
                }
                else
                {
                    return nullptr;
                }
            }
            // Check pre-transformation nodes
            else if (auto* inc = for_c->update->try_as<AstIncrement>())
            {
                auto* target_ident = inc->target->try_as<AstIdent>();
                if (!target_ident || target_ident->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                step_ascending = true;
            }
            else if (auto* dec = for_c->update->try_as<AstDecrement>())
            {
                auto* target_ident = dec->target->try_as<AstIdent>();
                if (!target_ident || target_ident->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                step_ascending = false;
            }
            else if (auto* compound = for_c->update->try_as<AstCompoundAssign>())
            {
                auto* target_ident = compound->target->try_as<AstIdent>();
                if (!target_ident || target_ident->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                if (compound->op == TokenType::kPlusAssign)
                {
                    step_ascending = true;
                    step_expr = compound->expr;
                }
                else if (compound->op == TokenType::kMinusAssign)
                {
                    step_ascending = false;
                    step_expr = compound->expr;
                }
                else
                {
                    return nullptr;
                }
            }
            else if (auto* assign = for_c->update->try_as<AstAssign>())
            {
                auto* target_ident = assign->first_var->try_as<AstIdent>();
                if (!target_ident || target_ident->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                if (!assign->first_expr || assign->first_expr->next_child)
                {
                    return nullptr;
                }
                auto* rhs_binop = assign->first_expr->try_as<AstBinOp>();
                if (!rhs_binop)
                {
                    return nullptr;
                }
                auto* rhs_left = rhs_binop->left->try_as<AstIdent>();
                if (!rhs_left || rhs_left->name->view() != loop_var->view())
                {
                    return nullptr;
                }
                if (rhs_binop->op == TokenType::kPlus)
                {
                    step_ascending = true;
                    step_expr = rhs_binop->right;
                }
                else if (rhs_binop->op == TokenType::kMinus)
                {
                    step_ascending = false;
                    step_expr = rhs_binop->right;
                }
                else
                {
                    return nullptr;
                }
            }
            else
            {
                return nullptr;
            }

            // Verify step direction matches comparison direction
            if (step_ascending != ascending)
            {
                return nullptr;
            }

            // Create the optimized ForCNumeric node
            auto* optimized = holder.make<AstForCNumeric>(loop_var, start_expr, end_expr, step_expr, ascending, inclusive);
            optimized->block = for_c->block;
            optimized->line = for_c->line;
            optimized->column = for_c->column;

            changed = true;
            return optimized;
        }

    public:
        explicit LoopOptimizer(AstHolder& h)
            : AstTransformer(h)
        {
        }

        // Override ForC to try optimization
        AstNode* visit_ForC(AstForC* node) override
        {
            // Try to optimize to ForCNumeric
            if (auto* optimized = try_optimize_for_c(node))
            {
                // Transform the block of the optimized node
                transform_block(optimized->block);
                return optimized;
            }

            // Fall back to default behavior (transforms children)
            return AstTransformer::visit_ForC(node);
        }
    };

    bool LoopOptimizationPass::apply(AstOptimizationContext& context)
    {
        LoopOptimizer optimizer(context.holder);

        if (context.program->block)
        {
            optimizer.transform_block(context.program->block);
        }

        return optimizer.has_changed();
    }

} // namespace behl
