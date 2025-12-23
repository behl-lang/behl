# Behl

Lua-inspired scripting language with C-like syntax, implemented in C++20.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

## Features

- **C-like Syntax** - Optional `;` terminators, `{}` blocks, and C-style comments
- **Dynamic Typing** - Lua-inspired type system with runtime flexibility
- **First-Class Functions** - Functions as values with closures and lexical scoping
- **Powerful Tables** - 0-indexed associative arrays (unlike Lua's 1-indexed)
- **Modern VM** - Register-based bytecode with incremental garbage collection
- **Metatables** - Customize table behavior with metamethods
- **C++ API** - Easy embedding with a Lua-like API
- **Optimizations** - Constant folding, loop optimization, tail call optimization, dead store elimination

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

## Installation

### Prerequisites

- **CMake** 3.26 or later
- **C++20** compiler (MSVC 2022+, GCC 11+, Clang 14+)

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

The executable will be located in:
- Multi-config generators (MSVC, Xcode): `build/Release/behl[.exe]`
- Single-config generators (Make, Ninja): `build/behl`

## Getting Started

Create a file `hello.behl`:

```cpp
print("Hello, World!")

let name = "Behl"
print("Welcome to " + name + "!")
```

Run it:

```bash
./behl hello.behl
```

## Language Highlights

### Variables and Types

```cpp
let x = 42                     // integer
let pi = 3.14159               // number (float)
let name = "Behl"              // string
let active = true              // boolean
let data = nil                 // nil

const MAX_SIZE = 1000          // constant (semicolons optional)
```

### Control Flow

```cpp
// If/else with elseif
if (x > 10) {
    print("Greater")
} elseif (x == 10) {
    print("Equal")
} else {
    print("Less")
}

// C-style for loops
for (let i = 0; i < 10; i++) {
    print(i)
}

// While loops
while (condition) {
    // ...
}
```

### Functions

```cpp
function greet(name) {
    return "Hello, " + name + "!"
}

// Functions are first-class values
let fn = greet
print(fn("World"))
```

### Tables (0-indexed)

```cpp
// Array-like
let arr = {10, 20, 30}
print(arr[0])  // 10 (first element)

// Dictionary-like
let person = {
    ["name"] = "Alice",
    ["age"] = 30
}
print(person["name"])

// Iteration
for (k, v in pairs(person)) {
    print(k + ": " + tostring(v))
}
```

## Embedding in C++

```cpp
#include <behl/behl.hpp>

int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S, true);  // true = modules globally accessible
    
    // Load and run a script
    behl::load_string(S, "return 2 + 3");
    behl::call(S, 0, 1);
    
    int result = behl::to_integer(S, -1);
    // result == 5
    
    behl::close(S);
    return 0;
}
```

Link against the `behl` library:

```cmake
target_link_libraries(your_target PRIVATE behl)
```

## Documentation

 **[Full Documentation](https://behl-lang.github.io/behl/)**

The complete documentation is available online and includes:

**Language & Core:**
- **[Getting Started](https://behl-lang.github.io/behl/getting-started.html)** - Installation and first steps
- **[Language Reference](https://behl-lang.github.io/behl/language/)** - Complete syntax and semantics
- **[Standard Library](https://behl-lang.github.io/behl/standard-library.html)** - Built-in functions and modules

**C++ Integration:**
- **[Embedding Guide](https://behl-lang.github.io/behl/embedding/)** - Complete C++ API documentation
- **[Examples](https://behl-lang.github.io/behl/examples.html)** - Code samples and patterns

**Advanced:**
- **[Optimizations](https://behl-lang.github.io/behl/optimizations.html)** - Compiler optimizations and performance
- **[Differences from Lua](https://behl-lang.github.io/behl/differences-from-lua.html)** - For Lua developers

## Project Structure

```
behl/
├── src/                  # Source code
│   ├── frontend/         # Lexer, parser, semantic analysis
│   ├── backend/          # Compiler, bytecode generation
│   ├── vm/               # Virtual machine execution
│   ├── optimization/     # AST optimization passes
│   ├── api/              # Public C++ API
│   └── gc/               # Garbage collector
├── include/              # Public headers
├── tests/                # Unit tests (GoogleTest)
├── docs/                 # Documentation
└── vscode-extension/     # VS Code language support
```

## Key Differences from Lua

- **C-like syntax**: Optional `;` terminators, `{}` blocks, `//` and `/* */` comments
- **0-indexed tables**: First element is at index 0, not 1
- **Operators**: `**` for power (not `^`), `!=` for not equal (not `~=`)
- **Logical operators**: `&&`, `||`, `!` instead of `and`, `or`, `not`
- **String concatenation**: `+` instead of `..`
- **Type introspection**: `typeof()` instead of `type()`
- **Module system**: `import()` instead of `require()`
- **Variable declarations**: Explicit `let` and `const` keywords required

## Testing

Run the test suite:

```bash
cd build
ctest -C Release --output-on-failure
```

## License

Behl is distributed under the MIT License. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Acknowledgments

Behl is inspired by Lua's elegant design and semantics, while bringing the familiarity of C-like syntax to scripting.
