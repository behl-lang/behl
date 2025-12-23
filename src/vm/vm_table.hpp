#pragma once

#include "bytecode.hpp"
#include "frame.hpp"
#include "gc/gc.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_detail.hpp"
#include "vm_metatable.hpp"
#include "vm_operands.hpp"

#include <behl/exceptions.hpp>
#include <cassert>
#include <cmath>
#include <optional>

namespace behl
{
    BEHL_FORCEINLINE
    std::optional<size_t> key_as_positive_index(const Value& key)
    {
        if (key.is_integer())
        {
            const Integer k = key.get_integer();
            if (k >= 0)
            {
                return static_cast<size_t>(k);
            }

            return std::nullopt;
        }

        if (key.is_fp())
        {
            const FP d = key.get_fp();
            if (std::floor(d) == d && d >= 0 && d <= static_cast<FP>(INT64_MAX))
            {
                return static_cast<size_t>(static_cast<Integer>(d));
            }
        }

        return std::nullopt;
    }

    BEHL_FORCEINLINE
    Value* table_raw_get_slot(auto* t, const Value& key)
    {
        // Try to interpret key as a non-negative array index
        if (auto idx = key_as_positive_index(key))
        {
            const auto i = *idx;
            if (i < t->array.size())
            {
                return &t->array[i];
            }
        }

        if (t->hash.empty())
        {
            return nullptr;
        }

        auto it = t->hash.find(key);
        return (it != t->hash.end()) ? &it->second : nullptr;
    }

    BEHL_FORCEINLINE
    const Value table_raw_getfield(GCTable* t, const Value& key)
    {
        auto* slot = table_raw_get_slot(t, key);

        return (slot != nullptr) ? *slot : Value::Nil{};
    }

    // Metatable-aware table get for VM
    inline Value table_getfield_vm(State* state, GCTable* t, const Value key)
    {
        // First try raw get
        const Value& out = table_raw_getfield(t, key);

        // If result is nil and table has a metatable, try __index
        if (out.is_nil() && t->metatable != nullptr)
        {
            // Create a Value wrapper for the table to call metatable_get_method
            Value table_value(const_cast<GCTable*>(t));
            Value metamethod = metatable_get_method<MetaMethodType::kIndex>(table_value);

            if (metamethod.is_callable())
            {
                auto result = metatable_call_method_result(state, metamethod, table_value, key);
                if (result.has_value())
                {
                    return result;
                }
            }
            else if (metamethod.is_table())
            {
                // __index is a table: recursively get from it
                GCTable* mm_table = metamethod.get_table();
                return table_getfield_vm(state, mm_table, key);
            }
        }

        return out;
    }

    BEHL_FORCEINLINE
    void table_raw_setfield(State* S, struct GCTable* t, const Value& key, const Value& v)
    {
        // Try to interpret key as a non-negative array index
        if (const auto idx = key_as_positive_index(key))
        {
            const auto i = *idx;
            const size_t arr_size = t->array.size();

            // Hot path: sequential append
            if (i == arr_size)
            {
                t->array.push_back(S, v);
                return;
            }
            // In-bounds update
            if (i < arr_size)
            {
                t->array[i] = v;
                return;
            }
            // Near miss: resize if within growth limit
            if (i < arr_size + kTableArrayGrowthLimit)
            {
                t->array.resize(S, i + 1);
                t->array[i] = v;
                return;
            }
        }

        // Use hash table for non-array indices
        t->hash.insert_or_assign(S, key, v);
        // t->hash.emplace(key, v);
    }

    // Metatable-aware table set for VM
    inline void table_setfield_vm(State* S, GCTable* t, const Value key, const Value v)
    {
        // Try to find existing slot
        Value* slot = table_raw_get_slot(t, key);

        // If key exists, update it directly
        if (slot != nullptr)
        {
            *slot = v;
            return;
        }

        // Key doesn't exist - check for __newindex metamethod
        if (t->metatable != nullptr)
        {
            // Create a Value wrapper for the table
            Value table_value(t);
            Value metamethod = metatable_get_method<MetaMethodType::kNewIndex>(table_value);

            if (metamethod.has_value())
            {
                if (metamethod.is_callable())
                {
                    // __newindex is a closure: call it
                    metatable_call_method(S, metamethod, table_value, key, v);
                    return;
                }
                else if (metamethod.is_table())
                {
                    // __newindex is a table: recursively set in it
                    GCTable* mm_table = metamethod.get_table();
                    table_setfield_vm(S, mm_table, key, v);
                    return;
                }
            }
        }

        // No metamethod, do raw set
        table_raw_setfield(S, t, key, v);
    }

