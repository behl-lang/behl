#include "ast/ast.hpp"
#include "backend/compiler.hpp"
#include "behl/behl.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/semantics_pass.hpp"
#include "state.hpp"

#include <benchmark/benchmark.h>
#include <string>

using namespace behl;

// Benchmark compiling a simple arithmetic expression
static void BM_Compiler_SimpleArithmetic(benchmark::State& state)
{
    std::string_view code = "let x = 10 + 20 * 30 - 5 / 2;";
    State* S = new_state();
    {
        auto tokens = tokenize(S, code, "benchmark");
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");
        ast = SemanticsPass::apply(S, holder, ast);
        for (auto _ : state)
        {
            state.PauseTiming();
            behl::gc_collect(S);
            state.ResumeTiming();

            auto proto = compile(S, ast, "benchmark");
            benchmark::DoNotOptimize(proto);
        }
    }
    close(S);
}
BENCHMARK(BM_Compiler_SimpleArithmetic)->Unit(benchmark::kMillisecond);

// Benchmark compiling a function definition
static void BM_Compiler_FunctionDefinition(benchmark::State& state)
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
    State* S = new_state();
    {
        auto tokens = tokenize(S, code, "benchmark");
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");
        ast = SemanticsPass::apply(S, holder, ast);
        for (auto _ : state)
        {
            state.PauseTiming();
            behl::gc_collect(S);
            state.ResumeTiming();

            auto proto = compile(S, ast, "benchmark");
            benchmark::DoNotOptimize(proto);
        }
    }
    close(S);
}
BENCHMARK(BM_Compiler_FunctionDefinition)->Unit(benchmark::kMillisecond);

// Benchmark compiling a loop with table operations
static void BM_Compiler_LoopWithTable(benchmark::State& state)
{
    std::string_view code = R"(
        let t = {1, 2, 3, 4, 5};
        let sum = 0;
        for (let i = 0; i < 5; i++) {
            sum += t[i];
        }
    )";
    State* S = new_state();
    {
        auto tokens = tokenize(S, code, "benchmark");
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");
        ast = SemanticsPass::apply(S, holder, ast);
        for (auto _ : state)
        {
            state.PauseTiming();
            behl::gc_collect(S);
            state.ResumeTiming();

            auto proto = compile(S, ast, "benchmark");
            benchmark::DoNotOptimize(proto);
        }
    }
    close(S);
}
BENCHMARK(BM_Compiler_LoopWithTable)->Unit(benchmark::kMillisecond);

// Benchmark compiling nested functions with closures
static void BM_Compiler_NestedFunctionsWithClosures(benchmark::State& state)
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
    State* S = new_state();
    {
        auto tokens = tokenize(S, code, "benchmark");
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");
        ast = SemanticsPass::apply(S, holder, ast);
        for (auto _ : state)
        {
            state.PauseTiming();
            behl::gc_collect(S);
            state.ResumeTiming();

            auto proto = compile(S, ast, "benchmark");
            benchmark::DoNotOptimize(proto);
        }
    }
    close(S);
}
BENCHMARK(BM_Compiler_NestedFunctionsWithClosures)->Unit(benchmark::kMillisecond);

// Benchmark compiling complex nested structure
static void BM_Compiler_ComplexNested(benchmark::State& state)
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
    State* S = new_state();
    {
        auto tokens = tokenize(S, code, "benchmark");
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");
        ast = SemanticsPass::apply(S, holder, ast);
        for (auto _ : state)
        {
            state.PauseTiming();
            behl::gc_collect(S);
            state.ResumeTiming();

            auto proto = compile(S, ast, "benchmark");
            benchmark::DoNotOptimize(proto);
        }
    }
    close(S);
}
BENCHMARK(BM_Compiler_ComplexNested)->Unit(benchmark::kMillisecond);

// Benchmark compiling power operator expressions
static void BM_Compiler_PowerOperator(benchmark::State& state)
{
    std::string_view code = "let x = 2 ** 3 ** 2;";
    State* S = new_state();
    {
        auto tokens = tokenize(S, code, "benchmark");
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");
        ast = SemanticsPass::apply(S, holder, ast);
        for (auto _ : state)
        {
            state.PauseTiming();
            behl::gc_collect(S);
            state.ResumeTiming();

            auto proto = compile(S, ast, "benchmark");
            benchmark::DoNotOptimize(proto);
        }
    }
    close(S);
}
BENCHMARK(BM_Compiler_PowerOperator)->Unit(benchmark::kMillisecond);

// Benchmark compiling table constructors
static void BM_Compiler_TableConstructors(benchmark::State& state)
{
    std::string_view code = R"(
        let t = {
            name = "test",
            value = 42,
            nested = {a = 1, b = 2, c = 3},
            array = {10, 20, 30, 40, 50}
        };
    )";
    State* S = new_state();
    {
        auto tokens = tokenize(S, code, "benchmark");
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");
        ast = SemanticsPass::apply(S, holder, ast);
        for (auto _ : state)
        {
            state.PauseTiming();
            behl::gc_collect(S);
            state.ResumeTiming();

            auto proto = compile(S, ast, "benchmark");
            benchmark::DoNotOptimize(proto);
        }
    }
    close(S);
}
BENCHMARK(BM_Compiler_TableConstructors)->Unit(benchmark::kMillisecond);
