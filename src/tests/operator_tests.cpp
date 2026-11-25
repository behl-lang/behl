#include "state.hpp"
#include "test_helpers.hpp"

#include <array>
#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>

class OperatorTest : public ::testing::TestWithParam<bool>
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

TEST_P(OperatorTest, ExecuteArithmetic)
{
    constexpr std::string_view code = R"(
        return 10 + 5
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(OperatorTest, NumberAdditionStillWorks)
{
    constexpr std::string_view code = R"(
        return 1 + 2 + 3
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(OperatorTest, CompoundAssignmentPlusEquals)
{
    constexpr std::string_view code = R"(
        let x = 10
        x += 5
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(OperatorTest, CompoundAssignmentMinusEquals)
{
    constexpr std::string_view code = R"(
        let x = 20
        x -= 8
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(OperatorTest, CompoundAssignmentStarEquals)
{
    constexpr std::string_view code = R"(
        let x = 6
        x *= 7
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(OperatorTest, CompoundAssignmentSlashEquals)
{
    constexpr std::string_view code = R"(
        let x = 100
        x /= 5
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 20);
}

TEST_P(OperatorTest, CompoundAssignmentPercentEquals)
{
    constexpr std::string_view code = R"(
        let x = 17
        x %= 5
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(OperatorTest, IncrementOperator)
{
    constexpr std::string_view code = R"(
        let x = 5
        x++
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(OperatorTest, DecrementOperator)
{
    constexpr std::string_view code = R"(
        let x = 10
        x--
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 9);
}

TEST_P(OperatorTest, IncrementInLoop)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 5; i++) {
            sum += i
        }
        return sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(OperatorTest, CompoundAssignmentChained)
{
    constexpr std::string_view code = R"(
        let x = 10
        x += 5
        x *= 2
        x -= 6
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 24);
}

TEST_P(OperatorTest, IncrementGlobal)
{
    constexpr std::string_view code = R"(
        g = 5
        g++
        return g
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(OperatorTest, DecrementGlobal)
{
    constexpr std::string_view code = R"(
        g = 10
        g--
        return g
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 9);
}

TEST_P(OperatorTest, LogicalNotOperator)
{
    constexpr std::string_view code = R"(
        let a = true
        a = !a
        return a
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, LogicalNotOnFalse)
{
    constexpr std::string_view code = R"(
        return !false
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, LogicalNotOnNil)
{
    constexpr std::string_view code = R"(
        return !nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, LogicalNotOnNumber)
{
    constexpr std::string_view code = R"(
        return !0
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(OperatorTest, MultipleAssignmentIndexUsesValueBeforeAssignment)
{
    constexpr std::string_view code = R"(
        let a = {}
        let i = 0
        i, a[i] = 1, 99
        return i, a[0], a[1]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 1);
    EXPECT_EQ(behl::to_integer(S, -2), 99);
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(OperatorTest, MultipleAssignmentTableUsesValueBeforeAssignment)
{
    constexpr std::string_view code = R"(
        let a = {}
        let c = a
        a, a[0] = 5, 99
        return a, c[0]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 5);
    EXPECT_EQ(behl::to_integer(S, -1), 99);
}

TEST_P(OperatorTest, BitwiseNotOnIntegralFloatMatchesBinaryBitwise)
{
    constexpr std::string_view code = R"(
        let v = 3.0
        return 3.0 & -1, ~3.0, ~v
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 3);
    EXPECT_EQ(behl::to_integer(S, -2), -4);
    EXPECT_EQ(behl::to_integer(S, -1), -4);
}

TEST_P(OperatorTest, BitwisePrecedenceWithoutParentheses)
{
    constexpr std::string_view code = R"(
        let r = ""
        r = r + tostring(1 | 2 & 3) + ","
        r = r + tostring(1 | 6 ^ 3) + ","
        r = r + tostring(5 & 3 ^ 1) + ","
        r = r + tostring(1 << 2 + 1) + ","
        r = r + tostring(2 + 1 << 1) + ","
        r = r + tostring(6 & 3 == 2) + ","
        r = r + tostring(8 >> 1 | 1) + ","
        r = r + tostring(~0 & 5) + ","
        r = r + tostring(1 | 2 ^ 3 & 4)
        let a = 1
        let b = 2
        let c = 3
        let d = 4
        r = r + ";" + tostring(a | b & c) + "," + tostring(a | b ^ c & d) + "," + tostring(a << b + a) + "," + tostring(b * c >> a)
        return r
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "3,5,0,8,6,true,5,5,3;3,3,8,3");
}

TEST_P(OperatorTest, LogicalOperatorsAllCombinations)
{
    constexpr std::array<std::string_view, 6> values = { "nil", "false", "true", "0", "1", "'s'" };
    const auto truthy = [](std::string_view v) { return v != "nil" && v != "false"; };
    std::string code = "let bad = \"\"\n"
                       "function chk(ok, s) { if (ok != true) { bad = bad + s + \";\" } }\n";
    for (size_t i = 0; i < values.size(); ++i)
    {
        const std::string a(values[i]);
        const std::string na = truthy(a) ? "false" : "true";
        const std::string li = std::to_string(i);
        code += "{ let a = " + a + "; let r = 0\n";
        code += "  chk((!" + a + ") == " + na + ", \"not lit " + li + "\")\n";
        code += "  chk((!a) == " + na + ", \"not var " + li + "\")\n";
        code += "  if (!a) { r = true } else { r = false }; chk(r == " + na + ", \"if not var " + li + "\")\n";
        code += "  if (!" + a + ") { r = true } else { r = false }; chk(r == " + na + ", \"if not lit " + li + "\")\n";
        code += "}\n";
        for (size_t j = 0; j < values.size(); ++j)
        {
            const std::string b(values[j]);
            const std::string and_v = truthy(a) ? b : a;
            const std::string or_v = truthy(a) ? a : b;
            const std::string t_and = truthy(and_v) ? "true" : "false";
            const std::string t_or = truthy(or_v) ? "true" : "false";
            const std::string nt_and = truthy(and_v) ? "false" : "true";
            const std::string lab = li + " " + std::to_string(j);
            code += "{ let a = " + a + "; let b = " + b + "; let r = 0\n";
            code += "  chk((" + a + " && " + b + ") == " + and_v + ", \"and lit " + lab + "\")\n";
            code += "  chk((a && b) == " + and_v + ", \"and var " + lab + "\")\n";
            code += "  chk((" + a + " || " + b + ") == " + or_v + ", \"or lit " + lab + "\")\n";
            code += "  chk((a || b) == " + or_v + ", \"or var " + lab + "\")\n";
            code += "  chk((!(a && b)) == " + nt_and + ", \"not and var " + lab + "\")\n";
            code += "  if (" + a + " && " + b + ") { r = true } else { r = false }; chk(r == " + t_and + ", \"if and lit " + lab
                + "\")\n";
            code += "  if (a && b) { r = true } else { r = false }; chk(r == " + t_and + ", \"if and var " + lab + "\")\n";
            code += "  if (" + a + " || " + b + ") { r = true } else { r = false }; chk(r == " + t_or + ", \"if or lit " + lab
                + "\")\n";
            code += "  if (a || b) { r = true } else { r = false }; chk(r == " + t_or + ", \"if or var " + lab + "\")\n";
            code += "  r = 0; while (a && b) { r = true; break }; chk(r == (" + t_and + " ? true : 0), \"while and var " + lab
                + "\")\n";
            code += "  r = (a || b) ? 1 : 2; chk(r == (" + t_or + " ? 1 : 2), \"ternary or var " + lab + "\")\n";
            code += "}\n";
        }
    }
    code += "return bad\n";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "");
}

