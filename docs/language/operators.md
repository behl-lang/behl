---
layout: default
title: Operators
parent: Language
nav_order: 4
---

# Operators
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `5 + 3` → `8` |
| `-` | Subtraction | `5 - 3` → `2` |
| `*` | Multiplication | `5 * 3` → `15` |
| `/` | Division | `5 / 2` → `2.5` |
| `%` | Modulo | `5 % 2` → `1` |
| `**` | Power | `2 ** 3` → `8` |
| `-` | Unary negation | `-5` → `-5` |

### Examples

```cpp
let sum = 10 + 5;      // 15
let diff = 10 - 5;     // 5
let product = 10 * 5;  // 50
let quotient = 10 / 3; // 3.333...
let remainder = 10 % 3; // 1
let power = 2 ** 10;   // 1024
let neg = -42;         // -42
```

### Integer vs Float Division

```cpp
let a = 10 / 2;  // 5 (integer - exact division)
let b = 10 / 3;  // 3.333... (float - inexact)
let c = 5.0 / 2; // 2.5 (float - operand is float)
```

## Comparison Operators

| Operator | Description |
|----------|-------------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `<=` | Less or equal |
| `>` | Greater than |
| `>=` | Greater or equal |

### Examples

```cpp
let a = 5, b = 10;

print(a == b);  // false
print(a != b);  // true
print(a < b);   // true
print(a <= b);  // true
print(a > b);   // false
print(a >= b);  // false
```

### Equality Semantics

```cpp
// Value equality for primitives
print(42 == 42);        // true
print("hello" == "hello"); // true

// Reference equality for tables
let t1 = {1, 2, 3};
let t2 = {1, 2, 3};
let t3 = t1;

print(t1 == t2);  // false (different tables)
print(t1 == t3);  // true (same reference)
```

## Logical Operators

| Operator | Description | Short-circuits |
|----------|-------------|----------------|
| `&&` | Logical AND | Yes |
| <code>&#124;&#124;</code> | Logical OR | Yes |
| `!` | Logical NOT | N/A |

### Short-Circuit Evaluation

Logical operators short-circuit: they don't evaluate the right operand if the result is determined by the left operand.

```cpp
// AND: if left is false, right is not evaluated
let x = false && expensive_function();  // expensive_function() not called

// OR: if left is true, right is not evaluated
let y = true || expensive_function();   // expensive_function() not called

// Useful for safe access
let safe = (obj != nil) && (obj["value"] > 0);
```

### Examples

```cpp
let a = true, b = false;

print(a && b);   // false
print(a || b);   // true
print(!a);       // false
print(!b);       // true

// Chaining
print(a && b || !a);  // true
```

### Truthiness

Only `false` and `nil` are falsy. Everything else is truthy:

```cpp
if (0) { print("0 is truthy"); }         // Prints
if ("") { print("Empty string is truthy"); }  // Prints
if ({}) { print("Empty table is truthy"); }   // Prints

if (false) { print("Won't print"); }
if (nil) { print("Won't print"); }
```

## Bitwise Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&` | Bitwise AND | `5 & 3` → `1` |
| <code>&#124;</code> | Bitwise OR | <code>5 &#124; 3</code> → `7` |
| `^` | Bitwise XOR | `5 ^ 3` → `6` |
| `~` | Bitwise NOT | `~5` → `-6` |
| `<<` | Left shift | `5 << 1` → `10` |
| `>>` | Right shift | `5 >> 1` → `2` |

### Examples

```cpp
let a = 0b1010;  // 10 in binary
let b = 0b1100;  // 12 in binary

print(a & b);    // 0b1000 = 8
print(a | b);    // 0b1110 = 14
print(a ^ b);    // 0b0110 = 6
print(~a);       // 0b...11110101 = -11

// Shifts
let x = 1 << 3;  // 8 (1 * 2^3)
let y = 16 >> 2; // 4 (16 / 2^2)
```

