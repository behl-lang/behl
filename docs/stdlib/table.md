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

## table.insert(t, [pos,] value)

With two arguments, appends `value` at index `rawlen(t)`. With three arguments, inserts `value` at the 0-based position `pos` and shifts the elements from `pos` up to `rawlen(t) - 1` one slot up, like Lua's `table.insert`. The table is written raw, `__newindex` is not invoked.

- `pos` must be an integer in `[0, rawlen(t)]`, otherwise it raises `bad argument #2 to 'insert' (position out of bounds)`. `pos == rawlen(t)` appends.
- Any argument count other than two or three raises `wrong number of arguments to 'insert'`, so `table.insert(t)` is an error.

```cpp
let t = {10, 20};
table.insert(t, 30);
// t is now {10, 20, 30}

table.insert(t, 0, 5);
// t is now {5, 10, 20, 30}

table.insert(t, 2, 15);
// t is now {5, 10, 15, 20, 30}
```

---

## table.print(t, options)

Debug print of table contents. Useful for inspecting table structure. Prints exactly what `table.dump(t, options)` returns. An optional `options` table accepts `indent` (spaces per level, default 4), `compact` (disable newlines, default false), and `max_recursion` (max nesting depth, default 100). Option fields with the wrong type are ignored. See `table.dump` for the output format.

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

Returns the length of the array part of a table (consecutive integer keys starting from 0), or the length in bytes of a string or buffer. Equivalent to the global `rawlen`. Raises `bad argument #1 (expected table, string or buffer, got ...)` for any other value or a missing argument.

```cpp
let t = {10, 20, 30};
print(table.rawlen(t));  // 3
```

---

## table.rawget(t, key)

Get a value from a table without invoking the `__index` metamethod. A `nil` or NaN key returns `nil`. Raises `bad argument #1 (expected table, got ...)` for a non-table and `bad argument #2 to 'rawget' (value expected)` if the key is missing.

```cpp
let t = {x = 10};
print(table.rawget(t, "x"));  // 10
```

---

## table.rawset(t, key, value)

Set a value in a table without invoking the `__newindex` metamethod. Setting a key to `nil` clears it.

- Raises `bad argument #1 (expected table, got ...)` for a non-table, and `bad argument #2 to 'rawset' (value expected)` or `bad argument #3 to 'rawset' (value expected)` if the key or the value is missing (pass `nil` explicitly to clear a key).
- A `nil` key raises `index is nil`, a NaN key raises `index is NaN`.

**Returns:** The table

```cpp
let t = {};
table.rawset(t, "x", 10);
```

---

## table.dump(t, options)

Returns a string representation of a table's contents, in the same format as `table.print`. Accepts the same optional `options` table (`indent`, `compact`, `max_recursion`).

- A table whose keys are exactly `0` to `n - 1` is written as a list: `{1, 2, 3}`. Any other table is written as `[key] = value` pairs: `{["a"] = 1, [2.5] = 0}`. Entries whose value is `nil` are not shown.
- Strings are quoted, numbers and booleans are written as literals. Functions, C functions, userdata and buffers are written as their raw `tostring` form, for example `function:0x...`, `cfunction:0x...`, `userdata:0x...` and `buffer:0x...` (`__tostring` is not called).
- A table nested deeper than `max_recursion`, or a table that contains itself (directly or through other tables), is written as `{...}`. Cycles always terminate, whatever `max_recursion` is, and deep nesting does not overflow the C stack. A table that appears twice without being its own ancestor is written out both times.
- `indent` must be between 0 and 1024; a negative value raises `bad argument #2 to 'dump' (indent must not be negative)`.

```cpp
let t = {10, 20, 30};
print(table.dump(t));
// {
//     10,
//     20,
//     30
// }

print(table.dump({a = 1}, {compact = true}));  // {["a"] = 1}

let c = {};
c[0] = c;
print(table.dump(c, {compact = true}));  // {{...}}
```

---

## table.unpack(t, start, end)

Returns the values of `t` from index `start` to `end`, inclusive, as multiple return values. Both bounds are 0-indexed and optional, defaulting to the full array part (`0` to `rawlen(t) - 1`); out-of-range bounds are clamped. A bound that is given must be an integer (an integral float such as `1.0` is accepted); anything else, such as `1.5`, `"a"` or a table, raises `bad argument #2` or `bad argument #3`.

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

- `table.insert(t, value)` appends; `table.insert(t, pos, value)` inserts and shifts
- Table indices are **0-indexed** (unlike Lua's 1-indexed)
- These functions work on the array part of tables (consecutive integer keys from 0)
