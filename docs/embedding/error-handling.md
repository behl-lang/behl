---
layout: default
title: Error Handling
parent: Embedding
nav_order: 6
---

# Error Handling
{: .no_toc }

Handle compilation and runtime errors in Behl scripts.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Behl reports errors to the host through status codes, not C++ exceptions:

1. **Compile errors** (syntax, semantic and reference errors found while compiling) are returned by `load_string` / `load_buffer`.
2. **Runtime errors** (type errors, `error()` calls, errors raised by C functions) are returned by `call`.

In both cases the error value is left on top of the stack.

There is no separate protected-call function in the C API: `behl::call` is always protected.

---

## Status Codes

Defined in `<behl/types.hpp>` (included by `<behl/behl.hpp>`):

| Constant | Value | Returned by | Meaning |
|----------|-------|-------------|---------|
| `behl::kErrorRuntime` | `-1` | `call` | A runtime error was raised |
| `behl::kErrorMemory` | `-2` | `call`, `load_string`, `load_buffer` | Memory allocation failed (`std::bad_alloc`) |
| `behl::kErrorSyntax` | `-3` | `load_string`, `load_buffer` | The chunk failed to compile |

`call`, `load_string` and `load_buffer` are `[[nodiscard]]`. Always check the result.

---

## Handling Compile Errors

```cpp
[[nodiscard]] int32_t behl::load_string(behl::State* S, std::string_view str, bool optimize = true);
[[nodiscard]] int32_t behl::load_buffer(behl::State* S, std::string_view str, std::string_view chunkname, bool optimize = true);
```

Returns `0` on success and pushes the compiled function. On failure returns `behl::kErrorSyntax` (or `behl::kErrorMemory`) and pushes the error message string instead.

```cpp
if (behl::load_string(S, "let x = ") != 0) {
    std::cerr << "Compile error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}
```

Compile error messages have the form `<chunk>(line,col): SyntaxError: ...`. The label can also be `SemanticError` or `ReferenceError` depending on the compiler stage that rejected the code.

---

## Handling Runtime Errors

```cpp
[[nodiscard]] int32_t behl::call(behl::State* S, int32_t nargs, int32_t nresults);
```

On success, returns the number of results pushed (`>= 0`). On error, returns `behl::kErrorRuntime` or `behl::kErrorMemory`. The function and its arguments are removed from the stack and the error value is pushed in their place.

```cpp
if (behl::load_string(S, "return nil + 1") != 0) {
    std::cerr << "Compile error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
} else if (behl::call(S, 0, 1) < 0) {
    std::cerr << "Runtime error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
} else {
    behl::Integer result = behl::to_integer(S, -1);
    behl::pop(S, 1);
}
```

### The error value can be any type

The error value is exactly what was raised. `error("msg")` and errors from the VM produce strings, but a script can raise any value, for example `error({code = 5})`. `behl::to_string` returns an empty string for non-string values, so check the type when it matters:

```cpp
if (behl::load_string(S, "error({code = 5})") != 0) {
    behl::pop(S, 1);
    return;
}

if (behl::call(S, 0, 0) == behl::kErrorRuntime) {
    if (behl::is_table(S, -1)) {
        behl::table_getfield(S, -1, "code");
        std::cerr << "Error code: " << behl::to_integer(S, -1) << "\n";
        behl::pop(S, 1);
    } else if (behl::is_string(S, -1)) {
        std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    } else {
        std::cerr << "Error object of type " << behl::value_typename(S, -1) << "\n";
    }
    behl::pop(S, 1);
}
```

---

## Error Message Format

Runtime errors raised by the VM are strings of the form:

```
<chunk>(line,col): TypeError: attempt to perform arithmetic on a 'nil' value and a 'integer' value
```

The labels in use are `TypeError`, `RuntimeError`, `ReferenceError`, `SyntaxError` and `SemanticError`. They are part of the message text only; there are no matching C++ types.

"attempt to call" errors and messages raised with `behl::error` have a stack trace appended:

```
RuntimeError: Cannot divide by zero
Stack trace:
  <native>: at safe_div
  <string>(1,8): at <main chunk>
```

There is one line per active call frame, most recent first. C functions show as `<native>`. The exact line and column numbers depend on the code.

---

## Raising Errors from C Functions

### `error`

```cpp
[[noreturn]] void behl::error(behl::State* S, std::string_view msg);
```

Raises a runtime error with a string message. The message is prefixed with `RuntimeError: ` and followed by a newline and the stack trace. Does not return.

### `error_value`

```cpp
[[noreturn]] void behl::error_value(behl::State* S);
```

Raises the value on top of the stack as the error, whatever its type. Script `pcall` and host `call` receive that exact value. Does not return.

```cpp
int raise_table(behl::State* S) {
    behl::table_new(S);
    behl::push_integer(S, 99);
    behl::table_setfield(S, -2, "id");
    behl::error_value(S);
}
```

### Only inside a call

`error` and `error_value` must only be called from a C function that Behl is currently calling (through a script or through `behl::call`). Calling them with no active call is API misuse and triggers an assertion.

