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

## Error Types

Behl has two types of errors:

1. **Compile errors** - Syntax errors, caught by `load_string` / `load_buffer`
2. **Runtime errors** - Type errors, undefined variables, custom errors

All errors are reported via C++ exceptions.

---

## Handling Compile Errors

### `load_string` Error Handling

```cpp
try {
    behl::load_string(S, "let x = ");  // Incomplete
} catch (const behl::SyntaxError& e) {
    std::cerr << "Compile error: " << e.what() << "\n";
}
```

**Throws:** `SyntaxError` or `ParserError` on compilation failure.

---

## Handling Runtime Errors

### `call` Error Handling

```cpp
try {
    behl::load_string(S, "return undefined_variable");
    behl::call(S, 0, 1);
    
    // Success
    int result = behl::to_integer(S, -1);
    behl::pop(S, 1);
} catch (const behl::RuntimeError& e) {
    std::cerr << "Runtime error: " << e.what() << "\n";
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

---

## Raising Errors

### `error(State*, std::string_view)`

Throws a `RuntimeError` exception from C++ code. Does not return.

```cpp
[[noreturn]] void behl::error(behl::State* S, std::string_view msg);
```

**Example:**
```cpp
int safe_divide(behl::State* S) {
    int a = behl::check_integer(S, 0);
    int b = behl::check_integer(S, 1);
    
    if (b == 0) {
        behl::error(S, "Division by zero!");
        // Throws RuntimeError, does not return
    }
    
    behl::push_integer(S, a / b);
    return 1;
}
```

---

## Type Checking Errors

The `check_*` functions automatically throw `TypeError` exceptions:

```cpp
int my_func(behl::State* S) {
    // Throws TypeError if argument is not an integer
    int x = behl::check_integer(S, 0);
    
    // Use x...
    behl::push_integer(S, x * 2);
    return 1;
}
```

**Example error message:**
```
bad argument #1 (expected integer, got string)
```

**Note:** You typically don't need to catch these exceptions unless you want custom error handling. The VM will propagate them automatically.

---

## Protected Calls from Scripts

Scripts can use `pcall()` to catch errors:

```javascript
// Behl script
function risky() {
    return undefined_var + 10;  // Will error
}

let success, result = pcall(risky);
if (success) {
    print("Result: " + tostring(result));
} else {
    print("Error: " + result);  // result is error message
}
```

---

## Complete Error Handling Example

```cpp
#include <behl/behl.hpp>
#include <behl/exceptions.hpp>
#include <iostream>

int safe_operation(behl::State* S) {
    int a = behl::check_integer(S, 0);
    int b = behl::check_integer(S, 1);
    
    if (b == 0) {
        behl::error(S, "Cannot divide by zero");
    }
    
    behl::push_integer(S, a / b);
    return 1;
}

int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    behl::register_function(S, "safe_div", safe_operation);
    
    // Test 1: Compile error
    try {
        behl::load_string(S, "let x = ");
    } catch (const behl::SyntaxError& e) {
        std::cerr << "Compile error: " << e.what() << "\n";
    }
    
    // Test 2: Runtime error
    try {
        behl::load_string(S, "return safe_div(10, 0)");
        behl::call(S, 0, 1);
    } catch (const behl::RuntimeError& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
    }
    
    // Test 3: Type error
    try {
        behl::load_string(S, "return safe_div('hello', 5)");
        behl::call(S, 0, 1);
    } catch (const behl::TypeError& e) {
        std::cerr << "Type error: " << e.what() << "\n";
    }
    
    // Test 4: Success case
    try {
        behl::load_string(S, "return safe_div(20, 4)");
        behl::call(S, 0, 1);
        std::cout << "Result: " << behl::to_integer(S, -1) << "\n";
        behl::pop(S, 1);
    } catch (const behl::BehlException& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
    }
    
    behl::close(S);
    return 0;
}
```

**Output:**
```
Compile error: unexpected end of input
Runtime error: Cannot divide by zero
Type error: bad argument #1 (expected integer, got string)
Result: 5
```

---

## Exception Types

Behl defines the following exception hierarchy:

```cpp
class BehlException : public std::exception  // Base class
    ├── SyntaxError          // Syntax/parsing errors
    ├── ParserError          // Parse tree errors
    ├── SemanticError        // Semantic analysis errors
    ├── RuntimeError         // Runtime execution errors
    ├── TypeError            // Type checking failures
    ├── ReferenceError       // Undefined variables
    └── ArithmeticError      // Math operation errors
```

## Best Practices

### 1. Use Try-Catch for Error Handling

```cpp
// Good
try {
    behl::load_string(S, code);
    behl::call(S, 0, 0);
} catch (const behl::BehlException& e) {
    std::cerr << "Error: " << e.what() << "\n";
}

// Bad - Ignores exceptions (will crash on error)
behl::load_string(S, code);
behl::call(S, 0, 0);
```

### 2. Use `check_*` for Type Safety

```cpp
// Good - Throws TypeError with descriptive message
int x = behl::check_integer(S, 0);

// Bad - Silent failure, returns 0 if not integer
int x = behl::to_integer(S, 0);
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

### 4. Catch Specific Exception Types

```cpp
// Good - Handle different errors appropriately
try {
    behl::load_string(S, code);
    behl::call(S, 0, 1);
} catch (const behl::SyntaxError& e) {
    // Handle compilation errors
} catch (const behl::TypeError& e) {
    // Handle type errors
} catch (const behl::RuntimeError& e) {
    // Handle runtime errors
}
```

---

## Next Steps

- Build reusable functionality with [Modules](modules)
- See the complete [API Reference](api-reference)
