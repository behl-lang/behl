---
layout: default
title: Defer Statement
parent: Language
nav_order: 8
---

# Defer Statement

Behl supports Go-like `defer` statements that execute code when leaving a scope. Deferred statements are executed in LIFO (last-in-first-out) order, making them ideal for cleanup operations.

## Basic Syntax

```javascript
function example() {
    defer print("cleanup");
    print("body");
}
// Output: body, cleanup
```

## Execution Order

Multiple defer statements execute in reverse order (LIFO):

```javascript
function test() {
    defer print("third");
    defer print("second");
    defer print("first");
    print("body");
}
// Output: body, first, second, third
```

## Defer with Blocks

Defer can execute a block of statements:

```javascript
function test() {
    let x = 42;
    defer {
        print("Cleaning up...");
        print("x was: " + tostring(x));
    };
    x = 100;
}
// Output: Cleaning up..., x was: 100
```

## Variable Access

Deferred statements execute at scope exit and see the final values of variables. Conceptually, the deferred code block is moved to just before the scope exits:

```javascript
function test() {
    let counter = 0;
    defer print("Final count: " + tostring(counter));
    
    counter = counter + 1;
    counter = counter + 10;
}
// Output: Final count: 11

// Equivalent to:
function test_equivalent() {
    let counter = 0;
    // ... deferred statement moved here ...
    counter = counter + 1;
    counter = counter + 10;
    print("Final count: " + tostring(counter));  // Executes at scope exit
}
```

## Early Returns

Defers execute even when returning early:

```javascript
function validate(x) {
    defer print("Validation complete");
    
    if (x < 0) {
        print("Invalid input");
        return false;
    }
    
    print("Valid input");
    return true;
}

validate(-5);
// Output: Invalid input, Validation complete
```

The return expression is evaluated first, then the defers run, then the function returns. A defer therefore cannot change the value that was already computed, but it does observe the state left behind by the return expression:

```javascript
function test() {
    let x = 1;
    defer x = 99;
    return x;
}
// Returns 1, not 99
```

Multiple return values, including forwarded calls like `return f();` and `return ...;`, are preserved across the defers.

## Break and Continue

`break` and `continue` also leave the scope, so the defers above the loop's scope run before the jump:

```javascript
function test() {
    for (let i = 0; i < 3; i++) {
        defer print("defer-" + tostring(i));
        if (i == 1) {
            break;
        }
        print("loop-" + tostring(i));
    }
}
// Output: loop-0, defer-0, defer-1
```

## Errors and Unwinding

Defers run while an error unwinds, in the frame that raised and in every frame above it:

```javascript
function inner() {
    defer print("inner cleanup");
    error("boom");
}

function outer() {
    defer print("outer cleanup");
    inner();
}

let ok, err = pcall(outer);
// Output: inner cleanup, outer cleanup
// ok is false, err is "boom"
```

Defer does not catch the error, it only runs on the way out. Use `pcall` to handle it.

If a defer itself raises, its error replaces the one that was propagating, matching Go. The frame's remaining defers still run:

```javascript
function test() {
    defer print("still runs");
    defer error("from defer");
    error("original");
}

let ok, err = pcall(test);
// Output: still runs
// err is "from defer"
```

## Restrictions

`break` and `continue` are not allowed inside a defer body. The loop they would target has already finished compiling by the time the body is emitted, so this is a compile error:

```javascript
while (i < 3) {
    defer { break; }  // Error: break statement outside of loop
    i = i + 1;
}
```

A single function may contain at most 32 `defer` statements. Exceeding that is a compile error, `too many defer statements in one function`. The limit is on `defer` statements written in the function body, not on how many times they execute, so a defer inside a loop counts once.

## Nested Scopes

Defers are scope-aware and execute at their enclosing scope's end:

```javascript
function test() {
    defer print("function-end");
    
    {
        defer print("block-end");
        print("inside-block");
    }  // block-end executes here
    
    print("after-block");
}  // function-end executes here

// Output: inside-block, block-end, after-block, function-end
```

## Loop Defers

Defer inside loops executes on every iteration:

```javascript
function test() {
    for (let i = 0; i < 3; i++) {
        defer print("defer-" + tostring(i));
        print("loop-" + tostring(i));
    }
}
// Output:
// loop-0, defer-0
// loop-1, defer-1
// loop-2, defer-2
```

## Common Use Cases

### Resource Cleanup

```javascript
function processFile(filename) {
    let file = fs.open(filename, "r");
    defer file:close();
    
    // Process file
    // File is closed on the normal return and while an error unwinds
    let contents, bytes = file:read(1024);
    return contents;
}
```

### Lock Management

```javascript
function criticalSection() {
    acquireLock();
    defer releaseLock();
    
    // Critical section code
    // Lock is always released
}
```

### Timing Operations

```javascript
function measureTime() {
    let start = os.clock();
    defer {
        let elapsed = os.clock() - start;
        print("Elapsed: " + tostring(elapsed));
    };
    
    // Timed operation
}
```

## Important Notes

1. **LIFO Order**: Defers execute in reverse order of declaration
2. **Final Values**: Variables have their final values when defer executes
3. **Scope-Based**: Executes at scope end (function or block), including on `break` and `continue`
4. **Return Safety**: Runs after the return expression is evaluated and before the function returns, leaving the returned values untouched
5. **Runs While Unwinding**: Defers still run when an error propagates, in the raising frame and every frame above it
6. **No Error Recovery**: Defer does NOT catch errors - use `pcall` for error handling. A defer that raises replaces the propagating error
7. **Restrictions**: No `break` or `continue` inside a defer body, and at most 32 `defer` statements per function
8. **Performance**: Minimal overhead - a defer that never executes costs nothing at runtime

## Defer vs Try-Finally

Behl does not have try-finally blocks. Use `defer` with `pcall` for similar functionality:

```javascript
function safeOperation() {
    let resource = allocateResource();
    defer freeResource(resource);
    
    let success, result = pcall(function() {
        // Risky operation
        return processResource(resource);
    });
    
    if (!success) {
        print("Error: " + result);
        return null;
    }
    
    return result;
}
```

## Comparison with Other Languages

| Language | Syntax | Execution | Notes |
|----------|--------|-----------|-------|
| Behl | `defer stmt;` | Scope end, LIFO | Executes at block/function end |
| Zig | `defer stmt;` | Scope end, LIFO | Nearly identical to Behl |
| Swift | `defer { }` | Scope end, LIFO | Same scope semantics |
| C 2Y (TS) | `defer { }` | Scope end, LIFO | Proposed C standard |
| Go | `defer stmt()` | **Function end**, LIFO | Accumulates all defers until function exit |

**Key difference from Go**: Behl's defer executes at scope exit (like Zig/Swift/C), not function exit. In a loop, Go accumulates all defers and executes them when the function returns, which can cause resource exhaustion. Behl executes each iteration's defer when that iteration's scope ends:

```javascript
// Behl/Zig/Swift behavior:
for (let i = 0; i < 3; i++) {
    let file = open("file" + tostring(i));
    defer close(file);  // Closes at end of THIS iteration
}

// Go behavior:
// for i := 0; i < 3; i++ {
//     file := open("file" + i)
//     defer close(file)  // All 3 files stay open until function returns!
// }
```

Behl's defer is semantically closest to Zig, Swift, and the proposed C 2Y defer.
