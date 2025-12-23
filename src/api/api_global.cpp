#include "behl.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "state.hpp"
#include "vm/value.hpp"

#include <cassert>
#include <variant>

namespace behl
{
    void register_function(State* S, std::string_view name, CFunction f)
    {
        assert(S != nullptr && "State can not be null");

        push_cfunction(S, f);
        set_global(S, name);
    }

    void create_module(State* S, std::string_view module_name, const ModuleDef& module_def, bool make_global)
    {
        assert(S != nullptr && "State can not be null");

        if (module_def.funcs.empty())
        {
            error(S, "create_module: function array is empty");
        }

        // Create module table
        table_new(S);

        // Register all functions in the module
        for (const auto& reg : module_def.funcs)
        {
            push_cfunction(S, reg.func);
            table_setfield(S, -2, reg.name);
        }

        // Register constants if provided
        for (const auto& c : module_def.consts)
        {
            std::visit(
                [S](auto&& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, Integer>)
                    {
                        push_integer(S, value);
                    }
                    else if constexpr (std::is_same_v<T, FP>)
                    {
                        push_number(S, value);
                    }
                    else if constexpr (std::is_same_v<T, std::string_view>)
                    {
                        push_string(S, value);
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        push_boolean(S, value);
                    }
                },
                c.value);
            table_setfield(S, -2, c.name);
        }

        // Get the module table from stack
        Value module_table = S->stack.back();
        if (!module_table.is_table())
        {
            error(S, "create_module: internal error - expected table on stack");
        }

        // Register in module cache so it can be imported
        auto* cached_module_name = gc_new_string(S, module_name);
        S->module_cache.insert_or_assign(S, cached_module_name, module_table);

        // If make_global is true, also register as a global variable
        if (make_global)
        {
            dup(S, -1); // Duplicate the module table
            set_global(S, module_name);
        }

        // Pop the module table from stack (it's now cached and possibly global)
        pop(S, 1);
    }

    void set_global(State* S, std::string_view name)
    {
        assert(S != nullptr && "State can not be null");

        if (S->stack.empty())
        {
            return;
        }

        Value value = S->stack.back();

        Value globals = S->globals_table;
        assert(globals.is_table() && "Globals table should be a TableData");

        auto* table = globals.get_table();
        assert(table != nullptr);

        auto* key_obj = gc_new_string(S, name);

        Value key(key_obj);
        table->hash.insert_or_assign(S, key, value);

        S->stack.pop_back();
    }

    void get_global(State* S, std::string_view name)
    {
        assert(S != nullptr && "State can not be null");

        if (name.empty())
        {
            S->stack.push_back(S, Value{});
            return;
        }

        const Value& globals = S->globals_table;
        assert(globals.is_table() && "Globals table should be a TableData");

        const auto* table = globals.get_table();
        assert(table != nullptr);

        auto* key_obj = gc_new_string(S, name);
        auto it = table->hash.find(Value(key_obj));
        if (it != table->hash.end())
        {
            S->stack.push_back(S, it->second);
        }
        else
        {
            S->stack.push_back(S, Value{});
        }
    }

} // namespace behl
