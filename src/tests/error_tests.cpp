#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>

class ErrorTest : public ::testing::TestWithParam<bool>
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

    std::string_view get_error()
    {
        EXPECT_EQ(behl::type(S, -1), behl::Type::kString);
        return behl::to_string(S, -1);
    }
};

TEST_P(ErrorTest, TypeError_ArithmeticOnNonNumber)
{
    constexpr std::string_view code = R"(
        let x = 5 + 'hello'
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CallNonFunction)
{
    constexpr std::string_view code = R"(
        let x = 5; x()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CallNil)
{
    constexpr std::string_view code = R"(
        let x = nil; x()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CallBoolean)
{
    constexpr std::string_view code = R"(
        let x = true; x()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CallNumber)
{
    constexpr std::string_view code = R"(
        let x = 3.14; x()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CallString)
{
    constexpr std::string_view code = R"(
        let x = "hello"; x()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CallTable)
{
    constexpr std::string_view code = R"(
        let x = {1, 2, 3}; x()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_IndexNonTable)
{
    constexpr std::string_view code = R"(
        let x = 5; let y = x[1]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_GetLengthOfInvalidType)
{
    constexpr std::string_view code = R"(
        let x = #123
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, ErrorIncludesSourceLocation)
{
    constexpr std::string_view code = R"(
        let x = 1 + nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, ErrorInNestedFunction)
{
    constexpr std::string_view code = R"(
        function outer() {
            function inner() {
                return 5 + 'bad'
            }
            return inner()
        }
        outer()
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, MultipleOperationsShowCorrectError)
{
    constexpr std::string_view code = R"(
        let a = 5
        let b = 10
        let c = a + b
        let d = c * 'invalid'
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code, false)); // Disable optimizations
    EXPECT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}
TEST_P(ErrorTest, ErrorFunction_BasicThrow)
{
    constexpr std::string_view code = R"(
        error("test error message")
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    ASSERT_TRUE(behl_test::call_fails(S, 0, 0));
    EXPECT_NE(behl_test::error_text(S).find("test error message"), std::string::npos);
}

TEST_P(ErrorTest, ErrorFunction_CaughtByPcall)
{
    constexpr std::string_view code = R"(
        function failing() {
            error("expected error");
        }
        let success, err = pcall(failing);
        return success, err;
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));

    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_FALSE(behl::to_boolean(S, -2));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("expected error"), std::string::npos);
}

TEST_P(ErrorTest, ErrorFunction_WithNumberConvertsToString)
{
    constexpr std::string_view code = R"(
        function test() {
            error(42);
        }
        let success, err = pcall(test);
        return success, err;
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(ErrorTest, ErrorFunction_InNestedCalls)
{
    constexpr std::string_view code = R"(
        function level3() {
            error("deep error");
        }
        function level2() {
            return level3();
        }
        function level1() {
            return level2();
        }
        let success, err = pcall(level1);
        return success, err;
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("deep error"), std::string::npos);
}

TEST_P(ErrorTest, ErrorFunction_WithConcatenatedMessage)
{
    constexpr std::string_view code = R"(
        let value = 123;
        function test() {
            error("Value is: " + tostring(value));
        }
        let success, err = pcall(test);
        return success, err;
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("Value is:"), std::string::npos);
    EXPECT_NE(err_msg.find("123"), std::string::npos);
}

TEST_P(ErrorTest, ErrorFunction_InClosure)
{
    constexpr std::string_view code = R"(
        function make_error_func(msg) {
            return function() {
                error(msg);
            };
        }
        let error_func = make_error_func("closure error");
        let success, err = pcall(error_func);
        return success, err;
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("closure error"), std::string::npos);
}

TEST_P(ErrorTest, ErrorFunction_MultipleInSequence)
{
    constexpr std::string_view code = R"(
        function error1() { error("first error"); }
        function error2() { error("second error"); }
        
        let s1, e1 = pcall(error1);
        let s2, e2 = pcall(error2);
        
        return s1, e1, s2, e2;
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));

    ASSERT_EQ(behl::get_top(S), 4);

    ASSERT_FALSE(behl::to_boolean(S, -4));
    std::string_view err1 = behl::to_string(S, -3);
    EXPECT_NE(err1.find("first error"), std::string::npos);

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err2 = behl::to_string(S, -1);
    EXPECT_NE(err2.find("second error"), std::string::npos);
}

TEST_P(ErrorTest, ErrorFunction_InLoop)
{
    constexpr std::string_view code = R"(
        function test() {
            for (let i = 0; i < 10; i++) {
                if (i == 5) {
                    error("loop error at " + tostring(i));
                }
            }
        }
        let success, err = pcall(test);
        return success, err;
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("loop error"), std::string::npos);
    EXPECT_NE(err_msg.find("5"), std::string::npos);
}

