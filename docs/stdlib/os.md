---
layout: default
title: os
parent: Standard Library
nav_order: 7
---

# os
{: .no_toc }

Operating system interface.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The os module provides access to operating system functions. It must be explicitly imported:

```cpp
const os = import("os");
let t = os.time();
```

---

## os.clock()

Returns CPU time used by the program in seconds.

```cpp
let start = os.clock();

// ... do some work ...
for (let i = 0; i < 1000000; i++) {
    let x = i * 2;
}

let elapsed = os.clock() - start;
print("Elapsed: " + tostring(elapsed) + " seconds");
```

**Returns:** CPU time as a floating-point number

**Use Case:** Measuring performance and execution time

---

## os.time()

Returns current time as a Unix timestamp (seconds since epoch).

```cpp
let timestamp = os.time();
print(timestamp);  // e.g., 1672531200

// Calculate time difference
let start = os.time();
// ... wait or do work ...
let elapsed = os.time() - start;
print("Real time elapsed: " + tostring(elapsed) + " seconds");
```

**Returns:** Integer timestamp

**Use Case:** Getting current time, calculating real-time durations

---

## os.exit(code)

Exits the program with the given status code.

```cpp
// Success
os.exit(0);

// Error
os.exit(1);

// Custom error code
os.exit(42);
```

**Parameters:**
- `code` - Exit status code (0 = success, non-zero = error)

**Note:** This function does not return - the program terminates immediately

---

## Example Usage

```cpp
const os = import("os");

// Benchmark a function
function benchmark(func) {
    let start = os.clock();
    func();
    let elapsed = os.clock() - start;
    return elapsed;
}

function slowOperation() {
    let sum = 0;
    for (let i = 0; i < 1000000; i++) {
        sum = sum + i;
    }
    return sum;
}

let time = benchmark(slowOperation);
print("Operation took: " + tostring(time) + " seconds");

// Log with timestamp
let timestamp = os.time();
print("[" + tostring(timestamp) + "] Application started");

// Conditional exit
function validateConfig(config) {
    if (config == nil) {
        print("Error: Config file not found");
        os.exit(1);
    }
}

let config = loadConfig();
validateConfig(config);
// ... continue if validation passed
```

---

## Notes

- `os.clock()` measures CPU time (time spent executing)
- `os.time()` measures real wall-clock time
- For accurate benchmarking, use `os.clock()`
- For timestamps and real-time measurements, use `os.time()`
