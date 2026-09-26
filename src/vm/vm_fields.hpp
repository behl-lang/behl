#pragma once

#include "bytecode.hpp"
#include "frame.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "platform/platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_buffer.hpp"
#include "vm_detail.hpp"
#include "vm_error.hpp"
#include "vm_metatable.hpp"
#include "vm_table.hpp"

namespace behl
{
    // Common implementation for all getfield operations
    BEHL_INLINE
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
        else if (table.is_buffer())
        {
            vm_buffer_get(S, frame, a, table.get_buffer(), key);
        }
        else
        {
            raise_type_error(S, get_current_location(frame), "attempt to index a non-table value");
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
    BEHL_INLINE
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
            raise_type_error(S, get_current_location(frame), "attempt to index a userdata value without __newindex");
        }
        else if (table.is_buffer())
        {
            vm_buffer_set(S, frame, table.get_buffer(), key, val);
        }
        else
        {
            raise_type_error(S, get_current_location(frame), "attempt to index a non-table value");
        }
    }

    BEHL_INLINE
    void handler_setfield(State* S, CallFrame& frame, Reg a, Reg b, Reg c)
    {
        Value& table = get_register(S, frame, a);
        const Value& key = get_register(S, frame, b);
        const Value& val = get_register(S, frame, c);
        setfield_impl(S, frame, table, key, val);
    }

    BEHL_INLINE
    void handler_setfieldi(State* S, CallFrame& frame, Reg a, Reg b, int32_t imm)
    {
        Value& table = get_register(S, frame, a);
        const Value& val = get_register(S, frame, b);
        const Value key = Value(static_cast<int64_t>(imm));
        setfield_impl(S, frame, table, key, val);
    }

    BEHL_INLINE
    void handler_setfields(State* S, CallFrame& frame, Reg a, Reg b, ConstIndex k)
    {
        Value& table = get_register(S, frame, a);
        const Value& val = get_register(S, frame, b);
        const Value& key = get_string_constant(frame.proto, k);
        setfield_impl(S, frame, table, key, val);
    }

    BEHL_INLINE
    void handler_self(State* S, CallFrame& frame, Reg a, Reg b, Reg c)
    {
        const Value& table = get_register(S, frame, b);
        const Value& key = get_register(S, frame, c);
        get_register(S, frame, a + 1) = table;

        if (!table.is_table())
        {
            raise_type_error(S, get_current_location(frame), "attempt to index a non-table value");
        }

        auto* table_data = table.get_table();

        Value& dst = get_register(S, frame, a);
        dst = table_raw_getfield(table_data, key);
    }

} // namespace behl
