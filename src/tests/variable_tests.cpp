#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>

class VariableTest : public ::testing::Test
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(VariableTest, ExecuteReturnInteger)
{
    constexpr std::string_view code = R"(
        return 42
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(VariableTest, ExecuteConstVariable)
{
    constexpr std::string_view code = R"(
        const x = 100
        return x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false)); // Disable optimizations to keep unused const
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
}

TEST_F(VariableTest, ConstIsImmutable)
{
    constexpr std::string_view code = R"(
        const x = 100
        x = 200
    )";

    EXPECT_THROW({ behl::load_string(S, code, false); }, behl::SemanticError); // Disable optimizations for error testing
}

TEST_F(VariableTest, GlobalAssignReflectsIn_G)
{
    constexpr std::string_view code = R"(
        x = 42
        return _G.x
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(VariableTest, SetVia_GReflectsAsGlobal)
{
    constexpr std::string_view code = R"(
        _G.y = 99
        return y
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 99);
}

TEST_F(VariableTest, GlobalTablePointsToItself)
{
    constexpr std::string_view code = R"(
        _G.__sentinel = 1234
        return _G._G.__sentinel
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1234);
}

TEST_F(VariableTest, MissingGlobalIsNil)
{
    constexpr std::string_view code = R"(
        return some_missing_global
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}
