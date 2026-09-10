#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>
#include <string>

class JitUnwindTest : public ::testing::Test
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

    void expect_caught_at_depth(int depth)
    {
        const std::string code = R"(
            function deep(n) {
                if (n <= 0) { error("boom") }
                return deep(n - 1)
            }
            let ok, msg = pcall(function() { return deep()"
            + std::to_string(depth) + R"() })
            if (ok) { return -1 }
            return 1
        )";
        ASSERT_NO_THROW(behl::load_string(S, code)) << "depth " << depth;
        ASSERT_NO_THROW(behl::call(S, 0, 1)) << "depth " << depth;
        EXPECT_EQ(behl::to_integer(S, -1), 1) << "depth " << depth;
        behl::set_top(S, 0);
    }
};

TEST_F(JitUnwindTest, ErrorCaughtBelowNestLimit)
{
    expect_caught_at_depth(10);
    expect_caught_at_depth(50);
    expect_caught_at_depth(140);
}

TEST_F(JitUnwindTest, ErrorCaughtAboveNestLimit)
{
    expect_caught_at_depth(200);
    expect_caught_at_depth(500);
    expect_caught_at_depth(2000);
}

TEST_F(JitUnwindTest, ErrorCaughtAcrossTheNestBoundary)
{
    for (int d = 145; d <= 155; ++d)
    {
        expect_caught_at_depth(d);
    }
}

TEST_F(JitUnwindTest, StateStillUsableAfterDeepError)
{
    constexpr std::string_view code = R"(
        function deep(n) {
            if (n <= 0) { error("boom") }
            return deep(n - 1)
        }
        function sum(n) {
            if (n <= 0) { return 0 }
            return n + sum(n - 1)
        }
        let ok, msg = pcall(function() { return deep(400) })
        if (ok) { return -1 }
        return sum(300)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 45150);
}

TEST_F(JitUnwindTest, RepeatedDeepErrorsDoNotLeakFrames)
{
    constexpr std::string_view code = R"(
        function deep(n) {
            if (n <= 0) { error("boom") }
            return deep(n - 1)
        }
        let caught = 0
        for (let i = 0; i < 50; i++) {
            let ok, msg = pcall(function() { return deep(300) })
            if (!ok) { caught = caught + 1 }
        }
        function sum(n) {
            if (n <= 0) { return 0 }
            return n + sum(n - 1)
        }
        if (sum(400) != 80200) { return -1 }
        return caught
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 50);
}

TEST_F(JitUnwindTest, ErrorFromSelfRecursiveTree)
{
    constexpr std::string_view code = R"(
        function fib(n) {
            if (n == 7) { error("boom") }
            if (n < 2) { return n }
            return fib(n - 1) + fib(n - 2)
        }
        let ok, msg = pcall(function() { return fib(20) })
        if (ok) { return -1 }
        return 1
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}

TEST_F(JitUnwindTest, ErrorAfterFunctionIsCompiled)
{
    constexpr std::string_view code = R"(
        function deep(n, fail_at) {
            if (n <= fail_at) { error("boom") }
            return deep(n - 1, fail_at)
        }
        function safe(n) {
            if (n <= 0) { return 0 }
            return safe(n - 1)
        }
        safe(300)
        safe(300)
        let ok, msg = pcall(function() { return deep(300, 0) })
        if (ok) { return -1 }
        return 1
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}

TEST_F(JitUnwindTest, InnerPcallDeepInChainOuterContinues)
{
    constexpr std::string_view code = R"(
        function boom(n) {
            if (n <= 0) { error("inner") }
            return boom(n - 1)
        }
        function middle(n) {
            if (n <= 0) {
                let ok, msg = pcall(function() { return boom(200) })
                if (ok) { return -1 }
                return 7
            }
            return middle(n - 1)
        }
        return middle(200)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 7);
}

TEST_F(JitUnwindTest, UncaughtDeepErrorReachesTheApi)
{
    constexpr std::string_view code = R"(
        function deep(n) {
            if (n <= 0) { error("boom") }
            return deep(n - 1)
        }
        return deep(400)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::BehlException);
}

TEST_F(JitUnwindTest, UncaughtDeepTypeErrorReachesTheApi)
{
    constexpr std::string_view code = R"(
        function deep(n) {
            if (n <= 0) { return {} + 1 }
            return deep(n - 1)
        }
        return deep(400)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::BehlException);
}

TEST_F(JitUnwindTest, ErrorFromMetamethodDeepInChain)
{
    constexpr std::string_view code = R"(
        let mt = {}
        mt.__add = function(a, b) { error("from metamethod") }
        let t = {}
        setmetatable(t, mt)

        function deep(n) {
            if (n <= 0) { return t + 1 }
            return deep(n - 1)
        }
        let ok, msg = pcall(function() { return deep(300) })
        if (ok) { return -1 }
        return 1
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}

TEST_F(JitUnwindTest, DeepRecursionSucceedsAcrossNestLimit)
{
    constexpr std::string_view code = R"(
        function sum(n) {
            if (n <= 0) { return 0 }
            return n + sum(n - 1)
        }
        return sum(5000)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 12502500);
}

TEST_F(JitUnwindTest, DeferRunsWhileUnwindingDeepChain)
{
    constexpr std::string_view code = R"(
        let count = 0
        function deep(n) {
            defer { count = count + 1 }
            if (n <= 0) { error("boom") }
            return deep(n - 1)
        }
        let ok, msg = pcall(function() { return deep(200) })
        if (ok) { return -1 }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 201);
}

