#include <behl/behl.hpp>
#include <gtest/gtest.h>

namespace behl
{
    class ConstTest : public ::testing::Test
    {
    protected:
        State* S;
        void SetUp() override
        {
            S = new_state();
            load_stdlib(S, true);
        }
        void TearDown() override
        {
            close(S);
        }
    };

    TEST_F(ConstTest, BasicConstDeclaration)
    {
        constexpr std::string_view code = R"(
            const x = 42;
            return x == 42;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstAssignmentFails)
    {
        constexpr std::string_view code = R"(
            const x = 10;
            x = 20;
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstReassignmentFails)
    {
        constexpr std::string_view code = R"(
            const value = 100;
            value = 200;
            return value;
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstInFunction)
    {
        constexpr std::string_view code = R"(
            function test() {
                const local_const = 50;
                return local_const * 2;
            }
            return test() == 100;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstInFunctionReassignmentFails)
    {
        constexpr std::string_view code = R"(
            function test() {
                const local_const = 50;
                local_const = 100;
                return local_const;
            }
            return test();
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, MultipleConstDeclarations)
    {
        constexpr std::string_view code = R"(
            const a = 1;
            const b = 2;
            const c = 3;
            return a + b + c == 6;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstWithExpression)
    {
        constexpr std::string_view code = R"(
            let x = 5;
            const y = x * 2 + 3;
            return y == 13;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstWithFunctionCall)
    {
        constexpr std::string_view code = R"(
            function getValue() {
                return 42;
            }
            const result = getValue();
            return result == 42;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstTableReference)
    {
        constexpr std::string_view code = R"(
            const t = {x = 10, y = 20};
            return t["x"] == 10 && t["y"] == 20;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstTableMutationAllowed)
    {
        constexpr std::string_view code = R"(
            const t = {x = 10};
            t["x"] = 20;
            t["y"] = 30;
            return t["x"] == 20 && t["y"] == 30;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstTableReassignmentFails)
    {
        constexpr std::string_view code = R"(
            const t = {x = 10};
            t = {y = 20};
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstInLoop)
    {
        constexpr std::string_view code = R"(
            let sum = 0;
            for (let i = 0; i < 5; i++) {
                const doubled = i * 2;
                sum = sum + doubled;
            }
            return sum == 20;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstInLoopReassignmentFails)
    {
        constexpr std::string_view code = R"(
            for (let i = 0; i < 5; i++) {
                const value = i;
                value = value + 1;
            }
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstInNestedScopes)
    {
        constexpr std::string_view code = R"(
            const outer = 10;
            {
                const inner = 20;
                {
                    const deep = 30;
                    if (outer + inner + deep == 60) {
                        return true;
                    }
                }
            }
            return false;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstShadowing)
    {
        constexpr std::string_view code = R"(
            const x = 10;
            {
                const x = 20;
                if (x != 20) {
                    return false;
                }
            }
            return x == 10;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstInConditional)
    {
        constexpr std::string_view code = R"(
            if (true) {
                const branch_const = 100;
                if (branch_const == 100) {
                    return true;
                }
            }
            return false;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstInConditionalReassignmentFails)
    {
        constexpr std::string_view code = R"(
            if (true) {
                const value = 100;
                value = 200;
            }
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstWithString)
    {
        constexpr std::string_view code = R"(
            const message = "hello";
            return message == "hello";
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstStringReassignmentFails)
    {
        constexpr std::string_view code = R"(
            const message = "hello";
            message = "world";
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstWithNil)
    {
        constexpr std::string_view code = R"(
            const value = nil;
            return value == nil;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstWithBoolean)
    {
        constexpr std::string_view code = R"(
            const flag = true;
            return flag == true;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstCapturedByClosure)
    {
        constexpr std::string_view code = R"(
            const captured = 42;
            function getCaptured() {
                return captured;
            }
            return getCaptured() == 42;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstCapturedByClosureReassignmentFails)
    {
        constexpr std::string_view code = R"(
            const captured = 42;
            function modifyCaptured() {
                captured = 100;
            }
            modifyCaptured();
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstFunctionParameter)
    {
        constexpr std::string_view code = R"(
            function test(const param) {
                return param * 2;
            }
            return test(21) == 42;
        )";

