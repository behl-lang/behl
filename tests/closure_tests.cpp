#include <behl/behl.hpp>
#include <gtest/gtest.h>

class ClosureTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(ClosureTest, SimpleClosureCapture)
{
    constexpr std::string_view code = R"(
        let captured = 42
        function getCaptured() {
            return captured
        }
        return getCaptured()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(ClosureTest, ClosureModification)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 6); // 1 + 2 + 3
}

TEST_F(ClosureTest, MultipleClosuresSameVariable)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 200);
}

TEST_F(ClosureTest, NestedClosures)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_F(ClosureTest, FunctionAsArgument)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(ClosureTest, FunctionReturningFunction)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(ClosureTest, NestedFunctionCalls)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 234); // 2*3*4 + 5*6*7 = 24 + 210 = 234
}

TEST_F(ClosureTest, RecursiveFactorial)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 720);
}

TEST_F(ClosureTest, NestedFunctionWithIncrement)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 6); // 1 + 2 + 3
}

TEST_F(ClosureTest, NestedFunctionWithDecrement)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 24); // 9 + 8 + 7
}
