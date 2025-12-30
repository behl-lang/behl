#include "frontend/semantics_pass.hpp"

#include "ast/ast_holder.hpp"
#include "common/format.hpp"
#include "common/hash_map.hpp"
#include "exceptions.hpp"
#include "memory.hpp"
#include "state.hpp"

namespace behl
{
    struct Scope
    {
        AutoHashMap<std::string_view, bool> locals;
        Scope* parent = nullptr;
        bool is_function_scope = false;

        Scope(State* state)
            : locals(state)
        {
        }

        ~Scope() = default;
    };

    struct SemanticsState
    {
        State* state;
        AstHolder& holder;
        Scope* current_scope = nullptr;
        std::string_view current_function_name{};  // Name of the function currently being analyzed
        bool is_module = false;                    // Module mode flag
        AstString** exported_names_tail = nullptr; // Tail pointer for appending to program's exported_names
    };

    struct ScopeGuard
    {
        SemanticsState& state;

        explicit ScopeGuard(SemanticsState& s, bool is_function = false)
            : state(s)
        {
            auto* new_scope = mem_create<Scope>(s.state, s.state);
            new_scope->parent = s.current_scope;
            new_scope->is_function_scope = is_function;
            s.current_scope = new_scope;
        }

        ~ScopeGuard()
        {
            if (state.current_scope)
            {
                auto* old_scope = state.current_scope;
                state.current_scope = state.current_scope->parent;
                mem_destroy(state.state, old_scope);
            }
        }

        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
    };

    static void transform_block(SemanticsState& state, AstBlock& block);
    static void transform_expression(SemanticsState& state, AstNode*& expr);

    static constexpr std::string_view kBuiltins[] = { "print", "typeof", "typeid", "getmetatable", "setmetatable", "rawlen",
        "pairs", "import", "error", "pcall", "tostring", "tonumber" };

    static bool is_builtin_function(std::string_view name)
    {
        for (const auto& builtin : kBuiltins)
        {
            if (name == builtin)
            {
                return true;
            }
        }
        return false;
    }

    void declare_local(SemanticsState& state, std::string_view name)
    {
        if (state.current_scope)
        {
            state.current_scope->locals.insert_or_assign(name, true);
        }
    }

    bool is_local(SemanticsState& state, std::string_view name)
    {
        Scope* scope = state.current_scope;
        while (scope)
        {
            if (scope->locals.contains(name))
            {
                return true;
            }

            if (scope->is_function_scope)
            {
                break;
            }
            scope = scope->parent;
        }
        return false;
    }

    bool is_upvalue(SemanticsState& state, std::string_view name)
    {
        if (!state.current_scope)
        {
            return false;
        }

        Scope* scope = state.current_scope;
        while (scope && !scope->is_function_scope)
        {
            scope = scope->parent;
        }

        if (scope && scope->parent)
        {
            scope = scope->parent;
            while (scope)
            {
                if (scope->locals.contains(name))
                {
                    return true;
                }
                scope = scope->parent;
            }
        }
        return false;
    }

