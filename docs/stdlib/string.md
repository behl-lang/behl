---
layout: default
title: string
parent: Standard Library
nav_order: 2
---

# string
{: .no_toc }

String manipulation utilities.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The string module provides utilities for string manipulation. It must be explicitly imported:

```cpp
const string = import("string");
let upper = string.upper("hello");
```

---

## string.len(s)

Returns the length of a string.

```cpp
print(string.len("hello"));  // 5
print(string.len(""));       // 0
```

---

## string.sub(s, start, end)

Returns substring from `start` to `end` (inclusive, 0-indexed).

```cpp
let s = "hello";
print(string.sub(s, 0, 1));  // "he"
print(string.sub(s, 1, 3));  // "ell"
print(string.sub(s, 2, 4));  // "llo"
```

**Parameters:**
- `s` - The string to extract from
- `start` - Starting index (0-based)
- `end` - Ending index (inclusive)

---

## string.find(s, pattern, start)

Finds pattern in string, returns start and end indices (or `nil` if not found).

```cpp
let idx_start, idx_end = string.find("hello world", "world");
// idx_start = 6, idx_end = 10

let result = string.find("hello", "xyz");
// result = nil (not found)
```

**Parameters:**
- `s` - String to search in
- `pattern` - Substring to find
- `start` - Optional starting position (default: 0)

**Returns:**
- Start and end indices if found
- `nil` if not found

---

## string.upper(s) / string.lower(s)

Convert case.

```cpp
print(string.upper("hello"));  // "HELLO"
print(string.lower("WORLD"));  // "world"
print(string.upper("Hello World!"));  // "HELLO WORLD!"
```

---

## string.reverse(s)

Reverse a string.

```cpp
print(string.reverse("hello"));  // "olleh"
print(string.reverse("12345"));  // "54321"
```

---

## string.format(fmt, ...)

Formatted string creation using `{}` placeholders.

```cpp
let s = string.format("Value: {}, Name: {}", 42, "test");
print(s);  // "Value: 42, Name: test"

let formatted = string.format("Pi: {}", 3.14159);
print(formatted);  // "Pi: 3.14159"

let escaped = string.format("Braces: {{ and }}");
print(escaped);  // "Braces: { and }"
```

**Supported Format Specifications:**

```cpp
// Argument indexing and reordering
let s = string.format("{1} {0}", "world", "hello");  // "hello world"

// Hex formatting
let hex = string.format("Hex: {:x}", 255);           // "Hex: ff"
let HEX = string.format("HEX: {:X}", 255);           // "HEX: FF"

// Width and alignment
let padded = string.format("Padded: {:5}", 42);      // "Padded:    42"
let left = string.format("Left: {:<5}", 42);         // "Left: 42   "
let right = string.format("Right: {:>5}", 42);       // "Right:    42"
let center = string.format("Center: {:^5}", 42);     // "Center:  42  "

// Float precision
let precise = string.format("Pi: {:.2}", 3.14159);   // "Pi: 3.14"
let fixed = string.format("Fixed: {:.4}", 2.5);      // "Fixed: 2.5000"

// Combined specifiers
let combo = string.format("{:>8.2}", 3.14159);       // "    3.14"
```

**Format Syntax:**
- `{}` - Automatic argument
- `{n}` - Indexed argument (0-based)
- `{:x}` / `{:X}` - Hexadecimal (lowercase/uppercase)
- `{:d}` - Decimal (explicit)
- `{:f}` - Float fixed-point
- `{:fill<width}` - Left align with fill character
- `{:fill>width}` - Right align with fill character
- `{:fill^width}` - Center align with fill character
- `{:<width}` - Left align (space fill)
- `{:>width}` - Right align (space fill)
- `{:^width}` - Center align (space fill)
- `{:width}` - Minimum width (right-aligned by default for numbers)
- `{:.precision}` - Float precision
- `{:width.precision}` - Combined width and precision
- `{:{}}` - Dynamic width from next argument (sequential)
- `{:.{}}` - Dynamic precision from next argument (sequential)
- `{:{}.{}}` - Dynamic width and precision (sequential)
- `{0:{1}}` - Indexed dynamic width (arg 1 is width for arg 0)
- `{0:.{1}}` - Indexed dynamic precision
- `{0:{1}.{2}}` - Indexed dynamic width and precision
- `{{` / `}}` - Escaped braces

**Example with dynamic parameters:**
```cpp
// Sequential consumption
let value = 42;
let w = 8;
let s = string.format("{:{}}", value, w);  // "      42" (width 8)

// Indexed parameters
let pi = 3.14159;
let width = 10;
let prec = 2;
let formatted = string.format("{0:{1}.{2}}", pi, width, prec);  // "      3.14"

// Out of order
let greeting = string.format("{1:{0}}", 10, "Hello");  // "     Hello"
```

**UTF-8 Support:** Format strings and arguments handle UTF-8 transparently. Multi-byte UTF-8 sequences are preserved in literal text and string arguments

---

## Example Usage

```cpp
const string = import("string");

// Text processing
let text = "Hello, World!";
let upper = string.upper(text);
let reversed = string.reverse(text);

print(upper);     // "HELLO, WORLD!"
print(reversed);  // "!dlroW ,olleH"

// String search
let message = "The quick brown fox";
let start, end = string.find(message, "quick");
if (start != nil) {
    print("Found at position: " + tostring(start));
}

// Substring extraction
let url = "https://example.com/path";
let domain = string.sub(url, 8, 18);  // "example.com"
print(domain);

// Formatted output
let name = "Alice";
let age = 30;
let bio = string.format("{} is {} years old", name, age);
print(bio);  // "Alice is 30 years old"
```
