#include "common/format.hpp"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<format>)
#    include <format>
#endif

#if defined(__cpp_lib_format)
#    define BEHL_BENCH_STD_FORMAT 1
#else
#    define BEHL_BENCH_STD_FORMAT 0
#endif

namespace
{
    constexpr size_t kCount = 1024;

    struct Inputs
    {
        std::vector<long long> ints;
        std::vector<double> doubles;
        std::vector<std::string> names;
        std::vector<std::vector<behl::format_arg>> packed;
    };

    const Inputs& inputs()
    {
        static const Inputs data = [] {
            Inputs in;
            std::mt19937_64 rng(4242);
            std::uniform_int_distribution<long long> int_dist(-1000000000LL, 1000000000LL);
            std::uniform_real_distribution<double> double_dist(-10000.0, 10000.0);
            std::uniform_int_distribution<int> len_dist(4, 12);
            std::uniform_int_distribution<int> char_dist('a', 'z');

            in.ints.reserve(kCount);
            in.doubles.reserve(kCount);
            in.names.reserve(kCount);
            for (size_t i = 0; i < kCount; ++i)
            {
                in.ints.push_back(int_dist(rng));
                in.doubles.push_back(double_dist(rng));
                std::string name(static_cast<size_t>(len_dist(rng)), ' ');
                for (char& c : name)
                {
                    c = static_cast<char>(char_dist(rng));
                }
                in.names.push_back(std::move(name));
            }

            in.packed.reserve(kCount);
            for (size_t i = 0; i < kCount; ++i)
            {
                in.packed.push_back({ in.ints[i], std::string_view(in.names[i]), in.doubles[i] });
            }
            return in;
        }();
        return data;
    }

    template<typename Fn>
    void run(benchmark::State& state, Fn&& fn)
    {
        const Inputs& in = inputs();
        for (auto _ : state)
        {
            for (size_t i = 0; i < kCount; ++i)
            {
                std::string text = fn(in, i);
                benchmark::DoNotOptimize(text);
            }
        }
        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kCount));
    }
} // namespace

static void BM_Format_Integers_BehlStatic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) {
        return behl::format<"{} {} {}">(in.ints[i], in.ints[(i + 1) % kCount], in.ints[(i + 2) % kCount]);
    });
}
BENCHMARK(BM_Format_Integers_BehlStatic);

static void BM_Format_Integers_BehlDynamic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) {
        return behl::format("{} {} {}", in.ints[i], in.ints[(i + 1) % kCount], in.ints[(i + 2) % kCount]);
    });
}
BENCHMARK(BM_Format_Integers_BehlDynamic);

static void BM_Format_MixedText_BehlStatic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) {
        return behl::format<"Player {} scored {} points in round {}">(in.names[i], in.ints[i], i);
    });
}
BENCHMARK(BM_Format_MixedText_BehlStatic);

static void BM_Format_MixedText_BehlDynamic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) {
        return behl::format("Player {} scored {} points in round {}", in.names[i], in.ints[i], i);
    });
}
BENCHMARK(BM_Format_MixedText_BehlDynamic);

static void BM_Format_Aligned_BehlStatic(benchmark::State& state)
{
    run(state,
        [](const Inputs& in, size_t i) { return behl::format<"[{:>14}] [{:<12}] [{:^9}]">(in.names[i], in.ints[i], i); });
}
BENCHMARK(BM_Format_Aligned_BehlStatic);

static void BM_Format_Aligned_BehlDynamic(benchmark::State& state)
{
    run(state,
        [](const Inputs& in, size_t i) { return behl::format("[{:>14}] [{:<12}] [{:^9}]", in.names[i], in.ints[i], i); });
}
BENCHMARK(BM_Format_Aligned_BehlDynamic);

static void BM_Format_Hex_BehlStatic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return behl::format<"{:x} {:X}">(in.ints[i], in.ints[(i + 7) % kCount]); });
}
BENCHMARK(BM_Format_Hex_BehlStatic);

static void BM_Format_Hex_BehlDynamic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return behl::format("{:x} {:X}", in.ints[i], in.ints[(i + 7) % kCount]); });
}
BENCHMARK(BM_Format_Hex_BehlDynamic);

static void BM_Format_FixedDouble_BehlStatic(benchmark::State& state)
{
    run(state,
        [](const Inputs& in, size_t i) { return behl::format<"{:.2f} {:.5f}">(in.doubles[i], in.doubles[(i + 3) % kCount]); });
}
BENCHMARK(BM_Format_FixedDouble_BehlStatic);

static void BM_Format_FixedDouble_BehlDynamic(benchmark::State& state)
{
    run(state,
        [](const Inputs& in, size_t i) { return behl::format("{:.2f} {:.5f}", in.doubles[i], in.doubles[(i + 3) % kCount]); });
}
BENCHMARK(BM_Format_FixedDouble_BehlDynamic);

static void BM_Format_ShortestDouble_BehlStatic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return behl::format<"{}">(in.doubles[i]); });
}
BENCHMARK(BM_Format_ShortestDouble_BehlStatic);

static void BM_Format_ShortestDouble_BehlDynamic(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return behl::format("{}", in.doubles[i]); });
}
BENCHMARK(BM_Format_ShortestDouble_BehlDynamic);

static void BM_Format_Runtime_BehlVformat(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return behl::vformat("id={} name={:>8} value={:.3f}", in.packed[i]); });
}
BENCHMARK(BM_Format_Runtime_BehlVformat);

#if BEHL_BENCH_STD_FORMAT

static void BM_Format_Integers_Std(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) {
        return std::format("{} {} {}", in.ints[i], in.ints[(i + 1) % kCount], in.ints[(i + 2) % kCount]);
    });
}
BENCHMARK(BM_Format_Integers_Std);

static void BM_Format_MixedText_Std(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) {
        return std::format("Player {} scored {} points in round {}", in.names[i], in.ints[i], i);
    });
}
BENCHMARK(BM_Format_MixedText_Std);

static void BM_Format_Aligned_Std(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return std::format("[{:>14}] [{:<12}] [{:^9}]", in.names[i], in.ints[i], i); });
}
BENCHMARK(BM_Format_Aligned_Std);

static void BM_Format_Hex_Std(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return std::format("{:x} {:X}", in.ints[i], in.ints[(i + 7) % kCount]); });
}
BENCHMARK(BM_Format_Hex_Std);

static void BM_Format_FixedDouble_Std(benchmark::State& state)
{
    run(state,
        [](const Inputs& in, size_t i) { return std::format("{:.2f} {:.5f}", in.doubles[i], in.doubles[(i + 3) % kCount]); });
}
BENCHMARK(BM_Format_FixedDouble_Std);

static void BM_Format_ShortestDouble_Std(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) { return std::format("{}", in.doubles[i]); });
}
BENCHMARK(BM_Format_ShortestDouble_Std);

static void BM_Format_Runtime_StdVformat(benchmark::State& state)
{
    run(state, [](const Inputs& in, size_t i) {
        const long long id = in.ints[i];
        const std::string_view name = in.names[i];
        const double value = in.doubles[i];
        return std::vformat("id={} name={:>8} value={:.3f}", std::make_format_args(id, name, value));
    });
}
BENCHMARK(BM_Format_Runtime_StdVformat);

#endif
