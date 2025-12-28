---
layout: default
title: process
parent: Standard Library
nav_order: 9
---

# process

The `process` module provides cross-platform process spawning and management capabilities. It supports spawning child processes, capturing I/O, waiting for completion, and sending signals. It is **not** loaded by default and must be explicitly loaded.

## Loading the Module

### C++ API

```cpp
#include <behl/behl.hpp>

behl::State* S = behl::new_state();
behl::load_stdlib(S);           // Load standard library
behl::load_lib_process(S);      // Load process module
```

### Behl Script

```javascript
// Import the process module
const process = import("process");
let proc = process.spawn("echo", {"hello"});
```

---

## Process Spawning

### process.spawn(command, args, options)

Spawn a new child process asynchronously and return a process handle.

**Parameters:**
- `command` (string) - Command to execute (searches PATH)
- `args` (array, optional) - Array of string arguments
- `options` (table, optional) - Spawn options:
  - `cwd` (string) - Working directory for the process
  - `stdin` (string) - Mode for stdin: `"inherit"`, `"pipe"`, `"null"` (default: `"inherit"`)
  - `stdout` (string) - Mode for stdout: `"inherit"`, `"pipe"`, `"null"` (default: `"inherit"`)
  - `stderr` (string) - Mode for stderr: `"inherit"`, `"pipe"`, `"null"` (default: `"inherit"`)
  - `env` (table) - Environment variables as key-value pairs (replaces parent environment if provided)

**Returns:**
- Process handle object on success
- `false, error_message` on failure

**I/O Modes:**
- `"inherit"` - Child inherits parent's stdio (default)
- `"pipe"` - Create pipe for reading/writing
- `"null"` - Redirect to `/dev/null` (Unix) or `NUL` (Windows)

**Example:**

```javascript
// Basic spawn
let proc = process.spawn("echo", {"hello world"});
proc:wait();

// With pipes for output capture
let proc = process.spawn("ls", {"-la"}, {
    stdout = "pipe",
    stderr = "pipe"
});

while proc:is_running() {
    let output = proc:read();
    if output {
        print(output);
    }
}

// With custom working directory
let proc = process.spawn("git", {"status"}, {
    cwd = "/path/to/repo",
    stdout = "pipe"
});

// With stdin pipe for input
let proc = process.spawn("cat", {}, {
    stdin = "pipe",
    stdout = "pipe"
});
proc:write("Hello from stdin\n");
let output = proc:read();

// With custom environment variables
let proc = process.spawn("node", {"app.js"}, {
    env = {
        NODE_ENV = "production",
        PORT = "3000",
        API_KEY = "secret123"
    }
});
```

---

### process.exec(command, args, options)

Execute a command and wait for it to complete, capturing all output.

**Parameters:**
- `command` (string) - Command to execute
- `args` (array, optional) - Array of string arguments
- `options` (table, optional) - Options:
  - `cwd` (string) - Working directory for the process
  - `env` (table) - Environment variables as key-value pairs (replaces parent environment if provided)

**Returns:**
- Result table on success with fields:
  - `stdout` (string) - Complete stdout output
  - `stderr` (string) - Complete stderr output
  - `exitcode` (integer) - Process exit code
- `false, error_message` on failure

**Note:** `stdout` and `stderr` are automatically set to `"pipe"` mode.

**Example:**

```javascript
// Execute and capture output
let result = process.exec("echo", {"hello"});
print(result.stdout);      // "hello\n"
print(result.exitcode);    // 0

// With error handling
let result = process.exec("git", {"status"});
if result {
    if result.exitcode == 0 {
        print("Git status:");
        print(result.stdout);
    } else {
        print("Error:");
        print(result.stderr);
    }
} else {
    print("Failed to execute");
}

// With custom directory

// With environment variables
let result = process.exec("printenv", {"MY_VAR"}, {
    env = {MY_VAR = "test_value"}
});
print(result.stdout);  // "test_value\n"
let result = process.exec("ls", {"-la"}, {cwd = "/tmp"});
```

---

## Process Handle Methods

Process handles returned by `process.spawn()` have the following methods:

### proc:get_pid()

Get the process ID (PID) of the child process.

**Returns:**
- Process ID (integer) on success
- `false, error_message` if handle is invalid

**Example:**

```javascript
let proc = process.spawn("sleep", {"5"});
let pid = proc:get_pid();
print("Process PID: " + pid);
```

---

### proc:is_running()

Check if the process is still running.

**Returns:**
- `true` if process is running
- `false` if process has exited or handle is invalid

**Example:**

```javascript
let proc = process.spawn("sleep", {"2"});
print(proc:is_running());  // true

// Wait for process to complete
proc:wait();
print(proc:is_running());  // false
```

