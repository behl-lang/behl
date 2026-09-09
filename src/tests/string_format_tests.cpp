#include <behl/behl.hpp>
#include <gtest/gtest.h>

namespace behl
{
    class StringFormatTest : public ::testing::Test
    {
    protected:
        State* S;
        void SetUp() override
        {
            S = new_state();
            load_stdlib(S);
        }
        void TearDown() override
        {
            close(S);
        }
    };

    TEST_F(StringFormatTest, BasicFormatting)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("Value: {}, Name: {}", 42, "test");
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Value: 42, Name: test");
    }

    TEST_F(StringFormatTest, FloatPrecision)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("Pi: {:.2f}", 3.14159);
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Pi: 3.14");
    }

    TEST_F(StringFormatTest, HexFormatting)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("Hex: 0x{:X}", 255);
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Hex: 0xFF");
    }

    TEST_F(StringFormatTest, PaddedFormatting)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("Padded: {:5}", 42);
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Padded:    42");
    }

    TEST_F(StringFormatTest, EscapedBraces)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("Braces: {{ and }}");
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Braces: { and }");
    }

    TEST_F(StringFormatTest, MultipleArguments)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let name = "Alice";
            let age = 30;
            let score = 95.5;
            let result = string.format("{} is {} years old with score {:.1f}", name, age, score);
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Alice is 30 years old with score 95.5");
    }

    TEST_F(StringFormatTest, MetatableToString)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            
            let point = {x = 10, y = 20};
            let mt = {};
            mt.__tostring = function(self) {
                return 'Point(' + tostring(self.x) + ', ' + tostring(self.y) + ')';
            };
            setmetatable(point, mt);
            
            let result = string.format("Location: {}", point);
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Location: Point(10, 20)");
    }

    TEST_F(StringFormatTest, ExplicitIndexing)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("{0} {1} {2}", "a", "b", "c");
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "a b c");
    }

    TEST_F(StringFormatTest, ReorderedArguments)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("{2} {0} {1}", "first", "second", "third");
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "third first second");
    }

    TEST_F(StringFormatTest, RepeatedArguments)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("{0} {0} {0}", "echo");
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "echo echo echo");
    }

    TEST_F(StringFormatTest, IndexWithFormatSpec)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("{0:.2f} and {1:X}", 3.14159, 255);
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "3.14 and FF");
    }

    TEST_F(StringFormatTest, ReuseWithDifferentSpecs)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("Decimal: {0}, Hex: {0:X}, Padded: {0:5}", 42);
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "Decimal: 42, Hex: 2A, Padded:    42");
    }

    TEST_F(StringFormatTest, MixedAutoAndExplicitIndexing)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("{1} comes after {0}", "first", "second");
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "second comes after first");
    }

    TEST_F(StringFormatTest, IndexWithAlignment)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let result = string.format("{0:<10} {0:>10} {0:^10}", "test");
            return result;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_EQ(to_string(S, -1), "test             test    test   ");
    }

} // namespace behl
