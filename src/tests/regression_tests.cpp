#include "state.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>
#include <string>

class RegressionTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_P(RegressionTest, RecursiveFibonacciDirectExpression)
{
    constexpr std::string_view code = R"(
        function fib(n) { 
            if (n < 2) {
                return n
            }
            return fib(n - 1) + fib(n - 2)
        }
        return fib(10)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 55);
}

TEST_P(RegressionTest, MultipleRecursiveCallsInExpression)
{
    constexpr std::string_view code = R"(
        function test(n) {
            if (n <= 0) {
                return 1
            }
            return test(n - 1) + test(n - 1) + test(n - 1)
        }
        return test(3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 27);
}

TEST_P(RegressionTest, JitReturnFastDeepRecursionValues)
{
    constexpr std::string_view code = R"(
        function depth(n) {
            if (n <= 0) { return 7 }
            return depth(n - 1) + 1
        }
        let acc = 0
        let i = 0
        while (i < 300) {
            acc = acc + depth(40)
            i = i + 1
        }
        return acc, depth(40), depth(0), depth(1)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::to_integer(S, -4), 300 * 47);
    ASSERT_EQ(behl::to_integer(S, -3), 47);
    ASSERT_EQ(behl::to_integer(S, -2), 7);
    ASSERT_EQ(behl::to_integer(S, -1), 8);
}

TEST_P(RegressionTest, JitReturnFastDistinctValuesPerFrame)
{
    constexpr std::string_view code = R"(
        function leaf(n) { return n * 3 }
        function mid(n) { return leaf(n) + leaf(n + 1) }
        function outer(n) { return mid(n) + mid(n + 2) }
        let acc = 0
        let i = 0
        while (i < 400) {
            acc = outer(i)
            i = i + 1
        }
        return acc, outer(0), outer(1), mid(5), leaf(9)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 5));
    ASSERT_EQ(behl::to_integer(S, -5), 3 * (399 + 400 + 401 + 402));
    ASSERT_EQ(behl::to_integer(S, -4), 3 * (0 + 1 + 2 + 3));
    ASSERT_EQ(behl::to_integer(S, -3), 3 * (1 + 2 + 3 + 4));
    ASSERT_EQ(behl::to_integer(S, -2), 3 * (5 + 6));
    ASSERT_EQ(behl::to_integer(S, -1), 27);
}

TEST_P(RegressionTest, JitReturnFastWithUpvaluesStillCorrect)
{
    constexpr std::string_view code = R"(
        function make(base) {
            function inner(n) {
                if (n <= 0) { return base }
                return inner(n - 1) + 1
            }
            return inner
        }
        let f = make(100)
        let acc = 0
        let i = 0
        while (i < 300) {
            acc = f(10)
            i = i + 1
        }
        return acc, f(0), f(5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::to_integer(S, -3), 110);
    ASSERT_EQ(behl::to_integer(S, -2), 100);
    ASSERT_EQ(behl::to_integer(S, -1), 105);
}

TEST_P(RegressionTest, JitReturnFastMixedResultCounts)
{
    constexpr std::string_view code = R"(
        function one(n) { return n + 1 }
        function two(n) { return n + 1, n + 2 }
        function none(n) { one(n) }
        let acc = 0
        let i = 0
        while (i < 400) {
            acc = one(i)
            none(i)
            let p, q = two(i)
            acc = acc + p + q
            i = i + 1
        }
        let a, b = two(10)
        return acc, one(5), a, b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::to_integer(S, -4), 400 + (400 + 401));
    ASSERT_EQ(behl::to_integer(S, -3), 6);
    ASSERT_EQ(behl::to_integer(S, -2), 11);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(RegressionTest, MultretTableConstructorFromHotCalls)
{
    constexpr std::string_view code = R"(
        function one(n) { return n + 1 }
        function none(n) { }
        let len = 0
        let last = 0
        let i = 0
        while (i < 400) {
            let t = {one(i), one(i + 1)}
            len = #t
            last = t[1]
            none(i)
            i = i + 1
        }
        let t2 = {one(7), one(8)}
        return len, last, #t2, t2[0], t2[1]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 5));
    ASSERT_EQ(behl::to_integer(S, -5), 2);
    ASSERT_EQ(behl::to_integer(S, -4), 401);
    ASSERT_EQ(behl::to_integer(S, -3), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 8);
    ASSERT_EQ(behl::to_integer(S, -1), 9);
}

