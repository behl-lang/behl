#include <behl/behl.hpp>
#include <gtest/gtest.h>

class EdgeCaseTest : public ::testing::Test
{
protected:
    behl::State* S;
    void SetUp() override
    {
        S = behl::new_state();
    }
    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(EdgeCaseTest, EmptyFunctionCall)
{
    constexpr std::string_view code = R"(
        function noArgs() {
            return 42
        }
        return noArgs()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(EdgeCaseTest, FunctionReturningFunction)
{
    constexpr std::string_view code = R"(
        function outer() {
            function inner() {
                return 99
            }
            return inner
        }
        let f = outer()
        return f()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 99);
}

TEST_F(EdgeCaseTest, ImmediatelyInvokedFunction)
{
    constexpr std::string_view code = R"(
        let result = (function(x) { return x * 2 })(21)
        return result
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(EdgeCaseTest, ChainedPropertyAccess)
{
    constexpr std::string_view code = R"(
        let t1 = {inner = {value = 123}}
        return t1.inner.value
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 123);
}

TEST_F(EdgeCaseTest, FunctionCallInTableConstructor)
{
    constexpr std::string_view code = R"(
        function getValue() {
            return 42
        }
        let t = {x = getValue(), y = 10}
        return t.x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(EdgeCaseTest, NestedTableAccess)
{
    constexpr std::string_view code = R"(
        let matrix = {{1, 2}, {3, 4}, {5, 6}}
        return matrix[1][1]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 4);
}

TEST_F(EdgeCaseTest, MultipleAssignmentsInLoop)
{
    constexpr std::string_view code = R"(
        function getValue(n) {
            return n * 3
        }
        let a = 0
        let b = 0
        for (let i = 1; i <= 2; i = i + 1) {
            a = getValue(i)
            b = a + 1
        }
        return b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 7);
}

TEST_F(EdgeCaseTest, FunctionAsTableValue)
{
    constexpr std::string_view code = R"(
        function add(a, b) {
            return a + b
        }
        let funcs = {op = add}
        return funcs.op(10, 20)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 30);
}

TEST_F(EdgeCaseTest, ConditionalFunctionSelection)
{
    constexpr std::string_view code = R"(
        function double(n) { return n * 2 }
        function triple(n) { return n * 3 }
        function choose(flag) {
            if (flag) {
                return double
            } else {
                return triple
            }
        }
        let f = choose(true)
        return f(7)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 14);
}

TEST_F(EdgeCaseTest, LoopWithComplexUpdate)
{
    constexpr std::string_view code = R"(
        function next(n) {
            return n + 2
        }
        let sum = 0
        for (let i = 0; i < 10; i = next(i)) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 20);
}

TEST_F(EdgeCaseTest, ComplexBooleanExpression)
{
    constexpr std::string_view code = R"(
        function check(a, b, c) {
            if (a > 5) {
                if (b < 10) {
                    return true
                }
            }
            if (c == 0) {
                return true
            }
            return false
        }
        return check(6, 8, 1)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(EdgeCaseTest, MethodCallSyntax)
{
    constexpr std::string_view code = R"(
        let obj = {
            value = 10,
            getValue = function(self) { return self.value }
        }
        return obj:getValue()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}
