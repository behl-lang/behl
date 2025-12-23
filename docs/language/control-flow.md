---
layout: default
title: Control Flow
parent: Language
nav_order: 5
---

# Control Flow
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## If Statement

### Basic If

```cpp
if (condition) {
    // executed if condition is true
}
```

### Single Statement (Braces Optional)

For single statements, braces are optional:

```cpp
if (x > 10)
    print("Greater than 10")

// Multiple statements require braces
if (x > 10) {
    print("Greater than 10")
    x = 0
}
```

### If-Else

```cpp
if (condition) {
    // executed if condition is true
} else {
    // executed if condition is false
}
```

### If-Elseif-Else

Chain multiple conditions:

```cpp
if (condition1) {
    // executed if condition1 is true
} elseif (condition2) {
    // executed if condition1 is false and condition2 is true
} elseif (condition3) {
    // executed if condition1 and condition2 are false and condition3 is true
} else {
    // executed if all conditions are false
}
```

### Examples

```cpp
let score = 85;

if (score >= 90) {
    print("A");
} elseif (score >= 80) {
    print("B");
} elseif (score >= 70) {
    print("C");
} else {
    print("F");
}
```

### Truthiness

Only `false` and `nil` are falsy:

```cpp
if (0) { print("0 is truthy"); }        // Prints
if ("") { print("Empty string"); }      // Prints
if ({}) { print("Empty table"); }       // Prints

if (false) { print("Won't print"); }
if (nil) { print("Won't print"); }
```

## While Loop

Execute code repeatedly while a condition is true:

```cpp
while (condition) {
    // loop body
}

// Single statement - braces optional
while (i < 10)
    i = i + 1
```

### Examples

```cpp
let i = 0;
while (i < 10) {
    print(i);
    i = i + 1;
}

// Infinite loop (use break to exit)
while (true) {
    let input = getUserInput();
    if (input == "quit") {
        break;
    }
    processInput(input);
}
```

## For Loop

### C-Style For Loop

```cpp
for (initialization; condition; increment) {
    // loop body
}
```

Components:
- **initialization**: Executed once before the loop
- **condition**: Checked before each iteration
- **increment**: Executed after each iteration

### Examples

```cpp
// Basic loop
for (let i = 0; i < 10; i++) {
    print(i);  // 0, 1, 2, ..., 9
}

// Multiple variables
for (let i = 0, j = 10; i < j; i++, j--) {
    print(i + " " + tostring(j));
}

// Countdown
for (let i = 10; i > 0; i--) {
    print(i);
}

// Step by different amounts
for (let i = 0; i < 100; i += 10) {
    print(i);  // 0, 10, 20, ..., 90
}
```

### Empty Components

```cpp
// Empty initialization (variable declared outside)
let i = 0;
for (; i < 10; i++) {
    print(i);
}

// Empty condition (infinite loop - use break)
for (let i = 0; ; i++) {
    if (i >= 10) break;
    print(i);
}

// Empty increment (manual increment in body)
for (let i = 0; i < 10; ) {
    print(i);
    i++;
}

// All empty (infinite loop)
for (;;) {
    if (shouldExit()) break;
    doWork();
}
```

## For-In Loop

Iterate over tables using iterator functions:

```cpp
for (key, value in iterator_function(table)) {
    // loop body
}
```

### Using pairs()

Iterate over all key-value pairs in a table:

```cpp
let t = {
    ["name"] = "Alice",
    ["age"] = 30,
    ["city"] = "NYC"
};

for (key, value in pairs(t)) {
    print(key + " = " + tostring(value));
}
```

### Array Iteration

```cpp
let arr = {10, 20, 30, 40, 50};

for (index, value in pairs(arr)) {
    print("arr[" + tostring(index) + "] = " + tostring(value));
}
// Output:
// arr[0] = 10
// arr[1] = 20
// arr[2] = 30
// arr[3] = 40
// arr[4] = 50
```

### Single Variable Form

```cpp
// Iterate over keys only
for (key in pairs(t)) {
    print(key);
}
```

