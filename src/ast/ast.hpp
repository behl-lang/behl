#pragma once

#include "ast/ast_holder.hpp"
#include "common/string.hpp"
#include "common/vector.hpp"
#include "frontend/lexer.hpp"

#include <behl/config.hpp>
#include <behl/types.hpp>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace behl
{
    enum class AstNodeType
    {
        kNil,
        kBool,
        kInteger,
        kFP,
        kString,
        kVararg,
        kIdent,
        kBinOp,
        kUnOp,
        kTernary,
        kFuncCall,
        kTableCtor,
        kIndex,
        kMember,
        kFuncDef,
        kAssign,
        kAssignLocal,
        kAssignGlobal,
        kAssignUpvalue,
        kCompoundAssign,
        kCompoundLocal,
        kCompoundGlobal,
        kCompoundUpvalue,
        kIncrement,
        kIncLocal,
        kIncGlobal,
        kIncUpvalue,
        kDecrement,
        kDecLocal,
        kDecGlobal,
        kDecUpvalue,
        kLocalDecl,
        kIf,
        kElseIf,
        kWhile,
        kForNum,
        kForIn,
        kForC,
        kForCNumeric,
        kFuncDefStat,
        kReturn,
        kBreak,
        kContinue,
        kDefer,
        kScope,
        kExprStat,
        kBlock,
        kProgram,
        kModuleDecl,
        kExportDecl,
        kExportList,
        kTableField
    };

    struct AstVisitor;

    struct AstNode
    {
        AstNodeType type;
        int32_t line = 1;
        int32_t column = 1;
        AstNode* next_child = nullptr;

        AstNode(AstNodeType t)
            : type(t)
        {
        }
        virtual ~AstNode() = default;
        virtual AstNode* clone(AstHolder& holder) const = 0;
        virtual void accept(AstVisitor& v) const = 0;

        bool is(AstNodeType t) const
        {
            return type == t;
        }
        template<typename T>
        T* as()
        {
            return static_cast<T*>(this);
        }
        template<typename T>
        const T* as() const
        {
            return static_cast<const T*>(this);
        }
        template<typename T>
        T* try_as()
        {
            return type == T::kType ? static_cast<T*>(this) : nullptr;
        }
        template<typename T>
        const T* try_as() const
        {
            return type == T::kType ? static_cast<const T*>(this) : nullptr;
        }
        template<typename T>
        T* clone_as(AstHolder& holder) const
        {
            auto* cloned = clone(holder);
            if (auto* ptr = cloned->try_as<T>())
            {
                return static_cast<T*>(ptr);
            }
            return nullptr;
        }

    protected:
        template<typename T, typename... TArgs>
        T* make_clone(AstHolder& holder, TArgs&&... args) const
        {
            auto* cloned = holder.make<T>(std::forward<TArgs>(args)...);
            return cloned;
        }
    };

    struct AstNil : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kNil;
        AstNil()
            : AstNode(AstNodeType::kNil)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstNil>(holder);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstBlock : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kBlock;
        AstNode* first_stat = nullptr;
        AstBlock()
            : AstNode(AstNodeType::kBlock)
        {
        }
        AstBlock(const AstBlock&) = delete;
        AstBlock& operator=(const AstBlock&) = delete;
        AstBlock(AstBlock&&) = default;
        AstBlock& operator=(AstBlock&&) = default;
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstBlock>(holder);
            AstNode** tail = &c->first_stat;
            for (AstNode* stat = first_stat; stat != nullptr; stat = stat->next_child)
            {
                *tail = stat->clone(holder);
                tail = &(*tail)->next_child;
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstProgram : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kProgram;
        AstBlock* block = nullptr;
        bool is_module = false;                   // Module mode flag
        AstString* first_exported_name = nullptr; // Accumulated export names (linked list)

        AstProgram()
            : AstNode(AstNodeType::kProgram)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstProgram>(holder);
            c->is_module = is_module;
            // Clone exported names linked list
            AstNode** tail = reinterpret_cast<AstNode**>(&c->first_exported_name);
            for (AstNode* name = reinterpret_cast<AstNode*>(first_exported_name); name; name = name->next_child)
            {
                auto* name_copy = name->clone(holder);
                *tail = name_copy;
                tail = &name_copy->next_child;
            }
            if (block)
            {
                c->block = static_cast<AstBlock*>(block->clone(holder));
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstBool : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kBool;
        bool value;
        AstBool(bool v)
            : AstNode(AstNodeType::kBool)
            , value(v)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstBool>(holder, value);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstInt : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kInteger;
        Integer value;
        explicit AstInt(Integer v)
            : AstNode(AstNodeType::kInteger)
            , value(v)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstInt>(holder, value);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstFP : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kFP;
        FP value;
        explicit AstFP(FP v)
            : AstNode(AstNodeType::kFP)
            , value(v)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstFP>(holder, value);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstString : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kString;
        char* data;
        size_t length;

        AstString(char* d, size_t len)
            : AstNode(AstNodeType::kString)
            , data(d)
            , length(len)
        {
        }

        std::string_view view() const
        {
            return std::string_view(data, length);
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return holder.make_string(view());
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstVararg : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kVararg;
        AstVararg()
            : AstNode(AstNodeType::kVararg)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstVararg>(holder);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstIdent : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kIdent;
        AstString* name;
        AstIdent(AstString* n)
            : AstNode(AstNodeType::kIdent)
            , name(n)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstIdent>(holder, static_cast<AstString*>(name->clone(holder)));
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstBinOp : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kBinOp;
        TokenType op;
        AstNode* left;
        AstNode* right;
        AstBinOp(TokenType o, AstNode* l, AstNode* r)
            : AstNode(AstNodeType::kBinOp)
            , op(o)
            , left(std::move(l))
            , right(std::move(r))
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* cloned = make_clone<AstBinOp>(holder, op, left->clone(holder), right->clone(holder));
            return cloned;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstUnOp : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kUnOp;
        TokenType op;
        AstNode* expr;
        AstUnOp(TokenType o, AstNode* e)
            : AstNode(AstNodeType::kUnOp)
            , op(o)
            , expr(std::move(e))
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstUnOp>(holder, op, expr->clone(holder));
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstTernary : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kTernary;
        AstNode* condition;
        AstNode* true_expr;
        AstNode* false_expr;

        AstTernary(AstNode* cond, AstNode* true_e, AstNode* false_e)
            : AstNode(AstNodeType::kTernary)
            , condition(std::move(cond))
            , true_expr(std::move(true_e))
            , false_expr(std::move(false_e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstTernary>(
                holder, condition->clone(holder), true_expr->clone(holder), false_expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstFuncCall : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kFuncCall;
        AstNode* func;
        AstNode* first_arg = nullptr;
        bool is_self_call = false; // Set by semantic analysis if calling the current function
        AstFuncCall(AstNode* f, AstNode* first_a = nullptr)
            : AstNode(AstNodeType::kFuncCall)
            , func(std::move(f))
            , first_arg(first_a)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstFuncCall>(holder, func->clone(holder));
            c->is_self_call = is_self_call;
            // Clone args linked list
            AstNode** tail = &c->first_arg;
            for (AstNode* arg = first_arg; arg; arg = arg->next_child)
            {
                auto* arg_copy = arg->clone(holder);
                *tail = arg_copy;
                tail = &arg_copy->next_child;
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct TableField : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kTableField;
        AstNode* key = nullptr; // nullptr for array-style entries
        AstNode* value = nullptr;

        TableField(AstNode* k, AstNode* v)
            : AstNode(AstNodeType::kTableField)
            , key(k)
            , value(v)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<TableField>(holder, key ? key->clone(holder) : nullptr, value->clone(holder));
        }
        void accept(AstVisitor& v) const override
        {
            (void)v; /* TableField doesn't visit */
        }
    };

    struct AstTableCtor : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kTableCtor;
        TableField* first_field = nullptr;
        AstTableCtor()
            : AstNode(AstNodeType::kTableCtor)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstTableCtor>(holder);
            // Clone fields linked list
            TableField** tail = &c->first_field;
            for (AstNode* node = first_field; node; node = node->next_child)
            {
                auto* field = static_cast<TableField*>(node);
                auto* field_copy = static_cast<TableField*>(field->clone(holder));
                *tail = field_copy;
                tail = reinterpret_cast<TableField**>(&field_copy->next_child);
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstIndex : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kIndex;
        AstNode* table;
        AstNode* key;
        AstIndex(AstNode* t, AstNode* k)
            : AstNode(AstNodeType::kIndex)
            , table(std::move(t))
            , key(std::move(k))
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstIndex>(holder, table->clone(holder), key->clone(holder));
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstMember : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kMember;
        AstNode* table;
        AstString* name;
        AstMember(AstNode* t, AstString* n)
            : AstNode(AstNodeType::kMember)
            , table(std::move(t))
            , name(n)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstMember>(holder, table->clone(holder), static_cast<AstString*>(name->clone(holder)));
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstFuncDef : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kFuncDef;
        AstString* first_param = nullptr;
        bool is_vararg = false;
        bool is_method = false;
        std::string_view self_name;
        AstBlock* block = nullptr;
        AstFuncDef()
            : AstNode(AstNodeType::kFuncDef)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstFuncDef>(holder);
            // Clone param linked list
            AstNode** param_tail = reinterpret_cast<AstNode**>(&c->first_param);
            for (AstNode* p = first_param; p; p = p->next_child)
            {
                *param_tail = p->clone(holder);
                param_tail = &(*param_tail)->next_child;
            }
            c->is_vararg = is_vararg;
            c->is_method = is_method;
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstAssign : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kAssign;
        AstNode* first_var = nullptr;
        AstNode* first_expr = nullptr;
        AstAssign()
            : AstNode(AstNodeType::kAssign)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* cloned = make_clone<AstAssign>(holder);
            AstNode** var_tail = &cloned->first_var;
            for (AstNode* v = first_var; v; v = v->next_child)
            {
                *var_tail = v->clone(holder);
                var_tail = &(*var_tail)->next_child;
            }
            AstNode** expr_tail = &cloned->first_expr;
            for (AstNode* e = first_expr; e; e = e->next_child)
            {
                *expr_tail = e->clone(holder);
                expr_tail = &(*expr_tail)->next_child;
            }
            return cloned;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstLocalDecl : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kLocalDecl;
        bool is_const = false;
        AstString* first_name = nullptr;
        AstNode* first_init = nullptr;
        AstLocalDecl(bool c, AstString* fn, AstNode* fi)
            : AstNode(AstNodeType::kLocalDecl)
            , is_const(c)
            , first_name(fn)
            , first_init(fi)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            AstString* cloned_names = nullptr;
            AstString** name_tail = &cloned_names;
            for (AstNode* n = reinterpret_cast<AstNode*>(first_name); n; n = n->next_child)
            {
                *name_tail = static_cast<AstString*>(n);
                name_tail = reinterpret_cast<AstString**>(&(*name_tail)->next_child);
            }
            AstNode* cloned_inits = nullptr;
            AstNode** init_tail = &cloned_inits;
            for (AstNode* init = first_init; init; init = init->next_child)
            {
                *init_tail = init->clone(holder);
                init_tail = &(*init_tail)->next_child;
            }
            return make_clone<AstLocalDecl>(holder, is_const, cloned_names, cloned_inits);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstAssignLocal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kAssignLocal;
        AstString* name;
        AstNode* expr;

        AstAssignLocal(AstString* n, AstNode* e)
            : AstNode(AstNodeType::kAssignLocal)
            , name(std::move(n))
            , expr(std::move(e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstAssignLocal>(holder, name, expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstAssignGlobal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kAssignGlobal;
        AstString* name;
        AstNode* expr;

        AstAssignGlobal(AstString* n, AstNode* e)
            : AstNode(AstNodeType::kAssignGlobal)
            , name(std::move(n))
            , expr(std::move(e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstAssignGlobal>(holder, name, expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstAssignUpvalue : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kAssignUpvalue;
        AstString* name;
        AstNode* expr;

        AstAssignUpvalue(AstString* n, AstNode* e)
            : AstNode(AstNodeType::kAssignUpvalue)
            , name(std::move(n))
            , expr(std::move(e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstAssignUpvalue>(holder, name, expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstCompoundAssign : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kCompoundAssign;
        AstNode* target;
        TokenType op;
        AstNode* expr;

        AstCompoundAssign(AstNode* t, TokenType o, AstNode* e)
            : AstNode(AstNodeType::kCompoundAssign)
            , target(std::move(t))
            , op(o)
            , expr(std::move(e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstCompoundAssign>(holder, target->clone(holder), op, expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstIncrement : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kIncrement;
        AstNode* target;

        explicit AstIncrement(AstNode* t)
            : AstNode(AstNodeType::kIncrement)
            , target(std::move(t))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstIncrement>(holder, target->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstDecrement : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kDecrement;
        AstNode* target;

        explicit AstDecrement(AstNode* t)
            : AstNode(AstNodeType::kDecrement)
            , target(std::move(t))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstDecrement>(holder, target->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstCompoundLocal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kCompoundLocal;
        AstString* name;
        TokenType op;
        AstNode* expr;

        AstCompoundLocal(AstString* n, TokenType o, AstNode* e)
            : AstNode(AstNodeType::kCompoundLocal)
            , name(std::move(n))
            , op(o)
            , expr(std::move(e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstCompoundLocal>(holder, name, op, expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstCompoundGlobal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kCompoundGlobal;
        AstString* name;
        TokenType op;
        AstNode* expr;

        AstCompoundGlobal(AstString* n, TokenType o, AstNode* e)
            : AstNode(AstNodeType::kCompoundGlobal)
            , name(std::move(n))
            , op(o)
            , expr(std::move(e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstCompoundGlobal>(holder, name, op, expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstCompoundUpvalue : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kCompoundUpvalue;
        AstString* name;
        TokenType op;
        AstNode* expr;

        AstCompoundUpvalue(AstString* n, TokenType o, AstNode* e)
            : AstNode(AstNodeType::kCompoundUpvalue)
            , name(std::move(n))
            , op(o)
            , expr(std::move(e))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstCompoundUpvalue>(holder, name, op, expr->clone(holder));
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstIncLocal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kIncLocal;
        AstString* name;

        explicit AstIncLocal(AstString* n)
            : AstNode(AstNodeType::kIncLocal)
            , name(std::move(n))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstIncLocal>(holder, name);
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstIncGlobal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kIncGlobal;
        AstString* name;

        explicit AstIncGlobal(AstString* n)
            : AstNode(AstNodeType::kIncGlobal)
            , name(std::move(n))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstIncGlobal>(holder, name);
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstIncUpvalue : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kIncUpvalue;
        AstString* name;

        explicit AstIncUpvalue(AstString* n)
            : AstNode(AstNodeType::kIncUpvalue)
            , name(std::move(n))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstIncUpvalue>(holder, name);
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstDecLocal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kDecLocal;
        AstString* name;

        explicit AstDecLocal(AstString* n)
            : AstNode(AstNodeType::kDecLocal)
            , name(std::move(n))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstDecLocal>(holder, name);
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstDecGlobal : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kDecGlobal;
        AstString* name;

        explicit AstDecGlobal(AstString* n)
            : AstNode(AstNodeType::kDecGlobal)
            , name(std::move(n))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstDecGlobal>(holder, name);
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstDecUpvalue : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kDecUpvalue;
        AstString* name;

        explicit AstDecUpvalue(AstString* n)
            : AstNode(AstNodeType::kDecUpvalue)
            , name(std::move(n))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstDecUpvalue>(holder, name);
        }

        void accept(AstVisitor& v) const override;
    };

    struct ElseIf : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kElseIf;
        AstNode* cond = nullptr;
        AstBlock* block = nullptr;
        ElseIf()
            : AstNode(AstNodeType::kElseIf)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto c = make_clone<ElseIf>(holder);
            if (cond)
            {
                c->cond = cond->clone(holder);
            }
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor&) const override
        {
            // ElseIf is not directly visited, only through AstIf
        }
    };

    struct AstIf : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kIf;
        AstNode* cond;
        AstBlock* then_block = nullptr;
        ElseIf* first_elseif = nullptr;
        AstBlock* else_block = nullptr;
        AstIf(AstNode* c)
            : AstNode(AstNodeType::kIf)
            , cond(std::move(c))
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto c = make_clone<AstIf>(holder, cond->clone(holder));

            if (then_block)
            {
                c->then_block = holder.make<AstBlock>();
                AstNode** tail = &c->then_block->first_stat;
                for (const AstNode* stat = then_block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }

            // Clone elseif chain
            ElseIf** elseif_tail = &c->first_elseif;
            for (ElseIf* elseif = first_elseif; elseif != nullptr; elseif = static_cast<ElseIf*>(elseif->next_child))
            {
                *elseif_tail = static_cast<ElseIf*>(elseif->clone(holder));
                elseif_tail = reinterpret_cast<ElseIf**>(&(*elseif_tail)->next_child);
            }

            if (else_block)
            {
                c->else_block = holder.make<AstBlock>();
                AstNode** else_tail = &c->else_block->first_stat;
                for (const AstNode* stat = else_block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *else_tail = stat->clone(holder);
                    else_tail = &(*else_tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstWhile : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kWhile;
        AstNode* cond;
        AstBlock* block = nullptr;
        AstWhile(AstNode* c)
            : AstNode(AstNodeType::kWhile)
            , cond(std::move(c))
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto c = make_clone<AstWhile>(holder, cond->clone(holder));
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstForNum : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kForNum;
        AstString* var;
        AstNode* start;
        AstNode* end;
        AstNode* step = nullptr;
        AstBlock* block = nullptr;
        AstForNum(AstString* v, AstNode* s, AstNode* e)
            : AstNode(AstNodeType::kForNum)
            , var(std::move(v))
            , start(std::move(s))
            , end(std::move(e))
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto c = make_clone<AstForNum>(holder, var, start->clone(holder), end->clone(holder));
            if (step)
            {
                c->step = step->clone(holder);
            }
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstForIn : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kForIn;
        AstNode* first_name = nullptr;
        AstNode* first_expr = nullptr;
        AstBlock* block = nullptr;
        bool declares_variables = false; // true if using 'let' to declare variables inline
        AstForIn()
            : AstNode(AstNodeType::kForIn)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto c = make_clone<AstForIn>(holder);
            // Clone names
            AstNode** name_tail = &c->first_name;
            for (const AstNode* name = first_name; name != nullptr; name = name->next_child)
            {
                *name_tail = name->clone(holder);
                name_tail = &(*name_tail)->next_child;
            }
            // Clone exprs
            AstNode** expr_tail = &c->first_expr;
            for (const AstNode* expr = first_expr; expr != nullptr; expr = expr->next_child)
            {
                *expr_tail = expr->clone(holder);
                expr_tail = &(*expr_tail)->next_child;
            }
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstForC : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kForC;
        AstNode* init;
        AstNode* condition;
        AstNode* update;
        AstBlock* block = nullptr;

        AstForC(AstNode* i, AstNode* c, AstNode* u)
            : AstNode(AstNodeType::kForC)
            , init(std::move(i))
            , condition(std::move(c))
            , update(std::move(u))
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            auto c = make_clone<AstForC>(holder, init ? init->clone(holder) : nullptr,
                condition ? condition->clone(holder) : nullptr, update ? update->clone(holder) : nullptr);
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    // Optimized numeric C-style for loop
    // Pattern: for(let i = start; i </<=/>/>= end; i++ / i-- / i += step / i -= step)
    struct AstForCNumeric : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kForCNumeric;
        AstString* var; // Loop variable name
        AstNode* start; // Initial value
        AstNode* end;   // End value (limit)
        AstNode* step;  // Step value (can be nullptr for implicit 1/-1)
        bool ascending; // true for <=/<, false for >=/>
        bool inclusive; // true for <=/>=, false for </>
        AstBlock* block = nullptr;

        AstForCNumeric(AstString* v, AstNode* s, AstNode* e, AstNode* st, bool asc, bool incl)
            : AstNode(AstNodeType::kForCNumeric)
            , var(v)
            , start(s)
            , end(e)
            , step(st)
            , ascending(asc)
            , inclusive(incl)
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            auto c = make_clone<AstForCNumeric>(holder, var, start->clone(holder), end->clone(holder),
                step ? step->clone(holder) : nullptr, ascending, inclusive);
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstFuncDefStat : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kFuncDefStat;
        AstNode* first_name_part = nullptr;
        bool is_method = false;
        bool is_local = false;
        AstString* first_param = nullptr;
        bool is_vararg = false;
        AstBlock* block = nullptr;
        AstFuncDefStat()
            : AstNode(AstNodeType::kFuncDefStat)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstFuncDefStat>(holder);
            // Clone name_parts linked list
            AstNode** name_tail = &c->first_name_part;
            for (AstNode* name = first_name_part; name; name = name->next_child)
            {
                *name_tail = name->clone(holder);
                name_tail = &(*name_tail)->next_child;
            }
            c->is_method = is_method;
            c->is_local = is_local;
            // Clone param linked list
            AstNode** param_tail = reinterpret_cast<AstNode**>(&c->first_param);
            for (AstNode* p = first_param; p; p = p->next_child)
            {
                *param_tail = p->clone(holder);
                param_tail = &(*param_tail)->next_child;
            }
            c->is_vararg = is_vararg;
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstReturn : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kReturn;
        AstNode* first_expr = nullptr;
        AstReturn(AstNode* first_e = nullptr)
            : AstNode(AstNodeType::kReturn)
            , first_expr(first_e)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstReturn>(holder);
            // Clone exprs linked list
            AstNode** tail = &c->first_expr;
            for (AstNode* expr = first_expr; expr; expr = expr->next_child)
            {
                auto* expr_copy = expr->clone(holder);
                *tail = expr_copy;
                tail = &expr_copy->next_child;
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstBreak : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kBreak;
        AstBreak()
            : AstNode(AstNodeType::kBreak)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstBreak>(holder);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstContinue : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kContinue;
        AstContinue()
            : AstNode(AstNodeType::kContinue)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstContinue>(holder);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstDefer : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kDefer;
        AstNode* body = nullptr; // Can be a single statement or AstBlock

        AstDefer(AstNode* body_node = nullptr)
            : AstNode(AstNodeType::kDefer)
            , body(body_node)
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstDefer>(holder);
            if (body)
            {
                c->body = body->clone(holder);
            }
            return c;
        }

        void accept(AstVisitor& v) const override;
    };

    struct AstScope : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kScope;
        AstBlock* block = nullptr;
        AstScope()
            : AstNode(AstNodeType::kScope)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto* c = make_clone<AstScope>(holder);
            if (block)
            {
                c->block = holder.make<AstBlock>();
                AstNode** tail = &c->block->first_stat;
                for (const AstNode* stat = block->first_stat; stat != nullptr; stat = stat->next_child)
                {
                    *tail = stat->clone(holder);
                    tail = &(*tail)->next_child;
                }
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstExprStat : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kExprStat;
        AstNode* expr;
        AstExprStat(AstNode* e)
            : AstNode(AstNodeType::kExprStat)
            , expr(std::move(e))
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            auto cloned_expr = expr->clone(holder);
            return make_clone<AstExprStat>(holder, std::move(cloned_expr));
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstModuleDecl : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kModuleDecl;

        AstModuleDecl()
            : AstNode(AstNodeType::kModuleDecl)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstModuleDecl>(holder);
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstExportDecl : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kExportDecl;
        AstNode* declaration; // FuncDefStat or LocalDecl (const only)

        explicit AstExportDecl(AstNode* decl)
            : AstNode(AstNodeType::kExportDecl)
            , declaration(decl)
        {
        }
        AstNode* clone(AstHolder& holder) const override
        {
            return make_clone<AstExportDecl>(holder, declaration->clone(holder));
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstExportList : AstNode
    {
        static constexpr AstNodeType kType = AstNodeType::kExportList;
        AstString* first_name = nullptr;

        explicit AstExportList()
            : AstNode(AstNodeType::kExportList)
        {
        }

        AstNode* clone(AstHolder& holder) const override
        {
            auto c = holder.make<AstExportList>();
            AstString** tail = &c->first_name;
            for (AstString* name = first_name; name != nullptr; name = static_cast<AstString*>(name->next_child))
            {
                auto cloned_name = holder.make_string(name->view());
                *tail = cloned_name;
                tail = reinterpret_cast<AstString**>(&cloned_name->next_child);
            }
            return c;
        }
        void accept(AstVisitor& v) const override;
    };

    struct AstVisitor
    {
        virtual ~AstVisitor() = default;

        virtual void visit(const AstNil&) = 0;
        virtual void visit(const AstBool&) = 0;
        virtual void visit(const AstInt&) = 0;
        virtual void visit(const AstFP&) = 0;
        virtual void visit(const AstString&) = 0;
        virtual void visit(const AstVararg&) = 0;
        virtual void visit(const AstIdent&) = 0;
        virtual void visit(const AstBinOp&) = 0;
        virtual void visit(const AstUnOp&) = 0;
        virtual void visit(const AstTernary&) = 0;
        virtual void visit(const AstFuncCall&) = 0;
        virtual void visit(const AstTableCtor&) = 0;
        virtual void visit(const AstIndex&) = 0;
        virtual void visit(const AstMember&) = 0;
        virtual void visit(const AstFuncDef&) = 0;

        virtual void visit(const AstAssign&) = 0;
        virtual void visit(const AstAssignLocal&) = 0;
        virtual void visit(const AstAssignGlobal&) = 0;
        virtual void visit(const AstAssignUpvalue&) = 0;
        virtual void visit(const AstCompoundAssign&) = 0;
        virtual void visit(const AstCompoundLocal&) = 0;
        virtual void visit(const AstCompoundGlobal&) = 0;
        virtual void visit(const AstCompoundUpvalue&) = 0;
        virtual void visit(const AstIncrement&) = 0;
        virtual void visit(const AstIncLocal&) = 0;
        virtual void visit(const AstIncGlobal&) = 0;
        virtual void visit(const AstIncUpvalue&) = 0;
        virtual void visit(const AstDecrement&) = 0;
        virtual void visit(const AstDecLocal&) = 0;
        virtual void visit(const AstDecGlobal&) = 0;
        virtual void visit(const AstDecUpvalue&) = 0;
        virtual void visit(const AstLocalDecl&) = 0;
        virtual void visit(const AstIf&) = 0;
        virtual void visit(const AstWhile&) = 0;
        virtual void visit(const AstForNum&) = 0;
        virtual void visit(const AstForIn&) = 0;
        virtual void visit(const AstForC&) = 0;
        virtual void visit(const AstForCNumeric&) = 0;
        virtual void visit(const AstFuncDefStat&) = 0;
        virtual void visit(const AstReturn&) = 0;
        virtual void visit(const AstBreak&) = 0;
        virtual void visit(const AstContinue&) = 0;
        virtual void visit(const AstDefer&) = 0;
        virtual void visit(const AstScope&) = 0;
        virtual void visit(const AstExprStat&) = 0;

        virtual void visit(const AstBlock&) = 0;
        virtual void visit(const AstProgram&) = 0;
        virtual void visit(const AstModuleDecl&) = 0;
        virtual void visit(const AstExportDecl&) = 0;
        virtual void visit(const AstExportList&) = 0;
    };

    inline void AstNil::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstBool::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstInt::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstFP::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstString::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstVararg::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstIdent::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstBinOp::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstUnOp::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstTernary::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstFuncCall::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstTableCtor::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstIndex::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstMember::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstFuncDef::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }

    inline void AstAssign::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstAssignLocal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstAssignGlobal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstAssignUpvalue::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstCompoundAssign::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstCompoundLocal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstCompoundGlobal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstCompoundUpvalue::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstIncrement::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstIncLocal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstIncGlobal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstIncUpvalue::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstDecrement::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstDecLocal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstDecGlobal::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstDecUpvalue::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstLocalDecl::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstIf::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstWhile::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstForNum::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstForIn::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstForC::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }

    inline void AstForCNumeric::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstFuncDefStat::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstReturn::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstBreak::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstContinue::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstDefer::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstScope::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstExprStat::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }

    inline void AstBlock::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstProgram::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstModuleDecl::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstExportDecl::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }
    inline void AstExportList::accept(AstVisitor& v) const
    {
        v.visit(*this);
    }

} // namespace behl
