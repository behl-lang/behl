#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <cmath>
#include <gtest/gtest.h>

class NumericTest : public ::testing::Test
{
protected:
    behl::State* S;
    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S);

        ASSERT_NE(S, nullptr);
        behl::set_top(S, 0);
    }
    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(NumericTest, Arithmetic_AddSubMul)
{
    constexpr std::string_view code = R"(
        let a = 10 + 20 - 5
        let b = -5 + 2
        let c = 6 * 7
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 25);
    ASSERT_EQ(behl::to_integer(S, -2), -3);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(NumericTest, Arithmetic_DivMod)
{
    constexpr std::string_view code = R"(
        let a = 7 / 2
        let b = 7 % 4
        return a, b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -2), 3.5);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_F(NumericTest, Bitwise_AndOrXor)
{
    constexpr std::string_view code = R"(
        let a = 5
        let b = 3
        let c = 6
        let d = 1
        let e = 8
        let f = 3
        return a & b, c | d, e ^ f
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 1);
    ASSERT_EQ(behl::to_integer(S, -2), 7);
    ASSERT_EQ(behl::to_integer(S, -1), 11);
}

TEST_F(NumericTest, Bitwise_ShiftsAndNot)
{
    constexpr std::string_view code = R"(
        let a = 1
        let b = 3
        let c = 8
        let d = 2
        let e = 3
        return a << b, c >> d, ~e
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_integer(S, -3), 8);
    ASSERT_EQ(behl::to_integer(S, -2), 2);
    ASSERT_EQ(behl::to_integer(S, -1), -4);
}

TEST_F(NumericTest, MixedFloatIntegerArithmetic)
{
    constexpr std::string_view code = R"(
        let a = 1 + 2.5
        let b = 3.0 * 4
        let c = 5 / 2.0
        let d = 6.5 - 2
        return a, b, c, d
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -4), 3.5);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -3), 12.0);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -2), 2.5);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 4.5);
}

TEST_F(NumericTest, FloatArithmetic)
{
    constexpr std::string_view code = R"(
        let a = 3.14 + 2.86
        let b = 10.5 - 3.2
        let c = 4.0 * 2.5
        let d = 15.0 / 3.0
        let e = 17.5 % 4.0
        return a, b, c, d, e
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 5));
    ASSERT_EQ(behl::get_top(S), 5);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -5), 6.0);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -4), 7.3);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -3), 10.0);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -2), 5.0);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 1.5);
}

TEST_F(NumericTest, UnaryMinus)
{
    constexpr std::string_view code = R"(
        let a = -5
        let b = -3.14
        let c = -(-10)
        let d = -(-2.5)
        return a, b, c, d
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), -5);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -3), -3.14);
    ASSERT_EQ(behl::to_integer(S, -2), 10);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 2.5);
}

TEST_F(NumericTest, NumericComparisons)
{
    constexpr std::string_view code = R"(
        let a = 1 < 2.0
        let b = 3.0 > 2
        let c = 4 <= 4.0
        let d = 5.0 >= 5
        let e = 6 == 6.0
        let f = 7 != 7.5
        return a, b, c, d, e, f
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 6));
    ASSERT_EQ(behl::get_top(S), 6);
    ASSERT_TRUE(behl::to_boolean(S, -6));
    ASSERT_TRUE(behl::to_boolean(S, -5));
    ASSERT_TRUE(behl::to_boolean(S, -4));
    ASSERT_TRUE(behl::to_boolean(S, -3));
    ASSERT_TRUE(behl::to_boolean(S, -2));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(NumericTest, TypeOfNumbers)
{
    constexpr std::string_view code = R"(
        let a = typeof(5)
        let b = typeof(3.14)
        let c = typeof(1 + 2.0)
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);
    ASSERT_EQ(behl::to_string(S, -3), "integer");
    ASSERT_EQ(behl::to_string(S, -2), "number");
    ASSERT_EQ(behl::to_string(S, -1), "number");
}

