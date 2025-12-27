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

The table module provides utilities for manipulating tables. It's available as a global or can be explicitly imported:

```cpp
// Direct access (implicit global)
table.insert(arr, 10);

// Or explicit import
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

## table.insert(t, pos, value)

Inserts a value at the specified position, shifting other elements.

```cpp
let t = {10, 20, 30};
table.insert(t, 1, 15);
// t is now {10, 15, 20, 30}

print(t[1]);  // 15
```

**Parameters:**
- `t` - Table to insert into
- `pos` - Position to insert at (0-indexed)
- `value` - Value to insert

---

## table.remove(t, pos)

Removes and returns the element at the specified position.

```cpp
let t = {10, 20, 30};
let val = table.remove(t, 1);

print(val);   // 20
print(t[0]);  // 10
print(t[1]);  // 30 (shifted down)
print(rawlen(t));  // 2
```

**Parameters:**
- `t` - Table to remove from
- `pos` - Position to remove (0-indexed)

**Returns:** The removed value

---

## table.print(t)

Debug print of table contents. Useful for inspecting table structure.

```cpp
let t = {
    name = "Alice",
    age = 30,
    [0] = "first",
    [1] = "second"
};

table.print(t);
// Output shows all key-value pairs
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

// Insert in the middle
table.insert(numbers, 1, 15);
// numbers = {10, 15, 20, 30}

// Remove elements
let removed = table.remove(numbers, 2);
print(removed);  // 20
// numbers = {10, 15, 30}

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

- `table.insert` with one argument appends to the end
- `table.insert` with position shifts elements to make room
- `table.remove` shifts remaining elements down
- All positions are **0-indexed** (unlike Lua's 1-indexed)
- These functions work on the array part of tables (consecutive integer keys from 0)
