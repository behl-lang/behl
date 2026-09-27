#pragma once

#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__)
#    define BEHL_JIT_X86_64 1
#    define BEHL_JIT_X86_32 0
#elif defined(_M_IX86) || defined(__i386__)
#    define BEHL_JIT_X86_64 0
#    define BEHL_JIT_X86_32 1
#else
#    define BEHL_JIT_X86_64 0
#    define BEHL_JIT_X86_32 0
#endif

#define BEHL_JIT_X86 (BEHL_JIT_X86_64 || BEHL_JIT_X86_32)

#if defined(_M_ARM64) || defined(__aarch64__)
#    define BEHL_JIT_AARCH64 1
#else
#    define BEHL_JIT_AARCH64 0
#endif

#if BEHL_JIT_X86 || BEHL_JIT_AARCH64
#    define BEHL_JIT_SUPPORTED 1
#else
#    define BEHL_JIT_SUPPORTED 0
#endif

namespace behl
{
    struct State;

#if BEHL_JIT_SUPPORTED
    inline constexpr uint32_t kJitStatsMaxHelpers = 128;
    inline constexpr uint32_t kJitStatsResultCodes = 4;

    struct JitStats
    {
        uint64_t helper_calls[kJitStatsMaxHelpers]{};
        uint64_t driver_entries{};
        uint64_t driver_results[kJitStatsResultCodes]{};
        uint64_t driver_interpreter_runs{};
    };

    uint32_t jit_stats_register_helper(const char* name) noexcept;
    void jit_stats_print(State* S);
#endif

} // namespace behl
