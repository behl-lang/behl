#include "gc/gco_table.hpp"
#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class TableTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S;

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

TEST_P(TableTest, ExecuteTableLengthZeroIndex)
{
    constexpr std::string_view code = R"(
        let t = {[0]=1, [1]=2, [2]=3}
        return #t
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(TableTest, ExecuteTableWithHole)
{
    constexpr std::string_view code = R"(
        let t = {[0]=1, [2]=3}
        return #t
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(TableTest, SparseIntegerKeyUsesHash)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[1] = "one"
        t[2] = "two"
        t[1000] = "thousand"  // Should go to hash (too sparse)
        return t[0], t[1], t[2], t[1000]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_string(S, -4), "zero");
    ASSERT_EQ(behl::to_string(S, -3), "one");
    ASSERT_EQ(behl::to_string(S, -2), "two");
    ASSERT_EQ(behl::to_string(S, -1), "thousand");
}

TEST_P(TableTest, VeryLargeSparseIndex)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "first"
        t[99999999] = "sparse"  // Should NOT allocate huge array
        return t[0], t[99999999]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_string(S, -2), "first");
    ASSERT_EQ(behl::to_string(S, -1), "sparse");
}

TEST_P(TableTest, MixedArrayAndHashAccess)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "a0"
        t[1] = "a1"
        t[2] = "a2"
        t["key"] = "hash"
        t[100] = "sparse"  // Hash due to sparseness
        return t[0], t[1], t[2], t["key"], t[100]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    ASSERT_EQ(behl::get_top(S), 5);
    ASSERT_EQ(behl::to_string(S, -5), "a0");
    ASSERT_EQ(behl::to_string(S, -4), "a1");
    ASSERT_EQ(behl::to_string(S, -3), "a2");
    ASSERT_EQ(behl::to_string(S, -2), "hash");
    ASSERT_EQ(behl::to_string(S, -1), "sparse");
}

TEST_P(TableTest, NegativeIndexUsesHash)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[-1] = "negative"
        t[-100] = "very negative"
        return t[0], t[-1], t[-100]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "zero");
    ASSERT_EQ(behl::to_string(S, -2), "negative");
    ASSERT_EQ(behl::to_string(S, -1), "very negative");
}

TEST_P(TableTest, FloatIndexConvertedToInteger)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0.0] = "zero"
        t[1.0] = "one"
        t[2.5] = "hash"  // Not integer, goes to hash
        return t[0], t[1.0], t[2.5]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "zero");
    ASSERT_EQ(behl::to_string(S, -2), "one");
    ASSERT_EQ(behl::to_string(S, -1), "hash");
}

TEST_P(TableTest, SparseFloatIndexUsesHash)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[1000.0] = "sparse float"  // Integer-valued but sparse
        return t[0], t[1000.0], t[1000]  // Should all work
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "zero");
    ASSERT_EQ(behl::to_string(S, -2), "sparse float");
    ASSERT_EQ(behl::to_string(S, -1), "sparse float");
}

TEST_P(TableTest, OverwriteSparseKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[1000] = "first"
        t[1000] = "second"  // Overwrite in hash
        return t[1000]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_string(S, -1), "second");
}

TEST_P(TableTest, BoundaryAtGrowthLimit)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[63] = "edge"    // Just within growth limit (0 + 64)
        t[64] = "boundary"  // Still within (0 + 64)
        t[65] = "over"    // Beyond growth limit, goes to hash
        return t[0], t[63], t[64], t[65]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_string(S, -4), "zero");
    ASSERT_EQ(behl::to_string(S, -3), "edge");
    ASSERT_EQ(behl::to_string(S, -2), "boundary");
    ASSERT_EQ(behl::to_string(S, -1), "over");
}

TEST_P(TableTest, AccessNonExistentSparseKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "exists"
        let x = t[99999]  // Should return nil, not crash
        return x == nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableTest, StringAndIntegerKeysDontCollide)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "integer zero"
        t["0"] = "string zero"
        return t[0], t["0"]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_string(S, -2), "integer zero");
    ASSERT_EQ(behl::to_string(S, -1), "string zero");
}

