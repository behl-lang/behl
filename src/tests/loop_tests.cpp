#include "state.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class LoopTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

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
};

TEST_P(LoopTest, WhileLoopBasic)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 1
        while (i <= 10) {
            sum = sum + i
            i = i + 1
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 55);
}

TEST_P(LoopTest, ForLoopBasic)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 1; i <= 10; i = i + 1) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 55);
}

TEST_P(LoopTest, ForLoopTwoCallsInBody_Discarded)
{
    constexpr std::string_view code = R"(
        function f(a) { return a }
        let n = 0
        for (let i = 0; i < 3; i++) {
            f(1)
            f(2)
            n = n + 1
        }
        return n
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(LoopTest, ForLoopTwoCallsInBody_Assigned)
{
    constexpr std::string_view code = R"(
        function f(a, b) { return a + b }
        let x = 0
        let y = 0
        let n = 0
        for (let i = 0; i < 3; i++) {
            x = f(1, 2)
            y = f(9, 4)
            n = n + 1
        }
        return x, y, n
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 3));
    ASSERT_EQ(behl::to_integer(S, -3), 3);
    ASSERT_EQ(behl::to_integer(S, -2), 13);
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(LoopTest, ForLoopManyCallsInBody_IndexSurvives)
{
    constexpr std::string_view code = R"(
        function f(a) { return a }
        let seen = 0
        for (let i = 0; i < 4; i++) {
            f(1)
            f(2)
            f(3)
            seen = seen + i
        }
        return seen
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(LoopTest, NestedForLoopsWithCallsInBody)
{
    constexpr std::string_view code = R"(
        function f(a) { return a }
        let total = 0
        for (let i = 0; i < 3; i++) {
            for (let j = 0; j < 3; j++) {
                f(1)
                f(2)
                total = total + 1
            }
        }
        return total
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 9);
}

TEST_P(LoopTest, ForEachLoopTwoCallsInBody)
{
    constexpr std::string_view code = R"(
        function f(a) { return a }
        let t = {10, 20, 30}
        let total = 0
        foreach (let k, v in t) {
            f(1)
            f(2)
            total = total + v
        }
        return total
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_P(LoopTest, NestedLoops)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 1; i <= 5; i = i + 1) {
            for (let j = 1; j <= 3; j = j + 1) {
                sum = sum + 1
            }
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(LoopTest, ForLoopWithContinue)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let iterations = 0
        for (let i = 0; i < 10; i = i + 1) {
            iterations = iterations + 1
            if (i % 2 == 0) {
                continue
            }
            sum = sum + i
        }
        return sum, iterations
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::to_integer(S, -2), 25);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(LoopTest, ForLoopWithBreak)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let iterations = 0
        for (let i = 0; i < 100; i = i + 1) {
            iterations = iterations + 1
            if (i >= 5) {
                break
            }
            sum = sum + i
        }
        return sum, iterations
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::to_integer(S, -2), 10);
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(LoopTest, ForLoopWithBreakAndContinue)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let iterations = 0
        for (let i = 0; i < 20; i = i + 1) {
            iterations = iterations + 1
            if (i > 10) {
                break
            }
            if (i % 2 == 0) {
                continue
            }
            sum = sum + i
        }
        return sum, iterations
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::to_integer(S, -2), 25);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(LoopTest, ForEachLoopWithLet)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let sum = 0
        foreach (let v in tab) {
            sum = sum + v
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_P(LoopTest, ForEachLoopWithExistingVariable)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let sum = 0
        let v
        foreach (v in tab) {
            sum = sum + v
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_P(LoopTest, ForEachLoopTwoVariablesWithLet)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let sum_keys = 0
        let sum_values = 0
        foreach (let k, v in tab) {
            sum_keys = sum_keys + k
            sum_values = sum_values + v
        }
        return sum_keys * 1000 + sum_values
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 3060);
}

TEST_P(LoopTest, ForEachLoopTwoVariablesExisting)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let sum_keys = 0
        let sum_values = 0
        let k
        let v
        foreach (k, v in tab) {
            sum_keys = sum_keys + k
            sum_values = sum_values + v
        }
        return sum_keys * 1000 + sum_values
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 3060);
}

TEST_P(LoopTest, ForEachLoopZeroIndexed)
{
    constexpr std::string_view code = R"(
        let tab = {100, 200, 300}
        let first_key = -1
        let first_value = -1
        foreach (let k, v in tab) {
            first_key = k
            first_value = v
            return first_key * 1000 + first_value
        }
        return 0
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 100);
}

TEST_P(LoopTest, ForInLoopWithExplicitPairs)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let sum = 0
        for (let v in pairs(tab)) {
            sum = sum + v
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 60);
}

TEST_P(LoopTest, ForInLoopWithTwoVariablesPairs)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let sum_keys = 0
        let sum_values = 0
        for (let k, v in pairs(tab)) {
            sum_keys = sum_keys + k
            sum_values = sum_values + v
        }
        return sum_keys * 1000 + sum_values
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 3060);
}