TEST_P(ErrorTest, TypeError_CompareIncompatibleTypes_LessThan)
{
    constexpr std::string_view code = R"(
        return 5 < "hello";
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 1));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CompareIncompatibleTypes_LessOrEqual)
{
    constexpr std::string_view code = R"(
        return true <= 42;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 1));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, TypeError_CompareTableWithNumber)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3};
        return t < 10;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 1));
    EXPECT_NE(behl_test::error_text(S).find("TypeError"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ErrorTest, LongErrorMessageIsNotReadPastItsEnd)
{
    constexpr std::string_view code = R"(
        const string = import("string")

        let msg = ""
        for (let i = 0; i < 40; i = i + 1) { msg = msg + "ABCDEFGH" }

        let ok, err = pcall(function() { error(msg) })

        let at = string.find(err, msg)
        if (at == nil) { return #msg, -1, "" }

        return #msg, at, string.sub(err, at + #msg, at + #msg)
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));

    ASSERT_EQ(behl::to_integer(S, -3), 320);
    ASSERT_GE(behl::to_integer(S, -2), 0) << "error text does not contain the message that was raised";
    EXPECT_EQ(behl::to_string(S, -1), "") << "bytes were read past the end of the message payload";
}

static std::string run_and_capture_error(behl::State* S, const std::string& code)
{
    if (behl::load_string(S, code, false) < 0 || behl::call(S, 0, 0) < 0)
    {
        return behl_test::error_text(S);
    }
    return {};
}

static void expect_runtime_error_text(behl::State* S, const std::string& body, const std::string& args, const char* needle)
{
    const std::string code = "let pad = 0\nlet f = function(a, b) { " + body + " }\nreturn f(" + args + ")\n";
    const std::string what = run_and_capture_error(S, code);
    ASSERT_FALSE(what.empty()) << "no error raised for: " << body;
    EXPECT_NE(what.find(needle), std::string::npos) << what;
    EXPECT_NE(what.find("TypeError"), std::string::npos) << what;
    EXPECT_NE(what.find("<string>(2,"), std::string::npos) << "error location missing or on the wrong line: " << what;
}

TEST_P(ErrorTest, CallNilMessageNamesTheProblemAndLocation)
{
    expect_runtime_error_text(S, "return a()", "nil, nil", "attempt to call");
}

TEST_P(ErrorTest, IndexNonTableReadMessageNamesTheProblemAndLocation)
{
    expect_runtime_error_text(S, "return a.field", "5, nil", "attempt to index");
}

TEST_P(ErrorTest, IndexNonTableWriteMessageNamesTheProblemAndLocation)
{
    expect_runtime_error_text(S, "a.field = 1", "5, nil", "attempt to index");
}

TEST_P(ErrorTest, ArithmeticOnNilMessageNamesTheProblemAndLocation)
{
    expect_runtime_error_text(S, "return a + b", "nil, 1", "attempt to perform arithmetic");
    const std::string what = run_and_capture_error(
        S, "let pad = 0\nlet f = function(a, b) { return a + b }\nreturn f(nil, 1)\n");
    EXPECT_NE(what.find("nil"), std::string::npos) << what;
}

TEST_P(ErrorTest, ArithmeticOnTableMessageNamesTheProblemAndLocation)
{
    expect_runtime_error_text(S, "return a * b", "{}, 2", "attempt to perform arithmetic");
    const std::string what = run_and_capture_error(
        S, "let pad = 0\nlet f = function(a, b) { return a * b }\nreturn f({}, 2)\n");
    EXPECT_NE(what.find("table"), std::string::npos) << what;
}

TEST_P(ErrorTest, CompareIncompatibleMessageNamesTheProblemAndLocation)
{
    expect_runtime_error_text(S, "return a < b", "1, \"x\"", "attempt to compare");
}

TEST_P(ErrorTest, ConcatInvalidMessageNamesTheProblemAndLocation)
{
    expect_runtime_error_text(S, "return a + b", "\"x\", {}", "concatenate");
}

TEST_P(ErrorTest, RuntimeErrorCaughtByPcallCarriesLocation)
{
    constexpr std::string_view code = "let pad = 0\nlet f = function(a) { return a.field }\nlet ok, err = pcall(f, 5)\nreturn "
                                      "ok, err\n";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    const std::string_view err = get_error();
    EXPECT_NE(err.find("attempt to index"), std::string_view::npos) << err;
    EXPECT_NE(err.find("<string>(2,"), std::string_view::npos) << err;
}

TEST_P(ErrorTest, LoopStartNotANumberRaises)
{
    constexpr std::string_view code = R"(
        let s = "a"
        let n = 0
        for (let i = s; i < 3; i = i + 1) { n = n + 1; if (n > 10) { break } }
        return n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 1));
}

