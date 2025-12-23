#pragma once

#include "bytecode.hpp"
#include "frame.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_detail.hpp"
#include "vm_metatable.hpp"
#include "vm_operands.hpp"

#include <algorithm>
#include <behl/exceptions.hpp>
#include <cassert>

namespace behl
{
    BEHL_FORCEINLINE
    uint32_t find_or_create_upvalue(State* S, uint32_t stack_index)
    {
        const int32_t target = static_cast<int32_t>(stack_index);
        auto& open = S->open_upvalue_indices;

        // Find insertion position with fast paths
        const auto find_pos = [&]() -> UpvalueIndexVector::iterator {
            if (open.empty())
            {
                return open.end();
            }

            const int32_t first = S->upvalues[open.front()].index;
            if (target < first)
            {
                return open.begin();
            }

            const int32_t last = S->upvalues[open.back()].index;
            if (target > last)
            {
                return open.end();
            }

            return std::lower_bound(
                open.begin(), open.end(), target, [&](uint32_t idx, int32_t val) { return S->upvalues[idx].index < val; });
        };

        const auto it = find_pos();

        // Check if already exists
        if (it != open.end() && S->upvalues[*it].index == target)
        {
            return *it;
        }

        // Try to reuse a freed upvalue slot
        uint32_t new_idx;
        if (!S->closed_upvalue_freelist.empty())
        {
            new_idx = S->closed_upvalue_freelist.back();
            S->closed_upvalue_freelist.pop_back();

            // Reinitialize the upvalue at this slot
            Upvalue& uv = S->upvalues[new_idx];
            uv.closed_value.set_nil();
            uv.index = static_cast<int32_t>(stack_index);
        }
        else
        {
            new_idx = static_cast<uint32_t>(S->upvalues.size());
            S->upvalues.emplace_back(S, stack_index);
        }

        open.insert(S, it, new_idx);
        return new_idx;
    }

    BEHL_FORCEINLINE
    void close_upvalues(State* S, uint32_t from_index)
    {
        auto& open = S->open_upvalue_indices;

        if (open.empty())
        {
            return;
        }

        auto& stack = S->stack;
        auto& upvalues = S->upvalues;

        // Fast path: check if we need to close anything at all
        // Since list is sorted, check the last (lowest stack position) upvalue
        const uint32_t last_index = static_cast<uint32_t>(upvalues[open.back()].index);
        if (last_index < from_index)
        {
            return;
        }

        // Fast path: if first upvalue >= from_index, close all
        const uint32_t first_index = static_cast<uint32_t>(upvalues[open.front()].index);
        if (first_index >= from_index)
        {
            // Close all upvalues
            for (const uint32_t uv_idx : open)
            {
                Upvalue& uv = upvalues[uv_idx];
                uv.closed_value = stack[static_cast<size_t>(uv.index)];
                uv.index = -1;
            }
            open.clear();
            return;
        }

        // Binary search for cutoff point
        auto it = std::lower_bound(open.begin(), open.end(), from_index,
            [&](uint32_t idx, uint32_t val) { return static_cast<uint32_t>(upvalues[idx].index) < val; });

        // Close upvalues from cutoff to end
        for (auto close_it = it; close_it != open.end(); ++close_it)
        {
            Upvalue& uv = upvalues[*close_it];
            uv.closed_value = stack[static_cast<size_t>(uv.index)];
            uv.index = -1;
        }

        // Resize to keep only upvalues below from_index
        open.resize(S, static_cast<size_t>(std::distance(open.begin(), it)));
    }

    BEHL_FORCEINLINE
    Value& upvalue_ref(State* S, uint32_t upvalue_index)
    {
        assert(upvalue_index < S->upvalues.size() && "upvalue_ptr: upvalue_index out of bounds");
        Upvalue& uv = S->upvalues[upvalue_index];
        if (uv.is_open())
        {
            assert(static_cast<uint32_t>(uv.index) < S->stack.size() && "upvalue_ptr: stack_index out of bounds");
            return S->stack[static_cast<size_t>(uv.index)];
        }
        return uv.closed_value;
    }

    BEHL_FORCEINLINE
    void handler_getupval(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const auto& upvalue_indices = S->stack[frame.base].get_closure()->upvalue_indices;
        assert(b < upvalue_indices.size() && "handler_getupval: upvalue index out of bounds");
        const auto upvalue_idx = upvalue_indices[b];
        get_register(S, frame, a) = upvalue_ref(S, upvalue_idx);
    }

    BEHL_FORCEINLINE
    void handler_setupval(State* S, CallFrame& frame, Reg a, Reg b)
    {
        const auto& upvalue_indices = S->stack[frame.base].get_closure()->upvalue_indices;
        assert(b < upvalue_indices.size() && "handler_setupval: up value index out of bounds");
        const auto upvalue_idx = upvalue_indices[b];
        const Value& v = get_register(S, frame, a);
        upvalue_ref(S, upvalue_idx) = v;
    }

} // namespace behl