---

### proc:wait()

Wait for the process to complete and return its exit code. This call blocks.

**Returns:**
- Exit code (integer) - The exit code returned by the process (meaning is application-specific, but by convention `0` typically indicates success)
- `-1` if handle is invalid or wait failed

**Exit Code Notes:**
- The meaning of exit codes is defined by the application itself
- By convention, `0` often means success, but not always
- Unix: Exit codes `128+N` typically indicate termination by signal N
- Check the specific application's documentation for exit code meanings

**Example:**

```javascript
let proc = process.spawn("echo", {"test"});
let exitcode = proc:wait();
print("Process exited with code: " + exitcode);

// By convention, 0 usually means success
if exitcode == 0 {
    print("Likely succeeded");
} else {
    print("Exited with code: " + exitcode);
}
```

---

### proc:kill()

Forcefully terminate the process.

**Returns:**
- `true` if signal was sent successfully
- `false, error_message` if handle is invalid

**Note:** Uses `SIGKILL` on Unix (cannot be caught) or `TerminateProcess` on Windows.

**Example:**

```javascript
let proc = process.spawn("sleep", {"100"});
// ... do some work ...
proc:kill();  // Force kill the process
```

---

### proc:signal(signal)

Send a signal to the process (Unix only, limited support on Windows).

**Parameters:**
- `signal` (integer) - Signal number (use process.SIG* constants)

**Returns:**
- `true` if signal was sent successfully
- `false, error_message` if handle is invalid

**Example:**

```javascript
// Unix/Linux: Graceful termination pattern
let proc = process.spawn("sleep", {"100"});

if process.platform() == "windows" {
    // On Windows, most signals immediately kill the process
    // Only SIGINT and SIGBREAK attempt graceful shutdown (console apps only)
    proc:signal(process.SIGINT);  // Try graceful first
    // ... poll or wait ...
    if proc:is_running() {
        proc:kill();  // Force kill
    }
} else {
    // On Unix: SIGTERM allows graceful shutdown
    proc:signal(process.SIGTERM);
    // ... poll or wait ...
    if proc:is_running() {
        proc:signal(process.SIGKILL);  // Force kill
    }
}
```

---

### proc:write(data)

Write data to the process's stdin. Requires `stdin = "pipe"` in spawn options.

**Parameters:**
- `data` (string) - Data to write

**Returns:**
- Number of bytes written (integer)
- `false, error_message` if handle is invalid or stdin not piped

**Example:**

```javascript
let proc = process.spawn("cat", {}, {
    stdin = "pipe",
    stdout = "pipe"
});

proc:write("Line 1\n");
proc:write("Line 2\n");
// Close stdin by closing the handle or calling proc:close()
```

---

### proc:read(max_bytes)

Read data from the process's stdout. Requires `stdout = "pipe"` in spawn options.

**Parameters:**
- `max_bytes` (integer, optional) - Maximum bytes to read (default: 4096)

**Returns:**
- String with output data (empty string if no data available)
- `false, error_message` if handle is invalid or stdout not piped

**Note:** This is a non-blocking read. It returns immediately even if no data is available.

**Example:**

```javascript
let proc = process.spawn("echo", {"test"}, {stdout = "pipe"});

// Poll for output
while proc:is_running() {
    let output = proc:read();
    if output {
        print("Got: " + output);
    }
}

// Read remaining output after process exits
let remaining = proc:read(8192);
```

---

### proc:read_err(max_bytes)

Read data from the process's stderr. Requires `stderr = "pipe"` in spawn options.

**Parameters:**
- `max_bytes` (integer, optional) - Maximum bytes to read (default: 4096)

**Returns:**
- String with error output (empty string if no data available)
- `false, error_message` if handle is invalid or stderr not piped

**Example:**

```javascript
let proc = process.spawn("bash", {"-c", "echo error >&2"}, {
    stderr = "pipe"
});

proc:wait();
let errors = proc:read_err();
print("Errors: " + errors);
```

---

### proc:close()

Close the process handle and release associated resources (pipes, handles).

**Returns:**
- `true` on success

**Note:** Process handles are automatically closed when garbage collected, but explicit closing is recommended.

**Example:**

```javascript
let proc = process.spawn("echo", {"test"});
proc:wait();
proc:close();  // Explicitly close
```

---

## Environment Variables

### Custom Environments

You can provide custom environment variables to spawned processes using the `env` option. When `env` is provided, it **replaces** the parent's environment entirely - the child does not inherit any environment variables from the parent unless explicitly included.

**Important:** If you omit the `env` option, the child inherits the parent's full environment.

**Example:**

