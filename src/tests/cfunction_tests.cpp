#include "common/format.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

class CFunctionTest : public ::testing::Test
{
protected:
    behl::State* S;
    int last_arg_count;
    std::vector<std::string> last_arg_types;
    std::vector<long long> last_int_args;
    std::vector<std::string> last_string_args;
    std::vector<bool> last_bool_args;

    void SetUp() override
    {
        S = behl::new_state();
        register_test_functions(S);
    }

    void TearDown() override
    {
        behl::close(S);
    }

    static int test_print(behl::State* state)
    {
        int n = behl::get_top(state);
        std::vector<std::string>& output = get_printed_output();
        output.clear();
        for (int i = 0; i < n; ++i)
        {
            switch (behl::type(state, i))
            {
                case behl::Type::kNil:
                    output.push_back("nil");
                    break;
                case behl::Type::kBoolean:
                    output.push_back(behl::to_boolean(state, i) ? "true" : "false");
                    break;
                case behl::Type::kInteger:
                    output.push_back(std::to_string(behl::to_integer(state, i)));
                    break;
                case behl::Type::kNumber:
                    output.push_back(std::to_string(behl::to_number(state, i)));
                    break;
                case behl::Type::kString:
                    output.push_back(std::string(behl::to_string(state, i)));
                    break;
                default:
                    output.push_back(std::string(behl::value_typename(state, i)));
                    break;
            }
        }
        return 0;
    }

    static int test_capture_args(behl::State* state)
    {
        if (auto* test_instance = get_test_instance())
        {
            test_instance->last_arg_count = behl::get_top(state);
            test_instance->last_arg_types.clear();
            test_instance->last_int_args.clear();
            test_instance->last_string_args.clear();
            test_instance->last_bool_args.clear();

            for (int i = 0; i < test_instance->last_arg_count; ++i)
            {
                behl::Type type = behl::type(state, i);
                test_instance->last_arg_types.push_back(type_name(type));

                switch (type)
                {
                    case behl::Type::kInteger:
                        test_instance->last_int_args.push_back(behl::to_integer(state, i));
                        break;
                    case behl::Type::kString:
                        test_instance->last_string_args.push_back(std::string(behl::to_string(state, i)));
                        break;
                    case behl::Type::kBoolean:
                        test_instance->last_bool_args.push_back(behl::to_boolean(state, i));
                        break;
                    default:
                        break;
                }
            }
        }
        return 0;
    }

    static int test_return_nothing([[maybe_unused]] behl::State* state)
    {
        return 0;
    }

    static int test_return_one(behl::State* state)
    {
        behl::push_integer(state, 42);
        return 1;
    }

    static int test_return_two(behl::State* state)
    {
        behl::push_string(state, "hello");
        behl::push_integer(state, 123);
        return 2;
    }

    static int test_return_many(behl::State* state)
    {
        behl::push_integer(state, 1);
        behl::push_integer(state, 2);
        behl::push_integer(state, 3);
        behl::push_string(state, "four");
        behl::push_boolean(state, true);
        return 5;
    }

    static int test_sum_integers(behl::State* state)
    {
        int n = behl::get_top(state);
        long long sum = 0;
        for (int i = 0; i < n; ++i)
        {
            if (behl::type(state, i) == behl::Type::kInteger)
            {
                sum += behl::to_integer(state, i);
            }
        }
        behl::push_integer(state, sum);
        return 1;
    }

    static int test_echo_all(behl::State* state)
    {
        return behl::get_top(state);
    }

    static int test_add_two_numbers(behl::State* state)
    {
        if (behl::get_top(state) >= 2)
        {
            long long a = behl::to_integer(state, 0);
            long long b = behl::to_integer(state, 1);
            behl::push_integer(state, a + b);
            return 1;
        }
        return 0;
    }

    static int test_concat_strings(behl::State* state)
    {
        int n = behl::get_top(state);
        std::string result;
        for (int i = 0; i < n; ++i)
        {
            if (behl::type(state, i) == behl::Type::kString)
            {
                result += behl::to_string(state, i);
            }
        }
        behl::push_string(state, result);
        return 1;
    }

    static int test_require_integer(behl::State* state)
    {
        long long val = behl::check_integer(state, 0);
        behl::push_integer(state, val * 2);
        return 1;
    }

