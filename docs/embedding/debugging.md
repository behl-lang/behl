---
layout: default
title: Debugging
parent: Embedding
nav_order: 9
---

# Debugging API
{: .no_toc }

Build debuggers and development tools with Behl's debugging API.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Behl provides a comprehensive debugging API for implementing debuggers, IDEs, and development tools. The API supports breakpoints, stepping, and execution control.

---

## Enabling Debug Mode

```cpp
#include <behl/debug.hpp>

// Enable debugging
behl::debug_enable(S, true);

// Check if debugging is enabled
bool enabled = behl::debug_is_enabled(S);

// Disable debugging
behl::debug_enable(S, false);
```

---

## Debug Events

The debugger triggers events when execution pauses:

```cpp
enum class DebugEvent {
    Breakpoint,  // Hit a breakpoint
    Step,        // Completed a step
    Pause        // Paused via debug_pause()
};
```

---

## Event Callbacks

Set a callback to receive debug events:

```cpp
void debug_event_handler(behl::State* S, behl::DebugEvent event) {
    const char* file = nullptr;
    int line = 0;
    int column = 0;
    
    if (behl::debug_get_location(S, &file, &line, &column)) {
        printf("Paused at %s:%d:%d\n", file, line, column);
    }
    
    // Resume execution
    behl::debug_continue(S);
}

behl::debug_set_event_callback(S, debug_event_handler);
```

---

## Breakpoints

### Setting Breakpoints

```cpp
// Set breakpoint at line 10 of current file
behl::debug_set_breakpoint(S, nullptr, 10);

// Set breakpoint at specific file and line
behl::debug_set_breakpoint(S, "script.behl", 25);
```

### Removing Breakpoints

```cpp
// Remove specific breakpoint
behl::debug_remove_breakpoint(S, "script.behl", 25);

// Remove all breakpoints
behl::debug_clear_breakpoints(S);
```

---

## Execution Control

### Stepping

```cpp
// Step into: execute next line, enter function calls
behl::debug_step_into(S);

// Step over: execute next line, skip function calls
behl::debug_step_over(S);

// Step out: continue until current function returns
behl::debug_step_out(S);
```

### Continue and Pause

```cpp
// Continue execution until next breakpoint
behl::debug_continue(S);

// Pause at next opportunity
behl::debug_pause(S);
```

---

## Location Information

Get the current execution location:

```cpp
const char* file = nullptr;
int line = 0;
int column = 0;

bool has_location = behl::debug_get_location(S, &file, &line, &column);

if (has_location) {
    printf("Current location: %s:%d:%d\n", file, line, column);
}
```

---

## Complete Debugger Example

```cpp
#include <behl/behl.hpp>
#include <behl/debug.hpp>
#include <iostream>
#include <string>

class SimpleDebugger {
private:
    behl::State* state;
    bool should_pause_next = false;
    
    static void event_callback(behl::State* S, behl::DebugEvent event) {
        // Get debugger instance from state
        SimpleDebugger* dbg = /* retrieve from state */;
        
        const char* file;
        int line, column;
        
        if (behl::debug_get_location(S, &file, &line, &column)) {
            std::cout << "Paused at " << file << ":" << line << "\n";
        }
        
        dbg->show_prompt(S);
    }
    
    void show_prompt(behl::State* S) {
        while (true) {
            std::cout << "(behl-debug) ";
            std::string cmd;
            std::getline(std::cin, cmd);
            
            if (cmd == "c" || cmd == "continue") {
                behl::debug_continue(S);
                break;
            }
            else if (cmd == "s" || cmd == "step") {
                behl::debug_step_into(S);
                break;
            }
            else if (cmd == "n" || cmd == "next") {
                behl::debug_step_over(S);
                break;
            }
            else if (cmd == "f" || cmd == "finish") {
                behl::debug_step_out(S);
                break;
            }
            else if (cmd == "bt" || cmd == "backtrace") {
                show_backtrace(S);
            }
            else if (cmd.rfind("b ", 0) == 0) {
                // Parse "b <line>"
                int line = std::stoi(cmd.substr(2));
                behl::debug_set_breakpoint(S, nullptr, line);
                std::cout << "Breakpoint set at line " << line << "\n";
            }
            else if (cmd == "h" || cmd == "help") {
                show_help();
            }
            else {
                std::cout << "Unknown command: " << cmd << "\n";
            }
        }
    }
    
    void show_backtrace(behl::State* S) {
        // Call debug.stacktrace()
        behl::get_global(S, "debug");
        behl::table_rawget_field(S, -1, "stacktrace");
        behl::call(S, 0, 1);
        std::cout << behl::to_string(S, -1) << "\n";
        behl::pop(S, 2);
    }
    
    void show_help() {
        std::cout << "Commands:\n"
                  << "  c, continue  - Continue execution\n"
                  << "  s, step      - Step into\n"
                  << "  n, next      - Step over\n"
                  << "  f, finish    - Step out\n"
                  << "  b <line>     - Set breakpoint\n"
                  << "  bt           - Show backtrace\n"
                  << "  h, help      - Show this help\n";
    }

public:
    SimpleDebugger(behl::State* S) : state(S) {
        behl::debug_enable(S, true);
        behl::debug_set_event_callback(S, event_callback);
    }
    
    void set_breakpoint(int line) {
        behl::debug_set_breakpoint(state, nullptr, line);
    }
    
    void run(const char* code) {
        behl::load_string(state, code);
        behl::call(state, 0, 0);
    }
};
```

