#include "common/format.hpp"

#include <gtest/gtest.h>

TEST(FormatTest, NoPlaceholders)
{
    auto result = behl::format("Hello, World!");
    EXPECT_EQ(result, "Hello, World!");
}

TEST(FormatTest, SingleInteger)
{
    auto result = behl::format("Value: {}", 42);
    EXPECT_EQ(result, "Value: 42");
}

TEST(FormatTest, NegativeInteger)
{
    auto result = behl::format("Value: {}", -42);
    EXPECT_EQ(result, "Value: -42");
}

/*
TEST(FormatTest, MissingArgument)
{
    auto result = behl::format("Value: {} {}", -42);
    EXPECT_EQ(result, "Value: -42");
}
*/

TEST(FormatTest, MultipleIntegers)
{
    auto result = behl::format("{} + {} = {}", 1, 2, 3);
    EXPECT_EQ(result, "1 + 2 = 3");
}

TEST(FormatTest, UnsignedInteger)
{
    unsigned int value = 12345u;
    auto result = behl::format("Unsigned: {}", value);
    EXPECT_EQ(result, "Unsigned: 12345");
}

TEST(FormatTest, LongLong)
{
    long long value = 9223372036854775807LL;
    auto result = behl::format("Long long: {}", value);
    EXPECT_EQ(result, "Long long: 9223372036854775807");
}

TEST(FormatTest, FloatDefault)
{
    auto result = behl::format("Float: {}", 3.14159);
    EXPECT_EQ(result, "Float: 3.14159");
}

TEST(FormatTest, FloatValue)
{
    auto result = behl::format("Pi: {:f}", 3.14159);
    EXPECT_EQ(result, "Pi: 3.14159");
}

TEST(FormatTest, NegativeFloat)
{
    auto result = behl::format("Value: {}", -2.5);
    EXPECT_EQ(result, "Value: -2.5");
}

TEST(FormatTest, FloatAsFloat)
{
    float value = 1.5f;
    auto result = behl::format("Float: {}", value);
    EXPECT_EQ(result, "Float: 1.5");
}

TEST(FormatTest, DoubleType)
{
    double value = 2.718281828;
    auto result = behl::format("Double: {}", value);
    EXPECT_EQ(result, "Double: 2.718281828");
}

TEST(FormatTest, DoubleWithSpecifier)
{
    double value = 123.456789;
    auto result = behl::format("Value: {:f}", value);
    EXPECT_EQ(result, "Value: 123.456789");
}

TEST(FormatTest, DoubleFullPrecision)
{
    double value = 2.718281828923413;
    auto result = behl::format("{}", value);
    EXPECT_EQ(result, "2.718281828923413");
}

TEST(FormatTest, HexLowercase)
{
    auto result = behl::format("Hex: {:x}", 255);
    EXPECT_EQ(result, "Hex: ff");
}

TEST(FormatTest, HexUppercase)
{
    auto result = behl::format("Hex: {:X}", 255);
    EXPECT_EQ(result, "Hex: FF");
}

TEST(FormatTest, HexZero)
{
    auto result = behl::format("Hex: {:x}", 0);
    EXPECT_EQ(result, "Hex: 0");
}

TEST(FormatTest, HexLarge)
{
    auto result = behl::format("Hex: {:x}", 0xDEADBEEF);
    EXPECT_EQ(result, "Hex: deadbeef");
}

TEST(FormatTest, DecimalSpecifier)
{
    auto result = behl::format("Decimal: {:d}", 42);
    EXPECT_EQ(result, "Decimal: 42");
}

TEST(FormatTest, BoolTrue)
{
    auto result = behl::format("Bool: {}", true);
    EXPECT_EQ(result, "Bool: true");
}

TEST(FormatTest, BoolFalse)
{
    auto result = behl::format("Bool: {}", false);
    EXPECT_EQ(result, "Bool: false");
}

TEST(FormatTest, Character)
{
    auto result = behl::format("Char: {}", 'A');
    EXPECT_EQ(result, "Char: A");
}

TEST(FormatTest, CharSpecifier)
{
    auto result = behl::format("Char: {:c}", 'Z');
    EXPECT_EQ(result, "Char: Z");
}

TEST(FormatTest, CString)
{
    auto result = behl::format("String: {}", "Hello");
    EXPECT_EQ(result, "String: Hello");
}

TEST(FormatTest, CStringNull)
{
    const char* str = nullptr;
    auto result = behl::format("String: {}", str);
    EXPECT_EQ(result, "String: (null)");
}

TEST(FormatTest, StdString)
{
    std::string str = "World";
    auto result = behl::format("String: {}", str);
    EXPECT_EQ(result, "String: World");
}

TEST(FormatTest, StringView)
{
    std::string_view sv = "View";
    auto result = behl::format("String: {}", sv);
    EXPECT_EQ(result, "String: View");
}