TEST_P(RegressionTest, JitTailCallMultArgsSelfRecursion)
{
    constexpr std::string_view code = R"(
        function ack(m, n) {
            if (m == 0) {
                return n + 1
            } elseif (n == 0) {
                return ack(m - 1, 1)
            } else {
                return ack(m - 1, ack(m, n - 1))
            }
        }
        let warm = 0
        let i = 0
        while (i < 40) {
            warm = ack(2, 3)
            i = i + 1
        }
        return warm, ack(0, 0), ack(1, 1), ack(2, 2), ack(3, 3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 5));
    ASSERT_EQ(behl::to_integer(S, -5), 9);
    ASSERT_EQ(behl::to_integer(S, -4), 1);
    ASSERT_EQ(behl::to_integer(S, -3), 3);
    ASSERT_EQ(behl::to_integer(S, -2), 7);
    ASSERT_EQ(behl::to_integer(S, -1), 61);
}

TEST_P(RegressionTest, JitNonSelfTailCallFromNestedSelfRecursion)
{
    constexpr std::string_view code = R"(
        function pick(a, b, c) {
            if (a <= 0) { return b + c }
            return pick(a - 1, c, b + 1)
        }
        function feed(n) {
            if (n <= 0) { return 100 }
            let t = feed(n - 1)
            return pick(2, n, t)
        }
        function feedm(n) {
            if (n <= 0) { return 100 }
            return pick(2, n, feedm(n - 1))
        }
        let acc = 0
        let accm = 0
        let i = 0
        while (i < 200) {
            acc = feed(6)
            accm = feedm(6)
            i = i + 1
        }
        return acc, accm, feed(1), feedm(1), feed(0)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 5));
    ASSERT_EQ(behl::to_integer(S, -5), 133);
    ASSERT_EQ(behl::to_integer(S, -4), 133);
    ASSERT_EQ(behl::to_integer(S, -3), 103);
    ASSERT_EQ(behl::to_integer(S, -2), 103);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
}

TEST_P(RegressionTest, JitReturnFastThroughPcall)
{
    constexpr std::string_view code = R"(
        function leaf(n) {
            if (n == 13) { error("boom") }
            return n + 1
        }
        function mid(n) { return leaf(n) + 1 }
        let acc = 0
        let i = 0
        while (i < 300) {
            acc = mid(1)
            i = i + 1
        }
        let ok, val = pcall(mid, 5)
        let bad, err = pcall(mid, 13)
        return acc, ok, val, bad
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::to_integer(S, -4), 3);
    ASSERT_TRUE(behl::to_boolean(S, -3));
    ASSERT_EQ(behl::to_integer(S, -2), 7);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(RegressionTest, LocalVariableNotCorruptedByFunctionDefinition)
{
    constexpr std::string_view code = R"(
        let x = 10
        function f() {
            return 42
        }
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 10) << "x should remain 10, not be corrupted by function";
}

TEST_P(RegressionTest, MultipleLocalsNotCorruptedByFunctionDefinition)
{
    constexpr std::string_view code = R"(
        let x = 10
        let y = 20
        let z = 30
        function dummy() {
            return 999
        }
        return x + y + z
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 60) << "All locals should remain uncorrupted";
}

TEST_P(RegressionTest, LocalVariableNotCorruptedByExpressionStatement)
{
    constexpr std::string_view code = R"(
        let x = 100
        let temp = 1 + 1  // Another statement that allocates registers
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
}

TEST_P(RegressionTest, LocalVariableNotCorruptedByFunctionCall)
{
    constexpr std::string_view code = R"(
        function helper() {
            return 777
        }
        let x = 50
        helper()  // Call should not corrupt x
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 50);
}

TEST_P(RegressionTest, LocalInOuterScopeNotCorruptedByNestedFunction)
{
    constexpr std::string_view code = R"(
        let outer = 123
        function dummy() {
            function inner() {
                return 456
            }
        }
        return outer
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 123);
}

