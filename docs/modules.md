---
layout: default
title: Module System
nav_order: 8
---

# Module System
{: .no_toc }

Behl's module system enables code organization and reusability through file-based modules with explicit exports and imports.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Behl uses a file-based module system similar to Node.js/ES6 modules. Each `.behl` file can be either:
- **Script mode** (default) - Normal script with access to global scope
- **Module mode** - Isolated scope with explicit exports, declared with `module;` at the top

## The `import()` Function

### Basic Usage

```cpp
// Import a module
const math = import("math");
print(math.pi);

// Import with relative path
const utils = import("./utils");

// Import from subdirectory
const db = import("database/postgres");
```

### Module Caching

Modules are executed once and cached. Subsequent imports return the same cached table:

```cpp
const logger1 = import("utils/logger");
logger1.log("Hello");

// Returns the same instance - state is preserved
const logger2 = import("utils/logger");
print(logger2.getLogCount()); // Includes the "Hello" log
```

**Key Points:**
- Modules execute only on first import
- State persists across all importers
- Cache key is the resolved absolute path

## Module Path Resolution

The `import()` function resolves module paths using a specific search strategy:

### 1. Relative Imports

Paths starting with `./` or `../` are resolved relative to the importing file:

```cpp
// In /project/src/main.behl
const helper = import("./helper");      // → /project/src/helper.behl
const shared = import("../shared");     // → /project/shared.behl
```

### 2. Same Directory

Non-relative imports first search in the importing file's directory:

```cpp
// In /project/src/main.behl
const config = import("config");        // → /project/src/config.behl
```

### 3. Modules Subdirectory

Next, searches in a `modules/` subdirectory relative to the importing file:

```cpp
// In /project/src/main.behl
const utils = import("utils/logger");   // → /project/src/modules/utils/logger.behl
```

### 4. Current Working Directory

Finally, searches relative to the current working directory where the script was executed:

```cpp
// If running from /project/
const math = import("math");            // → /project/modules/math.behl
                                        //    or /project/math.behl
```

**Note for Embedders:** Additional search paths can be configured via the C++ API by modifying `State.module_paths`.

### File Extension

The `.behl` extension is automatically added if not present:

```cpp
const math = import("math");            // → math.behl
const config = import("config.behl");   // → config.behl (no change)
```

## Module Mode

### Declaring a Module

Add `module;` as the first statement to enable module mode:

```cpp
module;

// Module code here
```

**Effects of Module Mode:**
- **No global scope access** - Cannot read/write global variables
- **Local by default** - Functions and variables are private unless exported
- **Must export explicitly** - Use `export` keyword (returns are automatically added)
- **Must import ALL modules** - Standard library modules (math, string, table, os, gc, debug) must be imported with `import("math")`, regardless of `make_global` setting
- **Only builtins accessible** - Core functions like `print`, `typeof`, `tostring`, `tonumber`, `import`, `error`, `pcall`, etc. are available without import

### Script Mode vs Module Mode

**Script Mode** (default):
```cpp
// Can access and modify globals
let x = 10;
globalVar = 20;

function helper() {
    return 42;
}
// helper is accessible globally
```

**Module Mode**:
```cpp
module;

// Must import - no access to global math
const math = import("math");

// Private - not accessible outside module
let count = 0;

function increment() {
    count++;
}

// Must export to make available
export function getCount() {
    return count;
}
```

## Exporting from Modules

Behl supports three export patterns:

### 1. Manual Export Table (Explicit)

Return a table containing exported values:

```cpp
module;

let privateVar = 42;

function publicFunc() {
    return privateVar * 2;
}

// Build and return export table
let exports = {};
exports.publicFunc = publicFunc;
return exports;
```

**Usage:**
```cpp
const mymod = import("mymod");
print(mymod.publicFunc()); // 84
```

### 2. Export Keyword (Recommended)

```cpp
module;

export const PI = 3.14159;

export function add(a, b) {
    return a + b;
}

// Private function
function helper() {
    return 42;
}
```

**Important:** Exported items are automatically collected and returned in an `__EXPORTS__` table. You don't need to manually return anything:

```cpp
const mymod = import("mymod");
print(mymod.PI);        // 3.14159
print(mymod.add(2, 3)); // 5
```

**Export Rules:**
- Can only export `const` declarations, not `let` (mutable variables cannot be exported)
- Can export functions
- Can export multiple items in one statement: `export { PI, add, multiply }`

### 3. Script Mode Return

Even without `module;`, you can return a table to create an exportable interface:

```cpp
// No "module;" - script mode

let counter = 0;

function increment() {
    counter++;
}

function get() {
    return counter;
}

// Return exports
return {
    increment = increment,
    get = get
};
```

## Module Examples

### Simple Module

**modules/math.behl:**
```cpp
module;

function add(a, b) {
    return a + b;
}

function multiply(a, b) {
    return a * b;
}

let exports = {};
exports.add = add;
exports.multiply = multiply;
return exports;
```

**main.behl:**
```cpp
const math = import("modules/math");
print(math.add(5, 3));      // 8
print(math.multiply(4, 7)); // 28
```

### Stateful Module

