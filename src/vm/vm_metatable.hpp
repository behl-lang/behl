#pragma once

#include "gc/gco_userdata.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm.hpp"

#include <array>
#include <optional>

namespace behl
{

    enum class MetaMethodType
    {
        kIndex,
        kNewIndex,
        kAdd,
        kSub,
        kMul,
        kDiv,
        kMod,
        kPow,
        kUnm,
        kBNot,
        kBAnd,
        kBOr,
        kBXor,
        kBShl,
        kBShr,
        kEq,
        kLt,
        kLe,
        kCall,
        kLen,
        kToString,
        kPairs,
        kGC,
        kMax,
    };

    constexpr auto kMetatableMethodNames = std::invoke([]() {
        //
        std::array<std::string_view, static_cast<size_t>(MetaMethodType::kMax)> method_names{};

        const auto assign_index = [&](MetaMethodType type, std::string_view name) {
            method_names[static_cast<size_t>(type)] = name;
        };

        assign_index(MetaMethodType::kIndex, "__index");
        assign_index(MetaMethodType::kNewIndex, "__newindex");
        assign_index(MetaMethodType::kAdd, "__add");
        assign_index(MetaMethodType::kSub, "__sub");
        assign_index(MetaMethodType::kMul, "__mul");
        assign_index(MetaMethodType::kDiv, "__div");
        assign_index(MetaMethodType::kMod, "__mod");
        assign_index(MetaMethodType::kPow, "__pow");
        assign_index(MetaMethodType::kUnm, "__unm");
        assign_index(MetaMethodType::kBNot, "__bnot");
        assign_index(MetaMethodType::kBAnd, "__band");
        assign_index(MetaMethodType::kBOr, "__bor");
        assign_index(MetaMethodType::kBXor, "__bxor");
        assign_index(MetaMethodType::kBShl, "__shl");
        assign_index(MetaMethodType::kBShr, "__shr");
        assign_index(MetaMethodType::kEq, "__eq");
        assign_index(MetaMethodType::kLt, "__lt");
        assign_index(MetaMethodType::kLe, "__le");
        assign_index(MetaMethodType::kCall, "__call");
        assign_index(MetaMethodType::kLen, "__len");
        assign_index(MetaMethodType::kToString, "__tostring");
        assign_index(MetaMethodType::kPairs, "__pairs");
        assign_index(MetaMethodType::kGC, "__gc");

        return method_names;
    });

    // Get metamethod from a value's metatable
    template<MetaMethodType MMIndex>
    BEHL_FORCEINLINE Value metatable_get_method(const Value& v) noexcept
    {
        const GCTable* metatable = nullptr;

        if (v.is_table())
        {
            const GCTable* t = v.get_table();
            metatable = t->metatable;
        }
        else if (v.is_userdata())
        {
            const UserdataData* ud = v.get_userdata();
            metatable = ud->metatable;
        }

        if (!metatable)
        {
            return Value::NullOpt{};
        }

        constexpr auto method = kMetatableMethodNames[static_cast<size_t>(MMIndex)];

        if (auto it = metatable->hash.find(method); it != metatable->hash.end())
        {
            // return it->value;
            return it->second;
        }

        return Value::NullOpt{};
    }

    // Call metamethod helper - pushes function+args, calls and returns first result
    template<typename... Args>
    BEHL_FORCEINLINE Value metatable_call_method_result(State* S, const Value& mm, Args&&... args)
    {
        auto& stack = S->stack;
        const size_t call_base = stack.size();

        stack.push_back(S, mm);
        (stack.push_back(S, std::forward<Args>(args)), ...);

        perform_call(S, static_cast<int>(sizeof...(Args)), 1, call_base);

        if (stack.size() > call_base)
        {
            auto result = stack[call_base];
            stack.resize(S, call_base);

            return result;
        }

        return Value::NullOpt{};
    }

    template<typename... Args>
    BEHL_FORCEINLINE void metatable_call_method(State* S, const Value& mm, Args&&... args)
    {
        auto& stack = S->stack;
        const size_t call_base = stack.size();

        stack.push_back(S, mm);
        (stack.push_back(S, std::forward<Args>(args)), ...);

        perform_call(S, static_cast<int>(sizeof...(Args)), 0, call_base);
        stack.resize(S, call_base);
    }

} // namespace behl