TEST_F(NumericTest, ImmediateOpcodeBoundaries_SmallIntegers)
{
    constexpr std::string_view code = R"(
        let a = 65535          // Max positive for immediate
        let b = -65536         // Min negative for immediate
        let c = 0              // Zero
        let d = 1 + 100        // ADDIMM within range
        let e = 1000 - 50      // SUBIMM within range
        let f = a + 0          // Edge case: max + 0
        let g = b - 0          // Edge case: min - 0
        return a, b, c, d, e, f, g
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 7));
    ASSERT_EQ(behl::get_top(S), 7);
    ASSERT_EQ(behl::to_integer(S, -7), 65535);
    ASSERT_EQ(behl::to_integer(S, -6), -65536);
    ASSERT_EQ(behl::to_integer(S, -5), 0);
    ASSERT_EQ(behl::to_integer(S, -4), 101);
    ASSERT_EQ(behl::to_integer(S, -3), 950);
    ASSERT_EQ(behl::to_integer(S, -2), 65535);
    ASSERT_EQ(behl::to_integer(S, -1), -65536);
}

TEST_F(NumericTest, ImmediateOpcodeBoundaries_LargeIntegers)
{
    constexpr std::string_view code = R"(
        let a = 65536          // Just above max immediate
        let b = -65537         // Just below min immediate
        let c = 1000000        // Large positive
        let d = -1000000       // Large negative
        let e = 1 + 70000      // ADDKI: constant too large for immediate
        let f = 100000 - 50    // SUBKI: base too large
        return a, b, c, d, e, f
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 6));
    ASSERT_EQ(behl::get_top(S), 6);
    ASSERT_EQ(behl::to_integer(S, -6), 65536);
    ASSERT_EQ(behl::to_integer(S, -5), -65537);
    ASSERT_EQ(behl::to_integer(S, -4), 1000000);
    ASSERT_EQ(behl::to_integer(S, -3), -1000000);
    ASSERT_EQ(behl::to_integer(S, -2), 70001);
    ASSERT_EQ(behl::to_integer(S, -1), 99950);
}

TEST_F(NumericTest, ImmediateOpcodeBoundaries_MixedOperations)
{
    constexpr std::string_view code = R"(
        let small = 100
        let large = 100000
        let a = small + 50     // ADDIMM
        let b = large + 50     // ADDKI (base is large)
        let c = small - 25     // SUBIMM
        let d = large - 25     // SUBKI (base is large)
        let e = 65535 + 1      // ADDIMM at boundary
        let f = -65536 - 1     // SUBIMM at boundary
        return a, b, c, d, e, f
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 6));
    ASSERT_EQ(behl::get_top(S), 6);
    ASSERT_EQ(behl::to_integer(S, -6), 150);
    ASSERT_EQ(behl::to_integer(S, -5), 100050);
    ASSERT_EQ(behl::to_integer(S, -4), 75);
    ASSERT_EQ(behl::to_integer(S, -3), 99975);
    ASSERT_EQ(behl::to_integer(S, -2), 65536);
    ASSERT_EQ(behl::to_integer(S, -1), -65537);
}

TEST_F(NumericTest, ImmediateOpcodeBoundaries_LoopCounters)
{
    constexpr std::string_view code = R"(
        let sum1 = 0
        for (let i = 0; i < 1000; i++) {
            sum1 += 1     // ADDIMM
        }
        let sum2 = 10000
        for (let i = 0; i < 100; i++) {
            sum2 -= 10    // SUBIMM
        }
        return sum1, sum2
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::to_integer(S, -2), 1000);
    ASSERT_EQ(behl::to_integer(S, -1), 9000);
}

