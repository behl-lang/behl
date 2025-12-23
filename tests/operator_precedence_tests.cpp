#include <behl/behl.hpp>
#include <gtest/gtest.h>

class OperatorPrecedenceTest : public ::testing::Test
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
    }
};

TEST_F(OperatorPrecedenceTest, ArithmeticPrecedence)
{
    constexpr std::string_view code = R"(
        if (2 + 3 * 4 != 14) { return false }
        if (2 * 3 + 4 != 10) { return false }
        if (-3 - 1 - 5 != -9) { return false }
        if (2 * 1 + 3 / 3 != 3) { return false }
        return true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(OperatorPrecedenceTest, ModuloOperations)
{
    constexpr std::string_view code = R"(
        if (10 % 3 != 1) { return false }
        if (15 % 4 != 3) { return false }
        if (20 % 6 != 2) { return false }
        return true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(OperatorPrecedenceTest, BitwiseOperations)
{
    constexpr std::string_view code = R"(
        if ((0xF0 | 0x0F) != 0xFF) { return false }
        if ((0xFF & 0x0F) != 0x0F) { return false }
        if ((0xF0 << 4) != 0xF00) { return false }
        if ((0xFF ^ 0xAA) != 0x55) { return false }
        return true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(OperatorPrecedenceTest, PowerOperator)
{
    constexpr std::string_view code = R"(
        if (2 ** 3 != 8) { return false }
        if (3 ** 2 != 9) { return false }
        if (2 ** 3 * 2 != 16) { return false }
        return true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(OperatorPrecedenceTest, ShortCircuitEvaluation)
{
    constexpr std::string_view code = R"(
        let counter = 0
        function increment() {
            counter++
            return true
        }
        
        if (false && increment()) { }
        if (counter != 0) { return false }
        
        if (true || increment()) { }
        if (counter != 0) { return false }
        
        if (true && increment()) { }
        if (counter != 1) { return false }
        
        return true
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(OperatorPrecedenceTest, StringComparisons)
{
    constexpr std::string_view code = R"(
        let result = 0
        if ("alo" < "alo1") { result = result + 1 }
        if ("" < "a") { result = result + 1 }
        if ("alo\0alo" < "alo\0b") { result = result + 1 }
        if ("alo" < "alo\0") { result = result + 1 }
        if ("\0" < "\1") { result = result + 1 }
        return result
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_F(OperatorPrecedenceTest, MixedArithmetic)
{
    constexpr std::string_view code = R"(
        let a = 10
        let b = 3.5
        let c = a + b
        let d = a * b
        let e = a / 2
        return c + d + e
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 13.5 + 35.0 + 5.0);
}

TEST_F(OperatorPrecedenceTest, NilComparisons)
{
    constexpr std::string_view code = R"(
        if (nil == nil) { return 1 }
        return 0
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_F(OperatorPrecedenceTest, BooleanComparisons)
{
    constexpr std::string_view code = R"(
        if ((true == true) && (false == false)) { return 1 }
        return 0
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}
