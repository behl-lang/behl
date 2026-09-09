#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace behl;

class DeferTest : public ::testing::Test
{
protected:
    State* S;

    void SetUp() override
    {
        S = new_state();
        ASSERT_NE(S, nullptr);
        load_stdlib(S);
        set_top(S, 0);
    }

    void TearDown() override
    {
        close(S);
    }

    void run_script(std::string_view code)
    {
        load_string(S, code);
        call(S, 0, kMultRet);
    }

    std::string get_error()
    {
        if (get_top(S) > 0 && type(S, -1) == Type::kString)
        {
            return std::string(to_string(S, -1));
        }
        return "";
    }
};

TEST_F(DeferTest, SimpleDeferStatement)
{
    constexpr std::string_view code = R"(
        let result = {};
        function test() {
            result[0] = "start";
            defer result[1] = "deferred";
            result[2] = "end";
        }
        test();
        return result[0], result[1], result[2];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(std::string(to_string(S, 0)), "start");
    ASSERT_EQ(std::string(to_string(S, 1)), "deferred");
    ASSERT_EQ(std::string(to_string(S, 2)), "end");
}

TEST_F(DeferTest, DeferWithPrint)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;
        
        function myprint(s) {
            output[idx] = s;
            idx = idx + 1;
        }
        
        function test() {
            myprint("start");
            defer myprint("deferred");
            myprint("end");
        }
        test();
        return output[0], output[1], output[2];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(std::string(to_string(S, 0)), "start");
    ASSERT_EQ(std::string(to_string(S, 1)), "end");
    ASSERT_EQ(std::string(to_string(S, 2)), "deferred");
}

TEST_F(DeferTest, MultipleDeferLIFO)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;
        
        function add(s) {
            output[idx] = s;
            idx = idx + 1;
        }
        
        function test() {
            defer add("third");
            defer add("second");
            defer add("first");
            add("body");
        }
        test();
        return output[0], output[1], output[2], output[3];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 4);
    ASSERT_EQ(std::string(to_string(S, 0)), "body");
    ASSERT_EQ(std::string(to_string(S, 1)), "first");
    ASSERT_EQ(std::string(to_string(S, 2)), "second");
    ASSERT_EQ(std::string(to_string(S, 3)), "third");
}

TEST_F(DeferTest, DeferWithBlock)
{
    constexpr std::string_view code = R"(
        let output = {};
        
        function test() {
            let x = 42;
            defer {
                output[0] = x;
                output[1] = "block";
            };
            x = 100;
        }
        test();
        return output[0], output[1];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(to_integer(S, 0), 100); // Should see updated value
    ASSERT_EQ(std::string(to_string(S, 1)), "block");
}

TEST_F(DeferTest, DeferSeesVariableMutations)
{
    constexpr std::string_view code = R"(
        let captured;
        function test() {
            let x = 1;
            defer captured = x;
            x = 2;
        }
        test();
        return captured;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 2); // Defer captures by reference
}

TEST_F(DeferTest, NestedScopes)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;
        
        function add(s) {
            output[idx] = s;
            idx = idx + 1;
        }
        
        function test() {
            defer add("outer");
            add("outer-body");
            
            {
                defer add("inner");
                add("inner-body");
            }
            
            add("back-outer");
        }
        test();
        return output[0], output[1], output[2], output[3], output[4];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 5);
    ASSERT_EQ(std::string(to_string(S, 0)), "outer-body");
    ASSERT_EQ(std::string(to_string(S, 1)), "inner-body");
    ASSERT_EQ(std::string(to_string(S, 2)), "inner"); // Inner defer executes when inner scope ends
    ASSERT_EQ(std::string(to_string(S, 3)), "back-outer");
    ASSERT_EQ(std::string(to_string(S, 4)), "outer"); // Outer defer executes at function end
}

TEST_F(DeferTest, DeferWithEarlyReturn)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;
        
        function add(s) {
            output[idx] = s;
            idx = idx + 1;
        }
        
        function test(x) {
            defer add("cleanup");
            
            if (x < 10) {
                add("early-return");
                return;
            }
            
            add("normal-path");
        }
        
        test(5);
        return output[0], output[1];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(std::string(to_string(S, 0)), "early-return");
    ASSERT_EQ(std::string(to_string(S, 1)), "cleanup"); // Defer runs before return
}

TEST_F(DeferTest, DeferWithReturnValue)
{
    constexpr std::string_view code = R"(
        let executed = false;
        
        function test() {
            defer executed = true;
            return 42;
        }
        
        let result = test();
        return result, executed;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(to_integer(S, 0), 42);
    ASSERT_TRUE(to_boolean(S, 1));
}

