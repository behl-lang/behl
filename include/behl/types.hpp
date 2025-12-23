#pragma once

#include <behl/config.hpp>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

namespace behl
{
    struct State;

    using CFunction = int (*)(State* S);
    using PrintHandler = void (*)(State* S, std::string_view msg);

    // Value type categories.
    inline constexpr uint8_t kTableLikeTypeBit = 1u << 4;
    inline constexpr uint8_t kNumericTypeBit = 1u << 5;
    inline constexpr uint8_t kGCTypeBit = 1u << 6;
    inline constexpr uint8_t kCallableTypeBit = 1u << 7;

    enum class Type : uint8_t
    {
        kNil = 0,

        kBoolean = 2,
        kInteger = kNumericTypeBit | 3,
        kNumber = kNumericTypeBit | 4,

        kString = kGCTypeBit | 5,
        kTable = kGCTypeBit | kTableLikeTypeBit | 6,
        kUserdata = kGCTypeBit | kTableLikeTypeBit | 9,
        kClosure = kGCTypeBit | kCallableTypeBit | 7,
        kCFunction = kCallableTypeBit | 8,

        // Internal type, this never reaches the public API.
        kNullOpt = 1,
    };

    // If the amount of returned values is variable, this should be passed as the nresults parameter when performing calls.
    constexpr int kMultRet = -1;

    // If the amount of arguments is variable (use all values on stack up to frame.top), pass this as the nargs parameter.
    constexpr int kMultArgs = -1;

    // Module registration structure
    struct ModuleReg
    {
        std::string_view name;
        CFunction func;
    };

    // Module constant registration
    struct ModuleConst
    {
        std::string_view name;
        std::variant<Integer, FP, std::string_view, bool> value;
    };

    // Module definition containing functions and constants
    struct ModuleDef
    {
        std::span<const ModuleReg> funcs;
        std::span<const ModuleConst> consts = {}; // Optional
    };

    enum class PinHandle : int32_t
    {
        kInvalid = -1,
    };

    // Helper to generate a UID from a string name (FNV-1a hash)
    constexpr uint32_t make_uid(std::string_view name)
    {
        uint32_t hash = 2166136261u;
        for (char c : name)
        {
            hash ^= static_cast<uint32_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

} // namespace behl