### Use Cases

```cpp
// Flags
const FLAG_A = 1 << 0;  // 0b0001
const FLAG_B = 1 << 1;  // 0b0010
const FLAG_C = 1 << 2;  // 0b0100

let flags = FLAG_A | FLAG_C;  // 0b0101
let hasA = (flags & FLAG_A) != 0;  // true
let hasB = (flags & FLAG_B) != 0;  // false
```

## Operator Precedence

From highest to lowest precedence:

| Level | Operators | Description |
|-------|-----------|-------------|
| 1 | `()` | Parentheses (grouping) |
| 2 | `!`, `~`, `-` (unary) | Unary operators |
| 3 | `**` | Power (right-associative) |
| 4 | `*`, `/`, `%` | Multiplicative |
| 5 | `+`, `-` | Additive |
| 6 | `<<`, `>>` | Shift |
| 7 | `<`, `<=`, `>`, `>=` | Relational |
| 8 | `==`, `!=` | Equality |
| 9 | `&` | Bitwise AND |
| 10 | `^` | Bitwise XOR |
| 11 | <code>&#124;</code> | Bitwise OR |
| 12 | `&&` | Logical AND |
| 13 | <code>&#124;&#124;</code> | Logical OR |

### Examples

```cpp
// Without parentheses
let x = 2 + 3 * 4;      // 14 (not 20)
let y = 10 - 5 - 2;     // 3 (left-to-right)
let z = 2 ** 3 ** 2;    // 512 (right-to-left: 2^(3^2))

// With parentheses for clarity
let a = (2 + 3) * 4;    // 20
let b = 10 - (5 - 2);   // 7
let c = (2 ** 3) ** 2;  // 64
```

## Compound Assignment Operators

| Operator | Equivalent |
|----------|------------|
| `x += y` | `x = x + y` |
| `x -= y` | `x = x - y` |
| `x *= y` | `x = x * y` |
| `x /= y` | `x = x / y` |
| `x %= y` | `x = x % y` |
| `x &= y` | `x = x & y` |
| <code>x &#124;= y</code> | <code>x = x &#124; y</code> |
| `x ^= y` | `x = x ^ y` |
| `x <<= y` | `x = x << y` |
| `x >>= y` | `x = x >> y` |

### Examples

```cpp
let x = 10;
x += 5;   // x = 15
x *= 2;   // x = 30
x /= 3;   // x = 10
x %= 7;   // x = 3
```

## Increment/Decrement Operators

| Operator | Description |
|----------|-------------|
| `x++` | Post-increment: use then add |
| `++x` | Pre-increment: add then use |
| `x--` | Post-decrement: use then subtract |
| `--x` | Pre-decrement: subtract then use |

### Examples

```cpp
let i = 5;

// Post-increment
let a = i++;  // a = 5, i = 6

// Pre-increment
let b = ++i;  // b = 7, i = 7

// Post-decrement
let c = i--;  // c = 7, i = 6

// Pre-decrement
let d = --i;  // d = 5, i = 5
```

### In Loops

```cpp
for (let i = 0; i < 10; i++) {   // i++ increments after each iteration
    print(i);
}

for (let i = 0; i < 10; ++i) {   // ++i also works, same effect in for loop
    print(i);
}
```

## Operator Overloading

Tables can define custom operator behavior using metamethods. See [Tables - Metatables](tables#metatables) for details.

```cpp
let vec = {["x"] = 1, ["y"] = 2};
let mt = {
    ["__add"] = function(a, b) {
        return {["x"] = a["x"] + b["x"], ["y"] = a["y"] + b["y"]};
    }
};
setmetatable(vec, mt);

let v2 = {["x"] = 3, ["y"] = 4};
setmetatable(v2, mt);

let result = vec + v2;  // Calls __add metamethod
// result = {["x"] = 4, ["y"] = 6}
```
