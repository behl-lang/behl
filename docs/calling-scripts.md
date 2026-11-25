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

Loading and calling report errors through return values, not exceptions. See [Error Handling](embedding/error-handling) for the full error model.

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

if (behl::load_string(S, code) != 0) {
    std::cerr << "Compilation error: " << behl::to_string(S, -1) << "\n";
    return 1;
}
// Compiled function is now on the stack
```

**Returns:** `0` on success and pushes the compiled function onto the stack. On failure returns `behl::kErrorSyntax` (or `behl::kErrorMemory`) and pushes the error message instead. It does not throw.

### From File

There is no file-loading entry point in the public API. Read the file yourself and hand the
contents to `load_buffer()`, which lets you supply the chunk name used in error messages:

```cpp
#include <fstream>
#include <sstream>

std::ifstream file("script.behl");
if (!file) {
    std::cerr << "Failed to open file\n";
    return 1;
}

std::ostringstream buffer;
buffer << file.rdbuf();
const std::string source = buffer.str();

if (behl::load_buffer(S, source, "script.behl") != 0) {
    std::cerr << "Failed to load file: " << behl::to_string(S, -1) << "\n";
    return 1;
}
// Function is on stack
```

**Returns:** the same status codes as `load_string()`. Opening the file is your responsibility.

## Executing Code

After loading, use `call()` to execute:

```cpp
// Load the script
if (behl::load_string(S, "return 2 + 3") != 0) {
    std::cerr << "Compilation error: " << behl::to_string(S, -1) << "\n";
    return 1;
}

// Call with 0 arguments, expecting 1 return value
if (behl::call(S, 0, 1) < 0) {
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    return 1;
}

// Result is now on top of stack
behl::Integer result = behl::to_integer(S, -1);
std::cout << "Result: " << result << "\n";  // 5

// Clean up stack
behl::pop(S, 1);
```

### Call Parameters

```cpp
[[nodiscard]] int32_t call(State* S, int32_t nargs, int32_t nresults);
```

- **`nargs`** - Number of arguments (pushed onto the stack after the function)
- **`nresults`** - Number of expected return values, or `behl::kMultRet` for all of them

**Returns:** the number of results pushed (`>= 0`) on success. On error returns `behl::kErrorRuntime` or `behl::kErrorMemory`; the function and its arguments are removed from the stack and the error value is pushed in their place. Script errors are not thrown. The error value is whatever was raised, so it is not always a string.

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
    
    if (behl::load_string(S, script) != 0 || behl::call(S, 0, 1) < 0) {
        std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    } else {
        // Get return value
        auto result = behl::to_string(S, -1);
        std::cout << "Returned: " << result << "\n";
    }
    behl::pop(S, 1);
    
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
if (behl::load_string(S, script) != 0 || behl::call(S, 0, 1) < 0) {
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
    return 1;
}

// Now call the function with arguments
behl::push_integer(S, 10);
behl::push_integer(S, 20);
if (behl::call(S, 2, 1) < 0) {  // 2 args, 1 result
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
    return 1;
}

behl::Integer result = behl::to_integer(S, -1);
std::cout << "10 + 20 = " << result << "\n";  // 30
behl::pop(S, 1);
```

## Retrieving Results

### Single Return Value

```cpp
if (behl::load_string(S, "return 42") == 0 && behl::call(S, 0, 1) >= 0) {
    behl::Integer value = behl::to_integer(S, -1);
}
behl::pop(S, 1);  // The result or the error value
```

### Multiple Return Values

```cpp
const char* script = R"(
    function divmod(a, b) {
        return a / b, a % b;
    }
    return divmod(17, 5);
)";

if (behl::load_string(S, script) != 0 || behl::call(S, 0, 2) < 0) {  // Expecting 2 results
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
    return 1;
}

behl::FP quotient = behl::to_number(S, -2);        // First result, `/` is float division
behl::Integer remainder = behl::to_integer(S, -1); // Second result

std::cout << "Quotient: " << quotient << "\n";   // 3.4
std::cout << "Remainder: " << remainder << "\n"; // 2

behl::pop(S, 2);
```

With `nresults` set to `behl::kMultRet`, the return value of `call` tells you how many results were pushed:

```cpp
if (behl::load_string(S, "return 1, 2, 3") == 0) {
    const int32_t n = behl::call(S, 0, behl::kMultRet);
    if (n >= 0) {
        std::cout << n << " results\n";  // 3 results
        behl::pop(S, n);
    } else {
        behl::pop(S, 1);
    }
}
```

### No Return Value

```cpp
if (behl::load_string(S, "print('Hello, World!')") != 0 || behl::call(S, 0, 0) < 0) {
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

## Error Handling

### Compilation Errors

```cpp
const char* bad_code = "let x = ;";  // Syntax error

