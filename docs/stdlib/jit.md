---
layout: default
title: jit
parent: Standard Library
nav_order: 10
---

# jit
{: .no_toc }

JIT compiler control.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The jit module provides control over Behl's just-in-time compiler. It must be explicitly imported:

```cpp
const jit = import("jit");
print(jit.status());
```

Behl compiles functions to native machine code on their first call, with no configuration required. Functions that use features the JIT does not support automatically fall back to the interpreter. Supported architectures are x86-64, x86 (32-bit), and AArch64 on Windows, Linux, and macOS (including Apple Silicon).

---

## jit.status()

Returns whether the JIT is currently active.

```cpp
const jit = import("jit");

if (jit.status()) {
    print("JIT is enabled");
}
```

**Returns:** `true` when the platform supports JIT compilation and it is enabled, `false` otherwise

**Note:** On unsupported architectures this always returns `false`.

---

## jit.optimized(func)

Returns whether a function has been compiled to native code.

```cpp
const jit = import("jit");

function hot(n) {
    let s = 0;
    for (let i = 0; i < n; i++) {
        s = s + i;
    }
    return s;
}

print(tostring(jit.optimized(hot)));  // false - not called yet
hot(1000);
print(tostring(jit.optimized(hot)));  // true - compiled on first call
```

**Returns:** `true` if the function currently has compiled code, `false` otherwise

**Note:** Compilation happens lazily on the first call, so a function reports `false` until it has been invoked. Functions the JIT declined to compile keep reporting `false` and run in the interpreter. Passing anything other than a script function raises a type error.

---

## jit.on()

Enables the JIT compiler.

```cpp
const jit = import("jit");
jit.on();
```

Compilation resumes lazily: functions are compiled again the next time they are called.

---

## jit.off()

Disables the JIT compiler and clears the entire code cache.

```cpp
const jit = import("jit");
jit.off();  // All functions run in the interpreter from here on
```

All compiled code is dropped and its memory is released. Already-running compiled code finishes safely before the memory is reclaimed.

**Use Case:**
- Debugging with predictable interpreter-only execution
- Comparing interpreter and JIT performance
- Reclaiming code memory in long-running embedders

---

## Example Usage

```cpp
const jit = import("jit");

function work(n) {
    let sum = 0;
    for (let i = 0; i < n; i++) {
        sum = sum + i * i;
    }
    return sum;
}

print(jit.status());              // true on supported platforms
work(1);                          // triggers compilation
print(jit.optimized(work));       // true

jit.off();                        // interpreter-only, cache cleared
print(jit.optimized(work));       // false

jit.on();                         // re-enable
work(1);                          // recompiles
print(jit.optimized(work));       // true
```
