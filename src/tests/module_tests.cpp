#include "state.hpp"

#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

class ModuleTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        ASSERT_NE(S, nullptr);
        behl::load_stdlib(S);
    }

    void TearDown() override
    {
        if (S)
        {
            behl::close(S);
        }
    }
};

TEST_P(ModuleTest, Module_ExportConst)
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

TEST_P(ModuleTest, Module_ExportFunction)
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

TEST_P(ModuleTest, Module_PrivateVariables)
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

TEST_P(ModuleTest, Module_NoGlobalAccess)
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

TEST_P(ModuleTest, Module_NoGlobalSet)
{
    constexpr std::string_view code = R"(
        module;
        globalVar = 123;
        export const TEST = 1;
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}

TEST_P(ModuleTest, Module_CanUseBuiltins)
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

TEST_P(ModuleTest, Module_StatefulExports)
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

TEST_P(ModuleTest, Module_Empty)
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

TEST_P(ModuleTest, Module_UndefinedVariable)
{
    constexpr std::string_view code = R"(
        module;
        export function test() {
            return undefinedVar;
        }
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}

TEST_P(ModuleTest, Module_CanAccessStdLib)
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

TEST_P(ModuleTest, Module_CannotAccessStdLibWithoutImport)
{
    constexpr std::string_view code = R"(
        module;
        export function test() {
            return math.abs(-5);
        }
    )";

    EXPECT_THROW(behl::load_string(S, code), std::exception);
}

class ModuleFileTest : public ::testing::TestWithParam<bool>
{
protected:
    behl::State* S = nullptr;
    std::filesystem::path root;

    void SetUp() override
    {
        S = behl::new_state();
        S->jit_enabled = GetParam();
        ASSERT_NE(S, nullptr);
        behl::load_stdlib(S);

        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        root = std::filesystem::temp_directory_path() / "behl_module_tests" / info->name();
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root);
    }

    void TearDown() override
    {
        if (S)
        {
            behl::close(S);
        }
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void write_file(const std::filesystem::path& relative, std::string_view content)
    {
        const auto full = root / relative;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream out(full);
        out << content;
    }

    std::string main_path() const
    {
        return (root / "main.behl").string();
    }
};

TEST_P(ModuleFileTest, ImportResolvesSiblingOfImporter)
{
    write_file("helper.behl", "module;\nexport const VALUE = 99;\n");

    constexpr std::string_view code = R"(
        const h = import("helper");
        return h.VALUE;
    )";
    ASSERT_NO_THROW(behl::load_buffer(S, code, main_path()));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 99);
}

TEST_P(ModuleFileTest, ImportResolvesModulesSubdirectoryOfImporter)
{
    write_file("modules/helper.behl", "module;\nexport const VALUE = 7;\n");

    constexpr std::string_view code = R"(
        const h = import("helper");
        return h.VALUE;
    )";
    ASSERT_NO_THROW(behl::load_buffer(S, code, main_path()));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 7);
}

TEST_P(ModuleFileTest, ImportResolvesExplicitRelativePath)
{
    write_file("helper.behl", "module;\nexport const VALUE = 11;\n");

    constexpr std::string_view code = R"(
        const h = import("./helper");
        return h.VALUE;
    )";
    ASSERT_NO_THROW(behl::load_buffer(S, code, main_path()));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 11);
}

TEST_P(ModuleFileTest, ImportResolvesFromNestedImporter)
{
    write_file("nested/helper.behl", "module;\nexport const VALUE = 5;\n");
    write_file("nested/mid.behl", "module;\nconst h = import(\"helper\");\nexport const DOUBLED = h.VALUE * 2;\n");

    constexpr std::string_view code = R"(
        const m = import("./nested/mid");
        return m.DOUBLED;
    )";
    ASSERT_NO_THROW(behl::load_buffer(S, code, main_path()));
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    EXPECT_EQ(behl::to_integer(S, -1), 10);
}

TEST_P(ModuleFileTest, ImportFailureNamesTheModule)
{
    constexpr std::string_view code = R"(
        const h = import("definitely_absent_module");
        return 1;
    )";
    ASSERT_NO_THROW(behl::load_buffer(S, code, main_path()));

    std::string message;
    try
    {
        behl::call(S, 0, 1);
    }
    catch (const behl::BehlException& e)
    {
        message = e.what();
    }

    ASSERT_FALSE(message.empty()) << "expected import of a missing module to throw";
    EXPECT_NE(message.find("definitely_absent_module"), std::string::npos)
        << "error message should name the module, got: " << message;
}

INSTANTIATE_TEST_SUITE_P(Mode, ModuleTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });

INSTANTIATE_TEST_SUITE_P(Mode, ModuleFileTest, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& param_info) { return param_info.param ? "jit" : "nojit"; });