TEST_F(DeferTest, MultipleDeferWithEarlyReturn)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;
        
        function add(s) {
            output[idx] = s;
            idx = idx + 1;
        }
        
        function test(shouldReturn) {
            defer add("defer1");
            defer add("defer2");
            defer add("defer3");
            
            if (shouldReturn) {
                return;
            }
            
            add("end");
        }
        
        test(true);
        return output[0], output[1], output[2];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(std::string(to_string(S, 0)), "defer3");
    ASSERT_EQ(std::string(to_string(S, 1)), "defer2");
    ASSERT_EQ(std::string(to_string(S, 2)), "defer1");
}

TEST_F(DeferTest, DeferInIfScope)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;
        
        function add(s) {
            output[idx] = s;
            idx = idx + 1;
        }
        
        function test(cond) {
            add("start");
            
            if (cond) {
                defer add("if-defer");
                add("if-body");
            }
            
            add("end");
        }
        
        test(true);
        return output[0], output[1], output[2], output[3];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 4);
    ASSERT_EQ(std::string(to_string(S, 0)), "start");
    ASSERT_EQ(std::string(to_string(S, 1)), "if-body");
    ASSERT_EQ(std::string(to_string(S, 2)), "if-defer"); // Defer executes when if-scope ends
    ASSERT_EQ(std::string(to_string(S, 3)), "end");
}

TEST_F(DeferTest, DeferAccessingLocalVariables)
{
    // Rewritten: the previous version returned a variable declared inside the
    // defer body, which only worked because defers were inlined before the
    // return expression was evaluated. A defer runs after the return value is
    // computed and its declarations are scoped to itself, so this checks what
    // the test name says instead: deferred code can read enclosing locals.
    constexpr std::string_view code = R"(
        let result = 0;
        function test() {
            let x = 10;
            let y = 20;
            defer result = x + y;
            return 1;
        }
        test();
        return result;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 30);
}

TEST_F(DeferTest, DeferWithFunctionCall)
{
    constexpr std::string_view code = R"(
        let closed = false;
        
        function cleanup() {
            closed = true;
        }
        
        function test() {
            defer cleanup();
            return "done";
        }
        
        let result = test();
        return result, closed;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(std::string(to_string(S, 0)), "done");
    ASSERT_TRUE(to_boolean(S, 1));
}

TEST_F(DeferTest, DeferPropagatesException)
{
    constexpr std::string_view code = R"(
        let executed = false;

        function test() {
            defer executed = true;
            error("test error");
        }

        test();
        return executed;
    )";

    ASSERT_ANY_THROW(run_script(code));
}

TEST_F(DeferTest, DeferExecutesOnException)
{
    constexpr std::string_view code = R"(
        let executed = false;

        function test() {
            defer executed = true;
            error("test error");
        }

        let ok = pcall(test);
        return ok, executed;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_FALSE(to_boolean(S, 0));
    ASSERT_TRUE(to_boolean(S, 1));
}

TEST_F(DeferTest, BlockDeferExecutesOnException)
{
    constexpr std::string_view code = R"(
        let output = {};

        function test() {
            {
                defer output[0] = "block";
                error("boom");
            }
        }

        pcall(test);
        return output[0];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "block");
}

TEST_F(DeferTest, DeferRunsForEveryFrameWhileUnwinding)
{
    constexpr std::string_view code = R"(
        let count = 0;

        function inner() {
            defer count = count + 1;
            error("boom");
        }

        function middle() {
            defer count = count + 1;
            inner();
        }

        function outer() {
            defer count = count + 1;
            middle();
        }

        pcall(outer);
        return count;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 3);
}

TEST_F(DeferTest, DeferErrorReplacesOriginalError)
{
    constexpr std::string_view code = R"(
        function test() {
            defer error("from defer");
            error("original");
        }

        let ok, msg = pcall(test);
        return ok, msg;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_FALSE(to_boolean(S, 0));
    ASSERT_NE(std::string(to_string(S, 1)).find("from defer"), std::string::npos);
}

TEST_F(DeferTest, DeferDoesNotDisturbMultipleReturnValues)
{
    constexpr std::string_view code = R"(
        function three() {
            return 1, 2, 3;
        }

        function test() {
            defer {
                let a = 9;
                let b = 10;
                let c = 11;
            }
            return three();
        }

        return test();
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(to_integer(S, 0), 1);
    ASSERT_EQ(to_integer(S, 1), 2);
    ASSERT_EQ(to_integer(S, 2), 3);
}

