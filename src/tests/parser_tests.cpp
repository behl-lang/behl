#include "ast/ast.hpp"
#include "behl/behl.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"

#include <behl/exceptions.hpp>

using namespace behl;

#include <gtest/gtest.h>

static size_t count_children(const AstNode* first)
{
    size_t count = 0;
    for (const AstNode* node = first; node != nullptr; node = node->next_child)
    {
        count++;
    }
    return count;
}

static const AstNode* get_child_at(const AstNode* first, size_t index)
{
    const AstNode* node = first;
    for (size_t i = 0; i < index && node != nullptr; i++)
    {
        node = node->next_child;
    }
    return node;
}

class ParserTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
        S = nullptr;
    }
};

static void expect_int_num(const AstNode& e, int64_t v)
{
    ASSERT_EQ(e.type, AstNodeType::kInteger);
    const auto& n = static_cast<const AstInt&>(e);
    ASSERT_EQ(n.value, v);
}

static void expect_float_num(const AstNode& e, double v)
{
    ASSERT_EQ(e.type, AstNodeType::kFP);
    const auto& n = static_cast<const AstFP&>(e);
    ASSERT_DOUBLE_EQ(n.value, v);
}

TEST_F(ParserTest, ParserLocalAssign)
{
    const auto source = "let x = 1";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kLocalDecl);
    const auto& local_decl = static_cast<const AstLocalDecl&>(*stat0);
    ASSERT_FALSE(local_decl.is_const);
    ASSERT_EQ(count_children(reinterpret_cast<const AstNode*>(local_decl.first_name)), 1);
    auto* name0 = static_cast<const AstString*>(get_child_at(reinterpret_cast<const AstNode*>(local_decl.first_name), 0));
    ASSERT_EQ(name0->view(), "x");
    ASSERT_EQ(count_children(local_decl.first_init), 1);
    expect_int_num(*get_child_at(local_decl.first_init, 0), 1);
}

TEST_F(ParserTest, ParserLocalConst)
{
    const auto source = "const y = 2.5";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kLocalDecl);
    const auto& local_decl = static_cast<const AstLocalDecl&>(*stat0);
    ASSERT_TRUE(local_decl.is_const);
    ASSERT_EQ(count_children(reinterpret_cast<const AstNode*>(local_decl.first_name)), 1);
    auto* name0 = static_cast<const AstString*>(get_child_at(reinterpret_cast<const AstNode*>(local_decl.first_name), 0));
    ASSERT_EQ(name0->view(), "y");
    ASSERT_EQ(count_children(local_decl.first_init), 1);
    expect_float_num(*get_child_at(local_decl.first_init, 0), 2.5);
}

TEST_F(ParserTest, ParserTableCtor)
{
    const auto source = "let z = {1, \"two\", true}";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kLocalDecl);
    const auto& local_decl = static_cast<const AstLocalDecl&>(*stat0);
    ASSERT_EQ(count_children(local_decl.first_init), 1);
    auto* init0 = get_child_at(local_decl.first_init, 0);
    ASSERT_EQ(init0->type, AstNodeType::kTableCtor);
    const auto& table = static_cast<const AstTableCtor&>(*init0);
    ASSERT_EQ(count_children(table.first_field), 3);
    const auto* field0 = static_cast<const TableField*>(get_child_at(table.first_field, 0));
    ASSERT_EQ(field0->key, nullptr);
    expect_int_num(*field0->value, 1);
    const auto* field1 = static_cast<const TableField*>(get_child_at(table.first_field, 1));
    ASSERT_EQ(field1->key, nullptr);
    ASSERT_EQ(field1->value->type, AstNodeType::kString);
    const auto& str = static_cast<const AstString&>(*field1->value);
    ASSERT_EQ(std::string(str.view()), "two");
    const auto* field2 = static_cast<const TableField*>(get_child_at(table.first_field, 2));
    ASSERT_EQ(field2->key, nullptr);
    ASSERT_EQ(field2->value->type, AstNodeType::kBool);
    const auto& bool_val = static_cast<const AstBool&>(*field2->value);
    ASSERT_TRUE(bool_val.value);
}

