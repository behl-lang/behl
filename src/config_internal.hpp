
#pragma once

#include <cstddef>
#include <cstdint>

namespace behl
{
#ifndef NDEBUG
    static constexpr bool kDebugMode = true;
#else
    static constexpr bool kDebugMode = false;
#endif

    // VM Configuration
    static constexpr size_t kMaxNestedProtos = 0xFFFFFFFF;

    static constexpr size_t kMaxRegisters = 255;

    static constexpr size_t kMaxUpvalues = 255;

    static constexpr size_t kMaxConstants = 0xFFFFFFFF;

    static constexpr uint32_t kMinRegisterPrealloc = 8;

    static constexpr size_t kTableArrayGrowthLimit = 64;

    // GC Configuration
    static constexpr size_t kGCInitialThreshold = 4096;

    static constexpr size_t kGCStepSize = 16;

    static constexpr size_t kGCBytesPerWorkUnit = 1024;

    static constexpr size_t kGCMinimumPoolLimit = 256;

    static constexpr size_t kGCMaximumPoolLimit = 4096;

    static constexpr size_t kGCPoolStepSize = 128;

    static constexpr bool kGCLoggingEnabled = false;

    static constexpr bool kGCEnableValidation = false;

    // Optimization Configuration
    static constexpr bool kOptimizationPassTiming = false;

    static constexpr bool kOptimizationPassDebug = false;

} // namespace behl
