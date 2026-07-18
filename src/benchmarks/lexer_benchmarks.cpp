#include "frontend/lexer.hpp"

#include <behl/behl.hpp>
#include <benchmark/benchmark.h>
#include <string>

using namespace behl;

static void BM_Lexer_SimpleArithmetic(benchmark::State& state)
{
    auto* S = behl::new_state();

    std::string_view code = "let x = 10 + 20 * 30 - 5 / 2;";
    for (auto _ : state)
    {
        auto tokens = tokenize(S, code, "benchmark");
        benchmark::DoNotOptimize(tokens);
    }

    behl::close(S);
}
BENCHMARK(BM_Lexer_SimpleArithmetic)->Unit(benchmark::kMillisecond);

static void BM_Lexer_FunctionDefinition(benchmark::State& state)
{
    auto* S = behl::new_state();

    std::string_view code = R"(
        function factorial(n) {
            if (n <= 1) {
                return 1;
            } else {
                return n * factorial(n - 1);
            }
        }
    )";
    for (auto _ : state)
    {
        auto tokens = tokenize(S, code, "benchmark");
        benchmark::DoNotOptimize(tokens);
    }

    behl::close(S);
}
BENCHMARK(BM_Lexer_FunctionDefinition)->Unit(benchmark::kMillisecond);

static void BM_Lexer_LoopWithTable(benchmark::State& state)
{
    auto* S = behl::new_state();

    std::string_view code = R"(
        let t = {1, 2, 3, 4, 5};
        let sum = 0;
        for (let i = 0; i < 5; i++) {
            sum += t[i];
        }
    )";
    for (auto _ : state)
    {
        auto tokens = tokenize(S, code, "benchmark");
        benchmark::DoNotOptimize(tokens);
    }

    behl::close(S);
}
BENCHMARK(BM_Lexer_LoopWithTable)->Unit(benchmark::kMillisecond);

static void BM_Lexer_ComplexNested(benchmark::State& state)
{
    auto* S = behl::new_state();

    std::string_view code = R"(
        function process(data) {
            let result = {};
            for (let i = 0; i < 100; i++) {
                if (data[i] > 50) {
                    result[i] = data[i] * 2;
                } else {
                    result[i] = data[i] / 2;
                }
            }
            return result;
        }
    )";
    for (auto _ : state)
    {
        auto tokens = tokenize(S, code, "benchmark");
        benchmark::DoNotOptimize(tokens);
    }

    behl::close(S);
}
BENCHMARK(BM_Lexer_ComplexNested)->Unit(benchmark::kMillisecond);

static void BM_Lexer_StringLiterals(benchmark::State& state)
{
    auto* S = behl::new_state();

    std::string_view code = R"(
        let name = "John Doe";
        let message = "Hello, " + name + "!";
        let multiline = "This is a\nlong string\nwith many\nlines";
    )";
    for (auto _ : state)
    {
        auto tokens = tokenize(S, code, "benchmark");
        benchmark::DoNotOptimize(tokens);
    }

    behl::close(S);
}
BENCHMARK(BM_Lexer_StringLiterals)->Unit(benchmark::kMillisecond);

static void BM_Lexer_HexadecimalLiterals(benchmark::State& state)
{
    auto* S = behl::new_state();

    std::string_view code = "let a = 0xFF; let b = 0x1234ABCD; let c = 0xDEADBEEF;";
    for (auto _ : state)
    {
        auto tokens = tokenize(S, code, "benchmark");
        benchmark::DoNotOptimize(tokens);
    }

    behl::close(S);
}
BENCHMARK(BM_Lexer_HexadecimalLiterals)->Unit(benchmark::kMillisecond);

static void BM_Lexer_Operators(benchmark::State& state)
{
    auto* S = behl::new_state();

    std::string_view code = "let x = a + b - c * d / e % f ** g & h | i ^ j << k >> l;";
    for (auto _ : state)
    {
        auto tokens = tokenize(S, code, "benchmark");
        benchmark::DoNotOptimize(tokens);
    }

    behl::close(S);
}
BENCHMARK(BM_Lexer_Operators)->Unit(benchmark::kMillisecond);
