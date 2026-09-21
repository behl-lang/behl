#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

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

INSTANTIATE_TEST_SUITE_P(Mode, OperatorTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
