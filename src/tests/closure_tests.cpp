#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class ClosureTest : public ::testing::TestWithParam<bool>
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

TEST_P(ClosureTest, SimpleClosureCapture)
{
    constexpr std::string_view code = R"(
        let captured = 42
        function getCaptured() {
            return captured
        }
        return getCaptured()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(ClosureTest, ClosureModification)
{
    constexpr std::string_view code = R"(
        let counter = 0
        function increment() {
            counter = counter + 1
            return counter
        }
        let a = increment()
        let b = increment()
        let c = increment()
        return a + b + c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 6); // 1 + 2 + 3
}

TEST_P(ClosureTest, MultipleClosuresSameVariable)
{
    constexpr std::string_view code = R"(
        let shared = 100
        function getter() {
            return shared
        }
        function setter(val) {
            shared = val
        }
        setter(200)
        return getter()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 200);
}

TEST_P(ClosureTest, NestedClosures)
{
    constexpr std::string_view code = R"(
        function outer(x) {
            function middle(y) {
                function inner(z) {
                    return x + y + z
                }
                return inner
            }
            return middle
        }
        let f = outer(10)
        let g = f(20)
        return g(30)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_P(ClosureTest, FunctionAsArgument)
{
    constexpr std::string_view code = R"(
        function apply(f, x) {
            return f(x)
        }
        function double(n) {
            return n * 2
        }
        return apply(double, 21)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(ClosureTest, FunctionReturningFunction)
{
    constexpr std::string_view code = R"(
        function makeAdder(x) {
            function adder(y) {
                return x + y
            }
            return adder
        }
        let add10 = makeAdder(10)
        return add10(32)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(ClosureTest, NestedFunctionCalls)
{
    constexpr std::string_view code = R"(
        function inner(a, b, c) {
            return a * b * c
        }
        function outer(p1, p2, p3, p4, p5, p6) {
            let result = inner(p1, p2, p3) + inner(p4, p5, p6)
            return result
        }
        return outer(2, 3, 4, 5, 6, 7)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 234); // 2*3*4 + 5*6*7 = 24 + 210 = 234
}

TEST_P(ClosureTest, RecursiveFactorial)
{
    constexpr std::string_view code = R"(
        function fact(n) {
            if (n <= 1) {
                return 1
            }
            return n * fact(n - 1)
        }
        return fact(6)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 720);
}

TEST_P(ClosureTest, NestedFunctionWithIncrement)
{
    constexpr std::string_view code = R"(
        function makeCounter() {
            let count = 0
            return function() {
                count++
                return count
            }
        }
        let counter = makeCounter()
        let a = counter()
        let b = counter()
        let c = counter()
        return a + b + c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 6); // 1 + 2 + 3
}

TEST_P(ClosureTest, NestedFunctionWithDecrement)
{
    constexpr std::string_view code = R"(
        function makeCountdown() {
            let count = 10
            return function() {
                count--
                return count
            }
        }
        let countdown = makeCountdown()
        let a = countdown()
        let b = countdown()
        let c = countdown()
        return a + b + c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 24); // 9 + 8 + 7
}

TEST_P(ClosureTest, ForBodyLocalCapturedPerIterationInFunction)
{
    constexpr std::string_view code = R"(
        function f() {
            let fs = {}
            for (let i = 0; i < 3; i++) {
                let y = i * 10
                fs[i] = function() { return y }
            }
            return tostring(fs[0]()) + "," + tostring(fs[1]()) + "," + tostring(fs[2]())
        }
        return f()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "0,10,20");
}

TEST_P(ClosureTest, WhileBodyLocalCapturedPerIterationInFunction)
{
    constexpr std::string_view code = R"(
        function f() {
            let fs = {}
            let i = 0
            while (i < 3) {
                let y = i * 10
                fs[i] = function() { return y }
                i++
            }
            return tostring(fs[0]()) + "," + tostring(fs[1]()) + "," + tostring(fs[2]())
        }
        return f()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "0,10,20");
}

TEST_P(ClosureTest, ForeachBodyLocalCapturedPerIterationInFunction)
{
    constexpr std::string_view code = R"(
        function f() {
            let fs = {}
            foreach (let k, v in {5, 6, 7}) {
                let y = v
                fs[k] = function() { return y }
            }
            return tostring(fs[0]()) + "," + tostring(fs[1]()) + "," + tostring(fs[2]())
        }
        return f()
    )";
    behl::load_stdlib(S);
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "5,6,7");
}

TEST_P(ClosureTest, LoopBodyLocalCapturedWithContinueAndBreakInFunction)
{
    constexpr std::string_view code = R"(
        function f() {
            let fs = {}
            let n = 0
            for (let i = 0; i < 5; i++) {
                let y = i * 10
                if (i == 1) {
                    continue
                }
                fs[n] = function() { return y }
                n++
                if (i == 3) {
                    break
                }
            }
            return tostring(fs[0]()) + "," + tostring(fs[1]()) + "," + tostring(fs[2]()) + "," + tostring(n)
        }
        return f()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "0,20,30,3");
}

TEST_P(ClosureTest, UpvalueClosedWhenFunctionExitsByError)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        let g = nil
        function f() {
            let x = 42
            g = function() { return x }
            error("boom")
        }
        let ok = pcall(f)
        function h(a, b, c, d, e) {
            let z = 99
            return z
        }
        h(1, 2, 3, 4, 5)
        let junk = {1, 2, 3, 4, 5, 6, 7, 8}
        return ok, g()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    ASSERT_TRUE(behl::is_integer(S, -1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

INSTANTIATE_TEST_SUITE_P(Mode, ClosureTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