TEST_P(RegressionTest, LocalCapturedByClosureNotCorrupted)
{
    constexpr std::string_view code = R"(
        let captured = 999
        function getCaptured() {
            return captured
        }
        let dummy = 111  // Another local to test register allocation
        return getCaptured()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 999);
}

TEST_P(RegressionTest, ComplexInterleavingOfLocalsAndFunctions)
{
    constexpr std::string_view code = R"(
        let a = 1
        function f1() { return 100 }
        let b = 2
        function f2() { return 200 }
        let c = 3
        function f3() { return 300 }
        return a + b + c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6) << "All locals should preserve their values";
}

TEST_P(RegressionTest, LocalNotCorruptedByTableCreation)
{
    constexpr std::string_view code = R"(
        let x = 42
        let t = {a = 1, b = 2}
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(RegressionTest, LocalInConditionalNotCorrupted)
{
    constexpr std::string_view code = R"(
        let x = 5
        if (true) {
            function dummy() { return 0 }
        }
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(RegressionTest, OriginalBugReportCase)
{
    constexpr std::string_view code = R"(
        let x = 10
        function test(a, b) {
            return a + b + x
        }
        return test(5, 3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 18) << "Should compute 5 + 3 + 10 = 18";
}

TEST_P(RegressionTest, FunctionWithSixParameters)
{
    constexpr std::string_view code = R"(function sum6(a, b, c, d, e, f) {
            return a + b + c + d + e + f
        }
        return sum6(1, 2, 3, 4, 5, 6)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 21);
}

TEST_P(RegressionTest, FunctionWithTenParameters)
{
    constexpr std::string_view code = R"(
        function sum10(a, b, c, d, e, f, g, h, i, j) {
            return a + b + c + d + e + f + g + h + i + j
        }
        return sum10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 55);
}

TEST_P(RegressionTest, FunctionWithFifteenParameters)
{
    constexpr std::string_view code = R"(
        function sum15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) {
            return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o
        }
        return sum15(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 120);
}

TEST_P(RegressionTest, RecursiveFunctionWithSixParameters)
{
    constexpr std::string_view code = R"(
        function rec_sum(a, b, c, d, e, depth) {
            if (depth <= 0) {
                return a + b + c + d + e
            }
            return rec_sum(a, b, c, d, e, depth - 1) + 1
        }
        return rec_sum(10, 20, 30, 40, 50, 5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 155);
}

TEST_P(RegressionTest, RecursiveFunctionWithTenParameters)
{
    constexpr std::string_view code = R"(
        function rec_product(a, b, c, d, e, f, g, h, i, depth) {
            if (depth <= 0) {
                return a * b + c * d + e * f + g * h + i
            }
            return rec_product(a, b, c, d, e, f, g, h, i, depth - 1)
        }
        return rec_product(2, 3, 4, 5, 6, 7, 8, 9, 10, 3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 150);
}

TEST_P(RegressionTest, TailCallWithSixParameters)
{
    constexpr std::string_view code = R"(
        function countdown(a, b, c, d, e, n) {
            if (n <= 0) {
                return a + b + c + d + e
            }
            return countdown(a + 1, b + 1, c + 1, d + 1, e + 1, n - 1)
        }
        return countdown(1, 2, 3, 4, 5, 10)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 65);
}

TEST_P(RegressionTest, TailCallWithTenParameters)
{
    constexpr std::string_view code = R"(
        function accumulate(a, b, c, d, e, f, g, h, i, n) {
            if (n <= 0) {
                return a + b + c + d + e + f + g + h + i
            }
            return accumulate(a + 1, b + 1, c + 1, d + 1, e + 1, f + 1, g + 1, h + 1, i + 1, n - 1)
        }
        return accumulate(1, 2, 3, 4, 5, 6, 7, 8, 9, 100)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 945);
}

TEST_P(RegressionTest, DeepTailRecursionWithEightParameters)
{
    constexpr std::string_view code = R"(
        function deep_tail(a, b, c, d, e, f, g, depth) {
            if (depth <= 0) {
                return a + b + c + d + e + f + g
            }
            return deep_tail(a, b, c, d, e, f, g, depth - 1)
        }
        return deep_tail(1, 2, 3, 4, 5, 6, 7, 10000)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 28);
}