TEST_P(LoopTest, CustomIteratorReverseIteration)
{
    constexpr std::string_view code = R"(
        function reverse_pairs(t) {
            function iter(state, key) {
                if (typeof(key) == "nil") {
                    let len = rawlen(state)
                    if (len > 0) {
                        let first_key = len - 1
                        return first_key, state[first_key]
                    }
                    return nil
                }
                let next_key = key - 1
                if (next_key < 0) {
                    return nil
                }
                return next_key, state[next_key]
            }
            return iter, t, nil
        }
        
        let tab = {10, 20, 30}
        let result = 0
        for (let k, v in reverse_pairs(tab)) {
            result = result * 1000 + v
        }
        return result
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 30020010);
}

TEST_P(LoopTest, ForEachLoopEmptyTable)
{
    constexpr std::string_view code = R"(
        let tab = {}
        let count = 0
        foreach (let v in tab) {
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(LoopTest, NestedForEachLoops)
{
    constexpr std::string_view code = R"(
        let tab1 = {1, 2}
        let tab2 = {10, 20}
        let sum = 0
        foreach (let v1 in tab1) {
            foreach (let v2 in tab2) {
                sum = sum + v1 * v2
            }
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 90);
}

TEST_P(LoopTest, ForInLoopWithPreDeclaredVariables)
{
    constexpr std::string_view code = R"(
        let tab = {10, 20, 30}
        let k
        let v
        let sum_keys = 0
        let sum_values = 0
        for (k, v in pairs(tab)) {
            sum_keys = sum_keys + k
            sum_values = sum_values + v
        }
        return sum_keys * 1000 + sum_values
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 3060);
}

TEST_P(LoopTest, ForEachMixedArrayAndHashParts)
{
    constexpr std::string_view code = R"(
        let t = {10, 20, 30}
        t["foo"] = "bar"
        t["x"] = 100
        t[100] = "sparse"  // Sparse integer key goes to hash
        
        let count = 0
        let found_array = 0
        let found_hash = 0
        
        foreach (let k, v in t) {
            count++
            if (typeof(k) == "integer" && k >= 0 && k < 3) {
                found_array++
            } else {
                found_hash++
            }
        }
        
        return count * 1000 + found_array * 100 + found_hash
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 6303);
}

TEST_P(LoopTest, OptimizedForLoopIncrement)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 10; i++) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 45);
}

TEST_P(LoopTest, OptimizedForLoopDecrement)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 10; i > 0; i--) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 55);
}

TEST_P(LoopTest, OptimizedForLoopWithStep)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 20; i += 3) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 63);
}

TEST_P(LoopTest, OptimizedForLoopInclusiveLessOrEqual)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 1; i <= 5; i++) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(LoopTest, OptimizedForLoopInclusiveGreaterOrEqual)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 5; i >= 1; i--) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 15);
}

TEST_P(LoopTest, ForLoopWithComplexCondition)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let n = 10
        for (let i = 0; i < n * 2 && i < 15; i++) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 105);
}

TEST_P(LoopTest, ForLoopWithFunctionCallInCondition)
{
    constexpr std::string_view code = R"(
        function getLimit() {
            return 5
        }
        let sum = 0
        for (let i = 0; i < getLimit(); i++) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(LoopTest, ForLoopWithTableAccessInCondition)
{
    constexpr std::string_view code = R"(
        let config = { limit = 8 }
        let sum = 0
        for (let i = 0; i < config["limit"]; i++) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 28); // 0+1+...+7
}

TEST_P(LoopTest, ForLoopWithArithmeticInCondition)
{
    constexpr std::string_view code = R"(
        let base = 5
        let sum = 0
        for (let i = 0; i < base + 5; i++) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 45);
}

TEST_P(LoopTest, ForLoopWithLogicalOrCondition)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let count = 0
        for (let i = 0; i < 20 || i < 5; i++) {
            if (i >= 10) {
                break
            }
            sum = sum + i
            count++
        }
        return sum, count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::to_integer(S, -2), 45);
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(LoopTest, ForLoopWithBooleanCondition)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let cont = true
        let i = 0
        while (cont) {
            sum = sum + i
            i = i + 1
            if (i >= 5) {
                cont = false
            }
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(LoopTest, ForLoopWithComplexUpdate)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 100; i = i * 2 + 1) {
            sum = sum + i
            if (i > 10) {
                break
            }
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 26);
}

TEST_P(LoopTest, ForLoopWithMultipleUpdates)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let j = 10
        for (let i = 0; i < 5; i++) {
            sum = sum + i + j
            j--
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 50);
}

TEST_P(LoopTest, ForLoopVariableBounds)
{
    constexpr std::string_view code = R"(
        let start = 5
        let end = 15
        let sum = 0
        for (let i = start; i < end; i++) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 95);
}

TEST_P(LoopTest, ForLoopWithFloatBounds)
{
    constexpr std::string_view code = R"(
        let sum = 0.0
        for (let i = 0.5; i < 5.5; i = i + 1.0) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 12.5);
}

