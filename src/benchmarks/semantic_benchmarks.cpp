#include "ast/ast.hpp"
#include "behl/behl.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/semantics_pass.hpp"

#include <benchmark/benchmark.h>
#include <string>

using namespace behl;

static void BM_Semantic_SimpleArithmetic(benchmark::State& state)
{
    const auto code = "let x = 10 + 20 * 30 - 5 / 2;";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");

        ast = SemanticsPass::apply(S, holder, ast);
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Semantic_SimpleArithmetic)->Unit(benchmark::kMillisecond);

static void BM_Semantic_FunctionDefinition(benchmark::State& state)
{
    const auto code = R"(
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
        auto ast = parse(holder, tokens, "benchmark");

        ast = SemanticsPass::apply(S, holder, ast);
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Semantic_FunctionDefinition)->Unit(benchmark::kMillisecond);

static void BM_Semantic_LoopWithLocals(benchmark::State& state)
{
    const auto code = R"(
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
        auto ast = parse(holder, tokens, "benchmark");

        ast = SemanticsPass::apply(S, holder, ast);
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Semantic_LoopWithLocals)->Unit(benchmark::kMillisecond);

static void BM_Semantic_NestedScopes(benchmark::State& state)
{
    const auto code = R"(
        function outer(x) {
            let a = x;
            function inner(y) {
                let b = y;
                return a + b;
            }
            return inner(10);
        }
    )";
    auto* S = behl::new_state();
    auto tokens = tokenize(S, code, "benchmark");
    for (auto _ : state)
    {
        AstHolder holder(S);
        auto ast = parse(holder, tokens, "benchmark");

        ast = SemanticsPass::apply(S, holder, ast);
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Semantic_NestedScopes)->Unit(benchmark::kMillisecond);

static void BM_Semantic_ClosuresWithUpvalues(benchmark::State& state)
{
    const auto code = R"(
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
        auto ast = parse(holder, tokens, "benchmark");

        ast = SemanticsPass::apply(S, holder, ast);
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Semantic_ClosuresWithUpvalues)->Unit(benchmark::kMillisecond);

static void BM_Semantic_ComplexNested(benchmark::State& state)
{
    const auto code = R"(
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
        auto ast = parse(holder, tokens, "benchmark");

        ast = SemanticsPass::apply(S, holder, ast);
        benchmark::DoNotOptimize(ast);
    }
    behl::close(S);
}
BENCHMARK(BM_Semantic_ComplexNested)->Unit(benchmark::kMillisecond);
