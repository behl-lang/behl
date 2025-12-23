#pragma once

#include <behl/config.hpp>
#include <behl/export.hpp>
#include <behl/types.hpp>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace behl
{
    // State management
    //////////////////////////////////////////////////////////////////////////

    // Creates and initializes a new interpreter state, must be closed with close() when no longer needed.
    BEHL_API State* new_state();

    // Closes and cleans up the given interpreter state, pointer is no longer valid after this call.
    BEHL_API void close(State* S);

    // Causes a runtime error in the given state with the provided message, does not return.
    [[noreturn]] BEHL_API void error(State* S, std::string_view msg);

    // Assigns the value at the top of the stack to the global table _G with the given name, pops the value.
    // All entries in the global table will become global variables.
    BEHL_API void set_global(State* S, std::string_view name);

    // Gets the value from the global table _G with the given name and pushes it onto the stack.
    BEHL_API void get_global(State* S, std::string_view name);

    // Registers a C function as a global function with the given name in the global table _G.
    BEHL_API void register_function(State* S, std::string_view name, CFunction f);

    // Sets a custom print handler to redirect print output. Pass nullptr to restore default behavior.
    BEHL_API void set_print_handler(State* S, PrintHandler handler);

    // Module and standard library loading
    ///////////////////////////////////////////////////////////////////////////

    // Module registration
    // Creates a module table, registers the functions and constants, and adds it to the module cache
    // After this, the module can be imported via: import "module_name".
    // If make_global is true, also registers the module as a global variable.
    BEHL_API void create_module(State* S, std::string_view module_name, const ModuleDef& module_def, bool make_global);

    // Stack manipulation
    ///////////////////////////////////////////////////////////////////////////

    // Returns the index of the top element in the stack, counting from 0.
    BEHL_API int32_t get_top(State* S);

    // Resizes the stack to contain n elements, when n is negative, removes -n elements from the top of the stack,
    // if n is positive it will be the absolute size of the stack, new elements are filled with nil.
    BEHL_API void set_top(State* S, int32_t n);

    // Pops n elements from the top of the stack.
    BEHL_API void pop(State* S, int32_t n);

    // Duplicates the value at the given index and pushes it onto the top of the stack.
    BEHL_API void dup(State* S, int32_t idx);

    // Removes the value at the given index from the stack, shifting down elements above it.
    BEHL_API void remove(State* S, int32_t idx);

    // Moves the element at the top of the stack to the given index, shifting elements up.
    BEHL_API void insert(State* S, int32_t idx);

    // Push operations
    ///////////////////////////////////////////////////////////////////////////

    // Pushes a nil value onto the stack.
    BEHL_API void push_nil(State* S);

    // Pushes a boolean value onto the stack.
    BEHL_API void push_boolean(State* S, bool b);

    // Pushes an integer value onto the stack.
    BEHL_API void push_integer(State* S, Integer n);

    // Pushes a floating-point number onto the stack.
    BEHL_API void push_number(State* S, FP n);

    // Pushes a string onto the stack.
    BEHL_API void push_string(State* S, std::string_view str);

    // Pushes a C function onto the stack.
    BEHL_API void push_cfunction(State* S, CFunction f);

    // Value operations
    //////////////////////////////////////////////////////////////////////////

    // Returns the boolean value of the value at idx, if value is not a boolean it will return false.
    BEHL_API bool to_boolean(State* S, int32_t idx);

    // Returns an integer value at idx, if the type at idx is a number it will convert it if it fits into an integer,
    // otherwise returns 0.
    BEHL_API Integer to_integer(State* S, int32_t idx);

    // Returns the value at idx as a floating-point number, if conversion is not possible returns 0.0.
    BEHL_API FP to_number(State* S, int32_t idx);

    // Returns the string value at idx, if the value is not a string returns an empty string.
    BEHL_API std::string_view to_string(State* S, int32_t idx);

    // Returns a pointer to the userdata data at idx, if the value is not userdata returns nullptr.
    BEHL_API void* to_userdata(State* S, int32_t idx);

    // Type checking and conversion with error handling
    //////////////////////////////////////////////////////////////////////////

    // Checks that the value at idx is of type t, throws a type error if not.
    BEHL_API void check_type(State* S, int32_t idx, Type t);

    // Converts and returns the value at idx as the specified type, throws a type error if conversion is not possible,
    // if it is a number type it will only convert if it will not lose information.
    BEHL_API Integer check_integer(State* S, int32_t idx);

    // Checks if the value is a number and returns it as FP, throws a type error if not.
    BEHL_API FP check_number(State* S, int32_t idx);

    // Checks if the value is a string and returns it, throws a type error if not.
    BEHL_API std::string_view check_string(State* S, int32_t idx);

    // Checks if the value is a boolean and returns it, throws a type error if not.
    BEHL_API bool check_boolean(State* S, int32_t idx);

    // Checks if the value is userdata with the specified uid and returns a pointer to its data.
    // Throws a type error if not userdata or if the uid doesn't match.
    BEHL_API void* check_userdata(State* S, int32_t idx, uint32_t uid);

    // Type operations
    ///////////////////////////////////////////////////////////////////////////

    // Returns the type of the value at idx.
    BEHL_API Type type(State* S, int32_t idx);

    // Returns the name of the given type as a string view.
    BEHL_API std::string_view type_name(Type t);

    // Returns the name of the type of the value at idx as a string view.
    BEHL_API std::string_view value_typename(State* S, int32_t idx);

    // Type checking functions - return true if the value at idx is of the specified type
    BEHL_API bool is_nil(State* S, int32_t idx);
    BEHL_API bool is_boolean(State* S, int32_t idx);
    BEHL_API bool is_integer(State* S, int32_t idx);
    BEHL_API bool is_number(State* S, int32_t idx); // true for both integer and floating-point
    BEHL_API bool is_string(State* S, int32_t idx);
    BEHL_API bool is_table(State* S, int32_t idx);
    BEHL_API bool is_function(State* S, int32_t idx); // true for both closures and C functions
    BEHL_API bool is_cfunction(State* S, int32_t idx);
    BEHL_API bool is_userdata(State* S, int32_t idx);

    // Userdata operations
    ///////////////////////////////////////////////////////////////////////////

    // Creates a new userdata block with a unique identifier for type safety.
    // The uid should uniquely identify the userdata type (use make_uid() helper or your own ID).
    BEHL_API void* userdata_new(State* S, SysInt size, uint32_t uid);

    // Returns the UID of the userdata at the given index, or 0 if not userdata.
    BEHL_API uint32_t userdata_get_uid(State* S, int32_t idx);

    // GC pinning operations
    //////////////////////////////////////////////////////////////////////////

    // Pops the last stack value and pins it, returning a pin handle, avoids GC collection on that value.
    BEHL_API PinHandle pin(State* S);

    // Pushes the pinned value associated with the pin handle back onto the stack.
    BEHL_API void pinned_push(State* S, PinHandle pin_handle);

    // Unpins the value associated with the pin handle, allowing it to be GC collected.
    BEHL_API void unpin(State* S, PinHandle pin_handle);

    // Table operations
    //////////////////////////////////////////////////////////////////////////

    // Creates a new empty table and pushes it onto the stack.
    BEHL_API void table_new(State* S);

    // Pushes the raw value from the table at idx for the key at the top of the stack, pops the key, bypasses metamethods.
    BEHL_API void table_rawget(State* S, int32_t idx);

    // Pops the value and key from the top of the stack and sets the table at idx[key] = value, bypasses metamethods.
    BEHL_API void table_rawset(State* S, int32_t idx);

    // Pushes the raw value from the table at idx for the string key k, bypasses metamethods.
    BEHL_API void table_rawgetfield(State* S, int32_t idx, std::string_view k);

    // Sets the table at idx[k] = value, where value is popped from the stack, bypasses metamethods.
    BEHL_API void table_rawsetfield(State* S, int32_t idx, std::string_view k);

    // Implements the table iteration protocol, pushes next key and value onto the stack, returns false if no more elements,
    // bypasses metamethods (same as table_next, provided for API consistency).
    BEHL_API bool table_rawnext(State* S, int32_t idx);

    // Implements the table iteration protocol, pushes next key and value onto the stack, returns false if no more elements,
    // bypasses metamethods (next() always bypasses metamethods; use pairs() for metamethod support).
    BEHL_API bool table_next(State* S, int32_t idx);

    // Pushes the raw length of the table at idx onto the stack, bypasses metamethods.
    BEHL_API void table_rawlen(State* S, int32_t idx);

    // Pushes the value from the table at idx for the key at the top of the stack, pops the key, respects metamethods.
    BEHL_API void table_get(State* S, int32_t idx);

    // Pops the value and key from the top of the stack and sets the table at idx[key] = value, respects metamethods.
    BEHL_API void table_set(State* S, int32_t idx);

    // Pushes the value from the table at idx for the string key k, respects metamethods.
    BEHL_API void table_getfield(State* S, int32_t idx, std::string_view k);

    // Sets the table at idx[k] = value, where value is popped from the stack, respects metamethods.
    BEHL_API void table_setfield(State* S, int32_t idx, std::string_view k);

    // Pushes the length of the table at idx onto the stack, respects metamethods.
    BEHL_API void table_len(State* S, int32_t idx);

    // Assigns a name to the table at idx for debugging purposes.
    BEHL_API void table_setname(State* S, int32_t idx, std::string_view name);

    // Metatable operations
    //////////////////////////////////////////////////////////////////////////

    // Pushes the metatable of the table/userdata at idx onto the stack, returns false if no metatable.
    BEHL_API bool metatable_get(State* S, int32_t idx);

    // Sets the metatable of the table/userdata at idx to the table at the top of the stack, pops the metatable.
    BEHL_API void metatable_set(State* S, int32_t idx);

    // Creates a new metatable with the given name and stores it in the registry, then pushes it onto the stack.
    // Returns true if metatable was created, false if it already exists.
    // Either way, the metatable is pushed onto the stack.
    BEHL_API bool metatable_new(State* S, std::string_view name);

    // Finds a metatable with the given name in the registry and pushes it onto the stack.
    // Pushes nil if no metatable with that name exists.
    BEHL_API void metatable_find(State* S, std::string_view name);

    // Loading and executing code
    //////////////////////////////////////////////////////////////////////////

    // Loads a chunk from the given buffer and pushes the resulting function onto the stack, throws on error.
    BEHL_API void load_buffer(State* S, std::string_view str, std::string_view chunkname, bool optimize = true);

    // Nearly identical to load_buffer but uses "<string>" as the chunk name.
    BEHL_API void load_string(State* S, std::string_view str, bool optimize = true);

    BEHL_API void call(State* S, int32_t nargs, int32_t nresults);

    // Garbage collection control
    //////////////////////////////////////////////////////////////////////////

    // Runs a full garbage collection cycle.
    BEHL_API void gc_collect(State* S);

    // Performs a single step of garbage collection.
    BEHL_API void gc_step(State* S);

    // Standard library loading
    //////////////////////////////////////////////////////////////////////////

    // Loads all standard libraries (core, table, gc, debug, math, os, string).
    // - make_global: If true, modules are registered as global variables (e.g., string.upper()).
    //                If false, modules are only cached and require explicit import().
    BEHL_API void load_stdlib(State* S, bool make_global);

    BEHL_API void load_lib_core(State* S);                     // Core functions (print, typeof, tonumber, tostring, etc.)
    BEHL_API void load_lib_table(State* S, bool make_global);  // Table manipulation functions
    BEHL_API void load_lib_gc(State* S, bool make_global);     // Garbage collector controls
    BEHL_API void load_lib_debug(State* S, bool make_global);  // Debug utilities
    BEHL_API void load_lib_math(State* S, bool make_global);   // Math functions
    BEHL_API void load_lib_os(State* S, bool make_global);     // OS functions (time, exit, etc.)
    BEHL_API void load_lib_string(State* S, bool make_global); // String manipulation functions
    BEHL_API void load_lib_fs(State* S, bool make_global);     // Filesystem operations (security-sensitive, opt-in)

} // namespace behl
