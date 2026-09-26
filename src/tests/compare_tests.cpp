#include "state.hpp"
#include "test_helpers.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class CompareTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S;
    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        behl::load_stdlib(S);
    }
    void TearDown() override
    {
        behl::close(S);
    }

    bool test_comparison(std::string_view code, bool should_error = false)
    {
        const auto loaded = behl_test::load_ok(S, code);
        if (!loaded)
        {
            ADD_FAILURE() << loaded.message();
            return false;
        }

        if (should_error)
        {
            if (behl::call(S, 0, 1) >= 0)
            {
                behl::pop(S, 1); // Pop result if no exception
                return false;    // Should have thrown
            }
            return true; // Expected exception
        }
        else
        {
            if (behl::call(S, 0, 1) < 0)
            {
                return false; // Unexpected exception
            }
            behl::pop(S, 1); // Pop result
            return true;
        }
    }
};

TEST_P(CompareTest, ComparisonsReturnBooleans_TrueCases)
{
    constexpr std::string_view code = "let a = {}\n"
                                      "a[0] = (1 == 1)\n"
                                      "a[1] = (1 != 2)\n"
                                      "a[2] = (1 < 2)\n"
                                      "a[3] = (2 <= 2)\n"
                                      "a[4] = (3 > 1)\n"
                                      "a[5] = (3 >= 3)\n"
                                      "return a";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
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

TEST_P(CompareTest, ComparisonsReturnBooleans_FalseCases)
{
    constexpr std::string_view code = "let a = {}\n"
                                      "a[0] = (1 == 2)\n"
                                      "a[1] = (1 != 1)\n"
                                      "a[2] = (2 < 1)\n"
                                      "a[3] = (2 <= 1)\n"
                                      "a[4] = (1 > 3)\n"
                                      "a[5] = (2 >= 3)\n"
                                      "return a";

    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
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

TEST_P(CompareTest, OrderingValid_NumberNumber)
{
    EXPECT_TRUE(test_comparison("return 1 < 2"));
    EXPECT_TRUE(test_comparison("return 1 <= 2"));
    EXPECT_TRUE(test_comparison("return 2 > 1"));
    EXPECT_TRUE(test_comparison("return 2 >= 1"));
    EXPECT_TRUE(test_comparison("return 1.5 < 2.5"));
    EXPECT_TRUE(test_comparison("return 1 < 2.5"));
}

TEST_P(CompareTest, OrderingValid_StringString)
{
    EXPECT_TRUE(test_comparison("return 'a' < 'b'"));
    EXPECT_TRUE(test_comparison("return 'a' <= 'b'"));
    EXPECT_TRUE(test_comparison("return 'b' > 'a'"));
    EXPECT_TRUE(test_comparison("return 'b' >= 'a'"));
}

TEST_P(CompareTest, OrderingError_NumberNil)
{
    EXPECT_TRUE(test_comparison("return 1 < nil", true));
    EXPECT_TRUE(test_comparison("return 1 <= nil", true));
    EXPECT_TRUE(test_comparison("return 1 > nil", true));
    EXPECT_TRUE(test_comparison("return 1 >= nil", true));
}

TEST_P(CompareTest, OrderingError_NumberString)
{
    EXPECT_TRUE(test_comparison("return 1 < 'str'", true));
    EXPECT_TRUE(test_comparison("return 1 <= 'str'", true));
    EXPECT_TRUE(test_comparison("return 1 > 'str'", true));
    EXPECT_TRUE(test_comparison("return 1 >= 'str'", true));
}

TEST_P(CompareTest, OrderingError_NumberBool)
{
    EXPECT_TRUE(test_comparison("return 1 < true", true));
    EXPECT_TRUE(test_comparison("return 1 <= true", true));
    EXPECT_TRUE(test_comparison("return 1 > true", true));
    EXPECT_TRUE(test_comparison("return 1 >= true", true));
}

TEST_P(CompareTest, OrderingError_NumberTable)
{
    EXPECT_TRUE(test_comparison("return 1 < {}", true));
    EXPECT_TRUE(test_comparison("return 1 <= {}", true));
    EXPECT_TRUE(test_comparison("return 1 > {}", true));
    EXPECT_TRUE(test_comparison("return 1 >= {}", true));
}

TEST_P(CompareTest, OrderingError_StringNumber)
{
    EXPECT_TRUE(test_comparison("return 'str' < 1", true));
    EXPECT_TRUE(test_comparison("return 'str' <= 1", true));
    EXPECT_TRUE(test_comparison("return 'str' > 1", true));
    EXPECT_TRUE(test_comparison("return 'str' >= 1", true));
}

TEST_P(CompareTest, OrderingError_NilNil)
{
    EXPECT_TRUE(test_comparison("return nil < nil", true));
    EXPECT_TRUE(test_comparison("return nil <= nil", true));
    EXPECT_TRUE(test_comparison("return nil > nil", true));
    EXPECT_TRUE(test_comparison("return nil >= nil", true));
}

TEST_P(CompareTest, OrderingError_BoolBool)
{
    EXPECT_TRUE(test_comparison("return true < false", true));
    EXPECT_TRUE(test_comparison("return true <= false", true));
    EXPECT_TRUE(test_comparison("return true > false", true));
    EXPECT_TRUE(test_comparison("return true >= false", true));
}

TEST_P(CompareTest, OrderingError_TableTable)
{
    EXPECT_TRUE(test_comparison("return {} < {}", true));
    EXPECT_TRUE(test_comparison("return {} <= {}", true));
    EXPECT_TRUE(test_comparison("return {} > {}", true));
    EXPECT_TRUE(test_comparison("return {} >= {}", true));
}

TEST_P(CompareTest, OrderingWithMetamethod_Lt)
{
    constexpr std::string_view code = R"(
        let t = {}
        setmetatable(t, {__lt = function(a, b) { return true }})
        return 1 < t
    )";
    EXPECT_TRUE(test_comparison(code));
}

TEST_P(CompareTest, OrderingWithMetamethod_Le)
{
    constexpr std::string_view code = R"(
        let t = {}
        setmetatable(t, {__le = function(a, b) { return true }})
        return 1 <= t
    )";
    EXPECT_TRUE(test_comparison(code));
}

