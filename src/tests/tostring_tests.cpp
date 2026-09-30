#include "behl/behl.hpp"
#include "state.hpp"
#include "test_helpers.hpp"

#include <gtest/gtest.h>
#include <string>

class ToStringTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        ASSERT_NE(S, nullptr);
        behl::load_lib_core(S);
    }

    void TearDown() override
    {
        if (S)
        {
            behl::close(S);
            S = nullptr;
        }
    }
};

TEST_P(ToStringTest, ToString_Nil)
{
    constexpr std::string_view code = R"(
        let x = nil;
        return tostring(x);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "nil");
}

TEST_P(ToStringTest, ToString_BooleanTrue)
{
    constexpr std::string_view code = R"(
        return tostring(true);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "true");
}

TEST_P(ToStringTest, ToString_BooleanFalse)
{
    constexpr std::string_view code = R"(
        return tostring(false);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "false");
}

TEST_P(ToStringTest, ToString_Integer)
{
    constexpr std::string_view code = R"(
        return tostring(42);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "42");
}

TEST_P(ToStringTest, ToString_NegativeInteger)
{
    constexpr std::string_view code = R"(
        return tostring(-123);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "-123");
}

TEST_P(ToStringTest, ToString_Float)
{
    constexpr std::string_view code = R"(
        return tostring(3.14159);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(std::string(result).find("3.14"), std::string_view::npos);
}

TEST_P(ToStringTest, ToString_NegativeFloat)
{
    constexpr std::string_view code = R"(
        return tostring(-2.718);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(std::string(result).find("-2.7"), std::string_view::npos);
}

TEST_P(ToStringTest, ToString_String)
{
    constexpr std::string_view code = R"(
        return tostring("hello");
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "hello");
}

TEST_P(ToStringTest, ToString_EmptyString)
{
    constexpr std::string_view code = R"(
        return tostring("");
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "");
}

TEST_P(ToStringTest, ToString_Function)
{
    constexpr std::string_view code = R"(
        function foo() {}
        return tostring(foo);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("function:"), std::string_view::npos);
}

TEST_P(ToStringTest, ToString_CFunction)
{
    constexpr std::string_view code = R"(
        return tostring(print);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("cfunction:"), std::string_view::npos);
}

TEST_P(ToStringTest, ToString_Table)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3};
        return tostring(t);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("table:"), std::string_view::npos);
}

TEST_P(ToStringTest, ToString_EmptyTable)
{
    constexpr std::string_view code = R"(
        let t = {};
        return tostring(t);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_NE(result.find("table:"), std::string_view::npos);
}

TEST_P(ToStringTest, ToString_TableWithMetamethod)
{
    constexpr std::string_view code = R"(
        let t = {value = 42};
        let mt = {
            __tostring = function(self) {
                return "custom:" + tostring(self.value);
            }
        };
        setmetatable(t, mt);
        return tostring(t);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "custom:42");
}

TEST_P(ToStringTest, ToString_TableWithMetamethodReturningNumber)
{
    constexpr std::string_view code = R"(
        let t = {};
        let mt = {
            __tostring = function(self) {
                return tostring(123);  // Must return string
            }
        };
        setmetatable(t, mt);
        return tostring(t);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "123");
}

TEST_P(ToStringTest, ToString_TableWithMetamethodReturningNil)
{
    constexpr std::string_view code = R"(
        let t = {};
        let mt = {
            __tostring = function(self) {
                return "nil";  // Must return string
            }
        };
        setmetatable(t, mt);
        return tostring(t);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "nil");
}

TEST_P(ToStringTest, ToString_ZeroInteger)
{
    constexpr std::string_view code = R"(
        return tostring(0);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "0");
}

TEST_P(ToStringTest, ToString_ZeroFloat)
{
    constexpr std::string_view code = R"(
        return tostring(0.0);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    std::string_view result = to_string(S, -1);
    EXPECT_TRUE(result == "0" || result == "0.0" || result == "0.000000");
}

TEST_P(ToStringTest, ToString_LargeInteger)
{
    constexpr std::string_view code = R"(
        return tostring(9223372036854775807);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "9223372036854775807");
}

TEST_P(ToStringTest, ToString_SmallFloat)
{
    constexpr std::string_view code = R"(
        return tostring(0.0001);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "0.0001");
}

TEST_P(ToStringTest, ToString_MultipleValues)
{
    constexpr std::string_view code = R"(
        let a = tostring(1);
        let b = tostring(2);
        let c = tostring(3);
        return a + b + c;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "123");
}

