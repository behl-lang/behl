#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
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
    ASSERT_TRUE(behl_test::load_ok(S, code));

    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(LoadTest, LoadStringSyntaxError)
{
    constexpr std::string_view bad_code = "let x = ";
    EXPECT_TRUE(behl_test::load_fails(S, bad_code));
    EXPECT_NE(behl_test::error_text(S).find("SyntaxError"), std::string::npos) << behl_test::error_text(S);
}

INSTANTIATE_TEST_SUITE_P(Mode, LoadTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
