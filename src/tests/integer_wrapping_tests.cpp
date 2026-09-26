#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include "test_helpers.hpp"

class IntegerWrappingTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
    }

    void TearDown() override
    {
        behl::close(S);
        S = nullptr;
    }
};

TEST_P(IntegerWrappingTest, AdditionOverflow)
{
    constexpr std::string_view code = R"(
        let max = 9223372036854775807  // INT64_MAX
        let result = max + 1
        return result
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), static_cast<int64_t>(0x8000000000000000ULL));
}

TEST_P(IntegerWrappingTest, SubtractionUnderflow)
{
    constexpr std::string_view code = R"(
        let min = -9223372036854775807 - 1  // Compute INT64_MIN at runtime
        let result = min - 1
        return result
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), 9223372036854775807LL);
}

TEST_P(IntegerWrappingTest, MultiplicationOverflow)
{
    constexpr std::string_view code = R"(
        let a = 9223372036854775807  // INT64_MAX
        let result = a * 2
        return result
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), -2);
}

TEST_P(IntegerWrappingTest, ConstantFoldedOverflowMatchesRuntime)
{
    constexpr std::string_view code = R"(
        function add(a, b) { return a + b }
        function sub(a, b) { return a - b }
        function mul(a, b) { return a * b }

        let folded_add = 9223372036854775807 + 1
        let folded_sub = (0 - 9223372036854775807 - 1) - 1
        let folded_mul = 9223372036854775807 * 2

        let runtime_add = add(9223372036854775807, 1)
        let runtime_sub = sub(0 - 9223372036854775807 - 1, 1)
        let runtime_mul = mul(9223372036854775807, 2)

        return folded_add, runtime_add, folded_sub, runtime_sub, folded_mul, runtime_mul
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 6));
    ASSERT_EQ(behl::get_top(S), 6);

    ASSERT_EQ(behl::type(S, -6), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -6), static_cast<int64_t>(0x8000000000000000ULL));
    ASSERT_EQ(behl::to_integer(S, -5), behl::to_integer(S, -6));

    ASSERT_EQ(behl::type(S, -4), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -4), 9223372036854775807LL);
    ASSERT_EQ(behl::to_integer(S, -3), behl::to_integer(S, -4));

    ASSERT_EQ(behl::type(S, -2), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -2), -2);
    ASSERT_EQ(behl::to_integer(S, -1), behl::to_integer(S, -2));
}

TEST_P(IntegerWrappingTest, ModuloOfMinByMinusOne)
{
    constexpr std::string_view code = R"(
        function imod(a, b) { return a % b }
        let min = 0 - 9223372036854775807 - 1
        let folded = (0 - 9223372036854775807 - 1) % (0 - 1)
        let runtime = imod(min, 0 - 1)
        let hot = 0
        for (let i = 0; i < 300; i++) { hot = imod(min, 0 - 1) }
        return folded, runtime, hot
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::type(S, -3), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -3), 0);
    ASSERT_EQ(behl::to_integer(S, -2), 0);
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(IntegerWrappingTest, PowerOverflow)
{
    constexpr std::string_view code = R"(
        let a = 2 ** 64
        let b = 3 ** 40
        let c = 2 ** 63
        return a, b, c
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    ASSERT_EQ(behl::type(S, -3), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -3), 0);
    ASSERT_EQ(behl::to_integer(S, -2), -6289078614652622815LL);
    ASSERT_EQ(behl::to_integer(S, -1), static_cast<int64_t>(0x8000000000000000ULL));
}

TEST_P(IntegerWrappingTest, NegationOfMin)
{
    constexpr std::string_view code = R"(
        let min = -9223372036854775807 - 1  // Compute INT64_MIN at runtime
        let result = -min
        return result
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), static_cast<int64_t>(0x8000000000000000ULL));
}

TEST_P(IntegerWrappingTest, ConstantFoldedNegationOfMinMatchesRuntime)
{
    constexpr std::string_view code = R"(
        function negate(v) { return -v }

        let folded = -(-9223372036854775807 - 1)
        let runtime = negate(-9223372036854775807 - 1)

        return folded, runtime
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);

    ASSERT_EQ(behl::type(S, -2), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -2), static_cast<int64_t>(0x8000000000000000ULL));
    ASSERT_EQ(behl::to_integer(S, -1), behl::to_integer(S, -2));
}

TEST_P(IntegerWrappingTest, ComplexWrapping)
{
    constexpr std::string_view code = R"(
        let a = 9223372036854775807  // INT64_MAX
        let b = 10
        let result = (a + b) - 5
        return result
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    int64_t expected = static_cast<int64_t>(0x8000000000000000ULL) + 4;
    ASSERT_EQ(behl::to_integer(S, -1), expected);
}

TEST_P(IntegerWrappingTest, IncrementDecrement)
{
    constexpr std::string_view code = R"(
        let max = 9223372036854775807
        max++
        let min = -9223372036854775807 - 1  // Compute INT64_MIN at runtime
        min--
        return max, min
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    ASSERT_EQ(behl::type(S, -2), behl::Type::kInteger);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -2), static_cast<int64_t>(0x8000000000000000ULL));
    ASSERT_EQ(behl::to_integer(S, -1), 9223372036854775807LL);
}

TEST_P(IntegerWrappingTest, ShiftCountsAtAndBeyondWidthFolded)
{
    constexpr std::string_view code = R"(
        let r = ""
        r = r + tostring(1 << 63) + "," + tostring(1 << 64) + "," + tostring(1 << -1) + "," + tostring(4 >> -1)
        r = r + "," + tostring(1 << 9223372036854775807) + "," + tostring(1 << (-9223372036854775807 - 1))
        r = r + "," + tostring(-8 >> 63) + "," + tostring(-8 >> 64) + "," + tostring(8 >> 64)
        r = r + "," + tostring(8 >> 9223372036854775807) + "," + tostring(-8 >> 9223372036854775807)
        r = r + "," + tostring(8 >> (-9223372036854775807 - 1)) + "," + tostring(1 >> -1)
        return r
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "-9223372036854775808,0,0,8,0,0,-1,-1,0,0,-1,0,2");
}

TEST_P(IntegerWrappingTest, ShiftCountsAtAndBeyondWidthRuntime)
{
    constexpr std::string_view code = R"(
        let one = 1
        let four = 4
        let eight = 8
        let neg8 = -8
        let c63 = 63
        let c64 = 64
        let cm1 = -1
        let maxint = 9223372036854775807
        let minint = -9223372036854775807 - 1
        let r = ""
        r = r + tostring(one << c63) + "," + tostring(one << c64) + "," + tostring(one << cm1) + "," + tostring(four >> cm1)
        r = r + "," + tostring(one << maxint) + "," + tostring(one << minint)
        r = r + "," + tostring(neg8 >> c63) + "," + tostring(neg8 >> c64) + "," + tostring(eight >> c64)
        r = r + "," + tostring(eight >> maxint) + "," + tostring(neg8 >> maxint)
        r = r + "," + tostring(eight >> minint) + "," + tostring(one >> cm1)
        return r
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "-9223372036854775808,0,0,8,0,0,-1,-1,0,0,-1,0,2");
}

INSTANTIATE_TEST_SUITE_P(Mode, IntegerWrappingTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