TEST_P(ToStringTest, ToString_InExpression)
{
    constexpr std::string_view code = R"(
        let result = "Value: " + tostring(42) + "!";
        return result;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "Value: 42!");
}

TEST_P(ToStringTest, ToString_InLoop)
{
    constexpr std::string_view code = R"(
        let result = "";
        for (let i = 0; i < 3; i++) {
            result = result + tostring(i);
        }
        return result;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "012");
}

TEST_P(ToStringTest, ToString_NestedCalls)
{
    constexpr std::string_view code = R"(
        return tostring(tostring(42));
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(is_string(S, -1));
    EXPECT_EQ(to_string(S, -1), "42");
}

TEST_P(ToStringTest, ToString_IntegerMinimum)
{
    constexpr std::string_view code = R"(
        let mini = -9223372036854775807 - 1;
        return tostring(mini);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "-9223372036854775808");
}

TEST_P(ToStringTest, ToString_Infinities)
{
    constexpr std::string_view code = R"(
        let one = 1.0;
        let zero = 0.0;
        return tostring(one / zero), tostring(zero - one / zero);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "inf");
    EXPECT_EQ(to_string(S, -1), "-inf");
}

TEST_P(ToStringTest, ToString_NaN)
{
    constexpr std::string_view code = R"(
        let zero = 0.0;
        return tostring(zero / zero);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "nan");
}

TEST_P(ToStringTest, ToString_LargeFloatUsesExponent)
{
    constexpr std::string_view code = R"(
        let ten = 10.0;
        return tostring(ten ** 100);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_string(S, -1), "1e+100");
}

TEST_P(ToStringTest, ToString_ShortestRoundTrip)
{
    constexpr std::string_view code = R"(
        let a = 0.1;
        let b = 0.2;
        return tostring(a), tostring(a + b);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "0.1");
    EXPECT_EQ(to_string(S, -1), "0.30000000000000004");
}

TEST_P(ToStringTest, ToString_WholeFloatsKeepDecimalPoint)
{
    constexpr std::string_view code = R"(
        let a = -1203.0;
        let b = 100.0;
        let c = 1.0;
        return tostring(a), tostring(b), tostring(c);
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(to_string(S, -3), "-1203.0");
    EXPECT_EQ(to_string(S, -2), "100.0");
    EXPECT_EQ(to_string(S, -1), "1.0");
}

TEST_P(ToStringTest, LocalShadowingMakesANormalCall)
{
    constexpr std::string_view code = R"(
        let tostring = function(v) { return "local" }
        let tonumber = function(v) { return -1 }
        let a = tostring(1)
        let b = tonumber("5")
        return a, b
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "local");
    EXPECT_EQ(to_integer(S, -1), -1);
}

TEST_P(ToStringTest, LocalShadowingInReturnMakesANormalCall)
{
    constexpr std::string_view code = R"(
        let tostring = function(v) { return "local" }
        let tonumber = function(v) { return -1 }
        function f(x) { return tostring(x) }
        function g(x) { return tonumber(x) }
        return f(1), g("5")
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "local");
    EXPECT_EQ(to_integer(S, -1), -1);
}

TEST_P(ToStringTest, UpvalueShadowingMakesANormalCall)
{
    constexpr std::string_view code = R"(
        let tostring = function(v) { return "upvalue" }
        let tonumber = function(v) { return -2 }
        function outer() {
            function inner(x) {
                let a = tostring(x)
                let b = tonumber(x)
                return a, b
            }
            return inner(1)
        }
        return outer()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "upvalue");
    EXPECT_EQ(to_integer(S, -1), -2);
}

TEST_P(ToStringTest, ParameterShadowingMakesANormalCall)
{
    constexpr std::string_view code = R"(
        function apply(tostring, tonumber, x) {
            let a = tostring(x)
            return a, tonumber(x)
        }
        return apply(function(v) { return "param" }, function(v) { return -3 }, 1)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "param");
    EXPECT_EQ(to_integer(S, -1), -3);
}

TEST_P(ToStringTest, ShadowingEndsWithItsScope)
{
    constexpr std::string_view code = R"(
        let r = ""
        {
            let tostring = function(v) { return "inner" }
            r = tostring(1)
        }
        return r, tostring(1)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "inner");
    EXPECT_EQ(to_string(S, -1), "1");
}