## Foreach Loop

Foreach is a simpler syntax for table iteration that automatically calls `pairs()`:

```cpp
foreach (key, value in table) {
    // loop body
}
```

This is equivalent to `for (key, value in pairs(table))` but more concise.

### Basic Foreach

```cpp
let arr = {10, 20, 30};
let sum = 0;

foreach (value in arr) {
    sum = sum + value;
}
print(sum);  // 60
```

### Foreach with Key and Value

```cpp
let t = {
    ["name"] = "Alice",
    ["age"] = 30,
    ["city"] = "NYC"
};

foreach (key, value in t) {
    print(key + " = " + tostring(value));
}
```

### Foreach with let

Variables can be declared inline with `let`:

```cpp
let arr = {10, 20, 30};

// Declare iteration variable
foreach (let v in arr) {
    print(v);
}

// Declare both variables
foreach (let k, v in arr) {
    print("arr[" + tostring(k) + "] = " + tostring(v));
}
```

### Foreach with Existing Variables

Or use existing variables:

```cpp
let k, v;

foreach (k, v in arr) {
    print(k + ": " + tostring(v));
}
// k and v remain accessible after loop
```

### Foreach vs For-In

Both work identically, but `foreach` is clearer for simple table iteration:

```cpp
// These are equivalent:
foreach (k, v in table) {
    process(k, v);
}

for (k, v in pairs(table)) {
    process(k, v);
}

// foreach is more concise
// for-in is needed for custom iterators
```

## Break Statement

Exit a loop immediately:

```cpp
for (let i = 0; i < 10; i++) {
    if (i == 5) {
        break;  // Exit loop when i is 5
    }
    print(i);  // Prints 0, 1, 2, 3, 4
}
```

### Examples

```cpp
// Search for an element
let arr = {10, 20, 30, 40, 50};
let target = 30;
let found = false;

for (i, v in pairs(arr)) {
    if (v == target) {
        found = true;
        break;
    }
}

if (found) {
    print("Found!");
}
```

## Continue Statement

Skip to the next iteration:

```cpp
for (let i = 0; i < 10; i++) {
    if (i % 2 == 0) {
        continue;  // Skip even numbers
    }
    print(i);  // Prints 1, 3, 5, 7, 9
}
```

### Examples

```cpp
// Process only valid items
let items = {1, -5, 10, -3, 20};

for (i, v in pairs(items)) {
    if (v < 0) {
        continue;  // Skip negative values
    }
    process(v);  // Only processes 1, 10, 20
}
```

## Nested Loops

Loops can be nested:

```cpp
for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
        print("i=" + tostring(i) + ", j=" + tostring(j));
    }
}
```

**Note**: `break` and `continue` only affect the innermost loop:

```cpp
for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
        if (j == 1) {
            break;  // Breaks inner loop only
        }
        print("i=" + tostring(i) + ", j=" + tostring(j));
    }
}
```

## Defer Statement

Execute code when leaving a scope:

```cpp
function processFile(filename) {
    let file = os.open(filename, "r");
    defer os.close(file);  // Executes at function end
    
    // Process file
    let content = file.read("*a");
    return content;
}  // File is closed here
```

See [Defer Statement](defer) for complete documentation.

## Best Practices
Use braces for clarity** - even for single-line bodies (braces are optional but recommended)
1. **Prefer for loops** for known iteration counts
2. **Use while loops** for condition-based iteration
3. **Use foreach or for-in** for table iteration (foreach is simpler)
4. **Always include braces** even for single-line bodies
5. **Avoid deep nesting** - extract nested loops into functions
6. **Use break/continue judiciously** - can make code harder to follow

```cpp
// Good: foreach for simple table iteration
foreach (k, v in table) {
    process(k, v);
}

// Good: Explicit control flow
for (let i = 0; i < 10; i++) {
    if (shouldSkip(i)) {
        continue;
    }
    process(i);
}

// Better (more explicit)
for (let i = 0; i < 10; i++) {
    if (!shouldSkip(i)) {
        process(i);
    }
}
```
