---
layout: default
title: Calling Scripts
parent: Advanced Topics
nav_order: 1
---

# Calling Scripts from C++
{: .no_toc }

How to load and execute Behl scripts from your C++ application.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Behl provides a straightforward API for embedding scripts in C++ applications. You can load scripts from strings or files, execute them, and retrieve results.

## Basic Setup

```cpp
#include <behl/behl.hpp>

int main() {
    // Create interpreter state
    behl::State* S = behl::new_state();
    
    // Load standard library
    behl::load_stdlib(S);
    
    // Your code here
    
    // Clean up
    behl::close(S);
    return 0;
}
```

## Loading Code

### From String

Use `load_string()` to load Behl code from a string:

```cpp
const char* code = R"(
    let x = 10;
    let y = 20;
    return x + y;
)";

try {
    behl::load_string(S, code);
    // Compiled function is now on the stack
} catch (const behl::SyntaxError& e) {
    std::cerr << "Compilation error: " << e.what() << "\n";
    return 1;
}
```

**Throws:** `SyntaxError` or `ParserError` on compilation failure.

**On success:** Pushes compiled function onto the stack.

### From File

Use `load_file()` to load from a file:

```cpp
try {
    behl::load_file(S, "script.behl");
    // Function is on stack
} catch (const behl::BehlException& e) {
    std::cerr << "Failed to load file: " << e.what() << "\n";
    return 1;
}
```

**Throws:** Exception on file not found or compilation error.

## Executing Code

After loading, use `call()` to execute:

```cpp
try {
    // Load the script
    behl::load_string(S, "return 2 + 3");
    
    // Call with 0 arguments, expecting 1 return value
    behl::call(S, 0, 1);
    
    // Get result
    int result = behl::to_integer(S, -1);
    behl::pop(S, 1);
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
}
```// Result is now on top of stack
int result = behl::to_integer(S, -1);
std::cout << "Result: " << result << "\n";  // 5

// Clean up stack
behl::pop(S, 1);
```

### Call Parameters

```cpp
bool call(State* S, int32_t nargs, int32_t nresults);
```

- **`nargs`** - Number of arguments (already pushed onto stack before the function)
- **`nresults`** - Number of expected return values

**Returns:** `true` on success, `false` on runtime error.

## Complete Example

```cpp
#include <behl/behl.hpp>
#include <iostream>

int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    
    // Load and execute a script
    const char* script = R"(
        let name = "Behl";
        let version = 1.0;
        
        print("Language: " + name);
        print("Version: " + tostring(version));
        
        return name + " v" + tostring(version);
    )";
    
    if (behl::load_string(S, script)) {
        if (behl::call(S, 0, 1)) {
            // Get return value
            auto result = behl::to_string(S, -1);
            std::cout << "Returned: " << result << "\n";
            behl::pop(S, 1);
        } else {
            std::cerr << "Execution error\n";
        }
    } else {
        std::cerr << "Compilation error\n";
    }
    
    behl::close(S);
    return 0;
}
```

**Output:**
```
Language: Behl
Version: 1.0
Returned: Behl v1.0
```

## Passing Arguments

Push arguments onto the stack before calling:

```cpp
const char* script = R"(
    // Arguments available as function parameters
    function add(a, b) {
        return a + b;
    }
    return add;
)";

// Load script (returns the add function)
behl::load_string(S, script);
behl::call(S, 0, 1);  // Get the function

// Now call the function with arguments
behl::push_integer(S, 10);
behl::push_integer(S, 20);
behl::call(S, 2, 1);  // 2 args, 1 result

int result = behl::to_integer(S, -1);
std::cout << "10 + 20 = " << result << "\n";  // 30
behl::pop(S, 1);
```

## Retrieving Results

### Single Return Value

```cpp
behl::load_string(S, "return 42");
behl::call(S, 0, 1);

int value = behl::to_integer(S, -1);
behl::pop(S, 1);
```

### Multiple Return Values

```cpp
const char* script = R"(
    function divmod(a, b) {
        return a / b, a % b;
    }
    return divmod(17, 5);
)";

behl::load_string(S, script);
behl::call(S, 0, 2);  // Expecting 2 results

int quotient = behl::to_integer(S, -2);   // First result
int remainder = behl::to_integer(S, -1);  // Second result

std::cout << "Quotient: " << quotient << "\n";   // 3
std::cout << "Remainder: " << remainder << "\n"; // 2

behl::pop(S, 2);
```

### No Return Value

```cpp
behl::load_string(S, "print('Hello, World!')");
behl::call(S, 0, 0);  // No return values expected
```

## Error Handling

### Compilation Errors

```cpp
const char* bad_code = "let x = ;";  // Syntax error

