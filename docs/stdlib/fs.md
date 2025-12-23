---
layout: default
title: fs
parent: Standard Library
nav_order: 8
---

# fs

The `fs` module provides cross-platform filesystem operations. It is **not** loaded by default and must be explicitly loaded.

## Loading the Module

### C++ API

```cpp
#include <behl/behl.hpp>

behl::State* S = behl::new_state();
behl::load_stdlib(S, true);       // Load standard library
behl::load_lib_fs(S, true);       // Load filesystem module as global

// fs module is now available as global: fs.read("file.txt")

// Or load without global access:
behl::load_lib_fs(S, false);      // Requires explicit import()
```

### Behl Script

```javascript
// After load_lib_fs(S, true) is called in C++
let content = fs.read("file.txt");

// Or with load_lib_fs(S, false):
let fs = import("fs");
let content = fs.read("file.txt");
```

---

## File Operations

### fs.open(path, mode)

Open a file and return a file handle for reading/writing.

**Parameters:**
- `path` - File path
- `mode` - Open mode:
  - `"r"` - Read only
  - `"w"` - Write (truncate existing)
  - `"a"` - Append
  - `"r+"` - Read and write
  - `"w+"` - Read and write (truncate existing)
  - `"a+"` - Read and append

**Returns:** File handle on success, `false, error` on failure

```javascript
let file, err = fs.open("data.txt", "r");
if (!file) {
    print("Open failed: " + err);
    return;
}

let content = file:read();
file:close();
```

**File Handle Methods:**

#### file:read(size)

Read from file.

**Parameters:**
- `size` - Number of bytes to read (required)

**Returns:** `content, bytes_read` - String content and actual number of bytes read

```javascript
let file = fs.open("data.txt", "r");
let chunk, bytes = file:read(100);  // Read up to 100 bytes
print("Read " + tostring(bytes) + " bytes");
file:close();
```

#### file:write(data)

Write string to file.

**Parameters:**
- `data` - String to write

**Returns:** `true` on success, `false, error` on failure

```javascript
let file = fs.open("output.txt", "w");
file:write("Hello, ");
file:write("World!\n");
file:close();
```

#### file:seek(whence, [offset])

Move file position.

**Parameters:**
- `whence` - Position reference: `"set"` (beginning), `"cur"` (current), `"end"` (end)
- `offset` (optional) - Number of bytes to move (default: 0)

**Returns:** New position in file

```javascript
let file = fs.open("data.txt", "r");
file:seek("set", 100);  // Move to byte 100
let pos = file:seek("cur", 0);  // Get current position
file:seek("end", 0);    // Move to end
file:close();
```

#### file:close()

Close the file handle.

**Returns:** `true`

```javascript
let file = fs.open("data.txt", "r");
// ... use file ...
file:close();
```

**Note:** File handles are automatically closed by the garbage collector, but it's good practice to close them explicitly.

### fs.read(path)

Read entire file contents as string (convenience function).

**Returns:** `content` on success, `false, error` on failure

```javascript
let content, err = fs.read("config.txt");
if (!content) {
    print("Error: " + err);
} else {
    print("File contents: " + content);
}
```

### fs.write(path, content)

Write string to file (overwrites existing, convenience function).

**Returns:** `true` on success, `false, error` on failure

```javascript
let success, err = fs.write("output.txt", "Hello, World!");
if (!success) {
    print("Write failed: " + err);
}
```

### fs.append(path, content)

Append string to file (convenience function).

**Returns:** `true` on success, `false, error` on failure

```javascript
fs.append("log.txt", "New log entry\n");
```

### fs.exists(path)

Check if file or directory exists.

**Returns:** `boolean`

```javascript
if (fs.exists("config.txt")) {
    print("Config file found");
}
```

### fs.remove(path)

Delete file or empty directory.

**Returns:** `true` if removed, `false` if doesn't exist, `false, error` on failure

```javascript
let removed, err = fs.remove("temp.txt");
if (removed) {
    print("File deleted");
}
```

### fs.rename(oldpath, newpath)

Rename or move file.

**Returns:** `true` on success, `false, error` on failure

```javascript
let success, err = fs.rename("old.txt", "new.txt");
if (!success) {
    print("Rename failed: " + err);
}
```

### fs.copy(source, dest)

Copy file (overwrites destination).

**Returns:** `true` on success, `false, error` on failure

```javascript
fs.copy("template.txt", "output.txt");
```

---

## Directory Operations

### fs.mkdir(path)

Create directory (creates parent directories if needed).

**Returns:** `true` if created, `false` if already exists, `false, error` on failure

```javascript
fs.mkdir("output/data/reports");  // Creates all intermediate dirs
```

### fs.rmdir(path)

Remove empty directory.

**Returns:** `true` if removed, `false` if doesn't exist, `false, error` on failure

```javascript
let removed, err = fs.rmdir("temp_dir");
if (!removed && err) {
    print("Error: " + err);
}
```

### fs.list(path)

List directory contents (returns 0-indexed table of filenames).

**Returns:** `table` on success, `null, error` on failure

```javascript
let entries, err = fs.list(".");
if (entries == null) {
    print("Error: " + err);
} else {
    for (let i = 0; i < #entries; i++) {
        print(entries[i]);
    }
}
```

### fs.cwd()

Get current working directory.

**Returns:** `string` path, or `null, error` on failure

```javascript
let dir = fs.cwd();
print("Current directory: " + dir);
```

### fs.chdir(path)

Change current working directory.

**Returns:** `true` on success, `false, error` on failure

