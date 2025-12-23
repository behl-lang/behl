---
layout: default
title: API Reference
parent: Embedding
nav_order: 8
---

# API Reference
{: .no_toc }

Complete C++ API function reference.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Header Files

To use the behl API:
```cpp
#include <behl/behl.hpp>        // Core API
#include <behl/exceptions.hpp>  // Exception types
```

---

## State Management

### `new_state()`
```cpp
State* new_state()
```
Creates a new behl interpreter state.

### `close(State*)`
```cpp
void close(State* S)
```
Closes and cleans up the interpreter state.

### `load_stdlib(State*, bool)`
```cpp
void load_stdlib(State* S, bool make_global)
```
Loads all standard library modules.
- `make_global` - If `true`, modules are global; if `false`, require `import()`.

---

## Stack Operations

### `get_top(State*)`
```cpp
int32_t get_top(State* S)
```
Returns the number of elements on the stack.

### `set_top(State*, int32_t)`
```cpp
void set_top(State* S, int32_t idx)
```
Sets the stack size.

### `pop(State*, int32_t)`
```cpp
void pop(State* S, int32_t n)
```
Pops `n` values from the stack.

### `dup(State*, int32_t)`
```cpp
void dup(State* S, int32_t idx)
```
Duplicates the value at `idx` and pushes it.

### `remove(State*, int32_t)`
```cpp
void remove(State* S, int32_t idx)
```
Removes the value at `idx`.

### `insert(State*, int32_t)`
```cpp
void insert(State* S, int32_t idx)
```
Moves the top value to `idx`, shifting others.

---

## Pushing Values

### `push_nil(State*)`
```cpp
void push_nil(State* S)
```

### `push_boolean(State*, bool)`
```cpp
void push_boolean(State* S, bool value)
```

### `push_integer(State*, Integer)`
```cpp
void push_integer(State* S, Integer value)
```

### `push_number(State*, FP)`
```cpp
void push_number(State* S, FP value)
```

### `push_string(State*, std::string_view)`
```cpp
void push_string(State* S, std::string_view str)
```

### `push_cfunction(State*, CFunction)`
```cpp
void push_cfunction(State* S, CFunction func)
```
Pushes a C function onto the stack.

---

## Reading Values

### `type(State*, int32_t)`
```cpp
Type type(State* S, int32_t idx)
```
Returns the type of value at `idx`.
- Types: `kNil`, `kBoolean`, `kInteger`, `kNumber`, `kString`, `kTable`, `kClosure`, `kCFunction`, `kUserdata`

### `type_name(Type)`
```cpp
std::string_view type_name(Type t)
```
Returns the name of a type as a string.

### `value_typename(State*, int32_t)`
```cpp
std::string_view value_typename(State* S, int32_t idx)
```
Returns the type name of value at `idx`.

### `to_boolean(State*, int32_t)`
```cpp
bool to_boolean(State* S, int32_t idx)
```
Converts value to boolean.

### `to_integer(State*, int32_t)`
```cpp
Integer to_integer(State* S, int32_t idx)
```
Converts value to integer. Returns `0` if conversion fails.

### `to_number(State*, int32_t)`
```cpp
FP to_number(State* S, int32_t idx)
```
Converts value to number. Returns `0.0` if conversion fails.

### `to_string(State*, int32_t)`
```cpp
std::string_view to_string(State* S, int32_t idx)
```
Returns string value. Returns empty string if not a string.

### `to_userdata(State*, int32_t)`
```cpp
void* to_userdata(State* S, int32_t idx)
```
Returns pointer to userdata, or `nullptr` if not userdata.

---

## Type Checking

All `check_*` functions throw `TypeError` if validation fails.

### `check_type(State*, int32_t, Type)`
```cpp
void check_type(State* S, int32_t idx, Type expected)
```
Throws if value at `idx` is not of expected type.

### `check_integer(State*, int32_t)`
```cpp
Integer check_integer(State* S, int32_t idx)
```
Returns integer value. Throws if not an integer.

### `check_number(State*, int32_t)`
```cpp
FP check_number(State* S, int32_t idx)
```
Returns number value. Throws if not a number.

### `check_string(State*, int32_t)`
```cpp
std::string_view check_string(State* S, int32_t idx)
```
Returns string value. Throws if not a string.

### `check_boolean(State*, int32_t)`
```cpp
bool check_boolean(State* S, int32_t idx)
```
Returns boolean value. Throws if not a boolean.

### `check_userdata(State*, int32_t, uint32_t)`
```cpp
void* check_userdata(State* S, int32_t idx, uint32_t uid)
```
Returns userdata pointer. Throws if not userdata or UID mismatch.

---

## Global Variables

### `set_global(State*, std::string_view)`
```cpp
void set_global(State* S, std::string_view name)
```
Pops value from stack and assigns to global variable.

### `get_global(State*, std::string_view)`
```cpp
void get_global(State* S, std::string_view name)
```
Pushes the value of a global variable.

### `register_function(State*, std::string_view, CFunction)`
```cpp
void register_function(State* S, std::string_view name, CFunction func)
```
Registers a C function as a global function.

---

## Loading and Executing

### `load_string(State*, std::string_view, bool)`
```cpp
void load_string(State* S, std::string_view code, bool optimize = true)
```
Compiles a string and pushes resulting function. Throws `SyntaxError` or `ParserError` on compilation failure.

### `load_buffer(State*, std::string_view, std::string_view, bool)`
```cpp
void load_buffer(State* S, std::string_view code, 
                std::string_view chunkname, bool optimize = true)
```
Like `load_string` but with custom chunk name for error messages. Throws on error.

