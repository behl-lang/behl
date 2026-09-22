#include "state.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <string>

class MathTest : public ::testing::TestWithParam<bool>
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
        S = nullptr;
    }

    void run(const std::string& expr, int results = 1)
    {
        const std::string code = "const math = import(\"math\")\nreturn " + expr + "\n";
        ASSERT_NO_THROW(behl::load_string(S, code));
        ASSERT_NO_THROW(behl::call(S, 0, results));
    }

    double number(const std::string& expr)
    {
        run(expr);
        return behl::to_number(S, -1);
    }

    int64_t integer(const std::string& expr)
    {
        run(expr);
        return behl::to_integer(S, -1);
    }

    behl::Type type_of(const std::string& expr)
    {
        run(expr);
        return behl::type(S, -1);
    }

    bool boolean(const std::string& expr)
    {
        run(expr);
        return behl::to_boolean(S, -1);
    }
};

TEST_P(MathTest, AbsPreservesArgumentType)
{
    ASSERT_EQ(type_of("math.abs(-5)"), behl::Type::kInteger);
    ASSERT_EQ(integer("math.abs(-5)"), 5);
    ASSERT_EQ(integer("math.abs(5)"), 5);
    ASSERT_EQ(integer("math.abs(0)"), 0);

    ASSERT_EQ(type_of("math.abs(-5.5)"), behl::Type::kNumber);
    ASSERT_DOUBLE_EQ(number("math.abs(-5.5)"), 5.5);
    ASSERT_DOUBLE_EQ(number("math.abs(-0.0)"), 0.0);
}

TEST_P(MathTest, AbsOfIntegerMinimumWrapsToItself)
{
    ASSERT_EQ(integer("math.abs(-9223372036854775807 - 1)"), std::numeric_limits<int64_t>::min());
}

TEST_P(MathTest, RoundingReturnsIntegersWhenRepresentable)
{
    ASSERT_EQ(type_of("math.floor(3.7)"), behl::Type::kInteger);
    ASSERT_EQ(integer("math.floor(3.7)"), 3);
    ASSERT_EQ(integer("math.floor(-3.2)"), -4);
    ASSERT_EQ(integer("math.ceil(3.2)"), 4);
    ASSERT_EQ(integer("math.ceil(-3.7)"), -3);
    ASSERT_EQ(integer("math.trunc(3.7)"), 3);
    ASSERT_EQ(integer("math.trunc(-3.7)"), -3);
    ASSERT_EQ(integer("math.round(3.5)"), 4);
    ASSERT_EQ(integer("math.round(-3.5)"), -4);
    ASSERT_EQ(integer("math.round(2.4)"), 2);
}

TEST_P(MathTest, RoundingStaysFloatWhenOutOfIntegerRange)
{
    ASSERT_EQ(type_of("math.floor(10.0 ** 300)"), behl::Type::kNumber);
    ASSERT_EQ(type_of("math.ceil(0.0 - 10.0 ** 300)"), behl::Type::kNumber);
    ASSERT_EQ(type_of("math.floor(1.0 / 0.0)"), behl::Type::kNumber);
    ASSERT_TRUE(std::isinf(number("math.floor(1.0 / 0.0)")));
}

TEST_P(MathTest, SqrtAndCbrtExactCases)
{
    ASSERT_DOUBLE_EQ(number("math.sqrt(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.sqrt(1.0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.sqrt(4.0)"), 2.0);
    ASSERT_DOUBLE_EQ(number("math.sqrt(2.25)"), 1.5);
    ASSERT_DOUBLE_EQ(number("math.cbrt(27.0)"), 3.0);
    ASSERT_DOUBLE_EQ(number("math.cbrt(0.0 - 8.0)"), -2.0);
}

TEST_P(MathTest, PowExactCases)
{
    ASSERT_DOUBLE_EQ(number("math.pow(2.0, 10.0)"), 1024.0);
    ASSERT_DOUBLE_EQ(number("math.pow(2.0, 0.0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.pow(2.0, 0.0 - 1.0)"), 0.5);
    ASSERT_DOUBLE_EQ(number("math.pow(9.0, 0.5)"), 3.0);
}

TEST_P(MathTest, ExponentialAndLogarithmExactCases)
{
    ASSERT_DOUBLE_EQ(number("math.exp(0.0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.expm1(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.log(1.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.log1p(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.log10(1000.0)"), 3.0);
    ASSERT_DOUBLE_EQ(number("math.log2(1024.0)"), 10.0);
    ASSERT_DOUBLE_EQ(number("math.log(8.0, 2.0)"), 3.0);
}

TEST_P(MathTest, TrigonometryExactCases)
{
    ASSERT_DOUBLE_EQ(number("math.sin(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.cos(0.0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.tan(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.asin(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.acos(1.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.atan(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.atan2(0.0, 1.0)"), 0.0);
}