TEST(FormatTest, StringSpecifier)
{
    auto result = behl::format("String: {:s}", "Test");
    EXPECT_EQ(result, "String: Test");
}

TEST(FormatTest, Pointer)
{
    int value = 42;
    int* ptr = &value;
    auto result = behl::format("Pointer: {}", ptr);
    EXPECT_TRUE(result.starts_with("Pointer: 0x"));
}

TEST(FormatTest, NullPointer)
{
    int* ptr = nullptr;
    auto result = behl::format("Pointer: {}", ptr);
    EXPECT_EQ(result, "Pointer: 0x0");
}

TEST(FormatTest, VoidPointer)
{
    int value = 42;
    void* ptr = &value;
    auto result = behl::format("Pointer: {:p}", ptr);
    EXPECT_TRUE(result.starts_with("Pointer: 0x"));
}

TEST(FormatTest, EscapedBraces)
{
    auto result = behl::format("Braces: {{}}");
    EXPECT_EQ(result, "Braces: {}");
}

TEST(FormatTest, EscapedBracesWithValue)
{
    auto result = behl::format("{{Value: {}}}", 42);
    EXPECT_EQ(result, "{Value: 42}");
}

TEST(FormatTest, MultipleEscapedBraces)
{
    auto result = behl::format("{{{{}}}}");
    EXPECT_EQ(result, "{{}}");
}

TEST(FormatTest, MixedTypes)
{
    auto result = behl::format("Int: {}, Float: {}, String: {}, Bool: {}", 42, 3.14, "Hello", true);
    EXPECT_EQ(result, "Int: 42, Float: 3.14, String: Hello, Bool: true");
}

TEST(FormatTest, MixedTypesWithSpecifiers)
{
    auto result = behl::format("Hex: {:x}, Decimal: {:d}, Float: {:f}", 255, 100, 2.5);
    EXPECT_EQ(result, "Hex: ff, Decimal: 100, Float: 2.5");
}

TEST(FormatTest, EmptyString)
{
    auto result = behl::format("");
    EXPECT_EQ(result, "");
}

TEST(FormatTest, OnlyPlaceholder)
{
    auto result = behl::format("{}", 42);
    EXPECT_EQ(result, "42");
}

TEST(FormatTest, MultiplePlaceholdersNoText)
{
    auto result = behl::format("{}{}{}", 1, 2, 3);
    EXPECT_EQ(result, "123");
}

TEST(FormatTest, ZeroValue)
{
    auto result = behl::format("Zero: {}", 0);
    EXPECT_EQ(result, "Zero: 0");
}

TEST(FormatTest, EmptyStringArg)
{
    auto result = behl::format("Empty: '{}'", "");
    EXPECT_EQ(result, "Empty: ''");
}

TEST(FormatTest, LogMessage)
{
    auto result = behl::format("[{}] {}: Value={}, Status={}", "INFO", "Test", 123, true);
    EXPECT_EQ(result, "[INFO] Test: Value=123, Status=true");
}

TEST(FormatTest, DebugOutput)
{
    int* ptr = reinterpret_cast<int*>(static_cast<uintptr_t>(0xDEADBEEF));
    auto result = behl::format("Object at {:p} has value {}", ptr, 42);
    EXPECT_EQ(result, "Object at 0xdeadbeef has value 42");
}

TEST(FormatTest, FormatStringAlias)
{
    behl::format_string<int> fmt_str = "Number: {}";
    auto result = behl::format(fmt_str, 123);
    EXPECT_EQ(result, "Number: 123");
}

TEST(FormatTest, LongString)
{
    std::string prefix(1000, 'x');
    auto result = behl::format("{}{}", prefix, 42);
    EXPECT_EQ(result.size(), 1002);
    EXPECT_TRUE(result.ends_with("42"));
}

