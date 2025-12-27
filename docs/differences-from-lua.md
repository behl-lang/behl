---
layout: default
title: Differences from Lua
parent: Advanced Topics
nav_order: 4
---

# Differences from Lua
{: .no_toc }

Key differences between Behl and Lua for developers familiar with Lua.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Behl is inspired by Lua's semantics but uses C-like syntax. If you're familiar with Lua, this guide will help you understand the key differences.

---

## Syntax Differences

### Statement Terminators

**Lua:**
```lua
local x = 10
print(x)
```

**behl:**
```cpp
let x = 10
print(x)
```

Semicolons `;` are **optional** in Behl (automatic semicolon insertion like JavaScript).

### Blocks and Scope

**Lua:**
```lua
if condition then
    -- code
end

while condition do
    -- code
end

for i = 1, 10 do
    -- code
end
```

**behl:**
```cpp
if (condition) {
    // code
}

while (condition) {
    // code
}

for (let i = 0; i < 10; i++) {
    // code
}
```

Behl uses `{}` braces for blocks instead of `do...end` or `then...end`.

### Comments

**Lua:**
```lua
-- Single line comment
--[[ Multi-line
     comment ]]
```

**behl:**
```cpp
// Single line comment
/* Multi-line
   comment */
```

### Variable Declaration

**Lua:**
```lua
local x = 10
```

**behl:**
```cpp
let x = 10;
const PI = 3.14;  // Immutable
```

Behl requires explicit `let` or `const` keywords.

### Functions

**Lua:**
```lua
function add(a, b)
    return a + b
end

local multiply = function(a, b)
    return a * b
end
```

**behl:**
```cpp
function add(a, b) {
    return a + b;
}

let multiply = function(a, b) {
    return a * b;
};
```

### String Concatenation

**Lua:**
```lua
local s = "Hello" .. " " .. "World"
local msg = "Count: " .. tostring(42)
```

**behl:**
```cpp
let s = "Hello" + " " + "World";
let msg = "Count: " + tostring(42);
```

Behl uses `+` instead of `..` for concatenation.

### Logical Operators

**Lua:**
```lua
if a and b then end
if a or b then end
if not a then end
```

**behl:**
```cpp
if (a && b) {}
if (a || b) {}
if (!a) {}
```

Behl uses C-style logical operators.

### Comparison Operators

**Lua:**
```lua
if a ~= b then end  -- Not equal
```

**behl:**
```cpp
if (a != b) {}  // Not equal
```

### Elseif vs elseif

**Lua:**
```lua
if condition1 then
    -- code
elseif condition2 then
    -- code
else
    -- code
end
```

**behl:**
```cpp
if (condition1) {
    // code
} elseif (condition2) {
    // code
} else {
    // code
}
```

Behl uses `elseif` (one word, like Lua), but with C-like syntax.

---

## Table Indexing

### 0-Indexed vs 1-Indexed

**CRITICAL DIFFERENCE:** behl tables are **0-indexed**, Lua tables are **1-indexed**.

**Lua:**
```lua
local t = {10, 20, 30}
print(t[1])  -- 10
print(t[3])  -- 30
```

**behl:**
```cpp
let t = {10, 20, 30};
print(t[0]);  // 10
print(t[2]);  // 30
```

### For Loops

**Lua:**
```lua
for i = 1, 10 do
    print(i)  -- 1, 2, 3, ..., 10
end
```

**behl:**
```cpp
for (let i = 0; i < 10; i++) {
    print(i);  // 0, 1, 2, ..., 9
}
```

### Iteration

**Lua:**
```lua
for k, v in pairs(t) do
    print(k, v)
end
```

**Behl:**
```cpp
for (k, v in pairs(t)) {
    print(k + ": " + tostring(v));
}
```

Note the parentheses around the iterator expression and variables in Behl.

---

## Operators

### Power Operator

**Lua:**
```lua
local x = 2 ^ 8  -- 256
```

**behl:**
```cpp
let x = 2 ** 8;  // 256
```

Behl uses `**` for exponentiation (like Python).

### Bitwise Operators

**Lua 5.3+:**
```lua
local a = x & y   -- AND
local b = x | y   -- OR
local c = x ~ y   -- XOR
local d = ~x      -- NOT
local e = x << 1  -- Left shift
local f = x >> 1  -- Right shift
```

**Behl:**
```cpp
let a = x & y;    // AND
let b = x | y;    // OR
let c = x ^ y;    // XOR (Behl uses ^ instead of ~)
let d = ~x;       // NOT
let e = x << 2;   // Left shift
let f = x >> 2;   // Right shift
```

**Note:** In Behl, `^` is XOR (not power). Power is `**`.

---

## Type System

### Type Names

Both languages have similar types, but with minor name differences:

| Lua | behl |
|-----|------|
| `nil` | `nil` |
| `boolean` | `boolean` |
| `number` | `integer` or `number` |
| `string` | `string` |
| `function` | `function` |
| `table` | `table` |
| `userdata` | `userdata` |

**Key difference:** Behl distinguishes between `integer` (int64) and `number` (double), whereas Lua 5.3+ has integer and float subtypes of number.

### Type Checking

**Lua:**
```lua
local t = type(x)
if t == "number" then
    -- ...
end
```

**behl:**
```cpp
let t = typeof(x);
if (t == "integer" || t == "number") {
    // ...
}
```

Behl uses `typeof()` instead of `type()`.

---

## Modules and Imports

### Module System

