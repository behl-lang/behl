#include <behl/behl.hpp>
#include <gtest/gtest.h>

class WhileLoopTest : public ::testing::Test
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

TEST_F(WhileLoopTest, BasicWhileLoop)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 1
        while (i <= 5) {
            sum = sum + i
            i = i + 1
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_F(WhileLoopTest, WhileLoopWithFunctionCall)
{
    constexpr std::string_view code = R"(
        function double(n) {
            return n * 2
        }
        let sum = 0
        let i = 1
        while (i <= 3) {
            sum = sum + double(i)
            i = i + 1
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

TEST_F(WhileLoopTest, WhileLoopWithAssignmentFromCall)
{
    constexpr std::string_view code = R"(
        function compute(n) {
            return n * n
        }
        let result = 0
        let i = 1
        while (i <= 4) {
            result = compute(i)
            i = i + 1
        }
        return result
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 16);
}

TEST_F(WhileLoopTest, NestedWhileLoops)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 0
        while (i < 3) {
            let j = 0
            while (j < 2) {
                sum = sum + 1
                j = j + 1
            }
            i = i + 1
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_F(WhileLoopTest, WhileWithComplexCondition)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 1
        let limit = 5
        let iterations = 0
        while (i <= limit) {
            iterations = iterations + 1
            if (sum >= 10) {
                break
            }
            sum = sum + i
            i = i + 1
        }
        return sum, iterations
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);

    ASSERT_EQ(behl::to_integer(S, -2), 10);

    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_F(WhileLoopTest, WhileLoopZeroIterations)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 10
        while (i < 5) {
            sum = sum + i
            i = i + 1
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(WhileLoopTest, WhileLoopWithTableAccess)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let sum = 0
        let i = 0
        while (i < #tab) {
            sum = sum + tab[i]
            i = i + 1
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_F(WhileLoopTest, WhileWithContinue)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 0
        let iterations = 0
        while (i < 10) {
            i = i + 1
            iterations = iterations + 1
            if (i % 2 == 0) {
                continue
            }
            sum = sum + i
        }
        return sum, iterations
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);

    ASSERT_EQ(behl::to_integer(S, -2), 25);

    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_F(WhileLoopTest, WhileWithBreakAndContinue)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 0
        let iterations = 0
        while (i < 20) {
            i = i + 1
            iterations = iterations + 1
            if (i > 10) {
                break
            }
            if (i % 2 == 0) {
                continue
            }
            sum = sum + i
        }
        return sum, iterations
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);

    ASSERT_EQ(behl::to_integer(S, -2), 25);

    ASSERT_EQ(behl::to_integer(S, -1), 11);
}
