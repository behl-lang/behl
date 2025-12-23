---
layout: default
title: Tables
parent: Language
nav_order: 7
---

# Tables
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Tables are the primary data structure in Behl. They serve as:
- **Arrays** - Sequential collections (0-indexed)
- **Dictionaries** - Key-value mappings
- **Objects** - Can hold data and functions

## Table Literals

Table literals use curly braces `{}` with comma-separated entries.

### Syntax Forms

```cpp
// Empty table
let t = {};

// Array entries (no key)
{ 10, 20, 30 }        // Creates [0]=10, [1]=20, [2]=30

// Named entries (identifier = value)
{ name = "Alice", age = 30 }   // Shorthand for ["name"]="Alice", ["age"]=30

// Explicit keys ([key] = value)
{ ["key"] = "value", [0] = 10, [true] = "yes" }  // Any expression as key

// Mixed
{ 10, 20, name = "Alice", ["x"] = 100 }  // Combines all forms
```

**Key syntax rules:**
- `key = value` — Only valid when `key` is an identifier (no spaces, special chars, or keywords)
- `[expr] = value` — Works with any expression as the key
- Plain `value` — Auto-assigns to next array index (0, 1, 2...)

### Empty Table

```cpp
let t = {};
```

### Array-Like Tables

Behl tables are **0-indexed** (unlike Lua's 1-indexed):

```cpp
let arr = {10, 20, 30, 40, 50};

print(arr[0]);  // 10 (first element)
print(arr[1]);  // 20
print(arr[4]);  // 50 (last element)
```

### Dictionary-Like Tables

```cpp
let person = {
    name = "Alice",
    age = 30,
    city = "NYC"
};

print(person["name"]);  // "Alice"
print(person.name);      // "Alice" (dot notation)
```

### Mixed Tables

Tables can contain both sequential and keyed elements:

```cpp
let mixed = {
    10,          // [0] = 10
    20,          // [1] = 20
    key = "val", // ["key"] = "val"
    30,          // [2] = 30
    x = 100      // ["x"] = 100
};

print(mixed[0]);     // 10
print(mixed[2]);     // 30
print(mixed["key"]); // "val"
print(mixed.x);      // 100
```

## Indexing

### Read Access

```cpp
let t = {10, 20, 30};

print(t[0]);   // 10
print(t[1]);   // 20
print(t[999]); // nil (missing keys return nil)
```

### Dot Notation

For string keys that are valid identifiers, you can use dot notation:

```cpp
let person = {
    name = "Alice",
    age = 30
};

// Both work identically:
print(person["name"]);  // "Alice"
print(person.name);     // "Alice"

person["age"] = 31;
person.age = 31;  // Same as above
```

**Limitations of dot notation:**
- Only works with string keys that are valid identifiers
- Cannot use with keys that have spaces, special characters, or are keywords
- Cannot use with non-string keys (numbers, booleans, tables)

```cpp
let t = {};

// These require bracket notation:
t["my key"] = 1;        // Has space
t["123"] = 2;           // Starts with digit
t["if"] = 3;            // Keyword
t[42] = 4;              // Number key
t[true] = 5;            // Boolean key

// These can use either notation:
t.name = "Alice";       // Valid identifier
t.value = 100;
```

### Write Access

```cpp
let t = {};

// Add elements with brackets
t[0] = 10;
t[1] = 20;
t["key"] = "value";

// Add elements with dot notation
t.name = "Alice";
t.count = 42;

// Modify elements
t[0] = 99;
t.name = "Bob";

print(t[0]);       // 99
print(t.name);     // "Bob"
print(t["key"]);   // "value"
```

### Key Types

Keys can be any type except `nil`:

```cpp
let t = {};

// String keys
t["name"] = "Alice";

// Integer keys
t[0] = 10;
t[42] = "answer";

// Boolean keys
t[true] = "yes";
t[false] = "no";

// Table keys (by reference)
let key_table = {1, 2, 3};
t[key_table] = "value";
```

## Table Length

Use `rawlen()` to get the length of the sequential part:

```cpp
let arr = {10, 20, 30, 40, 50};
print(rawlen(arr));  // 5

// rawlen counts consecutive integer keys starting from 0
let sparse = {
    [0] = 1,
    [1] = 2,
    [5] = 3  // Gap at index 2-4
};
print(rawlen(sparse));  // 2 (only counts 0 and 1)

let dict = {["name"] = "Alice", ["age"] = 30};
print(rawlen(dict));  // 0 (no sequential part)
```

### Alternative: `#` Operator

The `#` operator can be overridden with the `__len` metamethod:

```cpp
let t = {10, 20, 30};
print(#t);  // 3 (same as rawlen for simple arrays)

// With custom length via metamethod
let custom = {10, 20, 30};
setmetatable(custom, {
    __len = function(t) {
        return 100;  // Custom length
    }
});
print(#custom);  // 100
```

## Iteration

### Using `pairs()`

Iterate over all key-value pairs:

```cpp
let t = {
    name = "Alice",
    age = 30,
    [0] = 10,  // Use [0] for numeric keys
    [1] = 20
};

for (key, value in pairs(t)) {
    print(tostring(key) + " = " + tostring(value));
}
```

### Iterating Arrays

```cpp
let arr = {10, 20, 30, 40, 50};

// With pairs (gets index and value)
for (i, v in pairs(arr)) {
    print("arr[" + tostring(i) + "] = " + tostring(v));
}

// Manual iteration
for (let i = 0; i < rawlen(arr); i++) {
    print(arr[i]);
}
```

### Order of Iteration

**Important**: Iteration order is **not guaranteed** for non-sequential keys:

```cpp
let t = { c = 3, a = 1, b = 2 };

// Order may vary
for (k, v in pairs(t)) {
    print(k);  // May print in any order
}
```

Sequential keys (0, 1, 2, ...) are typically iterated in order, but don't rely on it for dictionary keys.

## Table Operations

### Copying Tables

Assignment creates a reference, not a copy:

```cpp
let t1 = {1, 2, 3};
let t2 = t1;  // t2 references same table

t2[0] = 99;
print(t1[0]);  // 99 (t1 modified too!)

// Create a shallow copy
function shallowCopy(t) {
    let copy = {};
    for (k, v in pairs(t)) {
        copy[k] = v;
    }
    return copy;
}

let t3 = shallowCopy(t1);
t3[0] = 100;
print(t1[0]);  // 99 (t1 unchanged)
```

### Checking for Keys

```cpp
let t = { name = "Alice" };

// Check if key exists
if (t["name"] != nil) {
    print("name exists");
}

// Can't distinguish between nil value and missing key this way
t["value"] = nil;
print(t["value"]);  // nil (but key exists!)

// Use pairs() to check if key actually exists
function hasKey(t, key) {
    for (k, _ in pairs(t)) {
        if (k == key) {
            return true;
        }
    }
    return false;
}
```

### Removing Keys

Set key to `nil` to remove it:

```cpp
let t = { a = 1, b = 2, c = 3 };

t["b"] = nil;  // Remove key "b"

for (k, v in pairs(t)) {
    print(k);  // Prints "a" and "c" only
}
```

## Tables as Objects

Use tables to create object-like structures:

```cpp
function createPerson(name, age) {
    let person = {
        name = name,
        age = age,
        greet = function(self) {
            print("Hello, I'm " + self.name);
        },
        birthday = function(self) {
            self.age = self.age + 1;
        }
    };
    return person;
}

let alice = createPerson("Alice", 30);
alice.greet(alice);      // "Hello, I'm Alice"
alice.birthday(alice);
print(alice.age);        // 31
```

## Best Practices

1. **Use 0-indexed arrays** - Remember Behl uses 0-based indexing
2. **Prefer sequential tables for arrays** - Better performance
3. **Check for nil** - Missing keys return nil
4. **Copy when needed** - Assignment creates references
5. **Consistent key types** - Don't mix integer and string keys for "array" tables

```cpp
// Good: Pure array (0-indexed)
let arr = {10, 20, 30};

// Good: Pure dictionary
let dict = { name = "Alice", age = 30 };

// Avoid: Mixed with gaps
let mixed = {
    [0] = 1,
    [5] = 2,       // Gap
    ["key"] = 3    // Mixed types
};
```


---

## See Also

- [Metatables](metatables) — Customize table behavior with metamethods
- [Functions](functions) — Using tables with functions
- [Iteration](iteration) — Looping through tables