TEST_F(ParserTest, ParserIfStatement)
{
    const auto source = "if (x) { y = 1 }";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kIf);
    const auto& if_stat = static_cast<const AstIf&>(*stat0);
    ASSERT_EQ(if_stat.cond->type, AstNodeType::kIdent);
    const auto& cond_ident = static_cast<const AstIdent&>(*if_stat.cond);
    ASSERT_EQ(cond_ident.name->view(), "x");
    ASSERT_EQ(count_children(if_stat.then_block->first_stat), 1);
    const AstNode* then_stat0 = get_child_at(if_stat.then_block->first_stat, 0);
    ASSERT_EQ(then_stat0->type, AstNodeType::kAssign);
    const auto& assign = static_cast<const AstAssign&>(*then_stat0);
    ASSERT_EQ(count_children(assign.first_var), 1);
    ASSERT_EQ(assign.first_var->type, AstNodeType::kIdent);
    const auto& var_ident = static_cast<const AstIdent&>(*assign.first_var);
    ASSERT_EQ(var_ident.name->view(), "y");
    ASSERT_EQ(count_children(assign.first_expr), 1);
    expect_int_num(*assign.first_expr, 1);
    ASSERT_EQ(count_children(if_stat.first_elseif), 0);
    ASSERT_FALSE(if_stat.else_block);
}

TEST_F(ParserTest, ParserFunctionCall)
{
    const auto source = "print(42)";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kExprStat);
    const auto& expr_stat = static_cast<const AstExprStat&>(*stat0);
    ASSERT_EQ(expr_stat.expr->type, AstNodeType::kFuncCall);
    const auto& func_call = static_cast<const AstFuncCall&>(*expr_stat.expr);
    ASSERT_EQ(func_call.func->type, AstNodeType::kIdent);
    const auto& func_ident = static_cast<const AstIdent&>(*func_call.func);
    ASSERT_EQ(func_ident.name->view(), "print");
    ASSERT_EQ(count_children(func_call.first_arg), 1);
    expect_int_num(*get_child_at(func_call.first_arg, 0), 42);
}

TEST_F(ParserTest, ParserBinaryOp)
{
    const auto source = "let a = 1 + 2 * 3";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kLocalDecl);
    const auto& local_decl = static_cast<const AstLocalDecl&>(*stat0);
    ASSERT_EQ(count_children(local_decl.first_init), 1);
    auto* init0 = get_child_at(local_decl.first_init, 0);
    ASSERT_EQ(init0->type, AstNodeType::kBinOp);
    const auto& bin_op_plus = static_cast<const AstBinOp&>(*init0);
    ASSERT_EQ(bin_op_plus.op, TokenType::kPlus);
    expect_int_num(*bin_op_plus.left, 1);
    ASSERT_EQ(bin_op_plus.right->type, AstNodeType::kBinOp);
    const auto& bin_op_mul = static_cast<const AstBinOp&>(*bin_op_plus.right);
    ASSERT_EQ(bin_op_mul.op, TokenType::kStar);
    expect_int_num(*bin_op_mul.left, 2);
    expect_int_num(*bin_op_mul.right, 3);
}

TEST_F(ParserTest, ParserInvalidSyntax)
{
    const auto source = "let x 1";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    ASSERT_THROW(parse(holder, tokens), behl::SyntaxError);
}

