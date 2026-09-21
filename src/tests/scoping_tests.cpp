#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class ScopingTest : public ::testing::TestWithParam<bool>
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

TEST_P(ScopingTest, LocalShadowingInConditionals)
{
    constexpr std::string_view code = R"(
        let i = 10
        if (true) {
            let i = 100
            if (i != 100) {
                return false
            }
        }
        if (true) {
            let i = 1000
            if (i != 1000) {
                return false
            }
        }
        return i == 10
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(ScopingTest, LocalShadowingInBranches)
{
    constexpr std::string_view code = R"(
        let i = 10
        if (i != 10) {
            let i = 20
            return i
        } else {
            let i = 30
            if (i != 30) {
                return false
            }
        }
        return i == 10
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(ScopingTest, LocalInNestedBlocks)
{
    constexpr std::string_view code = R"(
        function f(a) {
            let x = 3
            let b = a
            let c = a
            let d = b
            if (d == b) {
                let x = 99
                if (x != 99) {
                    return false
                }
            }
            return x == 3
        }
        return f(2)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_TRUE(behl::to_boolean(S, -1));
}

INSTANTIATE_TEST_SUITE_P(Mode, ScopingTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