**logger.behl:**
```cpp
module;

let logCount = 0;

function log(message) {
    logCount++;
    print("[LOG] " + message);
}

function getCount() {
    return logCount;
}

return {
    log = log,
    getCount = getCount
};
```

**Usage:**
```cpp
const logger = import("logger");
logger.log("Starting app");
logger.log("Loading config");
print("Total logs: " + tostring(logger.getCount())); // 2
```

### Relative Import Example

**database/shared.behl:**
```cpp
module;

function validateConnection(host) {
    return host != nil && typeof(host) == "string";
}

return { validateConnection = validateConnection };
```

**database/postgres.behl:**
```cpp
module;

// Import from same directory
const shared = import("./shared");

let queryCount = 0;

function connect(host, port) {
    if (!shared.validateConnection(host)) {
        error("Invalid host");
    }
    print("Connected to " + host + ":" + tostring(port));
}

function query(sql) {
    queryCount++;
    return { result = "success", count = queryCount };
}

return {
    connect = connect,
    query = query,
    getQueryCount = function() { return queryCount; }
};
```

## Built-in Modules

Behl provides standard modules that are registered when `load_stdlib(S, make_global)` is called:

- `math` - Mathematical functions and constants
- `string` - String manipulation utilities  
- `table` - Table manipulation functions
- `os` - Operating system functions
- `gc` - Garbage collector control
- `debug` - Debugging utilities

### Global vs Import-Only Access

The `make_global` parameter in `load_stdlib(S, make_global)` controls how modules are exposed **in script mode only**:

**Script Mode (no `module;` declaration):**

With `make_global = true`:
```cpp
// Direct access - no import needed
print(math.pi);              // 3.14159
let upper = string.upper("hello");
table.insert(arr, 10);
```

With `make_global = false`:
```cpp
// Explicit import required
const math = import("math");
const string = import("string");
print(math.pi);              // 3.14159
```

**Module Mode (with `module;` declaration):**

Regardless of `make_global`, standard library modules **must always be imported**:

```cpp
module;

// This ALWAYS fails, even with make_global = true
// print(math.pi);  // Error: Variable 'math' is not declared

// Must import explicitly
const math = import("math");
print(math.pi);  // OK
```

**Why this design?**
- **Script mode with globals** (`true`) is convenient for quick scripts, REPL, and prototyping
- **Script mode import-only** (`false`) provides better control and clearer dependencies  
- **Module mode** enforces isolation and explicit dependencies, making code more maintainable and preventing accidental global access

See [Standard Library](standard-library) for complete API documentation.

## C++ API for Modules

### Registering C++ Modules

Use `create_module()` to register native modules:

```cpp
#include <behl/behl.hpp>

static int add(behl::State* S) {
    int a = behl::to_integer(S, 0);
    int b = behl::to_integer(S, 1);
    behl::push_integer(S, a + b);
    return 1;
}

int main() {
    behl::State* S = behl::new_state();
    
    // Define module
    behl::ModuleReg funcs[] = {
        {"add", add}
    };
    behl::ModuleDef def = { .funcs = funcs };
    
    // Register module
    behl::create_module(S, "mymath", def);
    
    // Now available via import("mymath")
    behl::load_string(S, R"(
        const mymath = import("mymath");
        print(mymath.add(5, 3));
    )");
    behl::call(S, 0, 0);
    
    behl::close(S);
}
```

## Best Practices

### 1. Use Module Mode for Libraries

```cpp
module;  // Isolate from global scope

// Define your library
```

### 2. Explicit Exports

Be clear about what's public:

```cpp
module;

// Private helpers
function validateInput(x) { /* ... */ }

// Public API
return {
    processData = function(data) {
        if (!validateInput(data)) {
            error("Invalid data");
        }
        // ...
    }
};
```

### 3. Relative Imports for Project Files

```cpp
// Prefer relative imports within your project
const config = import("./config");
const utils = import("../utils/helpers");

// Use module paths for third-party or standard libs
const math = import("math");
```

### 4. Module State is Shared

Remember that module state persists:

```cpp
module;

letError Messages

Module mode provides clear compile-time errors when accessing undeclared variables:

```cpp
module;

// Error: Variable 'math' is not declared. Use 'let' or 'const' to declare 
// local variables, or 'import()' to load modules.
print(math.pi);

// Error: Variable 'globalVar' is not declared.
let x = globalVar;
```

These are **compile-time errors** (thrown as `SemanticError` during parsing), not runtime errors.

## Limitations

- No circular imports protection (may cause infinite loops)
- No dynamic module reloading (cache cannot be invalidated)
- Module mode validates identifiers at compile time, so any undefined variable access fails even if it would exist at runtime
        connectionPool = createPool();
    }
    return connectionPool;
}

return { getConnection = getConnection };
```

All importers share the same `connectionPool`.

## Limitations

- No circular imports protection (may cause infinite loops)
- No dynamic module reloading (cache cannot be invalidated)

## See Also

- [Standard Library](standard-library) - Built-in modules documentation
- [Embedding Guide](embedding/) - Embedding and extending with C++
- [Language](language/) - Core language features
