#include "common/format.hpp"

#include <gtest/gtest.h>

namespace behl
{
    TEST(FormatTest, BasicFormatting)
    {
        EXPECT_EQ(behl::format("Hello, {}!", "world"), "Hello, world!");
        EXPECT_EQ(behl::format("{} + {} = {}", 1, 2, 3), "1 + 2 = 3");
    }

    TEST(FormatTest, IntegerFormatting)
    {
        EXPECT_EQ(behl::format("{}", 42), "42");
        EXPECT_EQ(behl::format("{}", -42), "-42");
        EXPECT_EQ(behl::format("{:d}", 42), "42");
    }

    TEST(FormatTest, HexFormatting)
    {
        EXPECT_EQ(behl::format("{:x}", 255), "ff");
        EXPECT_EQ(behl::format("{:X}", 255), "FF");
        EXPECT_EQ(behl::format("0x{:X}", 255), "0xFF");
        EXPECT_EQ(behl::format("{:x}", 0), "0");
    }

    TEST(FormatTest, FloatFormatting)
    {
        EXPECT_EQ(behl::format("{}", 3.14), "3.14");
        EXPECT_EQ(behl::format("{:.2f}", 3.14159), "3.14");
        EXPECT_EQ(behl::format("{:.0f}", 3.14159), "3");
        EXPECT_EQ(behl::format("{:.4f}", 3.14), "3.1400");
    }

    TEST(FormatTest, BoolFormatting)
    {
        EXPECT_EQ(behl::format("{}", true), "true");
        EXPECT_EQ(behl::format("{}", false), "false");
    }

    TEST(FormatTest, StringFormatting)
    {
        EXPECT_EQ(behl::format("{}", "test"), "test");
        EXPECT_EQ(behl::format("{}", std::string("test")), "test");
        EXPECT_EQ(behl::format("{}", std::string_view("test")), "test");
    }

    TEST(FormatTest, PointerFormatting)
    {
        int x = 42;
        const void* ptr = &x;
        std::string result = behl::format("{}", ptr);
        EXPECT_FALSE(result.empty());
        EXPECT_NE(result, "0x0");
    }

    TEST(FormatTest, WidthFormatting)
    {
        EXPECT_EQ(behl::format("{:5}", 42), "   42");
        EXPECT_EQ(behl::format("{:10}", "test"), "test      ");
        EXPECT_EQ(behl::format("{:3}", 12345), "12345");
    }

    TEST(FormatTest, AlignmentLeft)
    {
        EXPECT_EQ(behl::format("{:<10}", "test"), "test      ");
        EXPECT_EQ(behl::format("{:<5}", 42), "42   "); // Explicit left alignment
    }

    TEST(FormatTest, AlignmentRight)
    {
        EXPECT_EQ(behl::format("{:>10}", "test"), "      test");
        EXPECT_EQ(behl::format("{:>5}", 42), "   42");
    }

    TEST(FormatTest, AlignmentCenter)
    {
        EXPECT_EQ(behl::format("{:^10}", "test"), "   test   ");
        EXPECT_EQ(behl::format("{:^6}", 42), "  42  ");
    }

    TEST(FormatTest, EscapedBraces)
    {
        EXPECT_EQ(behl::format("{{}}"), "{}");
        EXPECT_EQ(behl::format("{{ and }}"), "{ and }");
        EXPECT_EQ(behl::format("{{{}}}", 42), "{42}");
        EXPECT_EQ(behl::format("{{{{"), "{{");
        EXPECT_EQ(behl::format("}}}}"), "}}");
        EXPECT_EQ(behl::format("{{{{}}}}"), "{{}}");
        EXPECT_EQ(behl::format("Start {{ middle }} end"), "Start { middle } end");
        EXPECT_EQ(behl::format("{{}} {} {{}}", "value"), "{} value {}");
        EXPECT_EQ(behl::format("{{ {0} }}", 42), "{ 42 }");
    }

    TEST(FormatTest, ExplicitIndexing)
    {
        EXPECT_EQ(behl::format("{0} {1} {2}", "a", "b", "c"), "a b c");
        EXPECT_EQ(behl::format("{0}", 42), "42");
    }

    TEST(FormatTest, ReorderedArguments)
    {
        EXPECT_EQ(behl::format("{2} {0} {1}", "first", "second", "third"), "third first second");
        EXPECT_EQ(behl::format("{1} comes before {0}", "second", "first"), "first comes before second");
    }

    TEST(FormatTest, RepeatedArguments)
    {
        EXPECT_EQ(behl::format("{0} {0} {0}", "echo"), "echo echo echo");
        EXPECT_EQ(behl::format("{0} != {1}, but {0} == {0}", 1, 2), "1 != 2, but 1 == 1");
    }

