---
layout: default
title: Userdata
parent: Embedding
nav_order: 5
---

# Userdata
{: .no_toc }

Expose C++ objects to Behl scripts with type-safe userdata.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Userdata allows wrapping C++ objects for use in Behl scripts. Each userdata has a **unique identifier (UID)** for type safety, preventing accidental misuse of incompatible types.

**Key features:**
- Type-safe with UIDs
- Automatic garbage collection
- Metatable support for operators
- Resource cleanup via `__gc` metamethod

---

## Creating Userdata

### Step 1: Define Your Type and UID

```cpp
struct FileHandle {
    FILE* file;
    bool is_open;
};

// Generate a unique ID for this type (compile-time constant)
constexpr uint32_t FileHandle_UID = behl::make_uid("FileHandle");
```

The `make_uid()` function generates a unique 32-bit identifier from a string using FNV-1a hashing. Use consistent names to get the same UID across translation units.

### Step 2: Allocate Userdata

### `userdata_new(State*, size_t, uint32_t)`

Allocates userdata of given size with a UID and pushes it onto the stack.

```cpp
void* ptr = behl::userdata_new(S, sizeof(FileHandle), FileHandle_UID);
FileHandle* handle = static_cast<FileHandle*>(ptr);

// Initialize the data
handle->file = fopen("data.txt", "r");
handle->is_open = true;

// Stack: [userdata]
```

---

## Accessing Userdata

### `to_userdata(State*, int32_t)`

Returns pointer to userdata, or `nullptr` if not userdata.

```cpp
void* ptr = behl::to_userdata(S, -1);
if (ptr != nullptr) {
    FileHandle* handle = static_cast<FileHandle*>(ptr);
    // Use handle...
}
```

**Use case:** When you don't need type validation (e.g., in finalizers).

### `check_userdata(State*, int32_t, uint32_t)`

Type-safe retrieval that validates the UID and throws `TypeError` if wrong type.

```cpp
// Throws if not userdata or UID mismatch
FileHandle* handle = static_cast<FileHandle*>(
    behl::check_userdata(S, 0, FileHandle_UID)
);
```

**Error handling:**
```cpp
try {
    void* ptr = behl::check_userdata(S, 0, FileHandle_UID);
    FileHandle* handle = static_cast<FileHandle*>(ptr);
} catch (const behl::TypeError& e) {
    // Handle error: "bad argument #1 (expected userdata, got string)"
}
```

**Use case:** Always use in API functions for type safety.

### `userdata_get_uid(State*, int32_t)`

Returns the UID of userdata at the given index, or `0` if not userdata.

```cpp
uint32_t uid = behl::userdata_get_uid(S, -1);
if (uid == FileHandle_UID) {
    // It's a FileHandle
} else if (uid == Vector2D_UID) {
    // It's a Vector2D
}
```

### `is_userdata(State*, int32_t)`

Returns `true` if the value at the given index is userdata (any type).

```cpp
if (behl::is_userdata(S, -1)) {
    // It's some kind of userdata
}
```

---

## Complete Example: File I/O

