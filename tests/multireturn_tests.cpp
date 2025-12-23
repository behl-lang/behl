#include <behl/behl.hpp>
#include <gtest/gtest.h>

class MultiReturnTest : public ::testing::Test
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

TEST_F(MultiReturnTest, FunctionReturnsTwoValues_OnlyFirstReturned)
{
    constexpr std::string_view code = R"(
        function f() {
            return 10, 20
        }
        return f()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_F(MultiReturnTest, FunctionReturnsThreeValues_OnlyFirstReturned)
{
    constexpr std::string_view code = R"(
        function multi() {
            return 100, 200, 300
        }
        return multi()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
}

TEST_F(MultiReturnTest, FunctionReturnsMixedTypes_OnlyFirstReturned)
{
    constexpr std::string_view code = R"(
        function f() {
            return 42, "hello", true
        }
        return f()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(MultiReturnTest, FunctionWithMultipleReturns_RequestZeroResults)
{
    constexpr std::string_view code = R"(
        function f() {
            return 1, 2, 3
        }
        f()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(MultiReturnTest, NestedFunctionWithMultipleReturns_OnlyFirstReturned)
{
    constexpr std::string_view code = R"(
        function inner() {
            return 5, 10 
        }
        function outer() {
            let x = inner()
            return x
        }
        return outer()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_F(MultiReturnTest, MultipleReturnsInExpression_OnlyFirstUsed)
{
    constexpr std::string_view code = R"(
        function pair() {
            return 3, 7 
        }
        return pair() + 10
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 13);
}

TEST_F(MultiReturnTest, AssignmentFromMultipleReturns_OnlyFirstAssigned)
{
    constexpr std::string_view code = R"(
        function triple() {
            return 1, 2, 3 
        }
        let x = triple()
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_F(MultiReturnTest, FunctionWithEmptyReturn)
{
    constexpr std::string_view code = R"(
        function f() {
            return
        }
        f()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    ASSERT_EQ(behl::get_top(S), 0);
}

TEST_F(MultiReturnTest, FunctionReturnsSingleValue)
{
    constexpr std::string_view code = R"(
        function f() {
            return 42
        }
        return f()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(MultiReturnTest, FunctionReturnsComputedValues_OnlyFirstReturned)
{
    constexpr std::string_view code = R"(
        function compute(x) {
            return x + 1, x + 2, x + 3 
        }
        return compute(10)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 11);
}

TEST_F(MultiReturnTest, RecursiveFunctionWithMultipleReturns_OnlyFirstReturned)
{
    constexpr std::string_view code = R"(
        function fib(n) {
            if (n <= 1) {
                return n, n * 2
            }
            return fib(n - 1) + fib(n - 2), 999
        }
        return fib(5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_F(MultiReturnTest, FunctionImplicitReturn)
{
    constexpr std::string_view code = R"(
        function f() {}
        return f()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(MultiReturnTest, FunctionReturnsTableAndValue_OnlyFirstReturned)
{
    constexpr std::string_view code = R"(
        function make_pair() {
            return {a = 1, b = 2}, 100
        }
        return make_pair()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);
}

TEST_F(MultiReturnTest, MultipleReturnsInTableConstructor_OnlyFirstUsed)
{
    constexpr std::string_view code = R"(
        function pair() {
            return 5, 6 
        }
        let t = { x = pair() }
        return t.x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_F(MultiReturnTest, CallingWithMultret_GetsAllValues)
{
    constexpr std::string_view code = "function triple() { return 10, 20, 30 }";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    behl::get_global(S, "triple");
    ASSERT_EQ(behl::type(S, -1), behl::Type::kClosure);

    ASSERT_NO_THROW(behl::call(S, 0, behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 10);
    ASSERT_EQ(behl::to_integer(S, -2), 20);
    ASSERT_EQ(behl::to_integer(S, -1), 30);
}

TEST_F(MultiReturnTest, MultipleAssignmentFromFunctionCall)
{
    constexpr std::string_view code = R"(
        function triple() { return 100, 200, 300 }
        let a, b, c = triple()
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 100);
    ASSERT_EQ(behl::to_integer(S, -2), 200);
    ASSERT_EQ(behl::to_integer(S, -1), 300);
}

TEST_F(MultiReturnTest, MultipleAssignmentPadsWithNil)
{
    constexpr std::string_view code = R"(
        function two() { return 1, 2 }
        let a, b, c, d = two()
        return a, b, c, d
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), 1);
    ASSERT_EQ(behl::to_integer(S, -3), 2);
    ASSERT_EQ(behl::type(S, -2), behl::Type::kNil);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(MultiReturnTest, MultipleAssignmentTruncatesExtraValues)
{
    constexpr std::string_view code = R"(
        function five() { return 1, 2, 3, 4, 5 }
        let a, b = five()
        return a, b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_F(MultiReturnTest, FunctionCallAsLastArgumentPassesAllValues)
{
    constexpr std::string_view code = R"(
        function add(a, b, c) { return a + b + c }
        function triple() { return 10, 20, 30 }
        return add(triple())
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_F(MultiReturnTest, FunctionCallInMiddleReturnsOnlyFirst)
{
    constexpr std::string_view code = R"(
        function add(a, b, c) { return a + b + c }
        function triple() { return 10, 20, 30 }
        return add(triple(), 5, 15)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 30);
}

TEST_F(MultiReturnTest, ReturnPassesThroughAllValues)
{
    constexpr std::string_view code = R"(
        function inner() { return 1, 2, 3, 4 }
        function outer() { return inner() }
        return outer()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), 1);
    ASSERT_EQ(behl::to_integer(S, -3), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 3);
    ASSERT_EQ(behl::to_integer(S, -1), 4);
}

TEST_F(MultiReturnTest, ReturnWithMixedValuesAndCall)
{
    constexpr std::string_view code = R"(
        function inner() { return 3, 4, 5 }
        function outer() { return 1, 2, inner() }
        return outer()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 5);
    ASSERT_EQ(behl::to_integer(S, -5), 1);
    ASSERT_EQ(behl::to_integer(S, -4), 2);
    ASSERT_EQ(behl::to_integer(S, -3), 3);
    ASSERT_EQ(behl::to_integer(S, -2), 4);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_F(MultiReturnTest, ChainedCallsWithMultret)
{
    constexpr std::string_view code = R"(
        function third() { return 7, 8, 9 }
        function second() { return third() }
        function first() { return second() }
        return first()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 7);
    ASSERT_EQ(behl::to_integer(S, -2), 8);
    ASSERT_EQ(behl::to_integer(S, -1), 9);
}

TEST_F(MultiReturnTest, CallInExpressionGetsOnlyFirst)
{
    constexpr std::string_view code = R"(
        function multi() { return 10, 20, 30 }
        return multi() + 5
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_F(MultiReturnTest, MultipleAssignmentMixedTypes)
{
    constexpr std::string_view code = R"(
        function mixed() { return 42, "hello", true, nil }
        let a, b, c, d = mixed()
        return a, b, c, d
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), 42);
    ASSERT_EQ(behl::to_string(S, -3), "hello");
    ASSERT_EQ(behl::to_boolean(S, -2), true);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(MultiReturnTest, EmptyReturnAfterMultret)
{
    constexpr std::string_view code = R"(
        function foo() {
            if (true) {
                return 1, 2, 3
            }
            return
        }
        return foo()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 1);
    ASSERT_EQ(behl::to_integer(S, -2), 2);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_F(MultiReturnTest, RecursiveFunctionWithMultret)
{
    constexpr std::string_view code = R"(
        function fib_pair(n) {
            if (n <= 1) {
                return n, n * 2
            }
            return fib_pair(n - 1)
        }
        let a, b = fib_pair(5)
        return a, b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_F(MultiReturnTest, NoExplicitReturnInMultipleAssignment)
{
    constexpr std::string_view code = R"(
        function foo() {
            let x = 10
        }
        let a, b, c = foo()
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::type(S, -3), behl::Type::kNil);
    ASSERT_EQ(behl::type(S, -2), behl::Type::kNil);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}
TEST_F(MultiReturnTest, MultipleAssignmentToGlobals)
{
    constexpr std::string_view code = R"(
        function triple() {
            return 100, 200, 300
        }
        x, y, z = triple()
        return x, y, z
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 100);
    ASSERT_EQ(behl::to_integer(S, -2), 200);
    ASSERT_EQ(behl::to_integer(S, -1), 300);
}

TEST_F(MultiReturnTest, MultipleAssignmentGlobalsPadsWithNil)
{
    constexpr std::string_view code = R"(
        function two() { return 1, 2 }
        a, b, c, d = two()
        return a, b, c, d
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), 1);
    ASSERT_EQ(behl::to_integer(S, -3), 2);
    ASSERT_EQ(behl::type(S, -2), behl::Type::kNil);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}
