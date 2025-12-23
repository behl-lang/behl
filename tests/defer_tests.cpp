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
    constexpr std::string_view code = R"(
        function test() {
            let x = 10;
            let y = 20;
            defer let z = x + y;
            return z;
        }
        return test();
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

TEST_F(DeferTest, DeferDoesNotExecuteOnException)
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
