#include "ast/ast.hpp"
#include "backend/compiler.hpp"
#include "behl/behl.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/semantics_pass.hpp"
#include "gc/gco_proto.hpp"
#include "jit/jit.hpp"
#include "state.hpp"

#include <benchmark/benchmark.h>
#include <string>
#include <string_view>

using namespace behl;

namespace
{
    void run_codegen(benchmark::State& state, std::string_view code, bool nested)
    {
        if (!jit_supported())
        {
            state.SkipWithError("jit is not supported on this target");
            return;
        }

        State* S = new_state();
        {
            auto tokens = tokenize(S, code, "benchmark");
            AstHolder holder(S);
            auto ast = parse(holder, tokens, "benchmark");
            ast = SemanticsPass::apply(S, holder, ast);

            const GCProto* proto = compile(S, ast, "benchmark");
            if (nested)
            {
                proto = proto->protos[0];
            }

            JitEntry probe = jit_compile(S, proto);
            if (probe == nullptr)
            {
                state.SkipWithError("proto was declined by the jit compiler");
            }
            else
            {
                jit_release(S, probe);

                for (auto _ : state)
                {
                    JitEntry entry = jit_compile(S, proto);
                    benchmark::DoNotOptimize(entry);
                    jit_release(S, entry);
                }

                const auto instructions = static_cast<double>(proto->code.size());
                state.counters["bytecodes"] = instructions;
                state.counters["bytecodes/s"] =
                    benchmark::Counter(static_cast<double>(state.iterations()) * instructions, benchmark::Counter::kIsRate);
            }
        }
        close(S);
    }

    std::string make_straight_line(int statements)
    {
        std::string code = "function generated(seed) {\n    let acc = seed;\n";
        for (int i = 0; i < statements; i++)
        {
            code += "    acc = acc + " + std::to_string(i) + " * 3 - 1;\n";
        }
        code += "    return acc;\n}\n";
        return code;
    }
} // namespace

static void BM_Codegen_IntegerArithmetic(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function work(n) {
            let acc = 0;
            for (let i = 0; i < n; i++) {
                acc = acc + i * 3;
                acc = acc - i / 2;
                acc = acc % 1000;
                acc = acc + (i << 2);
            }
            return acc;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_IntegerArithmetic)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_FloatArithmetic(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function work(n) {
            let acc = 0.5;
            for (let i = 0; i < n; i++) {
                acc = acc + 1.25;
                acc = acc * 0.5;
                acc = acc - 0.125;
                acc = acc / 1.5;
            }
            return acc;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_FloatArithmetic)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_Bitwise(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function work(a, b) {
            let acc = a & b;
            acc = acc | (a ^ b);
            acc = acc << 3;
            acc = acc >> 1;
            acc = ~acc;
            acc = acc & 0xFFFF;
            return acc;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_Bitwise)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_ComparisonBranches(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function classify(x, y) {
            if (x < y) {
                return -1;
            } else if (x > y) {
                return 1;
            } else if (x == 0 && y == 0) {
                return 2;
            } else if (x <= 10 || y >= 20) {
                return 3;
            }
            return 0;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_ComparisonBranches)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_RecursiveCalls(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function fib(n) {
            if (n < 2) {
                return n;
            }
            return fib(n - 1) + fib(n - 2);
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_RecursiveCalls)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_TableAccess(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function work(t, n) {
            let total = 0;
            for (let i = 0; i < n; i++) {
                t[i] = i * 2;
                total = total + t[i];
            }
            t.name = "done";
            total = total + t.count;
            return total;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_TableAccess)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_TableConstruction(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function build(a, b) {
            let t = {
                name = "test",
                value = a,
                nested = {x = 1, y = 2, z = b},
                array = {10, 20, 30, 40, 50}
            };
            return t;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_TableConstruction)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_GlobalsAndUpvalues(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        let counter = 0;
        function outer(step) {
            let local_total = 0;
            function inner() {
                counter = counter + step;
                local_total = local_total + counter;
                return local_total;
            }
            inner();
            inner();
            return local_total + counter;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_GlobalsAndUpvalues)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_StringOperations(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function describe(name, n) {
            let out = "item:" + name;
            out = out + "-" + tostring(n);
            let size = #out;
            out = out + tostring(size);
            return out;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_StringOperations)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_MixedWorkload(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        function process(data, n) {
            let result = {};
            let total = 0;
            for (let i = 0; i < n; i++) {
                let v = data[i];
                if (v > 50) {
                    result[i] = v * 2;
                } else if (v < 0) {
                    result[i] = -v;
                } else {
                    result[i] = v / 2 + 1.5;
                }
                total = total + result[i];
            }
            result.total = total;
            result.label = "n=" + tostring(n);
            return result;
        }
    )";
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_MixedWorkload)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_TopLevelScript(benchmark::State& state)
{
    constexpr std::string_view code = R"(
        let t = {1, 2, 3, 4, 5};
        let sum = 0;
        for (let i = 0; i < 5; i++) {
            sum += t[i];
        }
        let scaled = sum * 2 - 1;
        return scaled;
    )";
    run_codegen(state, code, false);
}
BENCHMARK(BM_Codegen_TopLevelScript)->Unit(benchmark::kMicrosecond);

static void BM_Codegen_StraightLineScaling(benchmark::State& state)
{
    const std::string code = make_straight_line(static_cast<int>(state.range(0)));
    run_codegen(state, code, true);
}
BENCHMARK(BM_Codegen_StraightLineScaling)->RangeMultiplier(8)->Range(8, 4096)->Unit(benchmark::kMicrosecond);
