---
layout: default
title: Examples
parent: Guides
nav_order: 2
---

# Examples
{: .no_toc }

Practical code examples and tutorials for Behl.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Basic Examples

### Hello World

```cpp
print("Hello, World!");
```

### Variables and Types

```cpp
// Variables
let name = "Alice";
let age = 30;
let height = 5.7;
let isStudent = false;

// Constants
const PI = 3.14159;
const MAX_SIZE = 100;

// Type checking
print(typeof(name));    // "string"
print(typeof(age));     // "integer"
print(typeof(height));  // "number"
```

### Simple Calculator

```cpp
function add(a, b) {
    return a + b;
}

function subtract(a, b) {
    return a - b;
}

function multiply(a, b) {
    return a * b;
}

function divide(a, b) {
    if (b == 0) {
        error("Division by zero!");
    }
    return a / b;
}

print(add(5, 3));        // 8
print(subtract(10, 4));  // 6
print(multiply(7, 6));   // 42
print(divide(20, 4));    // 5
```

---

## Functions and Closures

### Fibonacci

```cpp
function fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

for (let i = 0; i < 10; i++) {
    print("fib(" + tostring(i) + ") = " + tostring(fibonacci(i)));
}
```

### Factorial

```cpp
function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

print(factorial(5));  // 120
```

### Counter with Closure

```cpp
function makeCounter(initial) {
    let count = initial;
    
    return function() {
        count = count + 1;
        return count;
    };
}

let counter1 = makeCounter(0);
let counter2 = makeCounter(10);

print(counter1());  // 1
print(counter1());  // 2
print(counter2());  // 11
print(counter1());  // 3
print(counter2());  // 12
```

### Higher-Order Functions

```cpp
function map(func, arr) {
    let result = {};
    for (let i = 0; i < rawlen(arr); i++) {
        result[i] = func(arr[i]);
    }
    return result;
}

function square(x) {
    return x * x;
}

let numbers = {1, 2, 3, 4, 5};
let squares = map(square, numbers);

for (let i = 0; i < rawlen(squares); i++) {
    print(squares[i]);  // 1, 4, 9, 16, 25
}
```

---

## Working with Tables

### Array Operations

```cpp
// Create array
let arr = {10, 20, 30, 40, 50};

// Access elements
print(arr[0]);  // 10
print(arr[4]);  // 50

// Modify
arr[2] = 99;

// Length
let len = rawlen(arr);
print("Length: " + tostring(len));

// Iterate
for (let i = 0; i < rawlen(arr); i++) {
    print("arr[" + tostring(i) + "] = " + tostring(arr[i]));
}
```

### Dictionary Operations

```cpp
// Create dictionary
let person = {
    ["name"] = "Alice",
    ["age"] = 30,
    ["city"] = "Seattle"
};

// Access
print(person["name"]);  // "Alice"

// Modify
person["age"] = 31;
person["email"] = "alice@example.com";

// Iterate with pairs
for (key, value in pairs(person)) {
    print(key + ": " + tostring(value));
}
```

### Nested Tables

```cpp
let company = {
    ["name"] = "TechCorp",
    ["employees"] = {
        {["name"] = "Alice", ["role"] = "Engineer"},
        {["name"] = "Bob", ["role"] = "Designer"},
        {["name"] = "Carol", ["role"] = "Manager"}
    }
};

let employees = company["employees"];
for (let i = 0; i < rawlen(employees); i++) {
    let emp = employees[i];
    print(emp["name"] + " - " + emp["role"]);
}
```

---

## Control Flow Examples

### FizzBuzz

```cpp
for (let i = 1; i <= 30; i++) {
    if (i % 15 == 0) {
        print("FizzBuzz");
    } elseif (i % 3 == 0) {
        print("Fizz");
    } elseif (i % 5 == 0) {
        print("Buzz");
    } else {
        print(i);
    }
}
```

### Prime Number Checker

```cpp
function isPrime(n) {
    if (n <= 1) {
        return false;
    }
    if (n == 2) {
        return true;
    }
    if (n % 2 == 0) {
        return false;
    }
    
    let i = 3;
    while (i * i <= n) {
        if (n % i == 0) {
            return false;
        }
        i = i + 2;
    }
    return true;
}

for (let n = 0; n < 20; n++) {
    if (isPrime(n)) {
        print(tostring(n) + " is prime");
    }
}
```