```javascript
// Child gets ONLY these variables (no PATH, HOME, etc. from parent)
let proc = process.spawn("node", {"script.js"}, {
    env = {
        NODE_ENV = "production",
        API_KEY = "secret123"
    }
});
```

### Inheriting and Adding Variables

To inherit the parent environment and add/override specific variables, you would need to explicitly copy parent variables (behl doesn't currently provide direct access to parent environment, so you'd set all needed variables):

```javascript
// Set all needed environment variables explicitly
let proc = process.spawn("bash", {"-c", "echo $MY_VAR"}, {
    env = {
        PATH = "/usr/local/bin:/usr/bin:/bin",  // Need to set PATH explicitly
        HOME = "/home/user",
        MY_VAR = "custom_value"
    }
});
```

### Platform-Specific Behavior

- **Windows**: Environment variable names are case-insensitive but preserved as provided
- **Unix/Linux/macOS**: Environment variable names are case-sensitive

**Example:**

```javascript
let platform = process.platform();

let proc;
if platform == "windows" {
    proc = process.spawn("cmd", {"/c", "echo %MY_VAR%"}, {
        env = {MY_VAR = "hello"}
    });
} else {
    proc = process.spawn("sh", {"-c", "echo $MY_VAR"}, {
        env = {MY_VAR = "hello"}
    });
}
```

---

## Platform Information

### process.platform()

Get the current platform identifier.

**Returns:**
- `"windows"` on Windows
- `"linux"` on Linux
- `"darwin"` on macOS

**Example:**

```javascript
let platform = process.platform();
if platform == "windows" {
    print("Running on Windows");
} else {
    print("Running on Unix-like system");
}
```

---

## Signal Constants (Unix)

The process module provides signal constants for use with `proc:signal()`. These are primarily for Unix-like systems. Windows has limited support.

### Common Signals

| Constant | Value | Description |
|----------|-------|-------------|
| `process.SIGINT` | 2 | Interrupt (Ctrl+C) |
| `process.SIGTERM` | 15 | Termination request (graceful) |
| `process.SIGKILL` | 9 | Force kill (cannot be caught) |
| `process.SIGHUP` | 1 | Hangup (terminal closed) |
| `process.SIGQUIT` | 3 | Quit with core dump |

### All Unix Signals

- `SIGHUP` - Hangup
- `SIGINT` - Interrupt (Ctrl+C)
- `SIGQUIT` - Quit
- `SIGILL` - Illegal instruction
- `SIGTRAP` - Trace trap
- `SIGABRT` - Abort
- `SIGBUS` - Bus error
- `SIGFPE` - Floating point exception
- `SIGKILL` - Kill (cannot be caught)
- `SIGUSR1` - User-defined signal 1
- `SIGSEGV` - Segmentation fault
- `SIGUSR2` - User-defined signal 2
- `SIGPIPE` - Broken pipe
- `SIGALRM` - Alarm clock
- `SIGTERM` - Termination (default)
- `SIGCHLD` - Child status changed
- `SIGCONT` - Continue if stopped
- `SIGSTOP` - Stop (cannot be caught)
- `SIGTSTP` - Terminal stop (Ctrl+Z)
- `SIGTTIN` - Terminal input
- `SIGTTOU` - Terminal output
- `SIGURG` - Urgent condition on socket
- `SIGXCPU` - CPU time limit exceeded
- `SIGXFSZ` - File size limit exceeded
- `SIGWINCH` - Window size change

### Windows Support

On Windows, only the following signals are available:
- `SIGINT` - Interrupt
- `SIGTERM` - Termination
- `SIGABRT` - Abort
- `SIGFPE` - Floating point exception
- `SIGSEGV` - Segmentation violation
- `SIGILL` - Illegal instruction
- `SIGBREAK` - Ctrl+Break (Windows-specific)
- `SIGKILL` - Convention value (9), not a real Windows signal

**Windows Signal Emulation:**
- `SIGINT` (2): Attempts to send `CTRL_C_EVENT` to the process console. If this fails, falls back to terminating the process.
- `SIGBREAK` (21): Attempts to send `CTRL_BREAK_EVENT` to the process console. If this fails, falls back to terminating the process.
- **`SIGTERM` and all other signals**: Immediately and forcefully terminate the process using `TerminateProcess()` with exit code `128 + signal_number` (POSIX convention). The process cannot catch or handle the termination.

**Note:** Console events (CTRL_C, CTRL_BREAK) only work if the target process has a console and shares the console group with the parent. Most GUI applications and detached processes will be forcefully terminated. There is no graceful equivalent to POSIX SIGTERM on Windows for general processes.

**Example:**

