---
layout: default
title: Varargs
parent: Language
nav_order: 9
---

# Varargs

Behl supports Lua-style variadic functions using the `...` syntax, allowing functions to accept a variable number of arguments.

## Basic Syntax

Use `...` as the last parameter to accept any number of arguments:

```javascript
function test(...) {
    let args = {...};  // Pack varargs into a table
    print("Received " + tostring(#args) + " arguments");
}

test(1, 2, 3);  // Received 3 arguments
test();         // Received 0 arguments
```

## Packing Varargs

Use `{...}` to pack variadic arguments into a 0-indexed table:

```javascript
function sum(...) {
    let args = {...};
    let total = 0;
    for (let i = 0; i < #args; i++) {
        total = total + args[i];
    }
    return total;
}

print(sum(1, 2, 3, 4, 5));  // 15
```

## Mixed Parameters

Combine regular parameters with varargs:

```javascript
function greet(greeting, ...) {
    let names = {...};
    for (let i = 0; i < #names; i++) {
        print(greeting + ", " + names[i] + "!");
    }
}

greet("Hello", "Alice", "Bob", "Charlie");
// Output:
// Hello, Alice!
// Hello, Bob!
// Hello, Charlie!
```

## Multiple Captures

You can pack `...` multiple times in the same function (each creates a new table):

```javascript
function analyze(...) {
    let args1 = {...};
    let args2 = {...};  // Same values, new table
    
    return #args1 + #args2;
}

print(analyze(1, 2, 3));  // 6 (3 + 3)
```

## Forwarding Varargs

Use `...` in a function call to forward all variadic arguments:

```javascript
function inner(...) {
    let args = {...};
    print("Inner received " + tostring(#args));
    return #args;
}

function outer(...) {
    print("Forwarding arguments...");
    return inner(...);  // Forward all varargs
}

outer(1, 2, 3, 4);
// Output:
// Forwarding arguments...
// Inner received 4
```

## Mixed Forwarding

Forward varargs along with additional arguments:

```javascript
function loggedCall(name, ...) {
    print("Calling " + name);
    return actualFunction(...);
}

function actualFunction(a, b, c) {
    return a + b + c;
}

print(loggedCall("sum", 10, 20, 30));
// Output:
// Calling sum
// 60
```

## Varargs with Closures

`...` itself is **not** capturable. It compiles to an instruction that reads the extra arguments of the *current* call frame, so an inner function cannot see the outer function's `...`. Pack it into a table first, and that table can be captured as an upvalue like any other local:

```javascript
function makeFormatter(prefix, ...) {
    let suffixes = {...};
    
    return function(text) {
        let result = prefix + text;
        for (let i = 0; i < #suffixes; i++) {
            result = result + suffixes[i];
        }
        return result;
    };
}

let formatter = makeFormatter("[", ":", "]");
print(formatter("INFO"));  // [INFO:]
```

## Varargs with Tables

Common pattern: varargs as table constructor arguments:

```javascript
function createPoint(...) {
    let coords = {...};
    return {
        x = coords[0] || 0,
        y = coords[1] || 0,
        z = coords[2] || 0
    };
}

let p1 = createPoint(1, 2, 3);
let p2 = createPoint(5, 10);  // z defaults to 0
```

## Inspecting Varargs

```javascript
function inspect(...) {
    let args = {...};
    
    print("Count: " + tostring(#args));
    
    for (let i = 0; i < #args; i++) {
        print("[" + tostring(i) + "] = " + 
              tostring(args[i]) + 
              " (" + typeof(args[i]) + ")");
    }
}

inspect(42, "hello", true, "x");
// Output:
// Count: 4
// [0] = 42 (integer)
// [1] = hello (string)
// [2] = true (boolean)
// [3] = x (string)
```

**Note**: `#` stops at the first `nil` in the array part, so packing a `nil` argument makes the count short. `inspect(42, "hello", true, nil)` reports a count of 3.

## Default Values with Varargs

Provide default values when varargs are empty:

```javascript
function process(required, ...) {
    let optional = {...};
    let option1 = optional[0] || "default1";
    let option2 = optional[1] || "default2";
    
    print(required + ", " + option1 + ", " + option2);
}

process("A");           // A, default1, default2
process("A", "B");      // A, B, default2
process("A", "B", "C"); // A, B, C
```

