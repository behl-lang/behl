---
layout: default
title: gc
parent: Standard Library
nav_order: 6
---

# gc
{: .no_toc }

Garbage collector control.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The gc module provides control over Behl's incremental garbage collector. It must be explicitly imported:

```cpp
const gc = import("gc");
gc.collect();
```

---

## gc.collect()

Runs a full garbage collection cycle.

```cpp
// Force garbage collection
gc.collect();

// After creating many temporary objects
for (let i = 0; i < 10000; i++) {
    let temp = {data = i};
}
gc.collect();  // Clean up unreachable objects
```

**Use Case:**
- Testing memory behavior
- Cleaning up after creating many temporary objects
- Forcing collection before memory-sensitive operations

**Note:** The GC runs automatically, so manual collection is rarely needed in production code.

---

## gc.count()

Returns the current memory usage in kilobytes.

```cpp
let mem_kb = gc.count();
print("Memory: " + tostring(mem_kb) + " KB");

// Monitor memory growth
let before = gc.count();
createLargeStructure();
let after = gc.count();
print("Allocated: " + tostring(after - before) + " KB");
```

**Returns:** Memory usage as a floating-point number in KB

**Use Case:**
- Memory profiling
- Detecting memory leaks
- Monitoring memory usage trends

---

## Example Usage

```cpp
const gc = import("gc");

// Memory profiling function
function profileMemory(operation, name) {
    gc.collect();  // Start clean
    
    let before = gc.count();
    operation();
    let after = gc.count();
    
    let delta = after - before;
    print(name + " used: " + tostring(delta) + " KB");
}

// Profile different operations
profileMemory(function() {
    let arr = {};
    for (let i = 0; i < 10000; i++) {
        table.insert(arr, i);
    }
}, "Array creation");

profileMemory(function() {
    let big = {};
    for (let i = 0; i < 10000; i++) {
        big["key" + tostring(i)] = i * 2;
    }
}, "Large hash table");

// Memory leak detection
function checkForLeaks() {
    gc.collect();
    let baseline = gc.count();
    
    for (let i = 0; i < 100; i++) {
        // Operation that shouldn't leak
        let temp = {data = i};
    }
    
    gc.collect();
    let final = gc.count();
    
    if (final > baseline + 1) {  // Allow small variation
        print("Warning: Possible memory leak!");
        print("Baseline: " + tostring(baseline) + " KB");
        print("Final: " + tostring(final) + " KB");
    }
}

checkForLeaks();

// Periodic monitoring
function monitorMemory() {
    let iterations = 0;
    while (iterations < 10) {
        // Do work
        processData();
        
        // Check memory every N iterations
        if (iterations % 100 == 0) {
            let mem = gc.count();
            print("Iteration " + tostring(iterations) + ": " + tostring(mem) + " KB");
            
            if (mem > 10000) {  // 10 MB threshold
                print("High memory usage, forcing GC");
                gc.collect();
            }
        }
        
        iterations++;
    }
}
```

---

## Notes

- Behl uses an **incremental garbage collector** that runs automatically
- Manual collection with `gc.collect()` is usually unnecessary
- Use `gc.count()` for profiling and monitoring
- Memory is reported in kilobytes (KB), not bytes
- The collector is generational and typically very efficient
- In tight loops, excessive `gc.collect()` calls can hurt performance
