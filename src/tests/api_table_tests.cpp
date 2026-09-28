#include "common/string.hpp"
#include "config_internal.hpp"
#include "gc/gco_table.hpp"
#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <set>
#include <string>
#include <vector>

using namespace behl;

class ApiTableTest : public ::testing::TestWithParam<bool>
{
protected:
    State* S = nullptr;

    void SetUp() override
    {
        S = new_state();
        S->jit_enabled = GetParam();
        load_stdlib(S);
        set_top(S, 0);
    }

    void TearDown() override
    {
        close(S);
    }

    GCTable* table_at(int32_t idx)
    {
        const int32_t abs_idx = idx >= 0 ? idx : get_top(S) + idx;
        return S->stack[static_cast<size_t>(abs_idx)].get_table();
    }

    ::testing::AssertionResult run_protected(CFunction f)
    {
        push_cfunction(S, f);
        return behl_test::call_ok(S, 0, 0);
    }

    ::testing::AssertionResult run_fails(CFunction f, std::string_view expected)
    {
        push_cfunction(S, f);
        if (!behl_test::call_fails(S, 0, 0))
        {
            return ::testing::AssertionFailure() << "call succeeded but was expected to fail";
        }
        const std::string err = behl_test::error_text(S);
        if (err.find(expected) == std::string::npos)
        {
            return ::testing::AssertionFailure() << "error '" << err << "' does not contain '" << expected << "'";
        }
        return ::testing::AssertionSuccess();
    }

    void rawset_int(int32_t idx, Integer k, Integer v)
    {
        push_integer(S, k);
        push_integer(S, v);
        table_rawset(S, idx < 0 ? idx - 2 : idx);
    }

    Integer rawget_int(int32_t idx, Integer k)
    {
        push_integer(S, k);
        table_rawget(S, idx < 0 ? idx - 1 : idx);
        const Integer out = is_integer(S, -1) ? to_integer(S, -1) : -999999;
        pop(S, 1);
        return out;
    }

    Integer rawlen_of(int32_t idx)
    {
        table_rawlen(S, idx);
        const Integer out = to_integer(S, -1);
        pop(S, 1);
        return out;
    }

    Integer len_of(int32_t idx)
    {
        table_len(S, idx);
        const Integer out = to_integer(S, -1);
        pop(S, 1);
        return out;
    }

    std::vector<Integer> collect_integer_keys(int32_t idx, size_t cap = 10000)
    {
        std::vector<Integer> keys;
        const int32_t abs_idx = idx >= 0 ? idx : get_top(S) + idx;
        push_nil(S);
        size_t guard = 0;
        while (table_next(S, abs_idx))
        {
            if (++guard > cap)
            {
                ADD_FAILURE() << "iteration did not terminate";
                pop(S, 2);
                break;
            }
            keys.push_back(is_integer(S, -2) ? to_integer(S, -2) : -999999);
            pop(S, 1);
        }
        return keys;
    }

    size_t count_entries(int32_t idx, size_t cap = 10000)
    {
        const int32_t abs_idx = idx >= 0 ? idx : get_top(S) + idx;
        size_t count = 0;
        push_nil(S);
        while (table_next(S, abs_idx))
        {
            if (++count > cap)
            {
                ADD_FAILURE() << "iteration did not terminate";
                pop(S, 2);
                break;
            }
            pop(S, 1);
        }
        return count;
    }
};

TEST_P(ApiTableTest, NewPushesEmptyTable)
{
    table_new(S);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(type(S, -1), Type::kTable);
    EXPECT_EQ(rawlen_of(-1), 0);
    EXPECT_EQ(count_entries(-1), 0u);
    EXPECT_EQ(get_top(S), 1);
    EXPECT_FALSE(table_at(-1)->has_name());
    EXPECT_EQ(table_at(-1)->metatable, nullptr);
}

TEST_P(ApiTableTest, NewCreatesDistinctTables)
{
    table_new(S);
    table_new(S);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_NE(table_at(0), table_at(1));
}

TEST_P(ApiTableTest, RawsetRawgetStackContractPositiveIndex)
{
    table_new(S);
    push_string(S, "key");
    push_integer(S, 7);
    table_rawset(S, 0);
    ASSERT_EQ(get_top(S), 1);

    push_string(S, "key");
    table_rawget(S, 0);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 7);
    EXPECT_EQ(type(S, 0), Type::kTable);
}

TEST_P(ApiTableTest, RawsetRawgetStackContractNegativeIndex)
{
    push_integer(S, 111);
    table_new(S);
    push_integer(S, 222);
    push_string(S, "k");
    push_string(S, "v");
    table_rawset(S, -4);
    ASSERT_EQ(get_top(S), 3);
    EXPECT_EQ(to_integer(S, 0), 111);
    EXPECT_EQ(to_integer(S, 2), 222);

    push_string(S, "k");
    table_rawget(S, -3);
    ASSERT_EQ(get_top(S), 4);
    EXPECT_EQ(to_string(S, -1), "v");
    pop(S, 1);

    table_rawgetfield(S, -2, "k");
    ASSERT_EQ(get_top(S), 4);
    EXPECT_EQ(to_string(S, -1), "v");
}

TEST_P(ApiTableTest, RawgetMissingKeyPushesNil)
{
    table_new(S);
    push_string(S, "nope");
    table_rawget(S, -2);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);
    push_integer(S, 0);
    table_rawget(S, -2);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, RawsetfieldRawgetfieldNegativeAndPositiveIndex)
{
    table_new(S);
    push_integer(S, 42);
    table_rawsetfield(S, -2, "answer");
    ASSERT_EQ(get_top(S), 1);
    push_integer(S, 43);
    table_rawsetfield(S, 0, "other");
    ASSERT_EQ(get_top(S), 1);

    table_rawgetfield(S, -1, "answer");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 42);
    table_rawgetfield(S, 0, "other");
    ASSERT_EQ(get_top(S), 3);
    EXPECT_EQ(to_integer(S, -1), 43);
    table_rawgetfield(S, -3, "absent");
    ASSERT_EQ(get_top(S), 4);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, SetGetWithoutMetatableMatchRaw)
{
    table_new(S);
    push_string(S, "a");
    push_integer(S, 1);
    table_set(S, -3);
    ASSERT_EQ(get_top(S), 1);
    push_integer(S, 2);
    table_setfield(S, -2, "b");
    ASSERT_EQ(get_top(S), 1);

    push_string(S, "a");
    table_get(S, -2);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 1);
    pop(S, 1);
    table_getfield(S, -1, "b");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 2);
    pop(S, 1);
    table_getfield(S, 0, "c");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, OverwriteExistingKey)
{
    table_new(S);
    push_integer(S, 1);
    table_rawsetfield(S, -2, "x");
    push_integer(S, 2);
    table_rawsetfield(S, -2, "x");
    table_rawgetfield(S, -1, "x");
    EXPECT_EQ(to_integer(S, -1), 2);
    pop(S, 1);
    EXPECT_EQ(count_entries(-1), 1u);
}