    static int test_require_number(behl::State* state)
    {
        double val = behl::check_number(state, 0);
        behl::push_number(state, val * 2.0);
        return 1;
    }

    static int test_require_string(behl::State* state)
    {
        const auto val = behl::check_string(state, 0);
        std::string upper;
        for (auto chr : val)
        {
            upper += static_cast<char>(std::toupper(chr));
        }
        behl::push_string(state, upper);
        return 1;
    }

    static int test_require_boolean(behl::State* state)
    {
        bool val = behl::check_boolean(state, 0);
        behl::push_boolean(state, !val);
        return 1;
    }

    static int test_require_two_integers(behl::State* state)
    {
        long long a = behl::check_integer(state, 0);
        long long b = behl::check_integer(state, 1);
        behl::push_integer(state, a * b);
        return 1;
    }

    static int test_require_mixed_types(behl::State* state)
    {
        long long num = behl::check_integer(state, 0);
        const auto str = behl::check_string(state, 1);
        bool flag = behl::check_boolean(state, 2);

        const auto result = behl::format("{} {} {}", num, str, flag ? "true" : "false");
        behl::push_string(state, result);
        return 1;
    }

    static int test_eval_code(behl::State* state)
    {
        const auto code = behl::check_string(state, 0);

        try
        {
            behl::load_string(state, code);
            behl::call(state, 0, 1);
            return 1;
        }
        catch (const std::exception& e)
        {
            behl::push_string(state, e.what());
            return 1;
        }
    }

    static int test_call_function(behl::State* state)
    {
        int num_args = behl::get_top(state) - 1;
        if (num_args < 0)
        {
            num_args = 0;
        }

        try
        {
            behl::call(state, num_args, 1);
            return 1;
        }
        catch (...)
        {
            return 1;
        }
    }

    static int test_eval_and_double(behl::State* state)
    {
        const auto code = behl::check_string(state, 0);

        try
        {
            behl::load_string(state, code);
            behl::call(state, 0, 1);
        }
        catch (...)
        {
            return 1;
        }

        if (behl::type(state, -1) != behl::Type::kInteger && behl::type(state, -1) != behl::Type::kNumber)
        {
            behl::set_top(state, 0);
            behl::push_string(state, "Result is not a number");
            return 1;
        }

        auto value = behl::to_integer(state, -1);
        behl::set_top(state, 0);
        behl::push_integer(state, value * 2);
        return 1;
    }

    static int test_call_global_function(behl::State* state)
    {
        const auto func_name = behl::check_string(state, 0);
        int num_args = behl::get_top(state) - 1;

        behl::get_global(state, func_name);

        std::vector<int> arg_indices;
        for (int i = 1; i <= num_args; ++i)
        {
            arg_indices.push_back(i);
        }

        if (num_args == 0)
        {
            try
            {
                behl::call(state, 0, 1);
            }
            catch (...)
            {
                return 1;
            }
            return 1;
        }

        behl::push_string(state, "test_call_global_function with args not yet implemented");
        return 1;
    }

    static std::string type_name(behl::Type t)
    {
        switch (t)
        {
            case behl::Type::kNil:
                return "nil";
            case behl::Type::kBoolean:
                return "boolean";
            case behl::Type::kInteger:
                return "integer";
            case behl::Type::kNumber:
                return "number";
            case behl::Type::kString:
                return "string";
            case behl::Type::kTable:
                return "table";
            case behl::Type::kCFunction:
                return "function";
            default:
                return "unknown";
        }
    }

    static std::vector<std::string>& get_printed_output()
    {
        static thread_local std::vector<std::string> output;
        return output;
    }

    static CFunctionTest*& get_test_instance()
    {
        static thread_local CFunctionTest* instance = nullptr;
        return instance;
    }

