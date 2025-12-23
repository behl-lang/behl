---
layout: default
title: Language
nav_order: 3
has_children: true
---

# Language

Complete language reference for Behl syntax and semantics.

Behl combines the simplicity and flexibility of Lua's semantics with the familiar syntax of C-style languages.

## Key Differences from C

- **Dynamic typing** - Variables don't have fixed types
- **Tables** - Primary data structure (arrays + dictionaries)
- **0-indexed** - Arrays start at index 0
- **First-class functions** - Functions are values
- **Garbage collection** - Automatic memory management
- **Multiple returns** - Functions can return multiple values

## Key Differences from Lua

- **C-like syntax** - Uses `;` terminators and `{}` blocks
- **0-indexed tables** - Unlike Lua's 1-indexed tables
- **C-style comments** - `//` and `/* */` instead of `--` and `--[[]]`
- **Logical operators** - `&&` `||` `!` instead of `and` `or` `not`
- **Power operator** - `**` instead of `^`
- **Bitwise operators** - `&` `|` `^` `~` instead of functions

See [Differences from Lua](../differences-from-lua) for complete details.

## Quick Example

```cpp
// C-like syntax with dynamic typing
function fibonacci(n) {
    if (n <= 1) {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

let result = fibonacci(10)
print(result) // 55
```
