#include "common/charconv.hpp"

#include <benchmark/benchmark.h>
#include <bit>
#include <charconv>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace
{
    std::vector<double> typical_values()
    {
        std::mt19937_64 rng(1234);
        std::vector<double> out;
        out.reserve(4096);
        for (int i = 0; i < 4096; ++i)
        {
            out.push_back(static_cast<double>(static_cast<int64_t>(rng() % 1000000)) / 100.0);
        }
        return out;
    }

    std::vector<double> hard_values()
    {
        std::mt19937_64 rng(5678);
        std::vector<double> out;
        out.reserve(4096);
        while (out.size() < 4096)
        {
            const double v = std::bit_cast<double>(rng());
            if (v == v && v * 0.0 == 0.0)
            {
                out.push_back(v);
            }
        }
        return out;
    }

    std::vector<std::string> as_text(const std::vector<double>& values)
    {
        std::vector<std::string> out;
        out.reserve(values.size());
        for (const double v : values)
        {
            char buf[64];
            const auto r = std::to_chars(buf, buf + sizeof(buf), v);
            out.emplace_back(buf, r.ptr);
        }
        return out;
    }
} // namespace

static void BM_FromChars_Std_Typical(benchmark::State& state)
{
    const auto text = as_text(typical_values());
    for (auto _ : state)
    {
        for (const auto& s : text)
        {
            double d = 0.0;
            std::from_chars(s.data(), s.data() + s.size(), d);
            benchmark::DoNotOptimize(d);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(text.size()));
}
BENCHMARK(BM_FromChars_Std_Typical);

static void BM_FromChars_Behl_Typical(benchmark::State& state)
{
    const auto text = as_text(typical_values());
    for (auto _ : state)
    {
        for (const auto& s : text)
        {
            double d = 0.0;
            behl::from_chars(s.data(), s.data() + s.size(), d);
            benchmark::DoNotOptimize(d);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(text.size()));
}
BENCHMARK(BM_FromChars_Behl_Typical);

static void BM_FromChars_Std_Hard(benchmark::State& state)
{
    const auto text = as_text(hard_values());
    for (auto _ : state)
    {
        for (const auto& s : text)
        {
            double d = 0.0;
            std::from_chars(s.data(), s.data() + s.size(), d);
            benchmark::DoNotOptimize(d);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(text.size()));
}
BENCHMARK(BM_FromChars_Std_Hard);

static void BM_FromChars_Behl_Hard(benchmark::State& state)
{
    const auto text = as_text(hard_values());
    for (auto _ : state)
    {
        for (const auto& s : text)
        {
            double d = 0.0;
            behl::from_chars(s.data(), s.data() + s.size(), d);
            benchmark::DoNotOptimize(d);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(text.size()));
}
BENCHMARK(BM_FromChars_Behl_Hard);

static void BM_ToChars_Std_Typical(benchmark::State& state)
{
    const auto values = typical_values();
    char buf[64];
    for (auto _ : state)
    {
        for (const double v : values)
        {
            const auto r = std::to_chars(buf, buf + sizeof(buf), v);
            benchmark::DoNotOptimize(r.ptr);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}
BENCHMARK(BM_ToChars_Std_Typical);

static void BM_ToChars_Behl_Typical(benchmark::State& state)
{
    const auto values = typical_values();
    char buf[64];
    for (auto _ : state)
    {
        for (const double v : values)
        {
            const auto r = behl::to_chars(buf, buf + sizeof(buf), v);
            benchmark::DoNotOptimize(r.ptr);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}
BENCHMARK(BM_ToChars_Behl_Typical);

static void BM_ToChars_Std_Hard(benchmark::State& state)
{
    const auto values = hard_values();
    char buf[64];
    for (auto _ : state)
    {
        for (const double v : values)
        {
            const auto r = std::to_chars(buf, buf + sizeof(buf), v);
            benchmark::DoNotOptimize(r.ptr);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}
BENCHMARK(BM_ToChars_Std_Hard);

static void BM_ToChars_Behl_Hard(benchmark::State& state)
{
    const auto values = hard_values();
    char buf[64];
    for (auto _ : state)
    {
        for (const double v : values)
        {
            const auto r = behl::to_chars(buf, buf + sizeof(buf), v);
            benchmark::DoNotOptimize(r.ptr);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}
BENCHMARK(BM_ToChars_Behl_Hard);
