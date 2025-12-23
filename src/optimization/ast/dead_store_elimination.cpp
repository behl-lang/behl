#include "dead_store_elimination.hpp"

#include "common/print.hpp"
#include "common/vector.hpp"
#include "config_internal.hpp"

#include <utility>

namespace behl
{
    static bool reads_variable(const AstNode* node, std::string_view var_name);

    struct DSEState
    {
        AstHolder& holder;
        size_t removed_count = 0;
        bool changed = false;
    };

    struct PendingStore
    {
        AstBlock* block = nullptr; // Owning block so we can unlink safely
        AstNode* stat = nullptr;   // Statement performing the store
        AstNode* expr = nullptr;   // Expression being stored (for side-effect checks)
    };

    // Best-effort check for expression side effects. Any function call or mutation counts.
    static bool expression_has_side_effects(const AstNode* node)
    {
        if (!node)
        {
            return false;
        }

        switch (node->type)
        {
            case AstNodeType::kFuncCall:
            case AstNodeType::kAssign:
            case AstNodeType::kAssignLocal:
            case AstNodeType::kAssignGlobal:
            case AstNodeType::kAssignUpvalue:
            case AstNodeType::kCompoundAssign:
            case AstNodeType::kCompoundLocal:
            case AstNodeType::kCompoundGlobal:
            case AstNodeType::kCompoundUpvalue:
            case AstNodeType::kIncrement:
            case AstNodeType::kIncLocal:
            case AstNodeType::kIncGlobal:
            case AstNodeType::kIncUpvalue:
            case AstNodeType::kDecrement:
            case AstNodeType::kDecLocal:
            case AstNodeType::kDecGlobal:
            case AstNodeType::kDecUpvalue:
                return true; // Directly performs work with possible side effects

            default:
                break;
        }

        if (auto* binop = node->try_as<AstBinOp>())
        {
            return expression_has_side_effects(binop->left) || expression_has_side_effects(binop->right);
        }
        if (auto* unop = node->try_as<AstUnOp>())
        {
            return expression_has_side_effects(unop->expr);
        }
        if (auto* call = node->try_as<AstFuncCall>())
        {
            if (expression_has_side_effects(call->func))
            {
                return true;
            }
            for (const AstNode* arg = call->first_arg; arg; arg = arg->next_child)
            {
                if (expression_has_side_effects(arg))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* index = node->try_as<AstIndex>())
        {
            return expression_has_side_effects(index->table) || expression_has_side_effects(index->key);
        }
        if (auto* member = node->try_as<AstMember>())
        {
            return expression_has_side_effects(member->table);
        }
        if (auto* table_ctor = node->try_as<AstTableCtor>())
        {
            for (const AstNode* n = table_ctor->first_field; n; n = n->next_child)
            {
                auto* field = static_cast<const TableField*>(n);
                if ((field->key && expression_has_side_effects(field->key)) || expression_has_side_effects(field->value))
                {
                    return true;
                }
            }
            return false; // Pure construction
        }
        if (auto* func_def [[maybe_unused]] = node->try_as<AstFuncDef>())
        {
            // Function literals are pure; body is handled when the function executes
            return false;
        }

        return false;
    }

    static bool statement_reads_variable(const AstNode* stat, const std::string_view var_name);

    static bool block_reads_variable(const AstBlock* block, const std::string_view var_name)
    {
        for (const AstNode* stat = block ? block->first_stat : nullptr; stat != nullptr; stat = stat->next_child)
        {
            if (statement_reads_variable(stat, var_name))
            {
                return true;
            }
        }
        return false;
    }

    static bool statement_reads_variable(const AstNode* stat, const std::string_view var_name)
    {
        if (!stat)
        {
            return false;
        }

        if (auto* local_decl = stat->try_as<AstLocalDecl>())
        {
            for (AstNode* init = local_decl->first_init; init; init = init->next_child)
            {
                if (reads_variable(init, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* assign_local = stat->try_as<AstAssignLocal>())
        {
            return reads_variable(assign_local->expr, var_name);
        }
        if (auto* assign_global = stat->try_as<AstAssignGlobal>())
        {
            return reads_variable(assign_global->expr, var_name);
        }
        if (auto* assign_upvalue = stat->try_as<AstAssignUpvalue>())
        {
            return reads_variable(assign_upvalue->expr, var_name);
        }
        if (auto* assign = stat->try_as<AstAssign>())
        {
            for (AstNode* v = assign->first_var; v; v = v->next_child)
            {
                if (reads_variable(v, var_name))
                {
                    return true;
                }
            }
            for (AstNode* e = assign->first_expr; e; e = e->next_child)
            {
                if (reads_variable(e, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* compound = stat->try_as<AstCompoundAssign>())
        {
            return reads_variable(compound->target, var_name) || reads_variable(compound->expr, var_name);
        }
        if (auto* compound_local = stat->try_as<AstCompoundLocal>())
        {
            return compound_local->name->view() == var_name || reads_variable(compound_local->expr, var_name);
        }
        if (auto* compound_global = stat->try_as<AstCompoundGlobal>())
        {
            return compound_global->name->view() == var_name || reads_variable(compound_global->expr, var_name);
        }
        if (auto* compound_upvalue = stat->try_as<AstCompoundUpvalue>())
        {
            return compound_upvalue->name->view() == var_name || reads_variable(compound_upvalue->expr, var_name);
        }
        if (auto* inc = stat->try_as<AstIncrement>())
        {
            return reads_variable(inc->target, var_name);
        }
        if (auto* inc_local = stat->try_as<AstIncLocal>())
        {
            return inc_local->name->view() == var_name;
        }
        if (auto* inc_global = stat->try_as<AstIncGlobal>())
        {
            return inc_global->name->view() == var_name;
        }
        if (auto* inc_upvalue = stat->try_as<AstIncUpvalue>())
        {
            return inc_upvalue->name->view() == var_name;
        }
        if (auto* dec_local = stat->try_as<AstDecLocal>())
        {
            return dec_local->name->view() == var_name;
        }
        if (auto* dec_global = stat->try_as<AstDecGlobal>())
        {
            return dec_global->name->view() == var_name;
        }
        if (auto* dec_upvalue = stat->try_as<AstDecUpvalue>())
        {
            return dec_upvalue->name->view() == var_name;
        }
        if (auto* return_stat = stat->try_as<AstReturn>())
        {
            for (AstNode* expr = return_stat->first_expr; expr; expr = expr->next_child)
            {
                if (reads_variable(expr, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        if (auto* expr_stat = stat->try_as<AstExprStat>())
        {
            return reads_variable(expr_stat->expr, var_name);
        }
        if (auto* if_stat = stat->try_as<AstIf>())
        {
            if (reads_variable(if_stat->cond, var_name))
            {
                return true;
            }
            if (if_stat->then_block && block_reads_variable(if_stat->then_block, var_name))
            {
                return true;
            }
            for (ElseIf* elseif = if_stat->first_elseif; elseif != nullptr; elseif = static_cast<ElseIf*>(elseif->next_child))
            {
                if ((elseif->cond && reads_variable(elseif->cond, var_name))
                    || (elseif->block && block_reads_variable(elseif->block, var_name)))
                {
                    return true;
                }
            }
            if (if_stat->else_block && block_reads_variable(if_stat->else_block, var_name))
            {
                return true;
            }
            return false;
        }
        if (auto* while_stat = stat->try_as<AstWhile>())
        {
            if (reads_variable(while_stat->cond, var_name))
            {
                return true;
            }
            return while_stat->block && block_reads_variable(while_stat->block, var_name);
        }
        if (auto* for_c = stat->try_as<AstForC>())
        {
            if ((for_c->init && reads_variable(for_c->init, var_name))
                || (for_c->condition && reads_variable(for_c->condition, var_name))
                || (for_c->update && reads_variable(for_c->update, var_name)))
            {
                return true;
            }
            return for_c->block && block_reads_variable(for_c->block, var_name);
        }
        if (auto* for_num = stat->try_as<AstForNum>())
        {
            if (reads_variable(for_num->start, var_name) || reads_variable(for_num->end, var_name))
            {
                return true;
            }
            if (for_num->step && reads_variable(for_num->step, var_name))
            {
                return true;
            }
            return for_num->block && block_reads_variable(for_num->block, var_name);
        }
        if (auto* for_in = stat->try_as<AstForIn>())
        {
            for (AstNode* expr = for_in->first_expr; expr; expr = expr->next_child)
            {
                if (reads_variable(expr, var_name))
                {
                    return true;
                }
            }
            return for_in->block && block_reads_variable(for_in->block, var_name);
        }
        if (auto* func_def_stat = stat->try_as<AstFuncDefStat>())
        {
            return func_def_stat->block && block_reads_variable(func_def_stat->block, var_name);
        }
        if (auto* scope_stat = stat->try_as<AstScope>())
        {
            return scope_stat->block && block_reads_variable(scope_stat->block, var_name);
        }

        return false;
    }

    static void unlink_statement(AstBlock& block, AstNode* target)
    {
        AstNode** link = &block.first_stat;
        while (*link)
        {
            if (*link == target)
            {
                *link = target->next_child;
                return;
            }
            link = &(*link)->next_child;
        }
    }

    // Check if an expression reads a variable
    static bool reads_variable(const AstNode* node, const std::string_view var_name)
    {
        if (!node)
        {
            return false;
        }

        if (auto* ident = node->try_as<AstIdent>())
        {
            return ident->name->view() == var_name;
        }
        else if (auto* binop = node->try_as<AstBinOp>())
        {
            return reads_variable(binop->left, var_name) || reads_variable(binop->right, var_name);
        }
        else if (auto* unop = node->try_as<AstUnOp>())
        {
            return reads_variable(unop->expr, var_name);
        }
        else if (auto* call = node->try_as<AstFuncCall>())
        {
            if (reads_variable(call->func, var_name))
            {
                return true;
            }
            for (AstNode* arg = call->first_arg; arg; arg = arg->next_child)
            {
                if (reads_variable(arg, var_name))
                {
                    return true;
                }
            }
            return false;
        }
        else if (auto* index = node->try_as<AstIndex>())
        {
            return reads_variable(index->table, var_name) || reads_variable(index->key, var_name);
        }
        else if (auto* member = node->try_as<AstMember>())
        {
            return reads_variable(member->table, var_name);
        }
        else if (auto* table_ctor = node->try_as<AstTableCtor>())
        {
            for (AstNode* n = table_ctor->first_field; n; n = n->next_child)
            {
                auto* field = static_cast<TableField*>(n);
                if (field->key && reads_variable(field->key, var_name))
                {
                    return true;
                }
                if (reads_variable(field->value, var_name))
                {
                    return true;
                }
            }
            return false;
        }

        return false;
    }

    static void eliminate_in_block(DSEState& state, AstBlock& block)
    {
        Vector<std::pair<std::string_view, PendingStore>> pending;
        pending.init(state.holder.state(), 4);

        auto find_entry = [&](const std::string_view name) -> PendingStore* {
            for (auto& entry : pending)
            {
                if (entry.first == name)
                {
                    return &entry.second;
                }
            }
            return nullptr;
        };

        auto erase_entry = [&](const std::string_view name) {
            for (size_t i = 0; i < pending.size(); ++i)
            {
                if (pending[i].first == name)
                {
                    if (i + 1 != pending.size())
                    {
                        pending[i] = std::move(pending.back());
                    }
                    pending.pop_back();
                    return;
                }
            }
        };

        auto invalidate_reads = [&](const AstNode* stat) {
            for (size_t i = 0; i < pending.size();)
            {
                if (statement_reads_variable(stat, pending[i].first))
                {
                    if (i + 1 != pending.size())
                    {
                        pending[i] = std::move(pending.back());
                    }
                    pending.pop_back();
                }
                else
                {
                    ++i;
                }
            }
        };

        auto try_eliminate_previous = [&](const std::string_view name, AstNode* expr) {
            PendingStore* prev = find_entry(name);
            if (!prev)
            {
                return;
            }

            // Only safe to drop a previous store if it had no side effects and the new
            // expression does not depend on the old value.
            if (!expression_has_side_effects(prev->expr) && !reads_variable(expr, name))
            {
                if (auto* prev_decl = prev->stat->try_as<AstLocalDecl>())
                {
                    // Preserve the declaration but drop its initializer.
                    if (prev_decl->first_init)
                    {
                        prev_decl->first_init = nullptr;
                        state.changed = true;
                        if constexpr (kOptimizationPassDebug)
                        {
                            state.removed_count++;
                        }
                    }
                }
                else
                {
                    unlink_statement(*prev->block, prev->stat);
                    state.changed = true;
                    if constexpr (kOptimizationPassDebug)
                    {
                        state.removed_count++;
                    }
                }
            }
            erase_entry(name);
        };

        for (AstNode** link = &block.first_stat; *link != nullptr; link = &(*link)->next_child)
        {
            AstNode* stat = *link;

            // First, invalidate any pending stores that are consumed here.
            invalidate_reads(stat);

            if (auto* local_decl = stat->try_as<AstLocalDecl>())
            {
                // Track only simple single-variable declarations with an initializer.
                if (!local_decl->is_const && local_decl->first_name && !local_decl->first_name->next_child
                    && local_decl->first_init && !local_decl->first_init->next_child)
                {
                    const std::string_view name = local_decl->first_name->view();
                    AstNode* init_expr = local_decl->first_init;

                    // If this declaration reads the variable, earlier value is required.
                    if (!reads_variable(init_expr, name))
                    {
                        try_eliminate_previous(name, init_expr);

                        if (!expression_has_side_effects(init_expr))
                        {
                            pending.emplace_back(
                                state.holder.state(), std::pair{ name, PendingStore{ &block, stat, init_expr } });
                        }
                    }
                    else
                    {
                        erase_entry(name);
                    }
                }
                else
                {
                    pending.clear();
                }
            }
            else if (auto* assign_local = stat->try_as<AstAssignLocal>())
            {
                const std::string_view name = assign_local->name->view();
                AstNode* expr = assign_local->expr;

                if (!reads_variable(expr, name))
                {
                    try_eliminate_previous(name, expr);

                    if (!expression_has_side_effects(expr))
                    {
                        pending.emplace_back(state.holder.state(), std::pair{ name, PendingStore{ &block, stat, expr } });
                    }
                }
                else
                {
                    erase_entry(name);
                }
            }
            else
            {
                // Handle nested blocks, but treat this statement as a barrier for the
                // simple linear analysis we are doing here.
                if (auto* if_stat = stat->try_as<AstIf>())
                {
                    if (if_stat->then_block)
                    {
                        eliminate_in_block(state, *if_stat->then_block);
                    }
                    for (ElseIf* elseif = if_stat->first_elseif; elseif != nullptr;
                        elseif = static_cast<ElseIf*>(elseif->next_child))
                    {
                        if (elseif->block)
                        {
                            eliminate_in_block(state, *elseif->block);
                        }
                    }
                    if (if_stat->else_block)
                    {
                        eliminate_in_block(state, *if_stat->else_block);
                    }
                }
                else if (auto* while_stat = stat->try_as<AstWhile>())
                {
                    if (while_stat->block)
                    {
                        eliminate_in_block(state, *while_stat->block);
                    }
                }
                else if (auto* for_c = stat->try_as<AstForC>())
                {
                    if (for_c->block)
                    {
                        eliminate_in_block(state, *for_c->block);
                    }
                }
                else if (auto* for_num = stat->try_as<AstForNum>())
                {
                    if (for_num->block)
                    {
                        eliminate_in_block(state, *for_num->block);
                    }
                }
                else if (auto* for_in = stat->try_as<AstForIn>())
                {
                    if (for_in->block)
                    {
                        eliminate_in_block(state, *for_in->block);
                    }
                }
                else if (auto* func_def_stat = stat->try_as<AstFuncDefStat>())
                {
                    if (func_def_stat->block)
                    {
                        eliminate_in_block(state, *func_def_stat->block);
                    }
                }
                else if (auto* scope_stat = stat->try_as<AstScope>())
                {
                    if (scope_stat->block)
                    {
                        eliminate_in_block(state, *scope_stat->block);
                    }
                }

                pending.clear();
            }
        }

        pending.destroy(state.holder.state());
    }

    bool DeadStoreEliminationPass::apply(AstOptimizationContext& context)
    {
        DSEState state{ context.holder };

        if (context.program->block)
        {
            eliminate_in_block(state, *context.program->block);
        }

        if constexpr (kOptimizationPassDebug)
        {
            if (state.removed_count > 0)
            {
                println("    Removed {} dead stores", state.removed_count);
            }
        }

        return state.changed;
    }

} // namespace behl