### `call(State*, int32_t, int32_t)`
```cpp
void call(State* S, int32_t nargs, int32_t nresults)
```
Calls function with `nargs` arguments, expecting `nresults` return values. Throws `RuntimeError`, `TypeError`, or other `BehlException` on error.

---

## Table Operations

### `table_new(State*)`
```cpp
void table_new(State* S)
```
Creates a new empty table and pushes it.

### `table_get(State*, int32_t)`
```cpp
void table_get(State* S, int32_t idx)
```
Gets value from table. Pops key, pushes value.

### `table_set(State*, int32_t)`
```cpp
void table_set(State* S, int32_t idx)
```
Sets value in table. Pops key and value.

### `table_rawget_field(State*, int32_t, std::string_view)`
```cpp
void table_rawget_field(State* S, int32_t idx, std::string_view field)
```
Gets field by string key (no metatable lookup).

### `table_rawset_field(State*, int32_t, std::string_view)`
```cpp
void table_rawset_field(State* S, int32_t idx, std::string_view field)
```
Sets field by string key (no metatable lookup). Pops value.

---

## Metatables

### `set_metatable(State*, int32_t)`
```cpp
void set_metatable(State* S, int32_t idx)
```
Sets metatable for value at `idx`. Pops the metatable from stack.

### `get_metatable(State*, int32_t)`
```cpp
bool get_metatable(State* S, int32_t idx)
```
Gets metatable of value at `idx` and pushes it. Returns `false` if no metatable.

---

## Userdata

### `userdata_new(State*, size_t, uint32_t)`
```cpp
void* userdata_new(State* S, size_t size, uint32_t uid)
```
Allocates userdata of given size with UID. Returns pointer and pushes userdata.

### `userdata_get_uid(State*, int32_t)`
```cpp
uint32_t userdata_get_uid(State* S, int32_t idx)
```
Returns UID of userdata at `idx`, or `0` if not userdata.

### `is_userdata(State*, int32_t)`
```cpp
bool is_userdata(State* S, int32_t idx)
```
Returns `true` if value at `idx` is userdata.

### `make_uid(std::string_view)`
```cpp
constexpr uint32_t make_uid(std::string_view name)
```
Generates unique 32-bit identifier from string using FNV-1a.

---

## Modules

### `create_module(State*, std::string_view, const ModuleDef&)`
```cpp
void create_module(State* S, std::string_view name, const ModuleDef& def)
```
Creates a module that can be imported from scripts.

**ModuleDef structure:**
```cpp
struct ModuleDef {
    std::span<const ModuleReg> funcs;
    std::span<const ModuleConst> consts;
};

struct ModuleReg {
    std::string_view name;
    CFunction func;
};

struct ModuleConst {
    std::string_view name;
    Value value;
};
```

---

## Error Handling

### `error(State*, std::string_view)`
```cpp
[[noreturn]] void error(State* S, std::string_view msg)
```
Throws a `RuntimeError` exception. Does not return.

### Exception Types

All behl exceptions inherit from `behl::BehlException`:

```cpp
namespace behl {
    class BehlException : public std::exception { };
    class SyntaxError : public BehlException { };      // Syntax errors
    class ParserError : public BehlException { };      // Parser errors
    class SemanticError : public BehlException { };    // Semantic errors
    class RuntimeError : public BehlException { };     // Runtime errors
    class TypeError : public BehlException { };        // Type errors
    class ReferenceError : public BehlException { };   // Undefined variables
    class ArithmeticError : public BehlException { };  // Math errors
}
```

**Usage:**
```cpp
try {
    behl::load_string(S, code);
    behl::call(S, 0, 0);
} catch (const behl::SyntaxError& e) {
    // Handle compile error
} catch (const behl::TypeError& e) {
    // Handle type error
} catch (const behl::RuntimeError& e) {
    // Handle runtime error
} catch (const behl::BehlException& e) {
    // Catch all behl errors
}
```

---

## Value Pinning

### `pin(State*)`
```cpp
PinHandle pin(State* S)
```
Pins value at top of stack to prevent GC. Removes value from stack.

### `pinned_push(State*, PinHandle)`
```cpp
void pinned_push(State* S, PinHandle handle)
```
Pushes a pinned value onto stack.

### `unpin(State*, PinHandle)`
```cpp
void unpin(State* S, PinHandle handle)
```
Releases a pinned value, allowing GC to collect it.

---

## Utilities

### `set_print_handler(State*, PrintHandler)`
```cpp
void set_print_handler(State* S, PrintHandler handler)
```
Sets custom print output handler.

**Handler signature:**
```cpp
void handler(State* S, std::string_view msg)
```

### `gc_collect(State*)`
```cpp
void gc_collect(State* S)
```
Runs a full garbage collection cycle.

---

## Type Definitions

```cpp
namespace behl {
    // Integer and float types (configurable)
    using Integer = int64_t;
    using FP = double;
    
    // C function signature
    using CFunction = int (*)(State* S);
    
    // Print handler signature
    using PrintHandler = void (*)(State* S, std::string_view msg);
    
    // Value pin handle
    using PinHandle = /* implementation-defined */;
    
    // Type enumeration
    enum class Type {
        kNil, kBoolean, kInteger, kNumber,
        kString, kTable, kClosure, kCFunction,
        kUserdata
    };
    
    // Special indices
    constexpr int32_t REGISTRY_INDEX = /* ... */;
}
```

---

## Constants

### `REGISTRY_INDEX`

Special stack index for the registry table. Use for storing metatables and other internal values.

```cpp
behl::table_rawset_field(S, behl::REGISTRY_INDEX, "MyType_mt");
```

---

## Next Steps

- See practical examples in [Examples](../examples)
- Review detailed guides for each topic
- Check the [Getting Started](getting-started) guide
