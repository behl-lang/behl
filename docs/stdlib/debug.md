---
layout: default
title: debug
parent: Standard Library
nav_order: 5
---

# debug
{: .no_toc }

Debugging utilities.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The debug module provides utilities for debugging and introspection. It must be explicitly imported:

```cpp
const debug = import("debug");
print(debug.stacktrace());
```

---

## debug.stacktrace()

Returns a string containing the current call stack.

```cpp
function inner() {
    print(debug.stacktrace());
}
}

function middle() {
    inner();
}

function outer() {
    middle();
}

outer();
```

**Output:**
```
Stack trace:
  at inner (script.behl:2)
  at middle (script.behl:6)
  at outer (script.behl:10)
  at <main> (script.behl:13)
```

**Returns:** String with formatted stack trace

**Use Case:**
- Understanding call flow
- Debugging unexpected behavior
- Logging error contexts
- Profiling call patterns

---

## Example Usage

```cpp
const debug = import("debug");

// Custom error handler with stack trace
function safeExecute(func) {
    let success, result = pcall(func);
    
    if (!success) {
        print("Error occurred: " + result);
        print("\nCall stack:");
        print(debug.stacktrace());
        return nil;
    }
    
    return result;
}

function riskyOperation() {
    // This will error
    error("Something went wrong!");
}

safeExecute(riskyOperation);

// Debug logging with context
function debugLog(message) {
    let timestamp = os.time();
    print("[" + tostring(timestamp) + "] " + message);
    print("Call stack:");
    print(debug.stacktrace());
}

function processData(data) {
    if (data == nil) {
        debugLog("processData called with nil!");
        return;
    }
    // ... process data
}

// Trace function calls
function traced(func, name) {
    return function(...) {
        print("Entering: " + name);
        print(debug.stacktrace());
        
        let result = func(...);
        
        print("Exiting: " + name);
        return result;
    };
}

let tracedCalculate = traced(function(x, y) {
    return x + y;
}, "calculate");

let sum = tracedCalculate(5, 3);

// Assert with stack trace
function assert(condition, message) {
    if (!condition) {
        print("Assertion failed: " + message);
        print(debug.stacktrace());
        error("Assertion failure");
    }
}

function divide(a, b) {
    assert(b != 0, "Division by zero");
    return a / b;
}

// This will show stack trace
divide(10, 0);
```

---

## Notes

- Stack traces include function names, file paths, and line numbers
- Stack traces are captured at the point `stacktrace()` is called
- Useful in error handlers to provide context
- Can add overhead if called frequently in hot code paths
- Stack depth may be limited in some implementations
