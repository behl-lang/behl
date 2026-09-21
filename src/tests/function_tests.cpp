#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class FunctionTest : public ::testing::TestWithParam<bool>
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

TEST_P(FunctionTest, ExecuteFunctionCall)
{
    constexpr std::string_view code = R"(
        function add(a, b) {
            return a + b
        }
        return add(3, 4)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 7);
}

TEST_P(FunctionTest, GlobalFunctionAcrossLoadStrings)
{
    constexpr std::string_view code = R"(
        function add(a, b) {
            return a + b
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));

    ASSERT_NO_THROW(behl::call(S, 0, 0));

    constexpr std::string_view code2 = R"(
        return add(2, 3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code2));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(FunctionTest, GlobalFunctionOverrideAcrossLoadStrings)
{
    constexpr std::string_view code = R"(
        function add(a, b) {
            return a + b
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    constexpr std::string_view code2 = R"(
        function add(a, b) {
            return a * b
        }
        return add(2, 3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code2));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(FunctionTest, GlobalFunctionDefineCallOverrideThenCall)
{
    constexpr std::string_view code = R"(
        function add(a, b) {
            return a + b
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    ASSERT_EQ(behl::get_top(S), 0);

    constexpr std::string_view code2 = R"(
        return add(2, 3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code2));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
    behl::pop(S, 1);

    constexpr std::string_view code3 = R"(
        function add(a, b) {
            return a * b
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code3));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    ASSERT_EQ(behl::get_top(S), 0);

    constexpr std::string_view code4 = R"(
        return add(2, 3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code4));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
    behl::pop(S, 1);
}

TEST_P(FunctionTest, ClosureCapturesLexicalVar)
{
    constexpr std::string_view code = R"(
        function make() {
            let x = 0
            return function() {
                x = x + 1
                return x
            }
        }
        let c = make()
        return c() + c()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(FunctionTest, ClosureIndependentCounters)
{
    constexpr std::string_view code = R"(
        function make() {
            let x = 0
            return function() {
                x = x + 1
                return x
            }
        }
        let c1 = make()
        let c2 = make()
        return c1() + c2() + c1()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 4);
}

TEST_P(FunctionTest, ClosureCapturesArgument)
{
    constexpr std::string_view code = R"(
        function mk(n) {
            return function() {
                return n
            }
        }
        return mk(5)()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(FunctionTest, ExecuteTableFunctionCall)
{
    constexpr std::string_view code = R"(
        let tab = {
            func = function(x) {
                return x + 1
            }
        }
        return tab.func(1)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(FunctionTest, ExecuteMethodCallColonPassesSelf)
{
    constexpr std::string_view code = R"(
        let t = { value = 41 }
        function t:inc(a) {
            return self.value + a
        }
        return t:inc(1)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(FunctionTest, ExecuteMethodCallDotDoesNotPassSelf)
{
    constexpr std::string_view code = R"(
        let t = {}
        function t:whoami() {
            return self
        }
        return t.whoami()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code)) << behl::to_string(S, -1);
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_P(FunctionTest, ExecuteMethodLocalCallColonPassesSelf)
{
    constexpr std::string_view code = R"(
        let t = { value = 41 }
        function t:inc(a) {
            return self.value + a
        }
        return t:inc(1)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(FunctionTest, ExecuteMethodLocalCallDotDoesNotPassSelf)
{
    constexpr std::string_view code = R"(
        let t = {}
        function t:whoami() {
            return self
        }
        return t.whoami()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_P(FunctionTest, ExecuteLocalFunctionMutualRecursion_EvenOdd)
{
    constexpr std::string_view code = R"(
        let even
        let odd
        even = function(n) {
            if (n == 0) {
                return true
            }
            return odd(n - 1)
        }
        odd = function(n) {
            if (n == 0) {
                return false
            }
            return even(n - 1)
        }
        return even(10)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(FunctionTest, UpvalueModificationWhileOnStack)
{
    constexpr std::string_view code = R"(
        function outer() {
            let x = 10
            function inner() {
                x = x + 1
            }
            inner()
            inner()
            return x
        }
        return outer()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

INSTANTIATE_TEST_SUITE_P(Mode, FunctionTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
