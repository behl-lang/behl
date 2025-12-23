#include <behl/behl.hpp>
#include <gtest/gtest.h>

class VarargsTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        behl::load_lib_core(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(VarargsTest, PureVarargsFunction)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return #args;
        }
        return test(1, 2, 3);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 3);
}

TEST_F(VarargsTest, VarargsWithValues)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return args[0] + args[1] + args[2];
        }
        return test(10, 20, 30);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 60);
}

TEST_F(VarargsTest, MixedParamsAndVarargs)
{
    constexpr std::string_view code = R"(
        function test(a, b, ...) {
            let rest = {...};
            return a + b + #rest;
        }
        return test(1, 2, 3, 4, 5);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 6); // 1 + 2 + 3 (count of varargs)
}

TEST_F(VarargsTest, MixedParamsAndVarargsValues)
{
    constexpr std::string_view code = R"(
        function test(a, b, ...) {
            let rest = {...};
            return a + b + rest[0] + rest[1] + rest[2];
        }
        return test(1, 2, 10, 20, 30);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 63); // 1 + 2 + 10 + 20 + 30
}

TEST_F(VarargsTest, NoVarargsPassed)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return #args;
        }
        return test();
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(VarargsTest, NoVarargsPassedWithParams)
{
    constexpr std::string_view code = R"(
        function test(a, b, ...) {
            let rest = {...};
            return a + b + #rest;
        }
        return test(10, 20);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 30); // 10 + 20 + 0
}

TEST_F(VarargsTest, VarargsMixedTypes)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return typeof(args[0]) + "," + typeof(args[1]) + "," + typeof(args[2]);
        }
        return test(42, "hello", true);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "integer,string,boolean");
}

TEST_F(VarargsTest, MultipleVarargCalls)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args1 = {...};
            let args2 = {...};
            return #args1 + #args2;
        }
        return test(1, 2, 3);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 6); // 3 + 3
}

TEST_F(VarargsTest, VarargsInNestedFunction)
{
    constexpr std::string_view code = R"(
        function outer(...) {
            let outer_args = {...};
            function inner() {
                return outer_args[0] + outer_args[1];
            }
            return inner();
        }
        return outer(10, 20);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 30);
}

TEST_F(VarargsTest, VarargsForwarding)
{
    constexpr std::string_view code = R"(
        function inner(...) {
            let args = {...};
            return #args;
        }
        function outer(...) {
            let args = {...};
            return inner(args[0], args[1], args[2]);
        }
        return outer(1, 2, 3);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 3);
}

TEST_F(VarargsTest, VarargsWithStringValues)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return args[0] + args[1] + args[2];
        }
        return test("Hello", " ", "World");
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "Hello World");
}

TEST_F(VarargsTest, VarargsTableIteration)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            let sum = 0;
            for(let i = 0; i < #args; i++) {
                sum += args[i];
            }
            return sum;
        }
        return test(1, 2, 3, 4, 5);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 15);
}

TEST_F(VarargsTest, VarargsWithSingleElement)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return args[0];
        }
        return test(42);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(VarargsTest, VarargsManyArguments)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return #args;
        }
        return test(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 15);
}

TEST_F(VarargsTest, VarargsWithNilValues)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let args = {...};
            return typeof(args[0]) + "," + typeof(args[1]) + "," + typeof(args[2]);
        }
        return test(nil, 42, nil);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "nil,integer,nil");
}

TEST_F(VarargsTest, VarargsDirectForwarding)
{
    constexpr std::string_view code = R"(
        function inner(...) {
            let args = {...};
            return #args;
        }
        function outer(...) {
            return inner(...);
        }
        return outer(1, 2, 3, 4);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 4);
}

TEST_F(VarargsTest, VarargsDirectForwardingWithPrefix)
{
    constexpr std::string_view code = R"(
        function inner(a, b, ...) {
            let rest = {...};
            return a + b + #rest;
        }
        function outer(...) {
            return inner(100, 200, ...);
        }
        return outer(1, 2, 3);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 303); // 100 + 200 + 3
}
