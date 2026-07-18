#include <behl/behl.hpp>
#include <gtest/gtest.h>

class LogicalOperatorTest : public ::testing::Test
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(LogicalOperatorTest, AndTrueTrue)
{
    constexpr std::string_view code = R"(
        return true && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndTrueFalse)
{
    constexpr std::string_view code = R"(
        return true && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndFalseTrue)
{
    constexpr std::string_view code = R"(
        return false && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndFalseFalse)
{
    constexpr std::string_view code = R"(
        return false && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, OrTrueTrue)
{
    constexpr std::string_view code = R"(
        return true || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, OrTrueFalse)
{
    constexpr std::string_view code = R"(
        return true || false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, OrFalseTrue)
{
    constexpr std::string_view code = R"(
        return false || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, OrFalseFalse)
{
    constexpr std::string_view code = R"(
        return false || false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndWithNil)
{
    constexpr std::string_view code = R"(
        return true && nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, OrWithNil)
{
    constexpr std::string_view code = R"(
        return nil || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndWithNumbers)
{
    constexpr std::string_view code = R"(
        return 5 && 10
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, OrWithNumbers)
{
    constexpr std::string_view code = R"(
        return 0 || 1
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndWithZero)
{
    constexpr std::string_view code = R"(
        return 0 && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndShortCircuitLeft)
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

TEST_F(LogicalOperatorTest, AndNoShortCircuit)
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

TEST_F(LogicalOperatorTest, OrShortCircuitLeft)
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

TEST_F(LogicalOperatorTest, OrNoShortCircuit)
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

TEST_F(LogicalOperatorTest, NilShortCircuitAnd)
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

TEST_F(LogicalOperatorTest, ChainedAnd)
{
    constexpr std::string_view code = R"(
        return true && true && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, ChainedAndWithFalse)
{
    constexpr std::string_view code = R"(
        return true && false && true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, ChainedOr)
{
    constexpr std::string_view code = R"(
        return false || false || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, ChainedOrAllFalse)
{
    constexpr std::string_view code = R"(
        return false || false || false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, MixedAndOr)
{
    constexpr std::string_view code = R"(
        return true || false && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, MixedOrAnd)
{
    constexpr std::string_view code = R"(
        return false && true || true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AndInIfCondition)
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

TEST_F(LogicalOperatorTest, OrInIfCondition)
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

TEST_F(LogicalOperatorTest, ComplexCondition)
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

TEST_F(LogicalOperatorTest, AndInWhileLoop)
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

TEST_F(LogicalOperatorTest, NotAndCombination)
{
    constexpr std::string_view code = R"(
        return !(true && false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, NotOrCombination)
{
    constexpr std::string_view code = R"(
        return !(false || false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, DeMorgansLaw1)
{
    constexpr std::string_view code = R"(
        return !(true && false) == (!true || !false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, DeMorgansLaw2)
{
    constexpr std::string_view code = R"(
        return !(true || false) == (!true && !false)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, AssignAndResult)
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

TEST_F(LogicalOperatorTest, AssignOrResult)
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

TEST_F(LogicalOperatorTest, AndReturnsLastValue)
{
    constexpr std::string_view code = R"(
        return 5 && 10
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_F(LogicalOperatorTest, AndReturnsFirstFalsy)
{
    constexpr std::string_view code = R"(
        return 5 && false
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_F(LogicalOperatorTest, OrReturnsFirstTruthy)
{
    constexpr std::string_view code = R"(
        return false || 5
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_F(LogicalOperatorTest, OrReturnsLastValue)
{
    constexpr std::string_view code = R"(
        return false || nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}
