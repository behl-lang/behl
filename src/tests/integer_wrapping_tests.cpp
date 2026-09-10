#include <behl/behl.hpp>
#include <gtest/gtest.h>

class IntegerWrappingTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
        S = nullptr;
    }
};

TEST_F(IntegerWrappingTest, AdditionOverflow)
{
    constexpr std::string_view code = R"(
        let max = 9223372036854775807  // INT64_MAX
        let result = max + 1
        return result
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), static_cast<int64_t>(0x8000000000000000ULL));
}

TEST_F(IntegerWrappingTest, SubtractionUnderflow)
{
    constexpr std::string_view code = R"(
        let min = -9223372036854775807 - 1  // Compute INT64_MIN at runtime
        let result = min - 1
        return result
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), 9223372036854775807LL);
}

TEST_F(IntegerWrappingTest, MultiplicationOverflow)
{
    constexpr std::string_view code = R"(
        let a = 9223372036854775807  // INT64_MAX
        let result = a * 2
        return result
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), -2);
}

TEST_F(IntegerWrappingTest, NegationOfMin)
{
    constexpr std::string_view code = R"(
        let min = -9223372036854775807 - 1  // Compute INT64_MIN at runtime
        let result = -min
        return result
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -1), static_cast<int64_t>(0x8000000000000000ULL));
}

TEST_F(IntegerWrappingTest, ComplexWrapping)
{
    constexpr std::string_view code = R"(
        let a = 9223372036854775807  // INT64_MAX
        let b = 10
        let result = (a + b) - 5
        return result
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    int64_t expected = static_cast<int64_t>(0x8000000000000000ULL) + 4;
    ASSERT_EQ(behl::to_integer(S, -1), expected);
}

TEST_F(IntegerWrappingTest, IncrementDecrement)
{
    constexpr std::string_view code = R"(
        let max = 9223372036854775807
        max++
        let min = -9223372036854775807 - 1  // Compute INT64_MIN at runtime
        min--
        return max, min
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::type(S, -2), behl::Type::kInteger);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);

    ASSERT_EQ(behl::to_integer(S, -2), static_cast<int64_t>(0x8000000000000000ULL));
    ASSERT_EQ(behl::to_integer(S, -1), 9223372036854775807LL);
}