    BEHL_FORCEINLINE
    void handler_getglobal(State* S, CallFrame& frame, Reg a, uint32_t k)
    {
        const Value& key = get_string_constant(frame.proto, k);

        const Value& globals = S->globals_table;
        assert(globals.is_table());

        auto* table = globals.get_table();

        if (auto it = table->hash.find(key); it != table->hash.end())
        {
            get_register(S, frame, a) = it->second;
        }
        else
        {
            get_register(S, frame, a).set_nil();
        }
    }

    BEHL_FORCEINLINE
    void handler_setglobal(State* S, CallFrame& frame, Reg a, uint32_t k)
    {
        const Value& key = get_string_constant(frame.proto, k);

        Value& globals = S->globals_table;
        assert(globals.is_table());

        auto* table = globals.get_table();

        const Value& v = get_register(S, frame, a);
        table->hash.insert_or_assign(S, key, v);
    }

    // Common implementation for all getfield operations
    BEHL_FORCEINLINE
    void getfield_impl(State* S, CallFrame& frame, Reg a, const Value table, const Value key)
    {
        if (table.is_table())
        {
            auto* table_data = table.get_table();
            get_register(S, S->call_stack.back(), a) = table_getfield_vm(S, table_data, key);
        }
        else if (table.is_userdata())
        {
            // Userdata with __index metamethod
            auto* userdata = table.get_userdata();
            if (userdata->metatable != nullptr)
            {
                Value metamethod = metatable_get_method<MetaMethodType::kIndex>(table);

                if (metamethod.has_value())
                {
                    if (metamethod.is_callable())
                    {
                        auto result = metatable_call_method_result(S, metamethod, table, key);
                        if (result.has_value())
                        {
                            get_register(S, S->call_stack.back(), a) = result;
                            return;
                        }
                    }
                    else if (metamethod.is_table())
                    {
                        // __index is a table: get from it
                        GCTable* mm_table = metamethod.get_table();
                        get_register(S, S->call_stack.back(), a) = table_getfield_vm(S, mm_table, key);
                        return;
                    }
                }
            }

            // No metatable or no __index: return nil
            get_register(S, frame, a).set_nil();
        }
        else
        {
            throw TypeError("attempt to index a non-table value", get_current_location(frame));
        }
    }

    BEHL_FORCEINLINE
    void handler_getfield(State* S, CallFrame& frame, Reg a, Reg b, Reg c)
    {
        Value& table = get_register(S, frame, b);
        const Value& key = get_register(S, frame, c);
        getfield_impl(S, frame, a, table, key);
    }

    BEHL_FORCEINLINE
    void handler_getfieldi(State* S, CallFrame& frame, Reg a, Reg b, int32_t imm)
    {
        Value& table = get_register(S, frame, b);
        const Value key = Value(static_cast<int64_t>(imm));
        getfield_impl(S, frame, a, table, key);
    }

    BEHL_FORCEINLINE
    void handler_getfields(State* S, CallFrame& frame, Reg a, Reg b, ConstIndex k)
    {
        Value& table = get_register(S, frame, b);
        const Value& key = get_string_constant(frame.proto, k);
        getfield_impl(S, frame, a, table, key);
    }

