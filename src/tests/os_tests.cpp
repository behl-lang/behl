#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include <string_view>

class OsTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        behl::close(S);
        S = nullptr;
    }
};

TEST_P(OsTest, ClockIsMonotonicNonDecreasing)
{
    constexpr std::string_view code = R"(
        const os = import("os");
        let prev = os.clock();
        let ok = typeof(prev) == "number";
        for (let i = 0; i < 10000; i++) {
            let now = os.clock();
            if (now < prev) { ok = false; }
            prev = now;
        }
        return ok;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OsTest, HrtimeIsMonotonicNonDecreasing)
{
    constexpr std::string_view code = R"(
        const os = import("os");
        let prev = os.hrtime();
        let ok = typeof(prev) == "number" && prev >= 0;
        for (let i = 0; i < 10000; i++) {
            let now = os.hrtime();
            if (now < prev) { ok = false; }
            prev = now;
        }
        return ok;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OsTest, DummyReturnsFloatOne)
{
    constexpr std::string_view code = R"(
        const os = import("os");
        return os.dummy();
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::type(S, -1), behl::Type::kNumber);
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 1.0);
}

INSTANTIATE_TEST_SUITE_P(Mode, OsTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
