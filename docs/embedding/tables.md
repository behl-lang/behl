---
layout: default
title: Tables
parent: Embedding
nav_order: 4
---

# Tables
{: .no_toc }

Create and manipulate Behl tables from C++.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Tables are Behl's primary data structure, serving as arrays, dictionaries, and objects. Tables are 0-indexed (unlike Lua's 1-indexed tables).

---

## Creating Tables

### `table_new(State*)`

Creates a new empty table and pushes it onto the stack.

```cpp
behl::table_new(S);  // Stack: [table]
```

---

## Table Operations

### `table_get(State*, int32_t)`

Gets a value from a table. Pops key, pushes value.

**Stack:** `[... table ... key]` → `[... table ... value]`

```cpp
// Get table["field"]
behl::push_string(S, "field");   // Stack: [table, "field"]
behl::table_get(S, -2);          // Stack: [table, value]
```

### `table_set(State*, int32_t)`

Sets a value in a table. Pops key and value.

**Stack:** `[... table ... key value]` → `[... table ...]`

```cpp
// Set table["field"] = 42
behl::push_string(S, "field");   // Stack: [table, "field"]
behl::push_integer(S, 42);       // Stack: [table, "field", 42]
behl::table_set(S, -3);          // Stack: [table]
```

### `table_rawget_field(State*, int32_t, std::string_view)`

Get a field by string key directly (no metatable lookup).

```cpp
behl::table_rawget_field(S, -1, "name");  // Get table["name"]
```

### `table_rawset_field(State*, int32_t, std::string_view)`

Set a field by string key directly (no metatable lookup). Pops value.

```cpp
behl::push_integer(S, 42);
behl::table_rawset_field(S, -2, "answer");  // table["answer"] = 42
```

---

## Examples

### Create and Populate Table

```cpp
// Create empty table
behl::table_new(S);

// Set fields
behl::push_integer(S, 100);
behl::table_rawset_field(S, -2, "health");

behl::push_string(S, "Player");
behl::table_rawset_field(S, -2, "name");

behl::push_boolean(S, true);
behl::table_rawset_field(S, -2, "alive");

// Table is now: {health: 100, name: "Player", alive: true}
// Stack: [table]
```

### Read Table Fields

```cpp
// Assume table is on stack
behl::table_rawget_field(S, -1, "health");  // Stack: [table, health_value]
int health = behl::to_integer(S, -1);
behl::pop(S, 1);  // Stack: [table]

behl::table_rawget_field(S, -1, "name");    // Stack: [table, name_value]
std::string_view name = behl::to_string(S, -1);
behl::pop(S, 1);  // Stack: [table]
```

### Array-Style Tables

```cpp
// Create array [10, 20, 30, 40]
behl::table_new(S);

for (int i = 0; i < 4; ++i) {
    behl::push_integer(S, i);           // Key (0-indexed)
    behl::push_integer(S, (i + 1) * 10); // Value
    behl::table_set(S, -3);             // Set table[i] = value
}

// Stack: [table]
```

### Pass Table to Script

```cpp
// Create configuration table
behl::table_new(S);
behl::push_integer(S, 800);
behl::table_rawset_field(S, -2, "width");
behl::push_integer(S, 600);
behl::table_rawset_field(S, -2, "height");

// Set as global
behl::set_global(S, "config");

// Now accessible in scripts:
// print(config["width"]);  // Output: 800
```

### Receive Table from Script

```cpp
const char* script = R"(
    return { x = 10, y = 20, name = "Point" };
)";

if (behl::load_string(S, script)) {
    if (behl::call(S, 0, 1)) {  // Returns 1 table
        // Stack: [table]
        
        behl::table_rawget_field(S, -1, "x");
        int x = behl::to_integer(S, -1);
        behl::pop(S, 1);
        
        behl::table_rawget_field(S, -1, "y");
        int y = behl::to_integer(S, -1);
        behl::pop(S, 1);
        
        std::cout << "Point: (" << x << ", " << y << ")\n";
        
        behl::pop(S, 1);  // Pop table
    }
}
```

---

## Metatables

### `set_metatable(State*, int32_t)`

Sets a metatable for the value at index. Pops the metatable from stack.

```cpp
behl::table_new(S);  // Create object
behl::table_new(S);  // Create metatable

// Add __index to metatable
behl::push_cfunction(S, my_index_func);
behl::table_rawset_field(S, -2, "__index");

// Set metatable
behl::set_metatable(S, -2);  // Pops metatable, attaches to object
```

### `get_metatable(State*, int32_t)`

Gets the metatable of the value at index and pushes it onto stack.

```cpp
if (behl::get_metatable(S, -1)) {
    // Metatable is on stack
} else {
    // No metatable
}
```

---

## Next Steps

- Expose C++ objects with [Userdata](userdata)
- Create reusable APIs with [Modules](modules)
- Handle errors gracefully with [Error Handling](error-handling)