TEST_F(NumericTest, DivisionByZero_ReturnsInfinity)
{
    constexpr std::string_view code = R"(
        let a = 5 / 0
        let b = -10 / 0
        let c = 5.0 / 0.0
        return a, b, c
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::get_top(S), 3);

    ASSERT_EQ(behl::type(S, -3), behl::Type::kNumber);
    ASSERT_TRUE(std::isinf(behl::to_number(S, -3)));
    ASSERT_GT(behl::to_number(S, -3), 0); // positive infinity

    ASSERT_EQ(behl::type(S, -2), behl::Type::kNumber);
    ASSERT_TRUE(std::isinf(behl::to_number(S, -2)));
    ASSERT_LT(behl::to_number(S, -2), 0); // negative infinity

    ASSERT_EQ(behl::type(S, -1), behl::Type::kNumber);
    ASSERT_TRUE(std::isinf(behl::to_number(S, -1)));
    ASSERT_GT(behl::to_number(S, -1), 0); // positive infinity
}

TEST_F(NumericTest, ModuloByZero_ThrowsError)
{
    constexpr std::string_view code = R"(
        let a = 5 % 0
        return a
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::TypeError);
}

TEST_F(NumericTest, FloatModuloByZero_ReturnsNaN)
{
    constexpr std::string_view code = R"(
        let a = 5.0 % 0.0
        return a
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNumber);

    ASSERT_TRUE(std::isnan(behl::to_number(S, -1)));
}

TEST_F(NumericTest, ToNumberDirectCall)
{
    constexpr std::string_view code = R"(
        return tonumber("42")
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(NumericTest, ToNumberAsArgument)
{
    constexpr std::string_view code = R"(
        function identity(x) {
            return x;
        }
        return identity(tonumber("123"));
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 123);
}

TEST_F(NumericTest, ToNumberWithAssignment)
{
    constexpr std::string_view code = R"(
        let n = tonumber("999");
        return n;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 999);
}

TEST_F(NumericTest, ToNumberFloat)
{
    constexpr std::string_view code = R"(
        return tonumber("3.14159");
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNumber);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 3.14159);
}

TEST_F(NumericTest, ToNumberInvalidString)
{
    constexpr std::string_view code = R"(
        return tonumber("not a number");
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(NumericTest, ToNumberAlreadyNumber)
{
    constexpr std::string_view code = R"(
        return tonumber(42), tonumber(3.14);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_EQ(behl::type(S, -2), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -2), 42);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNumber);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 3.14);
}

TEST_F(NumericTest, ToNumberMultipleCalls)
{
    constexpr std::string_view code = R"(
        let result = tonumber("10") + tonumber("20") + tonumber("5");
        return result;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 35);
}

TEST_F(NumericTest, ToNumberHexString)
{
    constexpr std::string_view code = R"(
        return tonumber("0xFF");
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(NumericTest, ToNumberNegativeString)
{
    constexpr std::string_view code = R"(
        return tonumber("-42");
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), -42);
}

TEST_F(NumericTest, Arithmetic_LargeIntegerConstants)
{
    constexpr std::string_view code = R"(
        let a = 10 + 1000;
        let b = 20 - 500;
        let c = 5 + 300;
        let d = 1000 - 1;
        return a, b, c, d;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_EQ(behl::to_integer(S, -4), 1010);
    ASSERT_EQ(behl::to_integer(S, -3), -480);
    ASSERT_EQ(behl::to_integer(S, -2), 305);
    ASSERT_EQ(behl::to_integer(S, -1), 999);
}

TEST_F(NumericTest, Comparison_LargeIntegerConstants)
{
    constexpr std::string_view code = R"(
        let a = 10 < 1000;
        let b = 500 <= 500;
        let c = 1000 > 10;
        let d = 300 >= 400;
        return a, b, c, d;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::get_top(S), 4);
    ASSERT_TRUE(behl::to_boolean(S, -4));
    ASSERT_TRUE(behl::to_boolean(S, -3));
    ASSERT_TRUE(behl::to_boolean(S, -2));
    ASSERT_FALSE(behl::to_boolean(S, -1));
}
