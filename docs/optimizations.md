---
layout: default
title: Optimizations
parent: Advanced Topics
nav_order: 3
---

# Optimizations
{: .no_toc }

Behl includes several compiler optimizations to improve runtime performance.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Behl performs optimizations at both the AST (Abstract Syntax Tree) and bytecode generation levels. These optimizations are applied automatically during compilation and require no special flags or configuration.

---

## Numeric For Loop Optimization

### Description

Simple numeric for loops are optimized to use specialized `FORPREP` and `FORLOOP` bytecode instructions instead of the generic loop constructs. This optimization significantly reduces the overhead of loop iteration.

### Conditions for Optimization

A for loop will be optimized if **all** of the following conditions are met:

1. **Variable declaration with `let`**: The loop variable must be declared with `let` (not `const`, not an existing variable)
2. **Simple comparison**: The condition must be a simple comparison (`<`, `<=`, `>`, `>=`)
3. **Loop variable as left operand**: The left side of the comparison must be the loop variable
4. **Increment/decrement pattern**: The update must be one of:
   - `i++` or `++i` (increment by 1)
   - `i--` or `--i` (decrement by 1)
   - `i += step` (increment by step)
   - `i -= step` (decrement by step)
5. **Direction matches**: The increment/decrement direction must match the comparison:
   - `i < end` or `i <= end` requires increment (`i++`, `i += step`)
   - `i > end` or `i >= end` requires decrement (`i--`, `i -= step`)

### Optimized Examples

```cpp
// Basic increment loop
for (let i = 0; i < 10; i++) {
    print(i);
}

// Inclusive range
for (let i = 0; i <= 10; i++) {
    print(i);
}

// Decrement loop
for (let i = 10; i > 0; i--) {
    print(i);
}

// Custom step
for (let i = 0; i < 100; i += 5) {
    print(i);
}

// Descending with step
for (let i = 100; i >= 0; i -= 10) {
    print(i);
}
```

### Non-Optimized Cases

```cpp
// Using existing variable (not let)
let i = 0;
for (i = 0; i < 10; i++) {  // NOT optimized
    print(i);
}

// Using const (cannot be incremented)
for (const i = 0; i < 10; i++) {  // Compile error
    print(i);
}

// Complex condition
function check(x) { return x < 10; }
for (let i = 0; check(i); i++) {  // NOT optimized
    print(i);
}

// Mismatched direction
for (let i = 0; i < 10; i--) {  // NOT optimized (infinite loop)
    print(i);
}

// Multiple variables
for (let i = 0, j = 10; i < j; i++, j--) {  // NOT optimized
    print(i + j);
}
```

### Performance Impact

Optimized numeric for loops are approximately **2-3x faster** than generic for loops because:
- Loop iteration uses specialized bytecode instructions
- Condition checking is streamlined
- No need for generic expression evaluation on each iteration

---

## Tail Call Optimization

### Description

Functions that end with a call to another function (tail position) are optimized to reuse the current stack frame instead of creating a new one. This prevents stack overflow in recursive functions and improves performance.

### What is a Tail Call?

A tail call is a function call that appears as the last operation before a return:

```cpp
// Tail call - last thing the function does
function countdown(n) {
    if (n <= 0) {
        return 0;
    }
    return countdown(n - 1);  // Tail call
}

// NOT a tail call - result is used after the call
function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);  // NOT tail call (multiplication after)
}
```

### Automatic Detection

Behl automatically detects and optimizes tail calls when:
1. A function call is the last statement before a return
2. The return value is directly returned without modification
3. No operations need to be performed after the call returns

### Optimized Examples

```cpp
// Tail-recursive countdown
function countdown(n) {
    if (n <= 0) {
        return 0;
    }
    return countdown(n - 1);  // Optimized
}

// Tail-recursive sum with accumulator
function sum_helper(n, acc) {
    if (n <= 0) {
        return acc;
    }
    return sum_helper(n - 1, acc + n);  // Optimized
}

// Mutual recursion
function is_even(n) {
    if (n == 0) {
        return true;
    }
    return is_odd(n - 1);  // Optimized
}

function is_odd(n) {
    if (n == 0) {
        return false;
    }
    return is_even(n - 1);  // Optimized
}
```

### Non-Optimized Cases

```cpp
// Operation after call
function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);  // NOT tail call
}

// Assignment then return
function bad_tail(n) {
    if (n <= 0) {
        return 0;
    }
    let result = bad_tail(n - 1);  // NOT tail call
    return result;
}

// Multiple return values
function multi(n) {
    if (n <= 0) {
        return 0, 0;
    }
    return multi(n - 1);  // [NOT OK] Complex return handling
}
```

