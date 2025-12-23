---
layout: default
title: Variables
parent: Language
nav_order: 3
---

# Variables
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Declaration

Variables are declared using `let` or `const`:

### Mutable Variables

```cpp
let x = 10;
x = 20;  // OK: can reassign

let y;   // Default initialized to nil
print(y); // nil
```

### Constants

```cpp
const PI = 3.14159;
// PI = 3.14;  // Error: cannot assign to const variable

const MAX_SIZE = 1000;
```

**Note**: `const` prevents reassignment of the variable, but doesn't make the value immutable:

```cpp
const arr = {1, 2, 3};
// arr = {4, 5, 6};  // Error: can't reassign
arr[0] = 99;         // OK: can modify table contents
```

## Scope

Variables are lexically scoped. Scopes are created by braces `{}`:

### Block Scope

```cpp
let x = 1;  // Outer scope

{
    let x = 2;  // Inner scope (shadows outer x)
    print(x);   // 2
}

print(x);  // 1 (outer x unchanged)
```

### Function Scope

Function bodies create their own scope:

```cpp
function test() {
    let local_var = 42;
    return local_var;
}

let result = test();
// print(local_var);  // Error: local_var not accessible here
```

### Shadowing

Inner scopes can shadow (hide) outer variables:

```cpp
let x = "outer";

function inner() {
    let x = "inner";  // Shadows outer x
    print(x);         // "inner"
}

inner();
print(x);  // "outer"
```

### Global Scope

Variables declared at the top level are global:

```cpp
let globalVar = 42;

function useGlobal() {
    print(globalVar);  // Can access global
}
```

### Implicit Globals

Assigning to an undeclared variable creates a **global** variable:

```cpp
function test() {
    implicitGlobal = 100;  // Creates global (no 'let')
}

test();
print(implicitGlobal);  // 100 (accessible globally)
```

**Warning**: Implicit globals can cause bugs. Always use `let` or `const` to declare variables explicitly:

```cpp
// Bad: Creates implicit global
function calculate() {
    result = x * 2;  // Oops, forgot 'let'
    return result;
}

// Good: Explicit local variable
function calculate() {
    let result = x * 2;
    return result;
}
```

**Best Practice**: Minimize globals. Use function parameters and local variables.

## Variable Lifetime

- **Local variables**: Lifetime is the scope in which they're declared
- **Global variables**: Live for the entire program execution
- **Captured variables**: In closures, variables live as long as the closure exists

```cpp
function makeCounter() {
    let count = 0;  // Lives as long as the returned function exists
    return function() {
        count = count + 1;
        return count;
    };
}

let counter = makeCounter();
print(counter());  // 1
print(counter());  // 2
// count still exists because closure captures it
```

## Initialization

### Explicit Initialization

```cpp
let x = 42;
let name = "Alice";
let active = true;
```

### Default Initialization

Variables without initializer default to `nil`:

```cpp
let x;
print(x);  // nil

let y = nil;
print(x == y);  // true
```

### Multiple Declarations

```cpp
// Separate statements
let x = 1;
let y = 2;
let z = 3;

// For loop allows multiple declarations
for (let i = 0, j = 10; i < j; i++, j--) {
    print(i + " " + tostring(j));
}
```

## Assignment

### Simple Assignment

```cpp
let x = 10;
x = 20;       // Reassignment
```

### Multiple Assignment

Behl supports multiple assignment with values or function returns:

```cpp
// Multiple declaration with values
let a, b = 1, 2;
// a = 1, b = 2

// Multiple assignment to existing variables
let x, y;
x, y = 10, 20;
// x = 10, y = 20

// From function returns
function getCoords() {
    return 10, 20;
}

let x, y = getCoords();
// x = 10, y = 20

// Excess variables get nil
let a, b, c = getCoords();
// a = 10, b = 20, c = nil

// Excess values are discarded
let p = getCoords();
// p = 10 (only first value)
```

### Compound Assignment

Behl supports compound assignment operators:

```cpp
let x = 10;
x += 5;   // x = x + 5  →  15
x -= 3;   // x = x - 3  →  12
x *= 2;   // x = x * 2  →  24
x /= 4;   // x = x / 4  →  6
x %= 5;   // x = x % 5  →  1
```

### Increment/Decrement

```cpp
let i = 0;
i++;      // i = i + 1
i--;      // i = i - 1

// Also works in expressions
let x = 5;
let y = x++;  // y = 5, x = 6 (post-increment)
```

## Best Practices

1. **Use `const` by default** - Only use `let` when you need to reassign
2. **Minimize scope** - Declare variables in the smallest scope possible
3. **Avoid globals** - Use function parameters and local variables
4. **Initialize explicitly** - Don't rely on default `nil` initialization
5. **Descriptive names** - Use meaningful variable names

```cpp
// Good
const MAX_RETRIES = 3;
let attemptCount = 0;

function processData(input) {
    let result = transform(input);
    return result;
}

// Avoid
let x = 3;         // Unclear name
let data;          // Uninitialized
globalCounter = 0; // Global variable
```
