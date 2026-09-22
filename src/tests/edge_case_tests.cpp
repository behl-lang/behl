#include "config_internal.hpp"
#include "state.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>
#include <string>

namespace
{
    constexpr int kPoolOverflow = 520;
    constexpr int kNarrowFieldLimit = 512;

    std::string fp_padding(double& expected)
    {
        std::string out;
        for (int i = 0; i < kNarrowFieldLimit; ++i)
        {
            const double v = static_cast<double>(i) + 0.5;
            out += "    acc = acc + " + std::to_string(v) + "\n";
            expected += v;
        }
        return out;
    }

    std::string int_padding(int64_t& expected)
    {
        std::string out;
        for (int i = 0; i < kNarrowFieldLimit; ++i)
        {
            const int64_t v = 100000 + i;
            out += "    acc = acc + " + std::to_string(v) + "\n";
            expected += v;
        }
        return out;
    }
} // namespace

class EdgeCaseTest : public ::testing::TestWithParam<bool>
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

TEST_P(EdgeCaseTest, EmptyFunctionCall)
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

TEST_P(EdgeCaseTest, FunctionReturningFunction)
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

TEST_P(EdgeCaseTest, ImmediatelyInvokedFunction)
{
    constexpr std::string_view code = R"(
        let result = (function(x) { return x * 2 })(21)
        return result
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(EdgeCaseTest, ChainedPropertyAccess)
{
    constexpr std::string_view code = R"(
        let t1 = {inner = {value = 123}}
        return t1.inner.value
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 123);
}

TEST_P(EdgeCaseTest, FunctionCallInTableConstructor)
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

TEST_P(EdgeCaseTest, NestedTableAccess)
{
    constexpr std::string_view code = R"(
        let matrix = {{1, 2}, {3, 4}, {5, 6}}
        return matrix[1][1]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 4);
}

TEST_P(EdgeCaseTest, MultipleAssignmentsInLoop)
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

TEST_P(EdgeCaseTest, FunctionAsTableValue)
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

TEST_P(EdgeCaseTest, ConditionalFunctionSelection)
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

TEST_P(EdgeCaseTest, LoopWithComplexUpdate)
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

TEST_P(EdgeCaseTest, ComplexBooleanExpression)
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

TEST_P(EdgeCaseTest, MethodCallSyntax)
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

TEST_P(EdgeCaseTest, FloatConstantIndexBeyondNarrowField)
{
    double expected = 0.0;
    std::string code = "function f() {\n    let acc = 0.0\n";
    for (int i = 0; i < kPoolOverflow; ++i)
    {
        const double v = static_cast<double>(i) + 0.5;
        code += "    acc = acc + " + std::to_string(v) + "\n";
        expected += v;
    }
    code += "    return acc\n}\nreturn f()\n";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), expected);
}

TEST_P(EdgeCaseTest, IntegerConstantIndexBeyondNarrowField)
{
    int64_t expected = 0;
    std::string code = "function f() {\n    let acc = 0\n";
    for (int i = 0; i < kPoolOverflow; ++i)
    {
        const int64_t v = 100000 + i;
        code += "    acc = acc + " + std::to_string(v) + "\n";
        expected += v;
    }
    code += "    return acc\n}\nreturn f()\n";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), expected);
}

TEST_P(EdgeCaseTest, StringConstantIndexBeyondNarrowField)
{
    std::string code = "function f() {\n    let acc = \"\"\n";
    std::string expected;
    for (int i = 0; i < kNarrowFieldLimit; ++i)
    {
        std::string tag = std::to_string(i);
        while (tag.size() < 3)
        {
            tag.insert(tag.begin(), '0');
        }
        code += "    acc = acc + \"s" + tag + "\"\n";
        expected += "s" + tag;
    }
    code += "    let tail = acc + \"final\"\n    return tail\n}\nreturn f()\n";
    expected += "final";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), expected);
}

TEST_P(EdgeCaseTest, FloatCompareConstantIndexBeyondNarrowField)
{
    double expected = 0.0;
    std::string code = "function f() {\n    let acc = 0.0\n";
    code += fp_padding(expected);
    code += "    if (acc < 8388609.25) { return 1 }\n    return 0\n}\nreturn f()\n";

    ASSERT_LT(expected, 8388609.25);
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(EdgeCaseTest, IntegerCompareConstantIndexBeyondNarrowField)
{
    int64_t expected = 0;
    std::string code = "function f() {\n    let acc = 0\n";
    code += int_padding(expected);
    code += "    if (acc > 100000000) { return 1 }\n    return 0\n}\nreturn f()\n";

    ASSERT_LT(expected, 100000000);
    ASSERT_GT(expected, 100000);
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(EdgeCaseTest, CompareInValueContextConstantIndexBeyondNarrowField)
{
    double expected = 0.0;
    std::string code = "function id(v) { return v }\nfunction f() {\n    let acc = 0.0\n";
    code += fp_padding(expected);
    code += "    return id(acc < 8388609.25)\n}\nreturn f()\n";

    ASSERT_LT(expected, 8388609.25);
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(EdgeCaseTest, CompoundAssignConstantIndexBeyondNarrowField)
{
    double expected = 0.0;
    std::string code = "function f() {\n    let acc = 0.0\n";
    code += fp_padding(expected);
    code += "    acc += 8388609.25\n    return acc\n}\nreturn f()\n";
    expected += 8388609.25;

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), expected);
}

TEST_P(EdgeCaseTest, MetamethodSurvivesConstantIndexFallback)
{
    behl::load_stdlib(S);
    double expected = 0.0;
    std::string code = "function f() {\n    let acc = 0.0\n";
    code += fp_padding(expected);
    code += "    let mt = {}\n";
    code += "    mt.__add = function(a, b) { return 4242 }\n";
    code += "    let obj = setmetatable({}, mt)\n";
    code += "    return obj + 8388609.25\n}\nreturn f()\n";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 4242);
}