### Performance Impact

Tail call optimization:
- **Prevents stack overflow** in deep recursion (can recurse thousands of times)
- **Reduces memory usage** by reusing stack frames
- **Improves cache locality** by keeping the working set small

Without tail call optimization, the `countdown(10000)` example would cause a stack overflow. With optimization, it runs with constant stack space.

---

## Constant Folding

### Description

Constant expressions are evaluated at compile time rather than at runtime. This eliminates unnecessary computations and reduces bytecode size.

### What Gets Folded

Behl automatically folds:

1. **Arithmetic operations** on constant integers and floats:
   - Addition, subtraction, multiplication, division, modulo
   - Power operator (`**`)
   
2. **Bitwise operations** on constant integers:
   - AND (`&`), OR (`|`), XOR (`^`)
   - Left shift (`<<`), right shift (`>>`)

3. **String concatenation** with constant strings

4. **Unary operations** on constants:
   - Unary minus (`-x`)
   - Bitwise NOT (`~x`)

### Examples

```cpp
// Arithmetic folding
let x = 10 + 32;              // Folded to: let x = 42;
let y = 5 * 6 + 2;            // Folded to: let y = 32;
let z = 2 ** 10;              // Folded to: let z = 1024;

// Bitwise folding
let mask = 0xFF & 0x0F;       // Folded to: let mask = 15;
let shifted = 1 << 8;         // Folded to: let shifted = 256;

// String concatenation
let msg = "Hello" + " " + "World";  // Folded to: let msg = "Hello World";

// Unary operations
let neg = -(10 + 5);          // Folded to: let neg = -15;

// Nested expressions
let complex = (2 + 3) * (4 - 1);  // Folded to: let complex = 15;
```

### Type Promotion

Constant folding respects type promotion rules:

```cpp
let a = 10 + 20;      // Integer folding: 30
let b = 10 + 20.0;    // Promotes to float: 30.0
let c = 10 / 5;       // Integer inputs, but / returns float: 2.0
```

### What Doesn't Get Folded

```cpp
// Variable expressions (runtime values)
let x = 10;
let y = x + 5;        // NOT folded (x is a variable)

// Function calls
let z = abs(-5);      // NOT folded (function call needed)

// Division by zero
let bad = 10 / 0;     // NOT folded (would be an error)
```

### Performance Impact

Constant folding provides:
- **Reduced bytecode size** - fewer instructions to execute
- **Faster execution** - no runtime computation needed
- **No overhead** - performed once at compile time

---

## Dead Store Elimination

### Description

Dead store elimination removes assignments to variables that are never read before being overwritten or going out of scope. This reduces unnecessary computation and bytecode size.

### What Gets Eliminated

Behl removes:

1. **Unused local variable initializations**:
   - Variables declared but never read
   - Variables that are reassigned before first use

2. **Redundant assignments**:
   - Assignments overwritten before the value is used
   - Assignments at the end of a scope that are never read

3. **Write-only variables**:
   - Variables that are written to but never consumed

### Safety Constraints

Dead store elimination is **conservative** and only removes stores when it's provably safe:

- **Preserves side effects**: Expressions with function calls or operations that may have side effects are never eliminated
- **Respects data dependencies**: If the new value depends on the old value, the old store is preserved
- **Nested scope analysis**: Properly handles stores that may be read in nested blocks

### Examples

```cpp
// Redundant initialization
function example1() {
    let x = 10;      // Dead store - never read
    x = 20;          // This is the first real use
    return x;
}
// Optimized to:
// let x;
// x = 20;
// return x;

// Unused variable
function example2() {
    let temp = compute();  // Dead store if compute() has no side effects
    return 42;
}

// Overwritten before use
function example3() {
    let result = 0;   // Dead store
    result = sum();   // First actual use
    return result;
}

// Sequential assignments
function example4(arr) {
    let i = 0;        // Dead store
    i = 1;            // Dead store
    i = 2;            // Kept - this is used
    return arr[i];
}
```

### Preserved Cases

```cpp
// Function call with side effects - PRESERVED
function has_side_effects() {
    let x = print("Important!");  // Kept - print() has side effects
    x = 10;
    return x;
}

// Value depends on old value - PRESERVED
function depends_on_old() {
    let x = 10;     // Kept
    x = x + 5;      // Needs the previous value
    return x;
}

// May be read in nested scope - PRESERVED
function nested() {
    let x = 10;     // Kept - might be read in if-block
    if (condition) {
        return x;
    }
    x = 20;
    return x;
}
```