TEST_P(CompareTest, EqualityNeverErrors_NumberNil)
{
    EXPECT_TRUE(test_comparison("return 1 == nil"));
    EXPECT_TRUE(test_comparison("return 1 != nil"));
}

TEST_P(CompareTest, EqualityNeverErrors_NumberString)
{
    EXPECT_TRUE(test_comparison("return 1 == 'str'"));
    EXPECT_TRUE(test_comparison("return 1 != 'str'"));
}

TEST_P(CompareTest, EqualityNeverErrors_NilNil)
{
    EXPECT_TRUE(test_comparison("return nil == nil"));
    EXPECT_TRUE(test_comparison("return nil != nil"));
}

TEST_P(CompareTest, EqualityNeverErrors_TableTable)
{
    EXPECT_TRUE(test_comparison("return {} == {}"));
    EXPECT_TRUE(test_comparison("return {} != {}"));
}

TEST_P(CompareTest, EqualityNeverErrors_AllTypeCombos)
{
    EXPECT_TRUE(test_comparison("return 1 == true"));
    EXPECT_TRUE(test_comparison("return 'str' == {}"));
    EXPECT_TRUE(test_comparison("return nil == false"));
    EXPECT_TRUE(test_comparison("return true != 'str'"));
}

TEST_P(CompareTest, EqualityWithMetamethod_Eq)
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

TEST_P(CompareTest, TernaryBasic_TrueCondition)
{
    constexpr std::string_view code = "return true ? 42 : 0";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryBasic_FalseCondition)
{
    constexpr std::string_view code = "return false ? 42 : 0";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 0);
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryWithComparison)
{
    constexpr std::string_view code = "let x = 10; return x > 5 ? 'big' : 'small'";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "big");
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryWithDifferentTypes)
{
    constexpr std::string_view code = "return 1 == 1 ? 'string' : 42";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "string");
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryNested)
{
    constexpr std::string_view code = "let x = 5; return x > 10 ? 'big' : x > 0 ? 'medium' : 'small'";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "medium");
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryNestedParens)
{
    constexpr std::string_view code = "let x = 5; return x > 10 ? 'big' : (x > 0 ? 'medium' : 'small')";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "medium");
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryWithNil)
{
    constexpr std::string_view code = "return nil ? 1 : 2";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 2);
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryWithZero)
{
    constexpr std::string_view code = "return 0 ? 'yes' : 'no'";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "yes");
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryWithFunctionCalls)
{
    constexpr std::string_view code = "function b() { return 1 } function c() { return 2 } let a = true; return a ? b() : c()";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 1);
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryInAssignment)
{
    constexpr std::string_view code = "let x = 10 > 5 ? 100 : 200; return x";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 100);
    behl::pop(S, 1);
}

