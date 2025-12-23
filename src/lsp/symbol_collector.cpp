#include "symbol_collector.hpp"

#include "ast/ast.hpp"

namespace behl
{

    std::vector<Symbol> SymbolCollector::collect(const AstNode* ast, int max_line)
    {
        symbols_.clear();
        max_line_ = max_line;
        if (ast)
        {
            ast->accept(*this);
        }
        return symbols_;
    }

    void SymbolCollector::visit_children(const AstNode* node)
    {
        if (!node)
        {
            return;
        }

        // The visitor pattern automatically handles traversal in accept() methods
        // We don't need to manually traverse here
    }

    // Visit methods that need to traverse children
    void SymbolCollector::visit(const AstBinOp& node)
    {
        if (node.left)
        {
            node.left->accept(*this);
        }
        if (node.right)
        {
            node.right->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstUnOp& node)
    {
        if (node.expr)
        {
            node.expr->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstTernary& node)
    {
        if (node.condition)
        {
            node.condition->accept(*this);
        }
        if (node.true_expr)
        {
            node.true_expr->accept(*this);
        }
        if (node.false_expr)
        {
            node.false_expr->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstFuncCall& node)
    {
        if (node.func)
        {
            node.func->accept(*this);
        }
        for (AstNode* arg = node.first_arg; arg; arg = arg->next_child)
        {
            arg->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstTableCtor& node)
    {
        for (AstNode* field = node.first_field; field; field = field->next_child)
        {
            field->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstIndex& node)
    {
        if (node.table)
        {
            node.table->accept(*this);
        }
        if (node.key)
        {
            node.key->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstMember& node)
    {
        if (node.table)
        {
            node.table->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstAssign& node)
    {
        for (AstNode* var = node.first_var; var; var = var->next_child)
        {
            var->accept(*this);
        }
        for (AstNode* expr = node.first_expr; expr; expr = expr->next_child)
        {
            expr->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstCompoundAssign& node)
    {
        if (node.target)
        {
            node.target->accept(*this);
        }
        if (node.expr)
        {
            node.expr->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstIncrement& node)
    {
        if (node.target)
        {
            node.target->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstDecrement& node)
    {
        if (node.target)
        {
            node.target->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstLocalDecl& node)
    {
        // Only include if before max_line or no limit
        if (max_line_ < 0 || node.line < max_line_)
        {
            // Local variable declaration - extract first identifier
            if (node.first_name)
            {
                symbols_.push_back(Symbol{ .name = String(node.first_name->data, node.first_name->length),
                    .is_function = false,
                    .is_constant = node.is_const,
                    .line = node.line,
                    .column = node.column });
            }
        }

        // Visit initializers
        for (AstNode* init = node.first_init; init; init = init->next_child)
        {
            init->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstFuncDefStat& node)
    {
        // Only include if before max_line or no limit
        if (max_line_ < 0 || node.line < max_line_)
        {
            // Function declaration - extract name from first_name_part
            if (node.first_name_part)
            {
                if (auto* name_str = node.first_name_part->try_as<AstString>())
                {
                    // Extract parameters
                    std::vector<std::string> params;
                    for (AstNode* param = node.first_param; param; param = param->next_child)
                    {
                        if (auto* param_str = param->try_as<AstString>())
                        {
                            params.emplace_back(param_str->data, param_str->length);
                        }
                    }

                    symbols_.push_back(Symbol{ .name = String(name_str->data, name_str->length),
                        .is_function = true,
                        .is_constant = false,
                        .line = node.line,
                        .column = node.column,
                        .parameters = std::move(params),
                        .is_vararg = node.is_vararg });
                }
            }
        }

        // Visit function body
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstIf& node)
    {
        if (node.cond)
        {
            node.cond->accept(*this);
        }
        if (node.then_block)
        {
            node.then_block->accept(*this);
        }
        // TODO: Handle elseif chain
        if (node.else_block)
        {
            node.else_block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstWhile& node)
    {
        if (node.cond)
        {
            node.cond->accept(*this);
        }
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstForNum& node)
    {
        if (node.start)
        {
            node.start->accept(*this);
        }
        if (node.end)
        {
            node.end->accept(*this);
        }
        if (node.step)
        {
            node.step->accept(*this);
        }
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstForIn& node)
    {
        for (AstNode* expr = node.first_expr; expr; expr = expr->next_child)
        {
            expr->accept(*this);
        }
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstForC& node)
    {
        if (node.init)
        {
            node.init->accept(*this);
        }
        if (node.condition)
        {
            node.condition->accept(*this);
        }
        if (node.update)
        {
            node.update->accept(*this);
        }
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstForCNumeric& node)
    {
        if (node.start)
        {
            node.start->accept(*this);
        }
        if (node.end)
        {
            node.end->accept(*this);
        }
        if (node.step)
        {
            node.step->accept(*this);
        }
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstReturn& node)
    {
        for (AstNode* expr = node.first_expr; expr; expr = expr->next_child)
        {
            expr->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstDefer& node)
    {
        if (node.body)
        {
            node.body->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstScope& node)
    {
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstExprStat& node)
    {
        if (node.expr)
        {
            node.expr->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstBlock& node)
    {
        for (AstNode* stat = node.first_stat; stat; stat = stat->next_child)
        {
            stat->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstProgram& node)
    {
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstModuleDecl& node)
    {
        // Module declaration has no children - it's just a marker node
    }

    void SymbolCollector::visit(const AstExportDecl& node)
    {
        if (node.declaration)
        {
            node.declaration->accept(*this);
        }
    }

    void SymbolCollector::visit(const AstExportList& node)
    {
        // Just export list, no traversal needed
    }

} // namespace behl
