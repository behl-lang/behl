#include "common/charconv.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <string>
#include <string_view>

namespace
{
    template<typename T>
    std::string to_text(T value)
    {
        std::array<char, 64> buf{};
        const auto r = behl::to_chars(buf.data(), buf.data() + buf.size(), value);
        EXPECT_EQ(r.ec, std::errc{});
        return std::string(buf.data(), r.ptr);
    }

    template<typename T>
    T parse(std::string_view text, std::errc expected = std::errc{})
    {
        T out{};
        const auto r = behl::from_chars(text.data(), text.data() + text.size(), out);
        EXPECT_EQ(r.ec, expected) << "input: " << text;
        return out;
    }

    template<typename T>
    using Bits = std::conditional_t<sizeof(T) == 8, uint64_t, uint32_t>;

    template<typename T>
    void expect_round_trip(T value)
    {
        const std::string text = to_text(value);
        const T back = parse<T>(text);
        ASSERT_EQ(std::bit_cast<Bits<T>>(back), std::bit_cast<Bits<T>>(value))
            << "value printed as \"" << text << "\" did not survive the round trip";
    }
} // namespace

namespace
{
    template<typename T>
    constexpr bool to_chars_equals(T value, std::string_view expected)
    {
        std::array<char, 64> buf{};
        const auto r = behl::to_chars(buf.data(), buf.data() + buf.size(), value);
        return r.ec == std::errc{} && std::string_view(buf.data(), static_cast<size_t>(r.ptr - buf.data())) == expected;
    }

    constexpr uint64_t parsed_bits(std::string_view text)
    {
        double out = 0.0;
        behl::from_chars(text.data(), text.data() + text.size(), out);
        return std::bit_cast<uint64_t>(out);
    }

    constexpr bool round_trips(double value)
    {
        std::array<char, 64> buf{};
        const auto r = behl::to_chars(buf.data(), buf.data() + buf.size(), value);
        double back = 0.0;
        behl::from_chars(buf.data(), r.ptr, back);
        return std::bit_cast<uint64_t>(back) == std::bit_cast<uint64_t>(value);
    }

    static_assert(to_chars_equals(0.1, "0.1"));
    static_assert(to_chars_equals(1.5, "1.5"));
    static_assert(to_chars_equals(-0.0, "-0.0"));
    static_assert(to_chars_equals(1e21, "1e+21"));
    static_assert(to_chars_equals(5e-324, "5e-324"));
    static_assert(to_chars_equals(1.7976931348623157e308, "1.7976931348623157e+308"));
    static_assert(to_chars_equals(123456789012345678.0, "1.2345678901234568e+17"));
    static_assert(to_chars_equals(-9223372036854775807LL - 1, "-9223372036854775808"));
    static_assert(to_chars_equals(18446744073709551615ull, "18446744073709551615"));
    static_assert(parsed_bits("0.1") == std::bit_cast<uint64_t>(0.1));
    static_assert(parsed_bits("3.518437208883201171875e13") == 0x42c0000000000002ull);
    static_assert(parsed_bits("1.00000000000000011102230246251565404236316680908203125") == 0x3ff0000000000000ull);
    static_assert(round_trips(3.141592653589793));
    static_assert(round_trips(2.2250738585072014e-308));
} // namespace

TEST(CharconvTest, RoundTripCuratedDoubles)
{
    const double values[] = {
        0.0,
        -0.0,
        1.0,
        -1.0,
        0.5,
        1.5,
        0.1,
        0.2,
        0.3,
        1.0 / 3.0,
        2.0 / 3.0,
        3.141592653589793,
        2.718281828459045,
        1e-5,
        1e-4,
        1e15,
        1e16,
        1e17,
        1e21,
        1e22,
        1e23,
        123456789012345678.0,
        9007199254740992.0,
        9007199254740993.0,
        4.9e-324,
        1.7976931348623157e308,
        2.2250738585072014e-308,
        -12345.6789,
    };

    for (const double v : values)
    {
        expect_round_trip(v);
    }
}

