#include <behl/behl.hpp>
#include <gtest/gtest.h>

class RecursionTest : public ::testing::Test
{
protected:
    behl::State* S;

    void SetUp() override
    {
        S = behl::new_state();
        ASSERT_NE(S, nullptr);
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }
};

TEST_F(RecursionTest, BasicFunctionCall)
{
    constexpr std::string_view code = R"(
        function test() {
            return 42
        }
        return test()
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(RecursionTest, FactorialRecursion)
{
    constexpr std::string_view code = R"(
        function fact(n) {
            if (n <= 1) {
                return 1
            }
            let x = n - 1
            let result = fact(x)
            return n * result
        }
        return fact(5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 120);
}

TEST_F(RecursionTest, FibonacciRecursion)
{
    constexpr std::string_view code = R"(
        function fib(n) {
            if (n <= 1) {
                return n
            }
            let x = n - 1
            let y = n - 2
            let a = fib(x)
            let b = fib(y)
            return a + b
        }
        return fib(10)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 55);
}

TEST_F(RecursionTest, TailRecursiveCountdown)
{
    constexpr std::string_view code = R"(
        function countdown(n) {
            if (n <= 0) {
                return 42
            }
            let next_n = n - 1
            return countdown(next_n)
        }
        return countdown(100)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(RecursionTest, TailCallMultipleParameters)
{
    constexpr std::string_view code = R"(
        function test(a, b) {
            if (a == 2) {
                return b
            }
            return test(a + 1, b + 10)
        }
        return test(0, 0)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 20);
}

TEST_F(RecursionTest, TailCallThreeParameters)
{
    constexpr std::string_view code = R"(
        function sum3(a, b, c) {
            if (a == 0) {
                return b + c
            }
            return sum3(a - 1, b + 1, c + 2)
        }
        return sum3(5, 0, 0)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_F(RecursionTest, TailCallComplexExpressions)
{
    constexpr std::string_view code = R"(
        function compute(x, y, z) {
            if (x >= 10) {
                return y * z
            }
            return compute(x + 1, y + x * 2, z + 1)
        }
        return compute(0, 1, 1)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    ASSERT_EQ(behl::to_integer(S, -1), 1001);
}

TEST_F(RecursionTest, TailCallDeepRecursion)
{
    constexpr std::string_view code = R"(
        function deep(n, acc) {
            if (n == 0) {
                return acc
            }
            return deep(n - 1, acc + 1)
        }
        return deep(1000, 0)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::to_integer(S, -1), 1000);
}

TEST_F(RecursionTest, TailCallToNativeFunction)
{
    const char* check_code = R"(
        return print
    )";
    ASSERT_NO_THROW(behl::load_string(S, check_code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kCFunction);
    behl::pop(S, 1);

    constexpr std::string_view code = R"(
        function wrapper(n) {
            if (n <= 0) {
                return print("Done")
            }
            return wrapper(n - 1)
        }
        return wrapper(3)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
}

TEST_F(RecursionTest, TailCallToNativeFunctionWithReturn)
{
    constexpr std::string_view code = R"(
        function get_type(n) {
            if (n <= 0) {
                return typeof(42)
            }
            return get_type(n - 1)
        }
        return get_type(5)
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::get_top(S), 1);

    const auto result = behl::to_string(S, -1);
    ASSERT_EQ(result, "integer");
}

TEST_F(RecursionTest, ComprehensiveHandlerCoverageDeep)
{
    constexpr std::string_view code = R"(
        function test_all_ops(depth) {
            if (depth <= 0) {
                return 1;
            }
            
            let a = 42;
            let b = 3.14;
            let c = true;
            let d = nil;
            let e = "test";
            
            let t = {x = 1, y = 2};
            let val = t.x;
            t.z = 3;
            
            let copy = a;
            
            let sum = a + val;
            let diff = a - val;
            let prod = a * val;
            let quot = a / 2;
            let rem = a % 5;
            let pow = 2 ** 3;
            
            let band = 15 & 7;
            let bor = 8 | 4;
            let bxor = 12 ^ 5;
            let shl = 1 << 3;
            let shr = 16 >> 2;
            let bnot = ~5;
            
            let neg = -a;
            let len = #t;
            
            let eq = (a == 42);
            let lt = (a < 50);
            let le = (a <= 42);
            
            a++;
            a--;
            
            t[0] = 100;
            let idx_val = t[0];
            
            let add_const = a + 10;
            let sub_const = a - 5;
            
            let rec = test_all_ops(depth - 1);
            
            return rec + sum + diff + prod + band + bor;
        }
        
        let result50 = test_all_ops(50);
        let result100 = test_all_ops(100);
        let result200 = test_all_ops(200);
        
        return result50 == 7251 && result100 == 14501 && result200 == 29001;
    )";
    ASSERT_NO_THROW(load_string(S, code));
    ASSERT_NO_THROW(call(S, 0, 1));
    EXPECT_TRUE(to_boolean(S, -1));
}