```cpp
int safe_divide(behl::State* S) {
    behl::Integer a = behl::check_integer(S, 0);
    behl::Integer b = behl::check_integer(S, 1);

    if (b == 0) {
        behl::error(S, "Division by zero!");
    }

    behl::push_integer(S, a / b);
    return 1;
}
```

---

## Type Checking Errors

The `check_*` functions (`check_integer`, `check_number`, `check_string`, `check_boolean`, `check_type`, `check_userdata`) are meant to be called from C functions called by Behl. When the check fails they raise an error, which the surrounding `call` or script `pcall` reports:

```cpp
int my_func(behl::State* S) {
    behl::Integer x = behl::check_integer(S, 0);
    behl::push_integer(S, x * 2);
    return 1;
}
```

**Example error message:**
```
TypeError: bad argument #1 (expected integer, got string)
```

---

## Foreign C++ Exceptions

A C++ exception thrown by a C function that is not raised through `behl::error`, `behl::error_value` or a `check_*` function is not caught by `behl::call`. It passes through `call` to the host, after Behl has unwound its own call frames and removed the function and arguments from the stack. Catch it in the host if your C functions can throw.

```cpp
try {
    if (behl::call(S, 0, 0) < 0) {
        std::cerr << "Behl error: " << behl::to_string(S, -1) << "\n";
        behl::pop(S, 1);
    }
} catch (const std::exception& e) {
    std::cerr << "Host exception: " << e.what() << "\n";
}
```

Two cases are handled differently:

- `std::bad_alloc` is converted to `behl::kErrorMemory` with a memory error message on the stack.
- Script-level `pcall` catches foreign exceptions and returns `false` plus the exception's `what()` text as a string (or `"unknown C++ exception"` if it is not derived from `std::exception`).

---

## Protected Calls from Scripts

Scripts use `pcall()` to catch errors. It returns `true` plus the results, or `false` plus the error value exactly as raised:

```javascript
function risky() {
    return undefined_var + 10;
}

let success, result = pcall(risky);
if (success) {
    print("Result: " + tostring(result));
} else {
    print("Error: " + result);
}

let ok, e = pcall(function() { error({code = 1}); });
print(e.code);  // 1
```

See [Language: Error Handling](../language/error-handling) for details.

---

## Complete Example

```cpp
#include <behl/behl.hpp>
#include <iostream>
#include <string_view>

int safe_div(behl::State* S) {
    behl::Integer a = behl::check_integer(S, 0);
    behl::Integer b = behl::check_integer(S, 1);

    if (b == 0) {
        behl::error(S, "Cannot divide by zero");
    }

    behl::push_integer(S, a / b);
    return 1;
}

void run(behl::State* S, std::string_view code) {
    if (behl::load_string(S, code) != 0) {
        std::cerr << "Compile error: " << behl::to_string(S, -1) << "\n";
        behl::pop(S, 1);
        return;
    }

    if (behl::call(S, 0, 1) < 0) {
        std::cerr << "Runtime error: " << behl::to_string(S, -1) << "\n";
        behl::pop(S, 1);
        return;
    }

    std::cout << "Result: " << behl::to_integer(S, -1) << "\n";
    behl::pop(S, 1);
}

int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    behl::register_function(S, "safe_div", safe_div);

    run(S, "let x = ");
    run(S, "return safe_div(10, 0)");
    run(S, "return safe_div('hello', 5)");
    run(S, "return safe_div(20, 4)");

    behl::close(S);
    return 0;
}
```

**Output (stack trace lines abbreviated):**
```
Compile error: <string>(1,9): SyntaxError: Unexpected token in expression
Runtime error: RuntimeError: Cannot divide by zero
Stack trace:
  ... one line per call frame ...
Runtime error: TypeError: bad argument #1 (expected integer, got string)
Result: 5
```

---

## Best Practices

### 1. Always Check Status Codes

```cpp
// Good
if (behl::load_string(S, code) != 0 || behl::call(S, 0, 0) < 0) {
    std::cerr << "Error: " << behl::to_string(S, -1) << "\n";
    behl::pop(S, 1);
}

// Bad: ignores the [[nodiscard]] results, the compiler warns and errors go unnoticed
behl::load_string(S, code);
behl::call(S, 0, 0);
```

### 2. Use `check_*` for Type Safety

```cpp
// Good: raises "bad argument" with a descriptive message
behl::Integer x = behl::check_integer(S, 0);

// Bad: silent failure, returns 0 if not an integer
behl::Integer x = behl::to_integer(S, 0);
```

### 3. Provide Descriptive Error Messages

```cpp
// Good
if (value < 0) {
    behl::error(S, "Value must be non-negative");
}

// Bad
if (value < 0) {
    behl::error(S, "Error");
}
```

### 4. Use Error Values for Structured Errors

Raise a table with `error_value` (C) or `error({...})` (script) when the caller needs to act on the error, not just print it.

---

## Next Steps

- Build reusable functionality with [Modules](modules)
- See the complete [API Reference](api-reference)
