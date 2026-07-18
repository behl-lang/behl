#include "gc/gc_object.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "state.hpp"
#include "vm/bytecode.hpp"
#include "vm/value.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>

class OptimizationsTest : public ::testing::Test
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

TEST_F(OptimizationsTest, NumericForLoopOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i < 10; i++) {
        }
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

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

TEST_F(OptimizationsTest, ComplexConditionNotOptimized)
{
    constexpr std::string_view code = R"(
        function check(x) {
            return x < 10
        }
        for (let i = 0; check(i); i++) {
        }
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

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

TEST_F(OptimizationsTest, DecrementingForLoopOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 10; i > 0; i--) {
        }
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

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

TEST_F(OptimizationsTest, ForLoopWithStepOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i < 100; i += 5) {
        }
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

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

TEST_F(OptimizationsTest, ConstLoopVariableNotOptimized)
{
    constexpr std::string_view code = R"(
        for (const i = 0; i < 10; i++) {
        }
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}

TEST_F(OptimizationsTest, ForLoopWithoutLetNotOptimized)
{
    constexpr std::string_view code = R"(
        let i = 999
        for (i = 0; i < 10; i++) {
        }
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

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

TEST_F(OptimizationsTest, InclusiveLoopOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i <= 10; i++) {
        }
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

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

TEST_F(OptimizationsTest, MismatchedDirectionNotOptimized)
{
    constexpr std::string_view code = R"(
        for (let i = 0; i < 10; i--) {
        }
    )";

    ASSERT_NO_THROW(behl::load_string(S, code));

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
