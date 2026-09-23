#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

#include <array>
#include <string>

class OperatorTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_P(OperatorTest, ExecuteArithmetic)
{
    constexpr std::string_view code = R"(
        return 10 + 5
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(OperatorTest, NumberAdditionStillWorks)
{
    constexpr std::string_view code = R"(
        return 1 + 2 + 3
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(OperatorTest, CompoundAssignmentPlusEquals)
{
    constexpr std::string_view code = R"(
        let x = 10
        x += 5
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(OperatorTest, CompoundAssignmentMinusEquals)
{
    constexpr std::string_view code = R"(
        let x = 20
        x -= 8
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(OperatorTest, CompoundAssignmentStarEquals)
{
    constexpr std::string_view code = R"(
        let x = 6
        x *= 7
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(OperatorTest, CompoundAssignmentSlashEquals)
{
    constexpr std::string_view code = R"(
        let x = 100
        x /= 5
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 20);
}

TEST_P(OperatorTest, CompoundAssignmentPercentEquals)
{
    constexpr std::string_view code = R"(
        let x = 17
        x %= 5
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(OperatorTest, IncrementOperator)
{
    constexpr std::string_view code = R"(
        let x = 5
        x++
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(OperatorTest, DecrementOperator)
{
    constexpr std::string_view code = R"(
        let x = 10
        x--
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 9);
}

TEST_P(OperatorTest, IncrementInLoop)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 5; i++) {
            sum += i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(OperatorTest, CompoundAssignmentChained)
{
    constexpr std::string_view code = R"(
        let x = 10
        x += 5
        x *= 2
        x -= 6
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 24);
}

TEST_P(OperatorTest, IncrementGlobal)
{
    constexpr std::string_view code = R"(
        g = 5
        g++
        return g
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(OperatorTest, DecrementGlobal)
{
    constexpr std::string_view code = R"(
        g = 10
        g--
        return g
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 9);
}

TEST_P(OperatorTest, LogicalNotOperator)
{
    constexpr std::string_view code = R"(
        let a = true
        a = !a
        return a
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, LogicalNotOnFalse)
{
    constexpr std::string_view code = R"(
        return !false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, LogicalNotOnNil)
{
    constexpr std::string_view code = R"(
        return !nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, LogicalNotOnNumber)
{
    constexpr std::string_view code = R"(
        return !0
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, MultipleAssignmentIndexUsesValueBeforeAssignment)
{
    constexpr std::string_view code = R"(
        let a = {}
        let i = 0
        i, a[i] = 1, 99
        return i, a[0], a[1]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 1);
    EXPECT_EQ(behl::to_integer(S, -2), 99);
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(OperatorTest, MultipleAssignmentTableUsesValueBeforeAssignment)
{
    constexpr std::string_view code = R"(
        let a = {}
        let c = a
        a, a[0] = 5, 99
        return a, c[0]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 5);
    EXPECT_EQ(behl::to_integer(S, -1), 99);
}

TEST_P(OperatorTest, BitwiseNotOnIntegralFloatMatchesBinaryBitwise)
{
    constexpr std::string_view code = R"(
        let v = 3.0
        return 3.0 & -1, ~3.0, ~v
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 3);
    EXPECT_EQ(behl::to_integer(S, -2), -4);
    EXPECT_EQ(behl::to_integer(S, -1), -4);
}

TEST_P(OperatorTest, BitwisePrecedenceWithoutParentheses)
{
    constexpr std::string_view code = R"(
        let r = ""
        r = r + tostring(1 | 2 & 3) + ","
        r = r + tostring(1 | 6 ^ 3) + ","
        r = r + tostring(5 & 3 ^ 1) + ","
        r = r + tostring(1 << 2 + 1) + ","
        r = r + tostring(2 + 1 << 1) + ","
        r = r + tostring(6 & 3 == 2) + ","
        r = r + tostring(8 >> 1 | 1) + ","
        r = r + tostring(~0 & 5) + ","
        r = r + tostring(1 | 2 ^ 3 & 4)
        let a = 1
        let b = 2
        let c = 3
        let d = 4
        r = r + ";" + tostring(a | b & c) + "," + tostring(a | b ^ c & d) + "," + tostring(a << b + a) + "," + tostring(b * c >> a)
        return r
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "3,5,0,8,6,true,5,5,3;3,3,8,3");
}

TEST_P(OperatorTest, LogicalOperatorsAllCombinations)
{
    constexpr std::array<std::string_view, 6> values = {"nil", "false", "true", "0", "1", "'s'"};
    const auto truthy = [](std::string_view v) { return v != "nil" && v != "false"; };
    std::string code = "let bad = \"\"\n"
                       "function chk(ok, s) { if (ok != true) { bad = bad + s + \";\" } }\n";
    for (size_t i = 0; i < values.size(); ++i)
    {
        const std::string a(values[i]);
        const std::string na = truthy(a) ? "false" : "true";
        const std::string li = std::to_string(i);
        code += "{ let a = " + a + "; let r = 0\n";
        code += "  chk((!" + a + ") == " + na + ", \"not lit " + li + "\")\n";
        code += "  chk((!a) == " + na + ", \"not var " + li + "\")\n";
        code += "  if (!a) { r = true } else { r = false }; chk(r == " + na + ", \"if not var " + li + "\")\n";
        code += "  if (!" + a + ") { r = true } else { r = false }; chk(r == " + na + ", \"if not lit " + li + "\")\n";
        code += "}\n";
        for (size_t j = 0; j < values.size(); ++j)
        {
            const std::string b(values[j]);
            const std::string and_v = truthy(a) ? b : a;
            const std::string or_v = truthy(a) ? a : b;
            const std::string t_and = truthy(and_v) ? "true" : "false";
            const std::string t_or = truthy(or_v) ? "true" : "false";
            const std::string nt_and = truthy(and_v) ? "false" : "true";
            const std::string lab = li + " " + std::to_string(j);
            code += "{ let a = " + a + "; let b = " + b + "; let r = 0\n";
            code += "  chk((" + a + " && " + b + ") == " + and_v + ", \"and lit " + lab + "\")\n";
            code += "  chk((a && b) == " + and_v + ", \"and var " + lab + "\")\n";
            code += "  chk((" + a + " || " + b + ") == " + or_v + ", \"or lit " + lab + "\")\n";
            code += "  chk((a || b) == " + or_v + ", \"or var " + lab + "\")\n";
            code += "  chk((!(a && b)) == " + nt_and + ", \"not and var " + lab + "\")\n";
            code += "  if (" + a + " && " + b + ") { r = true } else { r = false }; chk(r == " + t_and + ", \"if and lit " + lab + "\")\n";
            code += "  if (a && b) { r = true } else { r = false }; chk(r == " + t_and + ", \"if and var " + lab + "\")\n";
            code += "  if (" + a + " || " + b + ") { r = true } else { r = false }; chk(r == " + t_or + ", \"if or lit " + lab + "\")\n";
            code += "  if (a || b) { r = true } else { r = false }; chk(r == " + t_or + ", \"if or var " + lab + "\")\n";
            code += "  r = 0; while (a && b) { r = true; break }; chk(r == (" + t_and + " ? true : 0), \"while and var " + lab + "\")\n";
            code += "  r = (a || b) ? 1 : 2; chk(r == (" + t_or + " ? 1 : 2), \"ternary or var " + lab + "\")\n";
            code += "}\n";
        }
    }
    code += "return bad\n";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "");
}

INSTANTIATE_TEST_SUITE_P(Mode, OperatorTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