TEST_P(MathTest, TrigonometryApproximateCases)
{
    ASSERT_NEAR(number("math.sin(math.pi / 2.0)"), 1.0, 1e-12);
    ASSERT_NEAR(number("math.cos(math.pi)"), -1.0, 1e-12);
    ASSERT_NEAR(number("math.asin(1.0)"), std::acos(-1.0) / 2.0, 1e-12);
    ASSERT_NEAR(number("math.atan2(1.0, 1.0)"), std::acos(-1.0) / 4.0, 1e-12);
}

TEST_P(MathTest, HyperbolicExactCases)
{
    ASSERT_DOUBLE_EQ(number("math.sinh(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.cosh(0.0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.tanh(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.asinh(0.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.acosh(1.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.atanh(0.0)"), 0.0);
}

TEST_P(MathTest, DegreesAndRadians)
{
    ASSERT_NEAR(number("math.deg(math.pi)"), 180.0, 1e-12);
    ASSERT_DOUBLE_EQ(number("math.deg(0.0)"), 0.0);
    ASSERT_NEAR(number("math.rad(180.0)"), std::acos(-1.0), 1e-12);
    ASSERT_DOUBLE_EQ(number("math.rad(0.0)"), 0.0);
}

TEST_P(MathTest, MinMaxKeepIntegerTypeWhenAllArgumentsAreIntegers)
{
    ASSERT_EQ(type_of("math.min(3, 1, 2)"), behl::Type::kInteger);
    ASSERT_EQ(integer("math.min(3, 1, 2)"), 1);
    ASSERT_EQ(integer("math.max(3, 1, 2)"), 3);
    ASSERT_EQ(integer("math.min(5)"), 5);
    ASSERT_EQ(integer("math.max(5)"), 5);
    ASSERT_EQ(integer("math.min(-1, -2, -3)"), -3);
}

TEST_P(MathTest, MinMaxFallBackToFloatWhenAnyArgumentIsFloat)
{
    ASSERT_EQ(type_of("math.min(3, 1.5, 2)"), behl::Type::kNumber);
    ASSERT_DOUBLE_EQ(number("math.min(3, 1.5, 2)"), 1.5);
    ASSERT_DOUBLE_EQ(number("math.max(3, 1.5, 2)"), 3.0);
}

TEST_P(MathTest, MinMaxRequireAtLeastOneArgument)
{
    const std::string code = "const math = import(\"math\")\nreturn math.min()\n";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_THROW(behl::call(S, 0, 1), behl::RuntimeError);
}

TEST_P(MathTest, Clamp)
{
    ASSERT_DOUBLE_EQ(number("math.clamp(5.0, 0.0, 10.0)"), 5.0);
    ASSERT_DOUBLE_EQ(number("math.clamp(-5.0, 0.0, 10.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.clamp(15.0, 0.0, 10.0)"), 10.0);
    ASSERT_DOUBLE_EQ(number("math.clamp(0.0, 0.0, 10.0)"), 0.0);
    ASSERT_DOUBLE_EQ(number("math.clamp(10.0, 0.0, 10.0)"), 10.0);
}

TEST_P(MathTest, SignAlwaysReturnsInteger)
{
    ASSERT_EQ(type_of("math.sign(2.5)"), behl::Type::kInteger);
    ASSERT_EQ(integer("math.sign(2.5)"), 1);
    ASSERT_EQ(integer("math.sign(-2.5)"), -1);
    ASSERT_EQ(integer("math.sign(0.0)"), 0);
    ASSERT_EQ(integer("math.sign(-0.0)"), 0);
    ASSERT_EQ(integer("math.sign(7)"), 1);
}

TEST_P(MathTest, FmodAndHypot)
{
    ASSERT_DOUBLE_EQ(number("math.fmod(7.0, 3.0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.fmod(-7.0, 3.0)"), -1.0);
    ASSERT_DOUBLE_EQ(number("math.hypot(3.0, 4.0)"), 5.0);
    ASSERT_DOUBLE_EQ(number("math.hypot(0.0, 0.0)"), 0.0);
}

TEST_P(MathTest, ModfSplitsIntegralAndFractionalParts)
{
    run("math.modf(3.75)", 2);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -2), 3.0);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 0.75);
}

TEST_P(MathTest, FrexpAndLdexpAreInverses)
{
    run("math.frexp(8.0)", 2);
    ASSERT_DOUBLE_EQ(behl::to_number(S, -2), 0.5);
    ASSERT_EQ(behl::to_integer(S, -1), 4);

    ASSERT_DOUBLE_EQ(number("math.ldexp(0.5, 4)"), 8.0);
    ASSERT_DOUBLE_EQ(number("math.ldexp(1.0, 0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.ldexp(1.0, -1)"), 0.5);
}

