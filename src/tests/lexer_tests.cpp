#include "frontend/lexer.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
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
    std::string_view source = "let x = 1 const y = {0,1} if (x) { print(y) }";
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
    std::string_view source = "a + b - c * d / e % f ^ g & h | i << j >> k ~ l && m || n ! o != p == q < r <= s > t >= u";
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

TEST_F(LexerTest, LexerUnicodeIdentifierLatin1Range)
{
    std::string_view source = "let caf\xC3\xA9 = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[0].type, TokenType::kLet);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "caf\xC3\xA9");
    ASSERT_EQ(tokens[2].type, TokenType::kAssign);
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
    ASSERT_EQ(tokens[4].type, TokenType::kEOF);
}

TEST_F(LexerTest, LexerUnicodeIdentifierAboveLatin1)
{
    std::string_view source = "let \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E = 2";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerUnicodeIdentifierGreekAndAstral)
{
    std::string_view source = "let \xCF\x80 = 3 let \xF0\x9F\x98\x80 = 4";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 9);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xCF\x80");
    ASSERT_EQ(tokens[5].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[5].value, "\xF0\x9F\x98\x80");
}

TEST_F(LexerTest, LexerUnicodeIdentifierContinuesAfterAsciiStart)
{
    std::string_view source = "let a\xCF\x80"
                              "b = 5";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value,
        "a\xCF\x80"
        "b");
}

TEST_F(LexerTest, LexerRejectsNonBreakingSpaceInIdentifier)
{
    std::string_view source = "let a\xC2\xA0"
                              "b = 1";
    ASSERT_THROW(tokenize(S, source), SyntaxError);
}

TEST_F(LexerTest, LexerRejectsZeroWidthCharacters)
{
    ASSERT_THROW(tokenize(S,
                     "let a\xE2\x80\x8B"
                     "b = 1"),
        SyntaxError);
    ASSERT_THROW(tokenize(S,
                     "let a\xE2\x80\x8C"
                     "b = 1"),
        SyntaxError);
    ASSERT_THROW(tokenize(S,
                     "let a\xE2\x80\x8D"
                     "b = 1"),
        SyntaxError);
    ASSERT_THROW(tokenize(S,
                     "let a\xEF\xBB\xBF"
                     "b = 1"),
        SyntaxError);
}

TEST_F(LexerTest, LexerRejectsBidiControlsInIdentifier)
{
    ASSERT_THROW(tokenize(S,
                     "let a\xE2\x80\xAE"
                     "b = 1"),
        SyntaxError);
    ASSERT_THROW(tokenize(S,
                     "let a\xE2\x81\xA6"
                     "b = 1"),
        SyntaxError);
}

TEST_F(LexerTest, LexerRejectsUnicodeSpaceSeparators)
{
    ASSERT_THROW(tokenize(S,
                     "let a\xE2\x80\x83"
                     "b = 1"),
        SyntaxError);
    ASSERT_THROW(tokenize(S,
                     "let a\xE3\x80\x80"
                     "b = 1"),
        SyntaxError);
}

TEST_F(LexerTest, LexerNonAsciiIsNeverWhitespace)
{
    ASSERT_THROW(tokenize(S, "let\xC2\xA0x = 1"), SyntaxError);
    ASSERT_THROW(tokenize(S, "let\xE3\x80\x80x = 1"), SyntaxError);
}

TEST_F(LexerTest, LexerNonAsciiDigitsAreNotNumbers)
{
    std::string_view source = "let x = \xD9\xA0";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[3].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[3].value, "\xD9\xA0");
}

TEST_F(LexerTest, LexerNonAsciiDigitsDoNotExtendNumbers)
{
    std::string_view source = "let x = 1\xD9\xA0";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
    ASSERT_EQ(tokens[3].value, "1");
    ASSERT_EQ(tokens[4].type, TokenType::kIdentifier);
}

TEST_F(LexerTest, LexerUnicodeInStringsAndComments)
{
    std::string_view source = "// \xE6\x97\xA5\xE6\x9C\xAC\xC2\xA0\xE2\x80\xAE\n"
                              "let s = \"caf\xC3\xA9 \xF0\x9F\x98\x80\"";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "s");
    ASSERT_EQ(tokens[3].type, TokenType::kString);
}

