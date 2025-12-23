#pragma once

#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"

namespace behl
{

    class AstTransformer
    {
    protected:
        AstHolder& holder;
        bool changed = false;

    public:
        explicit AstTransformer(AstHolder& h)
            : holder(h)
        {
        }
        virtual ~AstTransformer() = default;

        // Returns true if any transformations were made
        bool has_changed() const
        {
            return changed;
        }
        void reset_changed()
        {
            changed = false;
        }

        // Main entry point - transforms a node and its children
        AstNode* transform(AstNode* node)
        {
            if (!node)
            {
                return nullptr;
            }

            switch (node->type)
            {
                case AstNodeType::kNil:
                    return visit_Nil(static_cast<AstNil*>(node));
                case AstNodeType::kBool:
                    return visit_Bool(static_cast<AstBool*>(node));
                case AstNodeType::kInteger:
                    return visit_Int(static_cast<AstInt*>(node));
                case AstNodeType::kFP:
                    return visit_FP(static_cast<AstFP*>(node));
                case AstNodeType::kString:
                    return visit_String(static_cast<AstString*>(node));
                case AstNodeType::kVararg:
                    return visit_Vararg(static_cast<AstVararg*>(node));
                case AstNodeType::kIdent:
                    return visit_Ident(static_cast<AstIdent*>(node));
                case AstNodeType::kBinOp:
                    return visit_BinOp(static_cast<AstBinOp*>(node));
                case AstNodeType::kUnOp:
                    return visit_UnOp(static_cast<AstUnOp*>(node));
                case AstNodeType::kTernary:
                    return visit_Ternary(static_cast<AstTernary*>(node));
                case AstNodeType::kFuncCall:
                    return visit_FuncCall(static_cast<AstFuncCall*>(node));
                case AstNodeType::kTableCtor:
                    return visit_TableCtor(static_cast<AstTableCtor*>(node));
                case AstNodeType::kIndex:
                    return visit_Index(static_cast<AstIndex*>(node));
                case AstNodeType::kMember:
                    return visit_Member(static_cast<AstMember*>(node));
                case AstNodeType::kFuncDef:
                    return visit_FuncDef(static_cast<AstFuncDef*>(node));
                case AstNodeType::kLocalDecl:
                    return visit_LocalDecl(static_cast<AstLocalDecl*>(node));
                case AstNodeType::kAssign:
                    return visit_Assign(static_cast<AstAssign*>(node));
                case AstNodeType::kAssignLocal:
                    return visit_AssignLocal(static_cast<AstAssignLocal*>(node));
                case AstNodeType::kAssignGlobal:
                    return visit_AssignGlobal(static_cast<AstAssignGlobal*>(node));
                case AstNodeType::kAssignUpvalue:
                    return visit_AssignUpvalue(static_cast<AstAssignUpvalue*>(node));
                case AstNodeType::kCompoundLocal:
                    return visit_CompoundLocal(static_cast<AstCompoundLocal*>(node));
                case AstNodeType::kCompoundGlobal:
                    return visit_CompoundGlobal(static_cast<AstCompoundGlobal*>(node));
                case AstNodeType::kIncLocal:
                    return visit_IncLocal(static_cast<AstIncLocal*>(node));
                case AstNodeType::kIncGlobal:
                    return visit_IncGlobal(static_cast<AstIncGlobal*>(node));
                case AstNodeType::kDecLocal:
                    return visit_DecLocal(static_cast<AstDecLocal*>(node));
                case AstNodeType::kDecGlobal:
                    return visit_DecGlobal(static_cast<AstDecGlobal*>(node));
                case AstNodeType::kIf:
                    return visit_If(static_cast<AstIf*>(node));
                case AstNodeType::kWhile:
                    return visit_While(static_cast<AstWhile*>(node));
                case AstNodeType::kForNum:
                    return visit_ForNum(static_cast<AstForNum*>(node));
                case AstNodeType::kForIn:
                    return visit_ForIn(static_cast<AstForIn*>(node));
                case AstNodeType::kForC:
                    return visit_ForC(static_cast<AstForC*>(node));
                case AstNodeType::kForCNumeric:
                    return visit_ForCNumeric(static_cast<AstForCNumeric*>(node));
                case AstNodeType::kFuncDefStat:
                    return visit_FuncDefStat(static_cast<AstFuncDefStat*>(node));
                case AstNodeType::kReturn:
                    return visit_Return(static_cast<AstReturn*>(node));
                case AstNodeType::kBreak:
                    return visit_Break(static_cast<AstBreak*>(node));
                case AstNodeType::kContinue:
                    return visit_Continue(static_cast<AstContinue*>(node));
                case AstNodeType::kDefer:
                    return visit_Defer(static_cast<AstDefer*>(node));
                case AstNodeType::kScope:
                    return visit_Scope(static_cast<AstScope*>(node));
                case AstNodeType::kExprStat:
                    return visit_ExprStat(static_cast<AstExprStat*>(node));
                default:
                    return node; // Unhandled types pass through
            }
        }

