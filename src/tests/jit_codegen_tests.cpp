#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>

class JitCodegenTest : public ::testing::TestWithParam<bool>
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

TEST_P(JitCodegenTest, JitFusedCompareJumpColdPathWithFloats)
{
    constexpr std::string_view code = R"(
        function fib(n) {
            if (n < 2) { return n }
            return fib(n - 1) + fib(n - 2)
        }
        function classify(x) {
            if (x >= 10) { return 1 }
            if (x != 3) { return 2 }
            return 3
        }
        let total = 0
        for (let i = 0; i < 30; i++) {
            total = total + fib(12) + classify(i) + classify(i + 0.5)
        }
        return total, fib(12.0), fib(7.5), classify(3), classify(3.0), classify(10.0)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 6));
    ASSERT_EQ(behl::to_integer(S, -6), 30 * 144 + (20 * 1 + 9 * 2 + 1 * 3) + (20 * 1 + 10 * 2));
    ASSERT_DOUBLE_EQ(behl::to_number(S, -5), 144.0);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -4), 23.5);
    ASSERT_EQ(behl::to_integer(S, -3), 3);
    ASSERT_EQ(behl::to_integer(S, -2), 3);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(JitCodegenTest, JitNonSelfCallLeavesUnpassedParamsNil)
{
    constexpr std::string_view code = R"(
        function dirty(a, b, c, d) { return a + b + c + d }
        function probe(x, y, z) {
            if (y == nil && z == nil) { return 1 }
            return 0
        }
        function caller(n) {
            let hits = 0
            for (let i = 0; i < n; i++) {
                dirty(111, 222, 333, 444)
                hits = hits + probe(i)
            }
            return hits
        }
        return caller(200)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 200);
}

TEST_P(JitCodegenTest, JitNonSelfCallMutualRecursionDeep)
{
    constexpr std::string_view code = R"(
        let even = nil
        let odd = nil
        even = function(n) {
            if (n == 0) { return 1 }
            let r = odd(n - 1)
            return r
        }
        odd = function(n) {
            if (n == 0) { return 0 }
            let r = even(n - 1)
            return r
        }
        let total = 0
        for (let i = 0; i < 50; i++) {
            total = total + even(i) + odd(i)
        }
        return total, even(3000), odd(3001)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::to_integer(S, -3), 50);
    ASSERT_EQ(behl::to_integer(S, -2), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(JitCodegenTest, JitNonSelfCallMixedCalleeKinds)
{
    constexpr std::string_view code = R"(
        function add(a, b) { return a + b }
        function sum(...) {
            let args = {...}
            return args[0] + args[1] + args[2]
        }
        function make(k) { return function(x) { return x + k } }
        let plus5 = make(5)
        const math = import("math")
        let total = 0
        for (let i = 0; i < 300; i++) {
            total = total + add(i, 1) + sum(i, 2, 3) + plus5(i) + math.abs(-i)
        }
        return total
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    const int64_t n = 300;
    const int64_t sum_i = n * (n - 1) / 2;
    ASSERT_EQ(behl::to_integer(S, -1), (sum_i + n) + (sum_i + 5 * n) + (sum_i + 5 * n) + sum_i);
}

TEST_P(JitCodegenTest, JitNonSelfCallResultCounts)
{
    constexpr std::string_view code = R"(
        let log = 0
        function two() { return 7, 9 }
        function one(x) { return x * 2 }
        function none(x) { log = log + x }
        function caller(n) {
            let acc = 0
            for (let i = 0; i < n; i++) {
                let a, b = two()
                none(i)
                acc = acc + a + b + one(i)
            }
            return acc
        }
        return caller(100), log
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::to_integer(S, -2), 100 * 16 + 2 * (99 * 100 / 2));
    ASSERT_EQ(behl::to_integer(S, -1), 99 * 100 / 2);
}

INSTANTIATE_TEST_SUITE_P(Mode, JitCodegenTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
