---
layout: default
title: Core
parent: Standard Library
nav_order: 4
---

# Core
{: .no_toc }

Global functions available without importing.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

These functions are available in the global scope when `load_stdlib()` is called. They don't require importing.

---

## `print(...)`

Prints values to standard output, separated by tabs.

```cpp
print("Hello");              // Hello
print(42, "answer", true);   // 42    answer    true
```

---

## `typeof(value)`

Returns the type of a value as a string.

```cpp
print(typeof(42));        // "integer"
print(typeof(3.14));      // "number"
print(typeof("hello"));   // "string"
print(typeof(true));      // "boolean"
print(typeof(nil));       // "nil"
print(typeof({}));        // "table"
print(typeof(print));     // "function"
```

**Returns:** `"nil"`, `"boolean"`, `"integer"`, `"number"`, `"string"`, `"table"`, `"function"`, or `"userdata"`

---

## `typeid(value)`

Returns the numeric type ID of a value.

```cpp
let id = typeid(42);
print(id);  // Type ID as integer
```

---

## `tostring(value)` / `tonumber(value)`

Type conversion functions (optimized built-ins).

```cpp
let s = tostring(42);       // "42"
let n = tonumber("123");    // 123

// tostring respects __tostring metamethod
let t = {};
setmetatable(t, {
    __tostring = function(obj) {
        return "CustomTable";
    }
});
print(tostring(t));  // "CustomTable"
```

---

## `error(message)`

Raises a runtime error with the given message.

```cpp
function validate(x) {
    if (x < 0) {
        error("Value must be non-negative!");
    }
}

validate(-5);  // RuntimeError: Value must be non-negative!
```

---

## `pcall(func, ...)`

Calls a function in protected mode, catching any errors.

```cpp
function risky() {
    error("Something failed!");
}

let success, result = pcall(risky);
if (success) {
    print("Success: " + tostring(result));
} else {
    print("Error caught: " + result);
}
```

**Returns:**
- On success: `true, return_values...`
- On error: `false, error_message`

---

## `pairs(table)`

Returns an iterator for iterating over table key-value pairs.

```cpp
let t = {
    name = "Alice",
    age = 30,
    [0] = "first"
};

for (key, value in pairs(t)) {
    print(key + " = " + tostring(value));
}
```

---

## `import(module_name)`

Loads and returns a module.

```cpp
const math = import("math");
print(math.PI);  // 3.14159...
```

Modules are cached, so multiple imports return the same table. See [Module System](../modules) for details.

---

## `getmetatable(table)` / `setmetatable(table, metatable)`

Get or set a table's metatable.

```cpp
let t = {};
let mt = {
    __tostring = function(obj) {
        return "MyTable";
    }
};

setmetatable(t, mt);
print(tostring(t));  // "MyTable"

let retrieved = getmetatable(t);
// retrieved == mt
```

---

## `rawget(table, key)`

Get value from table without invoking `__index` metamethod.

```cpp
let t = {x = 10};
setmetatable(t, {__index = function() { return "default"; }});

print(t.missing);            // "default" (via __index)
print(rawget(t, "missing")); // nil (bypasses __index)
```

---

## `rawset(table, key, value)`

Set value in table without invoking `__newindex` metamethod.

**Returns:** The table

```cpp
let t = {};
setmetatable(t, {__newindex = function() { error("Read-only"); }});

t.key = 1;            // Error: "Read-only"
rawset(t, "key", 1);  // Sets directly, no error
```

---

## `rawlen(table)`

Returns the length of the array part of a table (consecutive integer keys starting from 0).

```cpp
let t = {10, 20, 30};
print(rawlen(t));  // 3

let sparse = {[0] = 1, [5] = 2};
print(rawlen(sparse));  // 1 (only counts consecutive from 0)

setmetatable(t, {__len = function() { return 999; }});
print(#t);          // 999 (via __len)
print(rawlen(t));   // 3 (raw length, bypasses metamethod)
```
