#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include <string>

class CallStackTest : public ::testing::TestWithParam<bool>
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

TEST_P(CallStackTest, ChainedNonTailCalls)
{
    constexpr std::string_view code = R"(
        function f1(n) {
            return n + 1
        }
        function f2(n) {
            return f1(n) + 2
        }
        function f3(n) {
            return f2(n) + 3
        }
        function f4(n) {
            return f3(n) + 4
        }
        return f4(10)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 20);
}

TEST_P(CallStackTest, DeepNonTailCallStack)
{
    constexpr std::string_view code = R"(
        function recurse(n, acc) {
            if (n == 0) {
                return acc
            }
            return recurse(n - 1, acc + n)
        }
        return recurse(10, 0)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 55);
}

TEST_P(CallStackTest, MultipleCallsInExpression)
{
    constexpr std::string_view code = R"(
        function add(a, b) {
            return a + b
        }
        function mul(a, b) {
            return a * b
        }
        return add(mul(3, 4), mul(5, 6))
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(CallStackTest, CallWithMultipleNestedArguments)
{
    constexpr std::string_view code = R"(
        function f(a, b, c) {
            return a + b + c
        }
        function g(n) {
            return n * 2
        }
        return f(g(1), g(2), g(3))
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(CallStackTest, DeepCallStackWithLocals)
{
    constexpr std::string_view code = R"(
        function level5(n) {
            let x = n * 5
            return x
        }
        function level4(n) {
            let x = n * 4
            return level5(x)
        }
        function level3(n) {
            let x = n * 3
            return level4(x)
        }
        function level2(n) {
            let x = n * 2
            return level3(x)
        }
        function level1(n) {
            let x = n + 1
            return level2(x)
        }
        return level1(1)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 240);
}

TEST_P(CallStackTest, CallStackWithUpvalues)
{
    constexpr std::string_view code = R"(
        function makeCounter() {
            let count = 0
            function inner1() {
                count = count + 1
                return count
            }
            function inner2() {
                return inner1() + inner1()
            }
            return inner2
        }
        let counter = makeCounter()
        return counter()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(CallStackTest, RecursiveCallsWithLocalVariables)
{
    constexpr std::string_view code = R"(
        function sum_to_n(n) {
            if (n <= 0) {
                return 0
            }
            let prev = sum_to_n(n - 1)
            let current = n
            return prev + current
        }
        return sum_to_n(5)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(CallStackTest, CallStackWithTableArguments)
{
    constexpr std::string_view code = R"(
        function process(tab) {
            let sum = 0;
            for (let i = 0; i < #tab; i = i + 1) {
                sum = sum + tab[i];
            }
            return sum;
        }
        function wrapper(tab) {
            let result = process(tab);
            return result * 2;
        }
        return wrapper({1, 2, 3, 4});
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 20);
}

TEST_P(CallStackTest, NestedCallsInLoop)
{
    constexpr std::string_view code = R"(
        function process(n) {
            return n * n
        }
        function wrapper(n) {
            return process(n) + 1
        }
        let sum = 0
        for (let i = 1; i <= 3; i = i + 1) {
            sum = sum + wrapper(i)
        }
        return sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 17);
}

TEST_P(CallStackTest, MixedTailAndNonTailCalls)
{
    constexpr std::string_view code = R"(
        function f1(n) {
            if (n == 0) {
                return 100
            }
            return f2(n - 1) + 1
        }
        function f2(n) {
            if (n == 0) {
                return 200
            }
            return f1(n - 1)  // This is a tail call
        }
        return f1(3)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 202);
}

TEST_P(CallStackTest, CallStackWithMultipleReturns)
{
    constexpr std::string_view code = R"(
        function triple(a, b, c) {
            return a, b, c
        }
        function sum_triple(a, b, c) {
            return a + b + c
        }
        function wrapper() {
            let x, y, z = triple(1, 2, 3)
            return sum_triple(x, y, z)
        }
        return wrapper()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(CallStackTest, DeepCallStackStressTest)
{
    constexpr std::string_view code = R"(
        function chain(n, acc) {
            if (n == 0) {
                return acc
            }
            let temp = n * 2
            return chain(n - 1, acc + temp)
        }
        return chain(20, 0)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 420);
}

TEST_P(CallStackTest, ClosureCapturingMoreThan255UpvaluesBindsTheRightVariables)
{
    std::string code = "function outer() {\n";
    for (int i = 0; i < 200; ++i)
    {
        code += "let a" + std::to_string(i) + " = " + std::to_string(i) + "\n";
    }
    code += "function mid() {\n";
    for (int i = 0; i < 200; ++i)
    {
        code += "let b" + std::to_string(i) + " = " + std::to_string(1000 + i) + "\n";
    }
    code += "return function() { return { ";
    for (int i = 0; i < 200; ++i)
    {
        code += "a" + std::to_string(i) + ", ";
    }
    for (int i = 0; i < 200; ++i)
    {
        code += "b" + std::to_string(i) + (i + 1 < 200 ? ", " : " ");
    }
    code += "} }\n}\nreturn mid()\n}\n";
    code += "let t = outer()()\n";
    code += "let bad = 0\n";
    code += "for (let i = 0; i < 200; i = i + 1) {\n";
    code += "if (t[i] != i) { bad = bad + 1 }\n";
    code += "if (t[200 + i] != 1000 + i) { bad = bad + 1 }\n";
    code += "}\n";
    code += "return bad\n";

    if (behl::load_string(S, code) < 0)
    {
        SUCCEED() << "rejected at compile time, which is acceptable";
        return;
    }

    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    EXPECT_EQ(behl::to_integer(S, -1), 0) << "upvalue indices past 255 resolved to the wrong variables";
}

TEST_P(CallStackTest, DeepPlainRecursionUnderPcallSucceedsOrFailsCleanly)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        function rec(k) {
            if (k == 0) { return 0 }
            return 1 + rec(k - 1)
        }
        let ok, res = pcall(rec, 100000)
        let after = rec(10)
        return ok, res, after
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);

    if (behl::to_boolean(S, -3))
    {
        EXPECT_EQ(behl::to_integer(S, -2), 100000);
    }
    else
    {
        EXPECT_EQ(behl::type(S, -2), behl::Type::kString);
    }
    EXPECT_EQ(behl::to_integer(S, -1), 10);

    behl::set_top(S, 0);
    ASSERT_TRUE(behl_test::load_ok(S, "return rec(50)"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 50);
}

INSTANTIATE_TEST_SUITE_P(Mode, CallStackTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
