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
while for foreach in
true false nil
break continue defer
module export local
```

That is the complete list of 20 reserved words. All of them are recognised by the lexer and none can be used as an identifier.

`local` is reserved but no parser rule consumes it, so it is currently unusable: it is neither a declaration form nor a valid variable name.

### Not Keywords

- `import` - a regular function name, so it can be shadowed

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

Semicolons are **optional** in Behl. There is no automatic semicolon insertion: the parser simply does not require a `;` to terminate a statement, and consumes one only if it happens to be there.

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

The one exception is the `module` declaration, whose semicolon is required:

```cpp
module;   // required, 'module' alone is an error
```

## Literals

### Strings

String literals may be delimited by double or single quotes:

```cpp
let a = "double quoted";
let b = 'single quoted';
```

### Numbers

Decimal and hexadecimal integer literals are supported. There is no binary (`0b`) literal form:

```cpp
let dec = 255;
let hex = 0xFF;
let hex2 = 0xff;
```

Floating point literals may omit the leading zero:

```cpp
let half = .5;
let also = 0.5;
```
