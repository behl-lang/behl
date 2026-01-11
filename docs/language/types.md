---
layout: default
title: Values & Types
parent: Language
nav_order: 2
---

# Types and Values
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Behl is dynamically typed. Values carry type information at runtime, not variables.

```cpp
let x = 42;        // x holds an integer
x = "hello";       // now x holds a string
x = {1, 2, 3};     // now x holds a table
```

## Basic Types

| Type | Description | Example |
|------|-------------|---------|
| `nil` | Absence of value | `nil` |
| `boolean` | True or false | `true`, `false` |
| `integer` | 64-bit signed integer | `42`, `0xFF`, `-10` |
| `number` | 64-bit floating point | `3.14`, `1.5e-10` |
| `string` | Immutable text | `"hello"`, `'world'` |
| `function` | Callable function | Functions and closures |
| `table` | Associative array | `{1, 2, 3}` |
| `userdata` | C++ objects | Managed from C++ API |

## Type Query

Use `typeof()` to inspect a value's type:

```cpp
let x = 42;
print(typeof(x)); // "integer"

let y = 3.14;
print(typeof(y)); // "number"

let s = "hello";
print(typeof(s)); // "string"
```

## Nil

`nil` represents the absence of a value:

```cpp
let x = nil;
if (x == nil) {
    print("x is nil");
}

// Accessing non-existent table keys returns nil
let t = {};
print(t.missing); // nil
```

## Booleans

Boolean values are `true` and `false`:

```cpp
let isTrue = true;
let isFalse = false;

// Truthiness: Only false and nil are falsy
// Everything else is truthy (including 0 and empty strings)
if (0) {
    print("0 is truthy!"); // This prints
}

if ("") {
    print("Empty string is truthy!"); // This also prints
}

if (false) {
    print("Won't print");
}

if (nil) {
    print("Won't print");
}
```

## Numbers

Behl distinguishes between integers and floating-point numbers:

### Integer Literals

```cpp
let dec = 42;      // Decimal
let hex = 0xFF;    // Hexadecimal (255)
let neg = -10;     // Negative

// Hexadecimal (case insensitive)
let a = 0xff;      // 255
let b = 0xFF;      // 255
let c = 0XFF;      // 255
let d = 0xAbCdEf;  // 11259375

// Leading zeros are ignored (NOT octal)
let e = 007;       // 7 (decimal, not octal)
let f = 0123;      // 123 (decimal, not octal)
```

**Note**: Unlike C, leading zeros do NOT indicate octal. `007` is decimal 7, not octal 7.

### Floating-Point Literals

```cpp
let f1 = 3.14;
let f2 = 1.5e-10;  // Scientific notation
let f3 = .5;       // 0.5
let f4 = 5.;       // 5.0
```

### Type Promotion

Arithmetic operations promote to floating-point when needed:

```cpp
let x = 10 / 3;    // 3.333... (number)
let y = 10 / 2;    // 5 (integer - exact division)
let z = 5 + 3.14;  // 8.14 (promoted to number)
```

### Number Precision

- **Integers**: 64-bit signed (-2^63 to 2^63-1)
- **Floats**: IEEE 754 double precision (64-bit)

### Integer Overflow

