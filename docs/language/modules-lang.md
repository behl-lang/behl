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

Modules provide a way to organize and reuse code. Behl's module system allows you to import built-in modules, script modules written in Behl, and custom modules created from C++.

## Importing Modules

Use the `import()` function to load modules:

```cpp
const math = import("math");
print(math.pi);        // 3.14159...
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

Behl includes several built-in modules that must be explicitly imported:

### Standard Library Modules

All modules require explicit import:

```cpp
// Import modules explicitly
const math = import("math");
const string = import("string");

print(math.pi);
print(string.upper("hello"));
```

See [Standard Library](../standard-library) for complete module documentation.

### Module Loading

The registered modules are `math`, `string`, `table`, `os`, `gc`, `jit` and `debug`, plus the security-sensitive `fs` and `process` modules, which the host has to opt into. There is no `io` module.

```cpp
// Core modules
const math = import("math");
const string = import("string");
const table = import("table");

// System modules
const os = import("os");

// Opt-in modules (only if the host enabled them)
const fs = import("fs");
const process = import("process");
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
    return math.sin(math.pi / 2);
}
```

## Creating Script Modules

A `.behl` file becomes a module by starting with the `module;` declaration and marking its public names with `export`. `module;` must be the very first statement in the file.

```cpp
// mathutils.behl
module;

const PI2 = 6.28318;

let counter = 0;              // Private, not exported

let function helper(x) {      // Private, not exported
    return x * 2;
}

export const VERSION = "1.0";

export function double(x) {
    return helper(x);
}
```

Importing the file returns a table of exactly the exported names:

```cpp
const mathutils = import("./mathutils");

print(mathutils.VERSION);    // "1.0"
print(mathutils.double(21)); // 42
print(mathutils.counter);    // nil (private)
print(mathutils.helper);     // nil (private)
```

### Export Forms

| Form | Notes |
|------|-------|
| `export function name() {}` | Exports a function |
| `export const NAME = value` | Exports a constant |
| `export { a, b, c }` | Exports names declared earlier in the file |

`export let` is rejected at parse time. Mutable exports are not allowed, use `const` or getter/setter functions.

```cpp
module;

const A = 1;
const B = 2;

export { A, B };
```

### Module Files Cannot Use Globals

Inside a `module;` file, the global namespace is off limits and this is enforced at compile time:

- Reading an undeclared name is a compile error, even if a global with that name exists.
- A bare assignment to an undeclared name is a compile error, use `let` or `const`.
- Only locals, upvalues and the builtin whitelist (`print`, `typeof`, `typeid`, `getmetatable`, `setmetatable`, `rawlen`, `pairs`, `import`, `error`, `pcall`, `tostring`, `tonumber`) are visible.
- A top-level `function name() {}` in a module file is therefore compiled as a local, not a global.

Everything else has to come in through `import()`:

```cpp
module;

const math = import("math");   // Correct

export function hyp(a, b) {
    return math.sqrt(a ** 2 + b ** 2);
}
```

## Creating Custom Modules (C++)

Custom modules are created from C++ and exposed to Behl scripts. See [Creating Modules](../embedding/modules) for complete documentation.

### Basic Module Structure (C++ side)

```cpp
// C++ module registration
static constexpr behl::ModuleReg mymodule_funcs[] = {
    { "myFunction", my_function },
};

static constexpr behl::ModuleConst mymodule_consts[] = {
    { "ANSWER", static_cast<behl::Integer>(42) },
};

void register_mymodule(behl::State* S) {
    behl::ModuleDef def = { .funcs = mymodule_funcs, .consts = mymodule_consts };
    behl::create_module(S, "mymodule", def);
}
```

The table can also be built by hand with `behl::table_new`, `behl::push_cfunction` and `behl::table_setfield`:

```cpp
behl::table_new(S);
behl::push_cfunction(S, my_function);
behl::table_setfield(S, -2, "myFunction");
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

Names are resolved in this order, and the result is cached on first import:

1. The directory of the importing file.
2. A `modules/` subdirectory of the importing file's directory.
3. The module search paths configured by the host, relative to the working directory.

A name starting with `./` or `../` is resolved **only** against the importing file's directory, the search steps above are skipped entirely. The `.behl` extension is appended when the name does not already end with it.

See [Module System](../modules) for details on module paths and resolution.

### Importing Plain Scripts

`import()` runs the file and hands back whatever the chunk returns:

- A file with `module;` and `export` returns its exports table.
- A file without `module;` returns whatever its top-level `return` produces.
- A file that is neither a module nor returns a value imports as `nil`.

```cpp
// plain.behl - no module declaration, no return
print("side effect");
```

```cpp
const plain = import("./plain");  // Prints "side effect"
print(plain);                     // nil
```

A plain script that wants to be importable can just return a table:

```cpp
// legacy.behl
let exports = {};
exports.greet = function(name) { return "Hi " + name; };
return exports;
```

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
behl::table_setfield(S, -2, "version");
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