TEST_P(RegressionTest, NestedCallsWithSevenParameters)
{
    constexpr std::string_view code = R"(
        function inner(a, b, c) {
            return a * b * c
        }
        function outer(p1, p2, p3, p4, p5, p6, p7) {
            let result = inner(p1, p2, p3) + inner(p4, p5, p6)
            return result + p7
        }
        return outer(2, 3, 4, 5, 6, 7, 100)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 334);
}

TEST_P(RegressionTest, ManyParametersWithUpvalues)
{
    constexpr std::string_view code = R"(
        function make_adder(base1, base2, base3, base4, base5) {
            function adder(a, b, c, d, e) {
                return a + b + c + d + e + base1 + base2 + base3 + base4 + base5
            }
            return adder
        }
        let add_fn = make_adder(10, 20, 30, 40, 50)
        return add_fn(1, 2, 3, 4, 5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 165);
}

TEST_P(RegressionTest, RecursionPreservesAllTwelveParameters)
{
    constexpr std::string_view code = R"(
        function check_params(a, b, c, d, e, f, g, h, i, j, k, depth) {
            if (depth <= 0) {
                if (a != 1) { return false }
                if (b != 2) { return false }
                if (c != 3) { return false }
                if (d != 4) { return false }
                if (e != 5) { return false }
                if (f != 6) { return false }
                if (g != 7) { return false }
                if (h != 8) { return false }
                if (i != 9) { return false }
                if (j != 10) { return false }
                if (k != 11) { return false }
                return true
            }
            return check_params(a, b, c, d, e, f, g, h, i, j, k, depth - 1)
        }
        return check_params(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1)) << "All parameters should be preserved through recursion";
}

TEST_P(RegressionTest, MixedCallsWithEightParameters)
{
    constexpr std::string_view code = R"(
        function helper(a, b, c) {
            return a + b + c
        }
        function mixed_recurse(p1, p2, p3, p4, p5, p6, p7, depth) {
            if (depth <= 0) {
                return p1 + p2 + p3 + p4 + p5 + p6 + p7
            }
            let temp = helper(p1, p2, p3)
            if (depth > 5) {
                return mixed_recurse(temp, p2, p3, p4, p5, p6, p7, depth - 1) + 1
            }
            return mixed_recurse(temp, p2, p3, p4, p5, p6, p7, depth - 1)
        }
        return mixed_recurse(1, 2, 3, 4, 5, 6, 7, 10)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_integer(S, -1) > 0) << "Should compute without corruption";
}

TEST_P(RegressionTest, FunctionCallInIfCondition)
{
    constexpr std::string_view code = R"(
        function returns_true() {
            return true
        }
        if (returns_true()) {
            return 42
        }
        return 0
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42) << "Should execute then-block";
}

TEST_P(RegressionTest, FunctionCallInIfConditionReturnsFalse)
{
    constexpr std::string_view code = R"(
        function returns_false() {
            return false
        }
        if (returns_false()) {
            return 1
        }
        return 99
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 99) << "Should skip then-block";
}

TEST_P(RegressionTest, MultipleFunctionCallsInIfConditions)
{
    constexpr std::string_view code = R"(
        function test1() { return true }
        function test2() { return false }
        function test3() { return true }
        
        let result = 0
        if (test1()) {
            result = result + 1
        }
        if (test2()) {
            result = result + 10
        }
        if (test3()) {
            result = result + 100
        }
        return result
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 101) << "Should execute test1 and test3 blocks only";
}

TEST_P(RegressionTest, FunctionCallWithParametersInIfCondition)
{
    constexpr std::string_view code = R"(
        function check(value) {
            return value > 10
        }
        if (check(15)) {
            return 123
        }
        return 456
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 123);
}

TEST_P(RegressionTest, FunctionCallInWhileCondition)
{
    constexpr std::string_view code = R"(
        let counter = 0
        function should_continue() {
            counter = counter + 1
            return counter < 5
        }
        while (should_continue()) {
        }
        return counter
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5) << "Should loop until counter reaches 5";
}

