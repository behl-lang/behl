#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm_metatable.hpp"

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

        if (key.is_integer())
        {
            int64_t k = key.get_integer();
            if (k >= 0 && static_cast<size_t>(k) < t->array.size())
            {
                S->stack.push_back(S, t->array[static_cast<size_t>(k)]);
                return;
            }
        }

        auto it = t->hash.find(key);
        if (it != t->hash.end())
        {
            S->stack.push_back(S, it->second);
        }
        else
        {
            push_nil(S);
        }
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

        if (key.is_integer())
        {
            int64_t k = key.get_integer();
            if (k >= 0)
            {
                size_t sk = static_cast<size_t>(k);
                if (sk >= t->array.size())
                {
                    t->array.resize(S, sk + 1);
                }

                t->array[sk] = std::move(val);
                return;
            }
        }

        t->hash.insert_or_assign(S, key, val);
        // t->hash.emplace(key, val);
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
            if (prev_key.is_integer())
            {
                auto pk = prev_key.get_integer();
                if (pk >= 0)
                {
                    start_i = static_cast<size_t>(pk) + 1;
                }
                else
                {
                    in_array_phase = false;
                }
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
        if (!prev_key.is_nil())
        {
            // Try to find prev_key in hash (could be integer or non-integer)
            auto prev_it = t->hash.find(prev_key);
            if (prev_it != t->hash.end())
            {
                ++prev_it;
                it = prev_it;
            }
            else if (!prev_key.is_integer())
            {
                // Non-integer key not found in hash - error
                return false;
            }

            // integer key not in hash means we just finished array part, start hash iteration
        }

        if (it != t->hash.end())
        {
            S->stack.push_back(S, it->first);
            S->stack.push_back(S, it->second);
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

        const Value& table_val = S->stack[static_cast<size_t>(table_idx)];

        if (!table_val.is_table())
        {
            pop(S, 1);
            push_nil(S);
            return;
        }

        auto* t = table_val.get_table();
        assert(t != nullptr);

        Value key = S->stack.back();
        pop(S, 1);

        // Try raw access first
        Value result;
        bool found = false;

        if (key.is_integer())
        {
            int64_t k = key.get_integer();
            if (k >= 0 && static_cast<size_t>(k) < t->array.size())
            {
                result = t->array[static_cast<size_t>(k)];
                found = !result.is_nil();
            }
        }

        if (!found)
        {
            auto it = t->hash.find(key);
            if (it != t->hash.end())
            {
                result = it->second;
                found = !result.is_nil();
            }
        }

        // If found and not nil, return it
        if (found)
        {
            S->stack.push_back(S, result);
            return;
        }

        // Not found, check for __index metamethod
        Value index_mm = metatable_get_method<MetaMethodType::kIndex>(table_val);

        if (!index_mm.has_value())
        {
            push_nil(S);
            return;
        }

        // If __index is a function, call it
        if (index_mm.is_callable())
        {
            Value call_result = metatable_call_method_result(S, index_mm, table_val, key);
            if (call_result.has_value())
            {
                S->stack.push_back(S, call_result);
            }
            else
            {
                push_nil(S);
            }
            return;
        }

        // If __index is a table, retry lookup in that table
        if (index_mm.is_table())
        {
            S->stack.push_back(S, index_mm); // Push the __index table
            S->stack.push_back(S, key);      // Push the key
            table_get(S, -2);                // Recursive lookup
            // Remove the __index table, keep the result
            Value result_val = S->stack.back();
            pop(S, 1);
            pop(S, 1); // Remove __index table
            S->stack.push_back(S, result_val);
            return;
        }

        // __index is neither function nor table, return nil
        push_nil(S);
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

        Value& table_val = S->stack[static_cast<size_t>(table_idx)];

        if (!table_val.is_table())
        {
            pop(S, 2);
            return;
        }

        GCTable* t = table_val.get_table();
        assert(t != nullptr);

        Value val = S->stack.back();
        pop(S, 1);
        Value key = S->stack.back();
        pop(S, 1);

        // Check if key exists in the table (raw check)
        bool exists = false;

        if (key.is_integer())
        {
            const auto k = key.get_integer();
            if (k >= 0 && static_cast<size_t>(k) < t->array.size())
            {
                if (!t->array[static_cast<size_t>(k)].is_nil())
                {
                    exists = true;
                }
            }
        }

        if (!exists)
        {
            auto it = t->hash.find(key);
            if (it != t->hash.end() && !it->second.is_nil())
            {
                exists = true;
            }
        }

        // If key exists, do raw set
        if (exists)
        {
            if (key.is_integer())
            {
                const auto k = key.get_integer();
                if (k >= 0)
                {
                    size_t sk = static_cast<size_t>(k);
                    if (sk >= t->array.size())
                    {
                        t->array.resize(S, sk + 1);
                    }
                    t->array[sk] = std::move(val);
                    return;
                }
            }
            t->hash.insert_or_assign(S, key, val);
            return;
        }

        // Key doesn't exist, check for __newindex metamethod
        Value newindex_mm = metatable_get_method<MetaMethodType::kNewIndex>(table_val);

        if (!newindex_mm.has_value())
        {
            // No metamethod, do raw set
            if (key.is_integer())
            {
                const auto k = key.get_integer();
                if (k >= 0)
                {
                    size_t sk = static_cast<size_t>(k);
                    if (sk >= t->array.size())
                    {
                        t->array.resize(S, sk + 1);
                    }
                    t->array[sk] = std::move(val);
                    return;
                }
            }
            t->hash.insert_or_assign(S, key, val);
            return;
        }

        // If __newindex is a function, call it
        if (newindex_mm.is_callable())
        {
            metatable_call_method(S, newindex_mm, table_val, key, val);
            return;
        }

        // If __newindex is a table, retry set in that table
        if (newindex_mm.is_table())
        {
            S->stack.push_back(S, newindex_mm); // Push the __newindex table
            S->stack.push_back(S, key);         // Push the key
            S->stack.push_back(S, val);         // Push the value
            table_set(S, -3);                   // Recursive set
            pop(S, 1);                          // Remove the __newindex table
            return;
        }

        // __newindex is neither function nor table, ignore
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
        const Value& table_val = S->stack[static_cast<size_t>(table_idx)];
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
            target.get_table()->metatable = metatable;
        }
        else if (target.is_userdata())
        {
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