TEST(CharconvTest, RoundTripRandomBitPatterns)
{
    std::mt19937_64 rng(0x9E3779B97F4A7C15ull);
    int checked = 0;

    for (int i = 0; i < 200000; ++i)
    {
        const uint64_t bits = rng();
        const double v = std::bit_cast<double>(bits);
        if (std::isnan(v) || std::isinf(v))
        {
            continue;
        }
        const std::string text = to_text(v);
        const double back = parse<double>(text);
        ASSERT_EQ(std::bit_cast<uint64_t>(back), bits) << "printed as \"" << text << "\"";
        ++checked;
    }

    ASSERT_GT(checked, 190000);
}

TEST(CharconvTest, RoundTripRandomFloats)
{
    std::mt19937 rng(0xDEADBEEFu);
    int checked = 0;

    for (int i = 0; i < 200000; ++i)
    {
        const uint32_t bits = static_cast<uint32_t>(rng());
        const float v = std::bit_cast<float>(bits);
        if (std::isnan(v) || std::isinf(v))
        {
            continue;
        }
        const std::string text = to_text(v);
        const float back = parse<float>(text);
        ASSERT_EQ(std::bit_cast<uint32_t>(back), bits) << "printed as \"" << text << "\"";
        ++checked;
    }

    ASSERT_GT(checked, 190000);
}

TEST(CharconvTest, RoundTripSubnormals)
{
    for (int i = 0; i < 2000; ++i)
    {
        expect_round_trip(std::bit_cast<double>(static_cast<uint64_t>(i)));
        expect_round_trip(std::bit_cast<float>(static_cast<uint32_t>(i)));
    }

    expect_round_trip(std::numeric_limits<double>::denorm_min());
    expect_round_trip(std::numeric_limits<float>::denorm_min());
    expect_round_trip(std::nextafter(std::numeric_limits<double>::min(), 0.0));
}

TEST(CharconvTest, RoundTripPowersOfTwo)
{
    for (int e = -1074; e <= 1023; ++e)
    {
        const double v = std::ldexp(1.0, e);
        if (v == 0.0 || std::isinf(v))
        {
            continue;
        }
        expect_round_trip(v);
        expect_round_trip(-v);
    }
}

TEST(CharconvTest, RoundTripPowersOfTen)
{
    for (int e = -307; e <= 308; ++e)
    {
        const double v = std::pow(10.0, e);
        if (v == 0.0 || std::isinf(v))
        {
            continue;
        }
        expect_round_trip(v);
    }
}

TEST(CharconvTest, RoundTripNeighboursOfPowersOfTen)
{
    for (int e = -300; e <= 300; ++e)
    {
        const double v = std::pow(10.0, e);
        if (v == 0.0 || std::isinf(v))
        {
            continue;
        }
        expect_round_trip(std::nextafter(v, 0.0));
        expect_round_trip(std::nextafter(v, std::numeric_limits<double>::infinity()));
    }
}

TEST(CharconvTest, FormatKeepsIntegralValuesDistinguishable)
{
    ASSERT_EQ(to_text(0.0), "0.0");
    ASSERT_EQ(to_text(-0.0), "-0.0");
    ASSERT_EQ(to_text(1.0), "1.0");
    ASSERT_EQ(to_text(-1.0), "-1.0");
    ASSERT_EQ(to_text(100.0), "100.0");
    ASSERT_EQ(to_text(1.0f), "1.0");
}

TEST(CharconvTest, FormatIsShortest)
{
    ASSERT_EQ(to_text(1.5), "1.5");
    ASSERT_EQ(to_text(0.1), "0.1");
    ASSERT_EQ(to_text(0.1f), "0.1");
    ASSERT_EQ(to_text(1e-4), "0.0001");
    ASSERT_EQ(to_text(1e-5), "1e-05");
    ASSERT_EQ(to_text(1e21), "1e+21");
    ASSERT_EQ(to_text(1e22), "1e+22");
    ASSERT_EQ(to_text(123456789012345678.0), "1.2345678901234568e+17");
    ASSERT_EQ(to_text(std::numeric_limits<double>::denorm_min()), "5e-324");
    ASSERT_EQ(to_text(std::numeric_limits<double>::max()), "1.7976931348623157e+308");
    ASSERT_EQ(to_text(std::numeric_limits<double>::min()), "2.2250738585072014e-308");
    ASSERT_EQ(to_text(std::numeric_limits<float>::denorm_min()), "1e-45");
}