TEST_F(DeferTest, DeferDoesNotDisturbFixedReturnValues)
{
    constexpr std::string_view code = R"(
        function test() {
            defer {
                let a = 9;
                let b = 10;
                let c = 11;
            }
            return 1, 2, 3;
        }

        return test();
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(to_integer(S, 0), 1);
    ASSERT_EQ(to_integer(S, 1), 2);
    ASSERT_EQ(to_integer(S, 2), 3);
}

TEST_F(DeferTest, DeferInsideDefer)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;

        function add(s) {
            output[idx] = s;
            idx = idx + 1;
        }

        function test() {
            defer {
                add("outer-start");
                {
                    defer add("inner");
                    add("inner-body");
                }
                add("outer-end");
            }
            add("body");
        }

        test();
        return output[0], output[1], output[2], output[3], output[4];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 5);
    ASSERT_EQ(std::string(to_string(S, 0)), "body");
    ASSERT_EQ(std::string(to_string(S, 1)), "outer-start");
    ASSERT_EQ(std::string(to_string(S, 2)), "inner-body");
    ASSERT_EQ(std::string(to_string(S, 3)), "inner");
    ASSERT_EQ(std::string(to_string(S, 4)), "outer-end");
}

TEST_F(DeferTest, NestedBlockDefersRunInnermostFirstWhileUnwinding)
{
    constexpr std::string_view code = R"(
        let log = "";
        function test() {
            defer log = log + "F";
            {
                defer log = log + "B";
                {
                    defer log = log + "C";
                    error("boom");
                }
            }
        }

        pcall(test);
        return log;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "CBF");
}

TEST_F(DeferTest, BlockDeferThatAlreadyRanDoesNotRunAgainOnError)
{
    constexpr std::string_view code = R"(
        let log = "";
        function test() {
            defer log = log + "F";
            {
                defer log = log + "B";
                log = log + "b";
            }
            error("boom");
        }

        pcall(test);
        return log;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "bBF");
}

TEST_F(DeferTest, DeferStatementNeverReachedDoesNotRunOnError)
{
    constexpr std::string_view code = R"(
        let log = "";
        function test(flag) {
            defer log = log + "F";
            if (flag) {
                defer log = log + "X";
            }
            error("boom");
        }

        pcall(function() { test(false); });
        return log;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "F");
}

TEST_F(DeferTest, LoopBlockDeferRunsOncePerEnteredIteration)
{
    constexpr std::string_view code = R"(
        let log = "";
        function test() {
            let i = 0;
            while (i < 5) {
                defer log = log + "L";
                if (i == 2) {
                    error("boom");
                }
                i = i + 1;
            }
        }

        pcall(test);
        return log;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "LLL");
}

TEST_F(DeferTest, BlockLocalIsReadableByItsDeferWhileUnwinding)
{
    constexpr std::string_view code = R"(
        let log = "";
        function test() {
            {
                let x = "kept";
                defer log = log + x;
                error("boom");
            }
        }

        pcall(test);
        return log;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "kept");
}

TEST_F(DeferTest, ThrowingDeferDoesNotSkipRemainingDefers)
{
    constexpr std::string_view code = R"(
        let log = "";
        function test() {
            defer log = log + "A";
            defer error("boom");
            return 1;
        }

        let ok, msg = pcall(test);
        return log, ok, msg;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(std::string(to_string(S, 0)), "A");
    ASSERT_FALSE(to_boolean(S, 1));
    ASSERT_NE(std::string(to_string(S, 2)).find("boom"), std::string::npos);
}

TEST_F(DeferTest, ThrowingBlockDeferStillRunsOuterDefersWhileUnwinding)
{
    constexpr std::string_view code = R"(
        let log = "";
        function test() {
            defer log = log + "F";
            {
                defer error("from block defer");
                error("original");
            }
        }

        let ok, msg = pcall(test);
        return log, ok, msg;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(std::string(to_string(S, 0)), "F");
    ASSERT_FALSE(to_boolean(S, 1));
    ASSERT_NE(std::string(to_string(S, 2)).find("from block defer"), std::string::npos);
}

TEST_F(DeferTest, ThrowingDeferWhileADeeperFrameUnwinds)
{
    constexpr std::string_view code = R"(
        let log = "";

        function inner() {
            defer log = log + "i";
            error("deep");
        }

        function outer() {
            defer log = log + "o";
            {
                defer error("block");
                inner();
            }
        }

        let ok, msg = pcall(outer);
        return log, ok, msg;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(std::string(to_string(S, 0)), "io");
    ASSERT_FALSE(to_boolean(S, 1));
    ASSERT_NE(std::string(to_string(S, 2)).find("block"), std::string::npos);
}