TEST_P(TableTest, DenseArrayFollowedBySparseKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        for (let i = 0; i < 10; i++) {
            t[i] = i * 2
        }
        t[10000] = "sparse"  // Should use hash
        let sum = 0
        for (let i = 0; i < 10; i++) {
            sum = sum + t[i]
        }
        return sum, t[10000]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 90);
    ASSERT_EQ(behl::to_string(S, -1), "sparse");
}

TEST_P(TableTest, UnpackBasic)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let a, b, c = table.unpack({1, 2, 3})
        return a, b, c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 1);
    ASSERT_EQ(behl::to_integer(S, -2), 2);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(TableTest, UnpackSingleElement)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let x = table.unpack({42})
        return x
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(TableTest, UnpackEmptyTable)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let result = table.unpack({})
        return result == nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableTest, UnpackWithRange)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {10, 20, 30, 40, 50}
        let a, b = table.unpack(t, 1, 2)
        return a, b
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 20);
    ASSERT_EQ(behl::to_integer(S, -1), 30);
}

TEST_P(TableTest, UnpackWithStartOnly)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {10, 20, 30, 40}
        let a, b, c = table.unpack(t, 1)
        return a, b, c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 20);
    ASSERT_EQ(behl::to_integer(S, -2), 30);
    ASSERT_EQ(behl::to_integer(S, -1), 40);
}

TEST_P(TableTest, UnpackMixedTypes)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let a, b, c, d = table.unpack({1, "hello", true, 3.14})
        return a, b, c, d
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), 1);
    ASSERT_EQ(behl::to_string(S, -3), "hello");
    ASSERT_TRUE(behl::to_boolean(S, -2));
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 3.14);
}

TEST_P(TableTest, UnpackInvalidRange)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {1, 2, 3}
        let result = table.unpack(t, 5, 2)  // end < start
        return result == nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableTest, UnpackZeroIndex)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {"a", "b", "c"}  // 0-indexed: [0]="a", [1]="b", [2]="c"
        let a, b, c = table.unpack(t, 0, 2)
        return a, b, c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "a");
    ASSERT_EQ(behl::to_string(S, -2), "b");
    ASSERT_EQ(behl::to_string(S, -1), "c");
}

TEST_P(TableTest, JitIntKeyTableReadsInBoundsAndMisses)
{
    constexpr std::string_view code = R"(
        function f(t, n) {
            let sum = 0
            for (let i = 0; i < n; i++) {
                sum = sum + t[i]
            }
            return sum + t[0] + t[3]
        }
        let t = {}
        for (let i = 0; i < 10; i++) { t[i] = i * 2 }
        let a = f(t, 10) + f(t, 10)
        let miss = t[10]
        let k = 12
        let miss_reg = t[k]
        let neg = -1
        t[neg] = 7
        let one = 1.0
        return a, miss, miss_reg, t[neg], t[one]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    ASSERT_EQ(behl::to_integer(S, -5), 2 * (90 + 0 + 6));
    ASSERT_TRUE(behl::is_nil(S, -4));
    ASSERT_TRUE(behl::is_nil(S, -3));
    ASSERT_EQ(behl::to_integer(S, -2), 7);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableTest, JitIntKeyTableReadOfNilOrMissingUsesIndexMetamethod)
{
    constexpr std::string_view code = R"(
        function get(t, k) { return t[k] }
        function get0(t) { return t[0] }
        function get2(t) { return t[2] }
        let t = {}
        t[0] = 1
        t[1] = 2
        t[2] = 3
        t[2] = nil
        setmetatable(t, { __index = function(tbl, key) { return 100 + key } })
        let r = 0
        for (let i = 0; i < 50; i++) {
            r = get(t, 0) + get(t, 2) + get(t, 5) + get0(t) + get2(t)
        }
        return r
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 1 + 102 + 105 + 1 + 102);
}