```javascript
let proc = process.spawn("long_running_task", {});

// Try graceful shutdown first
proc:signal(process.SIGTERM);
os.sleep(2000);

// Force kill if still running
if proc:is_running() {
    proc:signal(process.SIGKILL);
}
```

---

## Complete Examples

### Example 1: Execute Command and Check Output

```javascript
let result = process.exec("git", {"--version"});
if result && result.exitcode == 0 {
    print("Git version: " + result.stdout);
} else {
    print("Git not found or error occurred");
}
```

### Example 2: Interactive Process with Pipes

```javascript
// Start a process with stdin/stdout pipes
let proc = process.spawn("python3", {"-i"}, {
    stdin = "pipe",
    stdout = "pipe",
    stderr = "pipe"
});

// Send commands
proc:write("print('Hello from Python')\n");
proc:write("print(2 + 2)\n");
proc:write("exit()\n");

// Read output (poll until no more data)
while true {
    let output = proc:read(4096);
    if output == "" {
        break;
    }
    print(output);
}

proc:wait();
proc:close();
```

### Example 3: Run Multiple Commands

```javascript
let commands = {
    {"echo", "Starting..."},
    {"ls", "-la"},
    {"echo", "Done"}
};

for i = 0, #commands - 1 {
    let result = process.exec(commands[i][0], {commands[i][1]});
    if result {
        print(result.stdout);
    }
}
```

### Example 4: Background Process with Monitoring

```javascript
// Start a long-running process
let proc = process.spawn("./server", {"--port", "8080"}, {
    stdout = "pipe",
    stderr = "pipe"
});

print("Server started with PID: " + proc:get_pid());

// Monitor for a while
let iterations = 50;
for i = 0, iterations - 1 {
    if !proc:is_running() {
        print("Server exited unexpectedly!");
        let stderr = proc:read_err();
        print("Error: " + stderr);
        break;
    }
    
    let output = proc:read();
    if output {
        print("[SERVER] " + output);
    }
}

// Clean shutdown
print("Shutting down server...");

if process.platform() == "windows" {
    // On Windows, try CTRL_C event for console apps
    proc:signal(process.SIGINT);
    // Brief wait to see if graceful shutdown worked
    for i = 0, 10 {
        if !proc:is_running() {
            break;
        }
    }
} else {
    // On Unix, SIGTERM allows graceful shutdown
    proc:signal(process.SIGTERM);
    // Brief wait for graceful shutdown
    for i = 0, 10 {
        if !proc:is_running() {
            break;
        }
    }
}

if proc:is_running() {
    print("Force killing...");
    proc:kill();
}

proc:close();
```

### Example 5: Platform-Specific Commands

```javascript
let platform = process.platform();
let result;

if platform == "windows" {
    result = process.exec("cmd", {"/c", "dir"});
} else {
    result = process.exec("ls", {"-l"});
}

if result {
    print(result.stdout);
}
```

---

## Implementation Notes

### Platform Differences

- **Unix/Linux/macOS**: Uses `posix_spawn()` for efficient process creation
  - Supports full signal handling
  - Working directory (`cwd`) supported via `posix_spawn_file_actions_addchdir_np()`
  - Non-blocking I/O on pipes
  
- **Windows**: Uses `CreateProcess` API
  - Limited signal support (mainly process termination)
  - Full working directory support
  - Anonymous pipes for I/O

### Resource Management

- Process handles are automatically cleaned up when garbage collected
- Explicit `proc:close()` is recommended to free resources early
- Unclosed pipes may block the child process if buffers fill

### Best Practices

1. **Always check return values**: Both `spawn()` and `exec()` can fail
2. **Close handles explicitly**: Don't rely on garbage collection for long-running scripts
3. **Handle both stdout and stderr**: Capture both streams when using pipes
4. **Use timeouts**: Implement timeouts when waiting for processes
5. **Graceful shutdown**: Send `SIGTERM` before `SIGKILL` on Unix
6. **Platform detection**: Use `process.platform()` for platform-specific behavior

---

## Error Handling

All process operations can fail and return error information:

```javascript
// Spawn failure
let proc = process.spawn("nonexistent_command", {});
if !proc {
    print("Failed to spawn process");
}

// Read from non-piped stream
let proc = process.spawn("echo", {"test"});
let output = proc:read();  // Returns false, "stdout not piped"

// Invalid handle usage
let proc = process.spawn("echo", {"test"});
proc:close();
let pid = proc:get_pid();  // Returns false, "process handle is invalid"
```

---

## See Also

- [os module](os.md) - Operating system interface (time, clock, exit)
- [fs module](fs.md) - File system operations
- [Standard Library](../standard-library.md) - Overview of all standard modules
