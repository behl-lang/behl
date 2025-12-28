---
layout: default
title: Calling Functions
parent: Embedding
nav_order: 3
---

# Calling Functions
{: .no_toc }

Load, compile, and execute Behl code from C++.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Loading Code

### `load_string(State*, std::string_view, bool optimize = true)`

Compiles a string and pushes the resulting function onto the stack.

```cpp
try {
    behl::load_string(S, "return 2 + 3");
    // Function is now on stack
} catch (const behl::SyntaxError& e) {
    std::cerr << "Compile error: " << e.what() << "\n";
}
```

**Parameters:**
- `code` - Source code to compile
- `optimize` - Enable compiler optimizations (default: `true`)

**Throws:** `SyntaxError` or `ParserError` on compilation failure.

### `load_buffer(State*, std::string_view, std::string_view chunkname, bool optimize = true)`

Like `load_string` but with a custom chunk name for error messages.

```cpp
try {
    behl::load_buffer(S, code, "script.behl");
} catch (const behl::SyntaxError& e) {
    std::cerr << "Error in script.behl: " << e.what() << "\n";
}
```

**Parameters:**
- `code` - Source code to compile
- `chunkname` - Name for error messages (e.g., filename)
- `optimize` - Enable compiler optimizations

---

## Calling Functions

### `call(State*, int32_t nargs, int32_t nresults)`

Calls a function with arguments, expecting return values.

```cpp
// Stack: [function, arg1, arg2]
try {
    behl::call(S, 2, 1);  // 2 args, 1 result
    int result = behl::to_integer(S, -1);
    behl::pop(S, 1);
} catch (const behl::RuntimeError& e) {
    std::cerr << "Runtime error: " << e.what() << "\n";
}
```

**Parameters:**
- `nargs` - Number of arguments already pushed on stack
- `nresults` - Number of expected return values

**Throws:** `RuntimeError`, `TypeError`, or other `BehlException` on error.

**Stack behavior:**
- Before: `[function, arg1, arg2, ...]`
- After (success): `[result1, result2, ...]`
- After (error): Exception thrown, stack unchanged

---

## Complete Examples

### Execute Simple Script

```cpp
behl::State* S = behl::new_state();
behl::load_stdlib(S);

const char* script = "print('Hello from Behl!')";

try {
    behl::load_string(S, script);
    behl::call(S, 0, 0);  // No args, no return values
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}

behl::close(S);
```

### Get Return Value

```cpp
const char* script = R"(
    let x = 10;
    let y = 20;
    return x + y;
)";

try {
    behl::load_string(S, script);
    behl::call(S, 0, 1);  // No args, 1 return value
    
    int result = behl::to_integer(S, -1);
    std::cout << "Result: " << result << "\n";  // Output: 30
    behl::pop(S, 1);
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

### Call Function with Arguments

```cpp
// Load script that defines a function
const char* script = R"(
    function multiply(a, b) {
        return a * b;
    }
)";

try {
    behl::load_string(S, script);
    behl::call(S, 0, 0);  // Execute to define function
    
    // Call the function
    behl::get_global(S, "multiply");  // Push function
    behl::push_integer(S, 6);         // Push arg 1
    behl::push_integer(S, 7);         // Push arg 2
    
    behl::call(S, 2, 1);  // 2 args, 1 result
    
    int result = behl::to_integer(S, -1);
    std::cout << "6 * 7 = " << result << "\n";  // Output: 42
    behl::pop(S, 1);
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

### Register and Call C++ Function

```cpp
// C++ function callable from Behl
int cpp_add(behl::State* S) {
    int a = behl::check_integer(S, 0);
    int b = behl::check_integer(S, 1);
    behl::push_integer(S, a + b);
    return 1;  // Number of return values
}

// Register it
behl::register_function(S, "add", cpp_add);

// Call from script
const char* script = R"(
    let result = add(10, 20);
    print("Result: " + tostring(result));
)";

try {
    behl::load_string(S, script);
    behl::call(S, 0, 0);
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

### Error Handling

```cpp
const char* bad_script = "return undefined_var + 10";

try {
    behl::load_string(S, bad_script);
    behl::call(S, 0, 1);
    
    // Success - use result
    int result = behl::to_integer(S, -1);
    behl::pop(S, 1);
} catch (const behl::SyntaxError& e) {
    std::cerr << "Compile error: " << e.what() << "\n";
} catch (const behl::RuntimeError& e) {
    std::cerr << "Runtime error: " << e.what() << "\n";
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

---

## C Function Signature

All C functions callable from Behl must have this signature:

```cpp
int function_name(behl::State* S);
```

**Return value:** Number of values pushed onto stack (return values).

**Example:**
```cpp
int my_func(behl::State* S) {
    // Get arguments (bottom to top)
    int arg1 = behl::check_integer(S, 0);
    int arg2 = behl::check_integer(S, 1);
    
    // Push return values
    behl::push_integer(S, arg1 + arg2);
    behl::push_integer(S, arg1 * arg2);
    
    return 2;  // Returning 2 values
}
```

---

## Next Steps

- Learn about [Tables](tables) for complex data structures
- Handle errors with [Error Handling](error-handling)
- Create reusable functionality with [Modules](modules)