    // Common implementation for all setfield operations
    BEHL_FORCEINLINE
    void setfield_impl(State* S, CallFrame& frame, Value& table, const Value& key, const Value& val)
    {
        if (table.is_table())
        {
            auto* table_data = table.get_table();
            table_setfield_vm(S, table_data, key, val);
        }
        else if (table.is_userdata())
        {
            // Userdata with __newindex metamethod
            auto* userdata = table.get_userdata();
            if (userdata->metatable != nullptr)
            {
                Value metamethod = metatable_get_method<MetaMethodType::kNewIndex>(table);

                if (metamethod.has_value())
                {
                    if (metamethod.is_callable())
                    {
                        metatable_call_method(S, metamethod, table, key, val);
                        return;
                    }
                    else if (metamethod.is_table())
                    {
                        // __newindex is a table: set in it
                        GCTable* mm_table = metamethod.get_table();
                        table_setfield_vm(S, mm_table, key, val);
                        return;
                    }
                }
            }
            // No metatable or no __newindex: throw error (userdata has no fields)
            throw TypeError("attempt to index a userdata value without __newindex", get_current_location(frame));
        }
        else
        {
            throw TypeError("attempt to index a non-table value", get_current_location(frame));
        }
    }

    BEHL_FORCEINLINE
    void handler_setfield(State* S, CallFrame& frame, Reg a, Reg b, Reg c)
    {
        Value& table = get_register(S, frame, a);
        const Value& key = get_register(S, frame, b);
        const Value& val = get_register(S, frame, c);
        setfield_impl(S, frame, table, key, val);
    }

    BEHL_FORCEINLINE
    void handler_setfieldi(State* S, CallFrame& frame, Reg a, Reg b, int32_t imm)
    {
        Value& table = get_register(S, frame, a);
        const Value& val = get_register(S, frame, b);
        const Value key = Value(static_cast<int64_t>(imm));
        setfield_impl(S, frame, table, key, val);
    }

    BEHL_FORCEINLINE
    void handler_setfields(State* S, CallFrame& frame, Reg a, Reg b, ConstIndex k)
    {
        Value& table = get_register(S, frame, a);
        const Value& val = get_register(S, frame, b);
        const Value& key = get_string_constant(frame.proto, k);
        setfield_impl(S, frame, table, key, val);
    }

    BEHL_FORCEINLINE
    void handler_newtable(State* S, CallFrame& frame, Reg a, uint8_t array_size, uint8_t hash_size)
    {
        auto* obj = gc_new_table(S, array_size, hash_size);

        get_register(S, frame, a) = Value(obj);
        gc_validate_on_stack(S, obj);
        gc_step(S);
    }

    BEHL_FORCEINLINE
    void handler_self(State* S, CallFrame& frame, Reg a, Reg b, Reg c)
    {
        const Value& table = get_register(S, frame, b);
        const Value& key = get_register(S, frame, c);
        get_register(S, frame, a + 1) = table;

        if (!table.is_table())
        {
            throw TypeError("attempt to index a non-table value", get_current_location(frame));
        }

        auto* table_data = table.get_table();

        Value& dst = get_register(S, frame, a);
        dst = table_raw_getfield(table_data, key);
    }

    BEHL_FORCEINLINE
    void handler_setlist(State* S, CallFrame& frame, Reg a, uint8_t num_fields, uint8_t extra)
    {
        Value table = get_register(S, frame, a);

        if (table.is_table())
        {
            auto* table_data = table.get_table();

            int64_t start_idx = extra;

            // If num_fields is 0, this means "use all values from top of stack" (multret)
            // This happens when the last field is a vararg expansion (...)
            uint8_t actual_num_fields;
            if (num_fields == 0)
            {
                // Calculate number of fields from stack top
                // Values start at register (a + 2), and frame.top points one past the last value
                uint8_t values_start = a + 2;
                actual_num_fields = static_cast<uint8_t>(frame.top - (frame.base + values_start));
            }
            else
            {
                actual_num_fields = num_fields;
            }

            size_t needed = static_cast<size_t>(start_idx - 1 + actual_num_fields);
            table_data->array.resize(S, needed);

            for (uint8_t i = 0; i < actual_num_fields; ++i)
            {
                Value val = get_register(S, frame, static_cast<Reg>(a + 2U + i));
                table_data->array[static_cast<size_t>(start_idx + i - 1)] = val;
            }
        }
    }

} // namespace behl