if (behl::load_string(S, bad_code) == behl::kErrorSyntax) {
    // "<string>(line,col): SyntaxError: ..."
    std::cerr << "Compilation error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

### Runtime Errors

```cpp
const char* code = "error('Something went wrong')";

if (behl::load_string(S, code) != 0) {
    behl::pop(S, 1);
} else if (behl::call(S, 0, 0) == behl::kErrorRuntime) {
    // "Something went wrong", exactly as raised by the script's error() call
    std::cerr << "Runtime error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

### Non-String Error Values

Scripts can raise any value, for example `error({code = 1})`. The error value on the stack is then a table, and `behl::to_string` returns an empty string for it. Check the type before reading it:

```cpp
if (behl::load_string(S, "error({code = 1})") != 0) {
    behl::pop(S, 1);
} else if (behl::call(S, 0, 0) < 0) {
    if (behl::is_table(S, -1)) {
        behl::table_getfield(S, -1, "code");
        std::cerr << "Error code: " << behl::to_integer(S, -1) << "\n";
        behl::pop(S, 1);
    }
    behl::pop(S, 1);
}
```

### C++ Exceptions from C Functions

`call` does not catch C++ exceptions thrown by your own C functions (anything not raised through `behl::error`, `behl::error_value` or a `check_*` function). They pass through `call` to your code. `std::bad_alloc` is handled differently: it is reported as `behl::kErrorMemory`.

## Working with Global Variables

### Setting Globals from C++

```cpp
// Set a global variable before executing script
behl::push_integer(S, 42);
behl::set_global(S, "magic_number");

// Use in script
if (behl::load_string(S, "print('Magic: ' + tostring(magic_number))") != 0 || behl::call(S, 0, 0) < 0) {
    std::cerr << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

### Reading Globals from C++

```cpp
// Execute script that sets globals
if (behl::load_string(S, "global_value = 100") != 0 || behl::call(S, 0, 0) < 0) {
    std::cerr << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}

// Read the global
behl::get_global(S, "global_value");
behl::Integer value = behl::to_integer(S, -1);
std::cout << "Global value: " << value << "\n";  // 100
behl::pop(S, 1);
```

## Executing Multiple Scripts

```cpp
bool run(behl::State* S, std::string_view code) {
    if (behl::load_string(S, code) != 0 || behl::call(S, 0, 0) < 0) {
        std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
        behl::pop(S, 1);
        return false;
    }
    return true;
}

behl::State* S = behl::new_state();
behl::load_stdlib(S);

// First script sets up data
run(S, R"(
    let config = {
        ["width"] = 800,
        ["height"] = 600
    };
)");

// Second script uses the data
run(S, R"(
    print("Resolution: " + tostring(config.width) + "x" + tostring(config.height));
)");

behl::close(S);
```

## Module Loading

Scripts can import modules if the standard library is loaded:

```cpp
behl::State* S = behl::new_state();
behl::load_stdlib(S);

const char* script = R"(
    const math = import("math");
    print("pi = " + tostring(math.pi));
)";

if (behl::load_string(S, script) != 0 || behl::call(S, 0, 0) < 0) {
    std::cerr << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

## Performance Tips

### Compile Once, Execute Many Times

Store the compiled function and call it multiple times:

```cpp
// Compile once
if (behl::load_string(S, R"(
    function process(data) {
        return data * 2;
    }
    return process;
)") != 0 || behl::call(S, 0, 1) < 0) {  // Get the function
    std::cerr << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
    return;
}

// Pin it for safe storage
behl::PinHandle func = behl::pin(S);

// Execute multiple times
for (int i = 0; i < 1000; i++) {
    behl::pinned_push(S, func);
    behl::push_integer(S, i);
    if (behl::call(S, 1, 1) < 0) {
        std::cerr << behl::to_string(S, -1) << "\n";
        behl::pop(S, 1);
        break;
    }
    behl::Integer result = behl::to_integer(S, -1);
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
    if (behl::load_string(S, script) != 0 || behl::call(S, 0, 0) < 0) {
        std::cerr << behl::to_string(S, -1) << "\n";
    }
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
    
    std::ifstream file(path);
    if (!file) {
        behl::close(S);
        return false;
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    
    if (behl::load_buffer(S, buffer.str(), path) != 0 || behl::call(S, 0, 1) < 0) {
        std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
        behl::close(S);
        return false;
    }
    
    // Expect a table
    if (!behl::is_table(S, -1)) {
        behl::close(S);
        return false;
    }
    
    // Read configuration
    behl::table_getfield(S, -1, "width");
    config.width = behl::to_integer(S, -1);
    behl::pop(S, 1);
    
    behl::table_getfield(S, -1, "height");
    config.height = behl::to_integer(S, -1);
    behl::pop(S, 1);
    
    behl::close(S);
    return true;
}
```

### Script Validation

```cpp
bool validate_script(const char* code) {
    behl::State* S = behl::new_state();
    const bool ok = behl::load_string(S, code) == 0;
    behl::close(S);
    return ok;
}
```

### Sandboxed Execution

```cpp
behl::State* S = behl::new_state();
// Don't load stdlib or limit what's loaded
behl::load_lib_math(S);  // Only math functions

if (behl::load_string(S, user_code) != 0 || behl::call(S, 0, 0) < 0) {
    std::cerr << "Script failed: " << behl::to_string(S, -1) << "\n";
}
behl::close(S);
```

## See Also

- [API Reference](embedding/api-reference) - Complete API documentation
- [Error Handling](embedding/error-handling) - Status codes, error values and messages
- [Exposing C++ Functions](embedding/getting-started#complete-example) - Making C++ functions callable from scripts
- [Callbacks and Pinning](callbacks) - Storing script functions in C++
- [Examples](examples) - More code examples
