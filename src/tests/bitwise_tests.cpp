#include <behl/behl.hpp>
#include <gtest/gtest.h>

class BitwiseTest : public ::testing::Test
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

TEST_F(BitwiseTest, BitwiseAnd_Integers)
{
    constexpr std::string_view code = R"(
        return 0xFF & 0x0F,
               15 & 7,
               0 & 5,
               -1 & 127;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 0x0F);
    EXPECT_EQ(behl::to_integer(S, -3), 7);
    EXPECT_EQ(behl::to_integer(S, -2), 0);
    EXPECT_EQ(behl::to_integer(S, -1), 127);
}

TEST_F(BitwiseTest, BitwiseOr_Integers)
{
    constexpr std::string_view code = R"(
        return 0xF0 | 0x0F,
               8 | 4,
               0 | 5,
               1 | 2 | 4;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 0xFF);
    EXPECT_EQ(behl::to_integer(S, -3), 12);
    EXPECT_EQ(behl::to_integer(S, -2), 5);
    EXPECT_EQ(behl::to_integer(S, -1), 7);
}

TEST_F(BitwiseTest, BitwiseXor_Integers)
{
    constexpr std::string_view code = R"(
        return 0xFF ^ 0xAA,
               12 ^ 5,
               7 ^ 7,
               0 ^ 15;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 0x55);
    EXPECT_EQ(behl::to_integer(S, -3), 9);
    EXPECT_EQ(behl::to_integer(S, -2), 0);
    EXPECT_EQ(behl::to_integer(S, -1), 15);
}

TEST_F(BitwiseTest, BitwiseNot_Integers)
{
    constexpr std::string_view code = R"(
        return ~0,
               ~1,
               ~-1,
               ~127;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), ~0);
    EXPECT_EQ(behl::to_integer(S, -3), ~1);
    EXPECT_EQ(behl::to_integer(S, -2), ~(-1));
    EXPECT_EQ(behl::to_integer(S, -1), ~127);
}

TEST_F(BitwiseTest, LeftShift_Integers)
{
    constexpr std::string_view code = R"(
        return 1 << 0,
               1 << 3,
               5 << 2,
               0xFF << 8;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 1);
    EXPECT_EQ(behl::to_integer(S, -3), 8);
    EXPECT_EQ(behl::to_integer(S, -2), 20);
    EXPECT_EQ(behl::to_integer(S, -1), 0xFF00);
}

TEST_F(BitwiseTest, RightShift_Integers)
{
    constexpr std::string_view code = R"(
        return 8 >> 0,
               8 >> 2,
               127 >> 3,
               0xFF00 >> 8;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 8);
    EXPECT_EQ(behl::to_integer(S, -3), 2);
    EXPECT_EQ(behl::to_integer(S, -2), 15);
    EXPECT_EQ(behl::to_integer(S, -1), 0xFF);
}

TEST_F(BitwiseTest, BitwiseAnd_FloatOperands)
{
    constexpr std::string_view code = R"(
        return 15.7 & 7,
               15 & 7.2,
               15.9 & 7.1;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 15 & 7);
    EXPECT_EQ(behl::to_integer(S, -2), 15 & 7);
    EXPECT_EQ(behl::to_integer(S, -1), 15 & 7);
}

TEST_F(BitwiseTest, BitwiseOr_FloatOperands)
{
    constexpr std::string_view code = R"(
        return 8.5 | 4,
               8 | 4.5,
               8.9 | 4.1;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 8 | 4);
    EXPECT_EQ(behl::to_integer(S, -2), 8 | 4);
    EXPECT_EQ(behl::to_integer(S, -1), 8 | 4);
}

TEST_F(BitwiseTest, BitwiseXor_FloatOperands)
{
    constexpr std::string_view code = R"(
        return 12.8 ^ 5,
               12 ^ 5.3,
               12.1 ^ 5.9;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 12 ^ 5);
    EXPECT_EQ(behl::to_integer(S, -2), 12 ^ 5);
    EXPECT_EQ(behl::to_integer(S, -1), 12 ^ 5);
}