TEST_P(ToStringTest, ArgumentWithComparisonInCondition)
{
    constexpr std::string_view code = R"(
        let x = 2
        let hits = 0
        if (tostring(x == 1)) { hits = hits + 1 }
        if (tostring(x == 1) == "false") { hits = hits + 10 }
        while (tostring(x > 5) == "false") { hits = hits + 100; x = x + 10 }
        if (tonumber(x == 1)) { hits = hits + 1000 }
        return hits
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 111);
}

TEST_P(ToStringTest, ArgumentWithLogicalOperatorsInCondition)
{
    constexpr std::string_view code = R"(
        let a = nil
        let b = false
        let hits = 0
        if (tostring(a || b)) { hits = hits + 1 }
        if (tostring(a && b) == "nil") { hits = hits + 10 }
        if (!tostring(!a)) { hits = hits + 100 }
        return hits
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 11);
}

TEST_P(ToStringTest, ExtraTargetsAreNil)
{
    constexpr std::string_view code = R"(
        function f() {
            {
                let p, q, r, s = 1, 2, 3, 4
            }
            let a, b, c = tostring(1)
            let d, e = tonumber("7")
            return a, b == nil, c == nil, d, e == nil
        }
        return f()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    EXPECT_EQ(to_string(S, -5), "1");
    EXPECT_TRUE(to_boolean(S, -4));
    EXPECT_TRUE(to_boolean(S, -3));
    EXPECT_EQ(to_integer(S, -2), 7);
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(ToStringTest, ExtraAssignmentTargetsAreNil)
{
    constexpr std::string_view code = R"(
        let a = 5
        let b = 6
        a, b = tostring(1)
        return a, b == nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "1");
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(ToStringTest, SingleResultInMultiValueContexts)
{
    constexpr std::string_view code = R"(
        function count(...) { let t = { ... }; return #t }
        function ret_multi(x) { return 7, tostring(x) }
        function ret_tail(x) { return count(tostring(x)) }
        let c, d = ret_multi(1.5)
        let t = { tostring(1) }
        let u = { 0, tonumber("2") }
        return count(tostring(1)), count(1, tonumber("2")), c, d, ret_tail(3), #t, #u
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 7));
    EXPECT_EQ(to_integer(S, -7), 1);
    EXPECT_EQ(to_integer(S, -6), 2);
    EXPECT_EQ(to_integer(S, -5), 7);
    EXPECT_EQ(to_string(S, -4), "1.5");
    EXPECT_EQ(to_integer(S, -3), 1);
    EXPECT_EQ(to_integer(S, -2), 1);
    EXPECT_EQ(to_integer(S, -1), 2);
}

TEST_P(ToStringTest, ReturnListEndingInIntrinsic)
{
    constexpr std::string_view code = R"(
        function f(x) { return 1, tostring(x) }
        function g(x) { return tonumber(x) }
        let a, b, c = f(9)
        let d, e = g("4")
        return a, b, c == nil, d, e == nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    EXPECT_EQ(to_integer(S, -5), 1);
    EXPECT_EQ(to_string(S, -4), "9");
    EXPECT_TRUE(to_boolean(S, -3));
    EXPECT_EQ(to_integer(S, -2), 4);
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(ToStringTest, ResultIntoVariousTargets)
{
    constexpr std::string_view code = R"(
        let x = 5
        x = tostring(x)
        let t = {}
        t.a = tostring(1)
        t[tostring(2)] = tostring(3)
        let y = nil
        y = y || tostring(4)
        let z = tostring(5) + tostring(6)
        let w = tonumber(tostring(7)) + tonumber("8")
        return x, t.a, t["2"], y, z, w
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 6));
    EXPECT_EQ(to_string(S, -6), "5");
    EXPECT_EQ(to_string(S, -5), "1");
    EXPECT_EQ(to_string(S, -4), "3");
    EXPECT_EQ(to_string(S, -3), "4");
    EXPECT_EQ(to_string(S, -2), "56");
    EXPECT_EQ(to_integer(S, -1), 15);
}

TEST_P(ToStringTest, ArgumentIsEvaluatedOnce)
{
    constexpr std::string_view code = R"(
        let calls = 0
        function f() { calls = calls + 1; return 1, "two" }
        let a = tostring(f())
        let b = tonumber(f())
        return calls, a, b
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(to_integer(S, -3), 2);
    EXPECT_EQ(to_string(S, -2), "1");
    EXPECT_EQ(to_integer(S, -1), 1);
}

INSTANTIATE_TEST_SUITE_P(Mode, ToStringTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