TEST_P(CompareTest, TernaryWithTableAccess)
{
    constexpr std::string_view code = R"(
        let t = {a = 10, b = 20}
        return t['a'] > 5 ? t['a'] : t['b']
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
    behl::pop(S, 1);
}

TEST_P(CompareTest, StringOrderingWithNulBytesAndPrefixes)
{
    constexpr std::string_view code = R"(
        const string = import("string");
        let nul = string.char(0);
        let r = "";
        r = r + (("alo" < "alo" + nul) ? "T" : "F");
        r = r + (("alo" + nul < "alo") ? "T" : "F");
        r = r + (("alo" + nul == "alo") ? "T" : "F");
        r = r + (("alo" <= "alo" + nul) ? "T" : "F");
        r = r + (("alo" + nul + "x" > "alo" + nul) ? "T" : "F");
        r = r + (("a" + nul + "b" < "a" + nul + "c") ? "T" : "F");
        r = r + ((nul < string.char(1)) ? "T" : "F");
        r = r + ((nul > "") ? "T" : "F");
        r = r + (("" < "a") ? "T" : "F");
        r = r + (("a" < "") ? "T" : "F");
        r = r + (("ab" < "abc") ? "T" : "F");
        r = r + (("Z" < "a") ? "T" : "F");
        r = r + (("a" + string.char(255) > "a" + string.char(1)) ? "T" : "F");
        return r;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "TFFTTTTTTFTTT");
}

TEST_P(CompareTest, NanComparisonsInValueContextAreFalse)
{
    constexpr std::string_view code = R"(
        let nan = 0.0 / 0.0;
        let one = 1;
        let onef = 1.0;
        let r = "";
        r = r + ((nan == nan) ? "T" : "F");
        r = r + ((nan < one) ? "T" : "F");
        r = r + ((nan <= one) ? "T" : "F");
        r = r + ((nan > one) ? "T" : "F");
        r = r + ((nan >= one) ? "T" : "F");
        r = r + ((onef < nan) ? "T" : "F");
        r = r + ((onef >= nan) ? "T" : "F");
        r = r + ((nan != nan) ? "T" : "F");
        return r, nan < 1, nan >= 1.0;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(behl::to_string(S, -3), "FFFFFFFT");
    EXPECT_FALSE(behl::to_boolean(S, -2));
    EXPECT_FALSE(behl::to_boolean(S, -1));
}

TEST_P(CompareTest, NanComparisonsInBranchContextAreFalse)
{
    constexpr std::string_view code = R"(
        let nan = 0.0 / 0.0;
        let one = 1;
        let onef = 1.0;
        let r = "";
        if (nan < 1) { r = r + "a"; }
        if (nan <= 1) { r = r + "b"; }
        if (nan > 1) { r = r + "c"; }
        if (nan >= 1) { r = r + "d"; }
        if (nan < one) { r = r + "e"; }
        if (nan < onef) { r = r + "f"; }
        if (one < nan) { r = r + "g"; }
        if (one > nan) { r = r + "h"; }
        if (nan == nan) { r = r + "i"; }
        if (onef <= nan) { r = r + "j"; }
        if (onef >= nan) { r = r + "k"; }
        if (nan < 1.0) { r = r + "l"; }
        return r;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "");
}

TEST_P(CompareTest, NanInequalityInBranchContextIsTrue)
{
    constexpr std::string_view code = R"(
        let nan = 0.0 / 0.0;
        let r = "";
        if (nan != nan) { r = r + "a"; }
        if (!(nan < 1)) { r = r + "b"; }
        if (!(nan >= 1)) { r = r + "c"; }
        return r;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "abc");
}

TEST_P(CompareTest, NanLoopConditionDoesNotEnterBody)
{
    constexpr std::string_view code = R"(
        let nan = 0.0 / 0.0;
        let count = 0;
        while (nan < 1) {
            count = count + 1;
            if (count > 3) { break; }
        }
        return count;
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

INSTANTIATE_TEST_SUITE_P(Mode, CompareTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