static int index_fn_returns_key_times_ten(State* L)
{
    if (!is_table(L, 0))
    {
        push_integer(L, -1);
        return 1;
    }
    push_integer(L, to_integer(L, 1) * 10);
    return 1;
}

TEST_P(ApiTableTest, GetUsesIndexFunction)
{
    table_new(S);
    rawset_int(-1, 0, 5);
    table_new(S);
    push_cfunction(S, index_fn_returns_key_times_ten);
    table_rawsetfield(S, -2, "__index");
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 1);

    push_integer(S, 0);
    table_get(S, -2);
    EXPECT_EQ(to_integer(S, -1), 5);
    pop(S, 1);

    push_integer(S, 7);
    table_get(S, -2);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 70);
    pop(S, 1);

    push_integer(S, 7);
    table_rawget(S, -2);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, GetfieldUsesIndexTable)
{
    table_new(S);
    table_new(S);
    table_new(S);
    push_string(S, "fallback");
    table_rawsetfield(S, -2, "name");
    table_rawsetfield(S, -2, "__index");
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 1);

    table_getfield(S, -1, "name");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_string(S, -1), "fallback");
    pop(S, 1);

    table_rawgetfield(S, -1, "name");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    table_getfield(S, -1, "missing");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, GetFollowsIndexChain)
{
    table_new(S);
    push_string(S, "deep");
    table_rawsetfield(S, -2, "v");

    table_new(S);
    table_new(S);
    dup(S, 0);
    table_rawsetfield(S, -2, "__index");
    metatable_set(S, -2);

    table_new(S);
    table_new(S);
    dup(S, 1);
    table_rawsetfield(S, -2, "__index");
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 3);

    table_getfield(S, 2, "v");
    ASSERT_EQ(get_top(S), 4);
    EXPECT_EQ(to_string(S, -1), "deep");
}

TEST_P(ApiTableTest, GetPrefersOwnValueOverIndex)
{
    table_new(S);
    push_integer(S, 1);
    table_rawsetfield(S, -2, "x");
    table_new(S);
    table_new(S);
    push_integer(S, 2);
    table_rawsetfield(S, -2, "x");
    table_rawsetfield(S, -2, "__index");
    metatable_set(S, -2);
    table_getfield(S, -1, "x");
    EXPECT_EQ(to_integer(S, -1), 1);
}

TEST_P(ApiTableTest, GetIndexLoopRaisesInsteadOfOverflowing)
{
    ASSERT_TRUE(run_fails(
        +[](State* L) -> int {
            table_new(L);
            table_new(L);
            dup(L, 0);
            table_rawsetfield(L, -2, "__index");
            metatable_set(L, 0);
            table_getfield(L, 0, "missing");
            return 0;
        },
        "'__index' chain too long"));
}

static int newindex_records(State* L)
{
    get_global(L, "__log");
    dup(L, 1);
    dup(L, 2);
    table_rawset(L, -3);
    return 0;
}

TEST_P(ApiTableTest, SetUsesNewindexFunctionForNewKeysOnly)
{
    table_new(S);
    set_global(S, "__log");

    table_new(S);
    push_integer(S, 1);
    table_rawsetfield(S, -2, "existing");
    table_new(S);
    push_cfunction(S, newindex_records);
    table_rawsetfield(S, -2, "__newindex");
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 1);

    push_integer(S, 5);
    table_setfield(S, -2, "fresh");
    ASSERT_EQ(get_top(S), 1);
    push_integer(S, 9);
    table_setfield(S, -2, "existing");
    ASSERT_EQ(get_top(S), 1);

    table_rawgetfield(S, -1, "fresh");
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);
    table_rawgetfield(S, -1, "existing");
    EXPECT_EQ(to_integer(S, -1), 9);
    pop(S, 1);

    get_global(S, "__log");
    table_rawgetfield(S, -1, "fresh");
    EXPECT_EQ(to_integer(S, -1), 5);
    pop(S, 1);
    table_rawgetfield(S, -1, "existing");
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, SetUsesNewindexTable)
{
    table_new(S);
    table_new(S);
    table_new(S);
    dup(S, 0);
    table_rawsetfield(S, -2, "__newindex");
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 2);

    push_string(S, "k");
    push_integer(S, 3);
    table_set(S, 1);
    ASSERT_EQ(get_top(S), 2);

    table_rawgetfield(S, 1, "k");
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);
    table_rawgetfield(S, 0, "k");
    EXPECT_EQ(to_integer(S, -1), 3);
}

TEST_P(ApiTableTest, RawsetBypassesNewindex)
{
    table_new(S);
    set_global(S, "__log");
    table_new(S);
    table_new(S);
    push_cfunction(S, newindex_records);
    table_rawsetfield(S, -2, "__newindex");
    metatable_set(S, -2);

    push_integer(S, 4);
    table_rawsetfield(S, -2, "direct");
    rawset_int(-1, 0, 8);
    ASSERT_EQ(get_top(S), 1);

    table_rawgetfield(S, -1, "direct");
    EXPECT_EQ(to_integer(S, -1), 4);
    pop(S, 1);
    EXPECT_EQ(rawget_int(-1, 0), 8);
    get_global(S, "__log");
    EXPECT_EQ(count_entries(-1), 0u);
}

TEST_P(ApiTableTest, SetNewindexLoopRaisesInsteadOfOverflowing)
{
    ASSERT_TRUE(run_fails(
        +[](State* L) -> int {
            table_new(L);
            table_new(L);
            dup(L, 0);
            table_rawsetfield(L, -2, "__newindex");
            metatable_set(L, 0);
            push_integer(L, 1);
            table_setfield(L, 0, "missing");
            return 0;
        },
        "'__newindex' chain too long"));
}

TEST_P(ApiTableTest, SetNilOnExistingKeyRemovesItFromIteration)
{
    table_new(S);
    push_integer(S, 1);
    table_rawsetfield(S, -2, "a");
    push_integer(S, 2);
    table_rawsetfield(S, -2, "b");
    push_nil(S);
    table_rawsetfield(S, -2, "a");

    size_t seen = 0;
    size_t nil_values = 0;
    push_nil(S);
    while (table_next(S, 0))
    {
        if (++seen > 10)
        {
            pop(S, 2);
            break;
        }
        if (is_nil(S, -1))
        {
            ++nil_values;
        }
        pop(S, 1);
    }
    EXPECT_EQ(seen, 1u);
    EXPECT_EQ(nil_values, 0u);
    EXPECT_EQ(get_top(S), 1);
}

static int len_fn_returns_99(State* L)
{
    push_integer(L, is_table(L, 0) ? 99 : -1);
    return 1;
}

