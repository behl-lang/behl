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

Returns the number of live GC-managed objects.

```cpp
let count = gc.count();
print("Live objects: " + tostring(count));

// Monitor object count growth
let before = gc.count();
createLargeStructure();
let after = gc.count();
print("Objects allocated: " + tostring(after - before));
```

**Returns:** Live object count as an integer

**Use Case:**
- Object count profiling
- Detecting object leaks
- Monitoring allocation trends

---

## gc.step()

Runs a single incremental GC step.

```cpp
gc.step();
```

**Use Case:** Advancing the incremental collector manually instead of letting it run automatically

---

## gc.countall()

Returns the total number of objects tracked by the GC, including those pending finalization or not yet swept.

```cpp
let count = gc.countall();
print("Tracked objects: " + tostring(count));
```

**Returns:** Object count as an integer

---

## gc.countfree()

Returns the number of free objects.

```cpp
let count = gc.countfree();
print("Free objects: " + tostring(count));
```

**Returns:** Object count as an integer

**Note:** Currently always returns `0`.

---

## gc.threshold()

Returns the current GC threshold, the object count at which the next collection cycle is triggered.

```cpp
let threshold = gc.threshold();
print("Threshold: " + tostring(threshold));
```

**Returns:** Threshold as an integer

---

## gc.setthreshold(n)

Sets the GC threshold.

**Parameters:**
- `n` - New threshold value. Ignored if not greater than `0`.

```cpp
gc.setthreshold(50000);
```

---

## gc.phase()

Returns the current phase of the incremental garbage collector.

```cpp
let phase = gc.phase();
print("GC phase: " + phase);
```

**Returns:** One of `"idle"`, `"mark"`, `"sweep"`, `"finalize"`

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
    print(name + " used: " + tostring(delta) + " objects");
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
        print("Warning: Possible object leak!");
        print("Baseline: " + tostring(baseline) + " objects");
        print("Final: " + tostring(final) + " objects");
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
            print("Iteration " + tostring(iterations) + ": " + tostring(mem) + " objects");
            
            if (mem > 10000) {  // object count threshold
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
- `gc.count()` reports the number of live GC objects, not memory in bytes or kilobytes
- The collector is generational and typically very efficient
- In tight loops, excessive `gc.collect()` calls can hurt performance