### Performance Impact

Dead store elimination provides:
- **Reduced bytecode size** - fewer assignments to compile
- **Faster execution** - skips unnecessary stores
- **Better register allocation** - compiler has more freedom
- **Improved cache usage** - less memory traffic

### Limitations

The current implementation uses a **linear, single-pass analysis** within each block. This means:

- Only eliminates stores within the same basic block
- Conservative across control flow boundaries (if/while/for)
- Does not perform inter-procedural analysis

Future improvements may include:
- Control flow analysis across branches
- Global dead store elimination
- More aggressive side-effect analysis

---

## Future Optimizations

The following optimizations are planned for future releases:

### Dead Code Elimination

Remove unreachable code after returns or unconditional jumps:

```cpp
function foo() {
    return 5;
    print("Never executed");  // Could be removed
}
```

### Inline Small Functions

Inline function calls to eliminate call overhead:

```cpp
function add(a, b) { return a + b; }
let x = add(3, 4);  // Could inline to: let x = 3 + 4;
```

### Peephole Optimizations

Replace common instruction sequences with more efficient equivalents at the bytecode level.

---

## Disabling Optimizations

### From the C++ API

When using the C++ embedding API, you can disable all AST-level optimizations by passing `false` as the `optimize` parameter:

```cpp
// Load with optimizations (default)
behl::load_string(S, code);
behl::load_string(S, code, true);

// Load without optimizations
behl::load_string(S, code, false);
behl::load_buffer(S, code, "script.behl", false);
```

When optimizations are disabled:
- Constant folding is **not** performed
- Loop optimization is **not** performed
- Dead store elimination is **not** performed
- Tail call optimization **still runs** (bytecode-level optimization)

### Use Cases for Disabling

You might want to disable optimizations when:
- **Debugging**: To match source code more closely in bytecode dumps
- **Testing**: To verify optimization correctness by comparing optimized vs unoptimized behavior
- **Development**: When working on the compiler itself
- **Diagnostics**: To isolate whether an issue is caused by an optimization pass

### From Scripts

There is currently no way to disable optimizations from within Behl scripts themselves. The optimization setting is controlled at the compilation level when calling `load_string` or `load_buffer`.

### Important Notes

- Disabling optimizations does not affect correctness - only performance
- Individual optimization passes cannot be selectively disabled (all-or-nothing)
- Bytecode-level optimizations (like tail calls) run regardless of the `optimize` flag

---

## Implementation Details

### Optimization Passes

Optimizations are applied in multiple passes:

1. **AST-level optimizations** (`src/optimization/ast/`)
   - Loop optimization
   - Constant folding
   - Dead store elimination
   - Applied after parsing, before semantic analysis

2. **Bytecode-level optimizations** (`src/backend/compiler.cpp`)
   - Tail call detection and transformation
   - Applied during bytecode generation

### Testing

All optimizations have comprehensive test coverage in `tests/optimizations_tests.cpp` and related test files. The tests verify:
- Correct bytecode generation for optimized cases
- Proper handling of edge cases
- Equivalent behavior between optimized and non-optimized code

---

## Best Practices

### Writing Optimization-Friendly Code

1. **Use simple numeric for loops** when possible:
   ```cpp
   // Good
   for (let i = 0; i < n; i++) { }
   
   // Avoid if possible
   let i = 0;
   for (i = 0; i < n; i++) { }
   ```

2. **Structure recursive functions for tail calls**:
   ```cpp
   // Use accumulator pattern
   function sum(n, acc) {
       if (n <= 0) return acc;
       return sum(n - 1, acc + n);  // Tail call
   }
   
   // Instead of
   function sum(n) {
       if (n <= 0) return 0;
       return n + sum(n - 1);  // Not a tail call
   }
   ```

3. **Keep loop conditions simple**:
   ```cpp
   // Good - direct comparison
   for (let i = 0; i < array_length; i++) { }
   
   // Avoid - function call in condition
   for (let i = 0; i < get_length(); i++) { }
   ```

4. **Use constant expressions** where possible:
   ```cpp
   // Good - constants are folded
   const BUFFER_SIZE = 1024 * 8;
   const MAX_ITEMS = 100 + 50;
   
   // Less efficient - computed at runtime
   let size = compute_size();
   ```

### Measuring Performance

When optimizing code:
1. **Profile first** - Identify bottlenecks before optimizing
2. **Measure impact** - Compare before/after performance
3. **Verify correctness** - Ensure optimizations don't change behavior