TEST_F(ParserTest, ParserFunctionDef)
{
    const auto source = "function add(a, b) { return a + b }";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kFuncDefStat);
    const auto& func_def = static_cast<const AstFuncDefStat&>(*stat0);
    ASSERT_EQ(count_children(func_def.first_name_part), 1);
    const auto* name_part0 = static_cast<const AstString*>(get_child_at(func_def.first_name_part, 0));
    ASSERT_EQ(name_part0->view(), "add");
    ASSERT_FALSE(func_def.is_method);
    ASSERT_EQ(count_children(func_def.first_param), 2);
    const auto* param0 = static_cast<const AstString*>(get_child_at(func_def.first_param, 0));
    ASSERT_EQ(param0->view(), "a");
    const auto* param1 = static_cast<const AstString*>(get_child_at(func_def.first_param, 1));
    ASSERT_EQ(param1->view(), "b");
    ASSERT_FALSE(func_def.is_vararg);
    ASSERT_EQ(count_children(func_def.block->first_stat), 1);
    const AstNode* func_stat0 = get_child_at(func_def.block->first_stat, 0);
    ASSERT_EQ(func_stat0->type, AstNodeType::kReturn);
    const auto& ret = static_cast<const AstReturn&>(*func_stat0);
    ASSERT_EQ(count_children(ret.first_expr), 1);
    ASSERT_EQ(get_child_at(ret.first_expr, 0)->type, AstNodeType::kBinOp);
    const auto& bin_op = static_cast<const AstBinOp&>(*get_child_at(ret.first_expr, 0));
    ASSERT_EQ(bin_op.op, TokenType::kPlus);
    ASSERT_EQ(bin_op.left->type, AstNodeType::kIdent);
    const auto& left_ident = static_cast<const AstIdent&>(*bin_op.left);
    ASSERT_EQ(left_ident.name->view(), "a");
    ASSERT_EQ(bin_op.right->type, AstNodeType::kIdent);
    const auto& right_ident = static_cast<const AstIdent&>(*bin_op.right);
    ASSERT_EQ(right_ident.name->view(), "b");
}

TEST_F(ParserTest, ParserWhileLoop)
{
    const auto source = "while (x > 0) { x = x - 1 }";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kWhile);
    const auto& while_loop = static_cast<const AstWhile&>(*stat0);
    ASSERT_EQ(while_loop.cond->type, AstNodeType::kBinOp);
    const auto& cond = static_cast<const AstBinOp&>(*while_loop.cond);
    ASSERT_EQ(cond.op, TokenType::kGt);
    ASSERT_EQ(cond.left->type, AstNodeType::kIdent);
    const auto& left = static_cast<const AstIdent&>(*cond.left);
    ASSERT_EQ(left.name->view(), "x");
    expect_int_num(*cond.right, 0);
    ASSERT_EQ(count_children(while_loop.block->first_stat), 1);
    const AstNode* while_stat0 = get_child_at(while_loop.block->first_stat, 0);
    ASSERT_EQ(while_stat0->type, AstNodeType::kAssign);
    const auto& assign = static_cast<const AstAssign&>(*while_stat0);
    ASSERT_EQ(count_children(assign.first_var), 1);
    ASSERT_EQ(assign.first_var->type, AstNodeType::kIdent);
    const auto& var = static_cast<const AstIdent&>(*assign.first_var);
    ASSERT_EQ(var.name->view(), "x");
    ASSERT_EQ(count_children(assign.first_expr), 1);
    ASSERT_EQ(assign.first_expr->type, AstNodeType::kBinOp);
    const auto& expr = static_cast<const AstBinOp&>(*assign.first_expr);
    ASSERT_EQ(expr.op, TokenType::kMinus);
    ASSERT_EQ(expr.left->type, AstNodeType::kIdent);
    const auto& expr_left = static_cast<const AstIdent&>(*expr.left);
    ASSERT_EQ(expr_left.name->view(), "x");
    expect_int_num(*expr.right, 1);
}

TEST_F(ParserTest, ParserCStyleForLoop)
{
    const auto source = "for (let i = 0; i < 10; i++) { print(i) }";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kForC);
    const auto& for_c = static_cast<const AstForC&>(*stat0);

    ASSERT_EQ(for_c.init->type, AstNodeType::kLocalDecl);

    ASSERT_NE(for_c.condition, nullptr);

    ASSERT_NE(for_c.update, nullptr);

    ASSERT_NE(for_c.block, nullptr);
    ASSERT_EQ(count_children(for_c.block->first_stat), 1);
    const AstNode* for_stat0 = get_child_at(for_c.block->first_stat, 0);
    ASSERT_EQ(for_stat0->type, AstNodeType::kExprStat);
}

