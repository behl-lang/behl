#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>
#include <string>

// The JIT emits raw machine code with no unwind data, so a C++ exception must
// never unwind through a compiled frame. Every helper called from compiled code
// catches and converts to a result code, and the driver rethrows from C++ frames
// that do have unwind data. These tests exercise that on both propagation paths:
// shallow calls nest a C frame per behl call, while past kJitNestLimit the call
// helper hands the frame to the driver and the C stack stays flat. An error
// crossing that boundary takes a different route out.
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
            let ok, msg = pcall(function() { return deep()" + std::to_string(depth) + R"() })
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

// Past kJitNestLimit the call helper stops nesting C frames, so the error takes
// the driver path back out instead of unwinding a chain of helper frames.
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

// A failed deep call must leave the call stack exactly as it found it, or a
// later deep call drifts or overflows.
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

// The shape the JIT self-call path optimises: two recursive calls per frame.
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

// The function is compiled by the first successful call, so the error path here
// runs through already-compiled code rather than the interpreter.
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

// No pcall: the exception has to reach the C++ API boundary intact.
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

// A metamethod runs arbitrary script from inside a helper called by compiled
// code, so an error there unwinds through one more layer than a plain throw.
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

// Defers in enclosing frames run while an error unwinds a deep chain. The defer
// in the frame that actually throws does NOT run, in the interpreter as well as
// under the JIT, so 200 frames report a defer rather than 201. That asymmetry
// looks unintended next to docs/language/defer.md:143 ("automatically closed
// even if error occurs") but it predates the JIT work, so this pins current
// behaviour and will fail loudly if it is ever changed.
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

// ---------------------------------------------------------------------------
// C functions called from deep inside a compiled chain. These are the riskiest
// unwind paths: the throw originates in native code that compiled frames called
// into, and it has to get back out without unwinding through any of them.
// ---------------------------------------------------------------------------

struct CustomNativeException : public std::exception
{
    const char* what() const noexcept override { return "custom native exception"; }
};

static int native_raises_behl_error(behl::State* S)
{
    // behl::error is [[noreturn]], so there is nothing after it.
    behl::error(S, "native said no");
}

static int native_checks_argument(behl::State* S)
{
    // Throws a type error when the script passes the wrong type.
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
                if (n <= 0) { return )") + callee + R"( }
                return deep(n - 1)
            }
            let ok, msg = pcall(function() { return deep()" + std::to_string(depth) + R"() })
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

// A custom C++ exception is not a BehlException, so it exercises the catch(...)
// path and the exception_ptr round trip rather than the typed one.
TEST_F(JitNativeUnwindTest, CustomExceptionFromNativeReachesApiAtDepth)
{
    for (int depth : { 10, 140, 200, 1000 })
    {
        const std::string code = std::string(R"(
            function deep(n) {
                if (n <= 0) { return native_custom() }
                return deep(n - 1)
            }
            return deep()") + std::to_string(depth) + R"()
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
            return deep()") + std::to_string(depth) + R"()
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

// The native throw happens inside a metamethod, which is itself invoked from a
// helper called by compiled code, so this is the deepest layering of the three.
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
