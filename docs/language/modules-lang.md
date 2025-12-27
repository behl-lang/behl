---
layout: default
title: Modules
parent: Language
nav_order: 9
---

# Modules
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Modules provide a way to organize and reuse code. Behl's module system allows you to import built-in modules and custom modules created from C++.

## Importing Modules

Use the `import()` function to load modules:

```cpp
const math = import("math");
print(math.PI);        // 3.14159...
print(math.sqrt(16));  // 4
```

### Module Assignment

The `import()` function returns a table containing the module's exports:

```cpp
const string = import("string");
let upper = string.upper;

print(upper("hello"));  // "HELLO"
```

### Import Once

Modules are cached - subsequent imports return the same module instance:

```cpp
const math1 = import("math");
const math2 = import("math");

print(math1 == math2);  // true (same table reference)
```

## Built-in Modules

Behl includes several built-in modules:

### Standard Library Modules

When using `load_stdlib(S, true)`, modules are globally accessible:

```cpp
// With global modules (load_stdlib second parameter = true)
print(math.PI);
print(string.upper("hello"));

// Without global modules (second parameter = false)
const math = import("math");
print(math.PI);
```

See [Standard Library](../standard-library) for complete module documentation.

### Module Loading

```cpp
// Core modules
const math = import("math");
const string = import("string");
const table = import("table");

// System modules (if available)
const os = import("os");
const io = import("io");
```

## Module Usage Patterns

### Selective Import

Import only what you need:

```cpp
const math = import("math");
let sqrt = math.sqrt;
let pow = math.pow;

print(sqrt(16));    // 4
print(pow(2, 10));  // 1024
```

### Module Aliasing

Give modules shorter names:

```cpp
const m = import("math");
const s = import("string");

print(m.PI);
print(s.upper("hello"));
```

### Conditional Import

Import modules only when needed:

```cpp
function useAdvancedMath() {
    const math = import("math");
    return math.sin(math.PI / 2);
}
```

## Creating Custom Modules (C++)

Custom modules are created from C++ and exposed to Behl scripts. See [Creating Modules](../embedding/modules) for complete documentation.

### Basic Module Structure (C++ side)

```cpp
// C++ module registration
void register_mymodule(behl::State* S) {
    behl::new_table(S);  // Create module table
    
    // Add functions
    behl::push_cfunction(S, my_function);
    behl::set_field(S, -2, "myFunction");
    
    // Add constants
    behl::push_integer(S, 42);
    behl::set_field(S, -2, "ANSWER");
    
    // Register module
    behl::register_module(S, "mymodule");
}
```

### Using Custom Modules (Behl side)

```cpp
const mymodule = import("mymodule");
print(mymodule.ANSWER);       // 42
mymodule.myFunction();
```

## Module Organization

### Namespace Pattern

Organize related functionality:

```cpp
// In C++: create a module with sub-tables
const graphics = import("graphics");

graphics["2d"].drawCircle(x, y, radius);
graphics["3d"].drawCube(x, y, z, size);
```

### Constants Module

Group related constants:

```cpp
const colors = import("colors");

print(colors.RED);     // 0xFF0000
print(colors.GREEN);   // 0x00FF00
print(colors.BLUE);    // 0x0000FF
```

### Utility Module

Collection of helper functions:

```cpp
const utils = import("utils");

utils.clamp(value, min, max);
utils.lerp(a, b, t);
utils.randomRange(min, max);
```

## Best Practices

1. **Import at module scope** - Import modules once at the top
2. **Use const for modules** - Modules shouldn't be reassigned
3. **Descriptive names** - Use clear module names
4. **Minimize global pollution** - Prefer importing over global access
5. **Document module interfaces** - Clearly specify module exports

```cpp
// Good: Clear imports at top
const math = import("math");
const string = import("string");

function processData(input) {
    let cleaned = string.trim(input);
    let result = math.sqrt(tonumber(cleaned));
    return result;
}

// Avoid: Import inside hot loops
function badExample() {
    for (let i = 0; i < 1000; i++) {
        const math = import("math");  // Don't do this!
        print(math.sqrt(i));
    }
}
```

## Module System Design

### File-Based Modules

Behl supports loading modules from `.behl` script files:

```cpp
// Load from file
const mymodule = import("mymodule");  // Loads mymodule.behl
const utils = import("./utils");      // Relative path
const db = import("database/postgres"); // Nested path
```

Modules are resolved from configured module paths and cached on first import.
See [Module System](../modules) for details on module paths and resolution.

### Module Table Structure

Modules are simply tables with exported values:

```cpp
const math = import("math");

// Explore module contents
for (name, value in pairs(math)) {
    print(name + ": " + typeof(value));
}
```

### Module Caching

Once imported, modules are cached in the registry. All importers receive the same table reference:

```cpp
const math1 = import("math");
math1["custom"] = 42;  // Add custom field

const math2 = import("math");
print(math2.custom);  // 42 (same table reference)
```

## Advanced Patterns

### Module Initialization

Modules can have initialization logic:

```cpp
const config = import("config");

function initialize() {
    config.setup({
        ["debug"] = true,
        ["logLevel"] = 3
    });
}
```

### Lazy Loading

Defer module imports until needed:

```cpp
let cachedModule = nil;

function getModule() {
    if (cachedModule == nil) {
        cachedModule = import("expensive_module");
    }
    return cachedModule;
}

// Only imports when first called
getModule().doSomething();
```

### Module Versioning

Add version information to modules (C++ side):

```cpp
// C++ module registration
behl::push_string(S, "1.2.3");
behl::set_field(S, -2, "version");
```

```cpp
// Behl usage
const mymodule = import("mymodule");
print("Module version: " + mymodule.version);
```

## Related Documentation

- [Standard Library](../standard-library) - Built-in module reference
- [Creating Modules](../embedding/modules) - Creating custom modules
- [Module System](../modules) - Detailed module system documentation