TEST_F(ParserTest, ParserIfWithElseifElse)
{
    const auto source = "if (a) { b() } elseif (c) { d() } else { e() }";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kIf);
    const auto& if_stat = static_cast<const AstIf&>(*stat0);
    ASSERT_EQ(if_stat.cond->type, AstNodeType::kIdent);
    const auto& cond1 = static_cast<const AstIdent&>(*if_stat.cond);
    ASSERT_EQ(cond1.name->view(), "a");
    ASSERT_EQ(count_children(if_stat.then_block->first_stat), 1);
    const AstNode* then_stat0 = get_child_at(if_stat.then_block->first_stat, 0);
    ASSERT_EQ(then_stat0->type, AstNodeType::kExprStat);
    const auto& then_expr = static_cast<const AstExprStat&>(*then_stat0);
    ASSERT_EQ(then_expr.expr->type, AstNodeType::kFuncCall);
    const auto& b_call = static_cast<const AstFuncCall&>(*then_expr.expr);
    ASSERT_EQ(b_call.func->type, AstNodeType::kIdent);
    const auto& b_func = static_cast<const AstIdent&>(*b_call.func);
    ASSERT_EQ(b_func.name->view(), "b");
    ASSERT_EQ(count_children(b_call.first_arg), 0);
    ASSERT_EQ(count_children(if_stat.first_elseif), 1);
    const ElseIf* elseif0 = static_cast<const ElseIf*>(get_child_at(if_stat.first_elseif, 0));
    ASSERT_EQ(elseif0->cond->type, AstNodeType::kIdent);
    const auto& elseif_cond = static_cast<const AstIdent&>(*elseif0->cond);
    ASSERT_EQ(elseif_cond.name->view(), "c");
    ASSERT_EQ(count_children(elseif0->block->first_stat), 1);
    const AstNode* elseif_stat0 = get_child_at(elseif0->block->first_stat, 0);
    ASSERT_EQ(elseif_stat0->type, AstNodeType::kExprStat);
    const auto& elseif_expr = static_cast<const AstExprStat&>(*elseif_stat0);
    ASSERT_EQ(elseif_expr.expr->type, AstNodeType::kFuncCall);
    const auto& d_call = static_cast<const AstFuncCall&>(*elseif_expr.expr);
    ASSERT_EQ(d_call.func->type, AstNodeType::kIdent);
    const auto& d_func = static_cast<const AstIdent&>(*d_call.func);
    ASSERT_EQ(d_func.name->view(), "d");
    ASSERT_TRUE(if_stat.else_block);
    ASSERT_EQ(count_children(if_stat.else_block->first_stat), 1);
    const AstNode* else_stat0 = get_child_at(if_stat.else_block->first_stat, 0);
    ASSERT_EQ(else_stat0->type, AstNodeType::kExprStat);
    const auto& else_expr = static_cast<const AstExprStat&>(*else_stat0);
    ASSERT_EQ(else_expr.expr->type, AstNodeType::kFuncCall);
    const auto& e_call = static_cast<const AstFuncCall&>(*else_expr.expr);
    ASSERT_EQ(e_call.func->type, AstNodeType::kIdent);
    const auto& e_func = static_cast<const AstIdent&>(*e_call.func);
    ASSERT_EQ(e_func.name->view(), "e");
}

