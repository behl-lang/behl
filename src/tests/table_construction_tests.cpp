#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class TableConstructionTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_P(TableConstructionTest, ArrayAccess)
{
    constexpr std::string_view code = R"(
        let t = {10, 20, 30, 40, 50}
        if (t[0] != 10) { return false }
        if (t[2] != 30) { return false }
        if (t[4] != 50) { return false }
        t[1] = 99
        return t[1] == 99
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableConstructionTest, HashAccess)
{
    constexpr std::string_view code = R"(
        let t = {x = 100, y = 200}
        if (t["x"] != 100) { return false }
        if (t["y"] != 200) { return false }
        t["z"] = 300
        return t["z"] == 300
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableConstructionTest, MixedArrayHash)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3, x = 10, y = 20}
        if (t[0] != 1) { return false }
        if (t[1] != 2) { return false }
        if (t[2] != 3) { return false }
        if (t["x"] != 10) { return false }
        if (t["y"] != 20) { return false }
        return true
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(TableConstructionTest, EmptyTableCreation)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = 42
        return t[0]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(TableConstructionTest, ManyFixedItemsBeforeVarargKeepAllEntries)
{
    behl::load_stdlib(S);
    std::string code = "function f(...) { return {";
    for (int i = 0; i < 300; ++i)
    {
        code += std::to_string(i) + ",";
    }
    code += " ...} }\n"
            "let t = f(1000, 1001)\n"
            "let bad = 0\n"
            "for (let i = 0; i < 300; i++) { if (t[i] != i) { bad++ } }\n"
            "return bad, t[300], t[301], rawlen(t)\n";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 4));
    EXPECT_EQ(behl::to_integer(S, -4), 0);
    EXPECT_EQ(behl::to_integer(S, -3), 1000);
    EXPECT_EQ(behl::to_integer(S, -2), 1001);
    EXPECT_EQ(behl::to_integer(S, -1), 302);
}

INSTANTIATE_TEST_SUITE_P(Mode, TableConstructionTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
