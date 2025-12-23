#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"
#include "backend/compiler.hpp"
#include "behl.hpp"
#include "common/format.hpp"
#include "common/print.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "gc/gco_proto.hpp"
#include "gc/gco_table.hpp"
#include "gc/gco_userdata.hpp"
#include "modules.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm.hpp"
#include "vm/vm_detail.hpp"
#include "vm/vm_metatable.hpp"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace behl
{
    static int lib_print(State* S)
    {
        int n = get_top(S);

        const auto current_frame = S->call_stack.back();

        for (int i = 0; i < n; ++i)
        {
            const auto real_index = resolve_index(S, i);
            if (i > 0)
            {
                print(S, "\t");
            }

            const auto& val = S->stack[static_cast<size_t>(real_index)];

            // Handle nil specially since vm_tostring returns nil for nil
            if (val.get_type() == Type::kNil)
            {
                print(S, "nil");
            }
            else
            {
                // Use vm_tostring which respects __tostring metamethod
                Value str_val = vm_tostring(S, val, current_frame);
                if (str_val.get_type() == Type::kString)
                {
                    print(S, "{}", str_val.get_string()->view());
                }
                else
                {
                    // Fallback if vm_tostring returns nil (shouldn't happen for non-nil values)
                    print(S, "{}", str_val.get_type_string());
                }
            }
        }
        println(S, "");

        return 0;
    }

    static int lib_typeof(State* S)
    {
        push_string(S, value_typename(S, 0));
        return 1;
    }

    static int lib_typeid(State* S)
    {
        push_integer(S, static_cast<int>(type(S, 0)));
        return 1;
    }

    static int lib_getmetatable(State* S)
    {
        Type t = type(S, 0);
        if (t != Type::kTable && t != Type::kUserdata)
        {
            push_nil(S);
            return 1;
        }

        metatable_get(S, 0);
        return 1;
    }

    static int lib_setmetatable(State* S)
    {
        // First argument must be table or userdata
        Type t = type(S, 0);
        if (t != Type::kTable && t != Type::kUserdata)
        {
            error(S, "setmetatable: first argument must be a table or userdata");
        }

        // Metatable can be table or nil
        if (type(S, 1) != Type::kTable && type(S, 1) != Type::kNil)
        {
            error(S, "setmetatable: second argument must be a table or nil");
        }

        dup(S, 0); // Duplicate table
        dup(S, 1); // Duplicate metatable
        // Stack: [table, metatable, table_dup, metatable_dup]

        metatable_set(S, 2); // Set metatable on the duplicated table (at index 2)
        // Stack: [table, metatable, table_dup] (set_metatable popped metatable_dup)

        return 1;
    }

    static int lib_rawlen(State* S)
    {
        if (get_top(S) < 1)
        {
            push_integer(S, 0);
            return 1;
        }

        if (type(S, 0) != Type::kTable)
        {
            push_integer(S, 0);
            return 1;
        }

        table_rawlen(S, 0);
        return 1;
    }

    // Iterator function for pairs() - called each iteration
    // Stack: [table, key]
    // Returns: [next_key, value] or [nil] when done
    static int pairs_next(State* S)
    {
        if (get_top(S) < 2)
        {
            push_nil(S);
            return 1;
        }

        if (type(S, 0) != Type::kTable)
        {
            push_nil(S);
            return 1;
        }

        // Push the current key (at index 1) onto stack for table_rawnext
        dup(S, 1);

        // table_rawnext(S, 0) expects key on top of stack, table at index 0
        // It pops the key and pushes (next_key, value) if there's a next entry
        // Use rawnext because next() always bypasses metamethods
        if (table_rawnext(S, 0))
        {
            // table_rawnext pushed (key, value) onto stack
            return 2;
        }

        // No more entries
        push_nil(S);
        return 1;
    }

    // pairs(table) - returns (iterator_fn, table, nil)
    // Usage: for (k, v in pairs(t)) { ... }
    static int lib_pairs(State* S)
    {
        if (get_top(S) < 1)
        {
            error(S, "pairs: expected 1 argument");
        }

        if (type(S, 0) != Type::kTable)
        {
            error(S, "pairs: argument must be a table");
        }

        // Check for __pairs metamethod
        const auto& table_val = S->stack[static_cast<size_t>(resolve_index(S, 0))];
        Value pairs_mm = metatable_get_method<MetaMethodType::kPairs>(table_val);

        if (pairs_mm.is_callable())
        {
            // Call __pairs(table) - it should return (iterator_fn, state, initial_value)
            const size_t call_base = S->stack.size();
            S->stack.push_back(S, pairs_mm);
            S->stack.push_back(S, table_val);

            perform_call(S, 1, 3, call_base);

            // The metamethod should have returned 3 values on the stack
            // Return those 3 values: (iterator_fn, state, initial_value)
            return 3;
        }

        // No __pairs metamethod, use default iteration
        // Return (pairs_next, table, nil)
        // The iterator function
        push_cfunction(S, pairs_next);

        // The table (state)
        dup(S, 0);

        // Initial key (nil means start from beginning)
        push_nil(S);

        return 3;
    }

    // import(module_name) - Load and execute a module, return exports
    static int lib_import(State* S)
    {
        if (get_top(S) < 1)
        {
            error(S, "import: expected module name");
        }

        if (type(S, 0) != Type::kString)
        {
            error(S, "import: module name must be a string");
        }

        const auto module_name = check_string(S, 0);

        // Check if it's a cached built-in module (like "table", "math", etc.)
        if (auto it = S->module_cache.find(module_name); it != S->module_cache.end())
        {
            S->stack.push_back(S, it->second);
            return 1;
        }

        // Get current proto to find importing file path
        // Look at the frame BEFORE the current one (since current frame is this C function with proto=null)
        std::string_view importing_file = "./"; // Default to current directory
        if (S->call_stack.size() >= 2)
        {
            auto& caller_frame = S->call_stack[S->call_stack.size() - 2];
            if (caller_frame.proto && caller_frame.proto->source_path && caller_frame.proto->source_path->size() > 0)
            {
                importing_file = caller_frame.proto->source_path->view();
            }
        }

        // Resolve full path
        std::string resolved_path = resolve_module_path(S, module_name, importing_file);

        // Check cache with resolved path
        if (auto it = S->module_cache.find(resolved_path); it != S->module_cache.end())
        {
            S->stack.push_back(S, it->second);
            return 1;
        }

        // Load module file
        std::ifstream file(resolved_path);
        if (!file.is_open())
        {
            error(S, ::behl::format("Failed to open module file: {}", resolved_path));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        // Compile and execute module using load_buffer
        load_buffer(S, source, resolved_path, true);
        call(S, 0, 1);

        // Top of stack should be the module exports (or nil for non-modules)
        Value exports = S->stack.back();

        // Cache the module
        auto* cached_path = gc_new_string(S, resolved_path);
        S->module_cache.insert_or_assign(S, cached_path, exports);

        // Return exports (already on stack)
        return 1;
    }

    // error(message) - Raise an error with the given message
    static int lib_error(State* S)
    {
        if (get_top(S) < 1)
        {
            error(S, "error: expected at least 1 argument (message)");
        }

        const auto current_frame = S->call_stack.back();

        const Value& arg = S->stack[static_cast<size_t>(resolve_index(S, 0))];
        Value str_val = vm_tostring(S, arg, current_frame);

        if (str_val.is_string())
        {
            error(S, str_val.get_string()->data());
        }
        else
        {
            error(S, "error"); // Fallback if conversion fails
        }
    }

    // pcall(func, ...) - Call function in protected mode
    // Returns: (true, results...) on success, (false, error_message) on error
    static int lib_pcall(State* S)
    {
        if (get_top(S) < 1)
        {
            error(S, "pcall: expected at least 1 argument (function)");
        }

        const auto& func_val = S->stack[static_cast<size_t>(resolve_index(S, 0))];
        if (!func_val.is_callable())
        {
            error(S, "pcall: first argument must be a function");
        }

        // Stack: [func, arg1, arg2, ...]
        // We need to call func with args, then inject status bool at the front
        const int32_t nargs = get_top(S) - 1;
        const size_t call_frame_pos = S->call_stack.size();

        try
        {
            // Call function with call_nothrow - results go on top of stack
            call(S, nargs, kMultRet);

            // Stack now: [result1, result2, ...]
            const int32_t nresults = get_top(S);

            // Insert true at position 0 (start of results)
            push_boolean(S, true);
            insert(S, 0);

            // Return: true + all results
            return nresults + 1;
        }
        catch (const std::exception& e)
        {
            // Cleanup on exception
            auto* err_obj = gc_new_string(S, e.what());
            S->call_stack.resize(S, call_frame_pos);

            // Clear stack and push error result
            set_top(S, 0);
            push_boolean(S, false);
            S->stack.push_back(S, Value(err_obj));

            return 2;
        }
    }

    void load_lib_core(State* S)
    {
        register_function(S, "print", lib_print);
        register_function(S, "typeof", lib_typeof);
        register_function(S, "typeid", lib_typeid);
        register_function(S, "getmetatable", lib_getmetatable);
        register_function(S, "setmetatable", lib_setmetatable);
        register_function(S, "rawlen", lib_rawlen);
        register_function(S, "pairs", lib_pairs);
        register_function(S, "import", lib_import);
        register_function(S, "error", lib_error);
        register_function(S, "pcall", lib_pcall);
    }

} // namespace behl
