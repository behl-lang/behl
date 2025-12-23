#pragma once

#include "common/format.hpp"
#include "gc_types.hpp"

namespace behl
{

    template<>
    struct formatter<GCType>
    {
        void format(const GCType& gc_type, std::string& out, [[maybe_unused]] const format_spec& spec) const
        {
            const char* type_str;
            switch (gc_type)
            {
                case GCType::kDead:
                    type_str = "Dead";
                    break;
                case GCType::kTable:
                    type_str = "Table";
                    break;
                case GCType::kString:
                    type_str = "String";
                    break;
                case GCType::kClosure:
                    type_str = "Closure";
                    break;
                case GCType::kProto:
                    type_str = "Proto";
                    break;
                case GCType::kUserdata:
                    type_str = "Userdata";
                    break;
                default:
                    type_str = "Unknown";
                    break;
            }
            out += type_str;
        }
    };

    template<>
    struct formatter<GCColor>
    {
        void format(const GCColor& gc_color, std::string& out, [[maybe_unused]] const format_spec& spec) const
        {
            const char* color_str;
            switch (gc_color)
            {
                case GCColor::kWhite:
                    color_str = "White";
                    break;
                case GCColor::kGray:
                    color_str = "Gray";
                    break;
                case GCColor::kBlack:
                    color_str = "Black";
                    break;
                case GCColor::kFree:
                    color_str = "Free";
                    break;
                default:
                    color_str = "Unknown";
                    break;
            }
            out += color_str;
        }
    };

    template<>
    struct formatter<GCPhase>
    {
        void format(const GCPhase& gc_phase, std::string& out, [[maybe_unused]] const format_spec& spec) const
        {
            const char* phase_str;
            switch (gc_phase)
            {
                case GCPhase::kIdle:
                    phase_str = "Idle";
                    break;
                case GCPhase::kMark:
                    phase_str = "Mark";
                    break;
                case GCPhase::kSweep:
                    phase_str = "Sweep";
                    break;
                case GCPhase::kFinalize:
                    phase_str = "Finalize";
                    break;
                default:
                    phase_str = "Unknown";
                    break;
            }
            out += phase_str;
        }
    };

} // namespace behl
