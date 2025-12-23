---
layout: default
title: Creating Modules
parent: Embedding
nav_order: 7
---

# Creating Modules
{: .no_toc }

Build reusable C++ modules for Behl scripts.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Modules allow organizing C++ functionality that scripts can import. Modules contain functions and constants accessible via the `import()` statement.

---

## Creating a Module

### `create_module(State*, std::string_view, const ModuleDef&)`

Registers a module that can be imported from Behl scripts.

**Module structure:**
```cpp
struct ModuleDef {
    std::span<const ModuleReg> funcs;     // Functions
    std::span<const ModuleConst> consts;  // Constants
};

struct ModuleReg {
    std::string_view name;
    CFunction func;
};

struct ModuleConst {
    std::string_view name;
    Value value;  // Integer, number, string, or boolean
};
```

---

## Complete Example

```cpp
#include <behl/behl.hpp>

// Module functions
int mod_add(behl::State* S) {
    int a = behl::check_integer(S, 0);
    int b = behl::check_integer(S, 1);
    behl::push_integer(S, a + b);
    return 1;
}

int mod_sub(behl::State* S) {
    int a = behl::check_integer(S, 0);
    int b = behl::check_integer(S, 1);
    behl::push_integer(S, a - b);
    return 1;
}

int mod_mul(behl::State* S) {
    int a = behl::check_integer(S, 0);
    int b = behl::check_integer(S, 1);
    behl::push_integer(S, a * b);
    return 1;
}

// Register module
void register_math_module(behl::State* S) {
    // Define functions
    behl::ModuleReg funcs[] = {
        {"add", mod_add},
        {"sub", mod_sub},
        {"mul", mod_mul}
    };
    
    // Define constants
    behl::ModuleConst consts[] = {
        {"VERSION", 1},
        {"NAME", "mathops"}
    };
    
    // Create module definition
    behl::ModuleDef module_def = {
        .funcs = funcs,
        .consts = consts
    };
    
    // Register the module
    behl::create_module(S, "mathops", module_def);
}

int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S, true);
    
    // Register custom module
    register_math_module(S);
    
    // Use in script
    const char* script = R"(
        const mathops = import("mathops");
        
        print("Version: " + tostring(mathops["VERSION"]));
        print("5 + 3 = " + tostring(mathops["add"](5, 3)));
        print("10 - 4 = " + tostring(mathops["sub"](10, 4)));
        print("6 * 7 = " + tostring(mathops["mul"](6, 7)));
    )";
    
    if (behl::load_string(S, script)) {
        behl::call(S, 0, 0);
    }
    
    behl::close(S);
    return 0;
}
```

**Output:**
```
Version: 1
5 + 3 = 8
10 - 4 = 6
6 * 7 = 42
```

---

## Module Best Practices

### 1. Use Descriptive Names

```cpp
// Good - Clear purpose
behl::create_module(S, "fileio", module_def);
behl::create_module(S, "graphics", module_def);

// Bad - Vague
behl::create_module(S, "utils", module_def);
behl::create_module(S, "stuff", module_def);
```

### 2. Group Related Functionality

```cpp
// Good - Cohesive module
behl::ModuleReg file_funcs[] = {
    {"open", file_open},
    {"read", file_read},
    {"write", file_write},
    {"close", file_close}
};

// Bad - Unrelated functions
behl::ModuleReg misc_funcs[] = {
    {"open_file", file_open},
    {"draw_circle", draw_circle},
    {"parse_json", parse_json}
};
```

### 3. Provide Version Constants

```cpp
behl::ModuleConst consts[] = {
    {"VERSION_MAJOR", 1},
    {"VERSION_MINOR", 2},
    {"VERSION_PATCH", 3}
};
```

### 4. Use Type Checking

```cpp
int mod_func(behl::State* S) {
    // [GOOD] Validate arguments
    int x = behl::check_integer(S, 0);
    std::string_view s = behl::check_string(S, 1);
    // ...
}
```

---

## Modules vs Global Functions

### Global Functions

Simple, direct access:

```cpp
behl::register_function(S, "add", add_func);

// In script:
let result = add(5, 3);
```

**Use when:** You have a few utility functions.

### Modules

Organized, namespaced:

```cpp
behl::create_module(S, "math", module_def);

// In script:
const math = import("math");
let result = math["add"](5, 3);
```

**Use when:** You have many related functions or want to avoid global namespace pollution.

---

## Module with Userdata

Modules can return userdata types:

```cpp
struct Socket {
    int fd;
    bool connected;
};

constexpr uint32_t Socket_UID = behl::make_uid("Socket");

int net_connect(behl::State* S) {
    auto host = behl::check_string(S, 0);
    int port = behl::check_integer(S, 1);
    
    // Create socket userdata
    void* ptr = behl::userdata_new(S, sizeof(Socket), Socket_UID);
    Socket* sock = static_cast<Socket*>(ptr);
    
    // Connect... (implementation omitted)
    sock->fd = /* ... */;
    sock->connected = true;
    
    // Attach metatable with __gc
    // ...
    
    return 1;  // Return socket userdata
}

int net_send(behl::State* S) {
    Socket* sock = static_cast<Socket*>(
        behl::check_userdata(S, 0, Socket_UID)
    );
    auto data = behl::check_string(S, 1);
    
    // Send data...
    
    return 0;
}

void register_network_module(behl::State* S) {
    behl::ModuleReg funcs[] = {
        {"connect", net_connect},
        {"send", net_send},
        {"recv", net_recv}
    };
    
    behl::ModuleDef module_def = {.funcs = funcs};
    behl::create_module(S, "network", module_def);
}
```

**Usage:**
```javascript
const net = import("network");

let sock = net["connect"]("example.com", 80);
net["send"](sock, "GET / HTTP/1.0\r\n\r\n");
```

---

## Next Steps

- See the complete [API Reference](api-reference)
- Review [Examples](../examples) for more patterns
- Learn about [Error Handling](error-handling)
