---
layout: default
title: Getting Started
nav_order: 1
parent: Guides
---

# Getting Started
{: .no_toc }

Get up and running with Behl in minutes.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Installation

### Prerequisites

- **CMake** 3.26 or higher
- **C++20** compatible compiler:
  - MSVC 2022+
  - GCC 11+
  - Clang 14+

### Building from Source

```bash
# Clone the repository
git clone https://github.com/behl-lang/behl.git
cd behl

# Configure with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build (cross-platform)
cmake --build build --config Release

# Run tests (optional)
cd build
ctest -C Release --output-on-failure
```

**Note:** The executable will be located in:
- Multi-config generators (MSVC, Xcode): `build/Release/behl[.exe]`
- Single-config generators (Make, Ninja): `build/behl`

### Installation

```bash
# Install system-wide (optional)
sudo cmake --install build
```

---

## Your First Script

### Hello, World!

Create a file `hello.behl`:

```cpp
print("Hello, World!");
```

Run it:

```bash
# Run the script
./behl hello.behl
```

**Note:** On Windows, use `behl.exe` instead of `behl`.

Output:
```
Hello, World!
```

### Variables and Types

```cpp
// Variables with let
let x = 42;
let name = "behl";
let pi = 3.14159;
let isActive = true;

// Constants
const MAX_SIZE = 1000;

// Type checking
print(typeof(x));        // "integer"
print(typeof(pi));       // "number"
print(typeof(name));     // "string"
print(typeof(isActive)); // "boolean"
```

### Functions

```cpp
// Define a function
function greet(name) {
    return "Hello, " + name + "!";
}

// Call it
let message = greet("World");
print(message); // "Hello, World!"

// Functions are first-class values
let fn = greet;
print(fn("behl")); // "Hello, behl!"
```

### Control Flow

```cpp
// If/else
let x = 10;
if (x > 5) {
    print("x is greater than 5");
} elseif (x == 5) {
    print("x is 5");
} else {
    print("x is less than 5");
}

// While loop
let i = 0;
while (i < 5) {
    print(i);
    i = i + 1;
}

// For loop
for (let j = 0; j < 5; j++) {
    print(j);
}
```

### Tables

Tables are the primary data structure in Behl - they work as arrays, dictionaries, and objects:

```cpp
// Array-like (0-indexed)
let arr = {10, 20, 30, 40};
print(arr[0]); // 10
print(arr[3]); // 40

// Dictionary-like
let person = {
    ["name"] = "Alice",
    ["age"] = 30,
    ["city"] = "Seattle"
};
print(person["name"]); // "Alice"

// Iteration with pairs
for (k, v in pairs(person)) {
    print(k + ": " + tostring(v));
}
```

---

## Interactive Mode (REPL)

Start the interactive read-eval-print loop without any arguments:

```bash
./behl
```

This launches an interactive session where you can type Behl code and see results immediately:

```
Behl REPL
> let x = 42
> print(x)
42
> function greet(name) { return "Hello, " + name }
> greet("World")
Hello, World
> exit()
```

---

## Debugging

### Viewing Bytecode

You can dump the compiled bytecode to understand how your code is executed:

```bash
# Dump bytecode to console
./behl -b script.behl
```

### Error Messages

Behl provides detailed error messages with stack traces:

```cpp
function divide(a, b) {
    if (b == 0) {
        error("Division by zero!");
    }
    return a / b;
}

divide(10, 0); // RuntimeError: Division by zero!
```

### Protected Calls

Use `pcall` to catch errors:

```cpp
function risky() {
    error("Something went wrong!");
}

let success, result = pcall(risky);
if (!success) {
    print("Error caught: " + result);
}
```

---

## Standard Library

Load the standard library for access to built-in functions and modules:

```cpp
// When embedding in C++
behl::State* S = behl::new_state();

// Option 1: Make modules globally accessible
behl::load_stdlib(S);
// Usage: string.upper("hello"), math.sqrt(16)

// Option 2: Require explicit import() for better control
behl::load_stdlib(S);
// Usage: let str = import("string"); str.upper("hello")
```

### Available Modules

- **Core** - `typeof()`, `pairs()`, `import()`, `pcall()`, `error()`
- **Math** - Math functions and constants
- **String** - String manipulation functions
- **Table** - Table utilities
- **OS** - Operating system functions

### Importing Modules

```cpp
const math = import("math");
print(math.PI);        // 3.14159...
print(math.sqrt(16));  // 4
```

---

## Next Steps

- Explore the [Language](language/) reference for complete syntax details
- Check out the [Standard Library](standard-library) documentation
- Learn about [compiler optimizations](optimizations) and performance
- Learn about [embedding Behl](embedding/) in C++ applications
- Read [Examples](examples) for practical code samples

---

## Getting Help

- **GitHub Issues** - Report bugs or request features
- **Discussions** - Ask questions and share ideas
- **Examples** - Check the `docs/examples.md` documentation
