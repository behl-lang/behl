#pragma once

#include <cstdint>

namespace behl
{
    enum class GCPhase : uint8_t
    {
        kIdle,
        kMark,
        kSweep,
        kFinalize // Call __gc finalizers after sweep
    };

    enum class GCColor : uint8_t
    {
        kWhite,
        kGray,
        kBlack,
        kFree,
    };

    enum class GCType : uint8_t
    {
        kDead,
        kTable,
        kString,
        kClosure,
        kProto,
        kUserdata,
    };

} // namespace behl