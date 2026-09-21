#include "state.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>

class LoadTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        ASSERT_NE(S, nullptr);
        behl::set_top(S, 0);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_P(LoadTest, LoadStringSuccess)
{
    constexpr std::string_view code = "x = 42";
    ASSERT_NO_THROW(behl::load_string(S, code));

    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LoadTest, LoadStringSyntaxError)
{
    const char* bad_code = "let x = ";
    EXPECT_THROW({ behl::load_string(S, bad_code); }, behl::SyntaxError);
}

INSTANTIATE_TEST_SUITE_P(
    Mode, LoadTest, ::testing::Bool(), [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