```cpp
#include <behl/behl.hpp>
#include <cstdio>

// Define the userdata type
struct FileHandle {
    FILE* file;
    bool is_open;
};

constexpr uint32_t FileHandle_UID = behl::make_uid("FileHandle");

// Create a new file handle
static int file_open(behl::State* S) {
    auto filename = behl::check_string(S, 0);
    auto mode = behl::check_string(S, 1);
    
    // Allocate userdata
    void* ptr = behl::userdata_new(S, sizeof(FileHandle), FileHandle_UID);
    FileHandle* handle = static_cast<FileHandle*>(ptr);
    
    // Initialize the data
    handle->file = fopen(std::string(filename).c_str(), 
                        std::string(mode).c_str());
    handle->is_open = (handle->file != nullptr);
    
    if (!handle->is_open) {
        behl::error(S, "Failed to open file");
    }
    
    // Set up finalizer metatable (for automatic cleanup)
    behl::table_new(S);  // Create metatable
    behl::push_cfunction(S, file_finalizer);
    behl::table_rawset_field(S, -2, "__gc");
    behl::set_metatable(S, -2);  // Attach to userdata
    
    return 1;  // Return the userdata
}

// Read from file
static int file_read(behl::State* S) {
    // Type-safe retrieval with UID check
    FileHandle* handle = static_cast<FileHandle*>(
        behl::check_userdata(S, 0, FileHandle_UID)
    );
    
    if (!handle->is_open) {
        behl::error(S, "File is closed");
    }
    
    // Read line
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), handle->file)) {
        behl::push_string(S, buffer);
        return 1;
    }
    
    behl::push_nil(S);  // EOF
    return 1;
}

// Close file
static int file_close(behl::State* S) {
    FileHandle* handle = static_cast<FileHandle*>(
        behl::check_userdata(S, 0, FileHandle_UID)
    );
    
    if (handle->is_open) {
        fclose(handle->file);
        handle->is_open = false;
    }
    return 0;
}

// Finalizer (called by GC)
static int file_finalizer(behl::State* S) {
    // Use to_userdata since we don't validate in finalizers
    void* ptr = behl::to_userdata(S, 0);
    if (ptr) {
        FileHandle* handle = static_cast<FileHandle*>(ptr);
        if (handle->is_open) {
            fclose(handle->file);
            handle->is_open = false;
        }
    }
    return 0;
}

// Register the API
void register_file_api(behl::State* S) {
    behl::register_function(S, "file_open", file_open);
    behl::register_function(S, "file_read", file_read);
    behl::register_function(S, "file_close", file_close);
}
```

**Usage in Behl:**
```javascript
let f = file_open("data.txt", "r");
let line = file_read(f);
print(line);
file_close(f);  // Or let GC close it automatically

// Type safety:
let t = {};
file_read(t);  // TypeError: bad argument #1 (expected userdata, got table)
```

---

## Metatables and Operators

Userdata can have metatables for operator overloading and custom behavior:

```cpp
// Vector2D userdata
struct Vector2D {
    double x, y;
};

constexpr uint32_t Vector2D_UID = behl::make_uid("Vector2D");

// __add metamethod
static int vec2_add(behl::State* S) {
    Vector2D* a = static_cast<Vector2D*>(
        behl::check_userdata(S, 0, Vector2D_UID)
    );
    Vector2D* b = static_cast<Vector2D*>(
        behl::check_userdata(S, 1, Vector2D_UID)
    );
    
    // Create result
    void* ptr = behl::userdata_new(S, sizeof(Vector2D), Vector2D_UID);
    Vector2D* result = static_cast<Vector2D*>(ptr);
    result->x = a->x + b->x;
    result->y = a->y + b->y;
    
    // Copy metatable from first operand
    behl::get_metatable(S, 0);
    behl::set_metatable(S, -2);
    
    return 1;
}

// __tostring metamethod
static int vec2_tostring(behl::State* S) {
    Vector2D* v = static_cast<Vector2D*>(
        behl::check_userdata(S, 0, Vector2D_UID)
    );
    std::string str = behl::format("Vector2D({}, {})", v->x, v->y);
    behl::push_string(S, str);
    return 1;
}

// Create and store metatable
void create_vector2d_metatable(behl::State* S) {
    // Create metatable
    behl::table_new(S);
    
    // __add metamethod
    behl::push_cfunction(S, vec2_add);
    behl::table_rawset_field(S, -2, "__add");
    
    // __tostring metamethod
    behl::push_cfunction(S, vec2_tostring);
    behl::table_rawset_field(S, -2, "__tostring");
    
    // Store in registry for reuse
    behl::table_rawset_field(S, behl::REGISTRY_INDEX, "Vector2D_mt");
}

// Constructor
static int vec2_new(behl::State* S) {
    double x = behl::check_number(S, 0);
    double y = behl::check_number(S, 1);
    
    void* ptr = behl::userdata_new(S, sizeof(Vector2D), Vector2D_UID);
    Vector2D* v = static_cast<Vector2D*>(ptr);
    v->x = x;
    v->y = y;
    
    // Attach metatable
    behl::table_rawget_field(S, behl::REGISTRY_INDEX, "Vector2D_mt");
    behl::set_metatable(S, -2);
    
    return 1;
}
```

