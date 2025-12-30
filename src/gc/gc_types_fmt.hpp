#pragma once

#include "gc_types.hpp"

#include <string_view>

namespace behl
{
    inline std::string_view to_string(GCType gc_type)
    {
        switch (gc_type)
        {
            case GCType::kDead:
                return "Dead";
            case GCType::kTable:
                return "Table";
            case GCType::kString:
                return "String";
            case GCType::kClosure:
                return "Closure";
            case GCType::kProto:
                return "Proto";
            case GCType::kUserdata:
                return "Userdata";
            default:
                return "Unknown";
        }
    }

    inline std::string_view to_string(GCColor color)
    {
        switch (color)
        {
            case GCColor::kWhite:
                return "White";
            case GCColor::kGray:
                return "Gray";
            case GCColor::kBlack:
                return "Black";
            case GCColor::kFree:
                return "Free";
            default:
                return "Unknown";
        }
    }

    inline std::string_view to_string(GCPhase phase)
    {
        switch (phase)
        {
            case GCPhase::kIdle:
                return "Idle";
            case GCPhase::kMark:
                return "Mark";
            case GCPhase::kSweep:
                return "Sweep";
            case GCPhase::kFinalize:
                return "Finalize";
            default:
                return "Unknown";
        }
    }
} // namespace behl