TEST_P(MathTest, SpecialValuePredicates)
{
    ASSERT_TRUE(boolean("math.is_nan(0.0 / 0.0)"));
    ASSERT_FALSE(boolean("math.is_nan(1.0)"));
    ASSERT_TRUE(boolean("math.is_inf(1.0 / 0.0)"));
    ASSERT_TRUE(boolean("math.is_inf(-1.0 / 0.0)"));
    ASSERT_FALSE(boolean("math.is_inf(1.0)"));
    ASSERT_TRUE(boolean("math.is_finite(1.0)"));
    ASSERT_FALSE(boolean("math.is_finite(1.0 / 0.0)"));
    ASSERT_FALSE(boolean("math.is_finite(0.0 / 0.0)"));
}

TEST_P(MathTest, DomainErrorsProduceNaN)
{
    ASSERT_TRUE(std::isnan(number("math.sqrt(0.0 - 1.0)")));
    ASSERT_TRUE(std::isnan(number("math.log(0.0 - 1.0)")));
    ASSERT_TRUE(std::isnan(number("math.asin(2.0)")));
    ASSERT_TRUE(std::isnan(number("math.acos(2.0)")));
    ASSERT_TRUE(std::isnan(number("math.acosh(0.0)")));
    ASSERT_TRUE(std::isnan(number("math.atanh(2.0)")));
}

TEST_P(MathTest, LogarithmOfZeroIsNegativeInfinity)
{
    const double v = number("math.log(0.0)");
    ASSERT_TRUE(std::isinf(v));
    ASSERT_LT(v, 0.0);

    const double v10 = number("math.log10(0.0)");
    ASSERT_TRUE(std::isinf(v10));
    ASSERT_LT(v10, 0.0);
}

TEST_P(MathTest, Constants)
{
    ASSERT_DOUBLE_EQ(number("math.pi"), std::acos(-1.0));
    ASSERT_NEAR(number("math.e"), 2.718281828459045, 1e-15);
    ASSERT_TRUE(std::isinf(number("math.huge")));
    ASSERT_GT(number("math.huge"), 0.0);
}

TEST_P(MathTest, IntegerArgumentsAreAcceptedByFloatFunctions)
{
    ASSERT_DOUBLE_EQ(number("math.sqrt(4)"), 2.0);
    ASSERT_DOUBLE_EQ(number("math.exp(0)"), 1.0);
    ASSERT_DOUBLE_EQ(number("math.hypot(3, 4)"), 5.0);
    ASSERT_DOUBLE_EQ(number("math.fmod(7, 3)"), 1.0);
}

TEST_P(MathTest, LdexpWithNonRepresentableExponentReachesFloatToIntConversion)
{
    const char* exponents[] = {
        "10.0 ** 300",
        "0.0 - 10.0 ** 300",
        "1.0 / 0.0",
        "0.0 - 1.0 / 0.0",
        "0.0 / 0.0",
    };

    for (const char* exponent : exponents)
    {
        behl::State* fresh = behl::new_state();
        fresh->jit_enabled = GetParam();
        behl::load_stdlib(fresh);

        const std::string code = std::string("const math = import(\"math\")\nreturn math.ldexp(1.0, ") + exponent + ")\n";

        EXPECT_NO_THROW(behl::load_string(fresh, code)) << exponent;
        EXPECT_NO_THROW(behl::call(fresh, 0, 1)) << exponent;
        EXPECT_EQ(behl::type(fresh, -1), behl::Type::kNumber) << exponent;

        behl::close(fresh);
    }
}

TEST_P(MathTest, LdexpTreatsNonRepresentableExponentAsZero)
{
    ASSERT_DOUBLE_EQ(number("math.ldexp(3.0, 10.0 ** 300)"), 3.0);
    ASSERT_DOUBLE_EQ(number("math.ldexp(3.0, 0.0 - 10.0 ** 300)"), 3.0);
    ASSERT_DOUBLE_EQ(number("math.ldexp(3.0, 1.0 / 0.0)"), 3.0);
    ASSERT_DOUBLE_EQ(number("math.ldexp(3.0, 0.0 - 1.0 / 0.0)"), 3.0);
    ASSERT_DOUBLE_EQ(number("math.ldexp(3.0, 0.0 / 0.0)"), 3.0);
}

TEST_P(MathTest, LdexpStillHonoursRepresentableFloatExponents)
{
    ASSERT_DOUBLE_EQ(number("math.ldexp(1.0, 4.0)"), 16.0);
    ASSERT_DOUBLE_EQ(number("math.ldexp(1.0, 0.0 - 2.0)"), 0.25);
}

INSTANTIATE_TEST_SUITE_P(Mode, MathTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
