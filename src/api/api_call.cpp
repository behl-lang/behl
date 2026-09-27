#include "api/api_internal.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_string.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm.hpp"
#include "vm/vm_error.hpp"

#include <behl/behl.hpp>
#include <cassert>
#include <exception>
#include <new>

namespace behl
{
    void call_unprotected(State* S, int32_t nargs, int32_t nresults)
    {
        assert(S != nullptr && "State can not be null");

        size_t actual_size = S->stack.size();

        if (actual_size < static_cast<size_t>(nargs + 1))
        {
            raise_type_error(S, SourceLocation{}, "not enough arguments for call");
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
            std::exception_ptr pending = std::current_exception();

            unwind_call_frames(S, call_frame_pos, pending);

            S->stack.resize(S, func_pos);
            truncate_call_frames(S, call_frame_pos);

            std::rethrow_exception(pending);
        }
    }

    int32_t push_error_from_exception(State* S)
    {
        try
        {
            throw;
        }
        catch (const Exception& e)
        {
            S->stack.push_back(S, e.value());
            return kErrorRuntime;
        }
        catch (const std::bad_alloc&)
        {
            S->stack.push_back(S, Value(S->memory_error_message));
            return kErrorMemory;
        }
    }

    int32_t call(State* S, int32_t nargs, int32_t nresults)
    {
        assert(S != nullptr && "State can not be null");

        const auto base = S->stack.size() >= static_cast<size_t>(nargs) + 1 ? S->stack.size() - static_cast<size_t>(nargs) - 1
                                                                            : size_t{ 0 };

        try
        {
            call_unprotected(S, nargs, nresults);
        }
        catch (const Exception&)
        {
            return push_error_from_exception(S);
        }
        catch (const std::bad_alloc&)
        {
            return push_error_from_exception(S);
        }

        return static_cast<int32_t>(S->stack.size() - base);
    }

} // namespace behl
