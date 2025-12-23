#include <behl/behl.hpp>
#include <benchmark/benchmark.h>
#include <string>

using namespace behl;

static void BM_ScriptCall_SimpleReturn(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = "return 42;";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1); // Duplicate function on stack
        call(S, 0, 1);
        pop(S, 1); // Pop result
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_SimpleReturn)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_Arithmetic(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = "return 10 + 20 * 30 - 5 / 2;";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_Arithmetic)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_FunctionCall(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        function add(a, b) {
            return a + b;
        }
        return add(10, 20);
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_FunctionCall)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_Factorial(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        function factorial(n) {
            if (n <= 1) {
                return 1;
            } else {
                return n * factorial(n - 1);
            }
        }
        return factorial(10);
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_Factorial)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_ForLoop(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        let sum = 0;
        for (let i = 0; i < 100; i++) {
            sum += i;
        }
        return sum;
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_ForLoop)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_TableOps(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        let t = {1, 2, 3, 4, 5};
        let sum = 0;
        for (let i = 0; i < 5; i++) {
            sum += t[i];
        }
        return sum;
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_TableOps)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_NestedCalls(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        function inner(x) {
            return x * 2;
        }
        function middle(x) {
            return inner(x) + 1;
        }
        function outer(x) {
            return middle(x) * 3;
        }
        return outer(5);
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_NestedCalls)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_Closure(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        function makeAdder(x) {
            return function(y) {
                return x + y;
            };
        }
        let add5 = makeAdder(5);
        return add5(10);
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_Closure)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_WhileLoop(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        let sum = 0;
        let i = 0;
        while (i < 100) {
            sum += i;
            i++;
        }
        return sum;
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_WhileLoop)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_StringConcat(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        let str = "Hello";
        for (let i = 0; i < 10; i++) {
            str = str + " World";
        }
        return str;
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_StringConcat)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_TableConstruct(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        let t = {
            name = "test",
            value = 42,
            nested = {a = 1, b = 2, c = 3},
            array = {10, 20, 30, 40, 50}
        };
        return t["value"];
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_TableConstruct)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_WithArguments(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        function multiply(a, b) {
            return a * b;
        }
        return multiply;
    )";
    load_string(S, code);
    call(S, 0, 1); // Load the function

    for (auto _ : state)
    {
        dup(S, -1); // Duplicate the multiply function
        push_integer(S, 42);
        push_integer(S, 7);
        call(S, 2, 1);
        pop(S, 1); // Pop result
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_WithArguments)->Unit(benchmark::kMicrosecond);

static void BM_ScriptCall_Fibonacci(benchmark::State& state)
{
    State* S = new_state();
    std::string_view code = R"(
        function fib(n) {
            if (n <= 1) {
                return n;
            }
            return fib(n - 1) + fib(n - 2);
        }
        return fib(15);
    )";
    load_string(S, code);

    for (auto _ : state)
    {
        dup(S, -1);
        call(S, 0, 1);
        pop(S, 1);
    }

    state.counters["calls/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);

    close(S);
}
BENCHMARK(BM_ScriptCall_Fibonacci)->Unit(benchmark::kMicrosecond);
