# Behl Grammar Specification

The complete syntax of Behl, in the style of the Lua reference manual. Like that grammar, the
expression rule here is intentionally ambiguous: precedence and associativity are given separately
in section 3, not encoded in the productions.

This is derived from and verified against the reference implementation (`src/frontend/lexer.cpp`
and `src/frontend/parser.cpp`). Where the prose docs and the implementation disagree, the
implementation is authoritative.

## 1. Syntax

```
chunk ::= [‘module’ ‘;’] block

block ::= {stat}

stat ::=  ‘;’ |
     varlist ‘=’ explist |
     var compoundop exp |
     var (‘++’ | ‘--’) |
     functioncall |
     ‘break’ |
     ‘continue’ |
     ‘{’ block ‘}’ |
     ‘defer’ (‘{’ block ‘}’ | stat) |
     ‘if’ ‘(’ exp ‘)’ body {‘elseif’ ‘(’ exp ‘)’ body} [‘else’ body] |
     ‘while’ ‘(’ exp ‘)’ body |
     ‘foreach’ ‘(’ [‘let’ | ‘const’] namelist ‘in’ exp ‘)’ body |
     forstat |
     ‘function’ funcname funcbody |
     (‘let’ | ‘const’) ‘function’ Name funcbody |
     (‘let’ | ‘const’) namelist [‘=’ explist] |
     ‘return’ [explist] |
     exportstat

body ::= ‘{’ block ‘}’ | stat

exportstat ::=  ‘export’ ‘function’ funcname funcbody |
     ‘export’ ‘const’ namelist [‘=’ explist] |
     ‘export’ ‘{’ namelist ‘}’

forstat ::=
     ‘for’ ‘(’ (‘let’ | ‘const’) Name ‘=’ exp {‘,’ Name [‘=’ exp]} ‘;’ exp ‘;’ [updatelist] ‘)’ body |
     ‘for’ ‘(’ [‘let’ | ‘const’] namelist ‘in’ explist ‘)’ body |
     ‘for’ ‘(’ Name ‘=’ exp ‘,’ exp [‘,’ exp] ‘)’ body |
     ‘for’ ‘(’ Name ‘=’ exp ‘;’ exp ‘;’ [update] ‘)’ body

updatelist ::= update {‘,’ update}

update ::= var compoundop exp | var (‘++’ | ‘--’) | varlist ‘=’ explist | var

funcname ::= Name {‘.’ Name} [‘:’ Name]

varlist ::= var {‘,’ var}

var ::=  Name | prefixexp ‘[’ exp ‘]’ | prefixexp ‘.’ Name

namelist ::= Name {‘,’ Name}

explist ::= exp {‘,’ exp}

exp ::=  ‘nil’ | ‘false’ | ‘true’ | Numeral | LiteralString | ‘...’ |
     functiondef | prefixexp | tableconstructor |
     exp binop exp | unop exp | exp ‘?’ exp ‘:’ exp

prefixexp ::= var | functioncall | ‘(’ exp ‘)’

functioncall ::=  prefixexp args | prefixexp ‘:’ Name args

args ::=  ‘(’ [explist] ‘)’

functiondef ::= ‘function’ funcbody

funcbody ::= ‘(’ [parlist] ‘)’ ‘{’ block ‘}’

parlist ::= namelist [‘,’ ‘...’] | ‘...’

tableconstructor ::= ‘{’ [fieldlist] ‘}’

fieldlist ::= field {fieldsep field}

field ::= ‘[’ exp ‘]’ ‘=’ exp | Name ‘=’ exp | ‘...’ | exp

fieldsep ::= ‘,’ | ‘;’

compoundop ::= ‘+=’ | ‘-=’ | ‘*=’ | ‘/=’ | ‘%=’

binop ::=  ‘+’ | ‘-’ | ‘*’ | ‘/’ | ‘**’ | ‘%’ |
     ‘&’ | ‘^’ | ‘|’ | ‘>>’ | ‘<<’ |
     ‘<’ | ‘<=’ | ‘>’ | ‘>=’ | ‘==’ | ‘!=’ |
     ‘&&’ | ‘||’

unop ::= ‘-’ | ‘!’ | ‘#’ | ‘~’
```

