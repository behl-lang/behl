---
layout: default
title: math
parent: Standard Library
nav_order: 3
---

# math
{: .no_toc }

Mathematical functions and constants.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The math module provides mathematical functions and constants. It's available as a global or can be explicitly imported:

```cpp
// Direct access (implicit global)
print(math.PI);

// Or explicit import
const math = import("math");
print(math.PI);
```

---

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `math.PI` | 3.14159... | π (pi) |
| `math.E` | 2.71828... | e (Euler's number) |

---

## Basic Functions

### math.abs(x)
Absolute value.

```cpp
print(math.abs(-5));   // 5
print(math.abs(3.14)); // 3.14
```

### math.floor(x) / math.ceil(x) / math.round(x)
Rounding functions.

```cpp
print(math.floor(3.7));  // 3
print(math.ceil(3.2));   // 4
print(math.round(3.5));  // 4
```

### math.min(x, y) / math.max(x, y)
Return minimum or maximum of two numbers.

```cpp
print(math.min(5, 10));  // 5
print(math.max(5, 10));  // 10
```

### math.sqrt(x)
Square root.

```cpp
print(math.sqrt(16));  // 4
print(math.sqrt(2));   // 1.41421...
```

### math.pow(x, y) / math.exp(x)
Power and exponential.

```cpp
print(math.pow(2, 8));   // 256
print(math.exp(1));      // 2.71828... (e^1)
```

### math.log(x) / math.log10(x) / math.log2(x)
Logarithms.

```cpp
print(math.log(math.E));  // 1 (natural log)
print(math.log10(100));      // 2
print(math.log2(8));         // 3
```

---

## Trigonometric Functions

### Basic Trigonometry

```cpp
print(math.sin(math.PI / 2));   // 1
print(math.cos(0));                 // 1
print(math.tan(math.PI / 4));   // 1
```

### Inverse Trigonometry

```cpp
print(math.asin(1));      // π/2
print(math.acos(0));      // π/2
print(math.atan(1));      // π/4
print(math.atan2(y, x));  // atan(y/x) with proper quadrant
```

---

## Hyperbolic Functions

```cpp
print(math.sinh(x));
print(math.cosh(x));
print(math.tanh(x));
```

### Inverse Hyperbolic

```cpp
print(math.asinh(x));
print(math.acosh(x));
print(math.atanh(x));
```

---

## Advanced Functions

### math.fmod(x, y)
Floating-point modulo.

```cpp
print(math.fmod(7.5, 2.0));  // 1.5
```

### math.modf(x)
Split number into integer and fractional parts.

```cpp
let int_part, frac_part = math.modf(3.14);
// int_part = 3, frac_part = 0.14
```

### math.hypot(x, y)
Euclidean distance: sqrt(x² + y²).

```cpp
print(math.hypot(3, 4));  // 5
```

### math.ldexp(m, e)
Multiply by power of 2: m × 2^e.

```cpp
print(math.ldexp(0.5, 3));  // 0.5 * 2^3 = 4
```

### math.frexp(x)
Extract mantissa and exponent.

```cpp
let mantissa, exp = math.frexp(8);
// mantissa = 0.5, exp = 4 (since 8 = 0.5 * 2^4)
```

---

## Type Checking

### math.is_nan(x)
Check if value is NaN (Not a Number).

```cpp
let nan = 0 / 0;
print(math.is_nan(nan));  // true
```

### math.is_inf(x)
Check if value is infinite.

```cpp
print(math.is_inf(1 / 0));  // true
```

### math.is_finite(x)
Check if value is finite (not NaN or infinity).

```cpp
print(math.is_finite(42));  // true
```

---

## Example Usage

```cpp
const math = import("math");

// Calculate circle area
let radius = 5;
let area = math.PI * math.pow(radius, 2);
print("Area: " + tostring(area));

// Distance between two points
let dx = 3;
let dy = 4;
let distance = math.hypot(dx, dy);
print("Distance: " + tostring(distance));  // 5

// Angle calculation
let angle = math.atan2(dy, dx);
print("Angle: " + tostring(angle) + " radians");
```
