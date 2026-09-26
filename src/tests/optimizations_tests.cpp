#include "gc/gc_object.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "state.hpp"
#include "test_helpers.hpp"
#include "vm/bytecode.hpp"
#include "vm/value.hpp"

#include <array>
#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <string>

class OptimizationsTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
    }

    void TearDown() override
    {
        behl::close(S);
    }

    behl::GCProto* get_proto_from_stack()
    {
        EXPECT_EQ(S->stack.size(), 1);
        behl::Value& val = S->stack[0];
        EXPECT_TRUE(val.is_closure());
        auto* closure = val.get_closure();
        EXPECT_NE(closure, nullptr);
        return closure->proto;
    }
};

TEST_P(OptimizationsTest, NumericForLoopOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i < 10; i++) {
        }
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    auto* proto = get_proto_from_stack();
    ASSERT_NE(proto, nullptr);

    bool has_forprep = false;
    bool has_forloop = false;

    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        auto opcode = proto->code[i].op();
        if (opcode == behl::OpCode::kOpForPrep)
        {
            has_forprep = true;
        }
        if (opcode == behl::OpCode::kOpForLoop)
        {
            has_forloop = true;
        }
    }

    EXPECT_TRUE(has_forprep) << "Optimized for loop should have FORPREP";
    EXPECT_TRUE(has_forloop) << "Optimized for loop should have FORLOOP";
}

TEST_P(OptimizationsTest, ComplexConditionNotOptimized)
{
    constexpr std::string_view code = R"(
        function check(x) {
            return x < 10
        }
        for (let i = 0; check(i); i++) {
        }
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    auto* proto = get_proto_from_stack();
    ASSERT_NE(proto, nullptr);

    bool has_forprep = false;
    bool has_forloop = false;

    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        auto opcode = proto->code[i].op();
        if (opcode == behl::OpCode::kOpForPrep)
        {
            has_forprep = true;
        }
        if (opcode == behl::OpCode::kOpForLoop)
        {
            has_forloop = true;
        }
    }

    EXPECT_FALSE(has_forprep) << "Complex condition for loop should not have FORPREP";
    EXPECT_FALSE(has_forloop) << "Complex condition for loop should not have FORLOOP";
}

TEST_P(OptimizationsTest, DecrementingForLoopOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 10; i > 0; i--) {
        }
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    auto* proto = get_proto_from_stack();
    ASSERT_NE(proto, nullptr);

    bool has_forprep = false;
    bool has_forloop = false;

    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        auto opcode = proto->code[i].op();
        if (opcode == behl::OpCode::kOpForPrep)
        {
            has_forprep = true;
        }
        if (opcode == behl::OpCode::kOpForLoop)
        {
            has_forloop = true;
        }
    }

    EXPECT_TRUE(has_forprep);
    EXPECT_TRUE(has_forloop);
}

TEST_P(OptimizationsTest, ForLoopWithStepOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i < 100; i += 5) {
        }
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    auto* proto = get_proto_from_stack();
    ASSERT_NE(proto, nullptr);

    bool has_forprep = false;
    bool has_forloop = false;

    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        auto opcode = proto->code[i].op();
        if (opcode == behl::OpCode::kOpForPrep)
        {
            has_forprep = true;
        }
        if (opcode == behl::OpCode::kOpForLoop)
        {
            has_forloop = true;
        }
    }

    EXPECT_TRUE(has_forprep);
    EXPECT_TRUE(has_forloop);
}

TEST_P(OptimizationsTest, ConstLoopVariableNotOptimized)
{
    constexpr std::string_view code = R"(
        for (const i = 0; i < 10; i++) {
        }
    )";

    EXPECT_TRUE(behl_test::load_fails(S, code));
}

TEST_P(OptimizationsTest, ForLoopWithoutLetNotOptimized)
{
    constexpr std::string_view code = R"(
        let i = 999
        for (i = 0; i < 10; i++) {
        }
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    auto* proto = get_proto_from_stack();
    ASSERT_NE(proto, nullptr);

    bool has_forprep = false;
    bool has_forloop = false;

    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        auto opcode = proto->code[i].op();
        if (opcode == behl::OpCode::kOpForPrep)
        {
            has_forprep = true;
        }
        if (opcode == behl::OpCode::kOpForLoop)
        {
            has_forloop = true;
        }
    }

    EXPECT_FALSE(has_forprep) << "For loop without let should not have FORPREP";
    EXPECT_FALSE(has_forloop) << "For loop without let should not have FORLOOP";
}

