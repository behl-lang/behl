---
layout: default
title: Standard Library
nav_order: 4
has_children: true
---

# Standard Library
{: .no_toc }

Built-in modules and core functions available in Behl.
{: .fs-6 .fw-300 }

---

## Overview

The Behl standard library provides core functionality through global functions and modules. When `load_stdlib(S)` is called, it loads:

- **Core Functions** - Global functions like `print()`, `typeof()`, `import()`
- **Standard Modules** - `math`, `string`, `table`, `os`, `gc`, `debug`

### Loading the Standard Library

```cpp
// When embedding in C++
behl::State* S = behl::new_state();
behl::load_stdlib(S);
```

### Accessing Modules

All standard library modules must be explicitly imported using `import()`:

```cpp
// Import modules explicitly
const math = import("math");
const string = import("string");
const table = import("table");

// Then use them
print(math.PI);
let upper = string.upper("hello");
table.insert(arr, value);
```

**Available modules:**
- `math` - Mathematical functions and constants
- `string` - String manipulation utilities
- `table` - Table operations
- `os` - Operating system functions
- `gc` - Garbage collector control
- `debug` - Debugging utilities

See [Module System](modules) for details.

---

## Quick Reference

### Core Functions
- [Core Functions](stdlib/core) - `print()`, `typeof()`, `tostring()`, `import()`, `error()`, `pcall()`, etc.

### Modules
- [Math Module](stdlib/math) - Mathematical functions and constants
- [String Module](stdlib/string) - String manipulation
- [Table Module](stdlib/table) - Table utilities
- [OS Module](stdlib/os) - Operating system interface
- [GC Module](stdlib/gc) - Garbage collector control
- [Debug Module](stdlib/debug) - Debugging utilities

---

## Example Usage

```cpp
// Using core functions
print("Hello, World!");
let t = typeof(42);  // "integer"

// Import modules explicitly
const math = import("math");
const string = import("string");
const table = import("table");

// Use imported modules
let angle = math.PI / 4;
let upper = string.upper("hello");
let sine = math.sin(angle);
let reversed = string.reverse(upper);
table.insert(arr, 10);

// Error handling
function safeDivide(a, b) {
    if (b == 0) {
        error("Division by zero");
    }
    return a / b;
}

let success, result = pcall(safeDivide, 10, 0);
if (!success) {
    print("Error: " + result);
}
```