TEST_P(ApiTableTest, LenRespectsLenRawlenDoesNot)
{
    table_new(S);
    rawset_int(-1, 0, 1);
    rawset_int(-1, 1, 2);
    EXPECT_EQ(len_of(-1), 2);
    table_new(S);
    push_cfunction(S, len_fn_returns_99);
    table_rawsetfield(S, -2, "__len");
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 1);

    table_len(S, -1);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 99);
    pop(S, 1);

    table_rawlen(S, 0);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 2);
}

TEST_P(ApiTableTest, LenWithFullStackPassesTableToLen)
{
    table_new(S);
    table_new(S);
    push_cfunction(S, len_fn_returns_99);
    table_rawsetfield(S, -2, "__len");
    metatable_set(S, -2);
    size_t guard = 0;
    while (S->stack.size() < S->stack.capacity() && guard < (size_t{ 1 } << 22))
    {
        push_nil(S);
        ++guard;
    }
    ASSERT_EQ(S->stack.size(), S->stack.capacity());
    table_len(S, 0);
    EXPECT_EQ(to_integer(S, -1), 99);
}

TEST_P(ApiTableTest, RawlenStopsAtFirstHole)
{
    table_new(S);
    rawset_int(-1, 0, 1);
    rawset_int(-1, 1, 2);
    rawset_int(-1, 3, 4);
    EXPECT_EQ(rawlen_of(-1), 2);
    EXPECT_EQ(len_of(-1), 2);
    rawset_int(-1, 2, 3);
    EXPECT_EQ(rawlen_of(-1), 4);
}

TEST_P(ApiTableTest, NextArrayOnlyInOrder)
{
    table_new(S);
    for (Integer i = 0; i < 5; ++i)
    {
        rawset_int(-1, i, i * 100);
    }
    push_nil(S);
    Integer expected = 0;
    while (table_next(S, 0))
    {
        ASSERT_LT(expected, 5);
        ASSERT_EQ(get_top(S), 3);
        EXPECT_EQ(type(S, -2), Type::kInteger);
        EXPECT_EQ(to_integer(S, -2), expected);
        EXPECT_EQ(to_integer(S, -1), expected * 100);
        pop(S, 1);
        ++expected;
    }
    EXPECT_EQ(expected, 5);
    EXPECT_EQ(get_top(S), 1);
}

TEST_P(ApiTableTest, NextHashOnly)
{
    table_new(S);
    const char* names[] = { "a", "b", "c", "d" };
    for (Integer i = 0; i < 4; ++i)
    {
        push_integer(S, i);
        table_rawsetfield(S, -2, names[i]);
    }
    std::set<std::string> seen;
    push_nil(S);
    size_t guard = 0;
    while (table_next(S, -2))
    {
        ASSERT_LT(++guard, 10u);
        ASSERT_EQ(type(S, -2), Type::kString);
        seen.insert(std::string(to_string(S, -2)));
        pop(S, 1);
    }
    EXPECT_EQ(seen.size(), 4u);
    EXPECT_EQ(get_top(S), 1);
}

TEST_P(ApiTableTest, NextMixedVisitsEachKeyOnce)
{
    table_new(S);
    rawset_int(-1, 0, 10);
    rawset_int(-1, 1, 11);
    rawset_int(-1, 1000, 12);
    rawset_int(-1, -5, 13);
    push_integer(S, 14);
    table_rawsetfield(S, -2, "s");
    push_nil(S);
    Integer sum = 0;
    size_t count = 0;
    while (table_next(S, 0))
    {
        ASSERT_LT(++count, 20u);
        sum += to_integer(S, -1);
        pop(S, 1);
    }
    EXPECT_EQ(count, 5u);
    EXPECT_EQ(sum, 10 + 11 + 12 + 13 + 14);
    EXPECT_EQ(get_top(S), 1);
}

TEST_P(ApiTableTest, RawnextMatchesNextAndIgnoresMetatable)
{
    table_new(S);
    rawset_int(-1, 0, 1);
    push_integer(S, 2);
    table_rawsetfield(S, -2, "x");
    table_new(S);
    table_new(S);
    push_integer(S, 3);
    table_rawsetfield(S, -2, "hidden");
    table_rawsetfield(S, -2, "__index");
    metatable_set(S, -2);

    size_t count = 0;
    push_nil(S);
    while (table_rawnext(S, 0))
    {
        ASSERT_LT(++count, 10u);
        pop(S, 1);
    }
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(get_top(S), 1);
}

TEST_P(ApiTableTest, NextSkipsArrayHoles)
{
    table_new(S);
    rawset_int(-1, 0, 1);
    rawset_int(-1, 5, 2);
    EXPECT_EQ(table_at(-1)->array.size(), 6u);
    const auto keys = collect_integer_keys(-1);
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], 0);
    EXPECT_EQ(keys[1], 5);
}

TEST_P(ApiTableTest, AssignExistingKeysDuringIteration)
{
    table_new(S);
    for (Integer i = 0; i < 4; ++i)
    {
        rawset_int(-1, i, i);
    }
    const char* names[] = { "a", "b", "c", "d", "e", "f" };
    for (Integer i = 0; i < 6; ++i)
    {
        push_integer(S, 100 + i);
        table_rawsetfield(S, -2, names[i]);
    }
    size_t count = 0;
    push_nil(S);
    while (table_next(S, 0))
    {
        ASSERT_LT(++count, 50u);
        dup(S, -2);
        push_integer(S, to_integer(S, -2) * 2);
        table_rawset(S, 0);
        pop(S, 1);
    }
    EXPECT_EQ(count, 10u);
    EXPECT_EQ(get_top(S), 1);
    EXPECT_EQ(rawget_int(0, 3), 6);
    table_rawgetfield(S, 0, "f");
    EXPECT_EQ(to_integer(S, -1), 210);
}

TEST_P(ApiTableTest, ClearKeysDuringIteration)
{
    table_new(S);
    const char* names[] = { "a", "b", "c", "d", "e", "f" };
    for (Integer i = 0; i < 6; ++i)
    {
        push_integer(S, i);
        table_rawsetfield(S, -2, names[i]);
    }
    size_t count = 0;
    push_nil(S);
    while (table_next(S, 0))
    {
        ASSERT_LT(++count, 50u);
        dup(S, -2);
        push_nil(S);
        table_rawset(S, 0);
        pop(S, 1);
    }
    EXPECT_EQ(count, 6u);
    EXPECT_EQ(count_entries(0), 0u);
}

TEST_P(ApiTableTest, NextInvalidStringKeyRaises)
{
    ASSERT_TRUE(run_fails(
        +[](State* L) -> int {
            table_new(L);
            push_integer(L, 1);
            table_rawsetfield(L, -2, "present");
            push_string(L, "missing");
            table_next(L, 0);
            return 0;
        },
        "invalid key to 'next'"));
}

