#include "common/format.hpp"

#include <benchmark/benchmark.h>
#include <charconv>
#include <format>
#include <string>

static void BM_std_double_to_string(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::to_string(3.14159);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_double_to_string);

// Simple string with single argument
static void BM_behl_format_simple_int(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("Value: {}", 42);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_simple_int);

static void BM_std_format_simple_int(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("Value: {}", 42);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_simple_int);

// Multiple integers
static void BM_behl_format_multi_int(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("x={}, y={}, z={}", 10, 20, 30);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_multi_int);

static void BM_std_format_multi_int(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("x={}, y={}, z={}", 10, 20, 30);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_multi_int);

// Floating point
static void BM_behl_format_float(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("Pi: {}", 3.14159);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_float);

static void BM_std_format_float(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("Pi: {}", 3.14159);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_float);

// Floating point with precision
static void BM_behl_format_float_precision(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("Pi: {:.2f}", 3.14159);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_float_precision);

static void BM_std_format_float_precision(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("Pi: {:.2f}", 3.14159);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_float_precision);

// String formatting
static void BM_behl_format_string(benchmark::State& state)
{
    const char* str = "Hello, World!";
    for (auto _ : state)
    {
        auto result = behl::format("Message: {}", str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_string);

static void BM_std_format_string(benchmark::State& state)
{
    const char* str = "Hello, World!";
    for (auto _ : state)
    {
        auto result = std::format("Message: {}", str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_string);

// Mixed types
static void BM_behl_format_mixed(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("Name: {}, Age: {}, Score: {}", "Alice", 30, 95.5);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_mixed);

static void BM_std_format_mixed(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("Name: {}, Age: {}, Score: {}", "Alice", 30, 95.5);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_mixed);

// Hex formatting
static void BM_behl_format_hex(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("Address: 0x{:x}", 0xDEADBEEF);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_hex);

static void BM_std_format_hex(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("Address: 0x{:x}", 0xDEADBEEF);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_hex);

// Complex format string
static void BM_behl_format_complex(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format(
            "User {{id={}, name=\"{}\", balance={:.2f}, active={}, hex=0x{:X}}}", 12345, "John Doe", 1234.56, true, 0xABCD);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_complex);

static void BM_std_format_complex(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format(
            "User {{id={}, name=\"{}\", balance={:.2f}, active={}, hex=0x{:X}}}", 12345, "John Doe", 1234.56, true, 0xABCD);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_complex);

// Many arguments
static void BM_behl_format_many_args(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("{} {} {} {} {} {} {} {}", 1, 2, 3, 4, 5, 6, 7, 8);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_many_args);

static void BM_std_format_many_args(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("{} {} {} {} {} {} {} {}", 1, 2, 3, 4, 5, 6, 7, 8);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_many_args);

// Plain text (no formatting)
static void BM_behl_format_plain_text(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = behl::format("This is just plain text with no placeholders");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_plain_text);

static void BM_std_format_plain_text(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = std::format("This is just plain text with no placeholders");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_plain_text);

// Pointer formatting
static void BM_behl_format_pointer(benchmark::State& state)
{
    void* ptr = reinterpret_cast<void*>(0x12345678);
    for (auto _ : state)
    {
        auto result = behl::format("Pointer: {}", ptr);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_behl_format_pointer);

static void BM_std_format_pointer(benchmark::State& state)
{
    void* ptr = reinterpret_cast<void*>(0x12345678);
    for (auto _ : state)
    {
        auto result = std::format("Pointer: {}", ptr);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_std_format_pointer);