TEST_P(OptimizationsTest, InclusiveLoopOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i <= 10; i++) {
        }
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    auto* proto = get_proto_from_stack();
    ASSERT_NE(proto, nullptr);

    bool has_forprep = false;
    bool has_forloop = false;

    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        auto opcode = proto->code[i].op();
        if (opcode == behl::OpCode::kOpForPrep)
        {
            has_forprep = true;
        }
        if (opcode == behl::OpCode::kOpForLoop)
        {
            has_forloop = true;
        }
    }

    EXPECT_TRUE(has_forprep);
    EXPECT_TRUE(has_forloop);
}

TEST_P(OptimizationsTest, MismatchedDirectionNotOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i < 10; i--) {
        }
    )";

    ASSERT_TRUE(behl_test::load_ok(S, code));

    auto* proto = get_proto_from_stack();
    ASSERT_NE(proto, nullptr);

    bool has_forprep = false;
    bool has_forloop = false;

    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        auto opcode = proto->code[i].op();
        if (opcode == behl::OpCode::kOpForPrep)
        {
            has_forprep = true;
        }
        if (opcode == behl::OpCode::kOpForLoop)
        {
            has_forloop = true;
        }
    }

    EXPECT_FALSE(has_forprep);
    EXPECT_FALSE(has_forloop);
}

TEST_P(OptimizationsTest, IndexByMultipliedZeroRegisterUsesRuntimeValue)
{
    constexpr std::string_view code = R"(
        let a = {}
        a[0] = 20
        let i = 0
        return a[i * 3], a[i * 2]
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_integer(S, -2), 20);
    EXPECT_EQ(behl::to_integer(S, -1), 20);
}

TEST_P(OptimizationsTest, FoldedExactDivisionIsFloat)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        let v = 10 / 2
        return typeof(v), v == 5
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_string(S, -2), "number");
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OptimizationsTest, RuntimeExactDivisionIsFloat)
{
    behl::load_stdlib(S);
    constexpr std::string_view code = R"(
        let a = 10
        let v = a / 2
        return typeof(v), v == 5
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_string(S, -2), "number");
    EXPECT_TRUE(behl::to_boolean(S, -1));
}

TEST_P(OptimizationsTest, FoldedArithmeticMatchesRuntime)
{
    behl::load_stdlib(S);
    constexpr std::array<std::string_view, 8> values = { "7", "-7", "3", "-3", "2.5", "-2.5", "0.5", "2" };
    constexpr std::array<std::string_view, 6> ops = { "+", "-", "*", "/", "**", "%" };
    std::string code = "let bad = \"\"\n"
                       "function same(x, y, s) { if (typeof(x) != typeof(y) || tostring(x) != tostring(y)) { bad = bad + s + "
                       "\";\" } }\n";
    for (const auto a : values)
    {
        for (const auto b : values)
        {
            const bool both_int = a.find('.') == std::string_view::npos && b.find('.') == std::string_view::npos;
            for (const auto op : ops)
            {
                if (both_int && op == "/")
                {
                    continue;
                }
                code += "{ let a = ";
                code += a;
                code += "; let b = ";
                code += b;
                code += "; same((";
                code += a;
                code += ") ";
                code += op;
                code += " (";
                code += b;
                code += "), a ";
                code += op;
                code += " b, \"";
                code += a;
                code += " ";
                code += op;
                code += " ";
                code += b;
                code += "\") }\n";
            }
        }
    }
    code += "return bad\n";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(behl::to_string(S, -1), "");
}

TEST_P(OptimizationsTest, NegativeZeroConstantIsDistinct)
{
    constexpr std::string_view code = R"(
        return 1 / -0.0, 1 / 0.0
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_number(S, -2), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(behl::to_number(S, -1), std::numeric_limits<double>::infinity());
}

TEST_P(OptimizationsTest, NegativeZeroLocalIsDistinct)
{
    constexpr std::string_view code = R"(
        let z = -0.0
        let w = 0.0
        return 1 / z, 1 / w
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(behl::to_number(S, -2), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(behl::to_number(S, -1), std::numeric_limits<double>::infinity());
}

INSTANTIATE_TEST_SUITE_P(Mode, OptimizationsTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
