#include "behl/behl.hpp"

#include <gtest/gtest.h>
#include <string>

class ToStringTest : public ::testing::Test
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

TEST_F(ToStringTest, ToString_Nil)
{
    constexpr std::string_view code = R"(
        let x = nil;
        return tostring(x);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "nil");
}

TEST_F(ToStringTest, ToString_BooleanTrue)
{
    constexpr std::string_view code = R"(
        return tostring(true);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "true");
}

TEST_F(ToStringTest, ToString_BooleanFalse)
{
    constexpr std::string_view code = R"(
        return tostring(false);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "false");
}

TEST_F(ToStringTest, ToString_Integer)
{
    constexpr std::string_view code = R"(
        return tostring(42);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "42");
}

TEST_F(ToStringTest, ToString_NegativeInteger)
{
    constexpr std::string_view code = R"(
        return tostring(-123);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "-123");
}

TEST_F(ToStringTest, ToString_Float)
{
    constexpr std::string_view code = R"(
        return tostring(3.14159);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(std::string(result).find("3.14"), std::string_view::npos);
}

TEST_F(ToStringTest, ToString_NegativeFloat)
{
    constexpr std::string_view code = R"(
        return tostring(-2.718);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(std::string(result).find("-2.7"), std::string_view::npos);
}

TEST_F(ToStringTest, ToString_String)
{
    constexpr std::string_view code = R"(
        return tostring("hello");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "hello");
}

TEST_F(ToStringTest, ToString_EmptyString)
{
    constexpr std::string_view code = R"(
        return tostring("");
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "");
}

TEST_F(ToStringTest, ToString_Function)
{
    constexpr std::string_view code = R"(
        function foo() {}
        return tostring(foo);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("function:"), std::string_view::npos);
}

TEST_F(ToStringTest, ToString_CFunction)
{
    constexpr std::string_view code = R"(
        return tostring(print);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("cfunction:"), std::string_view::npos);
}

TEST_F(ToStringTest, ToString_Table)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3};
        return tostring(t);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("table:"), std::string_view::npos);
}

TEST_F(ToStringTest, ToString_EmptyTable)
{
    constexpr std::string_view code = R"(
        let t = {};
        return tostring(t);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("table:"), std::string_view::npos);
}

TEST_F(ToStringTest, ToString_TableWithMetamethod)
{
    constexpr std::string_view code = R"(
        let t = {value = 42};
        let mt = {
            __tostring = function(self) {
                return "custom:" + tostring(self.value);
            }
        };
        setmetatable(t, mt);
        return tostring(t);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "custom:42");
}

TEST_F(ToStringTest, ToString_TableWithMetamethodReturningNumber)
{
    constexpr std::string_view code = R"(
        let t = {};
        let mt = {
            __tostring = function(self) {
                return tostring(123);  // Must return string
            }
        };
        setmetatable(t, mt);
        return tostring(t);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "123");
}

TEST_F(ToStringTest, ToString_TableWithMetamethodReturningNil)
{
    constexpr std::string_view code = R"(
        let t = {};
        let mt = {
            __tostring = function(self) {
                return "nil";  // Must return string
            }
        };
        setmetatable(t, mt);
        return tostring(t);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "nil");
}

TEST_F(ToStringTest, ToString_ZeroInteger)
{
    constexpr std::string_view code = R"(
        return tostring(0);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "0");
}

TEST_F(ToStringTest, ToString_ZeroFloat)
{
    constexpr std::string_view code = R"(
        return tostring(0.0);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_TRUE(result == "0" || result == "0.0" || result == "0.000000");
}

TEST_F(ToStringTest, ToString_LargeInteger)
{
    constexpr std::string_view code = R"(
        return tostring(9223372036854775807);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "9223372036854775807");
}

TEST_F(ToStringTest, ToString_SmallFloat)
{
    constexpr std::string_view code = R"(
        return tostring(0.0001);
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "0.0001");
}

TEST_F(ToStringTest, ToString_MultipleValues)
{
    constexpr std::string_view code = R"(
        let a = tostring(1);
        let b = tostring(2);
        let c = tostring(3);
        return a + b + c;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "123");
}

TEST_F(ToStringTest, ToString_InExpression)
{
    constexpr std::string_view code = R"(
        let result = "Value: " + tostring(42) + "!";
        return result;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "Value: 42!");
}

TEST_F(ToStringTest, ToString_InLoop)
{
    constexpr std::string_view code = R"(
        let result = "";
        for (let i = 0; i < 3; i++) {
            result = result + tostring(i);
        }
        return result;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "012");
}

TEST_F(ToStringTest, ToString_NestedCalls)
{
    constexpr std::string_view code = R"(
        return tostring(tostring(42));
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "42");
}