Constraints the grammar above does not capture but the parser enforces:

- In `varlist ‘=’ explist`, `var compoundop exp`, and `var ‘++’`/`‘--’`, every target must be a
  `Name`, an index, or a member. `compoundop` and `++`/`--` accept exactly one target.
- A `functioncall` is the only `exp` permitted to stand alone as a `stat`.
- In `parlist`, `‘...’` may appear only last (it takes no name).
- A method definition (`funcname` ending in `‘:’ Name`) gets an implicit leading `self` parameter,
  and a method call `a:m(args)` is evaluated as `a.m(a, args)`.
- `foreach (x in t)` is evaluated as iteration over `pairs(t)`.
- A `chunk`'s `‘module’ ‘;’` prefix, if present, must be first; it takes no name.

## 2. Lexical conventions

Source is UTF-8; `Name`s and keywords are ASCII.

**Name** (identifier):
```
Name ::= (letter | ‘_’) {letter | digit | ‘_’}
```
A `Name` matching a keyword is that keyword instead. Reserved keywords:
`let`, `const`, `if`, `else`, `elseif`, `while`, `for`, `foreach`, `function`, `return`,
`break`, `continue`, `defer`, `true`, `false`, `nil`, `in`, `module`, `export`, `local`.
(`local` is reserved but has no production yet; `import` is *not* reserved.)

**Numeral:**
```
Numeral ::= ‘0’ (‘x’ | ‘X’) hexdigit {hexdigit}
          | digit {digit} [‘.’ {digit}]
          | ‘.’ digit {digit}
```
No exponent notation and no hex floats. A decimal with a `.` is floating point; an integer that
does not fit in a signed 64-bit value falls back to floating point; a hex Numeral is an integer.

**LiteralString:** `"..."` or `'...'`, may span lines. Escapes: `\a \b \f \n \r \t \v \\ \' \"`
and backslash-newline (line continuation); any other escaped character is literal.

**Comments:** `//` to end of line, or `/* ... */` (non-nesting).

## 3. Operator precedence

Lowest to highest binding. All binary operators are left-associative except `**` and the ternary
`?:`, which are right-associative. Unary operators (`- ! # ~`) bind tighter than any binary
operator. Postfix `[] . : ()` bind tightest.

| Operators            | Assoc |
|----------------------|-------|
| `?:`                 | right |
| `\|\|`               | left  |
| `&&`                 | left  |
| `< > <= >= == !=`    | left  |
| `\|`                 | left  |
| `^`                  | left  |
| `&`                  | left  |
| `<< >>`              | left  |
| `+ -`                | left  |
| `* / %`              | left  |
| `**`                 | right |

## 4. Differences from Lua's grammar

For readers coming from the Lua reference grammar above, behl differs as follows:

- **Blocks** are `{ ... }`, not `do/then/end`; control-flow conditions are parenthesized.
- **No** `repeat`/`until`, `goto`/label, `global`, or `<attrib>` attributes.
- **`return`** is an ordinary `stat` and may appear anywhere in a `block`, not only last.
- **`args`** is only `‘(’ [explist] ‘)’`; there is no `f"str"` or `f{table}` call sugar.
- **Power is `**`** (right-assoc). `^` is **bitwise XOR**, not exponentiation.
- **Logical operators** are `&&`, `||`, `!`; inequality is `!=`. There is no `and`/`or`/`not`.
- **No `..`** concatenation operator (the lexer recognizes `..` but no rule uses it) and no `//`
  floor division; string concatenation is `+`.
- **Compound assignment** (`+= -= *= /= %=`), **`++`/`--`**, and the **ternary `?:`** exist.
- **`foreach`** is a dedicated form; the C-style and numeric `for` headers are behl-specific.
- **Tables are 0-indexed**, and `fieldlist` does **not** allow a trailing `fieldsep`.
- **`...`** as a parameter takes no name (`parlist`), unlike Lua's `‘...’ [Name]`.
- Precedence deviates from C: bitwise `& ^ |` bind **tighter** than comparison, and shifts bind
  **looser** than `+ -`. Parenthesize when mixing.

---

*Derived from the reference lexer and parser. Keep it verifiable against them: changes to
`lexer.cpp`/`parser.cpp` should be reflected here.*