    TEST(FormatTest, IndexWithFormatSpec)
    {
        EXPECT_EQ(behl::format("{0:.2f} and {1:X}", 3.14159, 255), "3.14 and FF");
        EXPECT_EQ(behl::format("{1:x} {0:d}", 10, 20), "14 10");
    }

    TEST(FormatTest, ReuseWithDifferentSpecs)
    {
        EXPECT_EQ(behl::format("Dec: {0}, Hex: {0:X}, Width: {0:5}", 42), "Dec: 42, Hex: 2A, Width:    42");
        EXPECT_EQ(behl::format("{0:.1f} vs {0:.3f}", 3.14159), "3.1 vs 3.142");
    }

    TEST(FormatTest, IndexWithAlignment)
    {
        EXPECT_EQ(behl::format("{0:<5} {0:>5} {0:^5}", "ab"), "ab       ab  ab  ");
    }

    TEST(FormatTest, MixedTypes)
    {
        EXPECT_EQ(behl::format("{} {} {} {}", 42, 3.14, "test", true), "42 3.14 test true");
        EXPECT_EQ(behl::format("{0} {1} {0}", 100, "middle"), "100 middle 100");
    }

    TEST(FormatTest, EmptyFormatString)
    {
        EXPECT_EQ(behl::format(""), "");
    }

    TEST(FormatTest, NoArguments)
    {
        EXPECT_EQ(behl::format("Just text"), "Just text");
        EXPECT_EQ(behl::format("Text with {{ braces }}"), "Text with { braces }");
    }

    TEST(FormatTest, ComplexFormat)
    {
        EXPECT_EQ(behl::format("Result: {0:.2f} ({1:X}), Status: {2}", 3.14159, 200, "OK"), "Result: 3.14 (C8), Status: OK");
    }

    TEST(FormatTest, ErrorUnmatchedOpenBrace)
    {
        EXPECT_THROW(behl::format("{", 42), std::runtime_error);
        EXPECT_THROW(behl::format("test {", 42), std::runtime_error);
    }

    TEST(FormatTest, ErrorUnmatchedCloseBrace)
    {
        EXPECT_THROW(behl::format("}", 42), std::runtime_error);
        EXPECT_THROW(behl::format("test }", 42), std::runtime_error);
    }

    TEST(FormatTest, ErrorNotEnoughArguments)
    {
        EXPECT_THROW(behl::format("{} {}", 42), std::runtime_error);
        EXPECT_THROW(behl::format("{0} {1}", 42), std::runtime_error);
    }

    TEST(FormatTest, ErrorInvalidArgumentIndex)
    {
        EXPECT_THROW(behl::format("{5}", 1, 2, 3), std::runtime_error);
    }

    // NTTP compile-time format tests
    TEST(FormatTest, NTTPBasic)
    {
        EXPECT_EQ(behl::format<"Hello, {}!">("world"), "Hello, world!");
        EXPECT_EQ(behl::format<"{} + {} = {}">(1, 2, 3), "1 + 2 = 3");
    }

    TEST(FormatTest, NTTPInteger)
    {
        EXPECT_EQ(behl::format<"{}">(42), "42");
        EXPECT_EQ(behl::format<"{}">(-42), "-42");
        EXPECT_EQ(behl::format<"{:d}">(42), "42");
    }

    TEST(FormatTest, NTTPHex)
    {
        EXPECT_EQ(behl::format<"{:x}">(255), "ff");
        EXPECT_EQ(behl::format<"{:X}">(255), "FF");
        EXPECT_EQ(behl::format<"0x{:X}">(255), "0xFF");
    }

    TEST(FormatTest, NTTPFloat)
    {
        EXPECT_EQ(behl::format<"{}">(3.14), "3.14");
        EXPECT_EQ(behl::format<"{:.2f}">(3.14159), "3.14");
        EXPECT_EQ(behl::format<"{:.0f}">(3.14159), "3");
    }

    TEST(FormatTest, NTTPArgumentIndexing)
    {
        EXPECT_EQ(behl::format<"{1} {0}">("world", "Hello"), "Hello world");
        EXPECT_EQ(behl::format<"{0} {1} {0}">(100, "middle"), "100 middle 100");
    }

    TEST(FormatTest, NTTPMixedTypes)
    {
        EXPECT_EQ(behl::format<"Result: {0:.2f} ({1:X}), Status: {2}">(3.14159, 200, "OK"), "Result: 3.14 (C8), Status: OK");
    }

