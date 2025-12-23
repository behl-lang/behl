#pragma once

#include "ast/ast.hpp"
#include "common/print.hpp"

#include <string>

namespace behl
{
    class AstDumper : public AstVisitor
    {
    private:
        int32_t indent_level = 0;

        void print_indent()
        {
            for (int i = 0; i < indent_level; ++i)
            {
                print("  ");
            }
        }

        void enter()
        {
            ++indent_level;
        }

        void leave()
        {
            --indent_level;
        }

    public:
        void visit(const AstProgram& node) override
        {
            print_indent();
            println("AstProgram");
            enter();
            if (node.block)
            {
                node.block->accept(*this);
            }
            leave();
        }

        void visit(const AstBlock& node) override
        {
            print_indent();
            println("AstBlock");
            enter();
            for (AstNode* stmt = node.first_stat; stmt; stmt = stmt->next_child)
            {
                stmt->accept(*this);
            }
            leave();
        }

        void visit(const AstLocalDecl& node) override
        {
            print_indent();
            println("AstLocalDecl (const={})", node.is_const);
            enter();
            for (AstNode* name = node.first_name; name; name = name->next_child)
            {
                print_indent();
                println("Name: {}", static_cast<const AstString*>(name)->view());
            }
            for (auto* init = node.first_init; init; init = init->next_child)
            {
                init->accept(*this);
            }
            leave();
        }

        void visit(const AstForC& node) override
        {
            print_indent();
            println("AstForC");
            enter();

            if (node.init)
            {
                print_indent();
                println("Init:");
                enter();
                node.init->accept(*this);
                leave();
            }

            if (node.condition)
            {
                print_indent();
                println("Condition:");
                enter();
                node.condition->accept(*this);
                leave();
            }

            if (node.update)
            {
                print_indent();
                println("Update:");
                enter();
                node.update->accept(*this);
                leave();
            }

            if (node.block)
            {
                print_indent();
                println("Block:");
                enter();
                node.block->accept(*this);
                leave();
            }

            leave();
        }

        void visit(const AstForCNumeric& node) override
        {
            print_indent();
            println("AstForCNumeric (ascending={}, inclusive={})", node.ascending, node.inclusive);
            enter();

            print_indent();
            println("Var: {}", node.var->view());

            print_indent();
            println("Start:");
            enter();
            node.start->accept(*this);
            leave();

            print_indent();
            println("End:");
            enter();
            node.end->accept(*this);
            leave();

            if (node.step)
            {
                print_indent();
                println("Step:");
                enter();
                node.step->accept(*this);
                leave();
            }

            if (node.block)
            {
                print_indent();
                println("Block:");
                enter();
                node.block->accept(*this);
                leave();
            }

            leave();
        }

        void visit(const AstBinOp& node) override
        {
            print_indent();
            println("AstBinOp (op={})", static_cast<int>(node.op));
            enter();
            node.left->accept(*this);
            node.right->accept(*this);
            leave();
        }

        void visit(const AstIdent& node) override
        {
            print_indent();
            println("AstIdent: {}", node.name->view());
        }

        void visit(const AstInt& node) override
        {
            print_indent();
            println("AstInt: {}", node.value);
        }

        void visit(const AstFP& node) override
        {
            print_indent();
            println("AstFloat: {}", node.value);
        }

        void visit(const AstIncrement& node) override
        {
            print_indent();
            println("AstIncrement");
            enter();
            node.target->accept(*this);
            leave();
        }

        void visit(const AstDecrement& node) override
        {
            print_indent();
            println("AstDecrement");
            enter();
            node.target->accept(*this);
            leave();
        }

        void visit(const AstCompoundAssign& node) override
        {
            print_indent();
            println("AstCompoundAssign (op={})", static_cast<int>(node.op));
            enter();
            node.target->accept(*this);
            node.expr->accept(*this);
            leave();
        }

        void visit(const AstCompoundLocal& node) override
        {
            print_indent();
            println("AstCompoundLocal (name={}, op={})", node.name->view(), static_cast<int>(node.op));
            enter();
            node.expr->accept(*this);
            leave();
        }

