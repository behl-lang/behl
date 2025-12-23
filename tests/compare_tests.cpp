#include <behl/behl.hpp>
#include <gtest/gtest.h>

class CompareTest : public ::testing::Test
{
protected:
    behl::State* S;
    void SetUp() override
    {
        S = behl::new_state();
        behl::load_stdlib(S, true);
    }
    void TearDown() override
    {
        behl::close(S);
    }

    bool test_comparison(std::string_view code, bool should_error = false)
    {
        behl::load_string(S, code);

        if (should_error)
        {
            try
            {
                behl::call(S, 0, 1);
                behl::pop(S, 1); // Pop result if no exception
                return false;    // Should have thrown
            }
            catch (...)
            {
                return true; // Expected exception
            }
        }
        else
        {
            try
            {
                behl::call(S, 0, 1);
                behl::pop(S, 1); // Pop result
                return true;
            }
            catch (...)
            {
                return false; // Unexpected exception
            }
        }
    }
};

TEST_F(CompareTest, ComparisonsReturnBooleans_TrueCases)
{
    constexpr std::string_view code = "let a = {}\n"
                                      "a[0] = (1 == 1)\n"
                                      "a[1] = (1 != 2)\n"
                                      "a[2] = (1 < 2)\n"
                                      "a[3] = (2 <= 2)\n"
                                      "a[4] = (3 > 1)\n"
                                      "a[5] = (3 >= 3)\n"
                                      "return a";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    for (int i = 0; i <= 5; ++i)
    {
        behl::push_integer(S, i);
        behl::table_rawget(S, -2);
        ASSERT_TRUE(behl::to_boolean(S, -1)) << "Expected true at index " << i;
        behl::pop(S, 1);
    }
}

TEST_F(CompareTest, ComparisonsReturnBooleans_FalseCases)
{
    constexpr std::string_view code = "let a = {}\n"
                                      "a[0] = (1 == 2)\n"
                                      "a[1] = (1 != 1)\n"
                                      "a[2] = (2 < 1)\n"
                                      "a[3] = (2 <= 1)\n"
                                      "a[4] = (1 > 3)\n"
                                      "a[5] = (2 >= 3)\n"
                                      "return a";

    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    for (int i = 0; i <= 5; ++i)
    {
        behl::push_integer(S, i);
        behl::table_rawget(S, -2);
        ASSERT_FALSE(behl::to_boolean(S, -1)) << "Expected false at index " << i;
        behl::pop(S, 1);
    }
}

TEST_F(CompareTest, OrderingValid_NumberNumber)
{
    EXPECT_TRUE(test_comparison("return 1 < 2"));
    EXPECT_TRUE(test_comparison("return 1 <= 2"));
    EXPECT_TRUE(test_comparison("return 2 > 1"));
    EXPECT_TRUE(test_comparison("return 2 >= 1"));
    EXPECT_TRUE(test_comparison("return 1.5 < 2.5"));
    EXPECT_TRUE(test_comparison("return 1 < 2.5"));
}

TEST_F(CompareTest, OrderingValid_StringString)
{
    EXPECT_TRUE(test_comparison("return 'a' < 'b'"));
    EXPECT_TRUE(test_comparison("return 'a' <= 'b'"));
    EXPECT_TRUE(test_comparison("return 'b' > 'a'"));
    EXPECT_TRUE(test_comparison("return 'b' >= 'a'"));
}

TEST_F(CompareTest, OrderingError_NumberNil)
{
    EXPECT_TRUE(test_comparison("return 1 < nil", true));
    EXPECT_TRUE(test_comparison("return 1 <= nil", true));
    EXPECT_TRUE(test_comparison("return 1 > nil", true));
    EXPECT_TRUE(test_comparison("return 1 >= nil", true));
}

TEST_F(CompareTest, OrderingError_NumberString)
{
    EXPECT_TRUE(test_comparison("return 1 < 'str'", true));
    EXPECT_TRUE(test_comparison("return 1 <= 'str'", true));
    EXPECT_TRUE(test_comparison("return 1 > 'str'", true));
    EXPECT_TRUE(test_comparison("return 1 >= 'str'", true));
}

TEST_F(CompareTest, OrderingError_NumberBool)
{
    EXPECT_TRUE(test_comparison("return 1 < true", true));
    EXPECT_TRUE(test_comparison("return 1 <= true", true));
    EXPECT_TRUE(test_comparison("return 1 > true", true));
    EXPECT_TRUE(test_comparison("return 1 >= true", true));
}

TEST_F(CompareTest, OrderingError_NumberTable)
{
    EXPECT_TRUE(test_comparison("return 1 < {}", true));
    EXPECT_TRUE(test_comparison("return 1 <= {}", true));
    EXPECT_TRUE(test_comparison("return 1 > {}", true));
    EXPECT_TRUE(test_comparison("return 1 >= {}", true));
}

TEST_F(CompareTest, OrderingError_StringNumber)
{
    EXPECT_TRUE(test_comparison("return 'str' < 1", true));
    EXPECT_TRUE(test_comparison("return 'str' <= 1", true));
    EXPECT_TRUE(test_comparison("return 'str' > 1", true));
    EXPECT_TRUE(test_comparison("return 'str' >= 1", true));
}

