#include "state.hpp"
#include "vm/value.hpp"

#include <algorithm>
#include <behl/behl.hpp>
#include <cassert>
#include <ranges>

namespace behl
{
    PinHandle pin(State* S)
    {
        assert(S != nullptr && "State can not be null");

        PinHandle pin_handle = PinHandle::kInvalid;

        if (S->stack.empty())
        {
            return pin_handle;
        }

        const Value& value = S->stack.back();

        if (S->free_pinned_indices.empty())
        {
            pin_handle = static_cast<PinHandle>(S->pinned.size());
            S->pinned.push_back(S, value);
        }
        else
        {
            pin_handle = static_cast<PinHandle>(S->free_pinned_indices.back());
            S->free_pinned_indices.pop_back();

            S->pinned[static_cast<size_t>(pin_handle)] = value;
        }

        S->stack.pop_back();

        return pin_handle;
    }

    void pinned_push(State* S, PinHandle pin_handle)
    {
        assert(S != nullptr);

        const auto pin_idx = static_cast<size_t>(pin_handle);
        assert(pin_idx < S->pinned.size());

        if (pin_idx >= S->pinned.size())
        {
            S->stack.push_back(S, Value{});
            return;
        }

        const Value& value = S->pinned[pin_idx];
        S->stack.push_back(S, value);
    }

    void unpin(State* S, PinHandle pin_handle)
    {
        assert(S != nullptr);

        const auto pin_idx = static_cast<size_t>(pin_handle);
        assert(pin_idx < S->pinned.size());

        if (pin_idx >= S->pinned.size())
        {
            return;
        }

        if (pin_idx == S->pinned.size() - 1)
        {
            // If last just pop.
            S->pinned.pop_back();

            if (S->free_pinned_indices.size() >= 32)
            {
                // Sort free indices to remove any trailing free indices.
                std::ranges::sort(S->free_pinned_indices);

                // Remove any trailing free indices.
                while (!S->free_pinned_indices.empty() && S->free_pinned_indices.back() == S->pinned.size() - 1)
                {
                    S->free_pinned_indices.pop_back();
                    S->pinned.pop_back();
                }
            }
        }
        else
        {
            // Mark index as free.
            S->pinned[pin_idx].set_nil();
            S->free_pinned_indices.push_back(S, pin_idx);
        }
    }

} // namespace behl