        // Transform a linked list of nodes (e.g., function arguments, initializers)
        // Supports removal: if transform returns nullptr, the node is removed from the list
        void transform_list(AstNode*& first)
        {
            AstNode** current = &first;
            while (*current)
            {
                AstNode* transformed = transform(*current);
                if (transformed == nullptr)
                {
                    // Remove this node from the list
                    changed = true;
                    *current = (*current)->next_child;
                }
                else if (transformed != *current)
                {
                    // Replace with a different node
                    changed = true;
                    transformed->next_child = (*current)->next_child;
                    *current = transformed;
                    current = &(*current)->next_child;
                }
                else
                {
                    // No change, move to next
                    current = &(*current)->next_child;
                }
            }
        }

        // Transform a block (list of statements)
        // Supports removal: returning nullptr from a visit method removes the statement
        void transform_block(AstBlock* block)
        {
            if (!block)
            {
                return;
            }
            transform_list(block->first_stat);
        }

        // Virtual methods for each node type - override to customize behavior
        // Default implementation visits children

        // Leaf nodes (no children to visit)
        virtual AstNode* visit_Nil(AstNil* node)
        {
            return node;
        }
        virtual AstNode* visit_Bool(AstBool* node)
        {
            return node;
        }
        virtual AstNode* visit_Int(AstInt* node)
        {
            return node;
        }
        virtual AstNode* visit_FP(AstFP* node)
        {
            return node;
        }
        virtual AstNode* visit_String(AstString* node)
        {
            return node;
        }
        virtual AstNode* visit_Vararg(AstVararg* node)
        {
            return node;
        }
        virtual AstNode* visit_Ident(AstIdent* node)
        {
            return node;
        }

        // Expressions with children
        virtual AstNode* visit_BinOp(AstBinOp* node)
        {
            node->left = transform(node->left);
            node->right = transform(node->right);
            return node;
        }

        virtual AstNode* visit_UnOp(AstUnOp* node)
        {
            node->expr = transform(node->expr);
            return node;
        }

        virtual AstNode* visit_Ternary(AstTernary* node)
        {
            node->condition = transform(node->condition);
            node->true_expr = transform(node->true_expr);
            node->false_expr = transform(node->false_expr);
            return node;
        }

        virtual AstNode* visit_FuncCall(AstFuncCall* node)
        {
            node->func = transform(node->func);
            transform_list(node->first_arg);
            return node;
        }

        virtual AstNode* visit_TableCtor(AstTableCtor* node)
        {
            for (TableField* field = node->first_field; field; field = static_cast<TableField*>(field->next_child))
            {
                if (field->key)
                {
                    field->key = transform(field->key);
                }
                field->value = transform(field->value);
            }
            return node;
        }

        virtual AstNode* visit_Index(AstIndex* node)
        {
            node->table = transform(node->table);
            node->key = transform(node->key);
            return node;
        }

        virtual AstNode* visit_Member(AstMember* node)
        {
            node->table = transform(node->table);
            return node;
        }