    AstNode* transform_assign(SemanticsState& state, AstAssign* assign)
    {
        size_t var_count = 0, expr_count = 0;
        for (AstNode* v = assign->first_var; v; v = v->next_child)
        {
            ++var_count;
        }
        for (AstNode* e = assign->first_expr; e; e = e->next_child)
        {
            ++expr_count;
        }

        if (var_count != 1 || expr_count != 1)
        {
            return assign;
        }

        auto* var_ident = assign->first_var->try_as<AstIdent>();
        if (!var_ident)
        {
            return assign;
        }

        const auto var_name = var_ident->name->view();

        // In module mode, disallow bare assignments (globals) except to stdlib
        if (state.is_module && !is_local(state, var_name) && !is_upvalue(state, var_name))
        {
            const auto msg = behl::format(
                "Cannot assign to undeclared variable '{}' in module mode. Use 'let' or 'const' to declare variables.",
                var_name);

            throw RuntimeError(msg);
        }

        if (auto* binop = assign->first_expr->try_as<AstBinOp>())
        {
            if (auto* lhs_ident = binop->left->try_as<AstIdent>())
            {
                if (lhs_ident->name->view() == var_name)
                {
                    if (binop->op == TokenType::kPlus || binop->op == TokenType::kMinus || binop->op == TokenType::kStar
                        || binop->op == TokenType::kSlash || binop->op == TokenType::kPercent)
                    {
                        if (is_local(state, var_name))
                        {
                            auto* node = state.holder.make<AstCompoundLocal>(var_ident->name, binop->op, binop->right);
                            node->line = assign->line;
                            node->column = assign->column;
                            return node;
                        }
                        else if (is_upvalue(state, var_name))
                        {
                            auto* node = state.holder.make<AstCompoundUpvalue>(var_ident->name, binop->op, binop->right);
                            node->line = assign->line;
                            node->column = assign->column;
                            return node;
                        }
                        else
                        {
                            auto* node = state.holder.make<AstCompoundGlobal>(var_ident->name, binop->op, binop->right);
                            node->line = assign->line;
                            node->column = assign->column;
                            return node;
                        }
                    }
                }
            }
        }

        if (is_local(state, var_name))
        {
            return state.holder.make<AstAssignLocal>(var_ident->name, assign->first_expr);
        }
        else if (is_upvalue(state, var_name))
        {
            return state.holder.make<AstAssignUpvalue>(var_ident->name, assign->first_expr);
        }
        else
        {
            return state.holder.make<AstAssignGlobal>(var_ident->name, assign->first_expr);
        }
    }

    AstNode* transform_compound_assign(SemanticsState& state, AstCompoundAssign* compound)
    {
        auto* var_ident = compound->target->try_as<AstIdent>();
        if (!var_ident)
        {
            return compound;
        }

        const auto var_name = var_ident->name->view();

        if (is_local(state, var_name))
        {
            auto* node = state.holder.make<AstCompoundLocal>(var_ident->name, compound->op, compound->expr);
            node->line = compound->line;
            node->column = compound->column;
            return node;
        }
        else if (is_upvalue(state, var_name))
        {
            auto* node = state.holder.make<AstCompoundUpvalue>(var_ident->name, compound->op, compound->expr);
            node->line = compound->line;
            node->column = compound->column;
            return node;
        }
        else
        {
            auto* node = state.holder.make<AstCompoundGlobal>(var_ident->name, compound->op, compound->expr);
            node->line = compound->line;
            node->column = compound->column;
            return node;
        }
    }

    AstNode* transform_increment(SemanticsState& state, AstIncrement* inc)
    {
        auto* var_ident = inc->target->try_as<AstIdent>();
        if (!var_ident)
        {
            return inc;
        }

        const auto var_name = var_ident->name->view();

        if (is_local(state, var_name))
        {
            auto* node = state.holder.make<AstIncLocal>(var_ident->name);
            node->line = inc->line;
            node->column = inc->column;
            return node;
        }
        else if (is_upvalue(state, var_name))
        {
            auto* node = state.holder.make<AstIncUpvalue>(var_ident->name);
            node->line = inc->line;
            node->column = inc->column;
            return node;
        }
        else
        {
            auto* node = state.holder.make<AstIncGlobal>(var_ident->name);
            node->line = inc->line;
            node->column = inc->column;
            return node;
        }
    }

    AstNode* transform_decrement(SemanticsState& state, AstDecrement* dec)
    {
        auto* var_ident = dec->target->try_as<AstIdent>();
        if (!var_ident)
        {
            return dec;
        }

        const auto var_name = var_ident->name->view();

        if (is_local(state, var_name))
        {
            auto* node = state.holder.make<AstDecLocal>(var_ident->name);
            node->line = dec->line;
            node->column = dec->column;
            return node;
        }
        else if (is_upvalue(state, var_name))
        {
            auto* node = state.holder.make<AstDecUpvalue>(var_ident->name);
            node->line = dec->line;
            node->column = dec->column;
            return node;
        }
        else
        {
            auto* node = state.holder.make<AstDecGlobal>(var_ident->name);
            node->line = dec->line;
            node->column = dec->column;
            return node;
        }
    }