TEST_P(TableTest, JitIntKeyTableWritesInBoundsAppendAndRegisterKeys)
{
    constexpr std::string_view code = R"(
        function fill(t, n) {
            for (let i = 0; i < n; i++) {
                t[i] = i + 1
            }
        }
        function overwrite(t, n) {
            for (let i = 0; i < n; i++) {
                t[i] = t[i] * 10
            }
            t[0] = "s"
            t[1] = {v = 5}
        }
        let t = {}
        fill(t, 20)
        overwrite(t, 20)
        let sum = 0
        for (let i = 2; i < 20; i++) { sum = sum + t[i] }
        return sum, t[0], t[1].v, #t
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    ASSERT_EQ(behl::to_integer(S, -4), 10 * (210 - 1 - 2));
    ASSERT_EQ(behl::to_string(S, -3), "s");
    ASSERT_EQ(behl::to_integer(S, -2), 5);
    ASSERT_EQ(behl::to_integer(S, -1), 20);
}

TEST_P(TableTest, JitIntKeyTableWriteOfExistingSlotSkipsNewIndex)
{
    constexpr std::string_view code = R"(
        let calls = 0
        let t = {}
        t[0] = 1
        t[1] = 2
        setmetatable(t, { __newindex = function(tbl, k, v) { calls = calls + 1 } })
        function set(tbl, k, v) { tbl[k] = v }
        for (let i = 0; i < 20; i++) {
            set(t, 0, i)
            t[1] = i
            set(t, 5, i)
        }
        return calls, t[0], t[1], t[5]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    ASSERT_EQ(behl::to_integer(S, -4), 20);
    ASSERT_EQ(behl::to_integer(S, -3), 19);
    ASSERT_EQ(behl::to_integer(S, -2), 19);
    ASSERT_TRUE(behl::is_nil(S, -1));
}

