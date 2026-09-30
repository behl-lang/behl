#include "gc/gc.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "state.hpp"
#include "test_helpers.hpp"
#include "vm/bytecode.hpp"
#include "vm/value.hpp"

#include <array>
#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

using namespace behl;

struct TypeOfTestCase
{
    std::string_view expr;
    std::string_view name;
    Type type;
};

static constexpr std::array kTypeCases = {
    TypeOfTestCase{ "nil", "nil", Type::kNil },
    TypeOfTestCase{ "true", "boolean", Type::kBoolean },
    TypeOfTestCase{ "42", "integer", Type::kInteger },
    TypeOfTestCase{ "4.5", "number", Type::kNumber },
    TypeOfTestCase{ "\"text\"", "string", Type::kString },
    TypeOfTestCase{ "{}", "table", Type::kTable },
    TypeOfTestCase{ "function() { return 1 }", "function", Type::kClosure },
    TypeOfTestCase{ "print", "function", Type::kCFunction },
    TypeOfTestCase{ "ud", "userdata", Type::kUserdata },
    TypeOfTestCase{ "buf", "buffer", Type::kBuffer },
};

static uint64_t total_helper_calls([[maybe_unused]] State* S)
{
#if BEHL_JIT_SUPPORTED
    uint64_t total = 0;
    for (const uint64_t count : S->jit_stats.helper_calls)
    {
        total += count;
    }
    return total;
#else
    return 0;
#endif
}

class TypeOfTest : public ::testing::TestWithParam<bool>
{
protected:
    State* S = nullptr;

    void SetUp() override
    {
        S = new_state();
        S->jit_enabled = GetParam();
        load_stdlib(S);
        userdata_new(S, 8, make_uid("TypeOfTestUserdata"));
        set_global(S, "ud");
        buffer_new(S, 4);
        set_global(S, "buf");
    }

    void TearDown() override
    {
        close(S);
    }

    GCProto* main_proto() const
    {
        EXPECT_EQ(get_top(S), 1);
        return S->stack.back().get_closure()->proto;
    }

    bool expect_jit_compiled(const GCProto* proto) const
    {
#if BEHL_JIT_SUPPORTED
        if (GetParam())
        {
            EXPECT_NE(proto->jit_code, nullptr) << "the JIT declined a proto using TYPEOF/TYPEID";
            EXPECT_FALSE(proto->jit_declined);
            return proto->jit_code != nullptr;
        }
#else
        (void)proto;
#endif
        return false;
    }
};

TEST_P(TypeOfTest, EveryTypeNameAndId)
{
    for (const TypeOfTestCase& tc : kTypeCases)
    {
        const std::string code = "let v = " + std::string(tc.expr) + "\nreturn typeof(v), typeid(v)";
        ASSERT_TRUE(behl_test::load_ok(S, code)) << tc.expr;
        ASSERT_TRUE(behl_test::call_ok(S, 0, 2)) << tc.expr;
        EXPECT_EQ(type(S, -2), Type::kString) << tc.expr;
        EXPECT_EQ(to_string(S, -2), tc.name) << tc.expr;
        EXPECT_EQ(type(S, -1), Type::kInteger) << tc.expr;
        EXPECT_EQ(to_integer(S, -1), static_cast<Integer>(static_cast<uint8_t>(tc.type))) << tc.expr;
        set_top(S, 0);
    }
}

TEST_P(TypeOfTest, EveryTypeInsideHotFunctionMatchesInterpreterNames)
{
    constexpr std::string_view code = R"(
        function probe(v) { return typeof(v), typeid(v) }
        let values = { nil, true, 42, 4.5, "text", {}, function() { return 1 }, print, ud, buf }
        let names = ""
        let ids = 0
        for (let round = 0; round < 200; round++) {
            names = ""
            ids = 0
            for (let i = 0; i < 10; i++) {
                let n, id = probe(values[i])
                names = names + n + ","
                ids = ids + id
            }
        }
        return names, ids
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));

    std::string expected_names;
    Integer expected_ids = 0;
    for (const TypeOfTestCase& tc : kTypeCases)
    {
        expected_names += tc.name;
        expected_names += ",";
        expected_ids += static_cast<Integer>(static_cast<uint8_t>(tc.type));
    }
    EXPECT_EQ(to_string(S, -2), expected_names);
    EXPECT_EQ(to_integer(S, -1), expected_ids);
}

