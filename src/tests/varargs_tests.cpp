#include "state.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <gtest/gtest.h>

class VarargsTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        behl::load_lib_core(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_P(VarargsTest, PureVarargsFunction)
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

TEST_P(VarargsTest, VarargsWithValues)
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

TEST_P(VarargsTest, MixedParamsAndVarargs)
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

TEST_P(VarargsTest, MixedParamsAndVarargsValues)
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

TEST_P(VarargsTest, NoVarargsPassed)
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

TEST_P(VarargsTest, NoVarargsPassedWithParams)
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

TEST_P(VarargsTest, VarargsMixedTypes)
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

TEST_P(VarargsTest, MultipleVarargCalls)
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

TEST_P(VarargsTest, VarargsInNestedFunction)
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

TEST_P(VarargsTest, VarargsForwarding)
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

TEST_P(VarargsTest, VarargsWithStringValues)
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

TEST_P(VarargsTest, VarargsTableIteration)
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

TEST_P(VarargsTest, VarargsWithSingleElement)
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

TEST_P(VarargsTest, VarargsManyArguments)
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

TEST_P(VarargsTest, VarargsWithNilValues)
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

TEST_P(VarargsTest, VarargsDirectForwarding)
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

TEST_P(VarargsTest, VarargsDirectForwardingWithPrefix)
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

TEST_P(VarargsTest, MultiAssignFromVarargsExact)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let a, b, c = ...;
            return a + b + c;
        }
        return test(10, 20, 30);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 60);
}

TEST_P(VarargsTest, MultiAssignFromVarargsNilPadded)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let a, b, c = ...;
            return typeof(a) + "," + typeof(b) + "," + typeof(c);
        }
        return test(10);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "integer,nil,nil");
}

TEST_P(VarargsTest, MultiAssignFromVarargsTruncated)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let a, b = ...;
            return a + b;
        }
        return test(10, 20, 30);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 30);
}

TEST_P(VarargsTest, MultiAssignWithParamsAndVarargs)
{
    constexpr std::string_view code = R"(
        function test(p, ...) {
            let a, b = ...;
            return p + a + b;
        }
        return test(1, 10, 20);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 31);
}

TEST_P(VarargsTest, SingleAssignFromVarargs)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let x = ...;
            return x;
        }
        return test(42, 99);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_P(VarargsTest, SingleAssignFromVarargsNoneIsNil)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let x = ...;
            return typeof(x);
        }
        return test();
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "nil");
}

TEST_P(VarargsTest, ReassignFromVarargs)
{
    constexpr std::string_view code = R"(
        function test(...) {
            let a = 0;
            let b = 0;
            a, b = ...;
            return a + b;
        }
        return test(5, 7);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(VarargsTest, ReturnVarargsAll)
{
    constexpr std::string_view code = R"(
        function inner(...) {
            return ...;
        }
        function outer(...) {
            let a, b, c = inner(...);
            return a + b + c;
        }
        return outer(10, 20, 30);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 60);
}

TEST_P(VarargsTest, ReturnPrefixThenVarargs)
{
    constexpr std::string_view code = R"(
        function inner(...) {
            return 1, ...;
        }
        function outer(...) {
            let a, b, c = inner(...);
            return a + b + c;
        }
        return outer(10, 20);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 31); // 1 + 10 + 20
}

TEST_P(VarargsTest, ReturnPrefixThenVarargsEmpty)
{
    constexpr std::string_view code = R"(
        function inner(...) {
            return 1, ...;
        }
        function outer(...) {
            let a, b = inner(...);
            return typeof(a) + "," + typeof(b);
        }
        return outer();
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "integer,nil");
}

TEST_P(VarargsTest, ReturnVarargsTruncatedWhenNotLast)
{
    constexpr std::string_view code = R"(
        function inner(...) {
            return ..., 99;
        }
        function outer(...) {
            let a, b = inner(...);
            return a + b;
        }
        return outer(10, 20);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 109); // 10 (... truncated to first) + 99
}

TEST_P(VarargsTest, VarargInNonVarargFunctionIsRejected)
{
    constexpr std::string_view code = R"(
        function plain() {
            let args = {...};
            return #args;
        }
        return plain();
    )";
    EXPECT_THROW(behl::load_string(S, code), behl::BehlException);
}

TEST_P(VarargsTest, VarargInNonVarargFunctionWithParamsIsRejected)
{
    constexpr std::string_view code = R"(
        function plain(a, b) {
            return (...);
        }
        return plain(1, 2);
    )";
    EXPECT_THROW(behl::load_string(S, code), behl::BehlException);
}

TEST_P(VarargsTest, VarargInNestedNonVarargFunctionIsRejected)
{
    constexpr std::string_view code = R"(
        function outer(...) {
            function inner() {
                let args = {...};
                return #args;
            }
            return inner();
        }
        return outer(1, 2, 3);
    )";
    EXPECT_THROW(behl::load_string(S, code), behl::BehlException);
}

TEST_P(VarargsTest, VarargAtTopLevelIsEmptyWithoutArguments)
{
    constexpr std::string_view code = R"(
        let args = {...};
        return #args;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(VarargsTest, MainChunkReceivesArgumentsAsVarargs)
{
    constexpr std::string_view code = R"(
        let args = {...};
        return #args;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    behl::push_string(S, "alpha");
    behl::push_string(S, "beta");
    behl::push_string(S, "gamma");
    ASSERT_NO_THROW(behl::call(S, 3, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(VarargsTest, MainChunkArgumentValuesAreReadable)
{
    constexpr std::string_view code = R"(
        let a, b = ...;
        return a + "/" + b;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    behl::push_string(S, "first");
    behl::push_string(S, "second");
    ASSERT_NO_THROW(behl::call(S, 2, 1));
    EXPECT_EQ(behl::to_string(S, -1), "first/second");
}

TEST_P(VarargsTest, VarargAsCallArgumentInNonVarargFunctionIsRejected)
{
    constexpr std::string_view code = R"(
        function sink(a) {
            return a;
        }
        function plain() {
            return sink(...);
        }
        return plain();
    )";
    EXPECT_THROW(behl::load_string(S, code), behl::BehlException);
}

TEST_P(VarargsTest, VarargInNestedVarargFunctionIsAccepted)
{
    constexpr std::string_view code = R"(
        function outer(...) {
            function inner(...) {
                let args = {...};
                return #args;
            }
            return inner(7, 8);
        }
        return outer(1, 2, 3);
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 2);
}

TEST_P(VarargsTest, NonVarargCallAfterVarargCallIsNotCorrupted)
{
    constexpr std::string_view code = R"(
        function eats(a, ...) {
            let args = {...};
            return #args;
        }
        function plain() {
            return 0;
        }
        let first = eats(1, 2, 3, 4, 5, 6);
        let second = plain();
        return first + second;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 5);
}

INSTANTIATE_TEST_SUITE_P(Mode, VarargsTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
