#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
using namespace behl;

class PCallTest : public ::testing::Test
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
};

TEST_F(PCallTest, SuccessfulCall)
{
    constexpr std::string_view code = R"(
        function add(a, b) {
            return a + b;
        }
        let success, result = pcall(add, 10, 20);
        return success, result;
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 2));

    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(type(S, -2), Type::kBoolean);
    ASSERT_TRUE(to_boolean(S, -2));
    ASSERT_EQ(type(S, -1), Type::kInteger);
    ASSERT_EQ(to_integer(S, -1), 30);
}

TEST_F(PCallTest, ErrorHandling)
{
    constexpr std::string_view code = R"(
        function failing_func() {
            error("intentional error");
        }
        success, err = pcall(failing_func);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_EQ(type(S, -1), Type::kBoolean);
    ASSERT_FALSE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "err");
    ASSERT_EQ(type(S, -1), Type::kString);
    std::string_view err_msg = to_string(S, -1);
    ASSERT_NE(err_msg.find("intentional error"), std::string_view::npos);
    pop(S, 1);
}

TEST_F(PCallTest, MultipleReturnValues)
{
    constexpr std::string_view code = R"(
        function multi_return(a, b, c) {
            return a, b, c, a + b + c;
        }
        success, r1, r2, r3, r4 = pcall(multi_return, 1, 2, 3);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "r1");
    ASSERT_EQ(to_integer(S, -1), 1);
    pop(S, 1);

    get_global(S, "r2");
    ASSERT_EQ(to_integer(S, -1), 2);
    pop(S, 1);

    get_global(S, "r3");
    ASSERT_EQ(to_integer(S, -1), 3);
    pop(S, 1);

    get_global(S, "r4");
    ASSERT_EQ(to_integer(S, -1), 6);
    pop(S, 1);
}

TEST_F(PCallTest, NoArguments)
{
    constexpr std::string_view code = R"(
        function get_value() {
            return 42;
        }
        success, value = pcall(get_value);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "value");
    ASSERT_EQ(to_integer(S, -1), 42);
    pop(S, 1);
}

TEST_F(PCallTest, RuntimeTypeError)
{
    constexpr std::string_view code = R"(
        function type_error() {
            let t = {};
            return t + 1; // This should cause a type error
        }
        success, err = pcall(type_error);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_FALSE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "err");
    ASSERT_EQ(type(S, -1), Type::kString);
    pop(S, 1);
}

TEST_F(PCallTest, DivisionByZero)
{
    constexpr std::string_view code = R"(
        function div_zero() {
            return 10 / 0;
        }
        success, result = pcall(div_zero);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_EQ(type(S, -1), Type::kBoolean);
    pop(S, 1);
}

TEST_F(PCallTest, ClosureCall)
{
    constexpr std::string_view code = R"(
        function make_adder(x) {
            return function(y) {
                return x + y;
            };
        }
        let add5 = make_adder(5);
        success, result = pcall(add5, 10);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "result");
    ASSERT_EQ(to_integer(S, -1), 15);
    pop(S, 1);
}

TEST_F(PCallTest, CFunctionCall)
{
    constexpr std::string_view code = R"(
        success, result = pcall(typeof, 42);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "result");
    ASSERT_EQ(type(S, -1), Type::kString);
    std::string_view type_name = to_string(S, -1);
    ASSERT_EQ(type_name, "integer");
    pop(S, 1);
}

TEST_F(PCallTest, NestedPCall)
{
    constexpr std::string_view code = R"(
        function inner() {
            error("inner error");
        }
        function outer() {
            let s, e = pcall(inner);
            if (!s) {
                return "caught: " + e;
            }
            return "no error";
        }
        success, result = pcall(outer);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "result");
    ASSERT_EQ(type(S, -1), Type::kString);
    std::string_view result = to_string(S, -1);
    ASSERT_NE(result.find("caught:"), std::string_view::npos);
    ASSERT_NE(result.find("inner error"), std::string_view::npos);
    pop(S, 1);
}

TEST_F(PCallTest, ReturnsNil)
{
    constexpr std::string_view code = R"(
        function returns_nil() {
            return nil;
        }
        success, result = pcall(returns_nil);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "result");
    ASSERT_EQ(type(S, -1), Type::kNil);
    pop(S, 1);
}

