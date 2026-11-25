#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class ControflowTest : public ::testing::TestWithParam<bool>
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

TEST_P(ControflowTest, ExecuteIfStatement)
{
    constexpr std::string_view code = R"(
        if (true) {
            return 123
        } else {
            return 456
        }
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 123);
}

TEST_P(ControflowTest, ExecuteCStyleForLoop)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 5; i = i + 1) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(ControflowTest, CStyleForLoopWithTable)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30, 40}
        let sum = 0
        for (let i = 0; i < #tab; i = i + 1) {
            sum = sum + tab[i]
        }
        return sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
}

TEST_P(ControflowTest, CStyleForLoopNested)
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
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(ControflowTest, IfConditionOnFieldAccess)
{
    constexpr std::string_view code = R"(
        let t = {f = 1}
        let r = 0
        if (t.f) {
            r = 1
        }
        return r
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(ControflowTest, IfConditionOnFieldAccessInFunction)
{
    constexpr std::string_view code = R"(
        function f(t) {
            if (t.f) {
                return 1
            }
            return 2
        }
        return f({f = 1}) * 10 + f({})
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(ControflowTest, WhileConditionOnFieldAccess)
{
    constexpr std::string_view code = R"(
        let t = {f = 1}
        let n = 0
        while (t.f) {
            n++
            t.f = nil
        }
        return n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(ControflowTest, IfNilConstantPreservesLocals)
{
    constexpr std::string_view code = R"(
        let a = 7
        let b = 8
        if (nil) {}
        return a * 10 + b
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 78);
}

TEST_P(ControflowTest, IfNilConstantPreservesLocalsInFunction)
{
    constexpr std::string_view code = R"(
        function g() {
            let a = 7
            let b = 8
            if (nil) {}
            return a * 10 + b
        }
        return g()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 78);
}

TEST_P(ControflowTest, WhileNilConstantPreservesLocals)
{
    constexpr std::string_view code = R"(
        function g() {
            let a = 7
            let b = 8
            while (nil) {}
            return a * 10 + b
        }
        let c = 7
        let d = 8
        while (nil) {}
        return g() * 100 + c * 10 + d
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 7878);
}

TEST_P(ControflowTest, IfFalseConstantPreservesLocals)
{
    constexpr std::string_view code = R"(
        function g() {
            let a = 7
            let b = 8
            if (false) {}
            return a * 10 + b
        }
        let c = 7
        let d = 8
        if (false) {}
        return g() * 100 + c * 10 + d
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 7878);
}

INSTANTIATE_TEST_SUITE_P(Mode, ControflowTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