TEST_F(BitwiseTest, LeftShift_FloatOperands)
{
    constexpr std::string_view code = R"(
        return 5.7 << 2,
               5 << 2.3,
               5.1 << 2.9;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 5 << 2);
    EXPECT_EQ(behl::to_integer(S, -2), 5 << 2);
    EXPECT_EQ(behl::to_integer(S, -1), 5 << 2);
}

TEST_F(BitwiseTest, RightShift_FloatOperands)
{
    constexpr std::string_view code = R"(
        return 127.9 >> 3,
               127 >> 3.7,
               127.5 >> 3.2;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 127 >> 3);
    EXPECT_EQ(behl::to_integer(S, -2), 127 >> 3);
    EXPECT_EQ(behl::to_integer(S, -1), 127 >> 3);
}

TEST_F(BitwiseTest, BitwiseAnd_NegativeNumbers)
{
    constexpr std::string_view code = R"(
        return -1 & 127,
               -8 & 15,
               -1 & -1;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), (-1) & 127);
    EXPECT_EQ(behl::to_integer(S, -2), (-8) & 15);
    EXPECT_EQ(behl::to_integer(S, -1), (-1) & (-1));
}

TEST_F(BitwiseTest, BitwiseOr_NegativeNumbers)
{
    constexpr std::string_view code = R"(
        return -1 | 0,
               -8 | 7,
               -1 | -2;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), (-1) | 0);
    EXPECT_EQ(behl::to_integer(S, -2), (-8) | 7);
    EXPECT_EQ(behl::to_integer(S, -1), (-1) | (-2));
}

TEST_F(BitwiseTest, BitwiseXor_NegativeNumbers)
{
    constexpr std::string_view code = R"(
        return -1 ^ 0,
               -8 ^ 7,
               -1 ^ -1;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), (-1) ^ 0);
    EXPECT_EQ(behl::to_integer(S, -2), (-8) ^ 7);
    EXPECT_EQ(behl::to_integer(S, -1), (-1) ^ (-1));
}

TEST_F(BitwiseTest, BitwiseNot_NegativeNumbers)
{
    constexpr std::string_view code = R"(
        return ~-1,
               ~-128,
               ~-255;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), ~(-1));
    EXPECT_EQ(behl::to_integer(S, -2), ~(-128));
    EXPECT_EQ(behl::to_integer(S, -1), ~(-255));
}

TEST_F(BitwiseTest, BitwiseAnd_StringOperands_ThrowsError)
{
    constexpr std::string_view code = R"(
        return "hello" & "world";
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, BitwiseAnd_TableOperands_ThrowsError)
{
    constexpr std::string_view code = R"(
        let t1 = {1, 2, 3};
        let t2 = {4, 5, 6};
        return t1 & t2;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, BitwiseOr_NilOperands_ThrowsError)
{
    constexpr std::string_view code = R"(
        return nil | 5;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, BitwiseXor_BooleanOperands_ThrowsError)
{
    constexpr std::string_view code = R"(
        return true ^ false;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, BitwiseNot_StringOperand_ThrowsError)
{
    constexpr std::string_view code = R"(
        return ~"test";
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, BitwiseNot_TableOperand_ThrowsError)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3};
        return ~t;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, LeftShift_StringOperand_ThrowsError)
{
    constexpr std::string_view code = R"(
        return 5 << "two";
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, RightShift_FunctionOperand_ThrowsError)
{
    constexpr std::string_view code = R"(
        function test() { return 42; }
        return 16 >> test;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(BitwiseTest, ComplexExpression_MixedOperations)
{
    constexpr std::string_view code = R"(
        return (5 & 3) | (2 ^ 6),
               (~7) & 0xFF,
               (1 << 4) | (16 >> 2);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), (5 & 3) | (2 ^ 6));
    EXPECT_EQ(behl::to_integer(S, -2), (~7) & 0xFF);
    EXPECT_EQ(behl::to_integer(S, -1), (1 << 4) | (16 >> 2));
}

