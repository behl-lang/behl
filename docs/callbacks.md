---
layout: default
title: Callbacks
parent: Advanced Topics
nav_order: 2
---

# Callbacks and Pinning
{: .no_toc }

How to safely store and invoke Behl functions from C++ using the pinning API.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

When integrating Behl with C++, you often need to store Behl functions (callbacks, event handlers, etc.) for later invocation. However, these functions can be garbage collected if not properly protected. The **pinning API** solves this problem by preventing GC from collecting specific values.

## The Problem

Storing stack indices or direct pointers to Behl values is **unsafe** and can lead to crashes:

```cpp
// WRONG - Function may be collected!
static behl::State* g_state = nullptr;
static int g_callback_ref = -1;  // Stack index - UNSAFE

void register_callback(behl::State* S) {
    // Function is at top of stack
    g_state = S;
    g_callback_ref = behl::get_top(S) - 1;  // Just storing index - WRONG!
}

void trigger_callback() {
    // Accessing g_callback_ref - MAY CRASH!
    // The function might have been garbage collected
    behl::dup(g_state, g_callback_ref);
    behl::call(g_state, 0, 0);
}
```

**Why this fails:**
- Stack indices are only valid temporarily
- The function can be garbage collected
- Stack can be reallocated, invalidating the index
- Accessing freed memory causes undefined behavior

## The Solution: Pinning API

The pinning API protects Behl values from garbage collection:

```cpp
behl::PinHandle pin(State* S);              // Pops value from stack, returns handle
void pinned_push(State* S, PinHandle h);    // Push pinned value back onto stack
void unpin(State* S, PinHandle h);          // Release pin, allow GC
```

### Basic Example

```cpp
// CORRECT - Using pinning
class EventHandler {
private:
    behl::State* state;
    behl::PinHandle callback_handle;
    
public:
    EventHandler(behl::State* S) : state(S), callback_handle(0) {}
    
    // Register a callback function
    void register_callback() {
        // Expect function on top of stack
        if (!behl::is_function(state, -1)) {
            behl::error(state, "Expected function");
        }
        
        // Release old callback if exists
        if (callback_handle) {
            behl::unpin(state, callback_handle);
        }
        
        // Pin the function (pops it from stack)
        callback_handle = behl::pin(state);
    }
    
    // Trigger the callback later
    void trigger(const char* event_name) {
        if (!callback_handle) {
            return;
        }
        
        // Push the pinned function back onto stack
        behl::pinned_push(state, callback_handle);
        
        // Push arguments
        behl::push_string(state, event_name);
        
        // Call the function
        behl::call(state, 1, 0);
    }
    
    ~EventHandler() {
        // Clean up the pin to allow GC
        if (callback_handle) {
            behl::unpin(state, callback_handle);
        }
    }
};
```

## Complete Example: Event System

Here's a full implementation of an event system using pinning:

```cpp
#include <behl/behl.hpp>
#include <vector>
#include <string>

class EventSystem {
private:
    struct Listener {
        behl::PinHandle callback;
        std::string event_type;
    };
    
    behl::State* state;
    std::vector<Listener> listeners;
    
public:
    EventSystem(behl::State* S) : state(S) {}
    
    // C function: on(event_name, callback)
    static int on(behl::State* S) {
        auto* self = static_cast<EventSystem*>(
            behl::to_userdata(S, 0)
        );
        auto event_name = behl::check_string(S, 1);
        
        if (!behl::is_function(S, 2)) {
            behl::error(S, "Expected function as second argument");
        }
        
        // Duplicate the function to pin it
        behl::dup(S, 2);
        behl::PinHandle handle = behl::pin(S);
        
        self->listeners.push_back({
            handle,
            std::string(event_name)
        });
        
        return 0;
    }
    
    // C function: emit(event_name, data)
    static int emit(behl::State* S) {
        auto* self = static_cast<EventSystem*>(
            behl::to_userdata(S, 0)
        );
        auto event_name = behl::check_string(S, 1);
        
        // Call all matching listeners
        for (const auto& listener : self->listeners) {
            if (listener.event_type == event_name) {
                // Push callback
                behl::pinned_push(S, listener.callback);
                
                // Push event data (if provided)
                if (behl::get_top(S) > 2) {
                    behl::dup(S, 2);
                } else {
                    behl::push_nil(S);
                }
                
                // Call callback
                behl::call(S, 1, 0);
            }
        }
        
        return 0;
    }
    
    ~EventSystem() {
        // Unpin all callbacks
        for (const auto& listener : listeners) {
            behl::unpin(state, listener.callback);
        }
    }
};

// Register the event system
int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    
    // Create event system
    EventSystem events(S);
    auto* userdata = behl::userdata_new(S, sizeof(EventSystem*));
    *static_cast<EventSystem**>(userdata) = &events;
    
    // Register methods
    behl::push_cfunction(S, EventSystem::on);
    behl::set_global(S, "on");
    behl::push_cfunction(S, EventSystem::emit);
    behl::set_global(S, "emit");
    
    // Use from Behl
    behl::load_string(S, R"(
        // Register event handlers
        on("data_received", function(data) {
            print("Received: " + tostring(data));
        });
        
        on("error", function(err) {
            print("Error: " + err);
        });
        
        // Trigger events
        emit("data_received", 42);
        emit("error", "Connection failed");
    )");
    
    behl::call(S, 0, 0);
    behl::close(S);
    
    return 0;
}
```

**Output:**
```
Received: 42
Error: Connection failed
```

## RAII Wrapper

For safer pinning, create an RAII wrapper:

```cpp
class PinnedValue {
private:
    behl::State* state;
    behl::PinHandle handle;
    
public:
    // Pin the value at top of stack
    PinnedValue(behl::State* S) : state(S), handle(behl::pin(S)) {}
    
    // Push the pinned value back onto stack
    void push() const {
        behl::pinned_push(state, handle);
    }
    
    // Get the handle
    behl::PinHandle get() const { return handle; }
    
    // Disable copying
    PinnedValue(const PinnedValue&) = delete;
    PinnedValue& operator=(const PinnedValue&) = delete;
    
    // Allow moving
    PinnedValue(PinnedValue&& other) noexcept 
        : state(other.state), handle(other.handle) {
        other.handle = 0;
    }
    
    PinnedValue& operator=(PinnedValue&& other) noexcept {
        if (this != &other) {
            if (handle) {
                behl::unpin(state, handle);
            }
            state = other.state;
            handle = other.handle;
            other.handle = 0;
        }
        return *this;
    }
    
    // Automatically unpin on destruction
    ~PinnedValue() {
        if (handle) {
            behl::unpin(state, handle);
        }
    }
};

// Usage
void example(behl::State* S) {
    // Value on stack will be automatically pinned and unpinned
    PinnedValue callback(S);
    
    // Later, push it back
    callback.push();
    behl::call(S, 0, 0);
    
    // Automatically unpinned when callback goes out of scope
}
```

## Common Use Cases

### 1. Timer Callbacks

```cpp
class Timer {
private:
    behl::State* state;
    behl::PinHandle callback;
    
public:
    void set_callback(behl::State* S) {
        if (callback) {
            behl::unpin(state, callback);
        }
        state = S;
        callback = behl::pin(S);
    }
    
    void on_tick() {
        behl::pinned_push(state, callback);
        behl::call(state, 0, 0);
    }
};
```

### 2. Async Operations

```cpp
class AsyncRequest {
private:
    behl::State* state;
    behl::PinHandle on_success;
    behl::PinHandle on_error;
    
public:
    void set_callbacks(behl::State* S) {
        // Expect two functions on stack
        on_error = behl::pin(S);    // Pops error callback
        on_success = behl::pin(S);  // Pops success callback
        state = S;
    }
    
    void complete(bool success, const std::string& result) {
        if (success) {
            behl::pinned_push(state, on_success);
            behl::push_string(state, result);
            behl::call(state, 1, 0);
        } else {
            behl::pinned_push(state, on_error);
            behl::push_string(state, result);
            behl::call(state, 1, 0);
        }
    }
    
    ~AsyncRequest() {
        behl::unpin(state, on_success);
        behl::unpin(state, on_error);
    }
};
```

### 3. Storing Tables

You can pin any Behl value, not just functions:

```cpp
class Config {
private:
    behl::State* state;
    behl::PinHandle config_table;
    
public:
    void load_config(behl::State* S) {
        state = S;
        // Table is on stack
        config_table = behl::pin(S);
    }
    
    int get_timeout() {
        behl::pinned_push(state, config_table);
        behl::push_string(state, "timeout");
        behl::table_get(state, -2);
        int timeout = behl::to_integer(state, -1);
        behl::pop(state, 2);  // Pop value and table
        return timeout;
    }
    
    ~Config() {
        behl::unpin(state, config_table);
    }
};
```

## Key Rules

### Must-Know Behavior

1. **`pin()` pops the value** from the stack
2. **Must call `unpin()`** when done to prevent memory leaks
3. **Pinned values are never collected** by GC
4. **Can pin any value type**: functions, tables, strings, etc.
5. **Handle value is 0 on failure** - always check!

### Best Practices

1. **Use RAII** - Wrap pins in classes with destructors
2. **Check for null handles** - Handle `0` means invalid
3. **Unpin in reverse order** - If order matters
4. **One pin per value** - Don't pin the same value twice unnecessarily
5. **Document lifetime** - Make clear who owns the pin

### Common Mistakes

```cpp
// [BAD] WRONG - Forgetting to unpin
void leak_example(behl::State* S) {
    behl::PinHandle h = behl::pin(S);
    // Never unpinned - memory leak!
}

// [BAD] WRONG - Using after unpin
void dangling_example(behl::State* S) {
    behl::PinHandle h = behl::pin(S);
    behl::unpin(S, h);
    behl::pinned_push(S, h);  // UNDEFINED BEHAVIOR
}

// [BAD] WRONG - Not checking handle
void unchecked_example(behl::State* S) {
    behl::PinHandle h = behl::pin(S);
    // What if h is 0?
    behl::pinned_push(S, h);  // May crash
}

// [GOOD] CORRECT - Proper error handling
void safe_example(behl::State* S) {
    behl::PinHandle h = behl::pin(S);
    if (!h) {
        behl::error(S, "Failed to pin value");
    }
    
    behl::pinned_push(S, h);
    behl::call(S, 0, 0);
    behl::unpin(S, h);
}
```

## Thread Safety

The pinning API is **not thread-safe**. If you need to access pinned values from multiple threads:

```cpp
#include <mutex>

class ThreadSafeCallback {
private:
    behl::State* state;
    behl::PinHandle callback;
    std::mutex mtx;
    
public:
    void trigger() {
        std::lock_guard<std::mutex> lock(mtx);
        
        behl::pinned_push(state, callback);
        behl::call(state, 0, 0);
    }
};
```

## Performance Considerations

- Pinning is lightweight - just adds value to a list
- Unpinning is O(n) where n is the number of pinned values
- Too many pins can increase GC pressure (pinned values can't be collected)
- Consider unpinning values you no longer need

## See Also

- [Embedding Guide](embedding/) - Core API documentation
- [API Reference](embedding/api-reference#value-pinning) - Detailed pinning API reference
- [Standard Library](standard-library) - Built-in functions and modules
- [Embedding Guide](getting-started) - Getting started with embedding Behl
