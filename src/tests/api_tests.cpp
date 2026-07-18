#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>
#include <set>
#include <string>
using namespace behl;

class APITest : public ::testing::Test
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
};

#define ASSERT_STRVIEWEQ(a, b) ASSERT_EQ(std::string_view(a), std::string_view(b))

TEST_F(APITest, StackBasics)
{
    push_integer(S, 42);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, 0), 42);
    ASSERT_EQ(to_integer(S, -1), 42);
    ASSERT_EQ(type(S, 0), behl::Type::kInteger);

    push_string(S, "hello");
    ASSERT_EQ(get_top(S), 2);
    ASSERT_TRUE(type(S, -1) == behl::Type::kString);
    const auto str = to_string(S, -1);
    ASSERT_STRVIEWEQ(str, "hello");
    ASSERT_EQ(to_integer(S, -2), 42);
    ASSERT_EQ(to_integer(S, 1), 0);
    ASSERT_EQ(type(S, 1), behl::Type::kString);

    pop(S, 1);
    ASSERT_EQ(get_top(S), 1);
    set_top(S, 3);
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(type(S, -1), behl::Type::kNil);
    ASSERT_EQ(type(S, 2), behl::Type::kNil);
}

TEST_F(APITest, DupFunction)
{
    push_integer(S, 123);
    dup(S, -1);
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(to_integer(S, -1), 123);
    ASSERT_EQ(to_integer(S, -2), 123);

    dup(S, 0);
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(to_integer(S, -1), 123);
    ASSERT_EQ(to_integer(S, -2), 123);
    ASSERT_EQ(to_integer(S, -3), 123);
}

TEST_F(APITest, TableBasics)
{
    table_new(S);
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(type(S, -1), behl::Type::kTable);

    push_integer(S, 1);
    push_integer(S, 42);
    table_rawset(S, -3);
    ASSERT_EQ(get_top(S), 1);

    push_integer(S, 1);
    table_rawget(S, -2);
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(to_integer(S, -1), 42);
}

TEST_F(APITest, GetTable)
{
    table_new(S);

    push_string(S, "key");
    push_string(S, "value");
    table_rawset(S, -3);

    push_string(S, "key");
    table_rawget(S, -2);
    ASSERT_EQ(type(S, -1), behl::Type::kString);
    const auto str = to_string(S, -1);
    ASSERT_STRVIEWEQ(str, "value");

    push_string(S, "nonexistent");
    table_rawget(S, -2);
    ASSERT_EQ(type(S, -1), behl::Type::kNil);
}

TEST_F(APITest, SetTable)
{
    table_new(S);

    push_integer(S, 1);
    push_integer(S, 100);
    table_rawset(S, -3);

    push_integer(S, 1);
    table_rawget(S, -2);
    ASSERT_EQ(to_integer(S, -1), 100);

    push_string(S, "str");
    push_integer(S, 200);
    table_rawset(S, -4);

    push_string(S, "str");
    table_rawget(S, -3);
    ASSERT_EQ(to_integer(S, -1), 200);
}

TEST_F(APITest, GetField)
{
    table_new(S);

    push_string(S, "field");
    push_integer(S, 42);
    table_rawset(S, -3);

    table_getfield(S, -1, "field");
    ASSERT_EQ(to_integer(S, -1), 42);
}

TEST_F(APITest, SetField)
{
    table_new(S);

    push_integer(S, 99);
    table_setfield(S, -2, "field");

    table_getfield(S, -1, "field");
    ASSERT_EQ(to_integer(S, -1), 99);
}

TEST_F(APITest, NextFunction)
{
    table_new(S);

    push_string(S, "a");
    push_integer(S, 1);
    table_rawset(S, -3);

    push_string(S, "b");
    push_integer(S, 2);
    table_rawset(S, -3);

    push_nil(S);
    while (table_next(S, -2))
    {
        ASSERT_EQ(get_top(S), 3);

        pop(S, 1);
    }
    ASSERT_EQ(get_top(S), 1);
}

TEST_F(APITest, LenFunction)
{
    table_new(S);

    for (int i = 0; i < 3; ++i)
    {
        push_integer(S, i);
        push_integer(S, i + 1);
        table_rawset(S, -3);
    }

    table_len(S, -1);
    ASSERT_EQ(to_integer(S, -1), 3);
}

TEST_F(APITest, TableWithHoles)
{
    table_new(S);

    push_integer(S, 0);
    push_integer(S, 1);
    table_rawset(S, -3);

    push_integer(S, 2);
    push_integer(S, 3);
    table_rawset(S, -3);

    table_len(S, -1);
    ASSERT_EQ(to_integer(S, -1), 1);
}

TEST_F(APITest, TableCyclesAndGC)
{
    table_new(S);
    table_new(S);

    push_string(S, "self");
    dup(S, -2);
    table_rawset(S, -3);

    push_string(S, "self");
    dup(S, -2);
    table_rawset(S, -5);

    behl::gc_collect(S);
    ASSERT_EQ(get_top(S), 2);
}

