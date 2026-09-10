#include "frontend/lexer.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

using namespace behl;

class LexerTest : public ::testing::Test
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

TEST_F(LexerTest, LexerBasic)
{
    std::string source = "let x = 1 const y = {0,1} if (x) { print(y) }";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 23);

    ASSERT_EQ(tokens[0].type, TokenType::kLet);
    ASSERT_EQ(tokens[0].value, "let");

    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "x");

    ASSERT_EQ(tokens[2].type, TokenType::kAssign);
    ASSERT_EQ(tokens[2].value, "=");

    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
    ASSERT_EQ(tokens[3].value, "1");

    ASSERT_EQ(tokens[4].type, TokenType::kConst);
    ASSERT_EQ(tokens[4].value, "const");

    ASSERT_EQ(tokens[5].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[5].value, "y");

    ASSERT_EQ(tokens[6].type, TokenType::kAssign);
    ASSERT_EQ(tokens[6].value, "=");

    ASSERT_EQ(tokens[7].type, TokenType::kLBrace);
    ASSERT_EQ(tokens[7].value, "{");

    ASSERT_EQ(tokens[8].type, TokenType::kNumber);
    ASSERT_EQ(tokens[8].value, "0");

    ASSERT_EQ(tokens[9].type, TokenType::kComma);
    ASSERT_EQ(tokens[9].value, ",");

    ASSERT_EQ(tokens[10].type, TokenType::kNumber);
    ASSERT_EQ(tokens[10].value, "1");

    ASSERT_EQ(tokens[11].type, TokenType::kRBrace);
    ASSERT_EQ(tokens[11].value, "}");

    ASSERT_EQ(tokens[12].type, TokenType::kIf);
    ASSERT_EQ(tokens[12].value, "if");

    ASSERT_EQ(tokens[13].type, TokenType::kLParen);
    ASSERT_EQ(tokens[13].value, "(");

    ASSERT_EQ(tokens[14].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[14].value, "x");

    ASSERT_EQ(tokens[15].type, TokenType::kRParen);
    ASSERT_EQ(tokens[15].value, ")");

    ASSERT_EQ(tokens[16].type, TokenType::kLBrace);
    ASSERT_EQ(tokens[16].value, "{");

    ASSERT_EQ(tokens[17].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[17].value, "print");

    ASSERT_EQ(tokens[18].type, TokenType::kLParen);
    ASSERT_EQ(tokens[18].value, "(");

    ASSERT_EQ(tokens[19].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[19].value, "y");

    ASSERT_EQ(tokens[20].type, TokenType::kRParen);
    ASSERT_EQ(tokens[20].value, ")");

    ASSERT_EQ(tokens[21].type, TokenType::kRBrace);
    ASSERT_EQ(tokens[21].value, "}");

    ASSERT_EQ(tokens[22].type, TokenType::kEOF);
}

TEST_F(LexerTest, LexerOperators)
{
    std::string source = "a + b - c * d / e % f ^ g & h | i << j >> k ~ l && m || n ! o != p == q < r <= s > t >= u";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens[1].type, TokenType::kPlus);
    ASSERT_EQ(tokens[3].type, TokenType::kMinus);
    ASSERT_EQ(tokens[5].type, TokenType::kStar);
    ASSERT_EQ(tokens[7].type, TokenType::kSlash);
    ASSERT_EQ(tokens[9].type, TokenType::kPercent);
    ASSERT_EQ(tokens[11].type, TokenType::kBXor);
    ASSERT_EQ(tokens[13].type, TokenType::kBAnd);
    ASSERT_EQ(tokens[15].type, TokenType::kBOr);
    ASSERT_EQ(tokens[17].type, TokenType::kBShl);
    ASSERT_EQ(tokens[19].type, TokenType::kBShr);
    ASSERT_EQ(tokens[21].type, TokenType::kBNot);
    ASSERT_EQ(tokens[23].type, TokenType::kAndOp);
    ASSERT_EQ(tokens[25].type, TokenType::kOrOp);
    ASSERT_EQ(tokens[27].type, TokenType::kNotOp);
    ASSERT_EQ(tokens[29].type, TokenType::kNe);
    ASSERT_EQ(tokens[31].type, TokenType::kEq);
    ASSERT_EQ(tokens[33].type, TokenType::kLt);
    ASSERT_EQ(tokens[35].type, TokenType::kLe);
    ASSERT_EQ(tokens[37].type, TokenType::kGt);
    ASSERT_EQ(tokens[39].type, TokenType::kGe);
    ASSERT_EQ(tokens[41].type, TokenType::kEOF);
}
