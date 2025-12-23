#include <behl/behl.hpp>
#include <gtest/gtest.h>

class TableConstructionTest : public ::testing::Test
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

TEST_F(TableConstructionTest, ArrayAccess)
{
    constexpr std::string_view code = R"(
        let t = {10, 20, 30, 40, 50}
        if (t[0] != 10) { return false }
        if (t[2] != 30) { return false }
        if (t[4] != 50) { return false }
        t[1] = 99
        return t[1] == 99
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(TableConstructionTest, HashAccess)
{
    constexpr std::string_view code = R"(
        let t = {x = 100, y = 200}
        if (t["x"] != 100) { return false }
        if (t["y"] != 200) { return false }
        t["z"] = 300
        return t["z"] == 300
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(TableConstructionTest, MixedArrayHash)
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
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_F(TableConstructionTest, EmptyTableCreation)
{
    constexpr std::string_view code = R"(
        let t = {}
        t[0] = 42
        return t[0]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}