TEST_F(DeferTest, DeferRunsWhenBreakLeavesTheScope)
{
    constexpr std::string_view code = R"(
        function test() {
            let out = "";
            let i = 0;
            while (i < 3) {
                defer out = out + "d";
                if (i == 1) {
                    break;
                }
                i = i + 1;
            }
            return out;
        }

        return test();
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "dd");
}

TEST_F(DeferTest, DeferRunsWhenContinueLeavesTheScope)
{
    constexpr std::string_view code = R"(
        function test() {
            let out = "";
            let i = 0;
            while (i < 3) {
                defer out = out + "c";
                i = i + 1;
                if (i == 2) {
                    continue;
                }
                out = out + ".";
            }
            return out;
        }

        return test();
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), ".cc.c");
}

TEST_F(DeferTest, DeferInNestedBlockRunsAtBlockExit)
{
    constexpr std::string_view code = R"(
        function test() {
            let out = "";
            {
                let x = "inner";
                defer out = out + x;
                out = out + "a";
            }
            out = out + "b";
            return out;
        }

        return test();
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "ainnerb");
}

TEST_F(DeferTest, DeferInForBodyRunsEveryIteration)
{
    constexpr std::string_view code = R"(
        function test() {
            let out = "";
            for (i = 0; i < 3; i = i + 1) {
                defer out = out + tostring(i);
            }
            return out;
        }

        return test();
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(std::string(to_string(S, 0)), "012");
}

TEST_F(DeferTest, BreakInsideDeferBodyIsRejected)
{
    constexpr std::string_view code = R"(
        function test() {
            let i = 0;
            while (i < 3) {
                defer { break; }
                i = i + 1;
            }
        }

        return test();
    )";

    ASSERT_ANY_THROW(run_script(code));
}

TEST_F(DeferTest, DeferInLoopRunsEveryIteration)
{
    constexpr std::string_view code = R"(
        let count = 0;

        function test() {
            let i = 0;
            while (i < 5) {
                defer count = count + 1;
                i = i + 1;
            }
        }

        test();
        return count;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 5);
}

TEST_F(DeferTest, MultipleNestedScopes)
{
    constexpr std::string_view code = R"(
        let output = {};
        let idx = 0;
        
        function add(s) {
            output[idx] = s;
            idx = idx + 1;
        }
        
        function test() {
            defer add("L0");
            {
                defer add("L1");
                {
                    defer add("L2");
                    {
                        defer add("L3");
                    }
                }
            }
        }
        
        test();
        return output[0], output[1], output[2], output[3];
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 4);
    ASSERT_EQ(std::string(to_string(S, 0)), "L3");
    ASSERT_EQ(std::string(to_string(S, 1)), "L2");
    ASSERT_EQ(std::string(to_string(S, 2)), "L1");
    ASSERT_EQ(std::string(to_string(S, 3)), "L0");
}

TEST_F(DeferTest, DeferWithTableAccess)
{
    constexpr std::string_view code = R"(
        let state = { file = nil, closed = false };
        
        function open_file(name) {
            state.file = name;
        }
        
        function close_file() {
            state.closed = true;
        }
        
        function test() {
            open_file("test.txt");
            defer close_file();
            return state.file;
        }
        
        let result = test();
        return result, state.closed;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(std::string(to_string(S, 0)), "test.txt");
    ASSERT_TRUE(to_boolean(S, 1));
}

TEST_F(DeferTest, DeferInMultipleFunctions)
{
    constexpr std::string_view code = R"(
        let count = 0;
        
        function func1() {
            defer count = count + 1;
        }
        
        function func2() {
            defer count = count + 10;
        }
        
        func1();
        func2();
        return count;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 11); // 1 from func1, 10 from func2
}

TEST_F(DeferTest, DeferBlockWithMultipleStatements)
{
    constexpr std::string_view code = R"(
        let a = 0;
        let b = 0;
        let c = 0;
        
        function test() {
            defer {
                a = 1;
                b = 2;
                c = 3;
            };
        }
        
        test();
        return a, b, c;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(to_integer(S, 0), 1);
    ASSERT_EQ(to_integer(S, 1), 2);
    ASSERT_EQ(to_integer(S, 2), 3);
}

TEST_F(DeferTest, DeferWithComplexExpression)
{
    constexpr std::string_view code = R"(
        let result = 0;
        
        function test() {
            let x = 5;
            defer result = x * x + 10;
            x = 3;
        }
        
        test();
        return result;
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 19); // 3 * 3 + 10 = 19
}

TEST_F(DeferTest, EmptyDeferBlock)
{
    constexpr std::string_view code = R"(
        function test() {
            defer {};
            return 42;
        }
        return test();
    )";

    ASSERT_NO_THROW(run_script(code));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 42);
}