TEST_P(LoopTest, ForLoopWithExistingVariable)
{
    constexpr std::string_view code = R"(
        let sum = 0
        let i = 999
        for (i = 0; i < 5; i++) {
            sum = sum + i
        }
        return sum, i
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 2));
    ASSERT_EQ(behl::to_integer(S, -2), 10);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(LoopTest, ForLoopExistingVariableComplex)
{
    constexpr std::string_view code = R"(
        let results = {}
        let k = 0
        for (k = 10; k > 5; k = k - 2) {
            results[k] = k * 2
        }
        return k, results[10], results[8], results[6]
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 4));
    ASSERT_EQ(behl::to_integer(S, -4), 4);
    ASSERT_EQ(behl::to_integer(S, -3), 20);
    ASSERT_EQ(behl::to_integer(S, -2), 16);
    ASSERT_EQ(behl::to_integer(S, -1), 12);
}

TEST_P(LoopTest, ForLoopExistingVariableNestedScope)
{
    constexpr std::string_view code = R"(
        let outer = 0
        {
            let i = 100
            for (i = 0; i < 3; i++) {
                outer = outer + i
            }
            outer = outer + i
        }
        return outer
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 6);
}

TEST_P(LoopTest, ForLoopMultipleVariableDeclarations)
{
    constexpr std::string_view code = R"(
        let result = 0
        for (let i = 0, j = 10; i < j; i++, j--) {
            result = result + i + j
        }
        return result
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 50);
}

TEST_P(LoopTest, ForLoopMultipleVariableDeclarationsComplex)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 1, j = 100, k = 2; i < 10; i++, j -= 10, k *= 2) {
            sum = sum + i + j + k
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 1607);
}

TEST_P(LoopTest, ForLoopZeroIterationsAscending)
{
    constexpr std::string_view code = R"(
        let count = 0
        for (let i = 5; i < 5; i++) {
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(LoopTest, ForLoopZeroIterationsDescending)
{
    constexpr std::string_view code = R"(
        let count = 0
        for (let i = 0; i > 5; i--) {
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 0);
}

TEST_P(LoopTest, ForLoopStepOvershootsLimit)
{
    constexpr std::string_view code = R"(
        let count = 0
        for (let i = 0; i < 10; i += 100) {
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 1);
}

TEST_P(LoopTest, ForLoopDescendingNonUnitStep)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 10; i > 0; i -= 3) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    // 10 + 7 + 4 + 1
    ASSERT_EQ(behl::to_integer(S, -1), 22);
}

TEST_P(LoopTest, ForLoopBodyWritesLoopVariable)
{
    // Writing the loop variable in the body keeps C semantics: the loop must
    // not be rewritten into the counted numeric form
    constexpr std::string_view code = R"(
        let count = 0
        for (let i = 0; i < 10; i++) {
            i = i + 1
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    // i advances by 2 per iteration: 0, 2, 4, 6, 8
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(LoopTest, ForLoopBodyShadowsLoopVariable)
{
    constexpr std::string_view code = R"(
        let sum = 0
        for (let i = 0; i < 3; i++) {
            let i = 100
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 300);
}

TEST_P(LoopTest, ForLoopClosureWritesLoopVariable)
{
    // A closure inside the body writing the loop variable must also keep the
    // loop on the generic C-semantics path
    constexpr std::string_view code = R"(
        let count = 0
        for (let i = 0; i < 10; i++) {
            let bump = function() {
                i = i + 1
            }
            bump()
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    // i advances by 2 per iteration: 0, 2, 4, 6, 8
    ASSERT_EQ(behl::to_integer(S, -1), 5);
}

TEST_P(LoopTest, ForLoopNearIntegerMaxInclusive)
{
    // The index reaching the maximum integer must terminate the loop instead
    // of wrapping around and looping forever
    constexpr std::string_view code = R"(
        let count = 0
        for (let i = 9223372036854775798; i <= 9223372036854775807; i++) {
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(LoopTest, ForLoopNearIntegerMaxOvershootStep)
{
    // The final step overshooting the maximum integer must terminate the loop
    constexpr std::string_view code = R"(
        let count = 0
        for (let i = 9223372036854775797; i < 9223372036854775807; i += 4) {
            count = count + 1
        }
        return count
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    // 9223372036854775797, +4, +4; the next step would pass the limit
    ASSERT_EQ(behl::to_integer(S, -1), 3);
}

TEST_P(LoopTest, ForLoopIntBoundsFloatStep)
{
    constexpr std::string_view code = R"(
        let sum = 0.0
        for (let i = 0; i < 5; i += 1.0) {
            sum = sum + i
        }
        return sum
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 10.0);
}

INSTANTIATE_TEST_SUITE_P(
    Mode, LoopTest, ::testing::Bool(), [](const ::testing::TestParamInfo<bool>& info) { return info.param ? "jit" : "nojit"; });
