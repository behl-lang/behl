---
layout: default
title: table
parent: Standard Library
nav_order: 1
---

# table
{: .no_toc }

Table manipulation utilities.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The table module provides utilities for manipulating tables. It must be explicitly imported:

```cpp
const table = import("table");
table.insert(arr, 10);
```

---

## table.insert(t, value)

Appends a value to the end of the table's array part.

```cpp
let t = {10, 20};
table.insert(t, 30);
// t is now {10, 20, 30}

print(t[2]);  // 30
```

---

## table.print(t, options)

Debug print of table contents. Useful for inspecting table structure. An optional `options` table accepts `indent` (spaces per level, default 4), `compact` (disable newlines, default false), and `max_recursion` (max nesting depth, default 100).

```cpp
let t = {
    name = "Alice",
    age = 30,
    [0] = "first",
    [1] = "second"
};

table.print(t);
// Output shows all key-value pairs

table.print(t, {compact = true});
// Prints on a single line
```

---

## table.rawlen(t)

Returns the length of the array part of a table (consecutive integer keys starting from 0). Equivalent to the global `rawlen`.

```cpp
let t = {10, 20, 30};
print(table.rawlen(t));  // 3
```

---

## table.rawget(t, key)

Get a value from a table without invoking the `__index` metamethod.

```cpp
let t = {x = 10};
print(table.rawget(t, "x"));  // 10
```

---

## table.rawset(t, key, value)

Set a value in a table without invoking the `__newindex` metamethod.

**Returns:** The table

```cpp
let t = {};
table.rawset(t, "x", 10);
```

---

## table.dump(t, options)

Returns a string representation of a table's contents, in the same format as `table.print`. Accepts the same optional `options` table (`indent`, `compact`, `max_recursion`).

```cpp
let t = {10, 20, 30};
print(table.dump(t));
```

---

## table.unpack(t, start, end)

Returns the values of `t` from index `start` to `end`, inclusive, as multiple return values. Both bounds are 0-indexed and optional, defaulting to the full array part (`0` to `rawlen(t) - 1`); out-of-range bounds are clamped.

```cpp
let t = {10, 20, 30};
let a, b, c = table.unpack(t);
// a = 10, b = 20, c = 30

let x, y = table.unpack(t, 0, 1);
// x = 10, y = 20
```

---

## table.set_name(t, name)

Sets a debug name for a table.

```cpp
let t = {};
table.set_name(t, "MyTable");
```

---

## Example Usage

```cpp
const table = import("table");

// Build an array
let numbers = {};
table.insert(numbers, 10);
table.insert(numbers, 20);
table.insert(numbers, 30);
print(rawlen(numbers));  // 3

// Debug output
table.print(numbers);

// Working with mixed tables
let mixed = {
    name = "Config",
    [0] = "item1",
    [1] = "item2"
};

table.insert(mixed, "item3");
// Array part: {item1, item2, item3}
// Hash part: {name = "Config"}

table.print(mixed);
```

---

## Notes

- `table.insert(t, value)` always appends to the end of the array part
- Table indices are **0-indexed** (unlike Lua's 1-indexed)
- These functions work on the array part of tables (consecutive integer keys from 0)