TEST_P(TableTest, JitIntKeyTableReadIntoTableRegister)
{
    constexpr std::string_view code = R"(
        let inner = {}
        inner[0] = 42
        let outer = {}
        outer[0] = inner
        let t = outer
        t = t[0]
        t = t[0]
        let idx = 0
        let u = outer
        u = u[idx]
        return t, u[idx]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::to_integer(S, -2), 42);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(TableTest, JitIntKeyIndexOfNonTableRaises)
{
    constexpr std::string_view code = R"(
        function get(t) { return t[0] }
        function getk(t, k) { return t[k] }
        let t = {}
        t[0] = 1
        get(t)
        getk(t, 0)
        let ok1 = pcall(get, 5)
        let ok2 = pcall(getk, true, 0)
        return ok1, ok2
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_FALSE(behl::to_boolean(S, -2));
    ASSERT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(TableTest, ArrayGrowthOverExistingHashKeysKeepsThem)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[100] = "h"
        for (let i = 0; i < 40; i++) { t[i] = i }
        t[101] = "x"
        let n = 0
        let finished = true
        for (let k, v in pairs(t)) {
            n++
            if (n > 10000) { finished = false; break }
        }
        return t[100], t[101], finished, n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_string(S, -4), "h");
    EXPECT_EQ(behl::to_string(S, -3), "x");
    EXPECT_TRUE(behl::to_boolean(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(TableTest, RawlenCoversKeysFilledInReverseAndInsertAppendsAfterThem)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {}
        for (let i = 100; i >= 0; i--) { t[i] = i }
        let len_before = rawlen(t)
        table.insert(t, "new")
        let n = 0
        for (let k, v in pairs(t)) {
            n++
            if (n > 10000) { break }
        }
        return len_before, t[64], t[101], rawlen(t), n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    EXPECT_EQ(behl::to_integer(S, -5), 101);
    EXPECT_EQ(behl::to_integer(S, -4), 64);
    EXPECT_EQ(behl::to_string(S, -3), "new");
    EXPECT_EQ(behl::to_integer(S, -2), 102);
    EXPECT_EQ(behl::to_integer(S, -1), 102);
}

TEST_P(TableTest, PairsIteratorRejectsMissingIntegerKey)
{
    constexpr std::string_view code = R"(
        let t = {10, 20, 30}
        t["a"] = 1
        t["b"] = 2
        let f, s, k0 = pairs(t)
        let ok_missing = pcall(f, t, 99)
        let ok_negative = pcall(f, t, -5)
        return ok_missing, ok_negative
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(TableTest, ClearingKeysDuringPairsVisitsEachKeyOnce)
{
    constexpr std::string_view code = R"(
        let t = {}
        for (let i = 0; i < 20; i++) { t[i] = i }
        for (let i = 0; i < 50; i++) { t["k" + tostring(i)] = i }
        let visits = 0
        for (let k, v in pairs(t)) {
            visits++
            t[k] = nil
            if (visits > 10000) { break }
        }
        let left = 0
        let guard = 0
        for (let k, v in pairs(t)) {
            guard++
            if (v != nil) { left++ }
            if (guard > 10000) { break }
        }
        return visits, left
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 70);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(TableTest, UpdatingValuesDuringPairsVisitsEachKeyOnce)
{
    constexpr std::string_view code = R"(
        let t = {}
        for (let i = 0; i < 20; i++) { t[i] = i }
        for (let i = 0; i < 50; i++) { t["k" + tostring(i)] = i }
        let visits = 0
        for (let k, v in pairs(t)) {
            visits++
            t[k] = v * 2
            if (visits > 10000) { break }
        }
        let sum = 0
        let guard = 0
        for (let k, v in pairs(t)) {
            guard++
            sum = sum + v
            if (guard > 10000) { break }
        }
        return visits, sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 70);
    EXPECT_EQ(behl::to_integer(S, -1), 2 * (190 + 1225));
}

TEST_P(TableTest, RawlenOfConstructorsWithNils)
{
    constexpr std::string_view code = R"(
        return rawlen({1, 2, nil}), rawlen({nil}), rawlen({1, 2, 3, nil, nil}), rawlen({1, nil, 3}), rawlen({})
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    EXPECT_EQ(behl::to_integer(S, -5), 2);
    EXPECT_EQ(behl::to_integer(S, -4), 0);
    EXPECT_EQ(behl::to_integer(S, -3), 3);
    EXPECT_EQ(behl::to_integer(S, -2), 1);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(TableTest, RawlenOfDenseArraysUpToForty)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let bad = -1
        for (let n = 0; n <= 40; n++) {
            let a = {}
            for (let i = 0; i < n; i++) { a[i] = i }
            let b = {}
            for (let i = 0; i < n; i++) { table.insert(b, i) }
            let c = {}
            for (let i = n - 1; i >= 0; i--) { c[i] = i }
            if (rawlen(a) != n || rawlen(b) != n || rawlen(c) != n || #a != n) {
                bad = n
                break
            }
        }
        return bad
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), -1);
}

TEST_P(TableTest, GrowByAssigningAtRawlen)
{
    constexpr std::string_view code = R"(
        let g = {}
        let bad = -1
        for (let i = 0; i < 300; i++) {
            if (rawlen(g) != i) { bad = i; break }
            g[rawlen(g)] = i * 3
        }
        return bad, rawlen(g), g[299]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), -1);
    EXPECT_EQ(behl::to_integer(S, -2), 300);
    EXPECT_EQ(behl::to_integer(S, -1), 897);
}

TEST_P(TableTest, KeysOfEveryTypeRoundTrip)
{
    constexpr std::string_view code = R"(
        let t = {}
        let tk = {}
        let fn = function() { return 1 }
        let long = "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz"
        t[1.5] = "float"
        t[2.0] = "intfloat"
        t[-0.0] = "negzero"
        t[long] = "long"
        t[fn] = "func"
        t[true] = "true"
        t[false] = "false"
        t[tk] = "table"
        let built = "abcdefghijklmnopqrstuvwxyz0123456789" + "abcdefghijklmnopqrstuvwxyz"
        return t[1.5] + t[2] + t[0] + t[0.0] + t[built] + t[fn] + t[true] + t[false] + t[tk]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "floatintfloatnegzeronegzerolongfunctruefalsetable");
}