TEST_P(ApiTableTest, NextInvalidIntegerKeyRaises)
{
    ASSERT_TRUE(run_fails(
        +[](State* L) -> int {
            table_new(L);
            push_integer(L, 0);
            push_integer(L, 1);
            table_rawset(L, 0);
            push_integer(L, 500);
            table_next(L, 0);
            return 0;
        },
        "invalid key to 'next'"));
}

TEST_P(ApiTableTest, NextOnNonTableReturnsFalse)
{
    push_integer(S, 5);
    push_nil(S);
    EXPECT_FALSE(table_next(S, 0));
    EXPECT_FALSE(table_rawnext(S, 0));
    push_nil(S);
    EXPECT_FALSE(table_next(S, 10));
}

TEST_P(ApiTableTest, IntegerKeyEdgeCases)
{
    table_new(S);
    const Integer keys[] = { 0, -1, -1000, Integer{ 1 } << 32, std::numeric_limits<Integer>::min(),
        std::numeric_limits<Integer>::max() };
    Integer v = 1;
    for (const Integer k : keys)
    {
        rawset_int(-1, k, v++);
    }
    v = 1;
    for (const Integer k : keys)
    {
        EXPECT_EQ(rawget_int(-1, k), v++) << k;
    }
    EXPECT_EQ(rawlen_of(-1), 1);
    EXPECT_LE(table_at(-1)->array.size(), 1u);
    EXPECT_EQ(count_entries(-1), 6u);
}

TEST_P(ApiTableTest, FloatKeyWithIntegerValueNormalisesInArray)
{
    table_new(S);
    push_number(S, 0.0);
    push_string(S, "zero");
    table_rawset(S, -3);
    push_number(S, 1.0);
    push_string(S, "one");
    table_rawset(S, -3);
    EXPECT_EQ(rawlen_of(-1), 2);
    push_integer(S, 1);
    table_rawget(S, -2);
    EXPECT_EQ(to_string(S, -1), "one");
    pop(S, 1);
    push_number(S, -0.0);
    table_rawget(S, -2);
    EXPECT_EQ(to_string(S, -1), "zero");
    pop(S, 1);
    const auto keys = collect_integer_keys(-1);
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[1], 1);
}

TEST_P(ApiTableTest, FloatKeyWithIntegerValueNormalisesInHash)
{
    table_new(S);
    push_number(S, 5000.0);
    push_string(S, "v");
    table_rawset(S, -3);
    push_integer(S, 5000);
    table_rawget(S, -2);
    EXPECT_EQ(to_string(S, -1), "v");
    pop(S, 1);
    push_integer(S, 5000);
    push_string(S, "w");
    table_rawset(S, -3);
    EXPECT_EQ(count_entries(-1), 1u);
    push_nil(S);
    ASSERT_TRUE(table_next(S, -2));
    EXPECT_EQ(type(S, -2), Type::kInteger);
    EXPECT_EQ(to_integer(S, -2), 5000);
    EXPECT_EQ(to_string(S, -1), "w");
}

TEST_P(ApiTableTest, NonIntegralFloatKeyIsDistinct)
{
    table_new(S);
    rawset_int(-1, 0, 1);
    push_number(S, 0.5);
    push_integer(S, 2);
    table_rawset(S, -3);
    EXPECT_EQ(rawget_int(-1, 0), 1);
    push_number(S, 0.5);
    table_rawget(S, -2);
    EXPECT_EQ(to_integer(S, -1), 2);
    pop(S, 1);
    EXPECT_EQ(rawlen_of(-1), 1);
    EXPECT_EQ(count_entries(-1), 2u);
}

TEST_P(ApiTableTest, IntegerAndStringKeysAreDistinct)
{
    table_new(S);
    rawset_int(-1, 1, 10);
    push_integer(S, 20);
    table_rawsetfield(S, -2, "1");
    EXPECT_EQ(rawget_int(-1, 1), 10);
    table_rawgetfield(S, -1, "1");
    EXPECT_EQ(to_integer(S, -1), 20);
    pop(S, 1);
    EXPECT_EQ(count_entries(-1), 2u);
}

TEST_P(ApiTableTest, BooleanAndTableKeys)
{
    table_new(S);
    push_boolean(S, true);
    push_integer(S, 1);
    table_rawset(S, -3);
    table_new(S);
    push_integer(S, 2);
    table_rawset(S, -3);
    EXPECT_EQ(count_entries(-1), 2u);
    push_boolean(S, true);
    table_rawget(S, -2);
    EXPECT_EQ(to_integer(S, -1), 1);
    pop(S, 1);
    push_boolean(S, false);
    table_rawget(S, -2);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, RawgetNilAndNanKeysReturnNil)
{
    table_new(S);
    push_nil(S);
    table_rawget(S, -2);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);
    push_number(S, std::numeric_limits<FP>::quiet_NaN());
    table_rawget(S, -2);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, RawsetNilKeyRaises)
{
    ASSERT_TRUE(run_fails(
        +[](State* L) -> int {
            table_new(L);
            push_nil(L);
            push_integer(L, 1);
            table_rawset(L, 0);
            return 0;
        },
        "index is nil"));
}

TEST_P(ApiTableTest, RawsetNanKeyRaises)
{
    ASSERT_TRUE(run_fails(
        +[](State* L) -> int {
            table_new(L);
            push_number(L, std::numeric_limits<FP>::quiet_NaN());
            push_integer(L, 1);
            table_rawset(L, 0);
            return 0;
        },
        "index is NaN"));
}

TEST_P(ApiTableTest, SetNilKeyRaises)
{
    ASSERT_TRUE(run_fails(
        +[](State* L) -> int {
            table_new(L);
            push_nil(L);
            push_integer(L, 1);
            table_set(L, 0);
            return 0;
        },
        "index is nil"));
}

TEST_P(ApiTableTest, IterationTerminatesAfterNilKeyAttempt)
{
    push_cfunction(
        S, +[](State* L) -> int {
            table_new(L);
            push_nil(L);
            push_integer(L, 1);
            table_rawset(L, 0);
            return 1;
        });
    const int32_t status = call(S, 0, 1);
    if (status < 0)
    {
        SUCCEED();
        return;
    }
    ASSERT_TRUE(is_table(S, -1));
    EXPECT_LE(count_entries(-1, 100), 0u);
}