    void register_test_functions(behl::State* state)
    {
        get_test_instance() = this;

        behl::push_cfunction(state, test_print);
        behl::set_global(state, "print");

        behl::push_cfunction(state, test_capture_args);
        behl::set_global(state, "capture_args");

        behl::push_cfunction(state, test_return_nothing);
        behl::set_global(state, "return_nothing");

        behl::push_cfunction(state, test_return_one);
        behl::set_global(state, "return_one");

        behl::push_cfunction(state, test_return_two);
        behl::set_global(state, "return_two");

        behl::push_cfunction(state, test_return_many);
        behl::set_global(state, "return_many");

        behl::push_cfunction(state, test_sum_integers);
        behl::set_global(state, "sum_integers");

        behl::push_cfunction(state, test_echo_all);
        behl::set_global(state, "echo_all");

        behl::push_cfunction(state, test_add_two_numbers);
        behl::set_global(state, "add_two_numbers");

        behl::push_cfunction(state, test_concat_strings);
        behl::set_global(state, "concat_strings");

        behl::push_cfunction(state, test_require_integer);
        behl::set_global(state, "require_integer");

        behl::push_cfunction(state, test_require_number);
        behl::set_global(state, "require_number");

        behl::push_cfunction(state, test_require_string);
        behl::set_global(state, "require_string");

        behl::push_cfunction(state, test_require_boolean);
        behl::set_global(state, "require_boolean");

        behl::push_cfunction(state, test_require_two_integers);
        behl::set_global(state, "require_two_integers");

        behl::push_cfunction(state, test_require_mixed_types);
        behl::set_global(state, "require_mixed_types");

        behl::push_cfunction(state, test_eval_code);
        behl::set_global(state, "eval_code");

        behl::push_cfunction(state, test_call_function);
        behl::set_global(state, "call_function");

        behl::push_cfunction(state, test_eval_and_double);
        behl::set_global(state, "eval_and_double");

        behl::push_cfunction(state, test_call_global_function);
        behl::set_global(state, "call_global_function");
    }
};

TEST_F(CFunctionTest, PrintSingleString)
{
    constexpr std::string_view code = "print('hello')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    const auto& output = get_printed_output();
    ASSERT_EQ(output.size(), 1);
    ASSERT_EQ(output[0], "hello");
}

TEST_F(CFunctionTest, PrintMultipleArguments)
{
    constexpr std::string_view code = "print('hello', 'world', 42)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    const auto& output = get_printed_output();
    ASSERT_EQ(output.size(), 3);
    ASSERT_EQ(output[0], "hello");
    ASSERT_EQ(output[1], "world");
    ASSERT_EQ(output[2], "42");
}

TEST_F(CFunctionTest, PrintNoArguments)
{
    constexpr std::string_view code = "print()";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    const auto& output = get_printed_output();
    ASSERT_EQ(output.size(), 0);
}

TEST_F(CFunctionTest, PrintMixedTypes)
{
    constexpr std::string_view code = "print(123, true, false, nil, 'test')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));
    const auto& output = get_printed_output();
    ASSERT_EQ(output.size(), 5);
    ASSERT_EQ(output[0], "123");
    ASSERT_EQ(output[1], "true");
    ASSERT_EQ(output[2], "false");
    ASSERT_EQ(output[3], "nil");
    ASSERT_EQ(output[4], "test");
}

TEST_F(CFunctionTest, NativeFunctionReceivesCorrectArgs_SingleString)
{
    constexpr std::string_view code = "capture_args('hello')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    ASSERT_EQ(last_arg_count, 1);
    ASSERT_EQ(last_arg_types.size(), 1);
    ASSERT_EQ(last_arg_types[0], "string");
    ASSERT_EQ(last_string_args.size(), 1);
    ASSERT_EQ(last_string_args[0], "hello");
}

TEST_F(CFunctionTest, NativeFunctionReceivesCorrectArgs_MultipleTypes)
{
    constexpr std::string_view code = "capture_args(42, 'test', true, false)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    ASSERT_EQ(last_arg_count, 4);
    ASSERT_EQ(last_arg_types.size(), 4);
    ASSERT_EQ(last_arg_types[0], "integer");
    ASSERT_EQ(last_arg_types[1], "string");
    ASSERT_EQ(last_arg_types[2], "boolean");
    ASSERT_EQ(last_arg_types[3], "boolean");

    ASSERT_EQ(last_int_args.size(), 1);
    ASSERT_EQ(last_int_args[0], 42);

    ASSERT_EQ(last_string_args.size(), 1);
    ASSERT_EQ(last_string_args[0], "test");

    ASSERT_EQ(last_bool_args.size(), 2);
    ASSERT_EQ(last_bool_args[0], true);
    ASSERT_EQ(last_bool_args[1], false);
}