TEST_F(PCallTest, ReturnsNothing)
{
    constexpr std::string_view code = R"(
        function returns_nothing() {
        }
        success, result = pcall(returns_nothing);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "result");
    ASSERT_EQ(type(S, -1), Type::kNil);
    pop(S, 1);
}

TEST_F(PCallTest, NonCallableError)
{
    constexpr std::string_view code = R"(
        success, err = pcall(42);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    EXPECT_ANY_THROW(call(S, 0, 0));
}

TEST_F(PCallTest, CallableTable)
{
    constexpr std::string_view code = R"(
        function try_call_table() {
            let t = {};
            return t(); // This will cause error inside protected call
        }
        success, err = pcall(try_call_table);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_FALSE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "err");
    ASSERT_EQ(type(S, -1), Type::kString);
    pop(S, 1);
}

TEST_F(PCallTest, RecursiveError)
{
    constexpr std::string_view code = R"(
        function recurse(n) {
            if (n == 0) {
                error("reached zero");
            }
            return recurse(n - 1);
        }
        success, err = pcall(recurse, 5);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_FALSE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "err");
    ASSERT_EQ(type(S, -1), Type::kString);
    std::string_view err_msg = to_string(S, -1);
    ASSERT_NE(err_msg.find("reached zero"), std::string_view::npos);
    pop(S, 1);
}

TEST_F(PCallTest, ErrorWithCustomMessage)
{
    constexpr std::string_view code = R"(
        function validate(x) {
            if (x < 0) {
                error("value must be non-negative");
            }
            return x * 2;
        }
        s1, r1 = pcall(validate, 5);
        s2, r2 = pcall(validate, -3);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "s1");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "r1");
    ASSERT_EQ(to_integer(S, -1), 10);
    pop(S, 1);

    get_global(S, "s2");
    ASSERT_FALSE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "r2");
    std::string_view err = to_string(S, -1);
    ASSERT_NE(err.find("non-negative"), std::string_view::npos);
    pop(S, 1);
}

TEST_F(PCallTest, ErrorWithFormattedMessage)
{
    constexpr std::string_view code = R"(
        function divide(a, b) {
            if (b == 0) {
                error("cannot divide " + tostring(a) + " by zero");
            }
            return a / b;
        }
        success, result = pcall(divide, 10, 0);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    get_global(S, "success");
    ASSERT_FALSE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "result");
    std::string_view err = to_string(S, -1);
    ASSERT_NE(err.find("cannot divide"), std::string_view::npos);
    ASSERT_NE(err.find("10"), std::string_view::npos);
    ASSERT_NE(err.find("zero"), std::string_view::npos);
    pop(S, 1);
}

TEST_F(PCallTest, MultipleDifferentErrors)
{
    constexpr std::string_view code = R"(
        function err1() { error("error type 1"); }
        function err2() { error("error type 2"); }
        function err3() { error("error type 3"); }
        
        s1, e1 = pcall(err1);
        s2, e2 = pcall(err2);
        s3, e3 = pcall(err3);
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 0));

    for (int i = 1; i <= 3; ++i)
    {
        std::string success_var = "s" + std::to_string(i);
        std::string error_var = "e" + std::to_string(i);

        get_global(S, success_var.c_str());
        ASSERT_FALSE(to_boolean(S, -1));
        pop(S, 1);

        get_global(S, error_var.c_str());
        std::string_view err = to_string(S, -1);
        std::string expected = std::string("error type ") + std::to_string(i);
        ASSERT_NE(err.find(expected), std::string_view::npos);
        pop(S, 1);
    }
}

TEST_F(PCallTest, ErrorDoesntAffectState)
{
    constexpr std::string_view code = R"(
        let counter = 0;
        
        function increment_and_maybe_error(should_error) {
            counter = counter + 1;
            if (should_error) {
                error("intentional");
            }
            return counter;
        }
        
        s1, r1 = pcall(increment_and_maybe_error, false);
        s2, r2 = pcall(increment_and_maybe_error, true);
        s3, r3 = pcall(increment_and_maybe_error, false);
        
        return counter;
    )";

    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));

    ASSERT_EQ(to_integer(S, -1), 3);

    get_global(S, "s1");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "r1");
    ASSERT_EQ(to_integer(S, -1), 1);
    pop(S, 1);

    get_global(S, "s2");
    ASSERT_FALSE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "s3");
    ASSERT_TRUE(to_boolean(S, -1));
    pop(S, 1);

    get_global(S, "r3");
    ASSERT_EQ(to_integer(S, -1), 3);
    pop(S, 1);
}