TEST_F(ParserTest, ParserTableWithKeys)
{
    const auto source = "let t = {a = 1, [\"b\"] = 2, [3] = 4}";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kLocalDecl);
    const auto& local_decl = static_cast<const AstLocalDecl&>(*stat0);
    ASSERT_EQ(count_children(local_decl.first_init), 1);
    auto* init0 = get_child_at(local_decl.first_init, 0);
    ASSERT_EQ(init0->type, AstNodeType::kTableCtor);
    const auto& table = static_cast<const AstTableCtor&>(*init0);
    ASSERT_EQ(count_children(table.first_field), 3);
    const auto* field0 = static_cast<const TableField*>(get_child_at(table.first_field, 0));
    ASSERT_NE(field0->key, nullptr);
    ASSERT_EQ(field0->key->type, AstNodeType::kIdent);
    const auto& key1 = static_cast<const AstIdent&>(*field0->key);
    ASSERT_EQ(key1.name->view(), "a");
    expect_int_num(*field0->value, 1);
    const auto* field1 = static_cast<const TableField*>(get_child_at(table.first_field, 1));
    ASSERT_NE(field1->key, nullptr);
    ASSERT_EQ(field1->key->type, AstNodeType::kString);
    const auto& key2 = static_cast<const AstString&>(*field1->key);
    ASSERT_EQ(std::string(key2.view()), "b");
    expect_int_num(*field1->value, 2);
    const auto* field2 = static_cast<const TableField*>(get_child_at(table.first_field, 2));
    ASSERT_NE(field2->key, nullptr);
    expect_int_num(*field2->key, 3);
    expect_int_num(*field2->value, 4);
}

TEST_F(ParserTest, ParserMethodCall)
{
    const auto source = "obj:method(1, 2)";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kExprStat);
    const auto& expr_stat = static_cast<const AstExprStat&>(*stat0);
    ASSERT_EQ(expr_stat.expr->type, AstNodeType::kFuncCall);
    const auto& call = static_cast<const AstFuncCall&>(*expr_stat.expr);
    ASSERT_EQ(call.func->type, AstNodeType::kMember);
    const auto& mem = static_cast<const AstMember&>(*call.func);
    ASSERT_EQ(mem.table->type, AstNodeType::kIdent);
    const auto& table = static_cast<const AstIdent&>(*mem.table);
    ASSERT_EQ(table.name->view(), "obj");
    ASSERT_EQ(mem.name->view(), "method");
    ASSERT_EQ(count_children(call.first_arg), 3);
    ASSERT_EQ(get_child_at(call.first_arg, 0)->type, AstNodeType::kIdent);
    const auto& arg0 = static_cast<const AstIdent&>(*get_child_at(call.first_arg, 0));
    ASSERT_EQ(arg0.name->view(), "obj");
    expect_int_num(*get_child_at(call.first_arg, 1), 1);
    expect_int_num(*get_child_at(call.first_arg, 2), 2);
}

TEST_F(ParserTest, ParserUnaryOp)
{
    const auto source = "let x = -1, #t, ~0";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kLocalDecl);
    const auto& let = static_cast<const AstLocalDecl&>(*stat0);
    ASSERT_EQ(count_children(let.first_init), 3);
    auto* init0 = get_child_at(let.first_init, 0);
    ASSERT_EQ(init0->type, AstNodeType::kUnOp);
    const auto& un1 = static_cast<const AstUnOp&>(*init0);
    ASSERT_EQ(un1.op, TokenType::kMinus);
    expect_int_num(*un1.expr, 1);
    auto* init1 = get_child_at(let.first_init, 1);
    ASSERT_EQ(init1->type, AstNodeType::kUnOp);
    const auto& un2 = static_cast<const AstUnOp&>(*init1);
    ASSERT_EQ(un2.op, TokenType::kHash);
    ASSERT_EQ(un2.expr->type, AstNodeType::kIdent);
    const auto& ident2 = static_cast<const AstIdent&>(*un2.expr);
    ASSERT_EQ(ident2.name->view(), "t");
    auto* init2 = get_child_at(let.first_init, 2);
    ASSERT_EQ(init2->type, AstNodeType::kUnOp);
    const auto& un3 = static_cast<const AstUnOp&>(*init2);
    ASSERT_EQ(un3.op, TokenType::kBNot);
    expect_int_num(*un3.expr, 0);
}