TEST_P(TypeOfTest, ResultIsTheRootedStateStringAndEqualsLiterals)
{
    constexpr std::string_view code = R"(
        let a = typeof(1)
        let b = typeof(2)
        let t = {}
        t[typeof(3)] = "hit"
        return a, b, a == "integer", t.integer, typeof("x") == "string", typeof(print) == typeof(function() {})
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 6));

    const GCString* expected = S->type_names[type_name_slot(Type::kInteger)];
    ASSERT_NE(expected, nullptr);
    EXPECT_EQ(S->stack[S->stack.size() - 6].get_string(), expected);
    EXPECT_EQ(S->stack[S->stack.size() - 5].get_string(), expected);
    EXPECT_TRUE(to_boolean(S, -4));
    EXPECT_EQ(to_string(S, -3), "hit");
    EXPECT_TRUE(to_boolean(S, -2));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(TypeOfTest, TypeNamesSurviveCollection)
{
    gc_collect(S);
    gc_collect(S);
    constexpr std::string_view code = R"(
        let s = ""
        for (let i = 0; i < 2000; i++) { s = tostring(i) + "garbage" }
        return typeof(s), typeof(nil), typeof(1.0)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    gc_collect(S);
    EXPECT_EQ(to_string(S, -3), "string");
    EXPECT_EQ(to_string(S, -2), "nil");
    EXPECT_EQ(to_string(S, -1), "number");
}

TEST_P(TypeOfTest, LocalShadowingMakesANormalCall)
{
    constexpr std::string_view code = R"(
        let typeof = function(v) { return "local" }
        let typeid = function(v) { return -1 }
        return typeof(1), typeid(1)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "local");
    EXPECT_EQ(to_integer(S, -1), -1);
}

TEST_P(TypeOfTest, UpvalueShadowingMakesANormalCall)
{
    constexpr std::string_view code = R"(
        let typeof = function(v) { return "upvalue" }
        let typeid = function(v) { return -2 }
        function outer() {
            function inner(x) { return typeof(x), typeid(x) }
            return inner(1)
        }
        return outer()
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "upvalue");
    EXPECT_EQ(to_integer(S, -1), -2);
}

TEST_P(TypeOfTest, ParameterShadowingMakesANormalCall)
{
    constexpr std::string_view code = R"(
        function apply(typeof, typeid, x) { return typeof(x), typeid(x) }
        return apply(function(v) { return "param" }, function(v) { return -3 }, 1)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "param");
    EXPECT_EQ(to_integer(S, -1), -3);
}

TEST_P(TypeOfTest, ShadowingEndsWithItsScope)
{
    constexpr std::string_view code = R"(
        let r = ""
        {
            let typeof = function(v) { return "inner" }
            r = typeof(1)
        }
        return r, typeof(1)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "inner");
    EXPECT_EQ(to_string(S, -1), "integer");
}

TEST_P(TypeOfTest, BareNamesAreNotValues)
{
    constexpr std::string_view code = R"(
        let f = typeof
        let g = typeid
        return f == nil, g == nil
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_TRUE(to_boolean(S, -2));
    EXPECT_TRUE(to_boolean(S, -1));
}

TEST_P(TypeOfTest, ZeroArgumentsIsACallOfNil)
{
    for (const std::string_view code : { "return typeof()", "return typeid()" })
    {
        ASSERT_TRUE(behl_test::load_ok(S, code)) << code;
        ASSERT_TRUE(behl_test::call_fails(S, 0, 1)) << code;
        EXPECT_NE(behl_test::error_text(S).find("attempt to call nil value"), std::string::npos) << code;
        set_top(S, 0);
    }
}

TEST_P(TypeOfTest, ExtraArgumentsIsACallOfNil)
{
    for (const std::string_view code : { "return typeof(1, 2)", "return typeid(1, 2)" })
    {
        ASSERT_TRUE(behl_test::load_ok(S, code)) << code;
        ASSERT_TRUE(behl_test::call_fails(S, 0, 1)) << code;
        EXPECT_NE(behl_test::error_text(S).find("attempt to call nil value"), std::string::npos) << code;
        set_top(S, 0);
    }
}

TEST_P(TypeOfTest, SingleResultInMultiValueContexts)
{
    constexpr std::string_view code = R"(
        function pack(...) { return select_count(...) }
        function select_count(...) { let t = { ... }; return #t }
        function ret_multi(x) { return 7, typeof(x) }
        function ret_tail(x) { return typeof(x) }
        let a, b = typeof(1)
        let c, d = ret_multi(1.5)
        return pack(typeof(1)), pack(1, typeid(2)), a, b == nil, c, d, ret_tail("s")
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 7));
    EXPECT_EQ(to_integer(S, -7), 1);
    EXPECT_EQ(to_integer(S, -6), 2);
    EXPECT_EQ(to_string(S, -5), "integer");
    EXPECT_TRUE(to_boolean(S, -4));
    EXPECT_EQ(to_integer(S, -3), 7);
    EXPECT_EQ(to_string(S, -2), "number");
    EXPECT_EQ(to_string(S, -1), "string");
}

TEST_P(TypeOfTest, ArgumentIsEvaluatedOnce)
{
    constexpr std::string_view code = R"(
        let calls = 0
        function f() { calls = calls + 1; return 1, "two" }
        let t = typeof(f())
        let i = typeid(f())
        return calls, t, i
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 3));
    EXPECT_EQ(to_integer(S, -3), 2);
    EXPECT_EQ(to_string(S, -2), "integer");
    EXPECT_EQ(to_integer(S, -1), static_cast<Integer>(static_cast<uint8_t>(Type::kInteger)));
}

TEST_P(TypeOfTest, UsableAsCondition)
{
    constexpr std::string_view code = R"(
        let hits = 0
        if (typeof(nil)) { hits = hits + 1 }
        if (typeid(nil)) { hits = hits + 10 }
        if (typeof(1) == "integer" && typeid(1.5) != typeid(1)) { hits = hits + 100 }
        return hits
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    EXPECT_EQ(to_integer(S, -1), 111);
}

TEST_P(TypeOfTest, BytecodeUsesTheIntrinsicOpcodes)
{
    constexpr std::string_view code = R"(
        let x = 1
        let a = typeof(x)
        let b = typeid(x)
        return a, b
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    const GCProto* proto = main_proto();

    bool saw_typeof = false;
    bool saw_typeid = false;
    bool saw_call = false;
    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        const Instruction ins = proto->code[i];
        const std::string text = instruction_to_string(ins, i);
        if (ins.op() == OpCode::kOpTypeOf)
        {
            saw_typeof = true;
            EXPECT_NE(text.find("TYPEOF"), std::string::npos) << text;
        }
        if (ins.op() == OpCode::kOpTypeId)
        {
            saw_typeid = true;
            EXPECT_NE(text.find("TYPEID"), std::string::npos) << text;
        }
        if (ins.op() == OpCode::kOpCall || ins.op() == OpCode::kOpTailCall || ins.op() == OpCode::kOpGetGlobal)
        {
            saw_call = true;
        }
    }
    EXPECT_TRUE(saw_typeof);
    EXPECT_TRUE(saw_typeid);
    EXPECT_FALSE(saw_call) << "typeof/typeid compiled to a global lookup or call";
}

TEST_P(TypeOfTest, ShadowedNameEmitsNoIntrinsicOpcode)
{
    constexpr std::string_view code = R"(
        let typeof = function(v) { return v }
        return typeof(1)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    const GCProto* proto = main_proto();
    for (size_t i = 0; i < proto->code.size(); ++i)
    {
        EXPECT_NE(proto->code[i].op(), OpCode::kOpTypeOf) << instruction_to_string(proto->code[i], i);
    }
}

