---
layout: default
title: Stack Operations
parent: Embedding
nav_order: 2
---

# Stack Operations
{: .no_toc }

Manipulate values on the behl stack.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The behl API uses a stack-based model similar to Lua. Values are manipulated on the stack using integer indices:
- **Positive indices** (0, 1, 2, ...) count from bottom
- **Negative indices** (-1, -2, -3, ...) count from top
- `-1` is the top of the stack

**Stack diagram:**
```
 4  |  value  |  <- top (index -1 or 3)
 3  |  value  |  <- index -2 or 2
 2  |  value  |  <- index -3 or 1
 1  |  value  |  <- bottom (index -4 or 0)
```

---

## Stack Management

### `get_top(State*)`

Returns the number of elements on the stack.

```cpp
int n = behl::get_top(S);
```

### `set_top(State*, int32_t)`

Sets the stack size. Can be used to pop multiple values.

```cpp
behl::set_top(S, 0);  // Clear stack
behl::set_top(S, 5);  // Ensure stack has 5 elements (fills with nil)
```

### `pop(State*, int32_t)`

Pops `n` values from the stack.

```cpp
behl::pop(S, 1);  // Pop top value
behl::pop(S, 2);  // Pop top 2 values
```

---

## Stack Manipulation

### `dup(State*, int32_t)`

Duplicates the value at index and pushes it onto the stack.

```cpp
behl::dup(S, -1);  // Duplicate top value
behl::dup(S, 0);   // Duplicate bottom value
```

### `remove(State*, int32_t)`

Removes the value at index, shifting down values above it.

```cpp
behl::remove(S, 0);   // Remove bottom value
behl::remove(S, -2);  // Remove second from top
```

### `insert(State*, int32_t)`

Moves the top value to the given index, shifting other values.

```cpp
behl::insert(S, 0);  // Move top to bottom
behl::insert(S, -2); // Insert before second from top
```

---

## Pushing Values

### `push_nil(State*)`

```cpp
behl::push_nil(S);
```

### `push_boolean(State*, bool)`

```cpp
behl::push_boolean(S, true);
behl::push_boolean(S, false);
```

### `push_integer(State*, Integer)`

```cpp
behl::push_integer(S, 42);
behl::push_integer(S, -100);
```

### `push_number(State*, FP)`

```cpp
behl::push_number(S, 3.14);
behl::push_number(S, -2.5);
```

### `push_string(State*, std::string_view)`

```cpp
behl::push_string(S, "Hello, World!");
behl::push_string(S, std::string("C++ string"));
```

### `push_cfunction(State*, CFunction)`

Pushes a C function onto the stack.

```cpp
int my_function(behl::State* S) {
    int x = behl::to_integer(S, 0);
    behl::push_integer(S, x * 2);
    return 1;  // Number of return values
}

behl::push_cfunction(S, my_function);
```

---

## Reading Values

### `type(State*, int32_t)`

Returns the type of the value at the given index.

```cpp
behl::Type t = behl::type(S, -1);

if (t == behl::Type::kInteger) {
    // Handle integer
} else if (t == behl::Type::kString) {
    // Handle string
}
```

**Types:**
- `Type::kNil`
- `Type::kBoolean`
- `Type::kInteger`
- `Type::kNumber`
- `Type::kString`
- `Type::kTable`
- `Type::kClosure` / `Type::kCFunction`
- `Type::kUserdata`

### `type_name(Type)` / `value_typename(State*, int32_t)`

Get the name of a type as a string.

```cpp
std::string_view name = behl::type_name(behl::Type::kInteger);  // "integer"
std::string_view name2 = behl::value_typename(S, -1);
```

### `to_boolean(State*, int32_t)`

Converts value to boolean. Returns `false` for `nil` and `false`, `true` otherwise.

```cpp
bool b = behl::to_boolean(S, -1);
```

### `to_integer(State*, int32_t)`

Converts value to integer. Returns 0 if conversion fails.

```cpp
behl::Integer n = behl::to_integer(S, 0);
```

### `to_number(State*, int32_t)`

Converts value to floating-point. Returns 0.0 if conversion fails.

```cpp
behl::FP f = behl::to_number(S, 0);
```

### `to_string(State*, int32_t)`

Returns string value. Returns empty string if not a string.

```cpp
std::string_view str = behl::to_string(S, -1);
```

---

## Type Checking

These functions throw `TypeError` if the value is not of the expected type.

### `check_type(State*, int32_t, Type)`

```cpp
behl::check_type(S, 0, behl::Type::kInteger);
```

**Throws:** `TypeError` with descriptive message if type doesn't match.

### `check_integer(State*, int32_t)`

```cpp
behl::Integer n = behl::check_integer(S, 0);
```

**Returns:** The integer value.
**Throws:** `TypeError` if not an integer.

### `check_number(State*, int32_t)`

```cpp
behl::FP f = behl::check_number(S, 0);
```

**Returns:** The number value.
**Throws:** `TypeError` if not a number.

### `check_string(State*, int32_t)`

```cpp
std::string_view s = behl::check_string(S, 0);
```

**Returns:** The string value.
**Throws:** `TypeError` if not a string.

### `check_boolean(State*, int32_t)`

```cpp
bool b = behl::check_boolean(S, 0);
```

**Returns:** The boolean value.
**Throws:** `TypeError` if not a boolean.

---

## Global Variables

### `set_global(State*, std::string_view)`

Pops a value from the stack and assigns it to a global variable.

```cpp
behl::push_integer(S, 42);
behl::set_global(S, "answer");
// Now `answer` is accessible in behl scripts
```

### `get_global(State*, std::string_view)`

Pushes the value of a global variable onto the stack.

```cpp
behl::get_global(S, "answer");
int n = behl::to_integer(S, -1);
```

### `register_function(State*, std::string_view, CFunction)`

Registers a C function as a global function.

```cpp
int my_add(behl::State* S) {
    int a = behl::to_integer(S, 0);
    int b = behl::to_integer(S, 1);
    behl::push_integer(S, a + b);
    return 1;
}

behl::register_function(S, "add", my_add);
// Now `add(2, 3)` is available in behl scripts
```

---

## Next Steps

- Learn how to [Call Functions](calling-functions) and execute scripts
- Work with [Tables](tables) for complex data structures
- Expose C++ types with [Userdata](userdata)
