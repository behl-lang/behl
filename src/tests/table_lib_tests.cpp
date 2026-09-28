#include "gc/gco_table.hpp"
#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

static std::string g_printed;

static void capture_print(behl::State*, std::string_view msg)
{
    g_printed.append(msg);
}

static std::string strip_newline(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    {
        s.pop_back();
    }
    return s;
}

class TableLibTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        behl::load_stdlib(S);
        g_printed.clear();
        behl::set_print_handler(S, capture_print);
    }

    void TearDown() override
    {
        behl::close(S);
    }

    static std::string with_import(std::string_view code)
    {
        return std::string("const table = import(\"table\");\n") + std::string(code);
    }

    ::testing::AssertionResult run(std::string_view code, int32_t nresults)
    {
        const std::string full = with_import(code);
        auto loaded = behl_test::load_ok(S, full);
        if (!loaded)
        {
            return loaded;
        }
        return behl_test::call_ok(S, 0, nresults);
    }

    ::testing::AssertionResult run_fails(std::string_view code, std::string_view expected)
    {
        const std::string full = with_import(code);
        auto loaded = behl_test::load_ok(S, full);
        if (!loaded)
        {
            return loaded;
        }
        if (!behl_test::call_fails(S, 0, 0))
        {
            return ::testing::AssertionFailure() << "script succeeded but was expected to fail";
        }
        const std::string err = behl_test::error_text(S);
        if (err.find(expected) == std::string::npos)
        {
            return ::testing::AssertionFailure() << "error '" << err << "' does not contain '" << expected << "'";
        }
        return ::testing::AssertionSuccess();
    }

    std::string dump_of(std::string_view expr)
    {
        const std::string code = "return table.dump(" + std::string(expr) + ")";
        if (!run(code, 1) || !behl::is_string(S, -1))
        {
            ADD_FAILURE() << "dump failed for " << expr << ": " << behl_test::error_text(S);
            return {};
        }
        std::string out(behl::to_string(S, -1));
        behl::pop(S, 1);
        return out;
    }

    std::string run_string(std::string_view code)
    {
        if (!run(code, 1) || !behl::is_string(S, -1))
        {
            ADD_FAILURE() << "script failed: " << behl_test::error_text(S);
            return {};
        }
        std::string out(behl::to_string(S, -1));
        behl::pop(S, 1);
        return out;
    }
};

TEST_P(TableLibTest, ModuleExposesAllFunctions)
{
    ASSERT_TRUE(run(R"(
        let names = {"rawlen", "rawget", "rawset", "insert", "dump", "print", "unpack", "set_name"};
        let n = 0;
        for (i = 0; i < 8; i = i + 1) {
            if (typeof(table[names[i]]) == "cfunction" || typeof(table[names[i]]) == "function") {
                n = n + 1;
            }
        }
        return n;
    )",
        1));
    EXPECT_EQ(behl::to_integer(S, -1), 8);
}

TEST_P(TableLibTest, RawlenCountsArrayPart)
{
    ASSERT_TRUE(run("return table.rawlen({}), table.rawlen({1, 2, 3}), table.rawlen({[0] = 1, [2] = 3}), "
                    "table.rawlen({x = 1, y = 2})",
        4));
    EXPECT_EQ(behl::to_integer(S, -4), 0);
    EXPECT_EQ(behl::to_integer(S, -3), 3);
    EXPECT_EQ(behl::to_integer(S, -2), 1);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(TableLibTest, RawlenBypassesLen)
{
    ASSERT_TRUE(run(R"(
        let t = {1, 2, 3};
        setmetatable(t, {__len = function(self) { return 99; }});
        return table.rawlen(t), #t;
    )",
        2));
    EXPECT_EQ(behl::to_integer(S, -2), 3);
    EXPECT_EQ(behl::to_integer(S, -1), 99);
}