TEST_P(TableTest, PairsVisitsKeysOfEveryType)
{
    constexpr std::string_view code = R"(
        let t = {}
        let tk = {}
        let fn = function() { return 1 }
        let long = "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz"
        t[1.5] = "float"
        t[2.0] = "intfloat"
        t[-0.0] = "negzero"
        t[long] = "long"
        t[fn] = "func"
        t[true] = "true"
        t[tk] = "table"
        let seen = {}
        let n = 0
        for (let k, v in pairs(t)) {
            n++
            seen[v] = typeof(k)
            if (n > 100) { break }
        }
        return n, seen["float"] + "," + seen["intfloat"] + "," + seen["negzero"] + "," + seen["long"] + "," + seen["func"] + "," + seen["true"] + "," + seen["table"]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 7);
    EXPECT_EQ(behl::to_string(S, -1), "number,integer,integer,string,function,boolean,table");
}

TEST_P(TableTest, PairsVisitsFalseKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[false] = 1
        t["a"] = 2
        t[true] = 3
        let n = 0
        let sum = 0
        for (let k, v in pairs(t)) {
            n++
            sum = sum + v
            if (n > 100) { break }
        }
        return n, sum
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 3);
    EXPECT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(TableTest, CFunctionAsKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[print] = "c"
        t["x"] = "s"
        let n = 0
        for (let k, v in pairs(t)) {
            n++
            if (n > 100) { break }
        }
        return t[print], t[tostring], n
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_string(S, -3), "c");
    EXPECT_TRUE(behl::is_nil(S, -2));
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableTest, NineThousandHashEntriesIterateAndMatchLookups)
{
    constexpr std::string_view code = R"(
        let h = {}
        for (let i = 0; i < 9000; i++) { h["key" + tostring(i)] = i }
        let count = 0
        let consistent = true
        for (let k, v in pairs(h)) {
            count++
            if (h[k] != v || k != "key" + tostring(v)) { consistent = false }
            if (count > 20000) { break }
        }
        return count, consistent
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 9000);
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableTest, PairsWithoutArgumentOrOnNonTableRaises)
{
    constexpr std::string_view code = R"(
        let ok_none = pcall(pairs)
        let ok_number = pcall(pairs, 5)
        let ok_nil = pcall(pairs, nil)
        let ok_string = pcall(pairs, "str")
        return ok_none, ok_number, ok_nil, ok_string
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_FALSE(behl::to_boolean(S, -4));
    EXPECT_FALSE(behl::to_boolean(S, -3));
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(TableTest, PowerOfTwoAndNegativeIntegerKeys)
{
    constexpr std::string_view code = R"(
        let big = {}
        for (let i = 0; i <= 62; i++) {
            big[2 ** i] = i
            big[-(2 ** i)] = -i
        }
        big[-9223372036854775807 - 1] = "min"
        big[9223372036854775807] = "max"
        let count = 0
        for (let k, v in pairs(big)) {
            count++
            if (count > 1000) { break }
        }
        let all_found = true
        for (let i = 0; i <= 62; i++) {
            if (big[2 ** i] != i || big[-(2 ** i)] != -i) { all_found = false }
        }
        return count, all_found, big[-9223372036854775807 - 1], big[9223372036854775807]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 128);
    EXPECT_TRUE(behl::to_boolean(S, -3));
    EXPECT_EQ(behl::to_string(S, -2), "min");
    EXPECT_EQ(behl::to_string(S, -1), "max");
}

TEST_P(TableTest, TableInsertAppends)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {10, 20}
        table.insert(t, 30)
        return rawlen(t), t[2]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 3);
    EXPECT_EQ(behl::to_integer(S, -1), 30);
}

TEST_P(TableTest, TableInsertBeforeHoleKeepsSparseKey)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {10, 20}
        t[4] = "far"
        table.insert(t, "x")
        return rawlen(t), t[2], t[3], t[4]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 3);
    EXPECT_EQ(behl::to_string(S, -3), "x");
    EXPECT_TRUE(behl::is_nil(S, -2));
    EXPECT_EQ(behl::to_string(S, -1), "far");
}

