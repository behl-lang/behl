#include "behl.hpp"
#include "gc/gc.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_userdata.hpp"
#include "state.hpp"
#include "vm/value.hpp"

#include <cassert>

namespace behl
{
    static inline size_t get_cfunction_base(const State* S)
    {
        assert(S != nullptr && "State can not be null");

        if (!S->call_stack.empty())
        {
            const auto& frame = S->call_stack.back();
            if (frame.proto == nullptr)
            {
                return frame.base + 1;
            }
        }

        return 0;
    }

    ptrdiff_t resolve_index(const State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        if (idx >= 0)
        {
            return static_cast<ptrdiff_t>(get_cfunction_base(S)) + idx;
        }
        else
        {
            return static_cast<ptrdiff_t>(S->stack.size()) + idx;
        }
    }

    int32_t get_top(State* S)
    {
        assert(S != nullptr && "State can not be null");

        return static_cast<int>(S->stack.size() - get_cfunction_base(S));
    }

    void set_top(State* S, int32_t n)
    {
        assert(S != nullptr && "State can not be null");

        size_t base = get_cfunction_base(S);
        size_t current_size = S->stack.size() - base;
        size_t new_size;
        if (n >= 0)
        {
            new_size = base + static_cast<size_t>(n);
        }
        else
        {
            size_t to_pop = static_cast<size_t>(-n);
            if (to_pop > current_size)
            {
                new_size = base;
            }
            else
            {
                new_size = base + current_size - to_pop;
            }
        }

        S->stack.resize(S, new_size);
    }

    void pop(State* S, int32_t n)
    {
        assert(S != nullptr && "State can not be null");

        const auto count = static_cast<size_t>(n);
        assert(count <= S->stack.size() && "Pop count exceeds stack size");

        S->stack.resize(S, S->stack.size() - count);
    }

    void dup(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            push_nil(S);
            return;
        }
        Value v = S->stack[static_cast<size_t>(r_idx)];
        S->stack.push_back(S, v);
    }

    void remove(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return;
        }

        S->stack.erase(S->stack.begin() + r_idx);
    }

    void insert(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        assert(!S->stack.empty() && "Stack must not be empty to perform insert");
        if (S->stack.empty())
        {
            return;
        }

        ptrdiff_t r_idx = resolve_index(S, idx);
        assert(r_idx >= 0 && r_idx < static_cast<ptrdiff_t>(S->stack.size()) && "Index out of bounds for insert");
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return;
        }

        Value top = S->stack.back();
        S->stack.pop_back();

        // Shift elements up to make room
        S->stack.push_back(S, Value{});
        for (ptrdiff_t i = static_cast<ptrdiff_t>(S->stack.size()) - 1; i > r_idx; --i)
        {
            S->stack[static_cast<size_t>(i)] = S->stack[static_cast<size_t>(i - 1)];
        }

        S->stack[static_cast<size_t>(r_idx)] = top;
    }

    void push_nil(State* S)
    {
        assert(S != nullptr && "State can not be null");

        S->stack.push_back(S, {});
    }

    void push_boolean(State* S, bool b)
    {
        assert(S != nullptr && "State can not be null");

        S->stack.push_back(S, Value(b));
    }

    void push_integer(State* S, Integer n)
    {
        assert(S != nullptr && "State can not be null");

        S->stack.push_back(S, Value(n));
    }

    void push_number(State* S, FP n)
    {
        assert(S != nullptr && "State can not be null");

        S->stack.push_back(S, Value(n));
    }

    void push_string(State* S, std::string_view str)
    {
        assert(S != nullptr && "State can not be null");

        auto* obj = gc_new_string(S, str);
        S->stack.push_back(S, Value(obj));
    }

    void push_cfunction(State* S, CFunction f)
    {
        assert(S != nullptr && "State can not be null");

        S->stack.push_back(S, Value(f));
    }

    void* userdata_new(State* S, SysInt size, uint32_t uid)
    {
        assert(S != nullptr && "State can not be null");

        // Allocate UserdataData object
        auto* userdata = gc_new_userdata(S, size);

        // Allocate the actual data buffer
        if (userdata == nullptr || (userdata->data == nullptr && size > 0))
        {
            error(S, "Out of memory allocating userdata");
        }

        // Set the type UID for type safety
        userdata->uid = uid;

        // Push userdata onto stack
        S->stack.push_back(S, Value(userdata));

        // Return pointer to data (or userdata itself for zero-size)
        return size > 0 ? userdata->data : reinterpret_cast<void*>(userdata);
    }

    uint32_t userdata_get_uid(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return 0;
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (!v.is_userdata())
        {
            return 0;
        }

        auto* userdata = v.get_userdata();
        return userdata ? userdata->uid : 0;
    }

    void* to_userdata(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return nullptr;
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (!v.is_userdata())
        {
            return nullptr;
        }

        auto* userdata = v.get_userdata();
        return userdata ? userdata->data : nullptr;
    }

    void* check_userdata(State* S, int32_t idx, uint32_t uid)
    {
        assert(S != nullptr && "State can not be null");

        check_type(S, idx, Type::kUserdata);

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            error(S, "Invalid stack index");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        auto* userdata = v.get_userdata();

        if (userdata->uid != uid)
        {
            error(S, "Type mismatch: userdata uid does not match expected type");
        }

        return userdata->data;
    }

} // namespace behl
