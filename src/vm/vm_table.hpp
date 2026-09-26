#pragma once

#include "bytecode.hpp"
#include "common/arithmetic.hpp"
#include "config_internal.hpp"
#include "frame.hpp"
#include "gc/gc.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "platform/platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_detail.hpp"
#include "vm_error.hpp"
#include "vm_metatable.hpp"
#include "vm_operands.hpp"

#include <cassert>
#include <cmath>
#include <optional>

namespace behl
{
    BEHL_INLINE
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
            Integer k = 0;
            if (std::floor(d) == d && d >= 0 && arithmetic::try_from_fp(d, k))
            {
                return static_cast<size_t>(k);
            }
        }

        return std::nullopt;
    }

    BEHL_INLINE
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

    BEHL_INLINE
    const Value table_raw_getfield(GCTable* t, const Value& key)
    {
        auto* slot = table_raw_get_slot(t, key);

        return (slot != nullptr) ? *slot : Value::Nil{};
    }

    // Metatable-aware table get for VM
    BEHL_INLINE
    Value table_getfield_vm(State* state, GCTable* t, const Value key)
    {
        for (int32_t depth = 0; depth < kMaxMetaChain; ++depth)
        {
            // First try raw get
            const Value& out = table_raw_getfield(t, key);

            // If result is nil and table has a metatable, try __index
            if (!out.is_nil() || t->metatable == nullptr)
            {
                return out;
            }

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
                return out;
            }
            if (!metamethod.is_table())
            {
                return out;
            }

            // __index is a table: recursively get from it
            t = metamethod.get_table();
        }

        raise_runtime_error(state, state->call_stack.empty() ? SourceLocation{} : get_current_location(state->call_stack.back()),
            "'__index' chain too long; possible loop");
    }

    inline void table_migrate_hash_to_array(State* S, GCTable* t, size_t from)
    {
        if (t->hash.empty())
        {
            return;
        }

        for (size_t i = from; i < t->array.size(); ++i)
        {
            const Value index_key(static_cast<Integer>(i));
            auto it = t->hash.find(index_key);
            if (it != t->hash.end())
            {
                if (t->array[i].is_nil())
                {
                    t->array[i] = it->second;
                }
                t->hash.erase(index_key);
            }
        }

        for (;;)
        {
            const Value next_key(static_cast<Integer>(t->array.size()));
            auto it = t->hash.find(next_key);
            if (it == t->hash.end())
            {
                return;
            }
            const Value moved = it->second;
            t->hash.erase(next_key);
            t->array.push_back(S, moved);
        }
    }

    BEHL_INLINE
    void table_raw_setfield(State* S, struct GCTable* t, const Value& key, const Value& v)
    {
        gc_barrier(S, t, v);
        gc_barrier(S, t, key);

        // Try to interpret key as a non-negative array index
        if (const auto idx = key_as_positive_index(key))
        {
            const auto i = *idx;
            const size_t arr_size = t->array.size();

            // Hot path: sequential append
            if (i == arr_size)
            {
                t->array.push_back(S, v);
                table_migrate_hash_to_array(S, t, arr_size);
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
                table_migrate_hash_to_array(S, t, arr_size);
                return;
            }
        }

        // Use hash table for non-array indices
        t->hash.insert_or_assign(S, key, v);
        // t->hash.emplace(key, v);
    }

    // Metatable-aware table set for VM
    BEHL_INLINE
    void table_setfield_vm(State* S, GCTable* t, const Value key, const Value v)
    {
        for (int32_t depth = 0; depth < kMaxMetaChain; ++depth)
        {
            // Try to find existing slot
            Value* slot = table_raw_get_slot(t, key);

            // If key exists, update it directly
            if (slot != nullptr)
            {
                gc_barrier(S, t, v);
                *slot = v;
                return;
            }

            // Key doesn't exist - check for __newindex metamethod
            if (t->metatable == nullptr)
            {
                table_raw_setfield(S, t, key, v);
                return;
            }

            // Create a Value wrapper for the table
            Value table_value(t);
            Value metamethod = metatable_get_method<MetaMethodType::kNewIndex>(table_value);

            if (metamethod.is_callable())
            {
                // __newindex is a closure: call it
                metatable_call_method(S, metamethod, table_value, key, v);
                return;
            }
            if (!metamethod.is_table())
            {
                // No metamethod, do raw set
                table_raw_setfield(S, t, key, v);
                return;
            }

            // __newindex is a table: recursively set in it
            t = metamethod.get_table();
        }

        raise_runtime_error(S, S->call_stack.empty() ? SourceLocation{} : get_current_location(S->call_stack.back()),
            "'__newindex' chain too long; possible loop");
    }

    BEHL_INLINE
    void handler_getglobal(State* S, CallFrame& frame, Reg a, uint32_t k)
    {
        const Value key = get_string_constant(frame.proto, k);

        const Value& globals = S->globals_table;
        assert(globals.is_table());

        auto* table = globals.get_table();
        const Value result = table_getfield_vm(S, table, key);
        get_register(S, S->call_stack.back(), a) = result;
    }

    BEHL_INLINE
    void handler_setglobal(State* S, CallFrame& frame, Reg a, uint32_t k)
    {
        const Value& key = get_string_constant(frame.proto, k);

        Value& globals = S->globals_table;
        assert(globals.is_table());

        auto* table = globals.get_table();
        table_setfield_vm(S, table, key, get_register(S, frame, a));
    }

    BEHL_FORCEINLINE
    void handler_newtable(State* S, CallFrame& frame, Reg a, uint8_t array_size, uint8_t hash_size)
    {
        auto* obj = gc_new_table(S, array_size, hash_size);

        get_register(S, frame, a) = Value(obj);
        gc_validate_on_stack(S, obj);
        gc_step(S);
    }

    BEHL_INLINE
    void handler_setlist(State* S, CallFrame& frame, Reg a, uint8_t num_fields, uint8_t batch)
    {
        const Value& table = get_register(S, frame, a);
        assert(table.is_table() && "SETLIST: register A must contain a table");

        if (!table.is_table())
        {
            return;
        }

        auto* table_data = table.get_table();

        const size_t start = static_cast<size_t>(batch) * kFieldsPerFlush;
        const size_t needed = start + num_fields;
        if (needed > table_data->array.size())
        {
            table_data->array.resize(S, needed);
        }

        for (uint8_t i = 0; i < num_fields; ++i)
        {
            const Value& item = get_register(S, frame, static_cast<Reg>(a + 1U + i));
            gc_barrier(S, table_data, item);
            table_data->array[start + i] = item;
        }
    }

} // namespace behl