TEST_F(CFunctionTest, NativeFunctionReceivesCorrectArgs_NoArgs)
{
    constexpr std::string_view code = "capture_args()";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    ASSERT_EQ(last_arg_count, 0);
    ASSERT_EQ(last_arg_types.size(), 0);
    ASSERT_EQ(last_int_args.size(), 0);
    ASSERT_EQ(last_string_args.size(), 0);
    ASSERT_EQ(last_bool_args.size(), 0);
}

TEST_F(CFunctionTest, NativeFunctionReceivesCorrectArgs_WithNil)
{
    constexpr std::string_view code = "capture_args(123, nil, 'end')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    ASSERT_EQ(last_arg_count, 3);
    ASSERT_EQ(last_arg_types.size(), 3);
    ASSERT_EQ(last_arg_types[0], "integer");
    ASSERT_EQ(last_arg_types[1], "nil");
    ASSERT_EQ(last_arg_types[2], "string");

    ASSERT_EQ(last_int_args.size(), 1);
    ASSERT_EQ(last_int_args[0], 123);

    ASSERT_EQ(last_string_args.size(), 1);
    ASSERT_EQ(last_string_args[0], "end");
}

TEST_F(CFunctionTest, NativeFunctionReturnsNothing)
{
    constexpr std::string_view code = "let a = return_nothing()";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    behl::get_global(S, "a");
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, NativeFunctionReturnsOneValue)
{
    behl::get_global(S, "return_one");
    ASSERT_EQ(behl::type(S, -1), behl::Type::kCFunction);
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, NativeFunctionReturnsTwoValues)
{
    behl::get_global(S, "return_two");
    ASSERT_NO_THROW(behl::call(S, 0, 2));

    ASSERT_EQ(behl::type(S, -2), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -2), "hello");

    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 123);

    behl::pop(S, 2);
}

TEST_F(CFunctionTest, NativeFunctionReturnsManyValues)
{
    behl::get_global(S, "return_many");
    ASSERT_NO_THROW(behl::call(S, 0, 5));

    ASSERT_EQ(behl::to_integer(S, -5), 1);
    ASSERT_EQ(behl::to_integer(S, -4), 2);
    ASSERT_EQ(behl::to_integer(S, -3), 3);
    ASSERT_EQ(behl::to_string(S, -2), "four");
    ASSERT_EQ(behl::to_boolean(S, -1), true);

    behl::pop(S, 5);
}