TEST_P(ErrorTest, LoopLimitStringRaises)
{
    constexpr std::string_view code = R"(
        let lim = "3"
        let n = 0
        for (let i = 0; i < lim; i = i + 1) { n = n + 1; if (n > 10) { break } }
        return n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 1));
}

TEST_P(ErrorTest, LoopLimitNilRaises)
{
    constexpr std::string_view code = R"(
        let lim = nil
        let n = 0
        for (let i = 0; i < lim; i = i + 1) { n = n + 1; if (n > 10) { break } }
        return n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 1));
}

TEST_P(ErrorTest, DescendingLoopLimitTableRaises)
{
    constexpr std::string_view code = R"(
        let lim = {}
        let n = 0
        for (let i = 10; i > lim; i = i - 1) { n = n + 1; if (n > 20) { break } }
        return n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    EXPECT_TRUE(behl_test::call_fails(S, 0, 1));
}

static std::string runtime_error_of(behl::State* S, std::string_view code)
{
    if (!behl_test::load_ok(S, code, false))
    {
        return "<load failed: " + behl_test::error_text(S) + ">";
    }
    if (behl::call(S, 0, 0) >= 0)
    {
        return "<no error>";
    }
    return behl_test::error_text(S);
}

static void expect_error_on_line(behl::State* S, std::string_view code, int line)
{
    const std::string err = runtime_error_of(S, code);
    std::string expected = "<string>(";
    expected += std::to_string(line);
    expected += ',';
    EXPECT_NE(err.find(expected), std::string::npos) << "expected line " << line << ", got: " << err;
    behl::set_top(S, 0);
}

TEST_P(ErrorTest, LocationOfMemberReadInIfCondition)
{
    expect_error_on_line(S, "let n = 5\nlet x = 1\nif (n.x) { }\n", 3);
}

TEST_P(ErrorTest, LocationOfMemberReadInWhileCondition)
{
    expect_error_on_line(S, "let n = 5\nlet x = 1\nwhile (n.x) { }\n", 3);
}

TEST_P(ErrorTest, LocationOfUnaryOperatorAssignedToGlobal)
{
    expect_error_on_line(S, "let n = {}\nlet x = 1\ng = -n\n", 3);
}

TEST_P(ErrorTest, LocationOfMemberReadAssignedToGlobal)
{
    expect_error_on_line(S, "let n = 5\nlet x = 1\ng = n.x\n", 3);
}

TEST_P(ErrorTest, LocationOfGlobalIncrement)
{
    expect_error_on_line(S, "g = {}\nlet x = 1\ng++\n", 3);
}

TEST_P(ErrorTest, LocationOfGlobalCompoundAssignment)
{
    expect_error_on_line(S, "g = {}\nlet x = 1\ng += 1\n", 3);
}

TEST_P(ErrorTest, LocationOfMemberReadAssignedToUpvalue)
{
    expect_error_on_line(S, "let n = 5\nlet u = 0\nfunction f() {\nlet x = 1\nu = n.x\n}\nf()\n", 5);
}

TEST_P(ErrorTest, LocationOfUpvalueIncrement)
{
    expect_error_on_line(S, "let u = {}\nfunction f() {\nlet x = 1\nu++\n}\nf()\n", 4);
}

TEST_P(ErrorTest, LocationOfNumericForBoundCompare)
{
    expect_error_on_line(S, "let n = {}\nlet x = 1\nfor (let i = 0; i < n; i = i + 1) { }\n", 3);
}

TEST_P(ErrorTest, LocationOfForInOverNonIterable)
{
    expect_error_on_line(S, "let n = 5\nlet x = 1\nfor (let k, v in n) { }\n", 3);
}

TEST_P(ErrorTest, LocationOfForInUndeclaredVariableCompileError)
{
    ASSERT_TRUE(behl_test::load_fails(S, "let n = 5\nlet x = 1\nfor (k, v in n) { }\n", false));
    const std::string err = behl_test::error_text(S);
    EXPECT_NE(err.find("<string>(3,"), std::string::npos) << err;
}

TEST_P(ErrorTest, LocationOfMethodDefinitionOnNonTable)
{
    expect_error_on_line(S, "let n = 5\nlet x = 1\nfunction n.m() { }\n", 3);
}

TEST_P(ErrorTest, LocationOfOperandOnContinuationLine)
{
    expect_error_on_line(S, "let n = {}\nlet x = 1\nlet y =\n    -n\n", 4);
}

TEST_P(ErrorTest, LocationOfTableConstructorFieldOnItsOwnLine)
{
    expect_error_on_line(S, "let n = 5\nlet t = {\n    a = 1,\n    b = n.x\n}\n", 4);
}