TEST(FormatTest, ManyArguments)
{
    auto result = behl::format("{} {} {} {} {} {} {} {} {} {}", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    EXPECT_EQ(result, "1 2 3 4 5 6 7 8 9 10");
}

TEST(FormatTest, LongType)
{
    long value = 1234567890L;
    auto result = behl::format("Long: {}", value);
    EXPECT_EQ(result, "Long: 1234567890");
}

TEST(FormatTest, UnsignedLongType)
{
    unsigned long value = 1234567890UL;
    auto result = behl::format("Unsigned long: {}", value);
    EXPECT_EQ(result, "Unsigned long: 1234567890");
}

TEST(FormatTest, UnsignedLongLongType)
{
    unsigned long long value = 18446744073709551615ULL;
    auto result = behl::format("Unsigned long long: {}", value);
    EXPECT_EQ(result, "Unsigned long long: 18446744073709551615");
}

TEST(FormatTest, NaN)
{
    double value = std::nan("");
    auto result = behl::format("NaN: {}", value);
    EXPECT_EQ(result, "NaN: nan");
}

TEST(FormatTest, PositiveInfinity)
{
    constexpr double value = std::numeric_limits<double>::infinity();
    auto result = behl::format("Inf: {}", value);
    EXPECT_EQ(result, "Inf: inf");
}

TEST(FormatTest, NegativeInfinity)
{
    constexpr double value = -std::numeric_limits<double>::infinity();
    auto result = behl::format("Inf: {}", value);
    EXPECT_EQ(result, "Inf: -inf");
}

TEST(FormatTest, HexWithUnsigned)
{
    unsigned int value = 4294967295u;
    auto result = behl::format("Max uint: {:x}", value);
    EXPECT_EQ(result, "Max uint: ffffffff");
}

TEST(FormatTest, HexWithLongLong)
{
    long long value = 0x123456789ABCDEFLL;
    auto result = behl::format("Hex LL: {:x}", value);
    EXPECT_EQ(result, "Hex LL: 123456789abcdef");
}

TEST(FormatTest, WidthLeftAlign)
{
    auto result = behl::format("{:<10}", "hello");
    EXPECT_EQ(result, "hello     ");
}

TEST(FormatTest, WidthRightAlign)
{
    auto result = behl::format("{:>10}", "hello");
    EXPECT_EQ(result, "     hello");
}

TEST(FormatTest, WidthCenterAlign)
{
    auto result = behl::format("{:^10}", "hello");
    EXPECT_EQ(result, "  hello   ");
}

TEST(FormatTest, WidthCenterAlignOdd)
{
    auto result = behl::format("{:^11}", "hello");
    EXPECT_EQ(result, "   hello   ");
}

TEST(FormatTest, WidthDefaultAlign)
{
    auto result = behl::format("{:10}", "hello");
    EXPECT_EQ(result, "hello     ");
}

TEST(FormatTest, WidthIntegerLeftAlign)
{
    auto result = behl::format("{:<5}", 42);
    EXPECT_EQ(result, "42   ");
}

TEST(FormatTest, WidthIntegerRightAlign)
{
    auto result = behl::format("{:>5}", 42);
    EXPECT_EQ(result, "   42");
}

TEST(FormatTest, WidthIntegerCenterAlign)
{
    auto result = behl::format("{:^5}", 42);
    EXPECT_EQ(result, " 42  ");
}

TEST(FormatTest, WidthFloatRightAlign)
{
    auto result = behl::format("{:>10}", 3.14);
    EXPECT_EQ(result, "      3.14");
}

TEST(FormatTest, WidthBoolLeftAlign)
{
    auto result = behl::format("{:<8}", true);
    EXPECT_EQ(result, "true    ");
}

TEST(FormatTest, WidthFillCharLeftAlign)
{
    auto result = behl::format("{:*<10}", "test");
    EXPECT_EQ(result, "test******");
}

TEST(FormatTest, WidthFillCharRightAlign)
{
    auto result = behl::format("{:*>10}", "test");
    EXPECT_EQ(result, "******test");
}

TEST(FormatTest, WidthFillCharCenterAlign)
{
    auto result = behl::format("{:*^10}", "test");
    EXPECT_EQ(result, "***test***");
}

TEST(FormatTest, WidthFillCharInteger)
{
    auto result = behl::format("{:0>5}", 42);
    EXPECT_EQ(result, "00042");
}

TEST(FormatTest, WidthFillCharHex)
{
    auto result = behl::format("{:0>6x}", 255);
    EXPECT_EQ(result, "0000ff");
}

TEST(FormatTest, WidthNoExtraPaddingNeeded)
{
    auto result = behl::format("{:<5}", "testing");
    EXPECT_EQ(result, "testing");
}

TEST(FormatTest, WidthExactFit)
{
    auto result = behl::format("{:<5}", "hello");
    EXPECT_EQ(result, "hello");
}

TEST(FormatTest, WidthWithStringView)
{
    std::string_view sv = "view";
    auto result = behl::format("{:>8}", sv);
    EXPECT_EQ(result, "    view");
}

TEST(FormatTest, WidthMultipleAlignments)
{
    auto result = behl::format("{:<5} {:>5} {:^5}", "a", "b", "c");
    EXPECT_EQ(result, "a         b   c  ");
}

TEST(FormatTest, WidthWithSpecifierLeftAlign)
{
    auto result = behl::format("{:<8x}", 255);
    EXPECT_EQ(result, "ff      ");
}

TEST(FormatTest, WidthWithSpecifierRightAlign)
{
    auto result = behl::format("{:>8X}", 255);
    EXPECT_EQ(result, "      FF");
}

TEST(FormatTest, WidthUnderscoreFill)
{
    auto result = behl::format("{:_^12}", "center");
    EXPECT_EQ(result, "___center___");
}

TEST(FormatTest, WidthDashFill)
{
    auto result = behl::format("{:->15}", "right");
    EXPECT_EQ(result, "----------right");
}

TEST(FormatTest, WidthPlusFill)
{
    auto result = behl::format("{:+<10}", 123);
    EXPECT_EQ(result, "123+++++++");
}