**Usage:**
```javascript
let v1 = vec2_new(1.0, 2.0);
let v2 = vec2_new(3.0, 4.0);
let v3 = v1 + v2;  // Calls __add
print(v3);         // Calls __tostring: "Vector2D(4.0, 6.0)"
```

---

## Polymorphic Userdata

Accept multiple userdata types:

```cpp
static int accepts_any_userdata(behl::State* S) {
    if (!behl::is_userdata(S, 0)) {
        behl::error(S, "Expected userdata");
    }
    
    uint32_t uid = behl::userdata_get_uid(S, 0);
    
    if (uid == FileHandle_UID) {
        FileHandle* f = static_cast<FileHandle*>(behl::to_userdata(S, 0));
        // Handle FileHandle...
    } else if (uid == Vector2D_UID) {
        Vector2D* v = static_cast<Vector2D*>(behl::to_userdata(S, 0));
        // Handle Vector2D...
    } else {
        behl::error(S, "Unknown userdata type");
    }
    
    return 0;
}
```

---

## Best Practices

### 1. Always Use UIDs

Never create userdata without a UID. This prevents type confusion bugs.

```cpp
// Bad - No type safety
void* ptr = malloc(sizeof(MyType));

// Good - Type-safe with UID
constexpr uint32_t MyType_UID = behl::make_uid("MyType");
void* ptr = behl::userdata_new(S, sizeof(MyType), MyType_UID);
```

### 2. Use `check_userdata` in API Functions

Always validate type and UID, provides clear error messages.

```cpp
// [GOOD] Type-safe API function
static int my_func(behl::State* S) {
    MyType* obj = static_cast<MyType*>(
        behl::check_userdata(S, 0, MyType_UID)
    );
    // ...
}
```

### 3. Use `to_userdata` in Finalizers

Finalizers should be defensive since they're called by GC.

```cpp
static int my_finalizer(behl::State* S) {
    void* ptr = behl::to_userdata(S, 0);
    if (ptr) {
        MyType* obj = static_cast<MyType*>(ptr);
        // Cleanup...
    }
    return 0;
}
```

### 4. Add `__gc` Metamethod

Ensures resources are cleaned up even if scripts forget.

```cpp
behl::table_new(S);
behl::push_cfunction(S, my_finalizer);
behl::table_rawset_field(S, -2, "__gc");
behl::set_metatable(S, -2);
```

### 5. Store Metatables in Registry

Reuse metatables for efficiency:

```cpp
// Create once
behl::table_rawget_field(S, behl::REGISTRY_INDEX, "MyType_mt");
if (behl::is_nil(S, -1)) {
    behl::pop(S, 1);
    behl::table_new(S);
    // Configure metatable...
    behl::table_rawset_field(S, behl::REGISTRY_INDEX, "MyType_mt");
}
```

### 6. Document UID Naming

Use consistent naming conventions like `"TypeName"` for `make_uid()`.

```cpp
// [GOOD] Consistent naming
constexpr uint32_t FileHandle_UID = behl::make_uid("FileHandle");
constexpr uint32_t Vector2D_UID = behl::make_uid("Vector2D");

// [BAD] Inconsistent - will generate different UIDs
constexpr uint32_t UID1 = behl::make_uid("file_handle");  // Different!
constexpr uint32_t UID2 = behl::make_uid("FileHandle");
```

---

## Next Steps

- Create reusable libraries with [Modules](modules)
- Handle errors gracefully with [Error Handling](error-handling)
- See complete examples in [API Reference](api-reference)