TEST_P(RegressionTest, ErrorColumnNumberForCallToNilValue)
{
    constexpr std::string_view code = R"(
        let undefined_func = nil;
        undefined_func(123, 456);
    )";

    ASSERT_NO_THROW(behl::load_string(S, code)) << "Code should compile successfully";
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError) << "Call should fail because undefined_func is nil";
}

TEST_P(RegressionTest, ManySequentialStatementsDoNotOverflowRegisters)
{
    std::string code = "function generated(seed, k) {\n    let acc = seed;\n";
    for (int i = 0; i < 512; i++)
    {
        code += "    acc = acc + k * 3 - 1;\n";
    }
    code += "    return acc;\n}\nreturn generated(1, 2);\n";

    ASSERT_NO_THROW(behl::load_string(S, code)) << "Repeating one statement shape must not exhaust the register file";
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 2561);
}

TEST_P(RegressionTest, SelfTailCallPreservesArgumentCount)
{
    constexpr std::string_view code = R"(
        function countdown(n, acc) {
            if (n <= 0) { return acc; }
            return countdown(n - 1, acc + n);
        }
        return countdown(150, 0);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 11325);
}

TEST_P(RegressionTest, SelfTailCallArityOneTwoThree)
{
    constexpr std::string_view code = R"(
        function one(n) {
            if (n <= 0) { return 7; }
            return one(n - 1);
        }
        function two(n, acc) {
            if (n <= 0) { return acc; }
            return two(n - 1, acc + 2);
        }
        function three(n, acc, step) {
            if (n <= 0) { return acc; }
            return three(n - 1, acc + step, step);
        }
        return one(60) + two(60, 0) + three(60, 0, 3);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 7 + 120 + 180);
}

TEST_P(RegressionTest, TwoDistinctCallsInOneLoopBody)
{
    constexpr std::string_view code = R"(
        let ping = nil;
        let pong = nil;

        ping = function(n, acc) {
            if (n <= 0) { return acc; }
            return pong(n - 1, acc + n);
        };

        pong = function(n, acc) {
            if (n <= 0) { return acc; }
            return ping(n - 1, acc + n);
        };

        function selfdown(n, acc) {
            if (n <= 0) { return acc; }
            return selfdown(n - 1, acc + n);
        }

        for (let i = 0; i < 200; i++) {
            ping(150, 0);
            selfdown(150, 0);
        }
        return ping(150, 0) + selfdown(150, 0);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 11325 + 11325);
}

TEST_P(RegressionTest, NonTailSelfCallPreservesArgumentCount)
{
    constexpr std::string_view code = R"(
        function sum_to(n) {
            if (n <= 0) { return 0; }
            return n + sum_to(n - 1);
        }
        function weighted(n, w) {
            if (n <= 0) { return 0; }
            return n * w + weighted(n - 1, w);
        }
        return sum_to(100) + weighted(100, 2);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 5050 + 10100);
}

TEST_P(RegressionTest, TopLevelLoopMatchesFunctionLoop)
{
    constexpr std::string_view code = R"(
        function loop_in_function(n) {
            let sum = 0;
            for (let i = 0; i < n; i++) {
                sum = sum + i;
            }
            return sum;
        }

        let from_function = loop_in_function(50000);

        let from_chunk = 0;
        for (let i = 0; i < 50000; i++) {
            from_chunk = from_chunk + i;
        }

        return (from_function == from_chunk) && (from_chunk == 1249975000);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(RegressionTest, SelfCallLeavesUnpassedParamsNil)
{
    constexpr std::string_view code = R"(
        function dirty(p, q, r, s) {
            return p + q + r + s;
        }

        function probe(n, a, b) {
            if (n <= 0) {
                if ((b == nil) && (a == 7)) { return 1; }
                return 0;
            }
            dirty(111, 222, 333, 444);
            let inner = probe(n - 1, 7);
            return inner;
        }

        for (let i = 0; i < 50; i++) {
            probe(20, 7);
        }
        return probe(20, 7) == 1;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1)) << "unpassed parameters must read nil after a self call";
}

INSTANTIATE_TEST_SUITE_P(Mode, RegressionTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