        // Stub implementations for other node types
        void visit(const AstNil&) override
        {
            print_indent();
            println("AstNil");
        }
        void visit(const AstBool& node) override
        {
            print_indent();
            println("AstBool: {}", node.value);
        }
        void visit(const AstString& node) override
        {
            print_indent();
            println("AstString: {}", node.view());
        }
        void visit(const AstVararg&) override
        {
            print_indent();
            println("AstVararg");
        }
        void visit(const AstUnOp&) override
        {
            print_indent();
            println("AstUnOp");
        }
        void visit(const AstFuncCall&) override
        {
            print_indent();
            println("AstFuncCall");
        }
        void visit(const AstTableCtor&) override
        {
            print_indent();
            println("AstTableCtor");
        }
        void visit(const AstIndex&) override
        {
            print_indent();
            println("AstIndex");
        }
        void visit(const AstMember&) override
        {
            print_indent();
            println("AstMember");
        }
        void visit(const AstFuncDef&) override
        {
            print_indent();
            println("AstFuncDef");
        }
        void visit(const AstAssign&) override
        {
            print_indent();
            println("AstAssign");
        }
        void visit(const AstAssignLocal&) override
        {
            print_indent();
            println("AstAssignLocal");
        }
        void visit(const AstAssignGlobal&) override
        {
            print_indent();
            println("AstAssignGlobal");
        }
        void visit(const AstAssignUpvalue&) override
        {
            print_indent();
            println("AstAssignUpvalue");
        }
        void visit(const AstCompoundUpvalue&) override
        {
            print_indent();
            println("AstCompoundUpvalue");
        }
        void visit(const AstCompoundGlobal&) override
        {
            print_indent();
            println("AstCompoundGlobal");
        }
        void visit(const AstIncLocal&) override
        {
            print_indent();
            println("AstIncLocal");
        }
        void visit(const AstIncGlobal&) override
        {
            print_indent();
            println("AstIncGlobal");
        }
        void visit(const AstIncUpvalue&) override
        {
            print_indent();
            println("AstIncUpvalue");
        }
        void visit(const AstDecLocal&) override
        {
            print_indent();
            println("AstDecLocal");
        }
        void visit(const AstDecGlobal&) override
        {
            print_indent();
            println("AstDecGlobal");
        }
        void visit(const AstDecUpvalue&) override
        {
            print_indent();
            println("AstDecUpvalue");
        }
        void visit(const AstIf&) override
        {
            print_indent();
            println("AstIf");
        }
        void visit(const AstWhile&) override
        {
            print_indent();
            println("AstWhile");
        }
        void visit(const AstForNum&) override
        {
            print_indent();
            println("AstForNum");
        }
        void visit(const AstForIn&) override
        {
            print_indent();
            println("AstForIn");
        }
        void visit(const AstFuncDefStat&) override
        {
            print_indent();
            println("AstFuncDefStat");
        }
        void visit(const AstReturn&) override
        {
            print_indent();
            println("AstReturn");
        }
        void visit(const AstBreak&) override
        {
            print_indent();
            println("AstBreak");
        }
        void visit(const AstContinue&) override
        {
            print_indent();
            println("AstContinue");
        }
        void visit(const AstScope&) override
        {
            print_indent();
            println("AstScope");
        }
        void visit(const AstExprStat&) override
        {
            print_indent();
            println("AstExprStat");
        }
        void visit(const AstModuleDecl&) override
        {
            print_indent();
            println("AstModuleDecl");
        }
        void visit(const AstExportDecl&) override
        {
            print_indent();
            println("AstExportDecl");
        }
        void visit(const AstExportList&) override
        {
            print_indent();
            println("AstExportList");
        }
        void visit(const AstTernary&) override
        {
            print_indent();
            println("AstTernary");
        }
    };

    inline void dump_ast(AstProgram* program)
    {
        println("\n=== AST DUMP ===");
        AstDumper dumper;
        program->accept(dumper);
        println("================\n");
    }

} // namespace behl