### Binary Search

```cpp
function binarySearch(arr, target) {
    let left = 0;
    let right = rawlen(arr) - 1;
    
    while (left <= right) {
        let mid = (left + right) / 2;
        let midVal = arr[mid];
        
        if (midVal == target) {
            return mid;
        } elseif (midVal < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return -1;  // Not found
}

let sorted = {1, 3, 5, 7, 9, 11, 13, 15};
let index = binarySearch(sorted, 9);
print("Found at index: " + tostring(index));  // 4
```

---

## Using the Standard Library

### Math Module

```cpp
const math = import("math");

// Constants
print("PI = " + tostring(math.PI));

// Basic operations
print(math.sqrt(16));      // 4
print(math.pow(2, 10));    // 1024
print(math.abs(-42));      // 42

// Trigonometry
let angle = math.PI / 4;
print(math.sin(angle));    // 0.707...
print(math.cos(angle));    // 0.707...

// Rounding
print(math.floor(3.7));    // 3
print(math.ceil(3.2));     // 4
print(math.round(3.5));    // 4
```

### String Module

```cpp
const string = import("string");

let text = "Hello, World!";

print(string.len(text));           // 13
print(string.upper(text));         // "HELLO, WORLD!"
print(string.lower(text));         // "hello, world!"
print(string.reverse("behl"));     // "lheb"

let sub = string.sub(text, 0, 4);
print(sub);  // "Hello"
```

### Table Module

```cpp
const table = import("table");

let arr = {10, 20, 30};

table.insert(arr, 40);        // Append
table.insert(arr, 0, 5);      // Insert at beginning

table.print(arr);              // Debug print

let removed = table.remove(arr, 1);
print("Removed: " + tostring(removed));
```

---

## Error Handling

### Protected Call Example

```cpp
const math = import("math");

function riskyOperation(x) {
    if (x < 0) {
        error("Negative value not allowed!");
    }
    return math.sqrt(x);
}

function safeCall(value) {
    let success, result = pcall(riskyOperation, value);
    
    if (success) {
        print("Result: " + tostring(result));
    } else {
        print("Error: " + result);
    }
}

safeCall(16);   // Result: 4
safeCall(-5);   // Error: Negative value not allowed!
```

### Input Validation

```cpp
function validateAge(age) {
    if (typeof(age) != "integer" && typeof(age) != "number") {
        error("Age must be a number");
    }
    if (age < 0 || age > 150) {
        error("Age must be between 0 and 150");
    }
    return true;
}

function processAge(age) {
    let success, err = pcall(validateAge, age);
    
    if (!success) {
        print("Validation failed: " + err);
        return false;
    }
    
    print("Age " + tostring(age) + " is valid");
    return true;
}

processAge(25);      // Valid
processAge(-5);      // Validation failed
processAge("abc");   // Validation failed
```

---

## Metatables

### Custom toString

```cpp
function createPerson(name, age) {
    let person = {
        ["name"] = name,
        ["age"] = age
    };
    
    let mt = {
        ["__tostring"] = function(p) {
            return p["name"] + " (" + tostring(p["age"]) + ")";
        }
    };
    
    setmetatable(person, mt);
    return person;
}

let alice = createPerson("Alice", 30);
print(tostring(alice));  // "Alice (30)"
```

### Custom Arithmetic

```cpp
function createVector(x, y) {
    let vec = {["x"] = x, ["y"] = y};
    
    let mt = {
        ["__add"] = function(a, b) {
            return createVector(a["x"] + b["x"], a["y"] + b["y"]);
        },
        ["__tostring"] = function(v) {
            return "(" + tostring(v["x"]) + ", " + tostring(v["y"]) + ")";
        }
    };
    
    setmetatable(vec, mt);
    return vec;
}

let v1 = createVector(1, 2);
let v2 = createVector(3, 4);
let v3 = v1 + v2;  // Uses __add metamethod

print(tostring(v3));  // "(4, 6)"
```

---

## Algorithms

### Bubble Sort