struct LocationCase
{
    const char* name;
    const char* statement;
    int line;
};

static constexpr std::string_view kLocationPrelude = "let n = 5\nlet t = {}\nlet s = \"str\"\nlet x = 1\n";

static constexpr LocationCase kLocationCases[] = {
    { "member read in local decl", "let a = n.x", 5 },
    { "index read constant key", "let a = n[1]", 5 },
    { "index read register key", "let a = n[x]", 5 },
    { "member read in local assign", "x = n.x", 5 },
    { "member read in global assign", "g = n.x", 5 },
    { "member read as call argument", "print(n.x)", 5 },
    { "member read in return", "return n.x", 5 },
    { "nested member read on nil", "let a = t.a.b", 5 },
    { "member write", "n.x = 1", 5 },
    { "index write constant key", "n[1] = 1", 5 },
    { "index write register key", "n[x] = 1", 5 },
    { "member write through nil", "t.a.b = 1", 5 },
    { "call non-function statement", "n()", 5 },
    { "call non-function in local decl", "let a = n()", 5 },
    { "call missing field", "x = t.missing()", 5 },
    { "call non-function in global assign", "g = n(1)", 5 },
    { "call non-function in return", "return n()", 5 },
    { "method call on non-table", "n:m()", 5 },
    { "method call missing method", "t:m()", 5 },
    { "add", "let a = t + 1", 5 },
    { "sub", "let a = 1 - t", 5 },
    { "mul", "let a = t * 2", 5 },
    { "div", "let a = t / 2", 5 },
    { "mod", "let a = t % 2", 5 },
    { "pow", "let a = t ** 2", 5 },
    { "unary minus", "let a = -t", 5 },
    { "band", "let a = t & 1", 5 },
    { "bor", "let a = t | 1", 5 },
    { "bxor", "let a = t ^ 1", 5 },
    { "shl", "let a = t << 1", 5 },
    { "shr", "let a = t >> 1", 5 },
    { "bnot", "let a = ~t", 5 },
    { "less than", "let a = t < 1", 5 },
    { "less equal", "let a = t <= 1", 5 },
    { "greater than", "let a = t > 1", 5 },
    { "greater equal", "let a = t >= 1", 5 },
    { "concat", "let a = s + t", 5 },
    { "length", "let a = #n", 5 },
    { "if condition", "if (n.x) { }", 5 },
    { "if condition compare", "if (t < 1) { }", 5 },
    { "else if condition", "if (false) { } else if (n.x) { }", 5 },
    { "while condition", "while (n.x) { }", 5 },
    { "for condition", "for (let i = 0; i < t; i = i + 1) { }", 5 },
    { "for step", "for (let i = 0; i < 3; i = i + t) { }", 5 },
    { "for-in over number", "for (let k, v in n) { }", 5 },
    { "ternary false branch", "let a = false ? 0 : n.x", 5 },
    { "ternary true branch", "let a = x ? n.x : 0", 5 },
    { "and right operand", "let a = x && n.x", 5 },
    { "or right operand", "let a = nil || n.x", 5 },
    { "array constructor field", "let a = { n.x }", 5 },
    { "hash constructor field", "let a = { k = n.x }", 5 },
    { "compound add local", "x += t", 5 },
    { "compound add field nil", "t.c += 1", 5 },
    { "compound add field on non-table", "n.c += 1", 5 },
    { "increment field nil", "t.c++", 5 },
    { "increment field on non-table", "n.c++", 5 },
    { "increment nil global", "g3++", 5 },
    { "decrement local table", "t--", 5 },
    { "function body", "function f() {\nlet a = n.x\n}\nf()", 6 },
    { "defer body", "function f() {\ndefer {\nlet a = n.x\n}\n}\nf()", 7 },
    { "call argument on next line", "print(\n    n.x\n)", 6 },
    { "constructor field on next line", "let a = {\n    k = n.x\n}", 6 },
    { "call on next line of chain", "let a = t\n    .missing()", 6 },
    { "second statement on same line", "let y = 1; let a = n.x", 5 },
};

TEST_P(ErrorTest, LocationMatrix)
{
    for (const LocationCase& c : kLocationCases)
    {
        const std::string code = std::string(kLocationPrelude) + c.statement + "\n";
        const std::string err = runtime_error_of(S, code);
        std::string expected = "<string>(";
        expected += std::to_string(c.line);
        expected += ',';
        EXPECT_NE(err.find(expected), std::string::npos)
            << "[" << c.name << "] expected line " << c.line << ", got: " << err.substr(0, err.find('\n'));
        behl::set_top(S, 0);
    }
}

INSTANTIATE_TEST_SUITE_P(Mode, ErrorTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });