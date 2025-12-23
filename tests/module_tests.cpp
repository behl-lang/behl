#include <behl/behl.hpp>
#include <gtest/gtest.h>

class ModuleTest : public ::testing::Test
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        ASSERT_NE(S, nullptr);
        behl::load_stdlib(S, true);
    }

    void TearDown() override
    {
        if (S)
        {
            behl::close(S);
        }
    }
};

TEST_F(ModuleTest, Module_ExportConst)
{
    constexpr std::string_view code = R"(
        module;
        export const VALUE = 42;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    behl::table_rawgetfield(S, -1, "VALUE");
    EXPECT_EQ(behl::type(S, -1), behl::Type::kInteger);
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(ModuleTest, Module_ExportFunction)
{
    constexpr std::string_view code = R"(
        module;
        export function add(a, b) {
            return a + b;
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    behl::table_rawgetfield(S, -1, "add");
    ASSERT_EQ(behl::type(S, -1), behl::Type::kClosure);

    behl::push_integer(S, 5);
    behl::push_integer(S, 3);
    ASSERT_NO_THROW(behl::call(S, 2, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 8);
}

TEST_F(ModuleTest, Module_PrivateVariables)
{
    constexpr std::string_view code = R"(
        module;
        let privateVar = 123;
        function privateFunc() {
            return 456;
        }
        export function getPrivate() {
            return privateVar + privateFunc();
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    behl::table_rawgetfield(S, -1, "privateVar");
    EXPECT_EQ(behl::type(S, -1), behl::Type::kNil);
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "privateFunc");
    EXPECT_EQ(behl::type(S, -1), behl::Type::kNil);
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "getPrivate");
    ASSERT_EQ(behl::type(S, -1), behl::Type::kClosure);
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 579);
}

TEST_F(ModuleTest, Module_NoGlobalAccess)
{
    behl::push_integer(S, 999);
    behl::set_global(S, "globalVar");

    constexpr std::string_view code = R"(
        module;
        export function tryAccessGlobal() {
            return globalVar;
        }
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}

TEST_F(ModuleTest, Module_NoGlobalSet)
{
    constexpr std::string_view code = R"(
        module;
        globalVar = 123;
        export const TEST = 1;
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}

TEST_F(ModuleTest, Module_CanUseBuiltins)
{
    constexpr std::string_view code = R"(
        module;
        export function stringify(x) {
            return tostring(x);
        }
        export function parseNum(s) {
            return tonumber(s);
        }
        export function getType(x) {
            return typeof(x);
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    behl::table_rawgetfield(S, -1, "stringify");
    behl::push_integer(S, 42);
    ASSERT_NO_THROW(behl::call(S, 1, 1));
    EXPECT_EQ(behl::to_string(S, -1), "42");
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "parseNum");
    behl::push_string(S, "123");
    ASSERT_NO_THROW(behl::call(S, 1, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 123);
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "getType");
    behl::push_integer(S, 42);
    ASSERT_NO_THROW(behl::call(S, 1, 1));
    EXPECT_EQ(behl::to_string(S, -1), "integer");
}

TEST_F(ModuleTest, Module_StatefulExports)
{
    constexpr std::string_view code = R"(
        module;
        let counter = 0;
        export function increment() {
            counter++;
            return counter;
        }
        export function getCount() {
            return counter;
        }
        export function reset() {
            counter = 0;
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    behl::table_rawgetfield(S, -1, "increment");
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 1);
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "increment");
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 2);
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "getCount");
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 2);
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "reset");
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    behl::table_rawgetfield(S, -1, "getCount");
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 0);
}

TEST_F(ModuleTest, Module_Empty)
{
    constexpr std::string_view code = R"(
        module;
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    behl::push_nil(S);
    EXPECT_FALSE(behl::table_next(S, -2));
}

TEST_F(ModuleTest, Module_UndefinedVariable)
{
    constexpr std::string_view code = R"(
        module;
        export function test() {
            return undefinedVar;
        }
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}

TEST_F(ModuleTest, Module_CanAccessStdLib)
{
    constexpr std::string_view code = R"(
        module;
        const string = import("string");
        const math = import("math");
        export function upperCase(s) {
            return string.upper(s);
        }
        export function abs(x) {
            return math.abs(x);
        }
    )";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    ASSERT_EQ(behl::type(S, -1), behl::Type::kTable);

    behl::table_rawgetfield(S, -1, "upperCase");
    behl::push_string(S, "hello");
    ASSERT_NO_THROW(behl::call(S, 1, 1));
    EXPECT_EQ(behl::to_string(S, -1), "HELLO");
    behl::pop(S, 1);

    behl::table_rawgetfield(S, -1, "abs");
    behl::push_integer(S, -42);
    ASSERT_NO_THROW(behl::call(S, 1, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 42);
}

TEST_F(ModuleTest, Module_CannotAccessStdLibWithoutImport)
{
    constexpr std::string_view code = R"(
        module;
        export function test() {
            return math.abs(-5);
        }
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}
