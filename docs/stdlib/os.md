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
let t = os.hrtime();
```

---

## os.clock()

Returns monotonic wall-clock time in seconds, measured from a steady clock. Not affected by system clock changes, and not CPU time used by the program.

```cpp
let start = os.clock();

// ... do some work ...
for (let i = 0; i < 1000000; i++) {
    let x = i * 2;
}

let elapsed = os.clock() - start;
print("Elapsed: " + tostring(elapsed) + " seconds");
```

**Returns:** Monotonic time as a floating-point number of seconds

**Use Case:** Measuring performance and execution time

---

## os.hrtime()

Returns the current time as seconds since the epoch, using a high resolution clock.

```cpp
let timestamp = os.hrtime();
print(timestamp);

let start = os.hrtime();
// ... wait or do work ...
let elapsed = os.hrtime() - start;
print("Elapsed: " + tostring(elapsed) + " seconds");
```

**Returns:** Floating-point number of seconds since the epoch

**Use Case:** Getting current time, calculating elapsed durations

---

## os.dummy()

Always returns `1.0`.

```cpp
let x = os.dummy();
print(x);  // 1.0
```

**Returns:** The floating-point number `1.0`

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
```

---

## Notes

- `os.clock()` and `os.hrtime()` are both monotonic wall-clock times, not CPU time
- `os.clock()` is measured from an arbitrary starting point; use it for elapsed-time measurements like benchmarking
- `os.hrtime()` is measured from the epoch; use it when you need a timestamp as well as elapsed time