```cpp
function bubbleSort(arr) {
    let n = rawlen(arr);
    
    for (let i = 0; i < n - 1; i++) {
        for (let j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                // Swap
                let temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

let data = {64, 34, 25, 12, 22, 11, 90};
bubbleSort(data);

for (let i = 0; i < rawlen(data); i++) {
    print(data[i]);
}
```

### Quick Sort

```cpp
function partition(arr, low, high) {
    let pivot = arr[high];
    let i = low - 1;
    
    for (let j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i = i + 1;
            let temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    
    let temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    
    return i + 1;
}

function quickSort(arr, low, high) {
    if (low < high) {
        let pi = partition(arr, low, high);
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

let data = {10, 7, 8, 9, 1, 5};
quickSort(data, 0, rawlen(data) - 1);

for (let i = 0; i < rawlen(data); i++) {
    print(data[i]);
}
```

---

## Practical Examples

### Simple Calculator REPL (Concept)

```cpp
function evaluate(expr) {
    // Simple expression evaluator
    let success, result = pcall(function() {
        return eval_code(expr);
    });
    
    if (success) {
        return result;
    } else {
        return "Error: " + result;
    }
}

// Example usage
print(evaluate("2 + 3 * 4"));     // 14
print(evaluate("sqrt(16)"));      // 4
print(evaluate("sin(PI / 2)"));   // 1
```

### Data Processing

```cpp
// Process a list of users
let users = {
    {["name"] = "Alice", ["age"] = 30, ["active"] = true},
    {["name"] = "Bob", ["age"] = 25, ["active"] = false},
    {["name"] = "Carol", ["age"] = 35, ["active"] = true}
};

// Filter active users
function filterActive(users) {
    let active = {};
    let count = 0;
    
    for (let i = 0; i < rawlen(users); i++) {
        if (users[i]["active"]) {
            active[count] = users[i];
            count = count + 1;
        }
    }
    
    return active;
}

// Calculate average age
function averageAge(users) {
    let sum = 0;
    let count = rawlen(users);
    
    for (let i = 0; i < count; i++) {
        sum = sum + users[i]["age"];
    }
    
    return sum / count;
}

let activeUsers = filterActive(users);
print("Active users: " + tostring(rawlen(activeUsers)));
print("Average age: " + tostring(averageAge(activeUsers)));
```

---

## C++ Integration Examples

### Userdata: Custom File API

This example shows how to create userdata from C++ and use it in Behl scripts.

**C++ Side:**

```cpp
#include <behl/behl.hpp>
#include <cstdio>

// Define userdata type
struct FileHandle {
    FILE* file;
    bool is_open;
};

constexpr uint32_t FileHandle_UID = behl::make_uid("FileHandle");

// Open a file
static int file_open(behl::State* S) {
    auto filename = behl::check_string(S, 0);
    auto mode = behl::check_string(S, 1);
    
    void* ptr = behl::userdata_new(S, sizeof(FileHandle), FileHandle_UID);
    FileHandle* handle = static_cast<FileHandle*>(ptr);
    
    handle->file = fopen(std::string(filename).c_str(), std::string(mode).c_str());
    handle->is_open = (handle->file != nullptr);
    
    if (!handle->is_open) {
        behl::error(S, "Failed to open file");
    }
    
    // Add finalizer for automatic cleanup
    behl::table_new(S);
    behl::push_cfunction(S, file_finalizer);
    behl::table_rawset_field(S, -2, "__gc");
    behl::set_metatable(S, -2);
    
    return 1;
}

// Read a line from file
static int file_read_line(behl::State* S) {
    FileHandle* handle = static_cast<FileHandle*>(
        behl::check_userdata(S, 0, FileHandle_UID)
    );
    
    if (!handle->is_open) {
        behl::push_nil(S);
        return 1;
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), handle->file)) {
        // Remove trailing newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
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
void setup_file_api(behl::State* S) {
    behl::register_function(S, "file_open", file_open);
    behl::register_function(S, "file_read_line", file_read_line);
    behl::register_function(S, "file_close", file_close);
}
```

**Behl Script:**

```js
// Open and read a file
let f = file_open("data.txt", "r");

while (true) {
    let line = file_read_line(f);
    if (line == nil) {
        break;
    }
    print(line);
}

file_close(f);  // Or let GC do it automatically

// Count lines in a file
function count_lines(filename) {
    let f = file_open(filename, "r");
    let count = 0;
    
    while (file_read_line(f) != nil) {
        count = count + 1;
    }
    
    file_close(f);
    return count;
}

print("Lines: " + tostring(count_lines("data.txt")));
```