struct CustomNativeException : public std::exception
{
    const char* what() const noexcept override
    {
        return "custom native exception";
    }
};

static int native_raises_behl_error(behl::State* S)
{
    behl::error(S, "native said no");
}

static int native_checks_argument(behl::State* S)
{
    (void)behl::check_integer(S, 0);
    return 0;
}

static int native_throws_custom(behl::State*)
{
    throw CustomNativeException{};
}

static int native_throws_std(behl::State*)
{
    throw std::runtime_error("std exception from native");
}

class JitNativeUnwindTest : public ::testing::Test
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S);
        ASSERT_NE(S, nullptr);

        behl::push_cfunction(S, native_raises_behl_error);
        behl::set_global(S, "native_error");
        behl::push_cfunction(S, native_checks_argument);
        behl::set_global(S, "native_check");
        behl::push_cfunction(S, native_throws_custom);
        behl::set_global(S, "native_custom");
        behl::push_cfunction(S, native_throws_std);
        behl::set_global(S, "native_std");

        behl::set_top(S, 0);
    }

    void TearDown() override
    {
        behl::close(S);
    }

    std::string deep_calling(const char* callee, int depth)
    {
        return std::string(R"(
            function deep(n) {
                if (n <= 0) { return )")
            + callee + R"( }
                return deep(n - 1)
            }
            let ok, msg = pcall(function() { return deep()"
            + std::to_string(depth) + R"() })
            if (ok) { return -1 }
            return 1
        )";
    }
};

TEST_F(JitNativeUnwindTest, BehlErrorFromNativeCaughtAtDepth)
{
    for (int depth : { 10, 140, 200, 1000 })
    {
        const std::string code = deep_calling("native_error()", depth);
        ASSERT_NO_THROW(behl::load_string(S, code)) << depth;
        ASSERT_NO_THROW(behl::call(S, 0, 1)) << depth;
        EXPECT_EQ(behl::to_integer(S, -1), 1) << "depth " << depth;
        behl::set_top(S, 0);
    }
}

TEST_F(JitNativeUnwindTest, BadArgumentTypeFromNativeCaughtAtDepth)
{
    for (int depth : { 10, 140, 200, 1000 })
    {
        const std::string code = deep_calling("native_check(\"not a number\")", depth);
        ASSERT_NO_THROW(behl::load_string(S, code)) << depth;
        ASSERT_NO_THROW(behl::call(S, 0, 1)) << depth;
        EXPECT_EQ(behl::to_integer(S, -1), 1) << "depth " << depth;
        behl::set_top(S, 0);
    }
}

TEST_F(JitNativeUnwindTest, StdlibNativeTypeErrorCaughtAtDepth)
{
    for (int depth : { 10, 200 })
    {
        const std::string code = deep_calling("tonumber()", depth);
        ASSERT_NO_THROW(behl::load_string(S, code)) << depth;
        ASSERT_NO_THROW(behl::call(S, 0, 1)) << depth;
        behl::set_top(S, 0);
    }
}

TEST_F(JitNativeUnwindTest, CustomExceptionFromNativeReachesApiAtDepth)
{
    for (int depth : { 10, 140, 200, 1000 })
    {
        const std::string code = std::string(R"(
            function deep(n) {
                if (n <= 0) { return native_custom() }
                return deep(n - 1)
            }
            return deep()")
            + std::to_string(depth) + R"()
        )";
        ASSERT_NO_THROW(behl::load_string(S, code)) << depth;
        EXPECT_THROW({ behl::call(S, 0, 1); }, CustomNativeException) << "depth " << depth;
        behl::set_top(S, 0);
    }
}

TEST_F(JitNativeUnwindTest, StdExceptionFromNativeReachesApiAtDepth)
{
    for (int depth : { 10, 200, 1000 })
    {
        const std::string code = std::string(R"(
            function deep(n) {
                if (n <= 0) { return native_std() }
                return deep(n - 1)
            }
            return deep()")
            + std::to_string(depth) + R"()
        )";
        ASSERT_NO_THROW(behl::load_string(S, code)) << depth;
        EXPECT_THROW({ behl::call(S, 0, 1); }, std::runtime_error) << "depth " << depth;
        behl::set_top(S, 0);
    }
}

TEST_F(JitNativeUnwindTest, StateUsableAfterNativeThrowAtDepth)
{
    const std::string code = deep_calling("native_error()", 500);
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
    behl::set_top(S, 0);

    constexpr std::string_view after = R"(
        function sum(n) {
            if (n <= 0) { return 0 }
            return n + sum(n - 1)
        }
        return sum(400)
    )";
    ASSERT_NO_THROW(behl::load_string(S, after));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 80200);
}

TEST_F(JitNativeUnwindTest, NativeThrowFromMetamethodAtDepth)
{
    constexpr std::string_view code = R"(
        let mt = {}
        mt.__add = function(a, b) { return native_error() }
        let t = {}
        setmetatable(t, mt)

        function deep(n) {
            if (n <= 0) { return t + 1 }
            return deep(n - 1)
        }
        let ok, msg = pcall(function() { return deep(300) })
        if (ok) { return -1 }
        return 1
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
}