Integer arithmetic uses **wrapping** (two's complement) on overflow:

```cpp
let max = 9223372036854775807  // INT64_MAX
let overflowed = max + 1        // Wraps to INT64_MIN (-9223372036854775808)

let min = -9223372036854775808  // INT64_MIN  
let underflowed = min - 1       // Wraps to INT64_MAX (9223372036854775807)

// Multiplication overflow
let large = 9223372036854775807
let result = large * 2          // Wraps to -2
```

**Note**: Overflow does not cause errors or convert to floats - values wrap around silently.

## Strings

Strings are immutable sequences of characters:

### String Literals

```cpp
let s1 = "double quotes";
let s2 = 'single quotes';

// Both styles are equivalent
let same1 = "hello";
let same2 = 'hello';
print(same1 == same2); // true
```

### Escape Sequences

| Sequence | Meaning |
|----------|---------|
| `\a` | Bell/Alert (ASCII 7) |
| `\b` | Backspace (ASCII 8) |
| `\f` | Form feed (ASCII 12) |
| `\n` | Newline (ASCII 10) |
| `\r` | Carriage return (ASCII 13) |
| `\t` | Horizontal tab (ASCII 9) |
| `\v` | Vertical tab (ASCII 11) |
| `\\` | Backslash |
| `\"` | Double quote |
| `\'` | Single quote |
| `\<newline>` | Escaped newline (ignored, allows multi-line literals) |

```cpp
let multiline = "Line 1\nLine 2\nLine 3";
let tabs = "Column1\tColumn2\tColumn3";
let quoted = "She said \"Hello!\"";

// Escaped newline allows breaking long strings
let long_str = "This is a very \
long string that spans \
multiple lines in source code";
```

### String Concatenation

Use the `+` operator to concatenate strings:

```cpp
let greeting = "Hello, " + "World!";
let msg = "Count: " + tostring(42);  // Convert numbers to strings

// Chain multiple concatenations
let full = "First " + "Second " + "Third";
```

**Note**: Automatic coercion is **not** supported. Use `tostring()` to convert numbers:

```cpp
let x = "Value: " + 42;          // Error!
let x = "Value: " + tostring(42); // OK
```

### String Length

Use the `#` operator (via rawlen) or `string.len()`:

```cpp
const string = import("string");

let s = "hello";
print(rawlen(s));        // 5
print(string.len(s));    // 5
```

### String Operations

See [Standard Library - string module](../standard-library#string-module) for string manipulation functions:

```cpp
const string = import("string");

string.upper("hello");      // "HELLO"
string.sub("hello", 0, 2);  // "hel"
string.find("hello", "ll"); // 2
```

## Functions

Functions are first-class values:

```cpp
function greet(name) {
    return "Hello, " + name;
}

// Functions are values
let fn = greet;
print(typeof(fn)); // "function"
```

See [Functions](functions) for complete documentation.

## Tables

Tables are associative arrays that can serve as arrays, dictionaries, or objects:

```cpp
let arr = {10, 20, 30};     // Array-like
let dict = {key = 42};  // Dictionary-like

print(typeof(arr));  // "table"
print(typeof(dict)); // "table"
```

See [Tables](tables) for complete documentation.

## Userdata

Userdata represents opaque C++ objects exposed to Behl scripts through the C++ API. They are used to wrap native resources like file handles, network sockets, or custom data structures.

### Characteristics

- **Opaque**: Scripts cannot directly access userdata fields
- **Type-safe**: Each userdata has a unique identifier (UID) to prevent type confusion
- **Garbage collected**: Automatically freed when no longer referenced
- **Metatable support**: Can have metatables for custom behavior
- **Finalizers**: `__gc` metamethod called before collection

### Usage from Scripts

```js
// Created from C++ API
let file = os.open("data.txt", "r");
print(typeof(file));  // "userdata"

// Access through C++ functions
let line = file_read(file);

// Type checking
if (typeof(file) == "userdata") {
    // It's userdata, but we don't know which type
}
```

### Features

**Metatables:**
Userdata can have metatables for custom operators and methods:

```js
let v1 = vec2_new(1.0, 2.0);
let v2 = vec2_new(3.0, 4.0);
let v3 = v1 + v2;  // Calls __add metamethod
print(v3);         // Calls __tostring
```

**Automatic cleanup:**
Userdata with `__gc` metamethods are automatically finalized:

```js
function process_file() {
    let f = file_open("data.txt", "r");
    // ... use file ...
    // File automatically closed by GC even if we forget
}
```

**Type safety:**
The C++ API enforces type checking:

```js
let table = {};
file_read(table);  // TypeError: bad argument #1 (expected userdata, got table)

let file1 = file_open("a.txt", "r");
let vec = vec2_new(1.0, 2.0);
file_read(vec);    // TypeError: userdata type mismatch
```

### Creating Userdata

See [Userdata](../embedding/userdata) for details on creating and managing userdata types from C++.

**Common use cases:**
- File I/O handles
- Database connections
- Graphics resources (textures, shaders)
- Network sockets
- Complex data structures (trees, graphs)
- Native library bindings

## Type Conversions

### Explicit Conversions

| Function | Description |
|----------|-------------|
| `tonumber(x)` | Convert to number, returns nil on failure |
| `tostring(x)` | Convert to string |

```cpp
let n = tonumber("42");     // 42
let s = tostring(123);      // "123"
let bad = tonumber("xyz");  // nil
```

### Implicit Conversions

Behl has **minimal** implicit conversion:

- **Arithmetic**: Integer + Float → Float
- **Comparison**: Same-type comparison only
- **Strings**: No automatic number coercion

```cpp
let x = 5 + 3.14;         // OK: 8.14 (promoted to float)
let s = "Value: " + 42;   // Error: no auto conversion
let ok = "Value: " + tostring(42); // OK: "Value: 42"
```
