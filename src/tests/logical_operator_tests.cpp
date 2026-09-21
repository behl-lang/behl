#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class LogicalOperatorTest : public ::testing::TestWithParam<bool>
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

TEST_P(LogicalOperatorTest, AndTrueTrue)
{
    constexpr std::string_view code = R"(
        return true && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndTrueFalse)
{
    constexpr std::string_view code = R"(
        return true && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndFalseTrue)
{
    constexpr std::string_view code = R"(
        return false && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndFalseFalse)
{
    constexpr std::string_view code = R"(
        return false && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, OrTrueTrue)
{
    constexpr std::string_view code = R"(
        return true || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, OrTrueFalse)
{
    constexpr std::string_view code = R"(
        return true || false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, OrFalseTrue)
{
    constexpr std::string_view code = R"(
        return false || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, OrFalseFalse)
{
    constexpr std::string_view code = R"(
        return false || false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndWithNil)
{
    constexpr std::string_view code = R"(
        return true && nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, OrWithNil)
{
    constexpr std::string_view code = R"(
        return nil || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndWithNumbers)
{
    constexpr std::string_view code = R"(
        return 5 && 10
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, OrWithNumbers)
{
    constexpr std::string_view code = R"(
        return 0 || 1
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndWithZero)
{
    constexpr std::string_view code = R"(
        return 0 && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndShortCircuitLeft)
{
    constexpr std::string_view code = R"(
        let x = 0;
        function inc() {
            x = x + 1;
            return true;
        }
        let result = false && inc();
        return x;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(LogicalOperatorTest, AndNoShortCircuit)
{
    constexpr std::string_view code = R"(
        let x = 0;
        function inc() {
            x = x + 1;
            return true;
        }
        let result = true && inc();
        return x;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(LogicalOperatorTest, OrShortCircuitLeft)
{
    constexpr std::string_view code = R"(
        let x = 0;
        function inc() {
            x = x + 1;
            return true;
        }
        let result = true || inc();
        return x;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(LogicalOperatorTest, OrNoShortCircuit)
{
    constexpr std::string_view code = R"(
        let x = 0;
        function inc() {
            x = x + 1;
            return true;
        }
        let result = false || inc();
        return x;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(LogicalOperatorTest, NilShortCircuitAnd)
{
    constexpr std::string_view code = R"(
        let x = 0;
        function inc() {
            x = x + 1;
            return true;
        }
        let result = nil && inc();
        return x;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(LogicalOperatorTest, ChainedAnd)
{
    constexpr std::string_view code = R"(
        return true && true && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, ChainedAndWithFalse)
{
    constexpr std::string_view code = R"(
        return true && false && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, ChainedOr)
{
    constexpr std::string_view code = R"(
        return false || false || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, ChainedOrAllFalse)
{
    constexpr std::string_view code = R"(
        return false || false || false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, MixedAndOr)
{
    constexpr std::string_view code = R"(
        return true || false && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, MixedOrAnd)
{
    constexpr std::string_view code = R"(
        return false && true || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndInIfCondition)
{
    constexpr std::string_view code = R"(
        let x = 5;
        let y = 10;
        if (x > 0 && y > 5) {
            return 1;
        }
        return 0;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(LogicalOperatorTest, OrInIfCondition)
{
    constexpr std::string_view code = R"(
        let x = 5;
        let y = 3;
        if (x < 0 || y > 2) {
            return 1;
        }
        return 0;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(LogicalOperatorTest, ComplexCondition)
{
    constexpr std::string_view code = R"(
        let a = 10;
        let b = 20;
        let c = 30;
        if ((a < b && b < c) || (a > 50)) {
            return 1;
        }
        return 0;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(LogicalOperatorTest, AndInWhileLoop)
{
    constexpr std::string_view code = R"(
        let i = 0;
        let sum = 0;
        while (i < 10 && sum < 20) {
            sum = sum + i;
            i = i + 1;
        }
        return i;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 7);
}

TEST_P(LogicalOperatorTest, NotAndCombination)
{
    constexpr std::string_view code = R"(
        return !(true && false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, NotOrCombination)
{
    constexpr std::string_view code = R"(
        return !(false || false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, DeMorgansLaw1)
{
    constexpr std::string_view code = R"(
        return !(true && false) == (!true || !false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, DeMorgansLaw2)
{
    constexpr std::string_view code = R"(
        return !(true || false) == (!true && !false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AssignAndResult)
{
    constexpr std::string_view code = R"(
        let x = true && false;
        return x;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AssignOrResult)
{
    constexpr std::string_view code = R"(
        let x = false || true;
        return x;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, AndReturnsLastValue)
{
    constexpr std::string_view code = R"(
        return 5 && 10
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(LogicalOperatorTest, AndReturnsFirstFalsy)
{
    constexpr std::string_view code = R"(
        return 5 && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(LogicalOperatorTest, OrReturnsFirstTruthy)
{
    constexpr std::string_view code = R"(
        return false || 5
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(LogicalOperatorTest, OrReturnsLastValue)
{
    constexpr std::string_view code = R"(
        return false || nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

INSTANTIATE_TEST_SUITE_P(Mode, LogicalOperatorTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