    void transform_function(SemanticsState& state, AstFuncDef& func)
    {
        ScopeGuard scope(state, true);

        if (func.is_method)
        {
            declare_local(state, "self");
        }
        for (AstNode* p = func.first_param; p; p = p->next_child)
        {
            auto* param = static_cast<AstString*>(p);
            declare_local(state, param->view());
        }

        if (func.block)
        {
            transform_block(state, *func.block);
        }
    }

    void transform_function_stat(SemanticsState& state, AstFuncDefStat& func)
    {
        // Save and set current function name (use first part for simple names)
        const auto saved_name = state.current_function_name;
        if (func.first_name_part)
        {
            auto* first_name = static_cast<AstString*>(func.first_name_part);
            state.current_function_name = first_name->view();
        }

        ScopeGuard scope(state, true);

        if (func.is_method)
        {
            declare_local(state, "self");
        }
        for (AstNode* p = func.first_param; p; p = p->next_child)
        {
            auto* param = static_cast<AstString*>(p);
            declare_local(state, param->view());
        }

        if (func.block)
        {
            transform_block(state, *func.block);
        }

        // Restore previous function name
        state.current_function_name = saved_name;
    }

    // Transform expressions (handles nested function expressions)
    static void transform_expression(SemanticsState& state, AstNode*& expr)
    {
        if (!expr)
        {
            return;
        }

        // Check identifier access in module mode - block ALL global access
        if (auto* ident = expr->try_as<AstIdent>())
        {
            const auto name = ident->name->view();

            // In module mode, any non-local, non-upvalue, non-builtin variable is an error
            if (state.is_module && !is_local(state, name) && !is_upvalue(state, name) && !is_builtin_function(name))
            {
                const auto msg = behl::format("Variable '{}' is not declared. Use 'let' or 'const' to declare local variables, "
                                              "or 'import()' to load modules.",
                    name);
                throw SemanticError(msg);
            }
        }

        // Check if this is a function call that might be a self-call
        if (auto* call = expr->try_as<AstFuncCall>())
        {
            // Check if calling an identifier that matches the current function name
            if (const auto* ident = call->func->try_as<AstIdent>())
            {
                if (!state.current_function_name.empty() && ident->name->view() == state.current_function_name)
                {
                    // This is a self-call!
                    call->is_self_call = true;
                }
            }

            // Recursively transform the function expression and arguments
            transform_expression(state, call->func);
            for (AstNode* arg = call->first_arg; arg; arg = arg->next_child)
            {
                transform_expression(state, arg);
            }
            return;
        }

        // Handle function expressions
        if (auto* func_def = expr->try_as<AstFuncDef>())
        {
            transform_function(state, *func_def);
            return;
        }

        // Recursively transform sub-expressions
        if (auto* binop = expr->try_as<AstBinOp>())
        {
            transform_expression(state, binop->left);
            transform_expression(state, binop->right);
        }
        else if (auto* unop = expr->try_as<AstUnOp>())
        {
            transform_expression(state, unop->expr);
        }
        else if (auto* ternary = expr->try_as<AstTernary>())
        {
            transform_expression(state, ternary->condition);
            transform_expression(state, ternary->true_expr);
            transform_expression(state, ternary->false_expr);
        }
        else if (auto* call = expr->try_as<AstFuncCall>())
        {
            transform_expression(state, call->func);
            for (AstNode* arg = call->first_arg; arg; arg = arg->next_child)
            {
                transform_expression(state, arg);
            }
        }
        else if (auto* index = expr->try_as<AstIndex>())
        {
            transform_expression(state, index->table);
            transform_expression(state, index->key);
        }
        else if (auto* member = expr->try_as<AstMember>())
        {
            transform_expression(state, member->table);
        }
        else if (auto* table_ctor = expr->try_as<AstTableCtor>())
        {
            for (AstNode* n = table_ctor->first_field; n; n = n->next_child)
            {
                auto* field = static_cast<TableField*>(n);
                if (field->key)
                {
                    transform_expression(state, field->key);
                }
                transform_expression(state, field->value);
            }
        }
    }