**Lua:**
```lua
local math = require("math")
print(math.pi)
print(math.sqrt(16))
```

**behl:**
```cpp
const math = import("math");
print(math.PI);
print(math.sqrt(16));
```

- Behl uses `import()` instead of `require()`
- Behl supports both dot notation (`.`) and bracket notation (`[]`) for table access

---

## Functions

### Multiple Return Values

Both support multiple returns, but syntax differs:

**Lua:**
```lua
function divmod(a, b)
    return a / b, a % b
end

local q, r = divmod(10, 3)
```

**behl:**
```cpp
function divmod(a, b) {
    return a / b, a % b;
}

let q, r = divmod(10, 3);
```

### Varargs

**Lua:**
```lua
function sum(...)
    local args = {...}
    local total = 0
    for _, v in ipairs(args) do
        total = total + v
    end
    return total
end
```

**Behl:**
```cpp
function sum(...) {
    let total = 0;
    for (let i = 0; i < rawlen(...); i++) {
        total = total + (...)[i];
    }
    return total;
}

sum(1, 2, 3, 4);
```

Behl fully supports varargs with similar semantics. See [Varargs](../language/varargs) for details.

---

## Global Variables

### Assignment

**Lua:**
```lua
x = 10  -- Global (no 'local')
```

**behl:**
```cpp
let x = 10;  // Must use 'let' or 'const'
```

Behl requires explicit variable declarations. There is no implicit global assignment.

### Table Construction

**Lua:**
```lua
local t = {name = "Alice", age = 30}
print(t.name)
```

**behl:**
```cpp
let t = { name = "Alice", age = 30 };
print(t.name);  // Or t["name"]
```

Behl supports both `key = value` shorthand (for identifiers) and `["key"] = value` (for any expression).

---

## Standard Library

### Common Differences

| Lua | Behl | Notes |
|-----|------|-------|
| `type()` | `typeof()` | Type introspection |
| `#t` | `rawlen(t)` | Table length |
| `t[#t+1]` | `t[rawlen(t)]` | Append (0-indexed) |
| `math.pi` | `math.PI` | Constants in uppercase |
| `string.len(s)` | `string.len(s)` | Dot notation supported |

### Missing Features

Behl currently does **not** support:
- `...` (varargs)
- `select()`
- `ipairs()` (use `pairs()` instead, remember 0-indexing)
- Lua 5.4+ attributes like `<const>` and `<close>`
- Coroutines (`coroutine` library)
- Weak tables

---

## Metatables

### Metamethods

Behl supports metatables with similar metamethods to Lua:

**Lua:**
```lua
local mt = {
    __tostring = function(t)
        return "MyTable"
    end,
    __add = function(a, b)
        return a.value + b.value
    end
}
setmetatable(t, mt)
```

**behl:**
```cpp
let mt = {
    ["__tostring"] = function(t) {
        return "MyTable";
    },
    ["__add"] = function(a, b) {
        return a["value"] + b["value"];
    }
};
setmetatable(t, mt);
```

**Supported metamethods:**
- **Arithmetic:** `__add`, `__sub`, `__mul`, `__div`, `__mod`, `__pow`, `__unm`
- **Bitwise:** `__band`, `__bor`, `__bxor`, `__bnot`, `__shl`, `__shr`
- **Comparison:** `__eq`, `__lt`, `__le`
- **Table:** `__index`, `__newindex`, `__len`, `__pairs`
- **Other:** `__call`, `__tostring`, `__gc`

---

## Error Handling

### Raising Errors

**Lua:**
```lua
error("Something went wrong")
```

**behl:**
```cpp
error("Something went wrong");
```

Same function name and behavior.

### Protected Calls

**Lua:**
```lua
local success, result = pcall(risky_function)
if not success then
    print("Error: " .. result)
end
```

**behl:**
```cpp
let success, result = pcall(risky_function);
if (!success) {
    print("Error: " + result);
}
```

Similar behavior, different syntax.

---

## C API

### Comparison

Both have similar C/C++ APIs for embedding:

**Lua C API:**
```c
lua_State* L = luaL_newstate();
lua_pushinteger(L, 42);
lua_setglobal(L, "answer");
lua_close(L);
```

**behl C++ API:**
```cpp
behl::State* S = behl::new_state();
behl::push_integer(S, 42);
behl::set_global(S, "answer");
behl::close(S);
```

The behl API is designed to be familiar to Lua developers while leveraging C++20 features internally.

---

## Performance

Behl aims to be competitive with Lua:
- Similar register-based VM architecture
- Incremental garbage collection
- Competitive performance in many benchmarks

---

## Migration Checklist

When porting Lua code to behl:

- [ ] Change `--` comments to `//` or `/* */`
- [ ] Replace `then...end` / `do...end` with `{...}`
- [ ] Change `local` to `let` or `const`
- [ ] Replace `..` concatenation with `+`
- [ ] Replace `and`, `or`, `not` with `&&`, `||`, `!`
- [ ] Replace `~=` with `!=`
- [ ] Change `^` (power) to `**`
- [ ] Change bitwise XOR from `~` to `^`
- [ ] **Adjust all array indices from 1-based to 0-based**
- [ ] Change `type()` to `typeof()`
- [ ] Change `require()` to `import()`

---

## Summary

Behl keeps Lua's powerful semantics (dynamic typing, first-class functions, metatables, GC) while adopting C-style syntax for familiarity. The biggest gotcha is **0-indexed tables** - everything else is mostly syntactic sugar.