```javascript
let success, err = fs.chdir("../parent");
if (!success) {
    print("chdir failed: " + err);
}
```

---

## File Information

### fs.is_file(path)

Check if path is a regular file.

**Returns:** `boolean`

```javascript
if (fs.is_file("data.txt")) {
    print("It's a file");
}
```

### fs.is_dir(path)

Check if path is a directory.

**Returns:** `boolean`

```javascript
if (fs.is_dir("output")) {
    print("It's a directory");
}
```

### fs.size(path)

Get file size in bytes.

**Returns:** `number` on success, `null, error` on failure

```javascript
let size, err = fs.size("data.bin");
if (size != null) {
    print("File is " + tostring(size) + " bytes");
}
```

---

## Path Operations

### fs.join(path1, path2, ...)

Join path components (handles separators correctly).

**Returns:** `string` combined path

```javascript
let path = fs.join("home", "user", "documents", "file.txt");
// Windows: "home\user\documents\file.txt"
// Unix:    "home/user/documents/file.txt"
```

### fs.absolute(path)

Convert relative path to absolute.

**Returns:** `string` absolute path, or `null, error` on failure

```javascript
let abs, err = fs.absolute("../data/file.txt");
print("Absolute: " + abs);
```

### fs.dirname(path)

Get directory part of path.

**Returns:** `string` directory

```javascript
let dir = fs.dirname("/home/user/file.txt");
// dir = "/home/user"
```

### fs.basename(path)

Get filename with extension.

**Returns:** `string` filename

```javascript
let name = fs.basename("/home/user/file.txt");
// name = "file.txt"
```

### fs.stem(path)

Get filename without extension.

**Returns:** `string` stem

```javascript
let stem = fs.stem("/home/user/file.txt");
// stem = "file"
```

### fs.extension(path)

Get file extension (including dot).

**Returns:** `string` extension

```javascript
let ext = fs.extension("/home/user/file.txt");
// ext = ".txt"
```

---

## Complete Examples

### Read Configuration File

```javascript
function loadConfig(path) {
    if (!fs.exists(path)) {
        return {port = 8080, host = "localhost"};  // Defaults
    }
    
    let content, err = fs.read(path);
    if (content == null) {
        print("Warning: " + err);
        return {port = 8080, host = "localhost"};
    }
    
    // Parse content (simplified)
    return parseConfig(content);
}

let config = loadConfig("config.txt");
```

### Process All Files in Directory

```javascript
function processDirectory(dir) {
    let entries, err = fs.list(dir);
    if (entries == null) {
        print("Error listing directory: " + err);
        return;
    }
    
    for (let i = 0; i < #entries; i++) {
        let path = fs.join(dir, entries[i]);
        
        if (fs.is_file(path)) {
            print("Processing: " + path);
            let content, err = fs.read(path);
            if (content != null) {
                // Process file content
                processFile(content);
            }
        }
    }
}

processDirectory("data");
```

### Safe File Operations with Defer

```javascript
function appendLog(message) {
    let log_path = fs.join(fs.cwd(), "app.log");
    
    let success, err = fs.append(log_path, message + "\n");
    if (!success) {
        print("Failed to write log: " + err);
    }
}

function withTempFile(callback) {
    let temp_path = fs.join(fs.cwd(), "temp.txt");
    
    fs.write(temp_path, "temporary data");
    defer fs.remove(temp_path);  // Cleanup on exit
    
    callback(temp_path);
}
```

### Create Directory Structure

```javascript
function ensureDirectory(path) {
    if (fs.exists(path)) {
        if (!fs.is_dir(path)) {
            print("Error: " + path + " exists but is not a directory");
            return false;
        }
        return true;
    }
    
    let success, err = fs.mkdir(path);
    if (!success) {
        print("Failed to create directory: " + err);
        return false;
    }
    
    return true;
}

ensureDirectory("output/reports/2024");
```

### File Backup

```javascript
function backupFile(path) {
    if (!fs.exists(path)) {
        print("File not found: " + path);
        return false;
    }
    
    let dir = fs.dirname(path);
    let name = fs.stem(path);
    let ext = fs.extension(path);
    
    let backup_path = fs.join(dir, name + ".backup" + ext);
    
    let success, err = fs.copy(path, backup_path);
    if (!success) {
        print("Backup failed: " + err);
        return false;
    }
    
    print("Backed up to: " + backup_path);
    return true;
}

backupFile("important.txt");
```

---

## Error Handling

All operations that can fail return `null` or `false` plus an error message as a second return value:

```javascript
// Pattern 1: Check for null
let content, err = fs.read("file.txt");
if (content == null) {
    print("Error: " + err);
    return;
}

// Pattern 2: Check boolean
let success, err = fs.write("output.txt", data);
if (!success) {
    print("Error: " + err);
    return;
}

// Pattern 3: Use pcall for critical operations
let ok, result, err = pcall(function() {
    return fs.read("critical.txt");
});

if (!ok) {
    print("Fatal error: " + result);
} elseif (result == null) {
    print("Read error: " + err);
}
```

---

## Platform Notes

- **Path Separators**: Automatically handled (`/` on Unix, `\` on Windows)
- **Permissions**: Some operations may require elevated privileges
- **Symbolic Links**: Followed by default in most operations
- **Thread Safety**: Not thread-safe - serialize filesystem access
- **Encodings**: Strings are UTF-8, but underlying filesystem encoding varies

## See Also

- [os Module](os) - Operating system interface
- [Defer Statement](../defer) - Cleanup with defer
- [Error Handling](../language-reference#error-handling) - pcall and error recovery