TEST_F(ParserTest, ParserMultipleAssignment)
{
    const auto source = "a, b = 1, 2";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kAssign);
    const auto& assign = static_cast<const AstAssign&>(*stat0);
    ASSERT_EQ(count_children(assign.first_var), 2);
    ASSERT_EQ(assign.first_var->type, AstNodeType::kIdent);
    const auto& var1 = static_cast<const AstIdent&>(*assign.first_var);
    ASSERT_EQ(var1.name->view(), "a");
    const AstNode* var2_node = get_child_at(assign.first_var, 1);
    ASSERT_EQ(var2_node->type, AstNodeType::kIdent);
    const auto& var2 = static_cast<const AstIdent&>(*var2_node);
    ASSERT_EQ(var2.name->view(), "b");
    ASSERT_EQ(count_children(assign.first_expr), 2);
    expect_int_num(*assign.first_expr, 1);
    expect_int_num(*get_child_at(assign.first_expr, 1), 2);
}

TEST_F(ParserTest, ParserMultipleLocal)
{
    const auto source = "let a, b = 1, 2";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kLocalDecl);
    const auto& let = static_cast<const AstLocalDecl&>(*stat0);
    ASSERT_EQ(count_children(reinterpret_cast<const AstNode*>(let.first_name)), 2);
    auto* name0 = static_cast<const AstString*>(get_child_at(reinterpret_cast<const AstNode*>(let.first_name), 0));
    ASSERT_EQ(name0->view(), "a");
    auto* name1 = static_cast<const AstString*>(get_child_at(reinterpret_cast<const AstNode*>(let.first_name), 1));
    ASSERT_EQ(name1->view(), "b");
    ASSERT_EQ(count_children(let.first_init), 2);
    expect_int_num(*get_child_at(let.first_init, 0), 1);
    expect_int_num(*get_child_at(let.first_init, 1), 2);
}

TEST_F(ParserTest, ParserReturnMultiple)
{
    const auto source = "function f() { return 1, 2 }";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kFuncDefStat);
    const auto& func = static_cast<const AstFuncDefStat&>(*stat0);
    ASSERT_EQ(count_children(func.block->first_stat), 1);
    const AstNode* func_stat0 = get_child_at(func.block->first_stat, 0);
    ASSERT_EQ(func_stat0->type, AstNodeType::kReturn);
    const auto& ret = static_cast<const AstReturn&>(*func_stat0);
    ASSERT_EQ(count_children(ret.first_expr), 2);
    expect_int_num(*get_child_at(ret.first_expr, 0), 1);
    expect_int_num(*get_child_at(ret.first_expr, 1), 2);
}

TEST_F(ParserTest, ParserBreakContinue)
{
    const auto source = "while (true) { if (cond) { break } else { continue } }";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    auto ast = parse(holder, tokens);
    ASSERT_EQ(count_children(ast->block->first_stat), 1);
    const AstNode* stat0 = get_child_at(ast->block->first_stat, 0);
    ASSERT_EQ(stat0->type, AstNodeType::kWhile);
    const auto& wh = static_cast<const AstWhile&>(*stat0);
    ASSERT_EQ(count_children(wh.block->first_stat), 1);
    const AstNode* wh_stat0 = get_child_at(wh.block->first_stat, 0);
    ASSERT_EQ(wh_stat0->type, AstNodeType::kIf);
    const auto& iff = static_cast<const AstIf&>(*wh_stat0);
    ASSERT_EQ(count_children(iff.then_block->first_stat), 1);
    const AstNode* then_stat0 = get_child_at(iff.then_block->first_stat, 0);
    ASSERT_EQ(then_stat0->type, AstNodeType::kBreak);
    ASSERT_TRUE(iff.else_block);
    ASSERT_EQ(count_children(iff.else_block->first_stat), 1);
    const AstNode* else_stat0 = get_child_at(iff.else_block->first_stat, 0);
    ASSERT_EQ(else_stat0->type, AstNodeType::kContinue);
}

TEST_F(ParserTest, ParserErrorMissingEnd)
{
    const auto source = "if true { print()";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    ASSERT_THROW(parse(holder, tokens), behl::SyntaxError);
}

TEST_F(ParserTest, ParserErrorInvalidAssign)
{
    const auto source = "1 = 2";
    auto tokens = tokenize(S, source);
    AstHolder holder(S);
    ASSERT_THROW(parse(holder, tokens), behl::SyntaxError);
}
