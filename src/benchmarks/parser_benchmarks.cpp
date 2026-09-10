#include "ast/ast.hpp"
#include "behl/behl.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"

#include <benchmark/benchmark.h>
#include <string>

using namespace behl;

static void BM_Parser_SimpleArithmetic(benchmark::State& state)
{
    std::string_view code = "let x = 10 + 20 * 30 - 5 / 2;";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        benchmark::DoNotOptimize(S);
        auto ast = parse(holder, tokens, "benchmark");
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Parser_SimpleArithmetic)->Unit(benchmark::kMillisecond);

static void BM_Parser_FunctionDefinition(benchmark::State& state)
{
    std::string_view code = R"(
        function factorial(n) {
            if (n <= 1) {
                return 1;
            } else {
                return n * factorial(n - 1);
            }
        }
    )";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        benchmark::DoNotOptimize(S);
        auto ast = parse(holder, tokens, "benchmark");
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Parser_FunctionDefinition)->Unit(benchmark::kMillisecond);

static void BM_Parser_LoopWithTable(benchmark::State& state)
{
    std::string_view code = R"(
        let t = {1, 2, 3, 4, 5};
        let sum = 0;
        for (let i = 0; i < 5; i++) {
            sum += t[i];
        }
    )";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        benchmark::DoNotOptimize(S);
        auto ast = parse(holder, tokens, "benchmark");
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Parser_LoopWithTable)->Unit(benchmark::kMillisecond);

static void BM_Parser_ComplexNested(benchmark::State& state)
{
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
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        benchmark::DoNotOptimize(S);
        auto ast = parse(holder, tokens, "benchmark");
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Parser_ComplexNested)->Unit(benchmark::kMillisecond);

static void BM_Parser_Closures(benchmark::State& state)
{
    std::string_view code = R"(
        function makeCounter() {
            let count = 0;
            return function() {
                count++;
                return count;
            };
        }
    )";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        benchmark::DoNotOptimize(S);
        auto ast = parse(holder, tokens, "benchmark");
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Parser_Closures)->Unit(benchmark::kMillisecond);

static void BM_Parser_OperatorPrecedence(benchmark::State& state)
{
    std::string_view code = "let x = 2 ** 3 ** 2 + 5 * 3 - 10 / 2 % 3;";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        benchmark::DoNotOptimize(S);
        auto ast = parse(holder, tokens, "benchmark");
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Parser_OperatorPrecedence)->Unit(benchmark::kMillisecond);

static void BM_Parser_TableConstructors(benchmark::State& state)
{
    std::string_view code = R"(
        let t = {
            name = "test",
            value = 42,
            nested = {a = 1, b = 2, c = 3},
            array = {10, 20, 30, 40, 50}
        };
    )";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        benchmark::DoNotOptimize(S);
        auto ast = parse(holder, tokens, "benchmark");
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Parser_TableConstructors)->Unit(benchmark::kMillisecond);
