---
layout: default
title: Error Handling
parent: Language
nav_order: 8
---

# Error Handling
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Raising Errors

Use the `error()` function to raise an error:

```cpp
function divide(a, b) {
    if (b == 0) {
        error("Division by zero!");
    }
    return a / b;
}

divide(10, 0);  // Error: Division by zero!
```

### Error Messages

`error()` accepts any value, but it converts the value to a string immediately and throws that string. The error a `pcall()` catches is therefore always a string:

```cpp
error("Something went wrong");
error("Error code: " + tostring(123));
error(42);  // Accepted, becomes the string "42"
```

### Immediate Termination

`error()` immediately terminates execution and propagates up the call stack:

```cpp
function outer() {
    print("Before");
    inner();
    print("After");  // Never executed
}

function inner() {
    error("Boom!");
}

outer();
// Output: "Before", then error
```

## Protected Calls

Use `pcall()` (protected call) to catch errors:

```cpp
function riskyOperation() {
    error("Oops!");
}

let success, result = pcall(riskyOperation);
if (success) {
    print("Success: " + tostring(result));
} else {
    print("Error: " + result);
}
// Output:
// Error: RuntimeError: Oops!
// Stack trace:
//   <one indented "file(line,col): at function" line per active frame>
```

The caught value is not the bare message. `error()` prefixes it with the error class and appends a full stack trace, so the string always has the shape `RuntimeError: <message>` followed by a newline and a `Stack trace:` block.

### pcall Return Values

`pcall()` returns:
1. **Success flag** (boolean): `true` if no error, `false` if error
2. **Result or error**:
   - If success: the function's return value(s)
   - If error: the error message

### Examples

```cpp
// Successful call
function add(a, b) {
    return a + b;
}

let ok, result = pcall(add, 5, 3);
print(ok);      // true
print(result);  // 8

// Failed call
function fail() {
    error("Failed!");
}

let ok, err = pcall(fail);
print(ok);   // false
print(err);  // "RuntimeError: Failed!" plus a newline and the stack trace
```

### Catching Multiple Return Values

```cpp
function multiReturn() {
    return 1, 2, 3;
}

let success, a, b, c = pcall(multiReturn);
if (success) {
    print(a);  // 1
    print(b);  // 2
    print(c);  // 3
}
```

## Error Propagation

Errors propagate up the call stack until caught by `pcall()`:

```cpp
function level3() {
    error("Error at level 3");
}

function level2() {
    level3();
}

function level1() {
    level2();
}

// Without pcall - program terminates
// level1();  // Uncaught error

// With pcall - error is caught
let ok, err = pcall(level1);
print("Caught: " + err);  // "Caught: RuntimeError: Error at level 3" plus the stack trace
```

## Nested Protected Calls

You can nest `pcall()` to handle errors at different levels:

```cpp
function inner() {
    let ok, result = pcall(function() {
        error("Inner error");
    });
    
    if (!ok) {
        print("Handled inner: " + result);
        error("Outer error");
    }
}

let ok, err = pcall(inner);
if (!ok) {
    print("Handled outer: " + err);
}
// Output:
// Handled inner: Inner error
// Handled outer: Outer error
```

## Error Handling Patterns

### Try-Catch Pattern

```cpp
function tryCatch(tryFunc, catchFunc) {
    let success, result = pcall(tryFunc);
    if (!success) {
        return catchFunc(result);
    }
    return result;
}

// Usage
let result = tryCatch(
    function() {
        return riskyOperation();
    },
    function(err) {
        print("Error: " + err);
        return nil;  // Default value
    }
);
```

### Resource Cleanup with Defer

Use `defer` to ensure cleanup happens even on error:

```cpp
const fs = import("fs");

function processFile(filename) {
    let file = fs.open(filename, "r");
    defer file:close();  // Always executed, even on error
    
    // Risky operation
    if (someCondition) {
        error("Processing failed!");
    }
    
    return file:read(1024);
}  // File closed here, even if error occurred

let ok, result = pcall(processFile, "data.txt");
if (!ok) {
    print("Error: " + result);
}
```

See [Defer Statement](defer) for more details.

### Validation Functions

```cpp
function validateAge(age) {
    if (typeof(age) != "integer") {
        error("Age must be an integer");
    }
    if (age < 0 || age > 150) {
        error("Age out of range: " + tostring(age));
    }
    return age;
}

function createPerson(name, age) {
    let ok, validAge = pcall(validateAge, age);
    if (!ok) {
        print("Invalid age: " + validAge);
        return nil;
    }
    
    return {
        ["name"] = name,
        ["age"] = validAge
    };
}
```

### Early Return on Error

```cpp
function processData(data) {
    let ok, parsed = pcall(parseData, data);
    if (!ok) {
        return nil, "Parse error: " + parsed;
    }
    
    let ok2, validated = pcall(validateData, parsed);
    if (!ok2) {
        return nil, "Validation error: " + validated;
    }
    
    return validated, nil;  // Success, no error
}

// Usage
let result, err = processData(input);
if (err != nil) {
    print("Error: " + err);
} else {
    print("Success: " + tostring(result));
}
```

## Best Practices

1. **Use descriptive error messages** - Include context about what went wrong
2. **Validate inputs early** - Check preconditions at function start
3. **Use pcall for external operations** - File I/O, network, user input
4. **Clean up resources** - Use `defer` for guaranteed cleanup
5. **Document error conditions** - Specify what errors a function can raise
6. **Don't swallow errors silently** - Log or handle errors appropriately

```cpp
const fs = import("fs");

// Good: Descriptive error
function withdraw(account, amount) {
    if (amount < 0) {
        error("Withdrawal amount must be positive, got: " + tostring(amount));
    }
    if (account.balance < amount) {
        error("Insufficient funds. Balance: " + tostring(account.balance) + 
              ", requested: " + tostring(amount));
    }
    account.balance = account.balance - amount;
}

// Good: Using pcall for external operations
function readConfig(filename) {
    let ok, content = pcall(function() {
        let file = fs.open(filename, "r");
        defer file:close();
        return file:read(1024);
    });
    
    if (!ok) {
        print("Warning: Could not read config, using defaults");
        return getDefaultConfig();
    }
    
    return parseConfig(content);
}
```

## Limitations

1. **No exception types** - Errors are always strings by the time they are caught
2. **Single error handler** - No multiple catch blocks like try-catch-finally
3. **Performance overhead** - `pcall` has some overhead; don't use in hot loops

Structured error objects are not possible. `error()` converts its argument to a string before throwing, and `pcall()` always hands back a string, so `typeof(err)` is `"string"` no matter what you passed in. Encode the category in the message text and match on it instead:

```cpp
const string = import("string");

function doSomething() {
    error("ValidationError: Invalid input");
}

let ok, err = pcall(doSomething);
if (!ok) {
    if (string.find(err, "ValidationError") >= 0) {
        print("Validation failed");
    } else {
        print("Unknown error: " + err);
    }
}
```
