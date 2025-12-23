---
layout: default
title: Embedding
nav_order: 5
has_children: true
---

# Embedding Behl in C++

Learn how to embed the Behl scripting language in your C++ application.

## Quick Start

```cpp
#include <behl/behl.hpp>

int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S, true);
    
    behl::load_string(S, "print('Hello from Behl!')");
    behl::call(S, 0, 0);
    
    behl::close(S);
    return 0;
}
```

## Guides

- **[Getting Started](getting-started)** - State management and basic setup
- **[Stack Operations](stack-operations)** - Working with the stack
- **[Calling Functions](calling-functions)** - Load and execute code
- **[Tables](tables)** - Create and manipulate tables
- **[Userdata](userdata)** - Wrap C++ objects for scripts
- **[Error Handling](error-handling)** - Handle errors gracefully
- **[Creating Modules](modules)** - Build reusable C++ modules
- **[Debugging](debugging)** - Debug API for tools
- **[API Reference](api-reference)** - Complete function list

## Philosophy

The behl C++ API is designed to be:
- **Familiar** - Similar to Lua's C API
- **Modern** - Leverages C++20 features
- **Type-safe** - Strong type checking with UIDs
- **Simple** - Easy to integrate and use