        try
        {
            load_string(S, code);
            ASSERT_NO_THROW(call(S, 0, 1));
            EXPECT_TRUE(to_boolean(S, -1));
        }
        catch (...)
        {
        }
    }

    TEST_F(ConstTest, ConstInWhileLoop)
    {
        constexpr std::string_view code = R"(
            let count = 0;
            let i = 0;
            while (i < 3) {
                const iteration = i;
                count = count + iteration;
                i = i + 1;
            }
            return count == 3;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstCompoundAssignmentFails)
    {
        constexpr std::string_view code = R"(
            const x = 10;
            x = x + 5;
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstIncrementFails)
    {
        constexpr std::string_view code = R"(
            const counter = 0;
            counter++;
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstDecrementFails)
    {
        constexpr std::string_view code = R"(
            const value = 10;
            value--;
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstArray)
    {
        constexpr std::string_view code = R"(
            const arr = {1, 2, 3, 4, 5};
            return arr[0] == 1 && arr[4] == 5;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstArrayMutationAllowed)
    {
        constexpr std::string_view code = R"(
            const arr = {1, 2, 3};
            arr[0] = 10;
            arr[3] = 40;
            return arr[0] == 10 && arr[3] == 40;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstArrayReassignmentFails)
    {
        constexpr std::string_view code = R"(
            const arr = {1, 2, 3};
            arr = {4, 5, 6};
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstNestedTables)
    {
        constexpr std::string_view code = R"(
            const data = {
                inner = {
                    value = 42
                }
            };
            return data["inner"]["value"] == 42;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstNestedTableMutationAllowed)
    {
        constexpr std::string_view code = R"(
            const data = {
                inner = {
                    value = 42
                }
            };
            data["inner"]["value"] = 100;
            return data["inner"]["value"] == 100;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstInForLoop)
    {
        constexpr std::string_view code = R"(
            let result = 0;
            for (const i = 0; i < 5; i++) {
                result = result + i;
            }
            return result == 10;
        )";

        try
        {
            load_string(S, code);
            EXPECT_ANY_THROW(call(S, 0, 1));
        }
        catch (...)
        {
        }
    }

    TEST_F(ConstTest, ConstGlobalVariable)
    {
        constexpr std::string_view code = R"(
            const GLOBAL_CONST = 1000;
            
            function useGlobal() {
                return GLOBAL_CONST;
            }
            
            return useGlobal() == 1000;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstGlobalReassignmentFails)
    {
        constexpr std::string_view code = R"(
            const GLOBAL_CONST = 1000;
            GLOBAL_CONST = 2000;
        )";

        EXPECT_ANY_THROW(load_string(S, code));
    }

    TEST_F(ConstTest, ConstWithComplexExpression)
    {
        constexpr std::string_view code = R"(
            let a = 10;
            let b = 20;
            const result = (a + b) * 2 - 5;
            return result == 55;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstInRecursiveFunction)
    {
        constexpr std::string_view code = R"(
            function factorial(n) {
                const base_case = 1;
                if (n <= base_case) {
                    return 1;
                }
                return n * factorial(n - 1);
            }
            return factorial(5) == 120;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, MultipleConstInSameScope)
    {
        constexpr std::string_view code = R"(
            const a = 1;
            const b = 2;
            const c = a + b;
            const d = c * 2;
            return d == 6;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstAfterLetInSameScope)
    {
        constexpr std::string_view code = R"(
            let x = 10;
            const y = 20;
            x = x + y;
            return x == 30 && y == 20;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, LetAfterConstInSameScope)
    {
        constexpr std::string_view code = R"(
            const x = 10;
            let y = 20;
            y = y + x;
            return x == 10 && y == 30;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstTableWithMethods)
    {
        constexpr std::string_view code = R"(
            const obj = {
                value = 42,
                getValue = function() {
                    return this["value"];
                }
            };
            return obj["value"] == 42;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstWithTernaryLikeExpression)
    {
        constexpr std::string_view code = R"(
            let condition = true;
            const result = condition && 100 || 200;
            return result == 100;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

    TEST_F(ConstTest, ConstMixedWithGlobals)
    {
        constexpr std::string_view code = R"(
            global_var = 50;
            const local_const = 100;
            global_var = global_var + local_const;
            return global_var == 150 && local_const == 100;
        )";

        ASSERT_NO_THROW(load_string(S, code));
        ASSERT_NO_THROW(call(S, 0, 1));
        EXPECT_TRUE(to_boolean(S, -1));
    }

} // namespace behl