TEST_P(EdgeCaseTest, ConstantLimitExceededThrows)
{
    std::string code;
    code.reserve(4u << 20);
    code += "function f() {\n    let acc = 0.0\n";
    for (size_t i = 0; i <= behl::kMaxConstants; ++i)
    {
        code += "    acc = acc + " + std::to_string(i) + ".5\n";
    }
    code += "    return acc\n}\nreturn f()\n";

    ASSERT_THROW(behl::load_string(S, code), behl::SyntaxError);
}

TEST_P(EdgeCaseTest, ConstantLimitBoundaryCompiles)
{
    double expected = 0.0;
    std::string code;
    code.reserve(4u << 20);
    code += "function f() {\n    let acc = 0.0\n";
    for (size_t i = 0; i < behl::kMaxConstants; ++i)
    {
        code += "    acc = acc + " + std::to_string(i) + ".5\n";
        expected += static_cast<double>(i) + 0.5;
    }
    code += "    return acc\n}\nreturn f()\n";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), expected);
}

TEST_P(EdgeCaseTest, StringRepNormalCases)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        const string = import("string")
        return string.rep("ab", 3) + "|" + string.rep("x", 1) + "|" + string.rep("y", 0)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_string(S, -1), "ababab|x|");
}

TEST_P(EdgeCaseTest, StringRepNegativeCountIsEmpty)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        const string = import("string")
        return string.rep("ab", -5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_string(S, -1), "");
}

TEST_P(EdgeCaseTest, StringRepEmptySourceIgnoresCount)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        const string = import("string")
        return string.rep("", 9223372036854775807)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_string(S, -1), "");
}

TEST_P(EdgeCaseTest, StringRepOverflowingCountIsRejected)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        const string = import("string")
        return string.rep("ab", 9223372036854775807)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_THROW(behl::call(S, 0, 1), behl::RuntimeError);
}

TEST_P(EdgeCaseTest, StringRepBeyondSizeLimitIsRejected)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        const string = import("string")
        return string.rep("a", 3000000000)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_THROW(behl::call(S, 0, 1), behl::RuntimeError);
}

TEST_P(EdgeCaseTest, StringRepLongSourceBeyondSizeLimitIsRejected)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        const string = import("string")
        let s = string.rep("abcdefgh", 128)
        return string.rep(s, 3000000)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_THROW(behl::call(S, 0, 1), behl::RuntimeError);
}

TEST_P(EdgeCaseTest, StringCaseConversionHandlesAscii)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        const string = import("string")
        return string.upper("abcXYZ123!") + "|" + string.lower("ABCxyz123!")
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_string(S, -1), "ABCXYZ123!|abcxyz123!");
}

TEST_P(EdgeCaseTest, StringCaseConversionLeavesHighBytesAlone)
{
    behl::load_stdlib(S);

    constexpr std::string_view code = R"(
        const string = import("string")
        let out = ""
        for (let i = 128; i < 256; i = i + 1) {
            let c = string.char(i)
            if (string.byte(string.upper(c), 0) != i) { return i }
            if (string.byte(string.lower(c), 0) != i) { return i }
        }
        return -1
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), -1) << "byte was altered by case conversion";
}

TEST_P(EdgeCaseTest, StringLiteralLargerThanAstPool)
{
    std::string code = "let s = " + std::string(1, '"') + std::string(70000, 'A') + std::string(1, '"') + "\nreturn #s\n";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 70000);
}

TEST_P(EdgeCaseTest, StringLiteralManyTimesTheAstPool)
{
    std::string code = "let s = " + std::string(1, '"') + std::string(1024 * 1024, 'B') + std::string(1, '"') + "\nreturn #s\n";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 1024 * 1024);
}

TEST_P(EdgeCaseTest, MultipleOversizedStringLiteralsCoexist)
{
    const std::string big_a(70000, 'A');
    const std::string big_b(90000, 'B');
    std::string code = "let a = " + std::string(1, '"') + big_a + std::string(1, '"') + "\n";
    code += "let b = " + std::string(1, '"') + big_b + std::string(1, '"') + "\n";
    code += "let c = " + std::string(1, '"') + "small" + std::string(1, '"') + "\n";
    code += "return #a + #b + #c\n";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 70000 + 90000 + 5);
}

INSTANTIATE_TEST_SUITE_P(Mode, EdgeCaseTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