TEST_F(BitwiseTest, ComplexExpression_Chained)
{
    constexpr std::string_view code = R"(
        return 0xFF & 0xF0 & 0x30,
               1 | 2 | 4 | 8,
               15 ^ 7 ^ 3;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 0xFF & 0xF0 & 0x30);
    EXPECT_EQ(behl::to_integer(S, -2), 1 | 2 | 4 | 8);
    EXPECT_EQ(behl::to_integer(S, -1), 15 ^ 7 ^ 3);
}

TEST_F(BitwiseTest, ComplexExpression_WithArithmetic)
{
    constexpr std::string_view code = R"(
        return (5 + 3) & (10 - 2),
               (2 * 4) | (16 / 4),
               (10 % 3) ^ 5;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), (5 + 3) & (10 - 2));
    EXPECT_EQ(behl::to_integer(S, -2), (2 * 4) | (16 / 4));
    EXPECT_EQ(behl::to_integer(S, -1), (10 % 3) ^ 5);
}

TEST_F(BitwiseTest, BitwiseAnd_RightOperandMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __band = function(a, b) {
                return 999;
            }
        };
        
        let obj = {value = 42};
        setmetatable(obj, mt);
        
        return 5 & obj;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 999);
}

TEST_F(BitwiseTest, BitwiseOr_RightOperandMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __bor = function(a, b) {
                return 777;
            }
        };
        
        let obj = {value = 42};
        setmetatable(obj, mt);
        
        return 10 | obj;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 777);
}

TEST_F(BitwiseTest, BitwiseXor_RightOperandMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __bxor = function(a, b) {
                return 555;
            }
        };
        
        let obj = {value = 42};
        setmetatable(obj, mt);
        
        return 15 ^ obj;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 555);
}

TEST_F(BitwiseTest, LeftShift_RightOperandMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __shl = function(a, b) {
                return 111;
            }
        };
        
        let obj = {value = 2};
        setmetatable(obj, mt);
        
        return 8 << obj;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 111);
}

TEST_F(BitwiseTest, RightShift_RightOperandMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __shr = function(a, b) {
                return 222;
            }
        };
        
        let obj = {value = 2};
        setmetatable(obj, mt);
        
        return 16 >> obj;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 222);
}

TEST_F(BitwiseTest, BitwiseNot_Metamethod)
{
    constexpr std::string_view code = R"(
        let mt = {
            __bnot = function(a) {
                return 333;
            }
        };
        
        let obj = {value = 42};
        setmetatable(obj, mt);
        
        return ~obj;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 333);
}

TEST_F(BitwiseTest, BitwiseAnd_ZeroValues)
{
    constexpr std::string_view code = R"(
        return 0 & 0,
               0 & 255,
               255 & 0;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 0);
    EXPECT_EQ(behl::to_integer(S, -2), 0);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(BitwiseTest, BitwiseOr_MaxValues)
{
    constexpr std::string_view code = R"(
        return 0xFF | 0xFF,
               0xFFFF | 0,
               0 | 0xFFFF;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 0xFF);
    EXPECT_EQ(behl::to_integer(S, -2), 0xFFFF);
    EXPECT_EQ(behl::to_integer(S, -1), 0xFFFF);
}

TEST_F(BitwiseTest, Shifts_ZeroShift)
{
    constexpr std::string_view code = R"(
        return 42 << 0,
               42 >> 0;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 42);
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(BitwiseTest, BitwiseNot_DoubleInversion)
{
    constexpr std::string_view code = R"(
        return ~~42,
               ~~0,
               ~~-1;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 42);
    EXPECT_EQ(behl::to_integer(S, -2), 0);
    EXPECT_EQ(behl::to_integer(S, -1), -1);
}

TEST_F(BitwiseTest, Variables_BitwiseOperations)
{
    constexpr std::string_view code = R"(
        let a = 0xF0;
        let b = 0x0F;
        let c = a & b;
        let d = a | b;
        let e = a ^ b;
        return c, d, e;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    EXPECT_EQ(behl::to_integer(S, -3), 0xF0 & 0x0F);
    EXPECT_EQ(behl::to_integer(S, -2), 0xF0 | 0x0F);
    EXPECT_EQ(behl::to_integer(S, -1), 0xF0 ^ 0x0F);
}
