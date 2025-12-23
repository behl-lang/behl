---
layout: default
title: Metatables
parent: Language
nav_order: 8
---

# Metatables
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

Metatables allow you to customize table behavior by defining special methods (metamethods) that override default operations like arithmetic, comparison, indexing, and more.

---

## Setting Metatables

### `setmetatable(table, metatable)`

Attaches a metatable to a table:

```cpp
let t = {};
let mt = {
    __tostring = function(obj) {
        return "Custom Table";
    }
};

setmetatable(t, mt);
print(tostring(t));  // "Custom Table"
```

### `getmetatable(table)`

Retrieves a table's metatable:

```cpp
let mt = getmetatable(t);
print(mt);  // The metatable table
```

---

## Metamethods

Metamethods are special keys in the metatable that define custom behavior.

### Arithmetic Metamethods

Override arithmetic operators:

```cpp
let vec1 = { x = 1, y = 2 };
let vec2 = { x = 3, y = 4 };

let vec_mt = {
    __add = function(a, b) {
        return { x = a.x + b.x, y = a.y + b.y };
    },
    __sub = function(a, b) {
        return { x = a.x - b.x, y = a.y - b.y };
    },
    __tostring = function(v) {
        return "(" + tostring(v.x) + ", " + tostring(v.y) + ")";
    }
};

setmetatable(vec1, vec_mt);
setmetatable(vec2, vec_mt);

let sum = vec1 + vec2;  // Uses __add
print(tostring(sum));   // "(4, 6)"
```

**Supported operators:**
- `__add` — Addition `+`
- `__sub` — Subtraction `-`
- `__mul` — Multiplication `*`
- `__div` — Division `/`
- `__mod` — Modulo `%`
- `__pow` — Power `**`
- `__unm` — Unary negation `-`

### Bitwise Metamethods

Override bitwise operators:

- `__band` — Bitwise AND `&`
- `__bor` — Bitwise OR `|`
- `__bxor` — Bitwise XOR `^`
- `__bnot` — Bitwise NOT `~`
- `__shl` — Left shift `<<`
- `__shr` — Right shift `>>`

```cpp
let bits_mt = {
    __band = function(a, b) {
        return a.value & b.value;
    }
};
```

### Comparison Metamethods

Override comparison operators:

```cpp
let mt = {
    __eq = function(a, b) {
        return a.id == b.id;
    },
    __lt = function(a, b) {
        return a.value < b.value;
    },
    __le = function(a, b) {
        return a.value <= b.value;
    }
};
```

**Supported operators:**
- `__eq` — Equality `==`
- `__lt` — Less than `<`
- `__le` — Less or equal `<=`

**Note**: `>` and `>=` are derived from `<` and `<=`.

### Index Metamethods

Control table access:

```cpp
let mt = {
    // Called when reading missing key
    __index = function(table, key) {
        return "default";
    },
    
    // Called when writing new key
    __newindex = function(table, key, value) {
        print("Setting " + key + " = " + tostring(value));
        rawset(table, key, value);  // Actually set the value
    }
};

let t = {};
setmetatable(t, mt);

print(t["missing"]);  // "default" (via __index)
t["new"] = 42;        // Prints "Setting new = 42" (via __newindex)
```

**Use cases:**
- Default values for missing keys
- Read-only tables
- Validation on write
- Property getters/setters

### Length Metamethod

Customize the `#` operator:

```cpp
let mt = {
    __len = function(t) {
        let count = 0;
        for (_, _ in pairs(t)) {
            count = count + 1;
        }
        return count;
    }
};

let t = { a = 1, b = 2, c = 3 };
setmetatable(t, mt);

print(#t);  // 3 (counts all keys via __len)
```

Without `__len`, the `#` operator only counts sequential integer keys starting from 0.

### Call Metamethod

Make tables callable like functions:

```cpp
let mt = {
    __call = function(table, arg) {
        return "Called with " + tostring(arg);
    }
};

let t = { data = 42 };
setmetatable(t, mt);

print(t(100));  // "Called with 100"
```

**Use cases:**
- Factory functions
- Callable objects
- Functor patterns

### Other Metamethods

- `__tostring` — String conversion via `tostring()`
- `__gc` — Garbage collection finalizer (cleanup when table is collected)
- `__pairs` — Custom iterator for `for...in` loops

---

## Inheritance with Metatables

Metatables enable prototype-based inheritance through `__index`:

### Basic Inheritance

```cpp
let base = {
    getName = function(self) {
        return self.name;
    },
    greet = function(self) {
        print("Hello from " + self.name);
    }
};

let derived_mt = {
    __index = base  // Look up missing keys in base
};

let obj = { name = "MyObject" };
setmetatable(obj, derived_mt);

print(obj.getName(obj));  // "MyObject" - method from base
obj.greet(obj);           // "Hello from MyObject"
```