TEST(CharconvTest, FormatSpecialValues)
{
    ASSERT_EQ(to_text(std::numeric_limits<double>::infinity()), "inf");
    ASSERT_EQ(to_text(-std::numeric_limits<double>::infinity()), "-inf");
    ASSERT_EQ(to_text(std::numeric_limits<double>::quiet_NaN()), "nan");
}

TEST(CharconvTest, ParseSpecialValues)
{
    ASSERT_TRUE(std::isnan(parse<double>("nan")));
    ASSERT_TRUE(std::isinf(parse<double>("inf")));
    ASSERT_GT(parse<double>("inf"), 0.0);
    ASSERT_LT(parse<double>("-inf"), 0.0);
    ASSERT_TRUE(std::isinf(parse<double>("Infinity")));
}

TEST(CharconvTest, ParseRejectsLeadingWhitespaceAndPlus)
{
    parse<double>("  1.5", std::errc::invalid_argument);
    parse<double>("+1.5", std::errc::invalid_argument);
    parse<double>("abc", std::errc::invalid_argument);
    parse<double>("", std::errc::invalid_argument);
    parse<double>(".", std::errc::invalid_argument);
    parse<double>("e5", std::errc::invalid_argument);
}

TEST(CharconvTest, ParseReportsOutOfRange)
{
    const double big = parse<double>("1e400", std::errc::result_out_of_range);
    ASSERT_TRUE(std::isinf(big));

    const double small = parse<double>("1e-400", std::errc::result_out_of_range);
    ASSERT_EQ(small, 0.0);
}

TEST(CharconvTest, ParseStopsAtFirstUnconsumedCharacter)
{
    const std::string_view text = "1.5abc";
    double out = 0.0;
    const auto r = behl::from_chars(text.data(), text.data() + text.size(), out);

    ASSERT_EQ(r.ec, std::errc{});
    ASSERT_EQ(r.ptr - text.data(), 3);
    ASSERT_EQ(out, 1.5);
}

TEST(CharconvTest, ParseDoesNotAcceptHexFloat)
{
    const std::string_view text = "0x1p3";
    double out = 0.0;
    const auto r = behl::from_chars(text.data(), text.data() + text.size(), out);

    ASSERT_EQ(r.ec, std::errc{});
    ASSERT_EQ(r.ptr - text.data(), 1);
    ASSERT_EQ(out, 0.0);
}

TEST(CharconvTest, ParseLongDigitStringsTakeTheSlowPath)
{
    ASSERT_EQ(parse<double>("2.2250738585072011e-308"), 2.2250738585072011e-308);
    ASSERT_EQ(parse<double>("2.2250738585072013e-308"), 2.2250738585072013e-308);
    ASSERT_EQ(parse<double>("0.500000000000000166533453693773481063544750213623046875"), 0.5000000000000002);
    ASSERT_EQ(parse<double>("3.518437208883201171875e13"), 35184372088832.015625);
    ASSERT_EQ(parse<double>("1.00000000000000011102230246251565404236316680908203125"), 1.0);
    ASSERT_EQ(parse<double>("1.000000000000000111022302462515654042363166809082031251"), 1.0000000000000002);

    std::string many_digits = "1.";
    many_digits.append(760, '0');
    many_digits += "1";
    ASSERT_EQ(parse<double>(many_digits), 1.0);

    std::string long_int(400, '9');
    parse<double>(long_int, std::errc::result_out_of_range);
}

TEST(CharconvTest, ParseHalfwayCasesRoundToEven)
{
    ASSERT_EQ(parse<double>("9007199254740992"), 9007199254740992.0);
    ASSERT_EQ(parse<double>("9007199254740993"), 9007199254740992.0);
    ASSERT_EQ(parse<double>("9007199254740994"), 9007199254740994.0);
    ASSERT_EQ(parse<double>("9007199254740995"), 9007199254740996.0);
}

