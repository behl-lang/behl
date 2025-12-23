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

The Behl standard library provides core functionality through global functions and modules. When `load_stdlib(S, make_global)` is called, it loads:

- **Core Functions** - Global functions like `print()`, `typeof()`, `import()`
- **Standard Modules** - `math`, `string`, `table`, `os`, `gc`, `debug`

### Loading the Standard Library

```cpp
// When embedding in C++
behl::State* S = behl::new_state();

// Make modules globally accessible (convenient for quick scripts)
behl::load_stdlib(S, true);
// Usage: string.upper("hello"), math.sqrt(16)

// Or require explicit import() for better control/sandboxing
behl::load_stdlib(S, false);
// Usage: let str = import("string"); str.upper("hello")
```

### Global vs Import-Only Modules

**With `make_global = true`**, modules are automatically available as global variables:

- `math` - Mathematical functions and constants
- `string` - String manipulation utilities
- `table` - Table operations
- `os` - Operating system functions
- `gc` - Garbage collector control
- `debug` - Debugging utilities

These can be accessed directly:

```cpp
print(math.PI);           // Access directly
let upper = string.upper("hello");
table.insert(arr, value);
```

**With `make_global = false`**, modules must be explicitly imported:

```cpp
const math = import("math");     // Same as global math
print(math.PI);
```

### Module Mode

In module mode (files starting with `module;`), globals are not accessible. You must explicitly import:

```cpp
module;

const math = import("math");
const string = import("string");

// Now use them
print(math.PI);
```

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

// Using modules directly (implicit globals)
let angle = math.PI / 4;
let upper = string.upper("hello");
table.insert(arr, 10);

// Or with explicit imports
const math = import("math");
const string = import("string");

let sine = math.sin(angle);
let reversed = string.reverse(upper);

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
