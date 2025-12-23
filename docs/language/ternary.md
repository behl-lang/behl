---
layout: default
title: Ternary Operator
parent: Language
nav_order: 4.5
---

# Ternary Conditional Operator
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The ternary conditional operator provides a concise way to choose between two values based on a condition.

## Syntax

```cpp
condition ? value_if_true : value_if_false
```

The operator evaluates the condition and returns:
- `value_if_true` if condition is truthy
- `value_if_false` if condition is falsy

## Basic Usage

```cpp
let x = 10
let result = x > 5 ? "big" : "small"
print(result)  // "big"

// In return statements
function abs(n) {
    return n >= 0 ? n : -n
}
```

## Truthiness

Only `false` and `nil` are falsy. Everything else (including `0`) is truthy:

```cpp
let a = 0 ? "yes" : "no"       // "yes" (0 is truthy)
let b = nil ? "yes" : "no"     // "no" (nil is falsy)
let c = false ? "yes" : "no"   // "no" (false is falsy)
let d = "" ? "yes" : "no"      // "yes" (empty string is truthy)
```

## With Comparisons

```cpp
let age = 18
let status = age >= 18 ? "adult" : "minor"

let max = a > b ? a : b
let min = a < b ? a : b
```

## Different Return Types

The true and false branches can have different types:

```cpp
let value = hasData ? "string" : 42

let result = condition ? 3.14 : nil

let output = flag ? {x = 1} : "not found"
```

## Nested Ternary

Ternary operators can be nested for multiple conditions:

```cpp
let x = 5
let size = x > 10 ? "large" : x > 5 ? "medium" : "small"
// Result: "small"

// More readable with parentheses
let size = x > 10 ? "large" : (x > 5 ? "medium" : "small")
```

**Warning**: Deeply nested ternary operators can be hard to read. Consider using if/else for complex logic:

```cpp
// Hard to read
let result = a ? b ? c ? d : e : f : g

// Better
let result
if (a) {
    if (b) {
        result = c ? d : e
    } else {
        result = f
    }
} else {
    result = g
}
```

## In Assignments

```cpp
let discount = isMember ? 0.20 : 0.05

let message = errorCount == 0 ? "Success" : "Failed"

let value = hasDefault ? getDefault() : computeValue()
```

## With Function Calls

Both branches can contain function calls:

```cpp
function getA() { return 1 }
function getB() { return 2 }

let result = condition ? getA() : getB()

// Only the chosen branch is evaluated (short-circuit)
let value = fast ? quickResult() : slowComputation()
```

## With Table Access

```cpp
let config = {fast = 10, slow = 100}
let timeout = isProduction ? config.slow : config.fast

let t = {a = 10, b = 20}
let value = useA ? t.a : t.b
```

## Operator Precedence

The ternary operator has low precedence. Use parentheses for clarity when mixing with other operators:

```cpp
// Without parentheses
let x = 5 + (condition ? 10 : 20)  // Clear

// Be explicit with complex expressions
let result = (a && b) ? (c || d) : (e && f)
```

## Short-Circuit Evaluation

Only the chosen branch is evaluated:

```cpp
let result = condition ? expensiveTrue() : expensiveFalse()
// Only one of the functions is called

// Useful for avoiding errors
let safe = obj != nil ? obj.value : "default"
```

## Best Practices

1. **Keep it simple** - Use for simple value selection
2. **Avoid deep nesting** - 2 levels max; use if/else for more
3. **Use parentheses** - Make precedence clear in complex expressions
4. **Consider readability** - If/else may be clearer for complex logic

```cpp
// Good: Simple value selection
let status = isActive ? "on" : "off"

// Good: Single nested level
let category = age >= 18 ? "adult" : age >= 13 ? "teen" : "child"

// Avoid: Too complex
let x = a ? b ? c ? d : e : f ? g : h : i ? j : k

// Better: Use if/else
let x
if (a) {
    x = b ? (c ? d : e) : (f ? g : h)
} else {
    x = i ? j : k
}
```

## Comparison with Other Languages

### JavaScript / C / C++ / Java
Identical syntax and behavior:
```javascript
let x = condition ? trueValue : falseValue
```

### Python
Python uses a different syntax:
```python
x = true_value if condition else false_value
```

### Lua
Lua doesn't have a ternary operator, uses `and`/`or`:
```lua
local x = condition and true_value or false_value
-- Warning: fails if true_value is falsy
```

Behl's ternary operator works correctly even when branches are falsy:
```cpp
let x = true ? false : true   // Result: false (works correctly)
// Lua equivalent would fail: local x = true and false or true  // Result: true (incorrect!)
```
