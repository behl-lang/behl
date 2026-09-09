#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

class ConcatTest : public ::testing::Test
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
};

TEST_F(ConcatTest, SelfAddLocalString)
{
    constexpr std::string_view code = R"(
        let c = "x"
        let s = ""
        for (let i = 0; i < 4; i++) {
            s = s + c
        }
        return s
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "xxxx");
}

TEST_F(ConcatTest, SelfAddLocalStringOutsideLoop)
{
    constexpr std::string_view code = R"(
        let b = "bar"
        let a = "foo"
        a = a + b
        return a
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "foobar");
}

TEST_F(ConcatTest, SelfAddLocalStillAddsIntegers)
{
    constexpr std::string_view code = R"(
        let c = 3
        let s = 10
        for (let i = 0; i < 4; i++) {
            s = s + c
        }
        return s
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 22);
}

TEST_F(ConcatTest, SelfAddLocalStringPlusNumberThrows)
{
    constexpr std::string_view code = R"(
        let c = 1
        let s = "a"
        s = s + c
        return s
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::TypeError);
}

TEST_F(ConcatTest, SelfAddLocalRespectsAddMetamethod)
{
    constexpr std::string_view code = R"(
        let mt = {}
        mt.__add = function(a, b) { return 99 }
        let t = {}
        setmetatable(t, mt)
        let other = 1
        t = t + other
        return t
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 99);
}

TEST_F(ConcatTest, AddStringConstant)
{
    constexpr std::string_view code = R"(
        let a = "foo"
        return a + "bar"
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "foobar");
}

TEST_F(ConcatTest, AddStringConstantInLoop)
{
    constexpr std::string_view code = R"(
        let s = ""
        for (let i = 0; i < 3; i++) {
            s = s + "ab"
        }
        return s
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "ababab");
}

TEST_F(ConcatTest, AddStringConstantToNumberThrows)
{
    constexpr std::string_view code = R"(
        let a = 5
        return a + "bar"
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::TypeError);
}

// The split arithmetic opcodes (kOpAdd + kOpMMAdd and friends) must run their
// metamethod exactly once: the fast half falls through to the slow half, and
// the slow half must not run again after the fast half already succeeded.
class SplitArithTest : public ::testing::Test
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

    void check_called_once(std::string_view event, std::string_view expr)
    {
        const std::string code = std::string(R"(
            let calls = 0
            let mt = {}
            mt.)") + std::string(event) + R"( = function(a, b) {
                calls = calls + 1
                return 7
            }
            let t1 = {}
            let t2 = {}
            setmetatable(t1, mt)
            setmetatable(t2, mt)
            let r = )" + std::string(expr) + R"(
            return calls, r
        )";
        ASSERT_NO_THROW(behl::load_string(S, code)) << event;
        ASSERT_NO_THROW(behl::call(S, 0, 2)) << event;
        EXPECT_EQ(behl::to_integer(S, -2), 1) << event << " metamethod call count";
        EXPECT_EQ(behl::to_integer(S, -1), 7) << event << " result";
        behl::set_top(S, 0);
    }
};

TEST_F(SplitArithTest, MetamethodRunsExactlyOnce)
{
    check_called_once("__add", "t1 + t2");
    check_called_once("__sub", "t1 - t2");
    check_called_once("__mul", "t1 * t2");
    check_called_once("__div", "t1 / t2");
    check_called_once("__mod", "t1 % t2");
    check_called_once("__pow", "t1 ** t2");
    check_called_once("__band", "t1 & t2");
    check_called_once("__bor", "t1 | t2");
    check_called_once("__bxor", "t1 ^ t2");
    check_called_once("__shl", "t1 << t2");
    check_called_once("__shr", "t1 >> t2");
}

TEST_F(SplitArithTest, NumericFastPathSkipsSlowHalf)
{
    constexpr std::string_view code = R"(
        let a = 7
        let b = 3
        return a + b, a - b, a * b, a / b, a % b, a ** b, a & b, a | b, a ^ b, a << b, a >> b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 11));
    EXPECT_EQ(behl::to_integer(S, -11), 10);
    EXPECT_EQ(behl::to_integer(S, -10), 4);
    EXPECT_EQ(behl::to_integer(S, -9), 21);
    EXPECT_DOUBLE_EQ(behl::to_number(S, -8), 7.0 / 3.0);
    EXPECT_EQ(behl::to_integer(S, -7), 1);
    EXPECT_EQ(behl::to_integer(S, -6), 343);
    EXPECT_EQ(behl::to_integer(S, -5), 3);
    EXPECT_EQ(behl::to_integer(S, -4), 7);
    EXPECT_EQ(behl::to_integer(S, -3), 4);
    EXPECT_EQ(behl::to_integer(S, -2), 56);
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(SplitArithTest, FloatOperandsTakeFastPath)
{
    constexpr std::string_view code = R"(
        let a = 7.5
        let b = 2.5
        return a + b, a - b, a * b, a / b
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -4), 10.0);
    EXPECT_DOUBLE_EQ(behl::to_number(S, -3), 5.0);
    EXPECT_DOUBLE_EQ(behl::to_number(S, -2), 18.75);
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 3.0);
}