        virtual AstNode* visit_FuncDef(AstFuncDef* node)
        {
            transform_block(node->block);
            return node;
        }

        // Statements
        virtual AstNode* visit_LocalDecl(AstLocalDecl* node)
        {
            transform_list(node->first_init);
            return node;
        }

        virtual AstNode* visit_Assign(AstAssign* node)
        {
            transform_list(node->first_expr);
            return node;
        }

        virtual AstNode* visit_AssignLocal(AstAssignLocal* node)
        {
            node->expr = transform(node->expr);
            return node;
        }

        virtual AstNode* visit_AssignGlobal(AstAssignGlobal* node)
        {
            node->expr = transform(node->expr);
            return node;
        }

        virtual AstNode* visit_AssignUpvalue(AstAssignUpvalue* node)
        {
            node->expr = transform(node->expr);
            return node;
        }

        virtual AstNode* visit_CompoundLocal(AstCompoundLocal* node)
        {
            node->expr = transform(node->expr);
            return node;
        }

        virtual AstNode* visit_CompoundGlobal(AstCompoundGlobal* node)
        {
            node->expr = transform(node->expr);
            return node;
        }

        virtual AstNode* visit_IncLocal(AstIncLocal* node)
        {
            return node;
        }
        virtual AstNode* visit_IncGlobal(AstIncGlobal* node)
        {
            return node;
        }
        virtual AstNode* visit_DecLocal(AstDecLocal* node)
        {
            return node;
        }
        virtual AstNode* visit_DecGlobal(AstDecGlobal* node)
        {
            return node;
        }

        virtual AstNode* visit_If(AstIf* node)
        {
            node->cond = transform(node->cond);
            transform_block(node->then_block);
            for (ElseIf* elseif = node->first_elseif; elseif; elseif = static_cast<ElseIf*>(elseif->next_child))
            {
                if (elseif->cond)
                {
                    elseif->cond = transform(elseif->cond);
                }
                transform_block(elseif->block);
            }
            transform_block(node->else_block);
            return node;
        }

        virtual AstNode* visit_While(AstWhile* node)
        {
            node->cond = transform(node->cond);
            transform_block(node->block);
            return node;
        }

        virtual AstNode* visit_ForNum(AstForNum* node)
        {
            node->start = transform(node->start);
            node->end = transform(node->end);
            if (node->step)
            {
                node->step = transform(node->step);
            }
            transform_block(node->block);
            return node;
        }

        virtual AstNode* visit_ForIn(AstForIn* node)
        {
            transform_list(node->first_expr);
            transform_block(node->block);
            return node;
        }

        virtual AstNode* visit_ForC(AstForC* node)
        {
            if (node->init)
            {
                node->init = transform(node->init);
            }
            if (node->condition)
            {
                node->condition = transform(node->condition);
            }
            if (node->update)
            {
                node->update = transform(node->update);
            }
            transform_block(node->block);
            return node;
        }

        virtual AstNode* visit_ForCNumeric(AstForCNumeric* node)
        {
            node->start = transform(node->start);
            node->end = transform(node->end);
            if (node->step)
            {
                node->step = transform(node->step);
            }
            transform_block(node->block);
            return node;
        }

        virtual AstNode* visit_FuncDefStat(AstFuncDefStat* node)
        {
            transform_block(node->block);
            return node;
        }

        virtual AstNode* visit_Return(AstReturn* node)
        {
            transform_list(node->first_expr);
            return node;
        }

        virtual AstNode* visit_Break(AstBreak* node)
        {
            return node;
        }
        virtual AstNode* visit_Continue(AstContinue* node)
        {
            return node;
        }

        virtual AstNode* visit_Defer(AstDefer* node)
        {
            if (node->body)
            {
                node->body = transform(node->body);
            }
            return node;
        }

        virtual AstNode* visit_Scope(AstScope* node)
        {
            transform_block(node->block);
            return node;
        }

        virtual AstNode* visit_ExprStat(AstExprStat* node)
        {
            node->expr = transform(node->expr);
            return node;
        }
    };

} // namespace behl