TEST_P(TableTest, UnpackClampsOutOfRangeBounds)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let count = function(...) { return rawlen({...}) }
        let past_end = count(table.unpack({1, 2, 3}, 0, 100))
        let both = count(table.unpack({1, 2, 3}, -100, 100))
        let a, b, c = table.unpack({1, 2, 3}, -5, 1)
        return past_end, both, a, b, c
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
    EXPECT_EQ(behl::to_integer(S, -5), 3);
    EXPECT_EQ(behl::to_integer(S, -4), 3);
    EXPECT_EQ(behl::to_integer(S, -3), 1);
    EXPECT_EQ(behl::to_integer(S, -2), 2);
    EXPECT_TRUE(behl::is_nil(S, -1));
}

TEST_P(TableTest, UnpackStopsAtRawlenBeforeHole)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let count = function(...) { return rawlen({...}) }
        let t = {1, 2}
        t[5] = 5
        return count(table.unpack(t, 0, 5)), count(table.unpack(t))
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 2);
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableTest, UnpackAcceptsIntegralFloatBounds)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let count = function(...) { return rawlen({...}) }
        let a, b = table.unpack({10, 20, 30}, 1.0, 2.0)
        return a, b, count(table.unpack({10, 20, 30}, 1.0, 2.0))
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 20);
    EXPECT_EQ(behl::to_integer(S, -2), 30);
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(TableTest, ApiIntegerKeysBeyondThirtyTwoBitsDoNotAliasArraySlots)
{
    constexpr behl::Integer kBigKeys[] = { behl::Integer{ 1 } << 32, (behl::Integer{ 1 } << 32) + 1, behl::Integer{ 1 } << 40,
        behl::Integer{ 1 } << 62 };

    behl::table_new(S);
    behl::push_integer(S, 0);
    behl::push_string(S, "slot0");
    behl::table_rawset(S, 0);
    behl::push_integer(S, 1);
    behl::push_string(S, "slot1");
    behl::table_set(S, 0);

    for (const behl::Integer key : kBigKeys)
    {
        behl::push_integer(S, key);
        behl::push_integer(S, key);
        behl::table_rawset(S, 0);
    }
    behl::push_integer(S, behl::Integer{ 1 } << 50);
    behl::push_string(S, "via set");
    behl::table_set(S, 0);

    for (const behl::Integer key : kBigKeys)
    {
        behl::push_integer(S, key);
        behl::table_rawget(S, 0);
        EXPECT_EQ(behl::to_integer(S, -1), key);
        behl::pop(S, 1);

        behl::push_integer(S, key);
        behl::table_get(S, 0);
        EXPECT_EQ(behl::to_integer(S, -1), key);
        behl::pop(S, 1);
    }

    behl::push_integer(S, behl::Integer{ 1 } << 50);
    behl::table_rawget(S, 0);
    EXPECT_EQ(behl::to_string(S, -1), "via set");
    behl::pop(S, 1);

    behl::push_integer(S, 0);
    behl::table_rawget(S, 0);
    EXPECT_EQ(behl::to_string(S, -1), "slot0");
    behl::pop(S, 1);
    behl::push_integer(S, 1);
    behl::table_get(S, 0);
    EXPECT_EQ(behl::to_string(S, -1), "slot1");
    behl::pop(S, 1);

    EXPECT_LT(S->stack[0].get_table()->array.size(), 16u);
}

TEST_P(TableTest, ScriptIntegerKeysBeyondThirtyTwoBitsDoNotAliasArraySlots)
{
    constexpr std::string_view code = R"(
        let t = { "zero", "one" }
        t[4294967296] = "two32"
        t[4294967297] = "two32+1"
        t[4294967296.0] = "two32 float"
        return t[0], t[1], t[4294967296], t[4294967297]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code, false));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_string(S, -4), "zero");
    EXPECT_EQ(behl::to_string(S, -3), "one");
    EXPECT_EQ(behl::to_string(S, -2), "two32 float");
    EXPECT_EQ(behl::to_string(S, -1), "two32+1");
}

INSTANTIATE_TEST_SUITE_P(Mode, TableTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
