#include "api/api_internal.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm_detail.hpp"
#include "vm/vm_error.hpp"
#include "vm/vm_metatable.hpp"
#include "vm/vm_table.hpp"

#include <behl/behl.hpp>
#include <variant>

namespace behl
{

    static GCTable* get_table(State* S, int32_t idx)
    {
        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return nullptr;
        }

        Value& v = S->stack[static_cast<size_t>(r_idx)];

        if (!v.is_table())
        {
            return nullptr;
        }

        return v.get_table();
    }

    void table_new(State* S)
    {
        auto* obj = gc_new_table(S, 0, 0);
        S->stack.push_back(S, Value(obj));
    }

    void table_rawget(State* S, int32_t idx)
    {
        if (get_top(S) < 1)
        {
            return;
        }

        ptrdiff_t table_idx = resolve_index(S, idx);

        Value key = S->stack.back();
        pop(S, 1);

        if (table_idx < 0 || table_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            push_nil(S);
            return;
        }

        const Value& table_val = S->stack[static_cast<size_t>(table_idx)];

        if (!table_val.is_table())
        {
            push_nil(S);
            return;
        }

        auto* t = table_val.get_table();
        assert(t != nullptr);

        S->stack.push_back(S, table_raw_getfield(t, key));
    }

    void table_rawset(State* S, int32_t idx)
    {
        if (get_top(S) < 2)
        {
            return;
        }

        ptrdiff_t table_idx = resolve_index(S, idx);

        Value val = S->stack.back();
        pop(S, 1);

        Value key = S->stack.back();
        pop(S, 1);

        if (table_idx < 0 || table_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return;
        }

        Value& table_val = S->stack[static_cast<size_t>(table_idx)];
        if (!table_val.is_table())
        {
            return;
        }

        GCTable* t = table_val.get_table();
        assert(t != nullptr);

        table_raw_setfield(S, t, key, val);
    }

    void table_rawgetfield(State* S, int32_t idx, std::string_view k)
    {
        push_string(S, k);
        if (idx < 0)
        {
            idx--;
        }
        table_rawget(S, idx);
    }

    void table_rawsetfield(State* S, int32_t idx, std::string_view k)
    {
        Value val = S->stack.back();
        pop(S, 1);

        push_string(S, k);
        S->stack.push_back(S, val);

        int32_t adjusted_idx = idx < 0 ? idx - 1 : idx;
        table_rawset(S, adjusted_idx);
    }

    bool table_rawnext(State* S, int32_t idx)
    {
        GCTable* t = get_table(S, idx);
        if (!t)
        {
            return false;
        }

        Value prev_key;
        if (get_top(S) > 0)
        {
            prev_key = S->stack.back();
            pop(S, 1);
        }
        else
        {
            prev_key = Value{};
        }

        bool in_array_phase = true;
        size_t start_i = 0;
        if (!prev_key.is_nil())
        {
            if (const auto index = key_as_positive_index(prev_key); index.has_value() && *index < t->array.size())
            {
                start_i = *index + 1;
            }
            else
            {
                in_array_phase = false;
            }
        }

        if (in_array_phase)
        {
            for (size_t i = start_i; i < t->array.size(); ++i)
            {
                if (!t->array[i].is_nil())
                {
                    push_integer(S, static_cast<long long>(i));
                    S->stack.push_back(S, t->array[i]);
                    return true;
                }
            }
        }

        auto it = t->hash.begin();
        if (!prev_key.is_nil() && !in_array_phase)
        {
            it = t->hash.find(prev_key);
            if (it == t->hash.end() && prev_key.is_gcobject())
            {
                it = t->hash.find_matching(
                    prev_key, [&prev_key](const Value& stored) { return stored.is_dead_key_of(prev_key); });
            }
            if (it == t->hash.end())
            {
                raise_runtime_error(S, SourceLocation{}, "invalid key to 'next'");
            }
            ++it;
        }

        while (it != t->hash.end() && it->second.is_nil())
        {
            ++it;
        }

        if (it != t->hash.end())
        {
            const Value next_key = it->first;
            const Value next_value = it->second;
            S->stack.push_back(S, next_key);
            S->stack.push_back(S, next_value);
            return true;
        }

        return false;
    }

    bool table_next(State* S, int32_t idx)
    {
        // next() always bypasses metamethods
        // For metamethod-respecting iteration, use pairs() which checks __pairs
        return table_rawnext(S, idx);
    }

    void table_rawlen(State* S, int32_t idx)
    {
        GCTable* t = get_table(S, idx);
        if (!t)
        {
            push_integer(S, 0);
            return;
        }

        size_t len = 0;
        for (; len < t->array.size(); ++len)
        {
            if (t->array[len].is_nil())
            {
                break;
            }
        }

        push_integer(S, static_cast<Integer>(len));
    }

    void table_get(State* S, int32_t idx)
    {
        if (get_top(S) < 1)
        {
            return;
        }

        ptrdiff_t table_idx = resolve_index(S, idx);
        if (table_idx < 0 || table_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            pop(S, 1);
            push_nil(S);
            return;
        }

        const Value table_val = S->stack[static_cast<size_t>(table_idx)];
        if (!table_val.is_table())
        {
            pop(S, 1);
            push_nil(S);
            return;
        }

        const Value key = S->stack.back();
        const Value result = table_getfield_vm(S, table_val.get_table(), key);
        pop(S, 1);
        S->stack.push_back(S, result);
    }

    void table_set(State* S, int32_t idx)
    {
        if (get_top(S) < 2)
        {
            return;
        }

        ptrdiff_t table_idx = resolve_index(S, idx);
        if (table_idx < 0 || table_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            pop(S, 2);
            return;
        }

        const Value table_val = S->stack[static_cast<size_t>(table_idx)];
        if (!table_val.is_table())
        {
            pop(S, 2);
            return;
        }

        const Value val = S->stack.back();
        const Value key = S->stack[S->stack.size() - 2];
        table_setfield_vm(S, table_val.get_table(), key, val);
        pop(S, 2);
    }

    void table_getfield(State* S, int32_t idx, std::string_view k)
    {
        push_string(S, k);
        if (idx < 0)
        {
            idx--;
        }
        table_get(S, idx);
    }

    void table_setfield(State* S, int32_t idx, std::string_view k)
    {
        Value val = S->stack.back();
        pop(S, 1);

        push_string(S, k);
        S->stack.push_back(S, val);

        int32_t adjusted_idx = idx < 0 ? idx - 1 : idx;
        table_set(S, adjusted_idx);
    }

    void table_len(State* S, int32_t idx)
    {
        GCTable* t = get_table(S, idx);
        if (!t)
        {
            push_integer(S, 0);
            return;
        }

        // Check for __len metamethod
        ptrdiff_t table_idx = resolve_index(S, idx);
        const Value table_val = S->stack[static_cast<size_t>(table_idx)];
        Value len_mm = metatable_get_method<MetaMethodType::kLen>(table_val);

        if (len_mm.is_callable())
        {
            // Call __len metamethod
            Value result = metatable_call_method_result(S, len_mm, table_val);
            if (result.has_value())
            {
                S->stack.push_back(S, result);
            }
            else
            {
                push_integer(S, 0);
            }
            return;
        }

        // Default behavior: count non-nil array elements
        size_t len = 0;
        for (; len < t->array.size(); ++len)
        {
            if (t->array[len].is_nil())
            {
                break;
            }
        }

        push_integer(S, static_cast<Integer>(len));
    }

    bool metatable_get(State* S, int32_t idx)
    {
        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            push_nil(S);
            return false;
        }

        const Value& target = S->stack[static_cast<size_t>(r_idx)];
        GCTable* metatable = nullptr;

        if (target.is_table())
        {
            metatable = target.get_table()->metatable;
        }
        else if (target.is_userdata())
        {
            metatable = target.get_userdata()->metatable;
        }

        if (!metatable)
        {
            push_nil(S);
            return false;
        }

        S->stack.push_back(S, Value(metatable));
        return true;
    }

    void metatable_set(State* S, int32_t idx)
    {
        if (get_top(S) < 1)
        {
            return;
        }

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            pop(S, 1);
            return;
        }

        Value& target = S->stack[static_cast<size_t>(r_idx)];
        Value mt_val = S->stack.back();
        pop(S, 1);

        GCTable* metatable = nullptr;
        if (mt_val.is_table())
        {
            metatable = mt_val.get_table();
        }

        if (target.is_table())
        {
            if (metatable != nullptr)
            {
                gc_barrier(S, target.get_table(), Value(metatable));
            }
            target.get_table()->metatable = metatable;
        }
        else if (target.is_userdata())
        {
            if (metatable != nullptr)
            {
                gc_barrier(S, target.get_userdata(), Value(metatable));
            }
            target.get_userdata()->metatable = metatable;
        }
    }

    void table_setname(State* S, int32_t idx, std::string_view name)
    {
        ptrdiff_t resolved = resolve_index(S, idx);
        if (resolved < 0 || resolved >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return;
        }
        size_t table_idx = static_cast<size_t>(resolved);

        Value& table_val = S->stack[table_idx];

        if (!table_val.is_table())
        {
            return;
        }

        auto* table = table_val.get_table();
        table->assign_name(name);
    }

    bool metatable_new(State* S, std::string_view name)
    {
        assert(S != nullptr && "State can not be null");

        // Create GCString key
        auto* key = gc_new_string(S, name);

        // Check if metatable already exists
        auto it = S->metatable_registry.find(key);
        if (it != S->metatable_registry.end())
        {
            // Already exists - push existing metatable
            S->stack.push_back(S, it->second);
            return false;
        }

        // Create new metatable
        auto* mt = gc_new_table(S, 0, 0);
        Value mt_value(mt);

        // Store in registry
        S->metatable_registry.insert_or_assign(S, key, mt_value);

        // Push onto stack
        S->stack.push_back(S, mt_value);
        return true;
    }

    void metatable_find(State* S, std::string_view name)
    {
        assert(S != nullptr && "State can not be null");

        // Create GCString key for lookup
        auto* key = gc_new_string(S, name);

        // Find in registry
        auto it = S->metatable_registry.find(key);
        if (it != S->metatable_registry.end())
        {
            // Found - push metatable
            S->stack.push_back(S, it->second);
        }
        else
        {
            // Not found - push nil
            S->stack.push_back(S, Value::Nil{});
        }
    }

} // namespace behl