When you access `obj.getName`, Behl:
1. Checks if `obj` has a `getName` key → No
2. Looks at `obj`'s metatable → Found
3. Checks metatable's `__index` → Points to `base` table
4. Returns `base.getName`

### Class-Like Pattern

```cpp
// Define a "class"
let Animal = {
    new = function(self, name, sound) {
        let instance = { name = name, sound = sound };
        setmetatable(instance, { __index = self });
        return instance;
    },
    
    speak = function(self) {
        print(self.name + " says " + self.sound);
    }
};

// Create instances
let dog = Animal.new(Animal, "Dog", "Woof");
let cat = Animal.new(Animal, "Cat", "Meow");

dog.speak(dog);  // "Dog says Woof"
cat.speak(cat);  // "Cat says Meow"
```

### Multi-Level Inheritance

```cpp
let Base = {
    baseMethod = function(self) {
        return "Base";
    }
};

let Derived = {
    derivedMethod = function(self) {
        return "Derived";
    }
};

// Derived inherits from Base
setmetatable(Derived, { __index = Base });

// Instance inherits from Derived
let obj = { value = 42 };
setmetatable(obj, { __index = Derived });

print(obj.derivedMethod(obj));  // "Derived"
print(obj.baseMethod(obj));     // "Base" (walks chain)
```

---

## Raw Table Operations

Bypass metamethods with raw operations:

### `rawget(table, key)`

Get value without invoking `__index`:

```cpp
let t = { x = 10 };
setmetatable(t, {
    __index = function() { return "default"; }
});

print(t["missing"]);           // "default" (via __index)
print(rawget(t, "missing"));   // nil (bypasses __index)
```

### `rawset(table, key, value)`

Set value without invoking `__newindex`:

```cpp
let t = {};
setmetatable(t, {
    __newindex = function(table, key, value) {
        print("Blocked: " + key);
    }
});

t["key1"] = 1;              // Prints "Blocked: key1", doesn't set
rawset(t, "key2", 2);       // Sets directly, no print
print(rawget(t, "key2"));   // 2
```

### `rawlen(table)`

Get length without invoking `__len`:

```cpp
let t = {10, 20, 30};
setmetatable(t, {
    __len = function() { return 999; }
});

print(#t);           // 999 (via __len)
print(rawlen(t));    // 3 (actual length)
```

---

## Practical Examples

### Read-Only Tables

```cpp
function makeReadOnly(t) {
    let mt = {
        __index = t,
        __newindex = function(table, key, value) {
            error("Table is read-only");
        }
    };
    return setmetatable({}, mt);
}

let config = makeReadOnly({ port = 8080, host = "localhost" });
print(config.port);   // 8080
config.port = 9000;   // Error: Table is read-only
```

### Default Values

```cpp
function tableWithDefaults(defaults) {
    return setmetatable({}, {
        __index = function(table, key) {
            return defaults[key];
        }
    });
}

let settings = tableWithDefaults({ timeout = 30, retries = 3 });
settings.timeout = 60;  // Override default

print(settings.timeout);  // 60 (custom value)
print(settings.retries);  // 3 (default value)
```

### Property Tracking

```cpp
function makeTracked(t) {
    let access_log = {};
    
    let mt = {
        __index = function(table, key) {
            access_log[key] = (access_log[key] or 0) + 1;
            return rawget(t, key);
        }
    };
    
    let proxy = setmetatable({}, mt);
    proxy._getLog = function() { return access_log; };
    return proxy;
}

let data = makeTracked({ x = 10, y = 20 });
print(data.x);
print(data.x);
print(data.y);

let log = data._getLog();
print(log["x"]);  // 2
print(log["y"]);  // 1
```

---

## Best Practices

1. **Use raw operations carefully** — Bypassing metamethods can break expectations
2. **Document metamethod behavior** — Make it clear when tables have custom behavior
3. **Avoid complex `__index` chains** — Deep inheritance can hurt performance
4. **Don't overuse metamethods** — Use only when the abstraction adds value
5. **Test metatable edge cases** — Especially with `nil` values and missing keys
6. **Use `rawset` in `__newindex`** — Prevent infinite recursion

```cpp
// Bad: Infinite recursion
let mt = {
    __newindex = function(table, key, value) {
        table[key] = value;  // Triggers __newindex again!
    }
};

// Good: Use rawset
let mt = {
    __newindex = function(table, key, value) {
        rawset(table, key, value);  // Direct write
    }
};
```

---

## See Also

- [Tables](tables) — Basic table operations
- [Functions](functions) — Creating methods and callbacks
- [Types](types) — Understanding Behl's type system
