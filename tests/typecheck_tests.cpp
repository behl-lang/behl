#include "common/format.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>

class TypecheckTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(TypecheckTest, CheckType_ValidTypes)
{
    behl::push_nil(S);
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kNil));

    behl::push_boolean(S, true);
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kBoolean));

    behl::push_integer(S, 42);
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kInteger));

    behl::push_number(S, 3.14);
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kNumber));

    behl::push_string(S, "hello");
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kString));

    behl::table_new(S);
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kTable));

    behl::push_cfunction(S, [](behl::State*) -> int { return 0; });
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kCFunction));
}

TEST_F(TypecheckTest, CheckType_WrongType_ThrowsTypeError)
{
    behl::push_integer(S, 42);
    EXPECT_THROW(
        {
            try
            {
                behl::check_type(S, -1, behl::Type::kString);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected string, got integer"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, CheckType_MultipleTypes)
{
    behl::push_integer(S, 1);
    behl::push_string(S, "test");
    behl::push_boolean(S, false);

    EXPECT_NO_THROW(behl::check_type(S, 0, behl::Type::kInteger));
    EXPECT_NO_THROW(behl::check_type(S, 1, behl::Type::kString));
    EXPECT_NO_THROW(behl::check_type(S, 2, behl::Type::kBoolean));
    EXPECT_NO_THROW(behl::check_type(S, -3, behl::Type::kInteger));
    EXPECT_NO_THROW(behl::check_type(S, -2, behl::Type::kString));
    EXPECT_NO_THROW(behl::check_type(S, -1, behl::Type::kBoolean));
}

TEST_F(TypecheckTest, CheckInteger_ValidInteger)
{
    behl::push_integer(S, 42);
    EXPECT_EQ(behl::check_integer(S, -1), 42);

    behl::push_integer(S, -100);
    EXPECT_EQ(behl::check_integer(S, -1), -100);

    behl::push_integer(S, 0);
    EXPECT_EQ(behl::check_integer(S, -1), 0);
}

TEST_F(TypecheckTest, CheckInteger_FromFloatWithNoFraction)
{
    behl::push_number(S, 10.0);
    EXPECT_EQ(behl::check_integer(S, -1), 10);

    behl::push_number(S, -5.0);
    EXPECT_EQ(behl::check_integer(S, -1), -5);
}

TEST_F(TypecheckTest, CheckInteger_FromFloatWithFraction_Throws)
{
    behl::push_number(S, 3.14);
    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected integer"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, CheckInteger_WrongTypes_Throw)
{
    behl::push_string(S, "42");
    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected integer, got string"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_nil(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected integer, got nil"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_boolean(S, true);
    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected integer, got boolean"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::table_new(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected integer, got table"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, CheckInteger_InvalidIndex_Throws)
{
    behl::push_integer(S, 42);
    EXPECT_THROW(behl::check_integer(S, 10), behl::TypeError);
    EXPECT_THROW(behl::check_integer(S, -10), behl::TypeError);
}

TEST_F(TypecheckTest, CheckInteger_MultipleArguments)
{
    behl::push_integer(S, 10);
    behl::push_integer(S, 20);
    behl::push_integer(S, 30);

    EXPECT_EQ(behl::check_integer(S, 0), 10);
    EXPECT_EQ(behl::check_integer(S, 1), 20);
    EXPECT_EQ(behl::check_integer(S, 2), 30);
    EXPECT_EQ(behl::check_integer(S, -3), 10);
    EXPECT_EQ(behl::check_integer(S, -2), 20);
    EXPECT_EQ(behl::check_integer(S, -1), 30);
}

TEST_F(TypecheckTest, CheckNumber_ValidNumber)
{
    behl::push_number(S, 3.14);
    EXPECT_DOUBLE_EQ(behl::check_number(S, -1), 3.14);

    behl::push_number(S, -2.5);
    EXPECT_DOUBLE_EQ(behl::check_number(S, -1), -2.5);

    behl::push_number(S, 0.0);
    EXPECT_DOUBLE_EQ(behl::check_number(S, -1), 0.0);
}

TEST_F(TypecheckTest, CheckNumber_FromInteger)
{
    behl::push_integer(S, 42);
    EXPECT_DOUBLE_EQ(behl::check_number(S, -1), 42.0);

    behl::push_integer(S, -100);
    EXPECT_DOUBLE_EQ(behl::check_number(S, -1), -100.0);
}

TEST_F(TypecheckTest, CheckNumber_WrongTypes_Throw)
{
    behl::push_string(S, "3.14");
    EXPECT_THROW(
        {
            try
            {
                behl::check_number(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected number, got string"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_nil(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_number(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected number, got nil"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_boolean(S, false);
    EXPECT_THROW(
        {
            try
            {
                behl::check_number(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected number, got boolean"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::table_new(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_number(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected number, got table"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, CheckNumber_InvalidIndex_Throws)
{
    behl::push_number(S, 3.14);
    EXPECT_THROW(behl::check_number(S, 10), behl::TypeError);
    EXPECT_THROW(behl::check_number(S, -10), behl::TypeError);
}

TEST_F(TypecheckTest, CheckNumber_MixedNumericTypes)
{
    behl::push_integer(S, 10);
    behl::push_number(S, 20.5);
    behl::push_integer(S, 30);

    EXPECT_DOUBLE_EQ(behl::check_number(S, 0), 10.0);
    EXPECT_DOUBLE_EQ(behl::check_number(S, 1), 20.5);
    EXPECT_DOUBLE_EQ(behl::check_number(S, 2), 30.0);
}

TEST_F(TypecheckTest, CheckString_ValidString)
{
    behl::push_string(S, "hello");
    EXPECT_EQ(behl::check_string(S, -1), "hello");

    behl::push_string(S, "");
    EXPECT_EQ(behl::check_string(S, -1), "");

    behl::push_string(S, "with spaces and special chars !@#$%");
    EXPECT_EQ(behl::check_string(S, -1), "with spaces and special chars !@#$%");
}

TEST_F(TypecheckTest, CheckString_WrongTypes_Throw)
{
    behl::push_integer(S, 42);
    EXPECT_THROW(
        {
            try
            {
                behl::check_string(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected string, got integer"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_number(S, 3.14);
    EXPECT_THROW(
        {
            try
            {
                behl::check_string(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected string, got number"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_nil(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_string(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected string, got nil"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_boolean(S, true);
    EXPECT_THROW(
        {
            try
            {
                behl::check_string(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected string, got boolean"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::table_new(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_string(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected string, got table"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, CheckString_InvalidIndex_Throws)
{
    behl::push_string(S, "test");
    EXPECT_THROW(behl::check_string(S, 10), behl::TypeError);
    EXPECT_THROW(behl::check_string(S, -10), behl::TypeError);
}

TEST_F(TypecheckTest, CheckString_MultipleStrings)
{
    behl::push_string(S, "first");
    behl::push_string(S, "second");
    behl::push_string(S, "third");

    EXPECT_EQ(behl::check_string(S, 0), "first");
    EXPECT_EQ(behl::check_string(S, 1), "second");
    EXPECT_EQ(behl::check_string(S, 2), "third");
    EXPECT_EQ(behl::check_string(S, -3), "first");
    EXPECT_EQ(behl::check_string(S, -2), "second");
    EXPECT_EQ(behl::check_string(S, -1), "third");
}

TEST_F(TypecheckTest, CheckBoolean_ValidBoolean)
{
    behl::push_boolean(S, true);
    EXPECT_EQ(behl::check_boolean(S, -1), true);

    behl::push_boolean(S, false);
    EXPECT_EQ(behl::check_boolean(S, -1), false);
}

TEST_F(TypecheckTest, CheckBoolean_WrongTypes_Throw)
{
    behl::push_integer(S, 1);
    EXPECT_THROW(
        {
            try
            {
                behl::check_boolean(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected boolean, got integer"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_number(S, 1.0);
    EXPECT_THROW(
        {
            try
            {
                behl::check_boolean(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected boolean, got number"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_string(S, "true");
    EXPECT_THROW(
        {
            try
            {
                behl::check_boolean(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected boolean, got string"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_nil(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_boolean(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected boolean, got nil"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::table_new(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_boolean(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected boolean, got table"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, CheckBoolean_InvalidIndex_Throws)
{
    behl::push_boolean(S, true);
    EXPECT_THROW(behl::check_boolean(S, 10), behl::TypeError);
    EXPECT_THROW(behl::check_boolean(S, -10), behl::TypeError);
}

TEST_F(TypecheckTest, CheckBoolean_MultipleBooleans)
{
    behl::push_boolean(S, true);
    behl::push_boolean(S, false);
    behl::push_boolean(S, true);

    EXPECT_EQ(behl::check_boolean(S, 0), true);
    EXPECT_EQ(behl::check_boolean(S, 1), false);
    EXPECT_EQ(behl::check_boolean(S, 2), true);
    EXPECT_EQ(behl::check_boolean(S, -3), true);
    EXPECT_EQ(behl::check_boolean(S, -2), false);
    EXPECT_EQ(behl::check_boolean(S, -1), true);
}

TEST_F(TypecheckTest, CheckUserdata_ValidUserdata)
{
    constexpr uint32_t TestUID = behl::make_uid("TestType");
    void* ptr = behl::userdata_new(S, 64, TestUID);
    ASSERT_NE(ptr, nullptr);

    int* data = static_cast<int*>(ptr);
    *data = 42;

    void* retrieved = behl::check_userdata(S, -1, TestUID);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*static_cast<int*>(retrieved), 42);
}

TEST_F(TypecheckTest, CheckUserdata_WrongUID_Throws)
{
    constexpr uint32_t TestUID1 = behl::make_uid("TypeA");
    constexpr uint32_t TestUID2 = behl::make_uid("TypeB");

    behl::userdata_new(S, 64, TestUID1);

    EXPECT_THROW(
        {
            try
            {
                behl::check_userdata(S, -1, TestUID2);
            }
            catch (const behl::RuntimeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("uid"), std::string::npos);
                throw;
            }
        },
        behl::RuntimeError);
}

TEST_F(TypecheckTest, CheckUserdata_WrongType_Throws)
{
    constexpr uint32_t TestUID = behl::make_uid("TestType");

    behl::push_integer(S, 42);
    EXPECT_THROW(
        {
            try
            {
                behl::check_userdata(S, -1, TestUID);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected userdata, got integer"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_string(S, "test");
    EXPECT_THROW(
        {
            try
            {
                behl::check_userdata(S, -1, TestUID);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected userdata, got string"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::push_nil(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_userdata(S, -1, TestUID);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected userdata, got nil"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);

    behl::table_new(S);
    EXPECT_THROW(
        {
            try
            {
                behl::check_userdata(S, -1, TestUID);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("expected userdata, got table"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, CheckUserdata_InvalidIndex_Throws)
{
    constexpr uint32_t TestUID = behl::make_uid("TestType");
    behl::userdata_new(S, 64, TestUID);

    EXPECT_THROW(behl::check_userdata(S, 10, TestUID), behl::TypeError);
    EXPECT_THROW(behl::check_userdata(S, -10, TestUID), behl::TypeError);
}

TEST_F(TypecheckTest, CheckUserdata_MultipleUserdataWithDifferentUIDs)
{
    constexpr uint32_t UID1 = behl::make_uid("TypeA");
    constexpr uint32_t UID2 = behl::make_uid("TypeB");
    constexpr uint32_t UID3 = behl::make_uid("TypeC");

    behl::userdata_new(S, 8, UID1);
    behl::userdata_new(S, 8, UID2);
    behl::userdata_new(S, 8, UID3);

    EXPECT_NO_THROW(behl::check_userdata(S, 0, UID1));
    EXPECT_NO_THROW(behl::check_userdata(S, 1, UID2));
    EXPECT_NO_THROW(behl::check_userdata(S, 2, UID3));

    EXPECT_THROW(behl::check_userdata(S, 0, UID2), behl::RuntimeError);
    EXPECT_THROW(behl::check_userdata(S, 1, UID3), behl::RuntimeError);
    EXPECT_THROW(behl::check_userdata(S, 2, UID1), behl::RuntimeError);
}

TEST_F(TypecheckTest, CheckMixedTypes_AllValid)
{
    behl::push_integer(S, 42);
    behl::push_string(S, "hello");
    behl::push_boolean(S, true);
    behl::push_number(S, 3.14);

    EXPECT_EQ(behl::check_integer(S, 0), 42);
    EXPECT_EQ(behl::check_string(S, 1), "hello");
    EXPECT_EQ(behl::check_boolean(S, 2), true);
    EXPECT_DOUBLE_EQ(behl::check_number(S, 3), 3.14);
}

TEST_F(TypecheckTest, CheckMixedTypes_OneInvalid_Throws)
{
    behl::push_integer(S, 42);
    behl::push_string(S, "hello");
    behl::push_boolean(S, true);

    EXPECT_EQ(behl::check_integer(S, 0), 42);
    EXPECT_EQ(behl::check_string(S, 1), "hello");

    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, 2);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("argument #3"), std::string::npos);
                EXPECT_NE(std::string(e.what()).find("expected integer, got boolean"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, ArgumentNumbering_NegativeIndices)
{
    behl::push_integer(S, 1);
    behl::push_integer(S, 2);
    behl::push_string(S, "wrong");

    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, -1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("argument #3"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, ArgumentNumbering_PositiveIndices)
{
    behl::push_integer(S, 1);
    behl::push_string(S, "wrong");
    behl::push_integer(S, 3);

    EXPECT_THROW(
        {
            try
            {
                behl::check_integer(S, 1);
            }
            catch (const behl::TypeError& e)
            {
                EXPECT_NE(std::string(e.what()).find("argument #2"), std::string::npos);
                throw;
            }
        },
        behl::TypeError);
}

TEST_F(TypecheckTest, InCFunction_CheckTypes)
{
    behl::push_cfunction(S, [](behl::State* state) -> int {
        auto a = behl::check_integer(state, 0);
        auto b = behl::check_string(state, 1);
        auto c = behl::check_boolean(state, 2);

        behl::push_string(state, behl::format("{} {} {}", a, b, c ? "true" : "false"));
        return 1;
    });
    behl::set_global(S, "test_func");

    constexpr std::string_view code1 = R"(return test_func(42, "hello", true);)";
    ASSERT_NO_THROW(behl::load_string(S, code1));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "42 hello true");
    behl::pop(S, 1);

    constexpr std::string_view code2 = R"(return test_func(42, 123, true);)";
    ASSERT_NO_THROW(behl::load_string(S, code2));
    EXPECT_ANY_THROW(behl::call(S, 0, 1));
}

TEST_F(TypecheckTest, InCFunction_NumberConversion)
{
    behl::push_cfunction(S, [](behl::State* state) -> int {
        auto a = behl::check_number(state, 0);
        auto b = behl::check_number(state, 1);
        behl::push_number(state, a + b);
        return 1;
    });
    behl::set_global(S, "add_nums");

    constexpr std::string_view code1 = R"(return add_nums(10, 20);)";
    ASSERT_NO_THROW(behl::load_string(S, code1));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 30.0);
    behl::pop(S, 1);

    constexpr std::string_view code2 = R"(return add_nums(10.5, 20);)";
    ASSERT_NO_THROW(behl::load_string(S, code2));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 30.5);
    behl::pop(S, 1);

    constexpr std::string_view code3 = R"(return add_nums(3.14, 2.86);)";
    ASSERT_NO_THROW(behl::load_string(S, code3));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_DOUBLE_EQ(behl::to_number(S, -1), 6.0);
}

TEST_F(TypecheckTest, InCFunction_IntegerConversion)
{
    behl::push_cfunction(S, [](behl::State* state) -> int {
        auto a = behl::check_integer(state, 0);
        auto b = behl::check_integer(state, 1);
        behl::push_integer(state, a + b);
        return 1;
    });
    behl::set_global(S, "add_ints");

    constexpr std::string_view code1 = R"(return add_ints(10, 20);)";
    ASSERT_NO_THROW(behl::load_string(S, code1));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 30);
    behl::pop(S, 1);

    constexpr std::string_view code2 = R"(return add_ints(10, 20.0);)";
    ASSERT_NO_THROW(behl::load_string(S, code2));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 30);
    behl::pop(S, 1);

    constexpr std::string_view code3 = R"(return add_ints(10, 20.5);)";
    ASSERT_NO_THROW(behl::load_string(S, code3));

    try
    {
        behl::call(S, 0, 1);
        FAIL() << "Expected exception to be thrown";
    }
    catch (const behl::TypeError& e)
    {
        EXPECT_NE(std::string_view(e.what()).find("expected integer"), std::string_view::npos);
    }
}

TEST_F(TypecheckTest, TypenameHelpers)
{
    behl::push_nil(S);
    EXPECT_EQ(behl::value_typename(S, -1), "nil");

    behl::push_boolean(S, true);
    EXPECT_EQ(behl::value_typename(S, -1), "boolean");

    behl::push_integer(S, 42);
    EXPECT_EQ(behl::value_typename(S, -1), "integer");

    behl::push_number(S, 3.14);
    EXPECT_EQ(behl::value_typename(S, -1), "number");

    behl::push_string(S, "test");
    EXPECT_EQ(behl::value_typename(S, -1), "string");

    behl::table_new(S);
    EXPECT_EQ(behl::value_typename(S, -1), "table");

    behl::push_cfunction(S, [](behl::State*) -> int { return 0; });
    EXPECT_EQ(behl::value_typename(S, -1), "function");

    constexpr uint32_t TestUID = behl::make_uid("TestType");
    behl::userdata_new(S, 8, TestUID);
    EXPECT_EQ(behl::value_typename(S, -1), "userdata");
}

TEST_F(TypecheckTest, TypenameFromTypeEnum)
{
    EXPECT_EQ(behl::type_name(behl::Type::kNil), "nil");
    EXPECT_EQ(behl::type_name(behl::Type::kBoolean), "boolean");
    EXPECT_EQ(behl::type_name(behl::Type::kInteger), "integer");
    EXPECT_EQ(behl::type_name(behl::Type::kNumber), "number");
    EXPECT_EQ(behl::type_name(behl::Type::kString), "string");
    EXPECT_EQ(behl::type_name(behl::Type::kTable), "table");
    EXPECT_EQ(behl::type_name(behl::Type::kClosure), "function");
    EXPECT_EQ(behl::type_name(behl::Type::kCFunction), "function");
    EXPECT_EQ(behl::type_name(behl::Type::kUserdata), "userdata");
}