TEST_P(TypeOfTest, HotLoopWithSlotCacheStaysNative)
{
    constexpr std::string_view code = R"(
        let s = 0
        let last = nil
        let f = 1.5
        for (let i = 0; i < 20000; i = i + 1) {
            last = typeof(i)
            s = s + typeid(last) + typeid(f) + typeid(i)
            last = typeof(f)
        }
        return s, last
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    const GCProto* proto = main_proto();
    dup(S, -1);
    const uint64_t before = total_helper_calls(S);
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    const uint64_t helpers = total_helper_calls(S) - before;

    constexpr Integer kPerIteration = static_cast<Integer>(static_cast<uint8_t>(Type::kString))
        + static_cast<Integer>(static_cast<uint8_t>(Type::kNumber))
        + static_cast<Integer>(static_cast<uint8_t>(Type::kInteger));
    EXPECT_EQ(to_integer(S, -2), 20000 * kPerIteration);
    EXPECT_EQ(to_string(S, -1), "number");
    if (expect_jit_compiled(proto))
    {
        EXPECT_LT(helpers, 64u) << "typeof/typeid fell back to helper calls";
    }
}

TEST_P(TypeOfTest, HotLoopWithoutSlotCacheStaysNative)
{
    constexpr std::string_view code = R"(
        function unused() { return 0 }
        let s = 0
        let names = 0
        let v = "str"
        for (let i = 0; i < 20000; i = i + 1) {
            let t = typeof(v)
            let u = typeof(i)
            s = s + typeid(t) + typeid(u) + typeid(names)
            v = i
        }
        return s, typeof(v)
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    const GCProto* proto = main_proto();
    ASSERT_FALSE(proto->protos.empty());
    dup(S, -1);
    const uint64_t before = total_helper_calls(S);
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    const uint64_t helpers = total_helper_calls(S) - before;

    constexpr Integer kString = static_cast<Integer>(static_cast<uint8_t>(Type::kString));
    constexpr Integer kInteger = static_cast<Integer>(static_cast<uint8_t>(Type::kInteger));
    EXPECT_EQ(to_integer(S, -2), 20000 * (kString + kString + kInteger));
    EXPECT_EQ(to_string(S, -1), "integer");
    if (expect_jit_compiled(proto))
    {
        EXPECT_LT(helpers, 64u) << "typeof/typeid fell back to helper calls";
    }
}

TEST_P(TypeOfTest, SameRegisterSourceAndDestination)
{
    constexpr std::string_view code = R"(
        function f(x) {
            for (let i = 0; i < 300; i++) {
                x = typeof(x)
            }
            return x
        }
        function g(x) {
            for (let i = 0; i < 300; i++) {
                x = typeid(x)
            }
            return x
        }
        return f(1), g("s")
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    ASSERT_TRUE(behl_test::call_ok(S, 0, 2));
    EXPECT_EQ(to_string(S, -2), "string");
    EXPECT_EQ(to_integer(S, -1), static_cast<Integer>(static_cast<uint8_t>(Type::kInteger)));
}

TEST_P(TypeOfTest, NoAllocationPerCall)
{
    constexpr std::string_view code = R"(
        let s = 0
        let last = nil
        for (let i = 0; i < 100000; i = i + 1) {
            last = typeof(i)
            last = typeof(last)
            s = s + typeid(last)
        }
        return s
    )";
    ASSERT_TRUE(behl_test::load_ok(S, code));
    gc_collect(S);
    const GCPauseGuard pause{ S };
    const size_t before = S->gc.gc_total_bytes;
    ASSERT_TRUE(behl_test::call_ok(S, 0, 1));
    const size_t after = S->gc.gc_total_bytes;
    EXPECT_EQ(to_integer(S, -1), 100000 * static_cast<Integer>(static_cast<uint8_t>(Type::kString)));
    const size_t grown = after > before ? after - before : 0;
    EXPECT_LT(grown, size_t{ 64 * 1024 }) << "typeof allocated per call";
}

INSTANTIATE_TEST_SUITE_P(Mode, TypeOfTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
