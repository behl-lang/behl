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

The string module provides utilities for string manipulation. It's available as a global or can be explicitly imported:

```cpp
// Direct access (implicit global)
let upper = string.upper("hello");

// Or explicit import
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

**Note:** Uses `{}` placeholder syntax (like Python/Rust), not printf-style `%d/%s/%f`.

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
