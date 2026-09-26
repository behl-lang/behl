#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include <string_view>

using namespace std::string_view_literals;

namespace behl
{
    class StringLibTest : public ::testing::TestWithParam<bool>
    {
    protected:
        State* S;
        void SetUp() override
        {
            S = new_state();
            S->jit_enabled = GetParam();
            load_stdlib(S);
        }
        void TearDown() override
        {
            close(S);
        }
    };

    TEST_P(StringLibTest, SubNegativeIndicesCountFromTheEnd)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let s = "hello";
            return string.sub(s, -3, -1), string.sub(s, -3), string.sub(s, 0, -5), string.sub(s, -100, 1);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
        EXPECT_EQ(to_string(S, -4), "llo");
        EXPECT_EQ(to_string(S, -3), "llo");
        EXPECT_EQ(to_string(S, -2), "h");
        EXPECT_EQ(to_string(S, -1), "he");
    }

    TEST_P(StringLibTest, SubSingleCharacterAndStartAfterEnd)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let s = "hello";
            return string.sub(s, 0, 0), string.sub(s, 4, 4), string.sub(s, 3, 1), string.sub(s, 0, -6);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
        EXPECT_EQ(to_string(S, -4), "h");
        EXPECT_EQ(to_string(S, -3), "o");
        EXPECT_EQ(to_string(S, -2), "");
        EXPECT_EQ(to_string(S, -1), "");
    }

    TEST_P(StringLibTest, SubBeyondLengthIsClamped)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let s = "hello";
            return string.sub(s, 5), string.sub(s, 2, 100), string.sub(s, 100, 200);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        EXPECT_EQ(to_string(S, -3), "");
        EXPECT_EQ(to_string(S, -2), "llo");
        EXPECT_EQ(to_string(S, -1), "");
    }

    TEST_P(StringLibTest, SubWithIntegerExtremes)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let maxi = 9223372036854775807;
            let mini = -9223372036854775807 - 1;
            let s = "hello";
            return string.sub(s, mini, maxi), string.sub(s, maxi, maxi), string.sub(s, mini, mini), string.sub(s, 0, mini);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
        EXPECT_EQ(to_string(S, -4), "hello");
        EXPECT_EQ(to_string(S, -3), "");
        EXPECT_EQ(to_string(S, -2), "");
        EXPECT_EQ(to_string(S, -1), "");
    }

    TEST_P(StringLibTest, SubOfEmptyStringIsEmpty)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            return string.sub("", 0), string.sub("", 0, 0), string.sub("", -1, -1);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        EXPECT_EQ(to_string(S, -3), "");
        EXPECT_EQ(to_string(S, -2), "");
        EXPECT_EQ(to_string(S, -1), "");
    }

    TEST_P(StringLibTest, SubPreservesNulBytes)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let z = "a" + string.char(0) + "b" + string.char(0);
            return string.sub(z, 1, 2), string.sub(z, -1), string.sub(z, 0);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        EXPECT_EQ(to_string(S, -3), "\0b"sv);
        EXPECT_EQ(to_string(S, -2), "\0"sv);
        EXPECT_EQ(to_string(S, -1), "a\0b\0"sv);
    }

    TEST_P(StringLibTest, FindFromStartOffset)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            return string.find("abcabc", "abc", 1), string.find("abcabc", "abc", 3), string.find("abcabc", "abc", 4),
                string.find("abc", "c", 2);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
        EXPECT_EQ(to_integer(S, -4), 3);
        EXPECT_EQ(to_integer(S, -3), 3);
        EXPECT_EQ(to_integer(S, -2), -1);
        EXPECT_EQ(to_integer(S, -1), 2);
    }

    TEST_P(StringLibTest, FindNegativeStartCountsFromTheEnd)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            return string.find("abcabc", "abc", -3), string.find("abcabc", "c", -1), string.find("abcabc", "a", -2);
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        EXPECT_EQ(to_integer(S, -3), 3);
        EXPECT_EQ(to_integer(S, -2), 5);
        EXPECT_EQ(to_integer(S, -1), -1);
    }

    TEST_P(StringLibTest, FindNotFoundReturnsMinusOne)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            return string.find("abc", "d"), string.find("abc", "abcd"), string.find("", "a");
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        EXPECT_EQ(type(S, -3), Type::kInteger);
        EXPECT_EQ(to_integer(S, -3), -1);
        EXPECT_EQ(to_integer(S, -2), -1);
        EXPECT_EQ(to_integer(S, -1), -1);
    }

    TEST_P(StringLibTest, FindMatchesNulBytes)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let z = "a" + string.char(0) + "b" + string.char(0);
            return string.find(z, string.char(0)), string.find(z, string.char(0), 2), string.find(z, "b" + string.char(0));
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        EXPECT_EQ(to_integer(S, -3), 1);
        EXPECT_EQ(to_integer(S, -2), 3);
        EXPECT_EQ(to_integer(S, -1), 2);
    }

    TEST_P(StringLibTest, FindTreatsPatternCharactersLiterally)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            return string.find("a.b*c", "."), string.find("a.b*c", "*"), string.find("x(%[^$", "%["),
                string.find("x(%[^$", "^$"), string.find("abc", ".*");
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
        EXPECT_EQ(to_integer(S, -5), 1);
        EXPECT_EQ(to_integer(S, -4), 3);
        EXPECT_EQ(to_integer(S, -3), 2);
        EXPECT_EQ(to_integer(S, -2), 4);
        EXPECT_EQ(to_integer(S, -1), -1);
    }

    TEST_P(StringLibTest, LengthCountsNulBytes)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let z = "a" + string.char(0) + "b" + string.char(0);
            return string.len(z), #z, string.len(""), #"", string.len(string.char(0));
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 5));
        EXPECT_EQ(to_integer(S, -5), 4);
        EXPECT_EQ(to_integer(S, -4), 4);
        EXPECT_EQ(to_integer(S, -3), 0);
        EXPECT_EQ(to_integer(S, -2), 0);
        EXPECT_EQ(to_integer(S, -1), 1);
    }

    TEST_P(StringLibTest, ReverseHandlesNulAndEmpty)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let z = "a" + string.char(0) + "b" + string.char(0);
            return string.reverse(z), string.reverse(""), string.reverse("x");
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
        EXPECT_EQ(to_string(S, -3), "\0b\0a"sv);
        EXPECT_EQ(to_string(S, -2), "");
        EXPECT_EQ(to_string(S, -1), "x");
    }

    TEST_P(StringLibTest, UpperLowerHandleNulAndEmpty)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let z = "a" + string.char(0) + "b" + string.char(0);
            return string.upper(z), string.lower("A" + string.char(0) + "B"), string.upper(""), string.lower("");
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
        EXPECT_EQ(to_string(S, -4), "A\0B\0"sv);
        EXPECT_EQ(to_string(S, -3), "a\0b"sv);
        EXPECT_EQ(to_string(S, -2), "");
        EXPECT_EQ(to_string(S, -1), "");
    }

    TEST_P(StringLibTest, ByteCharRoundTripForEveryByteValue)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let bad = -1;
            for (let i = 0; i < 256; i++) {
                let c = string.char(i);
                if (bad == -1 && (string.len(c) != 1 || string.byte(c) != i || string.byte(c, 0) != i)) {
                    bad = i;
                }
            }
            return bad;
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
        EXPECT_EQ(to_integer(S, -1), -1);
    }

    TEST_P(StringLibTest, CharWithSeveralCodesAndByteRange)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let all = string.char(0, 1, 127, 128, 255);
            return all, string.byte(all, 3), string.byte(all, 4), string.byte(all, 5), string.char(), string.byte("");
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 6));
        EXPECT_EQ(to_string(S, -6), "\x00\x01\x7f\x80\xff"sv);
        EXPECT_EQ(to_integer(S, -5), 128);
        EXPECT_EQ(to_integer(S, -4), 255);
        EXPECT_TRUE(is_nil(S, -3));
        EXPECT_EQ(to_string(S, -2), "");
        EXPECT_TRUE(is_nil(S, -1));
    }

    TEST_P(StringLibTest, RepEdges)
    {
        constexpr std::string_view code = R"(
            const string = import("string");
            let mini = -9223372036854775807 - 1;
            let z = "a" + string.char(0);
            return string.rep("ab", 1), string.rep("x", mini), string.rep(z, 2), string.len(string.rep("a", 1000));
        )";

        ASSERT_TRUE(behl_test::load_ok(S, code));
        ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
        EXPECT_EQ(to_string(S, -4), "ab");
        EXPECT_EQ(to_string(S, -3), "");
        EXPECT_EQ(to_string(S, -2), "a\0a\0"sv);
        EXPECT_EQ(to_integer(S, -1), 1000);
    }

    INSTANTIATE_TEST_SUITE_P(Mode, StringLibTest, ::testing::Bool(),
        [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });

}
