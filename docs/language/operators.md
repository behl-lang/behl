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
| `/` | Division (always float) | `5 / 2` → `2.5` |
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

### Division Always Produces a Float

`/` never performs integer division. Both operands are converted to floats, so the result is a float even when the division is exact:

```cpp
let a = 10 / 2;  // 5.0 (float, not integer 5)
let b = 10 / 3;  // 3.333... (float)
let c = 5.0 / 2; // 2.5 (float)
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
let a, b = 5, 10;

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
let a = 10;  // 1010 in binary
let b = 12;  // 1100 in binary

print(a & b);    // 8
print(a | b);    // 14
print(a ^ b);    // 6
print(~a);       // -11

// Shifts
let x = 1 << 3;  // 8 (1 * 2^3)
let y = 16 >> 2; // 4 (16 / 2^2)
```

### Use Cases

```cpp
// Flags
const FLAG_A = 1 << 0;  // 1 (bit 0)
const FLAG_B = 1 << 1;  // 2 (bit 1)
const FLAG_C = 1 << 2;  // 4 (bit 2)

let flags = FLAG_A | FLAG_C;  // 5 (bits 0 and 2)
let hasA = (flags & FLAG_A) != 0;  // true
let hasB = (flags & FLAG_B) != 0;  // false
```

## Operator Precedence

From highest to lowest precedence:

| Level | Operators | Description |
|-------|-----------|-------------|
| 1 | `()` | Parentheses (grouping) |
| 2 | `!`, `~`, `-`, `#` (unary) | Unary operators |
| 3 | `**` | Power (right-associative) |
| 4 | `*`, `/`, `%` | Multiplicative |
| 5 | `+`, `-` | Additive |
| 6 | `<<`, `>>` | Shift |
| 7 | `&` | Bitwise AND |
| 8 | `^` | Bitwise XOR |
| 9 | <code>&#124;</code> | Bitwise OR |
| 10 | `<`, `<=`, `>`, `>=`, `==`, `!=` | Comparison (relational and equality share one level) |
| 11 | `&&` | Logical AND |
| 12 | <code>&#124;&#124;</code> | Logical OR |
| 13 | `?:` | Ternary conditional (right-associative) |

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

There are no bitwise compound assignments: `&=`, `|=`, `^=`, `<<=` and `>>=` do not exist. Write `x = x & y` instead.

### Examples

```cpp
let x = 10;
x += 5;   // x = 15
x *= 2;   // x = 30
x /= 3;   // x = 10.0 (division always yields a float)
x %= 7;   // x = 3.0 (still a float)
```

## Increment/Decrement Operators

| Operator | Description |
|----------|-------------|
| `x++` | Increment by one |
| `x--` | Decrement by one |

Only the postfix forms exist. The prefix forms `++x` and `--x` are not parsed at all and are a syntax error.

`x++` and `x--` are **statements**, not expressions. They produce no value, so they cannot appear inside a larger expression:

```cpp
let i = 5;

i++;          // OK: statement
i--;          // OK: statement

// let a = i++;   // Syntax error: not an expression
// let b = ++i;   // Syntax error: prefix form does not exist
```

The left-hand side may be an identifier, an index, or a member:

```cpp
let t = {0, 0};
t[0]++;
```

### In Loops

The other place `++` and `--` are accepted is the update slot of a C-style `for` loop:

```cpp
for (let i = 0; i < 10; i++) {   // i++ increments after each iteration
    print(i);
}

// for (let i = 0; i < 10; ++i)  // Syntax error: prefix form does not exist
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