class CompoundTargetTest : public ::testing::TestWithParam<bool>
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

TEST_P(CompoundTargetTest, FieldWithEveryCompoundOperator)
{
    constexpr std::string_view code = R"(
        let t = { a = 10, b = 10, c = 10, d = 10, e = 10 }
        t.a += 3
        t.b -= 3
        t.c *= 3
        t.d /= 4
        t.e %= 3
        return t.a, t.b, t.c, t.d, t.e
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    EXPECT_EQ(behl::to_integer(S, -5), 13);
    EXPECT_EQ(behl::to_integer(S, -4), 7);
    EXPECT_EQ(behl::to_integer(S, -3), 30);
    EXPECT_EQ(behl::to_number(S, -2), 2.5);
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(CompoundTargetTest, IndexWithConstantStringAndRegisterKeys)
{
    constexpr std::string_view code = R"(
        let t = { 1, 2, 3 }
        t["name"] = 1
        t[1000] = 7
        let k = 2
        t[0] += 10
        t[k] *= 5
        t["name"] += 1
        t[1000] -= 2
        return t[0], t[2], t.name, t[1000]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 11);
    EXPECT_EQ(behl::to_integer(S, -3), 15);
    EXPECT_EQ(behl::to_integer(S, -2), 2);
    EXPECT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(CompoundTargetTest, NestedMemberChain)
{
    constexpr std::string_view code = R"(
        let a = { b = { c = 2 } }
        a.b.c *= 3
        a.b["c"] += 1
        a["b"].c -= 2
        return a.b.c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(CompoundTargetTest, IncrementAndDecrementOnFields)
{
    constexpr std::string_view code = R"(
        let t = { n = 0, 5, 5 }
        let a = { b = { c = 1 } }
        let k = 1
        t.n++
        t.n++
        t[0]--
        t[k]++
        a.b.c++
        a.b.c--
        a.b.c--
        return t.n, t[0], t[1], a.b.c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 2);
    EXPECT_EQ(behl::to_integer(S, -3), 4);
    EXPECT_EQ(behl::to_integer(S, -2), 6);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(CompoundTargetTest, TableAndKeyExpressionsAreEvaluatedOnce)
{
    constexpr std::string_view code = R"(
        let t = { x = 1 }
        let table_calls = 0
        let key_calls = 0
        function get() { table_calls = table_calls + 1; return t }
        function key() { key_calls = key_calls + 1; return "x" }
        get().x += 1
        t[key()] += 1
        get()[key()]++
        get()[key()] *= 10
        return t.x, table_calls, key_calls
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 40);
    EXPECT_EQ(behl::to_integer(S, -2), 3);
    EXPECT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(CompoundTargetTest, OldValueIsReadBeforeRightHandSide)
{
    constexpr std::string_view code = R"(
        let t = { x = 1, y = 3 }
        function bump() { t.x = 100; return 5 }
        t.x += bump()
        t.y += t.y
        return t.x, t.y
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 6);
    EXPECT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(CompoundTargetTest, StringConcatenationOnField)
{
    constexpr std::string_view code = R"(
        let t = { s = "a" }
        t.s += "b"
        t["s"] += "c"
        return t.s
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "abc");
}

TEST_P(CompoundTargetTest, MetamethodsSeeOneReadAndOneWrite)
{
    constexpr std::string_view code = R"(
        let store = { x = 10 }
        let reads = 0
        let writes = 0
        let p = setmetatable({}, {
            __index = function(self, k) { reads = reads + 1; return store[k] },
            __newindex = function(self, k, v) { writes = writes + 1; store[k] = v }
        })
        p.x += 5
        p.x++
        return store.x, reads, writes
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 16);
    EXPECT_EQ(behl::to_integer(S, -2), 2);
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(CompoundTargetTest, BufferElements)
{
    constexpr std::string_view code = R"(
        const buffer = import("buffer")
        let b = buffer.create(4)
        b[0] += 5
        b[1]++
        b[2]--
        b[3] = 250
        b[3] += 10
        return b[0], b[1], b[2], b[3]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 5);
    EXPECT_EQ(behl::to_integer(S, -3), 1);
    EXPECT_EQ(behl::to_integer(S, -2), 255);
    EXPECT_EQ(behl::to_integer(S, -1), 4);
}

