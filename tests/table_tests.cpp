#include <behl/behl.hpp>
#include <gtest/gtest.h>

class TableTest : public ::testing::Test
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(TableTest, ExecuteTableLengthZeroIndex)
{
    constexpr std::string_view code = R"(
        let t = {[0]=1, [1]=2, [2]=3}
        return #t
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_F(TableTest, ExecuteTableWithHole)
{
    constexpr std::string_view code = R"(
        let t = {[0]=1, [2]=3}
        return #t
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_F(TableTest, SparseIntegerKeyUsesHash)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[1] = "one"
        t[2] = "two"
        t[1000] = "thousand"  // Should go to hash (too sparse)
        return t[0], t[1], t[2], t[1000]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_string(S, -4), "zero");
    ASSERT_EQ(behl::to_string(S, -3), "one");
    ASSERT_EQ(behl::to_string(S, -2), "two");
    ASSERT_EQ(behl::to_string(S, -1), "thousand");
}

TEST_F(TableTest, VeryLargeSparseIndex)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "first"
        t[99999999] = "sparse"  // Should NOT allocate huge array
        return t[0], t[99999999]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_string(S, -2), "first");
    ASSERT_EQ(behl::to_string(S, -1), "sparse");
}

TEST_F(TableTest, MixedArrayAndHashAccess)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 5));
    ASSERT_EQ(behl::get_top(S), 5);
    ASSERT_EQ(behl::to_string(S, -5), "a0");
    ASSERT_EQ(behl::to_string(S, -4), "a1");
    ASSERT_EQ(behl::to_string(S, -3), "a2");
    ASSERT_EQ(behl::to_string(S, -2), "hash");
    ASSERT_EQ(behl::to_string(S, -1), "sparse");
}

TEST_F(TableTest, NegativeIndexUsesHash)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[-1] = "negative"
        t[-100] = "very negative"
        return t[0], t[-1], t[-100]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "zero");
    ASSERT_EQ(behl::to_string(S, -2), "negative");
    ASSERT_EQ(behl::to_string(S, -1), "very negative");
}

TEST_F(TableTest, FloatIndexConvertedToInteger)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0.0] = "zero"
        t[1.0] = "one"
        t[2.5] = "hash"  // Not integer, goes to hash
        return t[0], t[1.0], t[2.5]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "zero");
    ASSERT_EQ(behl::to_string(S, -2), "one");
    ASSERT_EQ(behl::to_string(S, -1), "hash");
}

TEST_F(TableTest, SparseFloatIndexUsesHash)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[1000.0] = "sparse float"  // Integer-valued but sparse
        return t[0], t[1000.0], t[1000]  // Should all work
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "zero");
    ASSERT_EQ(behl::to_string(S, -2), "sparse float");
    ASSERT_EQ(behl::to_string(S, -1), "sparse float");
}

TEST_F(TableTest, OverwriteSparseKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[1000] = "first"
        t[1000] = "second"  // Overwrite in hash
        return t[1000]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_string(S, -1), "second");
}

TEST_F(TableTest, BoundaryAtGrowthLimit)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "zero"
        t[63] = "edge"    // Just within growth limit (0 + 64)
        t[64] = "boundary"  // Still within (0 + 64)
        t[65] = "over"    // Beyond growth limit, goes to hash
        return t[0], t[63], t[64], t[65]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_string(S, -4), "zero");
    ASSERT_EQ(behl::to_string(S, -3), "edge");
    ASSERT_EQ(behl::to_string(S, -2), "boundary");
    ASSERT_EQ(behl::to_string(S, -1), "over");
}

TEST_F(TableTest, AccessNonExistentSparseKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "exists"
        let x = t[99999]  // Should return nil, not crash
        return x == nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(TableTest, StringAndIntegerKeysDontCollide)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = "integer zero"
        t["0"] = "string zero"
        return t[0], t["0"]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_string(S, -2), "integer zero");
    ASSERT_EQ(behl::to_string(S, -1), "string zero");
}

TEST_F(TableTest, DenseArrayFollowedBySparseKey)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 90);
    ASSERT_EQ(behl::to_string(S, -1), "sparse");
}

TEST_F(TableTest, UnpackBasic)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let a, b, c = table.unpack({1, 2, 3})
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 1);
    ASSERT_EQ(behl::to_integer(S, -2), 2);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_F(TableTest, UnpackSingleElement)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let x = table.unpack({42})
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(TableTest, UnpackEmptyTable)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let result = table.unpack({})
        return result == nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(TableTest, UnpackWithRange)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {10, 20, 30, 40, 50}
        let a, b = table.unpack(t, 1, 2)
        return a, b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 20);
    ASSERT_EQ(behl::to_integer(S, -1), 30);
}

TEST_F(TableTest, UnpackWithStartOnly)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {10, 20, 30, 40}
        let a, b, c = table.unpack(t, 1)
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 20);
    ASSERT_EQ(behl::to_integer(S, -2), 30);
    ASSERT_EQ(behl::to_integer(S, -1), 40);
}

TEST_F(TableTest, UnpackMixedTypes)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let a, b, c, d = table.unpack({1, "hello", true, 3.14})
        return a, b, c, d
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), 1);
    ASSERT_EQ(behl::to_string(S, -3), "hello");
    ASSERT_TRUE(behl::to_boolean(S, -2));
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 3.14);
}

TEST_F(TableTest, UnpackInvalidRange)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {1, 2, 3}
        let result = table.unpack(t, 5, 2)  // end < start
        return result == nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(TableTest, UnpackZeroIndex)
{
    constexpr std::string_view code = R"(
        const table = import("table")
        let t = {"a", "b", "c"}  // 0-indexed: [0]="a", [1]="b", [2]="c"
        let a, b, c = table.unpack(t, 0, 2)
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "a");
    ASSERT_EQ(behl::to_string(S, -2), "b");
    ASSERT_EQ(behl::to_string(S, -1), "c");
}