TEST_P(TableLibTest, RawlenMatchesGlobalRawlen)
{
    ASSERT_TRUE(run("let t = {5, 6, 7, 8}; t[10] = 1; return table.rawlen(t) == rawlen(t)", 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableLibTest, RawlenRejectsNonTable)
{
    EXPECT_TRUE(run_fails("return table.rawlen(5)", "bad argument #1 (expected table, string or buffer, got integer)"));
}

TEST_P(TableLibTest, RawlenOfStringIsByteLength)
{
    ASSERT_TRUE(run("return table.rawlen(\"hello\"), table.rawlen(\"\")", 2));
    EXPECT_EQ(behl::to_integer(S, -2), 5);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(TableLibTest, RawlenOfBufferIsByteLength)
{
    ASSERT_TRUE(run("const buffer = import(\"buffer\")\nreturn table.rawlen(buffer.create(12))", 1));
    EXPECT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(TableLibTest, RawlenRejectsMissingArgument)
{
    EXPECT_TRUE(run_fails("return table.rawlen()", "bad argument #1 (expected table, string or buffer, got nil)"));
}

TEST_P(TableLibTest, RawgetBypassesIndex)
{
    ASSERT_TRUE(run(R"(
        let t = {x = 1};
        setmetatable(t, {__index = function(self, k) { return "mm"; }});
        return table.rawget(t, "x"), table.rawget(t, "y"), t.y;
    )",
        3));
    EXPECT_EQ(behl::to_integer(S, -3), 1);
    EXPECT_TRUE(behl::is_nil(S, -2));
    EXPECT_EQ(behl::to_string(S, -1), "mm");
}

TEST_P(TableLibTest, RawgetBypassesIndexTable)
{
    ASSERT_TRUE(run(R"(
        let t = {};
        setmetatable(t, {__index = {y = 2}});
        return table.rawget(t, "y"), t.y;
    )",
        2));
    EXPECT_TRUE(behl::is_nil(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableLibTest, RawgetKeyEdgeCases)
{
    ASSERT_TRUE(run(R"(
        let t = {};
        t[0] = "zero";
        t[-1] = "neg";
        t[4294967296] = "big";
        t[9223372036854775807] = "max";
        t[-9223372036854775807 - 1] = "min";
        t["1"] = "str";
        t[1] = "one";
        return table.rawget(t, 0), table.rawget(t, -1), table.rawget(t, 4294967296),
            table.rawget(t, 9223372036854775807), table.rawget(t, -9223372036854775807 - 1),
            table.rawget(t, "1"), table.rawget(t, 1.0), table.rawget(t, 1.5);
    )",
        8));
    EXPECT_EQ(behl::to_string(S, -8), "zero");
    EXPECT_EQ(behl::to_string(S, -7), "neg");
    EXPECT_EQ(behl::to_string(S, -6), "big");
    EXPECT_EQ(behl::to_string(S, -5), "max");
    EXPECT_EQ(behl::to_string(S, -4), "min");
    EXPECT_EQ(behl::to_string(S, -3), "str");
    EXPECT_EQ(behl::to_string(S, -2), "one");
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(TableLibTest, RawgetNilKeyReturnsNil)
{
    ASSERT_TRUE(run("let t = {1}; return table.rawget(t, nil)", 1));
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(TableLibTest, RawgetRejectsNonTable)
{
    EXPECT_TRUE(run_fails("return table.rawget(1, 2)", "bad argument #1 (expected table, got integer)"));
}

TEST_P(TableLibTest, RawgetRejectsMissingKey)
{
    EXPECT_TRUE(run_fails("return table.rawget({})", "bad argument #2"));
}

TEST_P(TableLibTest, RawsetBypassesNewindex)
{
    ASSERT_TRUE(run(R"(
        let log = {};
        let t = {};
        setmetatable(t, {__newindex = function(self, k, v) { log[k] = v; }});
        table.rawset(t, "a", 1);
        t.b = 2;
        return table.rawget(t, "a"), log.a, table.rawget(t, "b"), log.b;
    )",
        4));
    EXPECT_EQ(behl::to_integer(S, -4), 1);
    EXPECT_TRUE(behl::is_nil(S, -3));
    EXPECT_TRUE(behl::is_nil(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableLibTest, RawsetReturnsTable)
{
    ASSERT_TRUE(run("let t = {}; return table.rawset(t, \"k\", 3) == t, t.k", 2));
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(TableLibTest, RawsetKeyEdgeCases)
{
    ASSERT_TRUE(run(R"(
        let t = {};
        table.rawset(t, 0, "a");
        table.rawset(t, 1.0, "b");
        table.rawset(t, -5, "c");
        table.rawset(t, 4294967296, "d");
        table.rawset(t, "0", "e");
        return t[0], t[1], t[-5], t[4294967296], t["0"], table.rawlen(t);
    )",
        6));
    EXPECT_EQ(behl::to_string(S, -6), "a");
    EXPECT_EQ(behl::to_string(S, -5), "b");
    EXPECT_EQ(behl::to_string(S, -4), "c");
    EXPECT_EQ(behl::to_string(S, -3), "d");
    EXPECT_EQ(behl::to_string(S, -2), "e");
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableLibTest, RawsetNilValueRemovesKey)
{
    ASSERT_TRUE(run(R"(
        let t = {a = 1, b = 2};
        table.rawset(t, "a", nil);
        let n = 0;
        for (let k, v in pairs(t)) {
            n = n + 1;
        }
        return n, t.a;
    )",
        2));
    EXPECT_EQ(behl::to_integer(S, -2), 1);
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(TableLibTest, RawsetRejectsNonTable)
{
    EXPECT_TRUE(run_fails("table.rawset(\"s\", 1, 2)", "bad argument #1 (expected table, got string)"));
}

TEST_P(TableLibTest, RawsetRejectsMissingValue)
{
    EXPECT_TRUE(run_fails("table.rawset({}, \"k\")", "bad argument #3"));
}

TEST_P(TableLibTest, RawsetRejectsNilKey)
{
    EXPECT_TRUE(run_fails("table.rawset({}, nil, 1)", "index is nil"));
}

TEST_P(TableLibTest, RawsetRejectsNanKey)
{
    EXPECT_TRUE(run_fails("let z = 0.0; table.rawset({}, z / z, 1)", "index is NaN"));
}

TEST_P(TableLibTest, InsertAppends)
{
    ASSERT_TRUE(run(R"(
        let t = {10, 20};
        table.insert(t, 30);
        let e = {};
        table.insert(e, "a");
        table.insert(e, "b");
        return table.rawlen(t), t[2], table.rawlen(e), e[0], e[1];
    )",
        5));
    EXPECT_EQ(behl::to_integer(S, -5), 3);
    EXPECT_EQ(behl::to_integer(S, -4), 30);
    EXPECT_EQ(behl::to_integer(S, -3), 2);
    EXPECT_EQ(behl::to_string(S, -2), "a");
    EXPECT_EQ(behl::to_string(S, -1), "b");
}

TEST_P(TableLibTest, InsertManyGrowsArray)
{
    ASSERT_TRUE(run(R"(
        let t = {};
        for (i = 0; i < 1000; i = i + 1) {
            table.insert(t, i * 2);
        }
        return table.rawlen(t), t[999], #t;
    )",
        3));
    EXPECT_EQ(behl::to_integer(S, -3), 1000);
    EXPECT_EQ(behl::to_integer(S, -2), 1998);
    EXPECT_EQ(behl::to_integer(S, -1), 1000);
}

TEST_P(TableLibTest, InsertFillsFirstHole)
{
    ASSERT_TRUE(run(R"(
        let t = {[0] = 1, [2] = 3};
        table.insert(t, 2);
        return table.rawlen(t), t[1], t[2];
    )",
        3));
    EXPECT_EQ(behl::to_integer(S, -3), 3);
    EXPECT_EQ(behl::to_integer(S, -2), 2);
    EXPECT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(TableLibTest, InsertAppendsBypassingNewindex)
{
    ASSERT_TRUE(run(R"(
        let hits = 0;
        let t = {};
        setmetatable(t, {__newindex = function(self, k, v) { hits = hits + 1; }});
        table.insert(t, 5);
        return table.rawlen(t), hits;
    )",
        2));
    EXPECT_EQ(behl::to_integer(S, -2), 1);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(TableLibTest, InsertAtFrontShifts)
{
    ASSERT_TRUE(run(R"(
        let t = {10, 20};
        table.insert(t, 0, "x");
        return table.rawlen(t), t[0], t[1], t[2];
    )",
        4));
    EXPECT_EQ(behl::to_integer(S, -4), 3);
    EXPECT_EQ(behl::to_string(S, -3), "x");
    EXPECT_EQ(behl::to_integer(S, -2), 10);
    EXPECT_EQ(behl::to_integer(S, -1), 20);
}

TEST_P(TableLibTest, InsertInMiddleShifts)
{
    ASSERT_TRUE(run(R"(
        let t = {10, 20, 30};
        table.insert(t, 1, "x");
        return table.rawlen(t), t[0], t[1], t[2], t[3];
    )",
        5));
    EXPECT_EQ(behl::to_integer(S, -5), 4);
    EXPECT_EQ(behl::to_integer(S, -4), 10);
    EXPECT_EQ(behl::to_string(S, -3), "x");
    EXPECT_EQ(behl::to_integer(S, -2), 20);
    EXPECT_EQ(behl::to_integer(S, -1), 30);
}

TEST_P(TableLibTest, InsertOnePastEndAppends)
{
    ASSERT_TRUE(run(R"(
        let t = {10, 20};
        table.insert(t, 2, "x");
        return table.rawlen(t), t[0], t[1], t[2];
    )",
        4));
    EXPECT_EQ(behl::to_integer(S, -4), 3);
    EXPECT_EQ(behl::to_integer(S, -3), 10);
    EXPECT_EQ(behl::to_integer(S, -2), 20);
    EXPECT_EQ(behl::to_string(S, -1), "x");
}

TEST_P(TableLibTest, InsertPositionOutOfRangeRaises)
{
    EXPECT_TRUE(run_fails("let t = {10, 20}; table.insert(t, 5, \"x\")", "out of bounds"));
}

TEST_P(TableLibTest, InsertNegativePositionRaises)
{
    EXPECT_TRUE(run_fails("let t = {10, 20}; table.insert(t, -1, \"x\")", "out of bounds"));
}

TEST_P(TableLibTest, InsertTooManyArgumentsRaises)
{
    EXPECT_TRUE(run_fails("let t = {}; table.insert(t, 0, 1, 2)", "wrong number of arguments"));
}

TEST_P(TableLibTest, InsertMissingValueRaises)
{
    EXPECT_TRUE(run_fails("let t = {}; table.insert(t)", "wrong number of arguments"));
}

TEST_P(TableLibTest, InsertMissingValueDoesNotGrowArray)
{
    ASSERT_TRUE(behl_test::load_ok(S, with_import("let t = {1}; pcall(function() { table.insert(t); }); return t")));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(behl::is_table(S, -1));
    EXPECT_EQ(S->stack[static_cast<size_t>(behl::get_top(S) - 1)].get_table()->array.size(), 1u);
}

TEST_P(TableLibTest, InsertRejectsNonTable)
{
    EXPECT_TRUE(run_fails("table.insert(5, 1)", "bad argument #1 (expected table, got integer)"));
    EXPECT_TRUE(run_fails("table.insert()", "bad argument #1 (expected table, got nil)"));
}

TEST_P(TableLibTest, UnpackEmptyReturnsNothing)
{
    ASSERT_TRUE(run("return table.unpack({})", behl::kMultRet));
    EXPECT_EQ(behl::get_top(S), 0);
}

TEST_P(TableLibTest, UnpackWholeArray)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30})", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 3);
    EXPECT_EQ(behl::to_integer(S, 0), 10);
    EXPECT_EQ(behl::to_integer(S, 1), 20);
    EXPECT_EQ(behl::to_integer(S, 2), 30);
}

