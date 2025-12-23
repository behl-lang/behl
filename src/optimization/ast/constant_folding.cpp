#include "constant_folding.hpp"

#include "ast/ast_transformer.hpp"
#include "common/print.hpp"
#include "config_internal.hpp"

#include <cmath>

namespace behl
{
    class ConstantFolder : public AstTransformer
    {
    private:
        size_t fold_count = 0;

        AstNode* try_fold_binop(AstBinOp* node)
        {
            // Try to fold if both operands are constants
            auto* left_int = node->left->try_as<AstInt>();
            auto* right_int = node->right->try_as<AstInt>();
            auto* left_float = node->left->try_as<AstFP>();
            auto* right_float = node->right->try_as<AstFP>();
            auto* left_string = node->left->try_as<AstString>();
            auto* right_string = node->right->try_as<AstString>();

            // String concatenation
            if (left_string && right_string && node->op == TokenType::kPlus)
            {
                if constexpr (kOptimizationPassDebug)
                {
                    fold_count++;
                    println("      Folding string concat (\"{}\"+\"{}\") -> \"{}\"", left_string->view(), right_string->view(),
                        std::string(left_string->view()) + std::string(right_string->view()));
                }
                changed = true;
                std::string concatenated = std::string(left_string->view()) + std::string(right_string->view());
                auto* folded = holder.make_string(concatenated);
                folded->line = node->line;
                folded->column = node->column;
                return folded;
            }

            // Integer-only operations
            if (left_int && right_int)
            {
                Integer result = 0;
                bool can_fold = true;

                switch (node->op)
                {
                    case TokenType::kPlus:
                        result = left_int->value + right_int->value;
                        break;
                    case TokenType::kMinus:
                        result = left_int->value - right_int->value;
                        break;
                    case TokenType::kStar:
                        result = left_int->value * right_int->value;
                        break;
                    case TokenType::kPercent:
                        if (right_int->value != 0)
                        {
                            result = left_int->value % right_int->value;
                        }
                        else
                        {
                            can_fold = false;
                        }
                        break;
                    case TokenType::kBAnd:
                        result = left_int->value & right_int->value;
                        break;
                    case TokenType::kBOr:
                        result = left_int->value | right_int->value;
                        break;
                    case TokenType::kBXor:
                        result = left_int->value ^ right_int->value;
                        break;
                    case TokenType::kBShl:
                        result = left_int->value << right_int->value;
                        break;
                    case TokenType::kBShr:
                        result = left_int->value >> right_int->value;
                        break;
                    default:
                        can_fold = false;
                }

                if (can_fold)
                {
                    if constexpr (kOptimizationPassDebug)
                    {
                        fold_count++;
                        println("      Folding integer binop {} ({} {} {}) -> {}", static_cast<int>(node->op), left_int->value,
                            static_cast<int>(node->op), right_int->value, result);
                    }
                    changed = true;
                    auto* folded = holder.make<AstInt>(result);
                    folded->line = node->line;
                    folded->column = node->column;
                    return folded;
                }
            }

            // Float operations (also handle int->float promotion)
            if ((left_int || left_float) && (right_int || right_float))
            {
                FP left_val = left_float ? left_float->value : static_cast<FP>(left_int->value);
                FP right_val = right_float ? right_float->value : static_cast<FP>(right_int->value);
                FP result = 0.0;
                bool can_fold = true;

                switch (node->op)
                {
                    case TokenType::kSlash:
                        if (right_val != 0.0)
                        {
                            result = left_val / right_val;
                        }
                        else
                        {
                            can_fold = false;
                        }
                        break;
                    case TokenType::kPower:
                        result = std::pow(left_val, right_val);
                        break;
                    case TokenType::kPlus:
                        if (left_float || right_float)
                        {
                            result = left_val + right_val;
                        }
                        else
                        {
                            can_fold = false;
                        }
                        break;
                    case TokenType::kMinus:
                        if (left_float || right_float)
                        {
                            result = left_val - right_val;
                        }
                        else
                        {
                            can_fold = false;
                        }
                        break;
                    case TokenType::kStar:
                        if (left_float || right_float)
                        {
                            result = left_val * right_val;
                        }
                        else
                        {
                            can_fold = false;
                        }
                        break;
                    default:
                        can_fold = false;
                }

                if (can_fold)
                {
                    if constexpr (kOptimizationPassDebug)
                    {
                        fold_count++;
                        println("      Folding float binop {} ({} {} {}) -> {}", static_cast<int>(node->op), left_val,
                            static_cast<int>(node->op), right_val, result);
                    }
                    changed = true;
                    // If result is whole number and both inputs were int, keep as int
                    if (left_int && right_int && std::floor(result) == result && result >= static_cast<FP>(INT64_MIN)
                        && result <= static_cast<FP>(INT64_MAX))
                    {
                        auto* folded = holder.make<AstInt>(static_cast<Integer>(result));
                        folded->line = node->line;
                        folded->column = node->column;
                        return folded;
                    }
                    else
                    {
                        auto* folded = holder.make<AstFP>(result);
                        folded->line = node->line;
                        folded->column = node->column;
                        return folded;
                    }
                }
            }

            return node;
        }

        AstNode* try_fold_unop(AstUnOp* node)
        {
            // Fold unary minus on constants
            if (node->op == TokenType::kMinus)
            {
                if (auto* int_node = node->expr->try_as<AstInt>())
                {
                    if constexpr (kOptimizationPassDebug)
                    {
                        fold_count++;
                        println("      Folding unary minus int (-{}) -> {}", int_node->value, -int_node->value);
                    }
                    changed = true;
                    auto* folded = holder.make<AstInt>(-int_node->value);
                    folded->line = node->line;
                    folded->column = node->column;
                    return folded;
                }
                else if (auto* float_node = node->expr->try_as<AstFP>())
                {
                    if constexpr (kOptimizationPassDebug)
                    {
                        fold_count++;
                        println("      Folding unary minus float (-{}) -> {}", float_node->value, -float_node->value);
                    }
                    changed = true;
                    auto* folded = holder.make<AstFP>(-float_node->value);
                    folded->line = node->line;
                    folded->column = node->column;
                    return folded;
                }
            }
            return node;
        }

    public:
        explicit ConstantFolder(AstHolder& h)
            : AstTransformer(h)
        {
        }

        size_t get_fold_count() const
        {
            return fold_count;
        }

        // Override visitor methods to add constant folding logic
        AstNode* visit_BinOp(AstBinOp* node) override
        {
            // First, transform children
            node->left = transform(node->left);
            node->right = transform(node->right);

            // Then try to fold this node
            return try_fold_binop(node);
        }

        AstNode* visit_UnOp(AstUnOp* node) override
        {
            // First, transform child
            node->expr = transform(node->expr);

            // Then try to fold this node
            return try_fold_unop(node);
        }
    };

    bool ConstantFoldingPass::apply(AstOptimizationContext& context)
    {
        ConstantFolder folder(context.holder);

        if (context.program->block)
        {
            folder.transform_block(context.program->block);
        }

        if constexpr (kOptimizationPassDebug)
        {
            if (folder.get_fold_count() > 0)
            {
                println("    Folded {} constant expressions", folder.get_fold_count());
            }
            println("    Returning changed = {}", folder.has_changed());
        }

        return folder.has_changed();
    }

} // namespace behl