TEST_P(ApiTableTest, ScriptNilKeyAssignmentRaises)
{
    ASSERT_TRUE(behl_test::load_ok(S, "let t = {}\nlet k = nil\nt[k] = 1\nreturn t"));
    ASSERT_TRUE(behl_test::call_fails(S, 0, 1));
    EXPECT_NE(behl_test::error_text(S).find("index is nil"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ApiTableTest, AppendAtRawlenGrowsArray)
{
    table_new(S);
    for (Integer i = 0; i < 200; ++i)
    {
        table_rawlen(S, -1);
        push_integer(S, i);
        table_rawset(S, -3);
    }
    EXPECT_EQ(rawlen_of(-1), 200);
    EXPECT_EQ(table_at(-1)->array.size(), 200u);
    EXPECT_EQ(table_at(-1)->hash.size(), 0u);
}

TEST_P(ApiTableTest, SetWithinGrowthLimitUsesArray)
{
    table_new(S);
    rawset_int(-1, static_cast<Integer>(kTableArrayGrowthLimit) - 1, 1);
    EXPECT_EQ(table_at(-1)->array.size(), kTableArrayGrowthLimit);
    EXPECT_EQ(table_at(-1)->hash.size(), 0u);
    EXPECT_EQ(rawlen_of(-1), 0);
    EXPECT_EQ(rawget_int(-1, static_cast<Integer>(kTableArrayGrowthLimit) - 1), 1);
}

TEST_P(ApiTableTest, SetPastGrowthLimitUsesHash)
{
    table_new(S);
    rawset_int(-1, static_cast<Integer>(kTableArrayGrowthLimit), 1);
    EXPECT_EQ(table_at(-1)->array.size(), 0u);
    EXPECT_EQ(table_at(-1)->hash.size(), 1u);
    EXPECT_EQ(rawget_int(-1, static_cast<Integer>(kTableArrayGrowthLimit)), 1);

    rawset_int(-1, 0, 2);
    rawset_int(-1, 1 + static_cast<Integer>(kTableArrayGrowthLimit), 3);
    EXPECT_EQ(table_at(-1)->array.size(), 1u);
    EXPECT_EQ(table_at(-1)->hash.size(), 2u);
}

TEST_P(ApiTableTest, FillingGapMigratesHashToArray)
{
    table_new(S);
    const Integer far = 100;
    rawset_int(-1, far, 1000);
    rawset_int(-1, far + 1, 1001);
    ASSERT_EQ(table_at(-1)->hash.size(), 2u);
    for (Integer i = 0; i < far; ++i)
    {
        rawset_int(-1, i, i);
    }
    EXPECT_EQ(table_at(-1)->hash.size(), 0u);
    EXPECT_EQ(table_at(-1)->array.size(), static_cast<size_t>(far + 2));
    EXPECT_EQ(rawlen_of(-1), far + 2);
    EXPECT_EQ(rawget_int(-1, far + 1), 1001);
    const auto keys = collect_integer_keys(-1);
    ASSERT_EQ(keys.size(), static_cast<size_t>(far + 2));
    for (size_t i = 0; i < keys.size(); ++i)
    {
        EXPECT_EQ(keys[i], static_cast<Integer>(i));
    }
}

TEST_P(ApiTableTest, ResizeMigratesHashKeysInsideNewRange)
{
    table_new(S);
    rawset_int(-1, 100, 7);
    rawset_int(-1, 63, 1);
    ASSERT_EQ(table_at(-1)->array.size(), 64u);
    ASSERT_EQ(table_at(-1)->hash.size(), 1u);
    rawset_int(-1, 120, 2);
    EXPECT_EQ(table_at(-1)->array.size(), 121u);
    EXPECT_EQ(table_at(-1)->hash.size(), 0u);
    EXPECT_EQ(rawget_int(-1, 100), 7);
    const auto keys = collect_integer_keys(-1);
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 63);
    EXPECT_EQ(keys[1], 100);
    EXPECT_EQ(keys[2], 120);
}

TEST_P(ApiTableTest, SetGrowthThresholdsMatchRawset)
{
    table_new(S);
    push_integer(S, static_cast<Integer>(kTableArrayGrowthLimit));
    push_integer(S, 1);
    table_set(S, -3);
    EXPECT_EQ(table_at(-1)->array.size(), 0u);
    push_integer(S, 0);
    push_integer(S, 1);
    table_set(S, -3);
    EXPECT_EQ(table_at(-1)->array.size(), 1u);
    EXPECT_EQ(rawlen_of(-1), 1);
}

TEST_P(ApiTableTest, MetatableGetWithoutMetatablePushesNil)
{
    table_new(S);
    EXPECT_FALSE(metatable_get(S, -1));
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);
    push_integer(S, 3);
    EXPECT_FALSE(metatable_get(S, -1));
    ASSERT_EQ(get_top(S), 3);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, MetatableSetGetOnTable)
{
    table_new(S);
    table_new(S);
    GCTable* mt = table_at(-1);
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_TRUE(metatable_get(S, -1));
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(table_at(-1), mt);
    pop(S, 1);

    push_nil(S);
    metatable_set(S, -2);
    ASSERT_EQ(get_top(S), 1);
    EXPECT_FALSE(metatable_get(S, -1));
    EXPECT_EQ(table_at(0)->metatable, nullptr);
}

TEST_P(ApiTableTest, MetatableSetGetOnUserdata)
{
    constexpr uint32_t kUid = make_uid("ApiTableTest.UD");
    ASSERT_NE(userdata_new(S, 8, kUid), nullptr);
    EXPECT_FALSE(metatable_get(S, -1));
    pop(S, 1);
    table_new(S);
    GCTable* mt = table_at(-1);
    metatable_set(S, 0);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_TRUE(metatable_get(S, 0));
    EXPECT_EQ(table_at(-1), mt);
    pop(S, 1);
    push_nil(S);
    metatable_set(S, 0);
    EXPECT_FALSE(metatable_get(S, 0));
    EXPECT_EQ(get_top(S), 2);
}

TEST_P(ApiTableTest, MetatableSetOnNonTablePopsAndIgnores)
{
    push_integer(S, 1);
    table_new(S);
    metatable_set(S, 0);
    ASSERT_EQ(get_top(S), 1);
    EXPECT_EQ(to_integer(S, 0), 1);
    table_new(S);
    metatable_set(S, 7);
    EXPECT_EQ(get_top(S), 1);
}

TEST_P(ApiTableTest, MetatableSetNonTableValueClears)
{
    table_new(S);
    table_new(S);
    metatable_set(S, 0);
    push_integer(S, 5);
    metatable_set(S, 0);
    ASSERT_EQ(get_top(S), 1);
    EXPECT_EQ(table_at(0)->metatable, nullptr);
}

TEST_P(ApiTableTest, MetatableNewAndFind)
{
    EXPECT_TRUE(metatable_new(S, "ApiTableTest.Type"));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_TRUE(is_table(S, -1));
    GCTable* first = table_at(-1);

    EXPECT_FALSE(metatable_new(S, "ApiTableTest.Type"));
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(table_at(-1), first);

    metatable_find(S, "ApiTableTest.Type");
    ASSERT_EQ(get_top(S), 3);
    EXPECT_EQ(table_at(-1), first);

    metatable_find(S, "ApiTableTest.Unknown");
    ASSERT_EQ(get_top(S), 4);
    EXPECT_TRUE(is_nil(S, -1));

    EXPECT_TRUE(metatable_new(S, "ApiTableTest.Other"));
    EXPECT_NE(table_at(-1), first);
}

