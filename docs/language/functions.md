---
layout: default
title: Functions
parent: Language
nav_order: 6
---

# Functions
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Function Definition

Define functions using the `function` keyword:

```cpp
function name(param1, param2) {
    // function body
    return result;
}
```

### Examples

```cpp
function add(a, b) {
    return a + b;
}

function greet(name) {
    print("Hello, " + name + "!");
}

function noParams() {
    print("No parameters");
}
```

## Parameters and Arguments

### Fixed Parameters

```cpp
function multiply(a, b) {
    return a * b;
}

let result = multiply(5, 3);  // 15
```

### Parameter Count Mismatch

Behl is flexible with argument counts:

```cpp
function example(a, b, c) {
    print(tostring(a) + ", " + tostring(b) + ", " + tostring(c));
}

example(1, 2, 3);     // "1, 2, 3"
example(1, 2);        // "1, 2, nil" (missing args = nil)
example(1, 2, 3, 4);  // "1, 2, 3" (extra args ignored)
```

## Return Values

### Single Return

```cpp
function square(x) {
    return x * x;
}

let result = square(5);  // 25
```

### No Return

Functions without `return` statement return `nil`:

```cpp
function doSomething() {
    print("Done");
}

let result = doSomething();
print(result);  // nil
```

### Multiple Returns

Functions can return multiple values:

```cpp
function divmod(a, b) {
    return a / b, a % b;
}

let quotient, remainder = divmod(10, 3);
// quotient = 3, remainder = 1
```

### Multiple Return Semantics

Multiple returns behave differently based on context:

#### Context 1: Last Argument Position

When a multi-return call is the **last argument**, all values are passed:

```cpp
function triple() {
    return 1, 2, 3;
}

function add3(a, b, c) {
    return a + b + c;
}

let result = add3(triple());  // Gets all 3 values: 1, 2, 3
print(result);  // 6
```

#### Context 2: Middle Argument Position

When a multi-return call is **not the last argument**, only the first value is used:

```cpp
let result = add3(triple(), 10, 20);
// triple() returns only 1 (first value)
// Result: 1 + 10 + 20 = 31
```

#### Context 3: Multiple Assignment

```cpp
let x, y, z = triple();  // x=1, y=2, z=3
```

#### Context 4: Fewer Variables

Excess values are discarded:

```cpp
let a, b = triple();  // a=1, b=2 (3 discarded)
let p = triple();     // p=1 (2, 3 discarded)
```

#### Context 5: More Variables

Missing values become `nil`:

```cpp
let a, b, c, d = triple();  // a=1, b=2, c=3, d=nil
```

### Examples

```cpp
function getCoords() {
    return 10, 20, 30;
}

// All values
let x, y, z = getCoords();  // x=10, y=20, z=30

// Subset
let a, b = getCoords();     // a=10, b=20

// Single
let p = getCoords();        // p=10

// With extra variables
let m, n, o, p = getCoords();  // m=10, n=20, o=30, p=nil

// In function call (last position)
function sum3(a, b, c) { return a + b + c; }
print(sum3(getCoords()));   // 60 (10+20+30)

// In function call (middle position)
print(sum3(getCoords(), 5, 5));  // 20 (10+5+5)
```

## First-Class Functions

Functions are values and can be:
- Assigned to variables
- Passed as arguments
- Returned from functions
- Stored in tables

### Function Variables

```cpp
// Named function
function add(a, b) {
    return a + b;
}

// Assign to variable
let fn = add;
print(fn(5, 3));  // 8

// Anonymous function
let multiply = function(a, b) {
    return a * b;
};
print(multiply(5, 3));  // 15
```

### Functions as Arguments

```cpp
function apply(f, value) {
    return f(value);
}

function square(x) {
    return x * x;
}

let result = apply(square, 5);  // 25

// With anonymous function
let result2 = apply(function(x) {
    return x * 2;
}, 10);  // 20
```

### Functions as Return Values

```cpp
function makeAdder(n) {
    return function(x) {
        return x + n;
    };
}

let add5 = makeAdder(5);
let add10 = makeAdder(10);

print(add5(3));   // 8
print(add10(3));  // 13
```

### Functions in Tables

```cpp
let math_ops = {
    ["add"] = function(a, b) { return a + b; },
    ["sub"] = function(a, b) { return a - b; },
    ["mul"] = function(a, b) { return a * b; }
};

print(math_ops["add"](5, 3));  // 8
print(math_ops["mul"](5, 3));  // 15
```

## Closures

Functions capture variables from their enclosing scope:

### Basic Closure

```cpp
function makeCounter() {
    let count = 0;
    return function() {
        count = count + 1;
        return count;
    };
}

let counter = makeCounter();
print(counter());  // 1
print(counter());  // 2
print(counter());  // 3
```

### Multiple Closures

Each closure has its own captured state:

```cpp
let counter1 = makeCounter();
let counter2 = makeCounter();

print(counter1());  // 1
print(counter1());  // 2
print(counter2());  // 1 (independent)
print(counter1());  // 3
```

### Capturing Parameters

```cpp
function makeMultiplier(factor) {
    return function(x) {
        return x * factor;
    };
}

let double = makeMultiplier(2);
let triple = makeMultiplier(3);

print(double(5));  // 10
print(triple(5));  // 15
```

### Mutable Captures

Closures can modify captured variables:

```cpp
function makeAccumulator() {
    let total = 0;
    return function(value) {
        total = total + value;
        return total;
    };
}

let acc = makeAccumulator();
print(acc(5));   // 5
print(acc(10));  // 15
print(acc(3));   // 18
```

## Recursion

Functions can call themselves:

### Basic Recursion

```cpp
function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

print(factorial(5));  // 120
```

### Fibonacci

```cpp
function fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

print(fibonacci(10));  // 55
```

### Tail Recursion

Behl optimizes tail calls. Functions that end with a call in tail position reuse the current stack frame:

```cpp
// Tail-call optimized
function countdown(n) {
    if (n <= 0) {
        print("Done!");
        return;
    }
    print(n);
    return countdown(n - 1);  // Tail position - optimized
}

// Also optimized
function factorial(n, acc) {
    if (n == 0) {
        return acc;
    }
    return factorial(n - 1, n * acc);  // Tail call
}
```

See [Optimizations](../optimizations.md#tail-call-optimization) for details.

## Method Call Syntax

Behl supports method call syntax using the colon `:` operator. This automatically passes the table as the first argument:

```cpp
let obj = {
    name = "Widget",
    getName = function(self) {
        return self.name;
    },
    setName = function(self, newName) {
        self.name = newName;
    }
};

// Using colon syntax (self is passed automatically)
print(obj:getName());           // "Widget"
obj:setName("Gadget");
print(obj:getName());           // "Gadget"

// Equivalent to bracket syntax (self passed explicitly)
print(obj["getName"](obj));     // "Widget"
obj["setName"](obj, "Gadget");
```

### Method Calls vs Function Calls

```cpp
let obj = {
    value = 42,
    
    method = function(self, x) {
        return self.value + x;
    }
};

// Method call: obj is passed as first argument
let result = obj:method(10);    // 52

// Function call: must pass obj explicitly
let result = obj.method(obj, 10);  // 52
let result = obj["method"](obj, 10);  // 52
```

### Creating Object-Like Structures

```cpp
function createCounter(start) {
    let counter = {
        count = start,
        
        increment = function(self) {
            self.count = self.count + 1;
            return self.count;
        },
        
        decrement = function(self) {
            self.count = self.count - 1;
            return self.count;
        },
        
        get = function(self) {
            return self.count;
        }
    };
    return counter;
}

let c = createCounter(10);
print(c:increment());  // 11
print(c:increment());  // 12
print(c:decrement());  // 11
print(c:get());        // 11
```

### Method Calls with Dot Notation

You can combine dot notation with method calls:

```cpp
let obj = {
    inner = {
        value = 100,
        getValue = function(self) {
            return self.value;
        }
    }
};

print(obj.inner:getValue());  // 100
```

## Variadic Functions

Functions can accept variable numbers of arguments using `...`:

```cpp
function sum(...) {
    let args = {...};  // Pack into table
    let total = 0;
    for (let i = 0; i < #args; i++) {
        total = total + args[i];
    }
    return total;
}

print(sum(1, 2, 3, 4, 5));  // 15
```

See [Varargs](varargs) for complete documentation.

## Function Scope

Functions create their own scope:

```cpp
let globalVar = "global";

function outer() {
    let outerVar = "outer";
    
    function inner() {
        let innerVar = "inner";
        
        // Can access all enclosing scopes
        print(globalVar);   // "global"
        print(outerVar);    // "outer"
        print(innerVar);    // "inner"
    }
    
    inner();
    // print(innerVar);  // Error: not accessible here
}

outer();
// print(outerVar);  // Error: not accessible here
```

## Best Practices

1. **Single responsibility** - Each function should do one thing well
2. **Descriptive names** - Use verbs for function names (`getUser`, `calculateTotal`)
3. **Limit parameters** - Prefer 0-3 parameters; use tables for more
4. **Return early** - Exit early on special cases
5. **Pure functions** - Prefer functions without side effects when possible
6. **Avoid deep nesting** - Extract nested logic into separate functions

```cpp
// Good: Clear, single purpose
function calculateTax(amount, rate) {
    if (amount < 0) {
        return 0;  // Early return
    }
    return amount * rate;
}

// Good: Using a table for many parameters
function createUser(config) {
    let name = config["name"];
    let email = config["email"];
    let age = config["age"];
    // ...
}

createUser({
    ["name"] = "Alice",
    ["email"] = "alice@example.com",
    ["age"] = 30
});
```
