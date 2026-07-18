#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>
#include <string>

class ErrorTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }

    std::string_view get_error()
    {
        EXPECT_EQ(behl::type(S, -1), behl::Type::kString);
        return behl::to_string(S, -1);
    }
};

TEST_F(ErrorTest, TypeError_ArithmeticOnNonNumber)
{
    constexpr std::string_view code = R"(
        let x = 5 + 'hello'
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CallNonFunction)
{
    constexpr std::string_view code = R"(
        let x = 5; x()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CallNil)
{
    constexpr std::string_view code = R"(
        let x = nil; x()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CallBoolean)
{
    constexpr std::string_view code = R"(
        let x = true; x()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CallNumber)
{
    constexpr std::string_view code = R"(
        let x = 3.14; x()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CallString)
{
    constexpr std::string_view code = R"(
        let x = "hello"; x()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CallTable)
{
    constexpr std::string_view code = R"(
        let x = {1, 2, 3}; x()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_IndexNonTable)
{
    constexpr std::string_view code = R"(
        let x = 5; let y = x[1]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_GetLengthOfInvalidType)
{
    constexpr std::string_view code = R"(
        let x = #123
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, ErrorIncludesSourceLocation)
{
    constexpr std::string_view code = R"(
        let x = 1 + nil
    )";
    ASSERT_NO_THROW(behl::load_string(S, code, false));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, ErrorInNestedFunction)
{
    constexpr std::string_view code = R"(
        function outer() {
            function inner() {
                return 5 + 'bad'
            }
            return inner()
        }
        outer()
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}

TEST_F(ErrorTest, MultipleOperationsShowCorrectError)
{
    constexpr std::string_view code = R"(
        let a = 5
        let b = 10
        let c = a + b
        let d = c * 'invalid'
    )";

    ASSERT_NO_THROW(behl::load_string(S, code, false)); // Disable optimizations
    EXPECT_THROW({ behl::call(S, 0, 0); }, behl::TypeError);
}
TEST_F(ErrorTest, ErrorFunction_BasicThrow)
{
    constexpr std::string_view code = R"(
        error("test error message")
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

    try
    {
        behl::call(S, 0, 0);
        FAIL() << "Expected exception to be thrown";
    }
    catch (const behl::BehlException& e)
    {
        EXPECT_NE(std::string_view(e.what()).find("test error message"), std::string_view::npos);
    }
}

TEST_F(ErrorTest, ErrorFunction_CaughtByPcall)
{
    constexpr std::string_view code = R"(
        function failing() {
            error("expected error");
        }
        let success, err = pcall(failing);
        return success, err;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));

    ASSERT_EQ(behl::get_top(S), 2);
    ASSERT_FALSE(behl::to_boolean(S, -2));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("expected error"), std::string::npos);
}

TEST_F(ErrorTest, ErrorFunction_WithNumberConvertsToString)
{
    constexpr std::string_view code = R"(
        function test() {
            error(42);
        }
        let success, err = pcall(test);
        return success, err;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("42"), std::string::npos);
}

TEST_F(ErrorTest, ErrorFunction_InNestedCalls)
{
    constexpr std::string_view code = R"(
        function level3() {
            error("deep error");
        }
        function level2() {
            return level3();
        }
        function level1() {
            return level2();
        }
        let success, err = pcall(level1);
        return success, err;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("deep error"), std::string::npos);
}

TEST_F(ErrorTest, ErrorFunction_WithConcatenatedMessage)
{
    constexpr std::string_view code = R"(
        let value = 123;
        function test() {
            error("Value is: " + tostring(value));
        }
        let success, err = pcall(test);
        return success, err;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("Value is:"), std::string::npos);
    EXPECT_NE(err_msg.find("123"), std::string::npos);
}

TEST_F(ErrorTest, ErrorFunction_InClosure)
{
    constexpr std::string_view code = R"(
        function make_error_func(msg) {
            return function() {
                error(msg);
            };
        }
        let error_func = make_error_func("closure error");
        let success, err = pcall(error_func);
        return success, err;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("closure error"), std::string::npos);
}

TEST_F(ErrorTest, ErrorFunction_MultipleInSequence)
{
    constexpr std::string_view code = R"(
        function error1() { error("first error"); }
        function error2() { error("second error"); }
        
        let s1, e1 = pcall(error1);
        let s2, e2 = pcall(error2);
        
        return s1, e1, s2, e2;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));

    ASSERT_EQ(behl::get_top(S), 4);

    ASSERT_FALSE(behl::to_boolean(S, -4));
    std::string_view err1 = behl::to_string(S, -3);
    EXPECT_NE(err1.find("first error"), std::string::npos);

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err2 = behl::to_string(S, -1);
    EXPECT_NE(err2.find("second error"), std::string::npos);
}

TEST_F(ErrorTest, ErrorFunction_InLoop)
{
    constexpr std::string_view code = R"(
        function test() {
            for (let i = 0; i < 10; i++) {
                if (i == 5) {
                    error("loop error at " + tostring(i));
                }
            }
        }
        let success, err = pcall(test);
        return success, err;
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));

    ASSERT_FALSE(behl::to_boolean(S, -2));
    std::string_view err_msg = behl::to_string(S, -1);
    EXPECT_NE(err_msg.find("loop error"), std::string::npos);
    EXPECT_NE(err_msg.find("5"), std::string::npos);
}

TEST_F(ErrorTest, TypeError_CompareIncompatibleTypes_LessThan)
{
    constexpr std::string_view code = R"(
        return 5 < "hello";
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CompareIncompatibleTypes_LessOrEqual)
{
    constexpr std::string_view code = R"(
        return true <= 42;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::TypeError);
}

TEST_F(ErrorTest, TypeError_CompareTableWithNumber)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3};
        return t < 10;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_THROW({ behl::call(S, 0, 1); }, behl::TypeError);
}