TEST_P(ApiTableTest, MetatableRegistrySurvivesCollection)
{
    ASSERT_TRUE(metatable_new(S, "ApiTableTest.Kept"));
    push_integer(S, 77);
    table_rawsetfield(S, -2, "marker");
    pop(S, 1);
    gc_collect(S);
    gc_collect(S);
    metatable_find(S, "ApiTableTest.Kept");
    ASSERT_TRUE(is_table(S, -1));
    table_rawgetfield(S, -1, "marker");
    EXPECT_EQ(to_integer(S, -1), 77);
}

TEST_P(ApiTableTest, MetatableFromRegistryDrivesIndex)
{
    ASSERT_TRUE(metatable_new(S, "ApiTableTest.Proto"));
    table_new(S);
    push_integer(S, 5);
    table_rawsetfield(S, -2, "field");
    table_rawsetfield(S, -2, "__index");
    pop(S, 1);
    table_new(S);
    metatable_find(S, "ApiTableTest.Proto");
    metatable_set(S, -2);
    table_getfield(S, -1, "field");
    EXPECT_EQ(to_integer(S, -1), 5);
}

TEST_P(ApiTableTest, SetnameStoresName)
{
    table_new(S);
    table_setname(S, -1, "Inventory");
    ASSERT_EQ(get_top(S), 1);
    EXPECT_TRUE(table_at(0)->has_name());
    EXPECT_EQ(table_at(0)->get_name(), "Inventory");
    table_setname(S, 0, "Renamed");
    EXPECT_EQ(table_at(0)->get_name(), "Renamed");
    table_setname(S, 0, "");
    EXPECT_FALSE(table_at(0)->has_name());
}

TEST_P(ApiTableTest, SetnameTruncatesLongName)
{
    table_new(S);
    const std::string long_name(200, 'n');
    table_setname(S, -1, long_name);
    const std::string_view stored = table_at(-1)->get_name();
    EXPECT_FALSE(stored.empty());
    EXPECT_LE(stored.size(), sizeof(GCTable::internal_name));
    EXPECT_EQ(stored, std::string_view(long_name).substr(0, stored.size()));
}

TEST_P(ApiTableTest, SetnameOnNonTableOrOutOfRangeIsNoop)
{
    push_integer(S, 1);
    table_setname(S, 0, "x");
    table_setname(S, 5, "x");
    table_setname(S, -5, "x");
    EXPECT_EQ(get_top(S), 1);
    EXPECT_EQ(to_integer(S, 0), 1);
}

TEST_P(ApiTableTest, NonTableTargetsDoNotCrash)
{
    push_integer(S, 1);

    push_string(S, "k");
    table_rawget(S, 0);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    push_string(S, "k");
    table_get(S, 0);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    push_string(S, "k");
    push_integer(S, 2);
    table_rawset(S, 0);
    ASSERT_EQ(get_top(S), 1);

    push_string(S, "k");
    push_integer(S, 2);
    table_set(S, 0);
    ASSERT_EQ(get_top(S), 1);

    table_rawgetfield(S, 0, "k");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    table_getfield(S, 0, "k");
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    push_integer(S, 3);
    table_rawsetfield(S, 0, "k");
    ASSERT_EQ(get_top(S), 1);
    push_integer(S, 3);
    table_setfield(S, 0, "k");
    ASSERT_EQ(get_top(S), 1);

    table_rawlen(S, 0);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 0);
    pop(S, 1);
    table_len(S, 0);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 0);
    pop(S, 1);
    EXPECT_EQ(to_integer(S, 0), 1);
}

TEST_P(ApiTableTest, OutOfRangeIndicesDoNotCrash)
{
    table_new(S);

    push_string(S, "k");
    table_rawget(S, 9);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    push_string(S, "k");
    table_get(S, -9);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    push_string(S, "k");
    push_integer(S, 1);
    table_rawset(S, 9);
    ASSERT_EQ(get_top(S), 1);

    push_string(S, "k");
    push_integer(S, 1);
    table_set(S, -9);
    ASSERT_EQ(get_top(S), 1);

    table_rawlen(S, 9);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 0);
    pop(S, 1);
    table_len(S, -9);
    ASSERT_EQ(get_top(S), 2);
    EXPECT_EQ(to_integer(S, -1), 0);
    pop(S, 1);

    EXPECT_FALSE(metatable_get(S, 9));
    ASSERT_EQ(get_top(S), 2);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    table_new(S);
    metatable_set(S, 9);
    ASSERT_EQ(get_top(S), 1);
    EXPECT_EQ(count_entries(0), 0u);
}

TEST_P(ApiTableTest, TooFewStackValuesDoNotCrash)
{
    table_rawget(S, 0);
    EXPECT_EQ(get_top(S), 0);
    table_get(S, 0);
    EXPECT_EQ(get_top(S), 0);
    table_rawset(S, 0);
    EXPECT_EQ(get_top(S), 0);
    table_set(S, 0);
    EXPECT_EQ(get_top(S), 0);
    metatable_set(S, 0);
    EXPECT_EQ(get_top(S), 0);

    table_new(S);
    table_rawset(S, 0);
    EXPECT_EQ(get_top(S), 1);
    table_set(S, 0);
    EXPECT_EQ(get_top(S), 1);
    EXPECT_TRUE(is_table(S, 0));
}

TEST_P(ApiTableTest, IndexZeroInsideCFunctionIsFirstArgument)
{
    table_new(S);
    push_integer(S, 21);
    table_rawsetfield(S, -2, "v");
    push_cfunction(
        S, +[](State* L) -> int {
            table_rawgetfield(L, 0, "v");
            push_integer(L, to_integer(L, -1) * 2);
            table_rawsetfield(L, 0, "w");
            table_rawgetfield(L, 0, "w");
            return 1;
        });
    insert(S, 0);
    ASSERT_TRUE(behl_test::call_ok(S, 1, 1));
    EXPECT_EQ(to_integer(S, -1), 42);
}

TEST_P(ApiTableTest, TablesSurviveCollectionWhileOnStack)
{
    table_new(S);
    for (Integer i = 0; i < 100; ++i)
    {
        table_new(S);
        rawset_int(-1, 0, i);
        push_integer(S, i);
        insert(S, -2);
        table_rawset(S, 0);
    }
    gc_collect(S);
    gc_collect(S);
    push_integer(S, 57);
    table_rawget(S, 0);
    ASSERT_TRUE(is_table(S, -1));
    EXPECT_EQ(rawget_int(-1, 0), 57);
}

