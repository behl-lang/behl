#pragma once

#include "ast/ast.hpp"
#include "common/string.hpp"

#include <string>
#include <vector>

namespace behl
{

    struct Symbol
    {
        String name;
        bool is_function;
        bool is_constant;
        int line;
        int column;
        std::vector<std::string> parameters; // Function parameters (if is_function)
        bool is_vararg = false;              // Whether function accepts varargs
    };

    class SymbolCollector : public AstVisitor
    {
    public:
        SymbolCollector() = default;

        // Collect symbols from an AST up to a given line
        std::vector<Symbol> collect(const AstNode* ast, int max_line = -1);

        // AstVisitor interface - implement only what we need
        void visit(const AstNil&) override
        {
        }
        void visit(const AstBool&) override
        {
        }
        void visit(const AstInt&) override
        {
        }
        void visit(const AstFP&) override
        {
        }
        void visit(const AstString&) override
        {
        }
        void visit(const AstVararg&) override
        {
        }
        void visit(const AstIdent&) override
        {
        }
        void visit(const AstBinOp&) override;
        void visit(const AstUnOp&) override;
        void visit(const AstTernary&) override;
        void visit(const AstFuncCall&) override;
        void visit(const AstTableCtor&) override;
        void visit(const AstIndex&) override;
        void visit(const AstMember&) override;
        void visit(const AstFuncDef&) override
        {
        }

        void visit(const AstAssign&) override;
        void visit(const AstAssignLocal&) override
        {
        }
        void visit(const AstAssignGlobal&) override
        {
        }
        void visit(const AstAssignUpvalue&) override
        {
        }
        void visit(const AstCompoundAssign&) override;
        void visit(const AstCompoundLocal&) override
        {
        }
        void visit(const AstCompoundGlobal&) override
        {
        }
        void visit(const AstCompoundUpvalue&) override
        {
        }
        void visit(const AstIncrement&) override;
        void visit(const AstIncLocal&) override
        {
        }
        void visit(const AstIncGlobal&) override
        {
        }
        void visit(const AstIncUpvalue&) override
        {
        }
        void visit(const AstDecrement&) override;
        void visit(const AstDecLocal&) override
        {
        }
        void visit(const AstDecGlobal&) override
        {
        }
        void visit(const AstDecUpvalue&) override
        {
        }
        void visit(const AstLocalDecl&) override;
        void visit(const AstIf&) override;
        void visit(const AstWhile&) override;
        void visit(const AstForNum&) override;
        void visit(const AstForIn&) override;
        void visit(const AstForC&) override;
        void visit(const AstForCNumeric&) override;
        void visit(const AstFuncDefStat&) override;
        void visit(const AstReturn&) override;
        void visit(const AstBreak&) override
        {
        }
        void visit(const AstContinue&) override
        {
        }
        void visit(const AstDefer&) override;
        void visit(const AstScope&) override;
        void visit(const AstExprStat&) override;

        void visit(const AstBlock&) override;
        void visit(const AstProgram&) override;
        void visit(const AstModuleDecl&) override;
        void visit(const AstExportDecl&) override;
        void visit(const AstExportList&) override;

    private:
        std::vector<Symbol> symbols_;
        int max_line_ = -1; // Maximum line to collect symbols from (-1 = no limit)

        void visit_children(const AstNode* node);
    };

} // namespace behl