TEST_F(CFunctionTest, NativeFunctionReturnValueTruncation)
{
    behl::get_global(S, "return_many");
    ASSERT_NO_THROW(behl::call(S, 0, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 1);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, NativeFunctionReturnValuePadding)
{
    behl::get_global(S, "return_one");
    ASSERT_NO_THROW(behl::call(S, 0, 3));

    ASSERT_EQ(behl::to_integer(S, -3), 42);
    ASSERT_EQ(behl::type(S, -2), behl::Type::kNil);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
    behl::pop(S, 3);
}

TEST_F(CFunctionTest, NativeFunctionManyArguments)
{
    constexpr std::string_view code = "capture_args(1, 2, 3, 4, 5, 'six', 'seven', true, false, nil)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    ASSERT_EQ(last_arg_count, 10);
    ASSERT_EQ(last_arg_types.size(), 10);

    ASSERT_EQ(last_int_args.size(), 5);
    ASSERT_EQ(last_int_args[0], 1);
    ASSERT_EQ(last_int_args[1], 2);
    ASSERT_EQ(last_int_args[2], 3);
    ASSERT_EQ(last_int_args[3], 4);
    ASSERT_EQ(last_int_args[4], 5);

    ASSERT_EQ(last_string_args.size(), 2);
    ASSERT_EQ(last_string_args[0], "six");
    ASSERT_EQ(last_string_args[1], "seven");

    ASSERT_EQ(last_bool_args.size(), 2);
    ASSERT_EQ(last_bool_args[0], true);
    ASSERT_EQ(last_bool_args[1], false);
}

TEST_F(CFunctionTest, NativeFunctionSumIntegers)
{
    behl::get_global(S, "sum_integers");
    behl::push_integer(S, 1);
    behl::push_integer(S, 2);
    behl::push_integer(S, 3);
    behl::push_integer(S, 4);
    behl::push_integer(S, 5);
    ASSERT_NO_THROW(behl::call(S, 5, 1));

    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 15);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, NativeFunctionSumManyIntegers)
{
    behl::get_global(S, "sum_integers");
    for (int i = 1; i <= 10; ++i)
    {
        behl::push_integer(S, i);
    }
    ASSERT_NO_THROW(behl::call(S, 10, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 55);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, NativeFunctionEchoAll)
{
    behl::get_global(S, "echo_all");
    behl::push_integer(S, 10);
    behl::push_string(S, "test");
    behl::push_boolean(S, true);
    behl::push_nil(S);
    ASSERT_NO_THROW(behl::call(S, 4, 4));

    ASSERT_EQ(behl::to_integer(S, -4), 10);
    ASSERT_EQ(behl::to_string(S, -3), "test");
    ASSERT_EQ(behl::to_boolean(S, -2), true);
    ASSERT_EQ(behl::type(S, -1), behl::Type::kNil);
    behl::pop(S, 4);
}

TEST_F(CFunctionTest, NativeFunctionAddTwoNumbers)
{
    behl::get_global(S, "add_two_numbers");
    behl::push_integer(S, 17);
    behl::push_integer(S, 25);
    ASSERT_NO_THROW(behl::call(S, 2, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, NativeFunctionConcatStrings)
{
    behl::get_global(S, "concat_strings");
    behl::push_string(S, "Hello");
    behl::push_string(S, " ");
    behl::push_string(S, "World");
    behl::push_string(S, "!");
    ASSERT_NO_THROW(behl::call(S, 4, 1));

    ASSERT_EQ(behl::to_string(S, -1), "Hello World!");
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, NativeFunctionNestedCall)
{
    behl::get_global(S, "sum_integers");
    behl::push_integer(S, 1);
    behl::push_integer(S, 2);
    behl::push_integer(S, 3);
    ASSERT_NO_THROW(behl::call(S, 3, 1));
    auto first_sum = behl::to_integer(S, -1);

    behl::get_global(S, "sum_integers");
    behl::push_integer(S, 4);
    behl::push_integer(S, 5);
    behl::push_integer(S, 6);
    ASSERT_NO_THROW(behl::call(S, 3, 1));
    auto second_sum = behl::to_integer(S, -1);

    behl::get_global(S, "add_two_numbers");
    behl::push_integer(S, first_sum);
    behl::push_integer(S, second_sum);
    ASSERT_NO_THROW(behl::call(S, 2, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 21);
    behl::pop(S, 3);
}

TEST_F(CFunctionTest, NativeFunctionChainedCalls)
{
    behl::get_global(S, "return_one");
    ASSERT_NO_THROW(behl::call(S, 0, 1));
    auto a = behl::to_integer(S, -1);
    ASSERT_EQ(a, 42);

    behl::get_global(S, "add_two_numbers");
    behl::push_integer(S, a);
    behl::push_integer(S, 8);
    ASSERT_NO_THROW(behl::call(S, 2, 1));
    auto b = behl::to_integer(S, -1);
    ASSERT_EQ(b, 50);

    behl::get_global(S, "sum_integers");
    behl::push_integer(S, b);
    behl::push_integer(S, 10);
    behl::push_integer(S, 20);
    ASSERT_NO_THROW(behl::call(S, 3, 1));
    auto c = behl::to_integer(S, -1);
    ASSERT_EQ(c, 80);

    behl::pop(S, 3);
}

TEST_F(CFunctionTest, CheckInteger_ValidInteger)
{
    behl::get_global(S, "require_integer");
    behl::push_integer(S, 42);
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 84);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CheckNumber_ValidNumber)
{
    behl::get_global(S, "require_number");
    behl::push_number(S, 3.5);
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 7.0);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CheckNumber_ValidInteger)
{
    behl::get_global(S, "require_number");
    behl::push_integer(S, 5);
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_DOUBLE_EQ(behl::to_number(S, -1), 10.0);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CheckString_ValidString)
{
    behl::get_global(S, "require_string");
    behl::push_string(S, "hello");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_string(S, -1), "HELLO");
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CheckBoolean_ValidBoolean)
{
    constexpr std::string_view code = "let result = require_boolean(true)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    ASSERT_NO_THROW(behl::call(S, 0, 0));

    behl::get_global(S, "result");
    ASSERT_EQ(behl::to_boolean(S, -1), false);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CheckInteger_TwoIntegers)
{
    behl::get_global(S, "require_two_integers");
    behl::push_integer(S, 6);
    behl::push_integer(S, 7);
    ASSERT_NO_THROW(behl::call(S, 2, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CheckMixedTypes_ValidTypes)
{
    behl::get_global(S, "require_mixed_types");
    behl::push_integer(S, 42);
    behl::push_string(S, "test");
    behl::push_boolean(S, true);
    ASSERT_NO_THROW(behl::call(S, 3, 1));

    ASSERT_EQ(behl::to_string(S, -1), "42 test true");
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CheckInteger_WrongType_String)
{
    constexpr std::string_view code = "let result = require_integer('not a number')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckInteger_WrongType_Nil)
{
    constexpr std::string_view code = "let result = require_integer(nil)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckInteger_WrongType_Boolean)
{
    constexpr std::string_view code = "let result = require_integer(true)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckNumber_WrongType_String)
{
    constexpr std::string_view code = "let result = require_number('not a number')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckString_WrongType_Integer)
{
    constexpr std::string_view code = "let result = require_string(42)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckString_WrongType_Nil)
{
    constexpr std::string_view code = "let result = require_string(nil)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckBoolean_WrongType_Integer)
{
    constexpr std::string_view code = "let result = require_boolean(42)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckBoolean_WrongType_String)
{
    constexpr std::string_view code = "let result = require_boolean('true')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckInteger_SecondArgumentWrong)
{
    constexpr std::string_view code = "let result = require_two_integers(10, 'not an int')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckMixedTypes_FirstArgumentWrong)
{
    constexpr std::string_view code = "let result = require_mixed_types('wrong', 'test', true)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckMixedTypes_SecondArgumentWrong)
{
    constexpr std::string_view code = "let result = require_mixed_types(42, 123, true)";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckMixedTypes_ThirdArgumentWrong)
{
    constexpr std::string_view code = "let result = require_mixed_types(42, 'test', 'not bool')";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CheckInteger_MissingArgument)
{
    constexpr std::string_view code = "let result = require_integer()";
    ASSERT_NO_THROW(behl::load_string(S, code));
    EXPECT_ANY_THROW(behl::call(S, 0, 0));
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalSimpleExpression)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return 2 + 3");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::type(S, -1), behl::Type::kInteger);
    ASSERT_EQ(behl::to_integer(S, -1), 5);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalStringLiteral)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return 'hello world'");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "hello world");
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalFunctionCall)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return sum_integers(1, 2, 3, 4)");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 10);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalAndDouble)
{
    behl::get_global(S, "eval_and_double");
    behl::push_string(S, "return 10 + 5");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 30);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalComplexExpression)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return (5 + 3) * 2 - 1");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 15);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalWithVariable)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "let x = 42; return x * 2");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 84);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_NestedEval)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return eval_code('return 5 + 5')");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 10);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_CallNativeFromEval)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return add_two_numbers(20, 22)");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalReturnsMultipleValues)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return return_two()");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    ASSERT_EQ(behl::to_string(S, -1), "hello");
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalInvalidCode)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "this is not valid syntax @#$");

    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    const auto result = behl::to_string(S, -1);

    ASSERT_TRUE(result.find("SyntaxError") != std::string_view::npos || result.find("Unexpected") != std::string_view::npos);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalRuntimeError)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return undefined_variable + 5");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::type(S, -1), behl::Type::kString);
    const auto result = behl::to_string(S, -1);

    ASSERT_GT(result.length(), 0);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_CallGlobalNoArgs)
{
    behl::get_global(S, "call_global_function");
    behl::push_string(S, "return_one");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 42);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_DeepNesting)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, "return sum_integers(5, 10, 5)");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 20);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalDefineFunction)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, R"(
        function make_adder(x) {
            return x + 10
        }
        return make_adder(5)
    )");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 15);
    behl::pop(S, 1);
}

TEST_F(CFunctionTest, CFunction_CallsBackIntoBehl_EvalLoop)
{
    behl::get_global(S, "eval_code");
    behl::push_string(S, R"(
        let sum = 0
        for (let i = 1; i <= 5; i = i + 1) {
            sum = sum + i
        }
        return sum
    )");
    ASSERT_NO_THROW(behl::call(S, 1, 1));

    ASSERT_EQ(behl::to_integer(S, -1), 15);
    behl::pop(S, 1);
}