    AstNode* transform_statement(SemanticsState& state, AstNode* node)
    {
        // Handle blocks (for multiple init/update statements in for loops)
        if (auto* block = node->try_as<AstBlock>())
        {
            transform_block(state, *block);
            return node;
        }

        // Handle export statements
        if (auto* export_decl = node->try_as<AstExportDecl>())
        {
            if (!state.is_module)
            {
                throw std::runtime_error("'export' can only be used in module mode. Add 'module;' at the top of the file.");
            }

            // Transform the inner declaration and collect export name
            export_decl->declaration = transform_statement(state, export_decl->declaration);

            // Extract exported name and add to list
            if (auto* func_def = export_decl->declaration->try_as<AstFuncDefStat>())
            {
                if (func_def->first_name_part)
                {
                    *state.exported_names_tail = static_cast<AstString*>(func_def->first_name_part);
                    state.exported_names_tail = reinterpret_cast<AstString**>(&func_def->first_name_part->next_child);
                }
            }
            else if (auto* local_decl = export_decl->declaration->try_as<AstLocalDecl>())
            {
                // Can only export const, not let
                if (!local_decl->is_const)
                {
                    throw std::runtime_error("Cannot export mutable variables. Only 'const' can be exported.");
                }
                for (AstNode* n = reinterpret_cast<AstNode*>(local_decl->first_name); n; n = n->next_child)
                {
                    auto* name = static_cast<AstString*>(n);
                    *state.exported_names_tail = name;
                    state.exported_names_tail = reinterpret_cast<AstString**>(&name->next_child);
                }
            }

            return export_decl;
        }
        else if (auto* export_list = node->try_as<AstExportList>())
        {
            if (!state.is_module)
            {
                throw std::runtime_error("'export' can only be used in module mode. Add 'module;' at the top of the file.");
            }

            // Add all names to exported list (need to clone to avoid breaking the chain)
            for (AstString* name = export_list->first_name; name != nullptr; name = static_cast<AstString*>(name->next_child))
            {
                auto cloned_name = state.holder.make_string(name->view());
                *state.exported_names_tail = cloned_name;
                state.exported_names_tail = reinterpret_cast<AstString**>(&cloned_name->next_child);
            }

            return export_list;
        }

        if (auto* assign = node->try_as<AstAssign>())
        {
            return transform_assign(state, assign);
        }
        else if (auto* compound = node->try_as<AstCompoundAssign>())
        {
            return transform_compound_assign(state, compound);
        }
        else if (auto* inc = node->try_as<AstIncrement>())
        {
            return transform_increment(state, inc);
        }
        else if (auto* dec = node->try_as<AstDecrement>())
        {
            return transform_decrement(state, dec);
        }
        else if (auto* local_decl = node->try_as<AstLocalDecl>())
        {
            for (AstNode* n = reinterpret_cast<AstNode*>(local_decl->first_name); n; n = n->next_child)
            {
                auto* name = static_cast<AstString*>(n);
                declare_local(state, name->view());
            }
            // Transform initializer expressions
            for (AstNode* init = local_decl->first_init; init; init = init->next_child)
            {
                transform_expression(state, init);
            }
            return node;
        }
        else if (auto* if_stmt = node->try_as<AstIf>())
        {
            if (if_stmt->then_block)
            {
                transform_block(state, *if_stmt->then_block);
            }
            for (ElseIf* elseif = if_stmt->first_elseif; elseif != nullptr; elseif = static_cast<ElseIf*>(elseif->next_child))
            {
                if (elseif->block)
                {
                    transform_block(state, *elseif->block);
                }
            }
            if (if_stmt->else_block)
            {
                transform_block(state, *if_stmt->else_block);
            }
            return node;
        }
        else if (auto* while_stmt = node->try_as<AstWhile>())
        {
            if (while_stmt->block)
            {
                transform_block(state, *while_stmt->block);
            }
            return node;
        }
        else if (auto* for_num = node->try_as<AstForNum>())
        {
            ScopeGuard scope(state);
            declare_local(state, for_num->var->view());
            if (for_num->block)
            {
                transform_block(state, *for_num->block);
            }
            return node;
        }
        else if (auto* for_in = node->try_as<AstForIn>())
        {
            ScopeGuard scope(state);
            for (const AstNode* name = for_in->first_name; name != nullptr; name = name->next_child)
            {
                auto* name_str = static_cast<const AstString*>(name);
                declare_local(state, name_str->view());
            }
            if (for_in->block)
            {
                transform_block(state, *for_in->block);
            }
            return node;
        }
        else if (auto* for_c = node->try_as<AstForC>())
        {
            // Regular ForC handling (optimization moved to LoopOptimizationPass)
            ScopeGuard scope(state);

            if (for_c->init)
            {
                for_c->init = transform_statement(state, for_c->init);
            }

            if (for_c->update)
            {
                for_c->update = transform_statement(state, for_c->update);
            }

            if (for_c->block)
            {
                transform_block(state, *for_c->block);
            }
            return node;
        }
        else if (auto* for_c_numeric = node->try_as<AstForCNumeric>())
        {
            // Handle optimized numeric for loops (created by LoopOptimizationPass)
            ScopeGuard scope(state);
            declare_local(state, for_c_numeric->var->view());
            if (for_c_numeric->block)
            {
                transform_block(state, *for_c_numeric->block);
            }
            return node;
        }
        else if (auto* func_def = node->try_as<AstFuncDefStat>())
        {
            if (func_def->is_local && func_def->first_name_part)
            {
                auto* first_name = static_cast<AstString*>(func_def->first_name_part);
                declare_local(state, first_name->view());
            }
            transform_function_stat(state, *func_def);
            return node;
        }
        else if (auto* scope_block = node->try_as<AstScope>())
        {
            ScopeGuard scope(state);
            if (scope_block->block)
            {
                transform_block(state, *scope_block->block);
            }
            return node;
        }

        // Transform expressions in other statement types
        if (auto* ret = node->try_as<AstReturn>())
        {
            for (AstNode* expr = ret->first_expr; expr; expr = expr->next_child)
            {
                transform_expression(state, expr);
            }
        }
        else if (auto* expr_stat = node->try_as<AstExprStat>())
        {
            transform_expression(state, expr_stat->expr);
        }

        return node;
    }

