#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>

// Guards the invariant that a string's hash depends only on its contents, never
// on which object holds them or how it was built. Two GCStrings with equal
// contents must be the same table key, and a string_view lookup through the C
// API must agree with a key stored from script.
class StringKeyTest : public ::testing::Test
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

    void run_expect_integer(const std::string& code, int64_t expected)
    {
        ASSERT_NO_THROW(behl::load_string(S, code));
        ASSERT_NO_THROW(behl::call(S, 0, 1));
        EXPECT_EQ(behl::to_integer(S, -1), expected) << code;
        behl::set_top(S, 0);
    }

    void run_expect_string(const std::string& code, const std::string& expected)
    {
        ASSERT_NO_THROW(behl::load_string(S, code));
        ASSERT_NO_THROW(behl::call(S, 0, 1));
        EXPECT_EQ(behl::to_string(S, -1), expected) << code;
        behl::set_top(S, 0);
    }
};

TEST_F(StringKeyTest, ConcatenatedKeyMatchesLiteral)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        let n = 5
        let k = "key_" + tostring(n)
        t[k] = 42
        return t["key_5"]
    )",
        42);
}

TEST_F(StringKeyTest, LiteralKeyReadByConcatenated)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t["key_5"] = 7
        let n = 5
        let k = "key_" + tostring(n)
        return t[k]
    )",
        7);
}

TEST_F(StringKeyTest, TostringBuiltKey)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t["n_12"] = 99
        let n = 12
        let k = "n_" + tostring(n)
        return t[k]
    )",
        99);
}

TEST_F(StringKeyTest, OverwriteThroughDifferentObjectKeepsOneEntry)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t["dup"] = 1
        let d = "d"
        let k = d + "up"
        t[k] = 2
        let count = 0
        for (let key, value in pairs(t)) { count = count + 1 }
        if (t["dup"] != 2) { return -1 }
        return count - 64
    )",
        1);
}

// The SSO buffer holds 30 characters plus a terminator, so keys either side of
// that boundary take different storage paths but must still hash identically.
TEST_F(StringKeyTest, SsoBoundaryKeysRoundTrip)
{
    for (int len = 28; len <= 34; ++len)
    {
        const std::string key(static_cast<size_t>(len), 'a');
        const std::string code = "let t = {}\n"
                                 "for (let f = 0; f < 64; f++) { t[\"filler_\" + tostring(f)] = f }\n"
                                 "let built = \"\"\n"
                                 "for (let i = 0; i < "
            + std::to_string(len)
            + "; i++) { built = built + \"a\" }\n"
              "t[built] = "
            + std::to_string(len)
            + "\n"
              "return t[\""
            + key + "\"]\n";
        run_expect_integer(code, len);
    }
}

TEST_F(StringKeyTest, LongKeysSharedPrefixAreDistinct)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        let base = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        t[base + "1"] = 1
        t[base + "2"] = 2
        t[base + "3"] = 3
        if (t[base + "1"] != 1) { return -1 }
        if (t[base + "2"] != 2) { return -2 }
        if (t[base + "3"] != 3) { return -3 }
        return 0
    )",
        0);
}

TEST_F(StringKeyTest, LongKeysSharedSuffixAreDistinct)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        let tail = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        t["1" + tail] = 1
        t["2" + tail] = 2
        if (t["1" + tail] != 1) { return -1 }
        if (t["2" + tail] != 2) { return -2 }
        return 0
    )",
        0);
}

TEST_F(StringKeyTest, EmptyStringKey)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t[""] = 5
        let e = "a"
        e = e + ""
        e = ""
        return t[e]
    )",
        5);
}

TEST_F(StringKeyTest, ManyGeneratedKeysRoundTrip)
{
    run_expect_integer(R"(
        let t = {}
        for (let i = 0; i < 500; i++) { t["key_" + tostring(i)] = i * 3 }
        let sum = 0
        for (let i = 0; i < 500; i++) { sum = sum + t["key_" + tostring(i)] }
        return sum
    )",
        374250);
}

TEST_F(StringKeyTest, KeysSurviveCollection)
{
    run_expect_integer(R"(
        const gc = import("gc")
        let t = {}
        for (let i = 0; i < 200; i++) { t["k" + tostring(i)] = i }
        gc.collect()
        let sum = 0
        for (let i = 0; i < 200; i++) { sum = sum + t["k" + tostring(i)] }
        return sum
    )",
        19900);
}

TEST_F(StringKeyTest, KeyLookupAfterManyTemporaries)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t["stable"] = 11
        for (let i = 0; i < 300; i++) { let junk = "tmp_" + tostring(i) }
        let part = "stab"
        return t[part + "le"]
    )",
        11);
}

// The C API looks up by string_view while script stores a GCString key, so both
// hash paths have to agree.
TEST_F(StringKeyTest, ApiStringViewLookupFindsScriptKey)
{
    constexpr std::string_view code = R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t["alpha"] = 1
        t["beta_" + tostring(2)] = 2
        return t
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::is_table(S, -1));

    behl::table_rawgetfield(S, -1, "alpha");
    EXPECT_EQ(behl::to_integer(S, -1), 1);
    behl::set_top(S, 1);

    behl::table_rawgetfield(S, -1, "beta_2");
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_F(StringKeyTest, ScriptFindsKeySetThroughApi)
{
    behl::table_new(S);
    behl::push_integer(S, 77);
    behl::table_rawsetfield(S, -2, "from_api");
    behl::set_global(S, "shared");

    run_expect_integer(R"(
        let k = "from" + "_api"
        return shared[k]
    )",
        77);
}

TEST_F(StringKeyTest, GlobalByNameMatchesScriptDefinition)
{
    constexpr std::string_view code = R"(
        globalvalue = 123
        return 0
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    behl::set_top(S, 0);

    behl::get_global(S, "globalvalue");
    EXPECT_EQ(behl::to_integer(S, -1), 123);
}

TEST_F(StringKeyTest, KeysDifferingOnlyByCaseAreDistinct)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t["Key"] = 1
        t["key"] = 2
        t["KEY"] = 3
        if (t["Key"] != 1) { return -1 }
        if (t["key"] != 2) { return -2 }
        if (t["KEY"] != 3) { return -3 }
        return 0
    )",
        0);
}

TEST_F(StringKeyTest, NumericAndStringKeysDoNotCollide)
{
    run_expect_integer(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        t[1] = 10
        t["1"] = 20
        if (t[1] != 10) { return -1 }
        if (t["1"] != 20) { return -2 }
        return 0
    )",
        0);
}

TEST_F(StringKeyTest, StringValueRoundTripsThroughTable)
{
    run_expect_string(R"(
        let t = {}
        for (let f = 0; f < 64; f++) { t["filler_" + tostring(f)] = f }
        let a = "a"
        t[a + "b"] = "val" + "ue"
        return t["ab"]
    )",
        "value");
}
