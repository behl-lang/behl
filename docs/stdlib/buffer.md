---
layout: default
title: buffer
parent: Standard Library
nav_order: 11
---

# buffer
{: .no_toc }

Mutable byte buffers, slices and typed little-endian access.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

A `buffer` is a first-class, mutable array of bytes (see [Values & Types](../language/types#buffers)). Scripts can read and write single bytes with `b[i]` and get the length with `#b`. The `buffer` module creates buffers and provides bulk operations and typed reads and writes. It is loaded by `load_stdlib` and must be explicitly imported:

```cpp
const buffer = import("buffer");

let b = buffer.create(4);
b[0] = 72;
b[1] = 105;
print(#b);                            // 4
print(buffer.to_string(b, 0, 2));     // "Hi"
```

All offsets are 0-based. Offsets, lengths and integer values accept integral floats such as `2.0`.

Every function checks its range against the buffer's current length. An access that does not fit raises:

```
RuntimeError: buffer access out of range (offset 1, count 4, length 4)
```

Passing something other than a buffer where a buffer is expected raises `bad argument #n (expected buffer, got T)`.

---

## buffer.create(len)

Creates a new zero-filled buffer of `len` bytes.

```cpp
let b = buffer.create(3);
print(#b, b[0], b[1], b[2]);  // 3  0  0  0
```

**Parameters:**
- `len` - Length in bytes, may be `0`

**Errors:**
- `len` negative: `buffer length must not be negative, got -1`

---

## buffer.from_string(s)

Creates a new buffer holding a copy of the bytes of `s`. Embedded zero bytes are kept.

```cpp
let b = buffer.from_string("hello");
print(#b, b[0]);  // 5  104
```

---

## buffer.to_string(b, offset, len)

Returns a string holding a copy of `len` bytes of `b` starting at `offset`.

```cpp
let b = buffer.from_string("abcdef");
print(buffer.to_string(b));        // "abcdef"
print(buffer.to_string(b, 2));     // "cdef"
print(buffer.to_string(b, 1, 2));  // "bc"
```

**Parameters:**
- `b` - The buffer
- `offset` - Optional starting offset (default: `0`)
- `len` - Optional byte count (default: the rest of the buffer after `offset`)

---

## buffer.slice(b, offset, len)

Returns a new buffer that is a view of `len` bytes of `b` starting at `offset`. The slice does not copy: it shares its bytes with `b`, so writes through either are visible through both. See [Slices](#slices).

```cpp
let b = buffer.from_string("hello");
let s = buffer.slice(b, 1, 3);
print(buffer.to_string(s));  // "ell"

s[0] = 69;
print(buffer.to_string(b));  // "hEllo"
```

**Parameters:**
- `b` - The source buffer, may itself be a slice
- `offset` - Starting offset in `b`
- `len` - Length of the slice

---

## buffer.resize(b, len)

Changes the length of `b` to `len` bytes. Existing bytes up to the new length are kept, and bytes added by growing are zero. Returns nothing.

```cpp
let b = buffer.from_string("hi");
buffer.resize(b, 4);
print(#b, b[1], b[2], b[3]);  // 4  105  0  0
```

**Errors:**
- `b` is a slice: `a buffer slice can not be resized`
- `len` negative: `buffer length must not be negative, got -1`

---

## buffer.copy(dst, dst_offset, src, src_offset, len)

Copies `len` bytes from `src` starting at `src_offset` into `dst` starting at `dst_offset`. The ranges may overlap (including when `dst` and `src` are the same buffer), the result is as if the source bytes were copied out first. Returns nothing.

```cpp
let src = buffer.from_string("abc");
let dst = buffer.create(5);
buffer.copy(dst, 1, src);
print(dst[0], dst[1], dst[3]);  // 0  97  99

let d = buffer.from_string("abcdef");
buffer.copy(d, 2, d, 0, 4);
print(buffer.to_string(d));     // "ababcd"
```

**Parameters:**
- `dst` - Destination buffer
- `dst_offset` - Offset in `dst`
- `src` - Source buffer
- `src_offset` - Optional offset in `src` (default: `0`)
- `len` - Optional byte count (default: the rest of `src` after `src_offset`)

Both ranges are checked, an out of range source or destination raises the range error.

---

## buffer.fill(b, offset, value, len)

Sets `len` bytes of `b` starting at `offset` to `value`. Only the low 8 bits of `value` are stored. Returns nothing.

```cpp
let b = buffer.create(6);
buffer.fill(b, 2, 65);
print(b[1], b[2], b[5]);  // 0  65  65

buffer.fill(b, 0, 0x142, 2);
print(b[0], b[1]);        // 66  66
```

**Parameters:**
- `b` - The buffer
- `offset` - Starting offset
- `value` - Byte value, an integer
- `len` - Optional byte count (default: the rest of the buffer after `offset`)

---

## Typed reads and writes

```cpp
buffer.read_u8(b, offset)       buffer.write_u8(b, offset, value)
buffer.read_i8(b, offset)       buffer.write_i8(b, offset, value)
buffer.read_u16(b, offset)      buffer.write_u16(b, offset, value)
buffer.read_i16(b, offset)      buffer.write_i16(b, offset, value)
buffer.read_u32(b, offset)      buffer.write_u32(b, offset, value)
buffer.read_i32(b, offset)      buffer.write_i32(b, offset, value)
buffer.read_u64(b, offset)      buffer.write_u64(b, offset, value)
buffer.read_i64(b, offset)      buffer.write_i64(b, offset, value)
buffer.read_f32(b, offset)      buffer.write_f32(b, offset, value)
buffer.read_f64(b, offset)      buffer.write_f64(b, offset, value)
```

Each function accesses 1, 2, 4 or 8 bytes starting at `offset`. The whole range must lie inside the buffer. Write functions return nothing.

**Byte order:** all multi-byte values are stored and read **little-endian**, regardless of the host CPU. A buffer written on one platform reads back identically on any other.

```cpp
let b = buffer.create(8);
buffer.write_u32(b, 0, 0x11223344);
print(b[0], b[1], b[2], b[3]);  // 68  51  34  17  (0x44 0x33 0x22 0x11)
```

### Integer widths

- The integer write functions take an integer and store its low 8, 16, 32 or 64 bits. `write_uN` and `write_iN` of the same width store identical bits.
- `read_uN` zero-extends and `read_iN` sign-extends the stored bits to a 64-bit integer.
- Behl integers are 64-bit signed, so `read_u64` and `read_i64` return the same value; a stored value above `2^63 - 1` reads back as a negative integer from both.

```cpp
let b = buffer.create(8);
buffer.write_i16(b, 0, -2);
print(buffer.read_u16(b, 0), buffer.read_i16(b, 0));  // 65534  -2

buffer.write_u8(b, 0, 300);
print(buffer.read_u8(b, 0));                          // 44

buffer.write_i64(b, 0, -1);
print(buffer.read_u64(b, 0), buffer.read_i64(b, 0));  // -1  -1
```

A non-integral value raises `bad argument #3 (expected integer, got number)`.

### Floats

`write_f32` and `write_f64` store an IEEE 754 single or double precision value. `write_f32` rounds the value to single precision, so it may not read back exactly. The read functions return a `number`.

```cpp
let b = buffer.create(8);
buffer.write_f32(b, 0, 0.1);
print(buffer.read_f32(b, 0));  // 0.10000000149011612

buffer.write_f64(b, 0, 0.1);
print(buffer.read_f64(b, 0));  // 0.1
```

---

## Slices

`buffer.slice` returns a buffer that views a range of another buffer's bytes instead of owning its own. A slice behaves like any other buffer: indexing, `#`, `typeof` (`"buffer"`) and every function in this module work on it, except `buffer.resize`.

- **Shared bytes:** writes through the slice are visible in the source and the other way round.
- **Slice of a slice:** slicing a slice creates a view of the original (root) buffer, with the offsets added up. There is no chain of slices.
- **Lifetime:** a slice keeps its root buffer alive. The root is not collected while any slice of it is reachable.
- **Resizing:** a slice can not be resized, only its root can. After the root is resized the slice sees the new bytes. If the root shrinks below the end of the slice, the slice's length is clamped to the bytes that still exist (possibly `0`); it grows back, up to its original length, when the root grows again.

```cpp
const buffer = import("buffer");

let root = buffer.from_string("hello");
let s = buffer.slice(root, 1, 3);
let s2 = buffer.slice(s, 1, 2);    // views root bytes 2..3
s2[0] = 88;
print(buffer.to_string(root));     // "heXlo"

buffer.resize(root, 2);
print(#s, #s2);                    // 1  0

buffer.resize(root, 5);
print(#s, #s2);                    // 3  2
```