TEST_P(CompoundTargetTest, FieldOfUpvalueTableInClosure)
{
    constexpr std::string_view code = R"(
        let t = { n = 0 }
        function f() { t.n += 2; t.n++ }
        f()
        f()
        return t.n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(CompoundTargetTest, FieldCompoundInHotLoop)
{
    constexpr std::string_view code = R"(
        let t = { sum = 0, count = 0 }
        let arr = { 0 }
        for (let i = 0; i < 2000; i = i + 1) {
            t.sum += i
            t.count++
            arr[0] -= 1
        }
        return t.sum, t.count, arr[0]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 1999000);
    EXPECT_EQ(behl::to_integer(S, -2), 2000);
    EXPECT_EQ(behl::to_integer(S, -1), -2000);
}

TEST_P(CompoundTargetTest, ErrorsOnInvalidFieldTargets)
{
    ASSERT_TRUE(behl_test::load_ok(S, "let t = {}\nt.missing += 1\n", false));
    ASSERT_TRUE(behl_test::call_fails(S, 0, 0));
    const std::string nil_err = behl_test::error_text(S);
    EXPECT_NE(nil_err.find("attempt to perform arithmetic"), std::string::npos) << nil_err;
    EXPECT_NE(nil_err.find("<string>(2,"), std::string::npos) << nil_err;
    behl::set_top(S, 0);

    ASSERT_TRUE(behl_test::load_ok(S, "let n = 5\nn.x++\n", false));
    ASSERT_TRUE(behl_test::call_fails(S, 0, 0));
    const std::string index_err = behl_test::error_text(S);
    EXPECT_NE(index_err.find("attempt to index a non-table value"), std::string::npos) << index_err;
    EXPECT_NE(index_err.find("<string>(2,"), std::string::npos) << index_err;
}

INSTANTIATE_TEST_SUITE_P(Mode, CompoundTargetTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });

INSTANTIATE_TEST_SUITE_P(Mode, OperatorTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
