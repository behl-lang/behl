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

## Variable Capture

Deferred statements capture variables by reference and see all mutations:

```javascript
function test() {
    let counter = 0;
    defer print("Final count: " + tostring(counter));
    
    counter = counter + 1;
    counter = counter + 10;
}
// Output: Final count: 11
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
    let file = os.open(filename, "r");
    defer os.close(file);
    
    // Process file
    // File is automatically closed even if error occurs
    return file.read("*a");
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
2. **Reference Capture**: Variables are captured by reference, not by value
3. **Scope-Based**: Executes at scope end (function or block)
4. **Return Safety**: Runs before return statements
5. **No Error Recovery**: Defer does NOT catch errors - use `pcall` for error handling
6. **Performance**: Minimal overhead - suitable for performance-critical code

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

| Language | Syntax | Execution |
|----------|--------|-----------|
| Behl | `defer stmt;` | Scope end, LIFO |
| Go | `defer stmt()` | Function end, LIFO |
| Swift | `defer { }` | Scope end, LIFO |
| Zig | `defer stmt;` | Scope end, LIFO |

Behl's defer is closest to Go's implementation but with block-level scope support similar to Swift.
