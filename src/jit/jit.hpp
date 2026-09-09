#pragma once

#include "gc/gco_proto.hpp"
#include "platform.hpp"
#include "state.hpp"

#include <cstddef>
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

    using JitEntry = uint32_t(BEHL_CALLCONV*)(State* S);

    constexpr uint32_t kJitResultOk = 0;
    constexpr uint32_t kJitResultError = 1;
    constexpr uint32_t kJitResultTailCall = 2;
    // A script frame was pushed by a call; the driver runs it and then re-enters
    // this function at the saved pc instead of nesting a C frame per call.
    constexpr uint32_t kJitResultCall = 3;

    constexpr uint32_t kJitError = 0xFFFFFFFFu;
    constexpr uint32_t kJitTailReplaced = 0xFFFFFFFEu;
    constexpr uint32_t kJitTailReturned = 0xFFFFFFFDu;
    constexpr uint32_t kJitCallPushed = 0xFFFFFFFCu;

    // Nesting a C frame per behl call is faster (no driver round trip) but eats
    // the C stack, so only do it while the call stack is shallow. Past this the
    // call helper hands the frame to the driver, which runs it with a flat stack.
    constexpr size_t kJitNestLimit = 150;
    constexpr size_t kJitMaxCallDepth = 1000000;

    bool jit_supported() noexcept;

    JitEntry jit_compile(State* S, const GCProto* proto);

    void jit_release(State* S, JitEntry entry) noexcept;

    bool jit_run_or_compile(State* S, const GCProto* proto);

    bool jit_drive(State* S, const GCProto* proto, uint32_t entry_depth);

    void* jit_exec_alloc(State* S, size_t size);
    void jit_exec_commit(void* mem, size_t size);
    void jit_clear_cache(State* S) noexcept;
    void jit_shutdown(State* S) noexcept;

    BEHL_FORCEINLINE
    bool jit_try_execute(State* S, const GCProto* proto)
    {
#if BEHL_JIT_SUPPORTED
        if (proto->jit_declined || !S->jit_enabled)
        {
            return false;
        }
        if (S->call_stack.size() > kJitMaxCallDepth)
        {
            return false;
        }
        return jit_run_or_compile(S, proto);
#else
        (void)S;
        (void)proto;
        return false;
#endif
    }

} // namespace behl