---

## Interactive Debugging Session

```cpp
int main() {
    behl::State* S = behl::new_state();
    behl::load_stdlib(S);
    
    SimpleDebugger debugger(S);
    
    // Set initial breakpoint
    debugger.set_breakpoint(3);
    
    // Run script
    const char* code = R"(
        let x = 1;
        let y = 2;
        let z = x + y;
        print("Result: " + tostring(z));
    )";
    
    debugger.run(code);
    
    behl::close(S);
    return 0;
}
```

---

## Practical Examples

### Conditional Breakpoints

```cpp
struct ConditionalBreakpoint {
    int line;
    std::function<bool(behl::State*)> condition;
};

std::vector<ConditionalBreakpoint> conditional_breakpoints;

void event_callback(behl::State* S, behl::DebugEvent event) {
    const char* file;
    int line;
    
    if (behl::debug_get_location(S, &file, &line, nullptr)) {
        for (const auto& bp : conditional_breakpoints) {
            if (bp.line == line && bp.condition(S)) {
                // Handle breakpoint
                handle_breakpoint(S, line);
                return;
            }
        }
    }
    
    behl::debug_continue(S);
}
```

### Execution Tracing

```cpp
void trace_callback(behl::State* S, behl::DebugEvent event) {
    const char* file;
    int line;
    
    if (behl::debug_get_location(S, &file, &line, nullptr)) {
        std::cout << "[TRACE] " << file << ":" << line << "\n";
    }
    
    behl::debug_step_into(S);  // Continue tracing
}

// Enable line-by-line tracing
behl::debug_enable(S, true);
behl::debug_set_event_callback(S, trace_callback);
behl::debug_pause(S);  // Start tracing from first line
```

### Performance Profiling

```cpp
#include <chrono>
#include <unordered_map>

std::unordered_map<int, std::chrono::microseconds> line_times;
std::chrono::time_point<std::chrono::steady_clock> last_time;

void profile_callback(behl::State* S, behl::DebugEvent event) {
    auto now = std::chrono::steady_clock::now();
    
    const char* file;
    int line;
    
    if (behl::debug_get_location(S, &file, &line, nullptr)) {
        if (last_time.time_since_epoch().count() > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                now - last_time);
            line_times[line] += elapsed;
        }
        
        last_time = now;
    }
    
    behl::debug_step_into(S);
}

void print_profile() {
    std::cout << "Line profiling results:\n";
    for (const auto& [line, time] : line_times) {
        std::cout << "Line " << line << ": " 
                  << time.count() << " μs\n";
    }
}
```

---

## Important Notes

1. **Performance Impact**: Debugging has overhead - disable in production
2. **Thread Safety**: Debug API is not thread-safe
3. **Callback Rules**: Event callbacks should call a stepping/continue function
4. **Infinite Loops**: Calling `debug_step_into()` in callback enables tracing
5. **Breakpoint Persistence**: Breakpoints persist until explicitly removed
6. **Location Accuracy**: Column information may not always be available

---

## Debug vs Release Builds

```cpp
#ifdef BEHL_DEBUG_BUILD
    behl::debug_enable(S, true);
    behl::debug_set_event_callback(S, my_debugger);
#endif
```

---

## Integration with IDEs

For IDE integration, consider:
- Implement DAP (Debug Adapter Protocol) server
- Use debug API to control execution
- Map breakpoints to line numbers
- Provide variable inspection via stack API
- Support hot reload with `load_string()`

---

## Next Steps

- Learn about [Error Handling](error-handling) for `pcall` and error recovery
- See [Stack Operations](stack-operations) for inspecting values during debugging
- Review the complete [API Reference](api-reference)
