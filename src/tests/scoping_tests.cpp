#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class ScopingTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

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

TEST_P(ScopingTest, LocalShadowingInConditionals)
{
    constexpr std::string_view code = R"(
        let i = 10
        if (true) {
            let i = 100
            if (i != 100) {
                return false
            }
        }
        if (true) {
            let i = 1000
            if (i != 1000) {
                return false
            }
        }
        return i == 10
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(ScopingTest, LocalShadowingInBranches)
{
    constexpr std::string_view code = R"(
        let i = 10
        if (i != 10) {
            let i = 20
            return i
        } else {
            let i = 30
            if (i != 30) {
                return false
            }
        }
        return i == 10
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(ScopingTest, LocalInNestedBlocks)
{
    constexpr std::string_view code = R"(
        function f(a) {
            let x = 3
            let b = a
            let c = a
            let d = b
            if (d == b) {
                let x = 99
                if (x != 99) {
                    return false
                }
            }
            return x == 3
        }
        return f(2)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(ScopingTest, SiblingBlockClosuresKeepOwnUpvaluesAtTopLevel)
{
    constexpr std::string_view code = R"(
        let f1 = nil
        let f2 = nil
        {
            let a = 10
            f1 = function() { return a }
        }
        {
            let b = 20
            f2 = function() { return b }
        }
        return tostring(f1()) + "," + tostring(f2())
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "10,20");
}

TEST_P(ScopingTest, SiblingBlockClosuresKeepOwnUpvaluesInFunction)
{
    constexpr std::string_view code = R"(
        function make() {
            let f1 = nil
            let f2 = nil
            {
                let a = 10
                f1 = function() { return a }
            }
            {
                let b = 20
                f2 = function() { return b }
            }
            return tostring(f1()) + "," + tostring(f2())
        }
        return make()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "10,20");
}

TEST_P(ScopingTest, TooManyTopLevelLocalsRaisesErrorOrWorks)
{
    std::string code;
    for (int i = 0; i < 300; ++i)
    {
        code += "let v" + std::to_string(i) + " = " + std::to_string(i) + "\n";
    }
    code += "return v0 + v299\n";
    if (behl::load_string(S, code) < 0 || behl::call(S, 0, 1) < 0)
    {
        SUCCEED();
        return;
    }
    EXPECT_EQ(behl::to_integer(S, -1), 299);
}

INSTANTIATE_TEST_SUITE_P(Mode, ScopingTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