TEST(CharconvTest, ParseAgreesWithRoundTripOnRandomDecimalStrings)
{
    std::mt19937_64 rng(0x5DEECE66Dull);
    std::uniform_int_distribution<int> digit(0, 9);
    std::uniform_int_distribution<int> exponent(-320, 300);
    std::uniform_int_distribution<int> length(1, 19);

    for (int i = 0; i < 20000; ++i)
    {
        std::string text;
        const int n = length(rng);
        for (int d = 0; d < n; ++d)
        {
            text += static_cast<char>('0' + digit(rng));
        }
        text += "e";
        text += std::to_string(exponent(rng));

        double out = 0.0;
        const auto r = behl::from_chars(text.data(), text.data() + text.size(), out);
        if (r.ec != std::errc{})
        {
            continue;
        }
        if (std::isinf(out) || out == 0.0)
        {
            continue;
        }
        expect_round_trip(out);
    }
}

TEST(CharconvTest, ToCharsReportsBufferTooSmall)
{
    std::array<char, 4> buf{};
    const auto r = behl::to_chars(buf.data(), buf.data() + buf.size(), 1.7976931348623157e308);
    ASSERT_EQ(r.ec, std::errc::value_too_large);
}

TEST(CharconvTest, ToCharsExactlyFitsItsOutput)
{
    const double values[] = { 0.0, 1.5, 0.1, 1e21, 1.7976931348623157e308, 5e-324 };

    for (const double v : values)
    {
        const std::string text = to_text(v);
        std::vector<char> exact(text.size());
        const auto ok = behl::to_chars(exact.data(), exact.data() + exact.size(), v);
        ASSERT_EQ(ok.ec, std::errc{}) << "exact-size buffer rejected for " << text;
        ASSERT_EQ(std::string(exact.data(), ok.ptr), text);

        if (text.size() > 1)
        {
            std::vector<char> tight(text.size() - 1);
            const auto too_small = behl::to_chars(tight.data(), tight.data() + tight.size(), v);
            ASSERT_EQ(too_small.ec, std::errc::value_too_large) << "one byte short accepted for " << text;
        }
    }
}

TEST(CharconvTest, IntegerOverloadsStillWork)
{
    ASSERT_EQ(to_text(int64_t{ -9223372036854775807LL - 1 }), "-9223372036854775808");
    ASSERT_EQ(to_text(uint64_t{ 18446744073709551615ull }), "18446744073709551615");
    ASSERT_EQ(parse<int64_t>("-42"), -42);
    ASSERT_EQ(parse<uint32_t>("4294967295"), 4294967295u);
}

namespace
{
    template<typename T>
    void expect_to_chars_matches_std(T value, int base)
    {
        std::array<char, 72> mine{};
        std::array<char, 72> theirs{};
        const auto a = behl::to_chars(mine.data(), mine.data() + mine.size(), value, base);
        const auto b = std::to_chars(theirs.data(), theirs.data() + theirs.size(), value, base);
        ASSERT_EQ(a.ec, b.ec);
        ASSERT_EQ(std::string_view(mine.data(), static_cast<size_t>(a.ptr - mine.data())),
            std::string_view(theirs.data(), static_cast<size_t>(b.ptr - theirs.data())))
            << "value " << +value << " base " << base;
    }

    template<typename T>
    void expect_from_chars_matches_std(std::string_view text, int base)
    {
        T mine = static_cast<T>(42);
        T theirs = static_cast<T>(42);
        const auto a = behl::from_chars(text.data(), text.data() + text.size(), mine, base);
        const auto b = std::from_chars(text.data(), text.data() + text.size(), theirs, base);
        ASSERT_EQ(a.ec, b.ec) << "input \"" << text << "\" base " << base;
        ASSERT_EQ(a.ptr - text.data(), b.ptr - text.data()) << "input \"" << text << "\" base " << base;
        ASSERT_EQ(mine, theirs) << "input \"" << text << "\" base " << base;
    }
} // namespace