## Variable Argument Count

Functions can handle different numbers of arguments:

```javascript
function max(...) {
    let args = {...};
    
    if (#args == 0) {
        return nil;
    }
    
    let maximum = args[0];
    for (let i = 1; i < #args; i++) {
        if (args[i] > maximum) {
            maximum = args[i];
        }
    }
    
    return maximum;
}

print(max(3, 7, 2, 9, 1));  // 9
print(max(5));              // 5
print(max());               // nil
```

## Common Patterns

### Logging Function

```javascript
function log(level, ...) {
    let messages = {...};
    let output = "[" + level + "] ";
    
    for (let i = 0; i < #messages; i++) {
        output = output + tostring(messages[i]);
        if (i < #messages - 1) {
            output = output + " ";
        }
    }
    
    print(output);
}

log("INFO", "Server", "started", "on", "port", 8080);
// Output: [INFO] Server started on port 8080
```

### Function Composition

```javascript
function compose(...) {
    let functions = {...};
    
    return function(value) {
        let result = value;
        for (let i = 0; i < #functions; i++) {
            result = functions[i](result);
        }
        return result;
    };
}

let addOne = function(x) { return x + 1; };
let double = function(x) { return x * 2; };
let square = function(x) { return x * x; };

let pipeline = compose(addOne, double, square);
print(pipeline(5));  // ((5 + 1) * 2)^2 = 144
```

### Assertion with Context

```javascript
function assert(condition, ...) {
    if (!condition) {
        let messages = {...};
        let errorMsg = "Assertion failed";
        
        if (#messages > 0) {
            errorMsg = errorMsg + ": ";
            for (let i = 0; i < #messages; i++) {
                errorMsg = errorMsg + tostring(messages[i]);
            }
        }
        
        error(errorMsg);
    }
}

assert(x > 0, "x must be positive, got ", x);
```

## Important Notes

1. **Position**: `...` must be the last parameter
2. **Table Creation**: Each `{...}` creates a new table
3. **0-Indexed**: Vararg tables are 0-indexed (like all Behl tables)
4. **Type Safety**: No type checking - use `typeof()` to validate
5. **Performance**: Minimal overhead compared to fixed parameters
6. **Empty Case**: `{...}` with no arguments creates empty table
7. **Last In A Table Constructor**: `...` must be the final entry of a table literal, `{..., 1}` is a compile error
8. **Multiple Assignment**: `let a, b, c = ...;` spreads the varargs across the names, padding with `nil` when there are too few and dropping the extras when there are too many
9. **Return**: `return ...;` returns every vararg, not just the first
10. **Vararg Functions Only**: using `...` in a function that does not declare `...` is a compile error, `cannot use '...' outside a vararg function`

## Script Arguments

The main chunk of a script is itself a vararg function, so `...` at the top level holds the arguments the script was invoked with. The CLI passes everything after the script path:

```
behl script.behl hello world 42
```

```cpp
let args = {...}
print(rawlen(args))  // 3
print(args[0])       // hello
```

Arguments arrive as strings, so convert them yourself with `tonumber()` where needed. A script run with no arguments sees an empty `...`.

Because everything after the script path is treated as an argument rather than as another file, the CLI runs exactly one script per invocation.

When embedding, the same mechanism applies: push the arguments after `load_string` or `load_buffer` and pass the count to `call`.

```cpp
function spread(...) {
    let a, b, c = ...;   // Missing values become nil
    print(a, b, c);
}

spread(1);         // 1  nil  nil
spread(1, 2, 3, 4); // 1  2  3 (the 4 is dropped)

function forward(...) {
    return ...;      // All varargs, not just the first
}

let x, y = forward(10, 20);  // x = 10, y = 20
```

## Comparison with Other Languages

| Language | Syntax | Packing |
|----------|--------|---------|
| Behl | `function(...) {...}` | `{...}` → table |
| Lua | `function(...) end` | `{...}` → table |
| JavaScript | `function(...args)` | Built-in array |
| Python | `def f(*args):` | Built-in tuple |
| C++ | `template<typename... Args>` | Parameter pack |

Behl's varargs are most similar to Lua, with explicit packing using `{...}` and the same forwarding semantics.
