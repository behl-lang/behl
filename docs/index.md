---
layout: default
title: Home
nav_order: 1
description: "Lua-inspired scripting language with C-like syntax, implemented in C++20"
permalink: /
---

# Behl Language
{: .fs-9 }

Lua-inspired scripting language with C-like syntax, implemented in C++20.
{: .fs-6 .fw-300 }

[Get Started](#getting-started){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[View on GitHub](https://github.com/behl-lang/behl){: .btn .fs-5 .mb-4 .mb-md-0 }

---

## What is Behl?

**Behl** combines the simplicity and flexibility of Lua's semantics with the familiar syntax of C-style languages. It features:

- **C-like Syntax** - Familiar `;` terminators, `{}` blocks, and C-style comments
- **Dynamic Typing** - Lua-inspired type system with runtime flexibility
- **First-Class Functions** - Functions are values that can be passed around
- **Tables** - 0-indexed associative arrays (unlike Lua's 1-indexed)
- **Modern VM** - Register-based bytecode with incremental garbage collection
- **C++ API** - Easy embedding with a Lua-like API: `push_integer()`, `call()`, etc.
- **Fast Performance** - Competitive with Lua in many benchmarks

## Quick Example

```cpp
// C-like syntax with dynamic typing
function fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

let result = fibonacci(10);
print(result); // 55
```

## Getting Started

### Requirements

- **CMake** 3.26 or later
- **C++20** compiler (MSVC 2022+, GCC 11+, Clang 14+)

### Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
cd build
ctest -C Release
```

### Your First Script

Create a file `hello.behl`:

```cpp
print("Hello, behl!");

let name = "World";
print("Hello, " + name);
```

Run it:

```bash
./behl hello.behl
```

## Key Features

### Familiar Syntax

If you know C, JavaScript, or Java, you already know Behl's syntax:

```cpp
// Variables with let
let x = 10;
const PI = 3.14159;

// For loops
for (let i = 0; i < 10; i++) {
    print(i);
}

// If/else
if (x > 5) {
    print("Greater");
} else {
    print("Smaller");
}
```

### Lua Semantics

Under the hood, Behl follows Lua's powerful model:

- **Dynamic typing** with `typeof()` introspection
- **Tables** as the primary data structure
- **Closures** and lexical scoping
- **Metatables** for customizing behavior
- **Garbage collection** for automatic memory management

### Easy Embedding

```cpp
#include <behl/behl.hpp>

int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    
    // Load and run a script
    behl::load_string(S, "return 2 + 3");
    behl::call(S, 0, 1);
    
    int result = behl::to_integer(S, -1);
    // result == 5
    
    behl::close(S);
    return 0;
}
```

## Documentation

### Guides

Tutorials and practical examples:

- [Getting Started](getting-started) - Installation, first program, basic concepts
- [Examples](examples) - Code samples and patterns

### Language

Complete language reference:

- [Syntax](language/lexical) - Comments, keywords, and structure
- [Values & Types](language/types) - Numbers, strings, booleans, and more
- [Variables](language/variables) - Declarations, scope, and constants
- [Operators](language/operators) - Arithmetic, logical, and bitwise
- [Control Flow](language/control-flow) - If/else, loops, and branches
- [Functions](language/functions) - Definitions, closures, and calls
- [Tables](language/tables) - Arrays, dictionaries, and metatables
- [Error Handling](language/error-handling) - Errors and protected calls
- [Modules](language/modules-lang) - Importing and organizing code

### API

Standard library and embedding:

**Standard Library:**
- [Core Functions](standard-library) - print, typeof, pcall, and more
- [Math Module](stdlib/math) - Mathematical operations
- [String Module](stdlib/string) - String manipulation
- [Table Module](stdlib/table) - Table utilities
- [OS Module](stdlib/os) - Operating system interface
- [FS Module](stdlib/fs) - File system operations
- [Debug Module](stdlib/debug) - Debugging utilities
- [GC Module](stdlib/gc) - Garbage collector control

**C++ Embedding:**
- [Embedding Overview](embedding/) - Integrate Behl in C++ applications
- [Getting Started](embedding/getting-started) - State management and setup
- [Stack Operations](embedding/stack-operations) - Manipulate values
- [Calling Functions](embedding/calling-functions) - Execute scripts
- [Tables](embedding/tables) - Work with tables from C++
- [Userdata](embedding/userdata) - Expose C++ objects
- [Error Handling](embedding/error-handling) - Handle errors
- [Creating Modules](embedding/modules) - Build C++ modules
- [Debugging](embedding/debugging) - Debug API for tools
- [API Reference](embedding/api-reference) - Complete function list

### Advanced Topics

Technical documentation and advanced usage patterns:

- [Calling Scripts](calling-scripts) - Execute Behl from C++
- [Callbacks](callbacks) - Store script functions in C++
- [Optimizations](optimizations) - Performance and compiler details
- [Differences from Lua](differences-from-lua) - For Lua developers

## License

Behl is distributed under the MIT License. See LICENSE file for details.