    TEST(FormatTest, NTTPNoArgs)
    {
        EXPECT_EQ(behl::format<"No placeholders">(), "No placeholders");
        EXPECT_EQ(behl::format<"Escaped {{ braces }}">(), "Escaped { braces }");
        EXPECT_EQ(behl::format<"{{{{}}}}">(), "{{}}");
        EXPECT_EQ(behl::format<"{{ {0} }}">(42), "{ 42 }");
    }

    // Dynamic width and precision tests
    TEST(FormatTest, DynamicWidth)
    {
        EXPECT_EQ(behl::format("{:{}}", 42, 5), "   42");
        EXPECT_EQ(behl::format("{:{}}", 42, 3), " 42");
        EXPECT_EQ(behl::format("{:{}}", 42, 1), "42");
        EXPECT_EQ(behl::format("{:<{}}", 42, 5), "42   ");
        EXPECT_EQ(behl::format("{:>{}}", 42, 5), "   42");
        EXPECT_EQ(behl::format("{:^{}}", 42, 5), " 42  ");
    }

    TEST(FormatTest, DynamicPrecision)
    {
        EXPECT_EQ(behl::format("{:.{}}", 3.14159, 2), "3.14");
        EXPECT_EQ(behl::format("{:.{}}", 3.14159, 4), "3.1416");
        EXPECT_EQ(behl::format("{:.{}}", 3.14159, 0), "3");
        EXPECT_EQ(behl::format("{:.{}}", 2.5, 3), "2.500");
    }

    TEST(FormatTest, DynamicWidthAndPrecision)
    {
        EXPECT_EQ(behl::format("{:{}.{}}", 3.14159, 8, 2), "    3.14");
        EXPECT_EQ(behl::format("{:{}.{}}", 3.14159, 6, 3), " 3.142");
        EXPECT_EQ(behl::format("{:<{}.{}}", 3.14159, 8, 2), "3.14    ");
        EXPECT_EQ(behl::format("{:>{}.{}}", 3.14159, 8, 2), "    3.14");
        EXPECT_EQ(behl::format("{:^{}.{}}", 3.14159, 8, 2), "  3.14  ");
    }

    TEST(FormatTest, DynamicMultipleArgs)
    {
        // Simple sequential consumption
        EXPECT_EQ(behl::format("{:{}} {}", 42, 5, 3.14), "   42 3.14");
        EXPECT_EQ(behl::format("{} {:{}.{}}", 42, 3.14, 8, 2), "42     3.14");
    }

    // Indexed dynamic width and precision tests
    TEST(FormatTest, IndexedDynamicWidth)
    {
        EXPECT_EQ(behl::format("{0:{1}}", 42, 5), "   42");
        EXPECT_EQ(behl::format("{0:{1}}", 42, 3), " 42");
        EXPECT_EQ(behl::format("{0:<{1}}", 42, 5), "42   ");
        EXPECT_EQ(behl::format("{0:>{1}}", 42, 5), "   42");
        EXPECT_EQ(behl::format("{0:^{1}}", 42, 5), " 42  ");
    }

    TEST(FormatTest, IndexedDynamicPrecision)
    {
        EXPECT_EQ(behl::format("{0:.{1}}", 3.14159, 2), "3.14");
        EXPECT_EQ(behl::format("{0:.{1}}", 3.14159, 4), "3.1416");
        EXPECT_EQ(behl::format("{0:.{1}}", 3.14159, 0), "3");
        EXPECT_EQ(behl::format("{0:.{1}}", 2.5, 3), "2.500");
    }

    TEST(FormatTest, IndexedDynamicWidthAndPrecision)
    {
        EXPECT_EQ(behl::format("{0:{1}.{2}}", 3.14159, 8, 2), "    3.14");
        EXPECT_EQ(behl::format("{0:{1}.{2}}", 3.14159, 6, 3), " 3.142");
        EXPECT_EQ(behl::format("{0:<{1}.{2}}", 3.14159, 8, 2), "3.14    ");
        EXPECT_EQ(behl::format("{0:>{1}.{2}}", 3.14159, 8, 2), "    3.14");
        EXPECT_EQ(behl::format("{0:^{1}.{2}}", 3.14159, 8, 2), "  3.14  ");
    }

    TEST(FormatTest, IndexedDynamicMixedOrder)
    {
        // Arguments can be referenced out of order
        EXPECT_EQ(behl::format("{1:{0}}", 5, 42), "   42");
        EXPECT_EQ(behl::format("{2:{0}.{1}}", 8, 2, 3.14159), "    3.14");
        EXPECT_EQ(behl::format("{0} {2:{1}}", "Value:", 6, 123), "Value:    123");
    }

} // namespace behl