TEST(CharconvTest, IntegerToCharsMatchesStdAcrossTypesAndBases)
{
    std::mt19937_64 rng(0xC0FFEEull);
    for (int base = 2; base <= 36; ++base)
    {
        for (int i = 0; i < 400; ++i)
        {
            const uint64_t bits = rng();
            expect_to_chars_matches_std(static_cast<int64_t>(bits), base);
            expect_to_chars_matches_std(bits, base);
            expect_to_chars_matches_std(static_cast<int32_t>(bits), base);
            expect_to_chars_matches_std(static_cast<uint16_t>(bits), base);
            expect_to_chars_matches_std(static_cast<int8_t>(bits), base);
        }
        expect_to_chars_matches_std(std::numeric_limits<int64_t>::min(), base);
        expect_to_chars_matches_std(std::numeric_limits<int64_t>::max(), base);
        expect_to_chars_matches_std(std::numeric_limits<uint64_t>::max(), base);
        expect_to_chars_matches_std(std::numeric_limits<int8_t>::min(), base);
        expect_to_chars_matches_std(0, base);
    }
}

TEST(CharconvTest, IntegerToCharsReportsBufferTooSmall)
{
    std::array<char, 3> buf{};
    const auto r = behl::to_chars(buf.data(), buf.data() + buf.size(), -1234);
    ASSERT_EQ(r.ec, std::errc::value_too_large);
    const auto fits = behl::to_chars(buf.data(), buf.data() + buf.size(), -12);
    ASSERT_EQ(fits.ec, std::errc{});
    ASSERT_EQ(std::string_view(buf.data(), static_cast<size_t>(fits.ptr - buf.data())), "-12");
}

TEST(CharconvTest, IntegerFromCharsMatchesStdOnEdgeCases)
{
    const std::string_view inputs[] = {
        "",
        "-",
        "+1",
        " 1",
        "0",
        "-0",
        "007",
        "123abc",
        "9223372036854775807",
        "9223372036854775808",
        "-9223372036854775808",
        "-9223372036854775809",
        "18446744073709551615",
        "18446744073709551616",
        "99999999999999999999999999",
        "-99999999999999999999999999",
        "127",
        "128",
        "-128",
        "-129",
        "255",
        "256",
        "ff",
        "FF",
        "7fffffffffffffff",
        "8000000000000000",
        "zz",
        "ZZ",
        "12.5",
        "1e5",
        "--1",
        "0x10",
    };

    for (const std::string_view text : inputs)
    {
        for (const int base : { 2, 8, 10, 16, 36 })
        {
            expect_from_chars_matches_std<int64_t>(text, base);
            expect_from_chars_matches_std<uint64_t>(text, base);
            expect_from_chars_matches_std<int32_t>(text, base);
            expect_from_chars_matches_std<uint8_t>(text, base);
            expect_from_chars_matches_std<int8_t>(text, base);
        }
    }
}

TEST(CharconvTest, IntegerFromCharsMatchesStdOnRandomDigitStrings)
{
    std::mt19937_64 rng(0xBADC0DEull);
    const std::string_view alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-+. ";
    std::uniform_int_distribution<size_t> pick(0, alphabet.size() - 1);
    std::uniform_int_distribution<int> length(0, 24);
    std::uniform_int_distribution<int> base_dist(2, 36);

    for (int i = 0; i < 20000; ++i)
    {
        std::string text;
        const int n = length(rng);
        for (int c = 0; c < n; ++c)
        {
            text += alphabet[pick(rng)];
        }
        const int base = base_dist(rng);
        expect_from_chars_matches_std<int64_t>(text, base);
        expect_from_chars_matches_std<uint64_t>(text, base);
        expect_from_chars_matches_std<int16_t>(text, base);
    }
}

namespace
{
    constexpr int64_t parse_int(std::string_view text, int base = 10)
    {
        int64_t out = 0;
        behl::from_chars(text.data(), text.data() + text.size(), out, base);
        return out;
    }

    static_assert(parse_int("-9223372036854775808") == std::numeric_limits<int64_t>::min());
    static_assert(parse_int("7fffffffffffffff", 16) == std::numeric_limits<int64_t>::max());
    static_assert(parse_int("-42") == -42);
} // namespace
