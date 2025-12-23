#include <behl/behl.hpp>
#include <gtest/gtest.h>

class ControflowTest : public ::testing::Test
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

TEST_F(ControflowTest, ExecuteIfStatement)
{
    constexpr std::string_view code = R"(
        if (true) {
            return 123
        } else {
            return 456
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 123);
}

TEST_F(ControflowTest, ExecuteCStyleForLoop)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 5; i = i + 1) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_F(ControflowTest, CStyleForLoopWithTable)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30, 40}
        let sum = 0
        for (let i = 0; i < #tab; i = i + 1) {
            sum = sum + tab[i]
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
}

TEST_F(ControflowTest, CStyleForLoopNested)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 3; i = i + 1) {
            for (let j = 0; j < 2; j = j + 1) {
                sum = sum + 1
            }
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}
