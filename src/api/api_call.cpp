#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_string.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm.hpp"

#include <cassert>
#include <exception>

namespace behl
{
    void call(State* S, int32_t nargs, int32_t nresults)
    {
        assert(S != nullptr && "State can not be null");

        size_t actual_size = S->stack.size();

        if (actual_size < static_cast<size_t>(nargs + 1))
        {
            throw TypeError("not enough arguments for call");
        }

        size_t func_pos = actual_size - static_cast<size_t>(nargs) - 1;
        assert(func_pos < S->stack.size() && "Function index out of range");

        size_t call_frame_pos = S->call_stack.size();

        try
        {
            perform_call(S, nargs, nresults, func_pos);
        }
        catch (...)
        {
            S->stack.resize(S, func_pos);
            S->call_stack.resize(S, call_frame_pos);

            throw;
        }
    }

} // namespace behl