TEST_P(ApiTableTest, TableFromScriptReadableThroughApi)
{
    ASSERT_TRUE(behl_test::load_ok(S, "return {10, 20, 30, name = \"x\", [100] = 5}"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(is_table(S, -1));
    EXPECT_EQ(rawlen_of(-1), 3);
    EXPECT_EQ(rawget_int(-1, 2), 30);
    EXPECT_EQ(rawget_int(-1, 100), 5);
    table_rawgetfield(S, -1, "name");
    EXPECT_EQ(to_string(S, -1), "x");
    pop(S, 1);
    EXPECT_EQ(count_entries(-1), 5u);
}

TEST_P(ApiTableTest, TableFromApiReadableInScript)
{
    table_new(S);
    rawset_int(-1, 0, 4);
    rawset_int(-1, 1, 5);
    push_integer(S, 6);
    table_rawsetfield(S, -2, "k");
    set_global(S, "api_t");
    ASSERT_TRUE(behl_test::load_ok(S, "return api_t[0] + api_t[1] + api_t.k + #api_t"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 17);
}

static size_t count_dead_keys(const GCTable* t)
{
    size_t dead = 0;
    for (const auto& [key, value] : t->hash)
    {
        if (key.is_dead_key())
        {
            EXPECT_TRUE(value.is_nil());
            ++dead;
        }
    }
    return dead;
}

static std::vector<std::string> colliding_names(size_t count, size_t mask)
{
    std::vector<std::string> names;
    const size_t target = StringHash32{}(std::string_view("dead0")) & mask;
    names.emplace_back("dead0");
    for (int i = 1; names.size() < count && i < 100000; ++i)
    {
        std::string name = "dead" + std::to_string(i);
        if ((StringHash32{}(std::string_view(name)) & mask) == target)
        {
            names.push_back(std::move(name));
        }
    }
    return names;
}

TEST_P(ApiTableTest, ClearCurrentKeyWithFullGcBetweenStepsVisitsEachKeyOnce)
{
    constexpr Integer kCount = 40;
    table_new(S);
    for (Integer i = 0; i < kCount; ++i)
    {
        push_string(S, "key" + std::to_string(i));
        push_integer(S, i);
        table_rawset(S, 0);
        table_new(S);
        push_integer(S, 1000 + i);
        table_rawset(S, 0);
    }
    gc_collect(S);

    std::set<std::string> seen_strings;
    std::set<Integer> seen_tables;
    size_t steps = 0;
    push_nil(S);
    while (table_next(S, 0))
    {
        ASSERT_LT(++steps, 1000u);
        if (is_string(S, -2))
        {
            EXPECT_TRUE(seen_strings.insert(std::string(to_string(S, -2))).second);
        }
        else
        {
            ASSERT_TRUE(is_table(S, -2));
            EXPECT_TRUE(seen_tables.insert(to_integer(S, -1)).second);
        }
        pop(S, 1);
        dup(S, -1);
        push_nil(S);
        table_rawset(S, 0);
        gc_collect(S);
    }
    EXPECT_EQ(seen_strings.size(), static_cast<size_t>(kCount));
    EXPECT_EQ(seen_tables.size(), static_cast<size_t>(kCount));
    EXPECT_EQ(count_entries(0), 0u);
    EXPECT_EQ(get_top(S), 1);
}

TEST_P(ApiTableTest, ClearCurrentKeyWithIncrementalStepsVisitsEachKeyOnce)
{
    constexpr Integer kCount = 60;
    table_new(S);
    for (Integer i = 0; i < kCount; ++i)
    {
        push_string(S, "inc" + std::to_string(i));
        push_integer(S, i);
        table_rawset(S, 0);
    }

    std::set<Integer> seen;
    size_t steps = 0;
    push_nil(S);
    while (table_next(S, 0))
    {
        ASSERT_LT(++steps, 1000u);
        EXPECT_TRUE(seen.insert(to_integer(S, -1)).second);
        pop(S, 1);
        dup(S, -1);
        push_nil(S);
        table_rawset(S, 0);
        for (int i = 0; i < 8; ++i)
        {
            gc_step(S);
        }
    }
    EXPECT_EQ(seen.size(), static_cast<size_t>(kCount));
    EXPECT_EQ(count_entries(0), 0u);
}

TEST_P(ApiTableTest, ClearOtherKeysWithGcDuringIterationSkipsThem)
{
    constexpr Integer kCount = 30;
    table_new(S);
    for (Integer i = 0; i < kCount; ++i)
    {
        push_string(S, "o" + std::to_string(i));
        push_integer(S, i);
        table_rawset(S, 0);
    }

    std::set<Integer> seen;
    size_t steps = 0;
    Integer first = -1;
    push_nil(S);
    while (table_next(S, 0))
    {
        ASSERT_LT(++steps, 1000u);
        const Integer v = to_integer(S, -1);
        EXPECT_TRUE(seen.insert(v).second);
        if (steps == 1)
        {
            first = v;
        }
        else
        {
            EXPECT_EQ(v % 2, 0) << v;
        }
        pop(S, 1);
        for (Integer i = 1; i < kCount; i += 2)
        {
            push_string(S, "o" + std::to_string(i));
            push_nil(S);
            table_rawset(S, 0);
        }
        gc_collect(S);
    }
    EXPECT_EQ(seen.size(), static_cast<size_t>(kCount / 2) + (first % 2 == 1 ? 1u : 0u));
}

TEST_P(ApiTableTest, ScriptPairsClearWithGcVisitsEachKeyOnce)
{
    ASSERT_TRUE(behl_test::load_ok(S, R"(
        const gc = import("gc");
        let t = {};
        for (i = 0; i < 50; i = i + 1) {
            t["s" + tostring(i)] = i;
            t[{}] = i;
        }
        gc.collect();
        let visits = 0;
        let sum = 0;
        for (let k, v in pairs(t)) {
            visits = visits + 1;
            sum = sum + v;
            t[k] = nil;
            gc.collect();
            gc.step();
        }
        let left = 0;
        for (let k, v in pairs(t)) {
            left = left + 1;
        }
        return visits, sum, left;
    )"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(to_integer(S, -3), 100);
    EXPECT_EQ(to_integer(S, -2), 2 * (49 * 50 / 2));
    EXPECT_EQ(to_integer(S, -1), 0);
}

static int g_dead_key_finalized = 0;

static int dead_key_finalizer(State*)
{
    ++g_dead_key_finalized;
    return 0;
}

TEST_P(ApiTableTest, NilEntryKeyBecomesCollectableWhileTableLives)
{
    constexpr uint32_t kUid = make_uid("ApiTableTest.DeadKey");
    g_dead_key_finalized = 0;
    table_new(S);

    table_new(S);
    push_cfunction(S, dead_key_finalizer);
    table_rawsetfield(S, -2, "__gc");
    const int32_t mt_idx = get_top(S) - 1;

    for (int i = 0; i < 2; ++i)
    {
        ASSERT_NE(userdata_new(S, 8, kUid), nullptr);
        dup(S, mt_idx);
        metatable_set(S, -2);
        push_integer(S, i);
        table_rawset(S, 0);
    }
    gc_collect(S);
    gc_collect(S);
    EXPECT_EQ(g_dead_key_finalized, 0);

    push_nil(S);
    ASSERT_TRUE(table_next(S, 0));
    pop(S, 1);
    push_nil(S);
    table_rawset(S, 0);

    gc_collect(S);
    gc_collect(S);
    EXPECT_EQ(g_dead_key_finalized, 1);
    EXPECT_EQ(count_dead_keys(table_at(0)), 1u);
    EXPECT_EQ(count_entries(0), 1u);
    EXPECT_EQ(table_at(0)->hash.size(), 2u);
}

TEST_P(ApiTableTest, StringKeyOfNilEntryIsCollected)
{
    table_new(S);
    push_string(S, "only_referenced_by_the_table");
    push_integer(S, 1);
    table_rawset(S, 0);
    gc_collect(S);
    gc_collect(S);
    const size_t before = S->gc.gc_all_objects.count();

    push_string(S, "only_referenced_by_the_table");
    push_nil(S);
    table_rawset(S, 0);
    gc_collect(S);
    gc_collect(S);

    EXPECT_EQ(count_dead_keys(table_at(0)), 1u);
    EXPECT_LT(S->gc.gc_all_objects.count(), before);
    push_string(S, "only_referenced_by_the_table");
    table_rawget(S, 0);
    EXPECT_TRUE(is_nil(S, -1));
}

TEST_P(ApiTableTest, NilEntriesDisappearOnRehash)
{
    table_new(S);
    const char* names[] = { "a", "b", "c", "d", "e" };
    for (const char* name : names)
    {
        push_integer(S, 1);
        table_rawsetfield(S, 0, name);
    }
    for (const char* name : names)
    {
        push_nil(S);
        table_rawsetfield(S, 0, name);
    }
    GCTable* t = table_at(0);
    EXPECT_EQ(t->hash.size(), 5u);
    const size_t capacity = t->hash.capacity();

    for (Integer i = 0; t->hash.size() >= 5u && i < 64; ++i)
    {
        push_integer(S, i);
        table_rawsetfield(S, 0, "n" + std::to_string(i));
    }
    EXPECT_LT(t->hash.size(), 5u);
    EXPECT_EQ(t->hash.capacity(), capacity);
    EXPECT_EQ(count_entries(0), t->hash.size());
    for (const auto& [key, value] : t->hash)
    {
        EXPECT_FALSE(value.is_nil());
        EXPECT_FALSE(key.is_dead_key());
    }
}

TEST_P(ApiTableTest, LookupsProbePastDeadKeys)
{
    const auto names = colliding_names(4, 7);
    ASSERT_EQ(names.size(), 4u);
    table_new(S);
    for (size_t i = 0; i < names.size(); ++i)
    {
        push_integer(S, static_cast<Integer>(i) + 1);
        table_rawsetfield(S, 0, names[i]);
    }
    ASSERT_EQ(table_at(0)->hash.capacity(), 8u);

    push_nil(S);
    table_rawsetfield(S, 0, names[0]);
    push_nil(S);
    table_rawsetfield(S, 0, names[2]);
    gc_collect(S);
    gc_collect(S);
    EXPECT_EQ(count_dead_keys(table_at(0)), 2u);

    table_rawgetfield(S, 0, names[1]);
    EXPECT_EQ(to_integer(S, -1), 2);
    pop(S, 1);
    table_rawgetfield(S, 0, names[3]);
    EXPECT_EQ(to_integer(S, -1), 4);
    pop(S, 1);
    table_rawgetfield(S, 0, names[0]);
    EXPECT_TRUE(is_nil(S, -1));
    pop(S, 1);

    push_integer(S, 30);
    table_rawsetfield(S, 0, names[3]);
    table_rawgetfield(S, 0, names[3]);
    EXPECT_EQ(to_integer(S, -1), 30);
    pop(S, 1);

    push_integer(S, 10);
    table_rawsetfield(S, 0, names[0]);
    table_rawgetfield(S, 0, names[0]);
    EXPECT_EQ(to_integer(S, -1), 10);
    pop(S, 1);
    EXPECT_EQ(count_entries(0), 3u);
}

TEST_P(ApiTableTest, NextResumesFromDeadKeyOnCollidingChain)
{
    const auto names = colliding_names(4, 7);
    ASSERT_EQ(names.size(), 4u);
    table_new(S);
    for (size_t i = 0; i < names.size(); ++i)
    {
        push_integer(S, static_cast<Integer>(i));
        table_rawsetfield(S, 0, names[i]);
    }

    std::set<Integer> seen;
    size_t steps = 0;
    push_nil(S);
    while (table_next(S, 0))
    {
        ASSERT_LT(++steps, 20u);
        EXPECT_TRUE(seen.insert(to_integer(S, -1)).second);
        pop(S, 1);
        dup(S, -1);
        push_nil(S);
        table_rawset(S, 0);
        gc_collect(S);
    }
    EXPECT_EQ(seen.size(), names.size());
}

TEST_P(ApiTableTest, NilAssignmentToAbsentKeyDoesNotGrowTable)
{
    table_new(S);
    push_nil(S);
    table_rawsetfield(S, 0, "absent");
    push_integer(S, 0);
    push_nil(S);
    table_rawset(S, 0);
    push_integer(S, 5000);
    push_nil(S);
    table_rawset(S, 0);
    EXPECT_EQ(table_at(0)->array.size(), 0u);
    EXPECT_EQ(table_at(0)->hash.size(), 0u);
}

TEST_P(ApiTableTest, ScriptNanKeyAssignmentRaises)
{
    ASSERT_TRUE(behl_test::load_ok(S, "let t = {}\nlet z = 0.0\nt[z / z] = 1\nreturn t"));
    ASSERT_TRUE(behl_test::call_fails(S, 0, 1));
    EXPECT_NE(behl_test::error_text(S).find("index is NaN"), std::string::npos) << behl_test::error_text(S);
}

TEST_P(ApiTableTest, ScriptNilAndNanKeyReadsReturnNil)
{
    ASSERT_TRUE(behl_test::load_ok(S, "let t = {1}\nlet z = 0.0\nlet k = nil\nreturn t[k] == nil && t[z / z] == nil"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(ApiTableTest, ScriptPairsSkipsNilEntries)
{
    ASSERT_TRUE(behl_test::load_ok(S, R"(
        let t = {a = 1, b = 2, c = 3};
        t.b = nil;
        let n = 0;
        for (let k, v in pairs(t)) {
            if (v == nil) { return -1; }
            n = n + 1;
        }
        return n;
    )"));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 2);
}

INSTANTIATE_TEST_SUITE_P(Mode, ApiTableTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