try {
    behl::load_string(S, bad_code);
} catch (const behl::SyntaxError& e) {
    std::cerr << "Compilation error: " << e.what() << "\n";
}
```

### Runtime Errors

```cpp
const char* code = "error('Something went wrong')";

try {
    behl::load_string(S, code);
    behl::call(S, 0, 0);
} catch (const behl::RuntimeError& e) {
    std::cerr << "Runtime error: " << e.what() << "\n";
}
```

### Exception-Based Error Handling

```cpp
try {
    behl::load_string(S, "return 1 / 0");
    behl::call(S, 0, 1);
    
    // Success - use result
    int result = behl::to_integer(S, -1);
    behl::pop(S, 1);
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

## Working with Global Variables

### Setting Globals from C++

```cpp
// Set a global variable before executing script
behl::push_integer(S, 42);
behl::set_global(S, "magic_number");

// Use in script
behl::load_string(S, "print('Magic: ' + tostring(magic_number))");
behl::call(S, 0, 0);
```

### Reading Globals from C++

```cpp
// Execute script that sets globals
behl::load_string(S, "global_value = 100");
behl::call(S, 0, 0);

// Read the global
behl::get_global(S, "global_value");
int value = behl::to_integer(S, -1);
std::cout << "Global value: " << value << "\n";  // 100
behl::pop(S, 1);
```

## Executing Multiple Scripts

```cpp
behl::State* S = behl::new_state();
behl::load_stdlib(S);

// First script sets up data
behl::load_string(S, R"(
    let config = {
        ["width"] = 800,
        ["height"] = 600
    };
)");
behl::call(S, 0, 0);

// Second script uses the data
behl::load_string(S, R"(
    print("Resolution: " + tostring(config.width) + "x" + tostring(config.height));
)");
behl::call(S, 0, 0);

behl::close(S);
```

## Module Loading

Scripts can import modules if the standard library is loaded:

```cpp
behl::State* S = behl::new_state();
behl::load_stdlib(S);

const char* script = R"(
    const math = import("math");
    print("PI = " + tostring(math.PI));
)";

behl::load_string(S, script);
behl::call(S, 0, 0);
```

## Performance Tips

### Compile Once, Execute Many Times

Store the compiled function and call it multiple times:

```cpp
// Compile once
behl::load_string(S, R"(
    function process(data) {
        return data * 2;
    }
    return process;
)");
behl::call(S, 0, 1);  // Get the function

// Pin it for safe storage
behl::PinHandle func = behl::pin(S);

// Execute multiple times
for (int i = 0; i < 1000; i++) {
    behl::pinned_push(S, func);
    behl::push_integer(S, i);
    behl::call(S, 1, 1);
    int result = behl::to_integer(S, -1);
    behl::pop(S, 1);
}

// Clean up
behl::unpin(S, func);
```

### Reuse State

Creating a new state is expensive. Reuse states when possible:

```cpp
behl::State* S = behl::new_state();
behl::load_stdlib(S);

for (const auto& script : scripts) {
    behl::load_string(S, script.c_str());
    behl::call(S, 0, 0);
    behl::set_top(S, 0);  // Clear stack between scripts
}

behl::close(S);
```

## Common Patterns

### Configuration Files

```cpp
bool load_config(const char* path, Config& config) {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    
    try {
        behl::load_file(S, path);
        behl::call(S, 0, 1);
        
        // Expect a table
        if (!behl::is_table(S, -1)) {
            behl::close(S);
            return false;
        }
        
        // Read configuration
        behl::get_field(S, -1, "width");
        config.width = behl::to_integer(S, -1);
        behl::pop(S, 1);
        
        behl::get_field(S, -1, "height");
        config.height = behl::to_integer(S, -1);
        behl::pop(S, 1);
        
        behl::close(S);
        return true;
    } catch (const behl::BehlException& e) {
        std::cerr << "Error: " << e.what() << "\n";
        behl::close(S);
        return false;
    }
}
```

### Script Validation

```cpp
bool validate_script(const char* code) {
    behl::State* S = behl::new_state();
    try {
        behl::load_string(S, code);
        behl::close(S);
        return true;
    } catch (const behl::BehlException&) {
        behl::close(S);
        return false;
    }
}
```

### Sandboxed Execution

```cpp
behl::State* S = behl::new_state();
// Don't load stdlib or limit what's loaded
behl::load_lib_math(S, true);  // Only math functions

behl::load_string(S, user_code);
behl::call(S, 0, 0);
behl::close(S);
```

## See Also

- [API Reference](embedding/api-reference) - Complete API documentation
- [Exposing C++ Functions](embedding/getting-started#complete-example) - Making C++ functions callable from scripts
- [Callbacks and Pinning](callbacks) - Storing script functions in C++
- [Examples](examples) - More code examples
