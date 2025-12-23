---
layout: default
title: Syntax
parent: Language
nav_order: 1
---

# Lexical Conventions
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Comments

Behl uses C-style comments:

```cpp
// Single-line comment

/* Multi-line
   comment */

/* Nested /* comments */ are not supported */
```

## Identifiers

Identifiers must start with a letter or underscore, followed by letters, digits, or underscores:

```cpp
let validName = 1;
let _private = 2;
let name123 = 3;
```

### Naming Conventions

While not enforced, these conventions are recommended:

- **Variables**: `camelCase` or `snake_case`
- **Constants**: `UPPER_CASE` or `PascalCase`
- **Functions**: `camelCase` or `snake_case`
- **Private/internal**: prefix with `_`

## Keywords

Reserved keywords cannot be used as identifiers:

```
let const function return
if else elseif
while for in
true false nil
break continue
```

### Context-Sensitive Keywords

Some identifiers have special meaning in specific contexts but can be used as variable names:

- `defer` - Statement keyword, but can be a variable name
- `import` - Function name, can be shadowed

## Whitespace

Whitespace (spaces, tabs, newlines) is generally ignored except to separate tokens:

```cpp
// These are equivalent
let x=1;
let   x  =  1  ;
let x =
    1;
```

However, meaningful newlines can improve readability:

```cpp
// Preferred
for (let i = 0; i < 10; i++) {
    print(i);
}

// Works but discouraged
for(let i=0;i<10;i++){print(i);}
```

## Semicolons

Semicolons are **optional** in Behl. The language supports automatic semicolon insertion:

```cpp
// With semicolons
let x = 10;
print(x);

// Without semicolons (also valid)
let x = 10
print(x)
```

Both styles work. Semicolons are helpful for clarity when putting multiple statements on one line:

```cpp
let x = 10; let y = 20; print(x + y)
```