TEST_F(CompareTest, OrderingError_NilNil)
{
    EXPECT_TRUE(test_comparison("return nil < nil", true));
    EXPECT_TRUE(test_comparison("return nil <= nil", true));
    EXPECT_TRUE(test_comparison("return nil > nil", true));
    EXPECT_TRUE(test_comparison("return nil >= nil", true));
}

TEST_F(CompareTest, OrderingError_BoolBool)
{
    EXPECT_TRUE(test_comparison("return true < false", true));
    EXPECT_TRUE(test_comparison("return true <= false", true));
    EXPECT_TRUE(test_comparison("return true > false", true));
    EXPECT_TRUE(test_comparison("return true >= false", true));
}

TEST_F(CompareTest, OrderingError_TableTable)
{
    EXPECT_TRUE(test_comparison("return {} < {}", true));
    EXPECT_TRUE(test_comparison("return {} <= {}", true));
    EXPECT_TRUE(test_comparison("return {} > {}", true));
    EXPECT_TRUE(test_comparison("return {} >= {}", true));
}

TEST_F(CompareTest, OrderingWithMetamethod_Lt)
{
    constexpr std::string_view code = R"(
        let t = {}
        setmetatable(t, {__lt = function(a, b) { return true }})
        return 1 < t
    )";
    EXPECT_TRUE(test_comparison(code));
}

TEST_F(CompareTest, OrderingWithMetamethod_Le)
{
    constexpr std::string_view code = R"(
        let t = {}
        setmetatable(t, {__le = function(a, b) { return true }})
        return 1 <= t
    )";
    EXPECT_TRUE(test_comparison(code));
}

TEST_F(CompareTest, EqualityNeverErrors_NumberNil)
{
    EXPECT_TRUE(test_comparison("return 1 == nil"));
    EXPECT_TRUE(test_comparison("return 1 != nil"));
}

TEST_F(CompareTest, EqualityNeverErrors_NumberString)
{
    EXPECT_TRUE(test_comparison("return 1 == 'str'"));
    EXPECT_TRUE(test_comparison("return 1 != 'str'"));
}

TEST_F(CompareTest, EqualityNeverErrors_NilNil)
{
    EXPECT_TRUE(test_comparison("return nil == nil"));
    EXPECT_TRUE(test_comparison("return nil != nil"));
}

TEST_F(CompareTest, EqualityNeverErrors_TableTable)
{
    EXPECT_TRUE(test_comparison("return {} == {}"));
    EXPECT_TRUE(test_comparison("return {} != {}"));
}

TEST_F(CompareTest, EqualityNeverErrors_AllTypeCombos)
{
    EXPECT_TRUE(test_comparison("return 1 == true"));
    EXPECT_TRUE(test_comparison("return 'str' == {}"));
    EXPECT_TRUE(test_comparison("return nil == false"));
    EXPECT_TRUE(test_comparison("return true != 'str'"));
}

TEST_F(CompareTest, EqualityWithMetamethod_Eq)
{
    constexpr std::string_view code = R"(
        let t1 = {}
        let t2 = {}
        setmetatable(t1, {__eq = function(a, b) { return true }})
        setmetatable(t2, {__eq = function(a, b) { return true }})
        return t1 == t2
    )";
    EXPECT_TRUE(test_comparison(code));
}

TEST_F(CompareTest, TernaryBasic_TrueCondition)
{
    constexpr std::string_view code = "return true ? 42 : 0";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryBasic_FalseCondition)
{
    constexpr std::string_view code = "return false ? 42 : 0";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 0);
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryWithComparison)
{
    constexpr std::string_view code = "let x = 10; return x > 5 ? 'big' : 'small'";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "big");
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryWithDifferentTypes)
{
    constexpr std::string_view code = "return 1 == 1 ? 'string' : 42";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "string");
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryNested)
{
    constexpr std::string_view code = "let x = 5; return x > 10 ? 'big' : x > 0 ? 'medium' : 'small'";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "medium");
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryNestedParens)
{
    constexpr std::string_view code = "let x = 5; return x > 10 ? 'big' : (x > 0 ? 'medium' : 'small')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "medium");
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryWithNil)
{
    constexpr std::string_view code = "return nil ? 1 : 2";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryWithZero)
{
    constexpr std::string_view code = "return 0 ? 'yes' : 'no'";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "yes");
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryWithFunctionCalls)
{
    constexpr std::string_view code = "function b() { return 1 } function c() { return 2 } let a = true; return a ? b() : c()";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryInAssignment)
{
    constexpr std::string_view code = "let x = 10 > 5 ? 100 : 200; return x";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
    behl::pop(S, 1);
}

TEST_F(CompareTest, TernaryWithTableAccess)
{
    constexpr std::string_view code = R"(
        let t = {a = 10, b = 20}
        return t['a'] > 5 ? t['a'] : t['b']
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
    behl::pop(S, 1);
}