TEST_F(APITest, TableIteration)
{
    table_new(S);

    push_string(S, "a");
    push_integer(S, 1);
    table_rawset(S, -3);

    push_string(S, "b");
    push_integer(S, 2);
    table_rawset(S, -3);

    std::set<std::string_view> keys;
    push_nil(S);
    while (table_next(S, -2))
    {
        ASSERT_EQ(get_top(S), 3);
        ASSERT_EQ(type(S, -2), Type::kString);
        const auto key = to_string(S, -2);
        keys.insert(key);
        pop(S, 1);
    }

    ASSERT_EQ(keys.size(), 2);
    ASSERT_TRUE(keys.count("a"));
    ASSERT_TRUE(keys.count("b"));
}

static int c_add2(State* S)
{
    long long a = to_integer(S, 0);
    long long b = to_integer(S, 1);
    push_integer(S, a + b);
    return 1;
}

TEST_F(APITest, CallNativeFunctionDirect)
{
    push_cfunction(S, &c_add2);
    push_integer(S, 3);
    push_integer(S, 4);
    ASSERT_NO_THROW(call(S, 2, 1));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, -1), 7);
}

TEST_F(APITest, RegisterNativeFunctionAndCallFromScript)
{
    register_function(S, "add2", &c_add2);
    constexpr std::string_view code = "return add2(10, 32);";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, -1), 42);
}

static int c_fail(State* S)
{
    error(S, "TypeError: native broke");
}

TEST_F(APITest, NativeFunctionErrorPropagates)
{
    push_cfunction(S, &c_fail);
    try
    {
        call(S, 0, 1);
        FAIL() << "Expected exception to be thrown";
    }
    catch (const behl::RuntimeError& e)
    {
        EXPECT_NE(std::string_view(e.what()).find("TypeError: native broke"), std::string_view::npos);
    }
}

static int c_add_checked(State* S)
{
    long long a = check_integer(S, 0);
    long long b = check_integer(S, 1);
    push_integer(S, a + b);
    return 1;
}

TEST_F(APITest, NativeCheckHelpers)
{
    push_cfunction(S, &c_add_checked);
    push_integer(S, 5);
    push_integer(S, 37);
    ASSERT_NO_THROW(call(S, 2, 1));
    ASSERT_EQ(to_integer(S, -1), 42);
    pop(S, 1);

    push_cfunction(S, &c_add_checked);
    push_integer(S, 5);
    push_string(S, "oops");
    EXPECT_THROW({ call(S, 2, 1); }, behl::TypeError);
}

static int c_return_multiple(State* S)
{
    push_integer(S, 1);
    push_integer(S, 2);
    push_integer(S, 3);
    return 3;
}

static int c_return_none(State* S)
{
    (void)S;
    return 0;
}

static int c_return_one(State* S)
{
    push_integer(S, 42);
    return 1;
}

TEST_F(APITest, CallWithMultretReturnsAllResults)
{
    push_cfunction(S, &c_return_multiple);
    ASSERT_NO_THROW(call(S, 0, kMultRet));
    ASSERT_EQ(get_top(S), 3);
    ASSERT_EQ(to_integer(S, -3), 1);
    ASSERT_EQ(to_integer(S, -2), 2);
    ASSERT_EQ(to_integer(S, -1), 3);
}

TEST_F(APITest, CallWithMultretHandlesZeroResults)
{
    push_cfunction(S, &c_return_none);
    ASSERT_NO_THROW(call(S, 0, kMultRet));
    ASSERT_EQ(get_top(S), 0);
}

TEST_F(APITest, CallWithMultretHandlesOneResult)
{
    push_cfunction(S, &c_return_one);
    ASSERT_NO_THROW(call(S, 0, kMultRet));
    ASSERT_EQ(get_top(S), 1);
    ASSERT_EQ(to_integer(S, -1), 42);
}

TEST_F(APITest, CallWithFixedResultCountTruncates)
{
    push_cfunction(S, &c_return_multiple);
    ASSERT_NO_THROW(call(S, 0, 2));
    ASSERT_EQ(get_top(S), 2);
    ASSERT_EQ(to_integer(S, -2), 1);
    ASSERT_EQ(to_integer(S, -1), 2);
}

TEST_F(APITest, CallWithFixedResultCountPadsWithNil)
{
    push_cfunction(S, &c_return_multiple);
    ASSERT_NO_THROW(call(S, 0, 5));
    ASSERT_EQ(get_top(S), 5);
    ASSERT_EQ(to_integer(S, -5), 1);
    ASSERT_EQ(to_integer(S, -4), 2);
    ASSERT_EQ(to_integer(S, -3), 3);
    ASSERT_EQ(type(S, -2), Type::kNil);
    ASSERT_EQ(type(S, -1), Type::kNil);
}

TEST_F(APITest, CallWithZeroResultsDiscards)
{
    push_cfunction(S, &c_return_multiple);
    ASSERT_NO_THROW(call(S, 0, 0));
    ASSERT_EQ(get_top(S), 0);
}
