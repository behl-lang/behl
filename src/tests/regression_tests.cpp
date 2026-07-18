#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>

class RegressionTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(RegressionTest, RecursiveFibonacciDirectExpression)
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

TEST_F(RegressionTest, MultipleRecursiveCallsInExpression)
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

TEST_F(RegressionTest, LocalVariableNotCorruptedByFunctionDefinition)
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

TEST_F(RegressionTest, MultipleLocalsNotCorruptedByFunctionDefinition)
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

TEST_F(RegressionTest, LocalVariableNotCorruptedByExpressionStatement)
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

TEST_F(RegressionTest, LocalVariableNotCorruptedByFunctionCall)
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

TEST_F(RegressionTest, LocalInOuterScopeNotCorruptedByNestedFunction)
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

TEST_F(RegressionTest, LocalCapturedByClosureNotCorrupted)
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

TEST_F(RegressionTest, ComplexInterleavingOfLocalsAndFunctions)
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

TEST_F(RegressionTest, LocalNotCorruptedByTableCreation)
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

TEST_F(RegressionTest, LocalInConditionalNotCorrupted)
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

TEST_F(RegressionTest, OriginalBugReportCase)
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

TEST_F(RegressionTest, FunctionWithSixParameters)
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

TEST_F(RegressionTest, FunctionWithTenParameters)
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

TEST_F(RegressionTest, FunctionWithFifteenParameters)
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

TEST_F(RegressionTest, RecursiveFunctionWithSixParameters)
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

TEST_F(RegressionTest, RecursiveFunctionWithTenParameters)
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

TEST_F(RegressionTest, TailCallWithSixParameters)
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

TEST_F(RegressionTest, TailCallWithTenParameters)
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

TEST_F(RegressionTest, DeepTailRecursionWithEightParameters)
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

TEST_F(RegressionTest, NestedCallsWithSevenParameters)
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

TEST_F(RegressionTest, ManyParametersWithUpvalues)
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

TEST_F(RegressionTest, RecursionPreservesAllTwelveParameters)
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

TEST_F(RegressionTest, MixedCallsWithEightParameters)
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

TEST_F(RegressionTest, FunctionCallInIfCondition)
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

TEST_F(RegressionTest, FunctionCallInIfConditionReturnsFalse)
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

TEST_F(RegressionTest, MultipleFunctionCallsInIfConditions)
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

TEST_F(RegressionTest, FunctionCallWithParametersInIfCondition)
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

TEST_F(RegressionTest, FunctionCallInWhileCondition)
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

TEST_F(RegressionTest, ErrorColumnNumberForCallToNilValue)
{
    constexpr std::string_view code = R"(
        let undefined_func = nil;
        undefined_func(123, 456);
    )";

    ASSERT_NO_THROW(behl::load_string(S, code)) << "Code should compile successfully";
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError) << "Call should fail because undefined_func is nil";
}
