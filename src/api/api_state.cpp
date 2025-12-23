#include "behl.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_string.hpp"
#include "gc/gco_table.hpp"
#include "state.hpp"
#include "vm/value.hpp"

#include <cassert>
#include <iostream>

namespace behl
{
    static void add_module_path(State* S, std::string_view path)
    {
        assert(S != nullptr && "State can not be null");

        auto* path_str = gc_new_string(S, path);
        S->module_paths.push_back(S, path_str);
    }

    static void default_print_handler(State*, std::string_view output)
    {
        std::cout << output;
    }

    State* new_state()
    {
        auto* state = new State();
        state->print_handler = default_print_handler;

        gc_init(state);
        gc_pause(state);

        state->stack.init(state, 128);
        state->call_stack.init(state, 64);
        state->upvalues.init(state, 64);
        state->open_upvalue_indices.init(state, 64);
        state->closed_upvalue_freelist.init(state, 0);
        auto* globals_obj = gc_new_table(state, 1024, 1024);
        globals_obj->assign_name("_G");

        state->globals_table = Value(globals_obj);

        assert(state->globals_table.is_gcobject() && "globals_table should be a heap object");
        assert(state->globals_table.is_table() && "globals_table should be a TableData");

        auto* tbl = state->globals_table.get_table();
        assert(tbl != nullptr);

        auto* key_obj = gc_new_string(state, "_G");
        tbl->hash.insert_or_assign(state, Value(key_obj), state->globals_table);

        state->call_stack.reserve(state, 128);

        // Initialize module search paths
        add_module_path(state, "./");
        add_module_path(state, "./modules/");
        add_module_path(state, "./lib/");

        gc_resume(state);

        return state;
    }

    void close(State* S)
    {
        assert(S != nullptr && "State can not be null");

        gc_close(S);

        S->stack.destroy(S);
        S->pinned.destroy(S);
        S->free_pinned_indices.destroy(S);
        S->call_stack.destroy(S);
        S->upvalues.destroy(S);
        S->open_upvalue_indices.destroy(S);
        S->closed_upvalue_freelist.destroy(S);
        S->module_paths.destroy(S);
        S->module_cache.destroy(S);
        S->metatable_registry.destroy(S);

        delete S;
    }

    void set_print_handler(State* S, PrintHandler handler)
    {
        assert(S != nullptr && "State can not be null");

        if (handler == nullptr)
        {
            // Restore default handler
            S->print_handler = default_print_handler;
        }
        else
        {
            S->print_handler = handler;
        }
    }

} // namespace behl
