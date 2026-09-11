---
layout: default
title: Getting Started
parent: Embedding
nav_order: 1
---

# Getting Started
{: .no_toc }

Basic setup for embedding Behl in your C++ application.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Include and Link

### Include Header

```cpp
#include <behl/behl.hpp>
```

All Behl functions are in the `behl` namespace.

### Linking

Link against the `behl` library:

```cmake
target_link_libraries(your_target PRIVATE behl)
```

---

## State Management

### Creating a State

The Behl `State` is the core runtime object. Create one with `new_state()`:

```cpp
behl::State* S = behl::new_state();
```

**Returns:** Pointer to the new state.

### Loading the Standard Library

Load the standard libraries (core, table, gc, jit, debug, math, os, string):

```cpp
behl::load_stdlib(S);
```

The core library installs its functions directly as globals (`print`, `pcall`, `error`, `pairs`,
`import`, ...); the others are modules and must be explicitly imported using `import()`. The
security-sensitive `fs` and `process` libraries are not loaded by `load_stdlib`, load them opt-in
with `load_lib_fs()` and `load_lib_process()`.

### Cleaning Up

Always close the state when done:

```cpp
behl::close(S);
S = nullptr;  // Good practice
```

---

## Complete Example

```cpp
#include <behl/behl.hpp>
#include <iostream>

int main() {
    // Create state
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    
    // Load and run a script
    const char* script = R"(
        let x = 10;
        let y = 20;
        print("Sum: " + tostring(x + y));
        return x + y;
    )";
    
    try {
        behl::load_string(S, script);
        behl::call(S, 0, 1);
        int result = behl::to_integer(S, -1);
        std::cout << "Result from C++: " << result << "\n";
        behl::pop(S, 1);
    } catch (const behl::BehlException& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    
    // Cleanup
    behl::close(S);
    return 0;
}
```

---

## Next Steps

- Learn about [Stack Operations](stack-operations) to manipulate values
- See how to [Call Functions](calling-functions) from C++
- Explore [Userdata](userdata) to expose C++ objects
