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

## string.find(s, substr, start)

Finds a substring in a string (plain substring search, no patterns), returns the start index, or `-1` if not found.

```cpp
let idx = string.find("hello world", "world");
// idx = 6

let result = string.find("hello", "xyz");
// result = -1 (not found)
```

**Parameters:**
- `s` - String to search in
- `substr` - Substring to find
- `start` - Optional starting position (default: 0)

**Returns:**
- The start index if found
- `-1` if not found

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

## string.char(...)

Converts one or more character codes (0-255) into a string.

```cpp
print(string.char(104, 105));  // "hi"
```

---

## string.byte(s, index)

Returns the byte value at `index` (0-based, default 0). Returns `nil` if the string is empty or the index is out of range.

```cpp
print(string.byte("A"));        // 65
print(string.byte("hello", 1)); // 101 ('e')
```

**Parameters:**
- `s` - The string
- `index` - Optional byte index (0-based, default: 0)

---

## string.rep(s, n)

Repeats a string `n` times. Returns an empty string if `n <= 0`.

```cpp
print(string.rep("ab", 3));  // "ababab"
print(string.rep("x", 0));   // ""
```

---

## string.split(s, sep)

Splits a string by a separator (plain substring, no patterns) into a 0-indexed table.

```cpp
let parts = string.split("a,b,c", ",");
print(parts[0]);  // "a"
print(parts[1]);  // "b"
print(parts[2]);  // "c"
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
- `{:f}` - Accepted but has no effect beyond `{:.precision}`; float formatting is controlled by precision only
- `{:<width}` - Left align (space fill)
- `{:>width}` - Right align (space fill)
- `{:^width}` - Center align (space fill)
- `{:width}` - Minimum width (right-aligned by default for numbers)
- `{:.precision}` - Float precision
- `{:width.precision}` - Combined width and precision
- `{{` / `}}` - Escaped braces

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
let start = string.find(message, "quick");
if (start != -1) {
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
