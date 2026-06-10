#include "loop_optimization.hpp"

#include "ast/ast_transformer.hpp"
#include "frontend/lexer.hpp"

namespace behl
{
    static bool block_writes_variable(const AstBlock* block, std::string_view var_name);

    // Check if an expression can write the variable, which only happens through
    // a closure defined inside it that captures and assigns it
    static bool expr_writes_variable(const AstNode* node, std::string_view var_name)
    {
        if (!node)
        {
            return false;
        }

        if (auto* func_def = node->try_as<AstFuncDef>())
        {
            return block_writes_variable(func_def->block, var_name);
        }
        if (auto* binop = node->try_as<AstBinOp>())
        {
            return expr_writes_variable(binop->left, var_name) || expr_writes_variable(binop->right, var_name);
        }
        if (auto* unop = node->try_as<AstUnOp>())
        {
            return expr_writes_variable(unop->expr, var_name);
        }
        if (auto* ternary = node->try_as<AstTernary>())
        {
            return expr_writes_variable(ternary->condition, var_name) || expr_writes_variable(ternary->true_expr, var_name)
                || expr_writes_variable(ternary->false_expr, var_name);
        }
        if (auto* call = node->try_as<AstFuncCall>())
        {
            if (expr_writes_variable(call->func, var_name))
            {
                return true;
            }
            for (const AstNode* arg = call->first_arg; arg; arg = arg->next_child)
            {
                if (expr_writes_variable(arg, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* index = node->try_as<AstIndex>())
        {
            return expr_writes_variable(index->table, var_name) || expr_writes_variable(index->key, var_name);
        }
        if (auto* member = node->try_as<AstMember>())
        {
            return expr_writes_variable(member->table, var_name);
        }
        if (auto* table_ctor = node->try_as<AstTableCtor>())
        {
            for (const AstNode* n = table_ctor->first_field; n; n = n->next_child)
            {
                auto* field = static_cast<const TableField*>(n);
                if ((field->key && expr_writes_variable(field->key, var_name))
                    || expr_writes_variable(field->value, var_name))
                {
                    return true;
                }
            }
            return false;
        }

        return false;
    }

    static bool ident_matches(const AstNode* node, std::string_view var_name)
    {
        auto* ident = node ? node->try_as<AstIdent>() : nullptr;
        return ident && ident->name->view() == var_name;
    }

    // Check if a statement writes (or shadows) the variable, including writes
    // from closures defined anywhere inside it
    static bool statement_writes_variable(const AstNode* stat, std::string_view var_name)
    {
        if (!stat)
        {
            return false;
        }

        if (auto* block = stat->try_as<AstBlock>())
        {
            return block_writes_variable(block, var_name);
        }
        if (auto* local_decl = stat->try_as<AstLocalDecl>())
        {
            // A redeclaration shadows the loop variable; treat it as a write to stay safe
            for (const AstNode* n = local_decl->first_name; n; n = n->next_child)
            {
                if (static_cast<const AstString*>(n)->view() == var_name)
                {
                    return true;
                }
            }
            for (const AstNode* init = local_decl->first_init; init; init = init->next_child)
            {
                if (expr_writes_variable(init, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* assign_local = stat->try_as<AstAssignLocal>())
        {
            return assign_local->name->view() == var_name || expr_writes_variable(assign_local->expr, var_name);
        }
        if (auto* assign_global = stat->try_as<AstAssignGlobal>())
        {
            return expr_writes_variable(assign_global->expr, var_name);
        }
        if (auto* assign_upvalue = stat->try_as<AstAssignUpvalue>())
        {
            return assign_upvalue->name->view() == var_name || expr_writes_variable(assign_upvalue->expr, var_name);
        }
        if (auto* assign = stat->try_as<AstAssign>())
        {
            for (const AstNode* v = assign->first_var; v; v = v->next_child)
            {
                if (ident_matches(v, var_name) || expr_writes_variable(v, var_name))
                {
                    return true;
                }
            }
            for (const AstNode* e = assign->first_expr; e; e = e->next_child)
            {
                if (expr_writes_variable(e, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* compound = stat->try_as<AstCompoundAssign>())
        {
            return ident_matches(compound->target, var_name) || expr_writes_variable(compound->target, var_name)
                || expr_writes_variable(compound->expr, var_name);
        }
        if (auto* compound_local = stat->try_as<AstCompoundLocal>())
        {
            return compound_local->name->view() == var_name || expr_writes_variable(compound_local->expr, var_name);
        }
        if (auto* compound_global = stat->try_as<AstCompoundGlobal>())
        {
            return expr_writes_variable(compound_global->expr, var_name);
        }
        if (auto* compound_upvalue = stat->try_as<AstCompoundUpvalue>())
        {
            return compound_upvalue->name->view() == var_name || expr_writes_variable(compound_upvalue->expr, var_name);
        }
        if (auto* inc = stat->try_as<AstIncrement>())
        {
            return ident_matches(inc->target, var_name) || expr_writes_variable(inc->target, var_name);
        }
        if (auto* dec = stat->try_as<AstDecrement>())
        {
            return ident_matches(dec->target, var_name) || expr_writes_variable(dec->target, var_name);
        }
        if (auto* inc_local = stat->try_as<AstIncLocal>())
        {
            return inc_local->name->view() == var_name;
        }
        if (auto* dec_local = stat->try_as<AstDecLocal>())
        {
            return dec_local->name->view() == var_name;
        }
        if (auto* inc_upvalue = stat->try_as<AstIncUpvalue>())
        {
            return inc_upvalue->name->view() == var_name;
        }
        if (auto* dec_upvalue = stat->try_as<AstDecUpvalue>())
        {
            return dec_upvalue->name->view() == var_name;
        }
        if (auto* return_stat = stat->try_as<AstReturn>())
        {
            for (const AstNode* expr = return_stat->first_expr; expr; expr = expr->next_child)
            {
                if (expr_writes_variable(expr, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* expr_stat = stat->try_as<AstExprStat>())
        {
            return expr_writes_variable(expr_stat->expr, var_name);
        }
        if (auto* if_stat = stat->try_as<AstIf>())
        {
            if (expr_writes_variable(if_stat->cond, var_name) || block_writes_variable(if_stat->then_block, var_name))
            {
                return true;
            }
            for (const ElseIf* elseif = if_stat->first_elseif; elseif;
                elseif = static_cast<const ElseIf*>(elseif->next_child))
            {
                if ((elseif->cond && expr_writes_variable(elseif->cond, var_name))
                    || block_writes_variable(elseif->block, var_name))
                {
                    return true;
                }
            }
            return block_writes_variable(if_stat->else_block, var_name);
        }
        if (auto* while_stat = stat->try_as<AstWhile>())
        {
            return expr_writes_variable(while_stat->cond, var_name) || block_writes_variable(while_stat->block, var_name);
        }
        if (auto* for_c = stat->try_as<AstForC>())
        {
            return statement_writes_variable(for_c->init, var_name) || expr_writes_variable(for_c->condition, var_name)
                || statement_writes_variable(for_c->update, var_name) || block_writes_variable(for_c->block, var_name);
        }
        if (auto* for_c_num = stat->try_as<AstForCNumeric>())
        {
            return for_c_num->var->view() == var_name || expr_writes_variable(for_c_num->start, var_name)
                || expr_writes_variable(for_c_num->end, var_name) || expr_writes_variable(for_c_num->step, var_name)
                || block_writes_variable(for_c_num->block, var_name);
        }
        if (auto* for_in = stat->try_as<AstForIn>())
        {
            for (const AstNode* n = for_in->first_name; n; n = n->next_child)
            {
                if (static_cast<const AstString*>(n)->view() == var_name)
                {
                    return true;
                }
            }
            for (const AstNode* expr = for_in->first_expr; expr; expr = expr->next_child)
            {
                if (expr_writes_variable(expr, var_name))
                {
                    return true;
                }
            }
            return block_writes_variable(for_in->block, var_name);
        }
        if (auto* func_def_stat = stat->try_as<AstFuncDefStat>())
        {
            return block_writes_variable(func_def_stat->block, var_name);
        }
        if (auto* defer_stat = stat->try_as<AstDefer>())
        {
            return statement_writes_variable(defer_stat->body, var_name);
        }
        if (auto* scope_stat = stat->try_as<AstScope>())
        {
            return block_writes_variable(scope_stat->block, var_name);
        }

        return false;
    }

    static bool block_writes_variable(const AstBlock* block, std::string_view var_name)
    {
        for (const AstNode* stat = block ? block->first_stat : nullptr; stat; stat = stat->next_child)
        {
            if (statement_writes_variable(stat, var_name))
            {
                return true;
            }
        }
        return false;
    }

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

            // The counted FORPREP/FORLOOP runtime fixes the trip count at loop
            // entry, so the body must not write or shadow the loop variable
            if (block_writes_variable(for_c->block, loop_var->view()))
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
