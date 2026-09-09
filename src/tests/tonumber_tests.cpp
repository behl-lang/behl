#include "behl/behl.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <string>

class ToNumberTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        ASSERT_NE(S, nullptr);
        behl::load_lib_core(S);
    }

    void TearDown() override
    {
        if (S)
        {
            behl::close(S);
            S = nullptr;
        }
    }
};

TEST_F(ToNumberTest, ToNumber_Integer)
{
    constexpr std::string_view code = R"(
        let result = tonumber(42);
        return result;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 42);
}

TEST_F(ToNumberTest, ToNumber_NegativeInteger)
{
    constexpr std::string_view code = R"(
        return tonumber(-123);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), -123);
}

TEST_F(ToNumberTest, ToNumber_ZeroInteger)
{
    constexpr std::string_view code = R"(
        return tonumber(0);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 0);
}

TEST_F(ToNumberTest, ToNumber_LargeInteger)
{
    constexpr std::string_view code = R"(
        return tonumber(9223372036854775807);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 9223372036854775807LL);
}

TEST_F(ToNumberTest, ToNumber_Float)
{
    constexpr std::string_view code = R"(
        return tonumber(3.14);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_NEAR(to_number(S, -1), 3.14, 0.01);
}

TEST_F(ToNumberTest, ToNumber_NegativeFloat)
{
    constexpr std::string_view code = R"(
        return tonumber(-2.718);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_NEAR(to_number(S, -1), -2.718, 0.001);
}

TEST_F(ToNumberTest, ToNumber_ZeroFloat)
{
    constexpr std::string_view code = R"(
        return tonumber(0.0);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_DOUBLE_EQ(to_number(S, -1), 0.0);
}

TEST_F(ToNumberTest, ToNumber_StringInteger)
{
    constexpr std::string_view code = R"(
        return tonumber("42");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 42);
}

TEST_F(ToNumberTest, ToNumber_StringNegativeInteger)
{
    constexpr std::string_view code = R"(
        return tonumber("-123");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), -123);
}

TEST_F(ToNumberTest, ToNumber_StringFloat)
{
    constexpr std::string_view code = R"(
        return tonumber("3.14159");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_NEAR(to_number(S, -1), 3.14159, 0.00001);
}

TEST_F(ToNumberTest, ToNumber_StringNegativeFloat)
{
    constexpr std::string_view code = R"(
        return tonumber("-2.5");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_NEAR(to_number(S, -1), -2.5, 0.01);
}

TEST_F(ToNumberTest, ToNumber_StringZero)
{
    constexpr std::string_view code = R"(
        return tonumber("0");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 0);
}

TEST_F(ToNumberTest, ToNumber_StringWithSpaces)
{
    constexpr std::string_view code = R"(
        return tonumber("  42  ");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_StringWithLeadingZeros)
{
    constexpr std::string_view code = R"(
        return tonumber("0042");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 42);
}

TEST_F(ToNumberTest, ToNumber_InvalidString)
{
    constexpr std::string_view code = R"(
        return tonumber("hello");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_EmptyString)
{
    constexpr std::string_view code = R"(
        return tonumber("");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_StringWithText)
{
    constexpr std::string_view code = R"(
        return tonumber("42abc");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_StringWithTrailingText)
{
    constexpr std::string_view code = R"(
        return tonumber("3.14pi");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_StringMultipleDots)
{
    constexpr std::string_view code = R"(
        return tonumber("1.2.3");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_Nil)
{
    constexpr std::string_view code = R"(
        return tonumber(nil);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_Boolean)
{
    constexpr std::string_view code = R"(
        return tonumber(true);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_BooleanFalse)
{
    constexpr std::string_view code = R"(
        return tonumber(false);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_Table)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3};
        return tonumber(t);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_Function)
{
    constexpr std::string_view code = R"(
        function foo() {}
        return tonumber(foo);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_ScientificNotation)
{
    constexpr std::string_view code = R"(
        return tonumber("1e10");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_NEAR(to_number(S, -1), 1e10, 1e5);
}

TEST_F(ToNumberTest, ToNumber_ScientificNotationNegative)
{
    constexpr std::string_view code = R"(
        return tonumber("1e-5");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_NEAR(to_number(S, -1), 1e-5, 1e-10);
}

TEST_F(ToNumberTest, ToNumber_HexString)
{
    constexpr std::string_view code = R"(
        return tonumber("0x10");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_F(ToNumberTest, ToNumber_MultipleConversions)
{
    constexpr std::string_view code = R"(
        let a = tonumber("10");
        let b = tonumber("20");
        let c = tonumber("30");
        return a + b + c;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 60);
}

TEST_F(ToNumberTest, ToNumber_InExpression)
{
    constexpr std::string_view code = R"(
        let result = tonumber("5") * tonumber("3");
        return result;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 15);
}

TEST_F(ToNumberTest, ToNumber_InLoop)
{
    constexpr std::string_view code = R"(
        let sum = 0;
        for (let i = 0; i < 3; i++) {
            sum = sum + tonumber(tostring(i));
        }
        return sum;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 3);
}

TEST_F(ToNumberTest, ToNumber_WithConditional)
{
    constexpr std::string_view code = R"(
        let x = tonumber("not a number");
        if (x == nil) {
            return 999;
        }
        return x;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 999);
}

TEST_F(ToNumberTest, ToNumber_ValidStringConditional)
{
    constexpr std::string_view code = R"(
        let x = tonumber("42");
        if (x != nil) {
            return x;
        } else {
            return -1;
        }
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 42);
}

TEST_F(ToNumberTest, ToNumber_PreservesIntegerType)
{
    constexpr std::string_view code = R"(
        return typeof(tonumber("123"));
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "integer");
}

TEST_F(ToNumberTest, ToNumber_PreservesFloatType)
{
    constexpr std::string_view code = R"(
        return typeof(tonumber("3.14"));
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "number");
}

TEST_F(ToNumberTest, ToNumber_IntegerPassthrough)
{
    constexpr std::string_view code = R"(
        return typeof(tonumber(42));
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "integer");
}

TEST_F(ToNumberTest, ToNumber_FloatPassthrough)
{
    constexpr std::string_view code = R"(
        return typeof(tonumber(3.14));
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "number");
}