TEST_P(TableLibTest, UnpackIntoLocals)
{
    ASSERT_TRUE(run("let a, b, c = table.unpack({1, 2, 3}); return a + b * 10 + c * 100", 1));
    EXPECT_EQ(behl::to_integer(S, -1), 321);
}

TEST_P(TableLibTest, UnpackExplicitStart)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30}, 1)", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 2);
    EXPECT_EQ(behl::to_integer(S, 0), 20);
    EXPECT_EQ(behl::to_integer(S, 1), 30);
}

TEST_P(TableLibTest, UnpackExplicitStartEnd)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30, 40}, 1, 2)", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 2);
    EXPECT_EQ(behl::to_integer(S, 0), 20);
    EXPECT_EQ(behl::to_integer(S, 1), 30);
}

TEST_P(TableLibTest, UnpackSingleElementRange)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30}, 2, 2)", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 1);
    EXPECT_EQ(behl::to_integer(S, 0), 30);
}

TEST_P(TableLibTest, UnpackStartAfterEndReturnsNothing)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30}, 2, 1)", behl::kMultRet));
    EXPECT_EQ(behl::get_top(S), 0);
}

TEST_P(TableLibTest, UnpackClampsBounds)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30}, -5, 100)", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 3);
    EXPECT_EQ(behl::to_integer(S, 0), 10);
    EXPECT_EQ(behl::to_integer(S, 2), 30);
}