### Userdata: Vector Math Library

**C++ Side:**

```cpp
#include <behl/behl.hpp>
#include <cmath>

struct Vector2D {
    double x, y;
};

constexpr uint32_t Vector2D_UID = behl::make_uid("Vector2D");

// Create vector
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

// Getters
static int vec2_get_x(behl::State* S) {
    Vector2D* v = static_cast<Vector2D*>(behl::check_userdata(S, 0, Vector2D_UID));
    behl::push_number(S, v->x);
    return 1;
}

static int vec2_get_y(behl::State* S) {
    Vector2D* v = static_cast<Vector2D*>(behl::check_userdata(S, 0, Vector2D_UID));
    behl::push_number(S, v->y);
    return 1;
}

// Addition metamethod
static int vec2_add(behl::State* S) {
    Vector2D* a = static_cast<Vector2D*>(behl::check_userdata(S, 0, Vector2D_UID));
    Vector2D* b = static_cast<Vector2D*>(behl::check_userdata(S, 1, Vector2D_UID));
    
    void* ptr = behl::userdata_new(S, sizeof(Vector2D), Vector2D_UID);
    Vector2D* result = static_cast<Vector2D*>(ptr);
    result->x = a->x + b->x;
    result->y = a->y + b->y;
    
    behl::get_metatable(S, 0);
    behl::set_metatable(S, -2);
    
    return 1;
}

// Magnitude
static int vec2_length(behl::State* S) {
    Vector2D* v = static_cast<Vector2D*>(behl::check_userdata(S, 0, Vector2D_UID));
    double len = std::sqrt(v->x * v->x + v->y * v->y);
    behl::push_number(S, len);
    return 1;
}

// __tostring
static int vec2_tostring(behl::State* S) {
    Vector2D* v = static_cast<Vector2D*>(behl::check_userdata(S, 0, Vector2D_UID));
    std::string str = behl::format("Vector2D({}, {})", v->x, v->y);
    behl::push_string(S, str);
    return 1;
}

void create_vector2d_metatable(behl::State* S) {
    behl::table_new(S);
    
    behl::push_cfunction(S, vec2_add);
    behl::table_rawset_field(S, -2, "__add");
    
    behl::push_cfunction(S, vec2_tostring);
    behl::table_rawset_field(S, -2, "__tostring");
    
    // Store in registry
    behl::table_rawset_field(S, behl::REGISTRY_INDEX, "Vector2D_mt");
}

void setup_vector_api(behl::State* S) {
    create_vector2d_metatable(S);
    behl::register_function(S, "vec2_new", vec2_new);
    behl::register_function(S, "vec2_get_x", vec2_get_x);
    behl::register_function(S, "vec2_get_y", vec2_get_y);
    behl::register_function(S, "vec2_length", vec2_length);
}
```

**Behl Script:**

```js
// Create vectors
let v1 = vec2_new(3.0, 4.0);
let v2 = vec2_new(1.0, 2.0);

// Use operator overloading
let v3 = v1 + v2;
print(v3);  // "Vector2D(4.0, 6.0)"

// Calculate magnitude
let len = vec2_length(v1);
print("Length: " + tostring(len));  // 5.0

// Access components
let x = vec2_get_x(v1);
let y = vec2_get_y(v1);
print("x=" + tostring(x) + ", y=" + tostring(y));

// Physics simulation with vectors
function update_position(pos, velocity, dt) {
    let scaled_vel = vec2_new(
        vec2_get_x(velocity) * dt,
        vec2_get_y(velocity) * dt
    );
    return pos + scaled_vel;
}

let position = vec2_new(0.0, 0.0);
let velocity = vec2_new(10.0, 5.0);
let dt = 0.016;  // 60 FPS

for (let i = 0; i < 10; i++) {
    position = update_position(position, velocity, dt);
    print("Frame " + tostring(i) + ": " + tostring(position));
}
```

---

## More Examples

Check the repository for additional examples:
- `*.behl` files in the root directory
- Unit tests in `tests/` for advanced usage patterns