static std::string lex_error(behl::State* S, std::string_view source)
{
    try
    {
        tokenize(S, source);
    }
    catch (const BehlException& e)
    {
        return e.what();
    }
    return "<no error>";
}

static void expect_invalid_utf8(behl::State* S, std::string_view source)
{
    const std::string message = lex_error(S, source);
    EXPECT_NE(message.find("Invalid UTF-8"), std::string::npos) << "got: " << message;
}

TEST_F(LexerTest, LexerRejectsInvalidContinuationByte)
{
    expect_invalid_utf8(S, "let a\xC3\x41 = 1");
    expect_invalid_utf8(S, "let \xE6\x97\x41 = 1");
    expect_invalid_utf8(S, "let \xF0\x9F\x98\x41 = 1");
}

TEST_F(LexerTest, LexerRejectsOverlongEncoding)
{
    expect_invalid_utf8(S, "let \xC0\xA2 = 1");
    expect_invalid_utf8(S, "let \xC1\xBF = 1");
    expect_invalid_utf8(S, "let \xE0\x80\xA2 = 1");
    expect_invalid_utf8(S, "let \xE0\x9F\xBF = 1");
    expect_invalid_utf8(S, "let \xF0\x80\x80\xA2 = 1");
    expect_invalid_utf8(S, "let \xF0\x8F\xBF\xBF = 1");
}

TEST_F(LexerTest, LexerRejectsSurrogateCodepoints)
{
    expect_invalid_utf8(S, "let \xED\xA0\x80 = 1");
    expect_invalid_utf8(S, "let \xED\xBF\xBF = 1");
}

TEST_F(LexerTest, LexerRejectsBareContinuationByte)
{
    expect_invalid_utf8(S, "let \x80x = 1");
    expect_invalid_utf8(S, "let \xBFx = 1");
}

TEST_F(LexerTest, LexerRejectsCodepointAboveUnicodeMax)
{
    expect_invalid_utf8(S, "let \xF4\x90\x80\x80 = 1");
    expect_invalid_utf8(S, "let \xF5\x80\x80\x80 = 1");
    expect_invalid_utf8(S, "let \xF7\xBF\xBF\xBF = 1");
    expect_invalid_utf8(S, "let \xF8\x80\x80\x80 = 1");
    expect_invalid_utf8(S, "let \xFF = 1");
}

TEST_F(LexerTest, LexerRejectsTruncatedSequence)
{
    expect_invalid_utf8(S, "let \xC2");
    expect_invalid_utf8(S, "let \xE6\x97");
    expect_invalid_utf8(S, "let \xF0\x9F\x98");
}

TEST_F(LexerTest, LexerRejectsMalformedUtf8InStringsAndComments)
{
    expect_invalid_utf8(S, "let s = \"caf\xE9\"");
    expect_invalid_utf8(S, "// caf\xE9\nlet x = 1");
}

TEST_F(LexerTest, LexerAcceptsMinTwoByte)
{
    std::string_view source = "let \xC2\x80 = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xC2\x80");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerAcceptsMaxTwoByte)
{
    std::string_view source = "let \xDF\xBF = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xDF\xBF");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerAcceptsMinThreeByte)
{
    std::string_view source = "let \xE0\xA0\x80 = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xE0\xA0\x80");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerAcceptsJustBelowSurrogates)
{
    std::string_view source = "let \xED\x9F\xBF = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xED\x9F\xBF");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerAcceptsJustAboveSurrogates)
{
    std::string_view source = "let \xEE\x80\x80 = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xEE\x80\x80");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerAcceptsMaxThreeByte)
{
    std::string_view source = "let \xEF\xBF\xBF = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xEF\xBF\xBF");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerAcceptsMinFourByte)
{
    std::string_view source = "let \xF0\x90\x80\x80 = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xF0\x90\x80\x80");
    ASSERT_EQ(tokens[3].type, TokenType::kNumber);
}

TEST_F(LexerTest, LexerAcceptsMaxValidCodepoint)
{
    std::string_view source = "let \xF4\x8F\xBF\xBF = 1";
    auto tokens = tokenize(S, source);

    ASSERT_EQ(tokens[1].type, TokenType::kIdentifier);
    ASSERT_EQ(tokens[1].value, "\xF4\x8F\xBF\xBF");
}