    static void transform_block(SemanticsState& state, AstBlock& block)
    {
        AstNode* new_first = nullptr;
        AstNode** tail = &new_first;

        for (AstNode* stat = block.first_stat; stat != nullptr; stat = stat->next_child)
        {
            *tail = transform_statement(state, stat);
            tail = &(*tail)->next_child;
        }

        block.first_stat = new_first;
    }

    AstProgram* SemanticsPass::apply(State* state, AstHolder& holder, AstProgram* program)
    {
        SemanticsState sem_state{ state, holder };
        sem_state.is_module = program->is_module;
        sem_state.exported_names_tail = &program->first_exported_name;

        ScopeGuard root_scope(sem_state);

        // In module mode, function declarations are local by default
        if (sem_state.is_module && program->block)
        {
            // We need to pre-declare all function names as locals
            // so they don't become globals
            for (AstNode* stat = program->block->first_stat; stat != nullptr; stat = stat->next_child)
            {
                if (auto* func_def = stat->try_as<AstFuncDefStat>())
                {
                    if (!func_def->is_local && func_def->first_name_part)
                    {
                        auto* first_name = static_cast<AstString*>(func_def->first_name_part);
                        declare_local(sem_state, first_name->view());
                    }
                }
                else if (auto* export_decl = stat->try_as<AstExportDecl>())
                {
                    if (auto* export_func_def = export_decl->declaration->try_as<AstFuncDefStat>())
                    {
                        if (export_func_def->first_name_part)
                        {
                            auto* first_name = static_cast<AstString*>(export_func_def->first_name_part);
                            declare_local(sem_state, first_name->view());
                        }
                    }
                }
            }
        }

        if (program->block)
        {
            transform_block(sem_state, *program->block);
        }

        return program;
    }

} // namespace behl
