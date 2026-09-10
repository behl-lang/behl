/**
 * @file numeric_literal_tests.cpp
 * @brief Tests for numeric literal parsing (decimal and hexadecimal)
 */

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class NumericLiteralTest : public ::testing::Test
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
    }
};

TEST_F(NumericLiteralTest, HexadecimalLowercase)
{
    constexpr std::string_view code = R"(
        let a = 0xff;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 255);
}

TEST_F(NumericLiteralTest, HexadecimalUppercase)
{
    constexpr std::string_view code = R"(
        let a = 0xFF;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 255);
}

TEST_F(NumericLiteralTest, HexadecimalUppercaseX)
{
    constexpr std::string_view code = R"(
        let a = 0XFF;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 255);
}

TEST_F(NumericLiteralTest, HexadecimalMixedCase)
{
    constexpr std::string_view code = R"(
        let a = 0xAbCdEf;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 11259375);
}

TEST_F(NumericLiteralTest, HexadecimalZero)
{
    constexpr std::string_view code = R"(
        let a = 0x0;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(NumericLiteralTest, HexadecimalSingleDigit)
{
    constexpr std::string_view code = R"(
        let a = 0xF;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 15);
}

TEST_F(NumericLiteralTest, HexadecimalLarge)
{
    constexpr std::string_view code = R"(
        let a = 0xFFFFFF;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 16777215);
}

TEST_F(NumericLiteralTest, HexadecimalAddition)
{
    constexpr std::string_view code = R"(
        let a = 0x10 + 0x20;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 48);
}

TEST_F(NumericLiteralTest, HexadecimalBitwiseAND)
{
    constexpr std::string_view code = R"(
        let a = 0xFF & 0xF0;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0xF0);
}

TEST_F(NumericLiteralTest, HexadecimalBitwiseOR)
{
    constexpr std::string_view code = R"(
        let a = 0x0F | 0xF0;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0xFF);
}

TEST_F(NumericLiteralTest, HexadecimalBitwiseXOR)
{
    constexpr std::string_view code = R"(
        let a = 0xFF ^ 0xAA;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0x55);
}

TEST_F(NumericLiteralTest, HexadecimalBitwiseNOT)
{
    constexpr std::string_view code = R"(
        let a = ~0xF;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), ~15);
}

TEST_F(NumericLiteralTest, HexadecimalLeftShift)
{
    constexpr std::string_view code = R"(
        let a = 0x1 << 4;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0x10);
}

TEST_F(NumericLiteralTest, HexadecimalRightShift)
{
    constexpr std::string_view code = R"(
        let a = 0x80 >> 4;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0x8);
}

TEST_F(NumericLiteralTest, DecimalInteger)
{
    constexpr std::string_view code = R"(
        let a = 42;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(NumericLiteralTest, DecimalFloat)
{
    constexpr std::string_view code = R"(
        let a = 3.14159;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 3.14159);
}

TEST_F(NumericLiteralTest, DecimalZero)
{
    constexpr std::string_view code = R"(
        let a = 0;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(NumericLiteralTest, DecimalLeadingZero)
{
    constexpr std::string_view code = R"(
        let a = 007;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 7);
}

TEST_F(NumericLiteralTest, MixedDecimalAndHex)
{
    constexpr std::string_view code = R"(
        let a = 10 + 0x10;
        return a;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 26);
}

TEST_F(NumericLiteralTest, MixedInTable)
{
    constexpr std::string_view code = R"(
        let t = {10, 0x10, 20, 0x20};
        return t[1];
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0x10);
}

TEST_F(NumericLiteralTest, HexadecimalInFunction)
{
    constexpr std::string_view code = R"(
        function get_color() {
            return 0xFF00FF;
        }
        return get_color();
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0xFF00FF);
}

TEST_F(NumericLiteralTest, HexadecimalAsTableKey)
{
    constexpr std::string_view code = R"(
        let t = {};
        t[0xFF] = "color";
        return t[255];
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_string(S, -1), "color");
}

TEST_F(NumericLiteralTest, HexadecimalComparison)
{
    constexpr std::string_view code = R"(
        if (0xFF == 255) {
            return 1;
        } else {
            return 0;
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}
