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
if (behl::load_string(S, "return 2 + 3") != 0) {
    std::cerr << "Compile error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
// On success the function is now on the stack
```

**Parameters:**
- `code` - Source code to compile
- `optimize` - Enable compiler optimizations (default: `true`)

**Returns:** `[[nodiscard]] int32_t`. `0` on success, with the compiled function pushed. On failure `behl::kErrorSyntax` (or `behl::kErrorMemory`), with the error message string pushed instead. Does not throw.

### `load_buffer(State*, std::string_view, std::string_view chunkname, bool optimize = true)`

Like `load_string` but with a custom chunk name for error messages.

```cpp
if (behl::load_buffer(S, code, "script.behl") != 0) {
    // "script.behl(line,col): SyntaxError: ..."
    std::cerr << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

**Parameters:**
- `code` - Source code to compile
- `chunkname` - Name for error messages (e.g., filename)
- `optimize` - Enable compiler optimizations

**Returns:** same as `load_string`.

---

## Calling Functions

### `call(State*, int32_t nargs, int32_t nresults)`

Calls a function with arguments, expecting return values. The call is always protected: script errors are returned, not thrown.

```cpp
// Stack: [function, arg1, arg2]
if (behl::call(S, 2, 1) < 0) {  // 2 args, 1 result
    std::cerr << "Runtime error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
} else {
    behl::Integer result = behl::to_integer(S, -1);
    behl::pop(S, 1);
}
```

**Parameters:**
- `nargs` - Number of arguments already pushed on stack
- `nresults` - Number of expected return values, or `behl::kMultRet`

**Returns:** `[[nodiscard]] int32_t`. The number of results pushed (`>= 0`) on success, or `behl::kErrorRuntime` / `behl::kErrorMemory` on error.

**Stack behavior:**
- Before: `[function, arg1, arg2, ...]`
- After (success): `[result1, result2, ...]`
- After (error): `[error_value]`, the function and its arguments are removed and the error value is pushed

The error value is exactly what was raised. It is a string for VM errors and `error("msg")`, but a script can raise any value, such as a table. See [Error Handling](error-handling).

C++ exceptions thrown by your own C functions (other than through `behl::error`, `behl::error_value` or `check_*`) are not caught by `call` and propagate to the caller. `std::bad_alloc` is reported as `behl::kErrorMemory`.

---

## Complete Examples

### Execute Simple Script

```cpp
behl::State* S = behl::new_state();
behl::load_stdlib(S);

const char* script = "print('Hello from Behl!')";

if (behl::load_string(S, script) != 0 || behl::call(S, 0, 0) < 0) {  // No args, no return values
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
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

if (behl::load_string(S, script) != 0 || behl::call(S, 0, 1) < 0) {  // No args, 1 return value
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
} else {
    behl::Integer result = behl::to_integer(S, -1);
    std::cout << "Result: " << result << "\n";  // Output: 30
}
behl::pop(S, 1);
```

### Call Function with Arguments

```cpp
// Load script that defines a function
const char* script = R"(
    function multiply(a, b) {
        return a * b;
    }
)";

if (behl::load_string(S, script) != 0 || behl::call(S, 0, 0) < 0) {  // Execute to define function
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
    return;
}

// Call the function
behl::get_global(S, "multiply");  // Push function
behl::push_integer(S, 6);         // Push arg 1
behl::push_integer(S, 7);         // Push arg 2

if (behl::call(S, 2, 1) < 0) {  // 2 args, 1 result
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
} else {
    behl::Integer result = behl::to_integer(S, -1);
    std::cout << "6 * 7 = " << result << "\n";  // Output: 42
}
behl::pop(S, 1);
```

### Register and Call C++ Function

```cpp
// C++ function callable from Behl
int cpp_add(behl::State* S) {
    behl::Integer a = behl::check_integer(S, 0);
    behl::Integer b = behl::check_integer(S, 1);
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

if (behl::load_string(S, script) != 0 || behl::call(S, 0, 0) < 0) {
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

### Error Handling

```cpp
const char* bad_script = "return undefined_var + 10";

if (behl::load_string(S, bad_script) != 0) {
    std::cerr << "Compile error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
} else if (behl::call(S, 0, 1) < 0) {
    // "<string>(line,col): TypeError: attempt to perform arithmetic on a 'nil' value and a 'integer' value"
    std::cerr << "Runtime error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
} else {
    behl::Integer result = behl::to_integer(S, -1);
    behl::pop(S, 1);
}
```

---

## C Function Signature

All C functions callable from Behl must have this signature:

```cpp
int function_name(behl::State* S);
```

**Return value:** Number of values pushed onto stack (return values).

To report an error, call `behl::error(S, msg)` or `behl::error_value(S)`, or let a `check_*` function fail. These must only be used while Behl is calling the function.

**Example:**
```cpp
int my_func(behl::State* S) {
    // Get arguments (bottom to top)
    behl::Integer arg1 = behl::check_integer(S, 0);
    behl::Integer arg2 = behl::check_integer(S, 1);
    
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