TEST_P(TableLibTest, UnpackStartBeyondLengthReturnsNothing)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30}, 7)", behl::kMultRet));
    EXPECT_EQ(behl::get_top(S), 0);
}

TEST_P(TableLibTest, UnpackStopsAtHole)
{
    ASSERT_TRUE(run("return table.unpack({[0] = 1, [2] = 3})", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 1);
    EXPECT_EQ(behl::to_integer(S, 0), 1);
}

TEST_P(TableLibTest, UnpackExplicitEndClampedAtHole)
{
    ASSERT_TRUE(run("return table.unpack({[0] = 1, [2] = 3}, 0, 2)", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 1);
    EXPECT_EQ(behl::to_integer(S, 0), 1);
}

TEST_P(TableLibTest, UnpackIgnoresHashPartAndIndex)
{
    ASSERT_TRUE(run(R"(
        let t = {1, 2, x = 3};
        setmetatable(t, {__index = function(self, k) { return 100; }});
        return table.unpack(t);
    )",
        behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 2);
    EXPECT_EQ(behl::to_integer(S, 1), 2);
}

TEST_P(TableLibTest, UnpackIntegralFloatBounds)
{
    ASSERT_TRUE(run("return table.unpack({10, 20, 30}, 1.0, 1.0)", behl::kMultRet));
    ASSERT_EQ(behl::get_top(S), 1);
    EXPECT_EQ(behl::to_integer(S, 0), 20);
}

TEST_P(TableLibTest, UnpackNonIntegralBoundRaises)
{
    EXPECT_TRUE(run_fails("return table.unpack({10, 20, 30}, 1.5)", "bad argument #2"));
}

TEST_P(TableLibTest, UnpackStringBoundRaises)
{
    EXPECT_TRUE(run_fails("return table.unpack({10, 20, 30}, \"a\")", "bad argument #2"));
    EXPECT_TRUE(run_fails("return table.unpack({10, 20, 30}, 0, {})", "bad argument #3"));
}

TEST_P(TableLibTest, UnpackRejectsNonTable)
{
    EXPECT_TRUE(run_fails("return table.unpack(\"abc\")", "bad argument #1 (expected table, got string)"));
    EXPECT_TRUE(run_fails("return table.unpack()", "bad argument #1 (expected table, got nil)"));
}

TEST_P(TableLibTest, UnpackLargeRangeDoesNotCrash)
{
    constexpr behl::Integer kCount = 300000;
    ASSERT_TRUE(behl_test::load_ok(S, with_import(R"(
        let t = {};
        for (i = 0; i < 300000; i = i + 1) {
            t[i] = i;
        }
        return table.unpack(t);
    )")));
    const int32_t status = behl::call(S, 0, behl::kMultRet);
    if (status < 0)
    {
        EXPECT_FALSE(behl_test::error_text(S).empty());
        return;
    }
    ASSERT_EQ(behl::get_top(S), kCount);
    EXPECT_EQ(behl::to_integer(S, 0), 0);
    EXPECT_EQ(behl::to_integer(S, -1), kCount - 1);
}

TEST_P(TableLibTest, UnpackLargeRangeIntoCallDoesNotCrash)
{
    ASSERT_TRUE(behl_test::load_ok(S, with_import(R"(
        let t = {};
        for (i = 0; i < 100000; i = i + 1) {
            t[i] = 1;
        }
        let count = function(...) {
            let args = {...};
            return rawlen(args);
        };
        return count(table.unpack(t));
    )")));
    const int32_t status = behl::call(S, 0, 1);
    if (status < 0)
    {
        EXPECT_FALSE(behl_test::error_text(S).empty());
        return;
    }
    EXPECT_EQ(behl::to_integer(S, -1), 100000);
}

TEST_P(TableLibTest, DumpSequentialDefault)
{
    EXPECT_EQ(dump_of("{1, 2, 3}"), "{\n    1,\n    2,\n    3\n}");
}

TEST_P(TableLibTest, DumpSequentialCompact)
{
    EXPECT_EQ(dump_of("{1, 2, 3}, {compact = true}"), "{1, 2, 3}");
}

TEST_P(TableLibTest, DumpCustomIndent)
{
    EXPECT_EQ(dump_of("{1, 2}, {indent = 2}"), "{\n  1,\n  2\n}");
}

TEST_P(TableLibTest, DumpEmptyCompact)
{
    EXPECT_EQ(dump_of("{}, {compact = true}"), "{}");
}

TEST_P(TableLibTest, DumpScalarValues)
{
    EXPECT_EQ(dump_of("{\"x\", true, false, 1.5, -7}, {compact = true}"), "{\"x\", true, false, 1.5, -7}");
}

TEST_P(TableLibTest, DumpHashDefault)
{
    EXPECT_EQ(dump_of("{a = 1}"), "{\n    [\"a\"] = 1\n}");
}

TEST_P(TableLibTest, DumpHashCompact)
{
    EXPECT_EQ(dump_of("{a = 1}, {compact = true}"), "{[\"a\"] = 1}");
}

TEST_P(TableLibTest, DumpHashCompactTwoEntries)
{
    const std::string out = dump_of("{a = 1, b = 2}, {compact = true}");
    EXPECT_TRUE(out == "{[\"a\"] = 1, [\"b\"] = 2}" || out == "{[\"b\"] = 2, [\"a\"] = 1}") << out;
}

TEST_P(TableLibTest, DumpNonStringKeys)
{
    EXPECT_EQ(dump_of("{[true] = 1}, {compact = true}"), "{[true] = 1}");
    EXPECT_EQ(dump_of("{[-1] = \"n\"}, {compact = true}"), "{[-1] = \"n\"}");
    EXPECT_EQ(dump_of("{[2.5] = 0}, {compact = true}"), "{[2.5] = 0}");
}

TEST_P(TableLibTest, DumpNonSequentialIntegerKeys)
{
    const std::string out = dump_of("{[0] = \"a\", [2] = \"c\"}, {compact = true}");
    EXPECT_NE(out.find("[0] = \"a\""), std::string::npos) << out;
    EXPECT_NE(out.find("[2] = \"c\""), std::string::npos) << out;
}

TEST_P(TableLibTest, DumpMixedTable)
{
    const std::string out = dump_of("{10, x = 1}, {compact = true}");
    EXPECT_NE(out.find("[0] = 10"), std::string::npos) << out;
    EXPECT_NE(out.find("[\"x\"] = 1"), std::string::npos) << out;
    EXPECT_EQ(out.front(), '{');
    EXPECT_EQ(out.back(), '}');
}

TEST_P(TableLibTest, DumpNestedCompact)
{
    EXPECT_EQ(dump_of("{{1, 2}, {3}}, {compact = true}"), "{{1, 2}, {3}}");
}

TEST_P(TableLibTest, DumpNestedDefault)
{
    EXPECT_EQ(dump_of("{{1}}"), "{\n    {\n        1\n    }\n}");
}

TEST_P(TableLibTest, DumpSharedSubtableIsNotACycle)
{
    EXPECT_EQ(run_string("let s = {1}; return table.dump({s, s}, {compact = true})"), "{{1}, {1}}");
}

TEST_P(TableLibTest, DumpArrayCycleTerminates)
{
    EXPECT_EQ(run_string("let t = {}; t[0] = t; return table.dump(t, {compact = true})"), "{{...}}");
}

TEST_P(TableLibTest, DumpFieldCycleTerminates)
{
    EXPECT_EQ(run_string("let t = {}; t.self = t; return table.dump(t, {compact = true})"), "{[\"self\"] = {...}}");
}

TEST_P(TableLibTest, DumpCycleWithHugeMaxRecursionTerminates)
{
    EXPECT_EQ(
        run_string("let t = {}; t[0] = t; return table.dump(t, {compact = true, max_recursion = 1000000000})"), "{{...}}");
}

TEST_P(TableLibTest, DumpMutualCycleTerminates)
{
    EXPECT_EQ(run_string("let a = {}; let b = {a}; a[0] = b; return table.dump(a, {compact = true})"), "{{{...}}}");
}

TEST_P(TableLibTest, DumpMaxRecursion)
{
    EXPECT_EQ(dump_of("{{1}}, {compact = true, max_recursion = 1}"), "{{...}}");
    EXPECT_EQ(dump_of("{{{1}}}, {compact = true, max_recursion = 2}"), "{{{...}}}");
}

TEST_P(TableLibTest, DumpDeepNestingStopsAtDefaultLimit)
{
    const std::string out = run_string(R"(
        let root = {};
        let cur = root;
        for (i = 0; i < 150; i = i + 1) {
            let n = {};
            cur[0] = n;
            cur = n;
        }
        return table.dump(root, {compact = true});
    )");
    EXPECT_NE(out.find("{...}"), std::string::npos);
    EXPECT_EQ(out.find("{}"), std::string::npos);
}

TEST_P(TableLibTest, DumpFunctionValueIsNotShownAsNil)
{
    const std::string out = dump_of("{print}, {compact = true}");
    EXPECT_EQ(out.find("nil"), std::string::npos) << out;
}

TEST_P(TableLibTest, DumpNegativeIndentDoesNotThrow)
{
    ASSERT_TRUE(behl_test::load_ok(S, with_import("return table.dump({1, 2}, {indent = -4})")));
    int32_t status = 0;
    EXPECT_NO_THROW(status = behl::call(S, 0, 1));
    (void)status;
}

TEST_P(TableLibTest, DumpIgnoresNonTableOptions)
{
    EXPECT_EQ(dump_of("{1}, \"compact\""), "{\n    1\n}");
}

TEST_P(TableLibTest, DumpIgnoresWrongTypedOptionFields)
{
    EXPECT_EQ(dump_of("{1}, {compact = 1, indent = \"x\", max_recursion = true}"), "{\n    1\n}");
}

TEST_P(TableLibTest, DumpDoesNotModifyTable)
{
    ASSERT_TRUE(run(R"(
        let t = {1, 2, x = 3};
        table.dump(t);
        return table.rawlen(t), t.x, t[1];
    )",
        3));
    EXPECT_EQ(behl::to_integer(S, -3), 2);
    EXPECT_EQ(behl::to_integer(S, -2), 3);
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableLibTest, DumpRejectsNonTable)
{
    EXPECT_TRUE(run_fails("return table.dump(5)", "bad argument #1 (expected table, got integer)"));
    EXPECT_TRUE(run_fails("return table.dump()", "bad argument #1 (expected table, got nil)"));
}

TEST_P(TableLibTest, DumpNamedTableStillDumpsContents)
{
    EXPECT_EQ(run_string("let t = {1, 2}; table.set_name(t, \"Named\"); return table.dump(t, {compact = true})"), "{1, 2}");
}

TEST_P(TableLibTest, PrintWritesDumpThroughHandler)
{
    ASSERT_TRUE(run("table.print({1, 2}, {compact = true})", 0));
    EXPECT_EQ(strip_newline(g_printed), "{1, 2}");
}

TEST_P(TableLibTest, PrintMatchesDump)
{
    ASSERT_TRUE(run("let t = {1, {2}, x = \"y\"}; table.print(t); return table.dump(t)", 1));
    EXPECT_EQ(strip_newline(g_printed), std::string(behl::to_string(S, -1)));
}

TEST_P(TableLibTest, PrintReturnsNothing)
{
    ASSERT_TRUE(run("return table.print({})", behl::kMultRet));
    EXPECT_EQ(behl::get_top(S), 0);
}

TEST_P(TableLibTest, PrintCycleTerminates)
{
    ASSERT_TRUE(run("let t = {}; t[0] = t; table.print(t, {compact = true})", 0));
    EXPECT_EQ(strip_newline(g_printed), "{{...}}");
}

TEST_P(TableLibTest, PrintRejectsNonTable)
{
    EXPECT_TRUE(run_fails("table.print(\"x\")", "bad argument #1 (expected table, got string)"));
    EXPECT_TRUE(g_printed.empty());
}

TEST_P(TableLibTest, SetNameStoresName)
{
    ASSERT_TRUE(run("let t = {}; table.set_name(t, \"Inventory\"); return t", 1));
    ASSERT_TRUE(behl::is_table(S, -1));
    const auto* t = S->stack[static_cast<size_t>(behl::get_top(S) - 1)].get_table();
    EXPECT_EQ(t->get_name(), "Inventory");
}

TEST_P(TableLibTest, SetNameReturnsNothing)
{
    ASSERT_TRUE(run("return table.set_name({}, \"x\")", behl::kMultRet));
    EXPECT_EQ(behl::get_top(S), 0);
}

TEST_P(TableLibTest, SetNameRejectsNonString)
{
    EXPECT_TRUE(run_fails("table.set_name({}, 5)", "bad argument #2 (expected string, got integer)"));
    EXPECT_TRUE(run_fails("table.set_name({}, {})", "bad argument #2 (expected string, got table)"));
}

TEST_P(TableLibTest, SetNameRejectsMissingName)
{
    EXPECT_TRUE(run_fails("table.set_name({})", "bad argument #2"));
}

TEST_P(TableLibTest, SetNameRejectsNonTable)
{
    EXPECT_TRUE(run_fails("table.set_name(\"t\", \"x\")", "bad argument #1 (expected table, got string)"));
}

INSTANTIATE_TEST_SUITE_P(Mode, TableLibTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
