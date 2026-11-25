# Flag Reference

The exact rules of the Flag language, its memory model, and the `flagc` compiler. For a guided introduction, see the [Overview](Overview.md); for standard library functions, see the [Library](Library.md).

- [How the compiler works](#how-the-compiler-works)
- [The command line](#the-command-line)
- [Files, folders and the library](#files-folders-and-the-library)
- [Syntax](#syntax)
- [Types in memory](#types-in-memory)
- [Numbers](#numbers)
- [Conversions](#conversions)
- [Unions and narrowing](#unions-and-narrowing)
- [Functions and calls](#functions-and-calls)
- [Generics and where](#generics-and-where)
- [Operators](#operators)
- [SIMD](#simd)
- [Iteration](#iteration)
- [Control flow](#control-flow)
- [Errors](#errors)
- [Constants and contexts](#constants-and-contexts)
- [Threads](#threads)
- [Modules](#modules)
- [Memory](#memory)
- [The memory checker](#the-memory-checker)
- [Debugging](#debugging)
- [Tests](#tests)
- [Calling C](#calling-c)
- [Libraries](#libraries)
- [Platforms](#platforms)
- [Editor support](#editor-support)
- [Diagnostics](#diagnostics)
- [Building the compiler](#building-the-compiler)
- [Rough edges](#rough-edges)

## How the compiler works

`flagc` is a single self-hosted executable written in Flag. Compiling a program runs five stages:

1. **Lexing** (`src/lex.flg`): cuts each source file into tokens.
2. **Parsing** (`src/parse.flg`): builds the syntax tree of the program.
3. **Type checking and IR generation** (`src/types.flg`, `src/gen.flg`, `src/call.flg`): checks types and builds, in memory, one list of instructions for the whole program: those of the backend, by their numbers and classes, with labels, the jumps that end a block, and marks that only `-memory` reads.
4. **Optimization and code emission** (`src/ir.flg`, `src/backend/`): inlines small functions (see [Inlining](#inlining)) unless `-debug` is enabled, drops the marks and the blocks that only jump on, cuts each function into the blocks of the backend, then runs the passes of the backend on it: SSA construction, slot promotion, load and copy elimination, global value numbering and code motion, if-conversion, the calling convention, instruction selection, spilling, register allocation, and the assembly (see [The backend](#the-backend)). For WebAssembly, `src/wasm.flg` writes the module from the SSA that the optimizations leave (see [WebAssembly](#webassembly)).
5. **Assembling and linking**: the system C compiler assembles the `.s` file and links it with the C library. A WebAssembly module needs neither.

Two flags insert extra passes:

- `-memory` runs `src/mem.flg` between stages 3 and 4 to check memory safety (see [The memory checker](#the-memory-checker)). It runs before inlining so warnings name the functions you wrote.
- `-debug` runs `src/dwarf.flg` between stages 4 and 5 to append DWARF debug information to the assembly (see [Debugging](#debugging)).

### What is checked and what is emitted

Code generation starts from `main`, from test functions under `flagc test` (see [Tests](#tests)), or from `export` functions in a shared library. Only reachable code is emitted into the final binary.

Type checking still checks every non-generic function, along with the default values of its parameters, even if nothing calls it, so errors in unused functions still fail compilation. The exceptions are:

- A function whose `where` clause is false on the current target (such as `func f() where @windows():` when compiling on Linux). It is checked when compiling for a target where it holds, where the C functions it calls have that platform's signatures.
- An inactive `where` block at the top level or inside a function (see [where](#where)).
- Generic functions, which are checked when instantiated by a call.

All `extern` C function declarations are checked: their parameter and return types must exist and be valid C ABI types.

`flagc` stops at the first error. It checks a function, then each generic instance created by its calls in order (recursively checking each instance's own callees before moving on), so when two calls in one function fail, the first is reported.

Intermediate files (`program.s`) are written next to the output path and deleted after linking succeeds unless `-keep` is passed. If assembling or linking fails, they remain on disk for inspection.

## The command line

```text
flagc [run|test] file.flg folder ... [-o program] [-shared] [-keep] [-check] [-memory] [-debug] [-target system-machine] [-overlay file] [-at file:line:column] [-timeout seconds] [-version] [-docs] [-cc anything for the C compiler] [-- arguments for run, or names for test]
```

| Argument / Option | Meaning |
|---|---|
| `file.flg` | Compiles a Flag source file. File order does not matter; all inputs are compiled together. |
| `folder` | Recursively adds every `.flg` file inside `folder`, sorted by name, without following directory symlinks. |
| `run` | Must be the first argument. Compiles and runs the program, forwarding everything after `--` as program arguments. Without `-o`, writes `flag-run-<number>.out` (`.exe` on Windows) in `TEMP` (or `/tmp` if unset) and deletes it afterward. `flagc` exits with the program's exit code. |
| `test` | Must be the first argument. Builds and runs tests in the input files, optionally filtered to test names containing any word after `--` (see [Tests](#tests)). `-o` keeps the test binary; `-timeout` sets the per-test limit in seconds (default `10`). |
| `-o program` | Sets the output path. Defaults to `a.out` (`a.exe` on Windows, `a.wasm` for `wasm-wasm32` and `wasm-wasm64`), or `a.so` / `a.dylib` / `a.dll` with `-shared`. |
| `-shared` | Builds a shared library instead of an executable (see [Libraries](#libraries)), or a WebAssembly module without `main` (see [WebAssembly](#webassembly)). Cannot be combined with `run`. |
| `-check` | Stops after type checking (and `-memory`, if passed). Writes no files and does not require `main`. |
| `-memory` | Runs the compile-time memory checker. Warnings do not change the exit code. |
| `-debug` | Emits DWARF debug information for `gdb` and `lldb` (see [Debugging](#debugging)), or the names of the functions in a WebAssembly module. |
| `-keep` | Keeps the `.s` file, and the `.o` file produced by `-debug` on macOS. |
| `-target system-machine` | Cross-compiles for another platform (see [Platforms](#platforms)). An unknown target exits with code 2. |
| `-overlay file`, `-at file:line:column` | Editor and tooling support (see [Editor support](#editor-support)). |
| `-timeout seconds` | Per-test timeout for `flagc test`, from `1` to `2000000` seconds (about 23 days). Default is `10`. |
| `-version` | Prints the compiler version and the commit it was built from. |
| `-docs` | Prints the folder that holds the documentation of this version (`Overview.md`, `Reference.md`, `Library.md`) and exits, with no input file. In a clone of the repository that is `bin/../docs`. Otherwise it is the `docs` folder inside the cache folder of the library (see [Standard library location](#files-folders-and-the-library)), written from the `flagc` binary the first time and again if it is deleted. Search it with your own tools: `grep -n '^## ' "$(flagc -docs)/Reference.md"`. |
| `-cc ...` | Forwards every following argument verbatim to the C compiler until the `--` of `run` or `test` (`.c`, `.o`, `.a`, `-l`, `-static`, and so on). A second `-cc` is forwarded too. |

Colors on `error:`, `warning:`, and `note:` are emitted only when stderr is a terminal, and never when the `NO_COLOR` environment variable is set.

### Exit codes

- `0`: compilation (or `run` / `test`) succeeded.
- `1`: a compile error occurred, a build step failed, or a test failed.
- `2`: the command line is invalid. That happens with no input files, a directory with no `.flg` files, a missing value after `-o`/`-target`/`-overlay`/`-at`/`-timeout`, an `-at` location outside the input files, a `-timeout` outside `1`..`2000000`, no test matching the filters after `--`, `run` with `-shared`, `run` or `test` with `-target wasm-wasm32` or `wasm-wasm64`, or an unrecognized argument.

Invalid command-line errors show the expected form:

```text
error: option -target requires a value
error: 'run' must be the first argument: flagc run -memory hi.flg
error: non-Flag file vec.c must be passed to the C compiler after -cc: flagc main.flg -cc vec.c
error: unrecognized argument check (not a flagc option, .flg file, or directory)
error: directory notes contains no .flg files
error: no test name matches parse or lex
```

### The C compiler

`flagc` invokes `cc` by default, or the command in the `CC` environment variable split on spaces (so `CC="zig cc"` works). Linking runs:

```text
cc -o program program.s -L<each -L after -cc> -L<library folder> -L<folder of each file of the program> <the library of each extern> <what goes after -cc>
```

Each `extern` library entry becomes `-lname`, `-framework X`, or a library file path (see [extern](#extern)). With `-shared`, `flagc` adds `-shared` before the `-cc` arguments, and on Linux also `-fPIC -Wl,-Bsymbolic` (see [Libraries](#libraries)).

Because every input file's directory (including directories loaded by quoted `use`) is passed with `-L`, you can place `libsomething.a` next to your code or package and link it via `extern "something"` or `extern "libsomething.a"`. A `-L` after `-cc` is searched first, so its library wins over one of the same name in the library folder.

If the C compiler cannot be executed:

```text
error: cannot run cc: No such file or directory; flagc links using a C compiler (set the CC environment variable to choose one)
```

## Files, folders and the library

- **`core.flg`** is always loaded first. It has no `module` declaration, so all non-`priv` names in it (`print`, lists, maps, text, `File` and its `write` and `close`, threads, etc.) are visible everywhere.
- **Standard library location**: `flagc` looks for a `lib` directory next to its own directory (`bin/../lib`). If absent, it unpacks the library embedded inside the `flagc` binary into a cache directory on first run:
  - `$XDG_CACHE_HOME/flag/<version>-<number>`
  - `%LOCALAPPDATA%\flag\<version>-<number>` on Windows
  - `$HOME/.cache/flag/<version>-<number>`
  - `/tmp/flag/<version>-<number>`

  The `<number>` hash changes whenever the library or the documentation changes. A standalone `flagc` binary therefore works on its own, while keeping `lib/` next to it lets you inspect or edit the library.
- **Quoted `use` (local packages)**: `use "vendor/vec"` loads every `.flg` file directly inside that directory (not subdirectories) as part of the program. Paths use `/` and are resolved relative to the file containing the `use` (or may be absolute). The last directory segment names the module, so one of its files must declare `module vec` (see [Modules](#modules)). A directory referenced from multiple files, or also passed on the command line, is loaded once. Naming a file (`use "vendor/vec.flg"`) is an error explaining where the file belongs. `flagc` never downloads packages; external code is a local folder or git submodule.
- **Load order**: `flagc` loads `core`, the command-line files and folders, the folders from quoted `use` declarations (transitively), and finally standard library modules. When a program file writes `use something` and no program file declares `module something`, `flagc` loads `something.flg` from the standard library. Standard library files always resolve standard library modules even when the program defines a module of the same name (see [Modules](#modules)).
- **Encoding and paths**: a leading UTF-8 BOM is skipped. A file passed twice is loaded once. Diagnostics show the paths you passed (joined with quoted `use` paths for imported folders), and display standard library paths relative to the current directory when inside it.

## Syntax

### Lines and blocks

- **Indentation**: a `:` opens a block consisting of the following lines indented more than the header line. The first line of the block establishes its indentation width.
  - Indenting deeper than the current block is an error: `unexpected indentation; this line is indented deeper than the enclosing block`.
  - Dedenting to a column that does not match any open block is an error: `unexpected indentation; this line does not match the indentation of any enclosing block`.
  - Tabs, spaces, or both are allowed as long as block nesting does not depend on tab width. As in Python, `flagc` measures every line twice (with tabs advancing to the next multiple of 8, and with tabs counting as 1) and rejects ambiguous mixes: `mixed tabs and spaces in indentation make block nesting depend on tab width`. For example, a `\t` block may contain a `\t    ` block, but mixing `\t\t` and `\t ` in one block, or `\t` and 8 spaces, is an error.
  - Type fields, enum blocks, and `extern` blocks follow the same block rules.
- **No inline suites**: `:` must always be followed by an indented block (there is no `if x: print(x)`): `expected an indented block after ':'; write 'pass' for an empty block`. Use `pass` for an empty block. Only an `extern` block may have an empty body.
- **Top level**: only declarations (`func`, `type`, constants, `context`, `extern`, top-level `where`, etc.) may appear at column 1; statements must be inside functions: `unexpected 'print'; only declarations are allowed at top level, move statements inside a function`. `module` must be on the first line of the file, and `use` declarations must precede the first declaration.
- **Line continuation**: newlines are ignored inside `()`, `[]`, and `{}`, so long calls and literals can wrap at any indentation. Inside text literals, newlines are literal text. There is no `\` line continuation.
- **Comments and line endings**: `#` comments to the end of the line; there are no multiline comments. Both `\n` and `\r\n` line endings are accepted, while a stray `\r` not followed by `\n` is an error.
- **Nesting limit**: blocks, expressions, and types may nest up to **500 levels**: `expression or block nesting exceeds 500 levels; extract inner parts into a variable or function`. Flat chains (`elif`, `case`, `a - b - c`, `a and b and c`, `a ifnull b ifnull c`) do not count as nesting and have no fixed limit. Chained inline conditionals (`a if c else b if d else e`) nest one level per `else`. Long chains scale linearly in compile time and memory: 2,500 runtime `elif` branches (each with a `print`) compile in ~0.4 s, 20,000 in ~3 s using ~0.6 GB (and a 20,000-case `switch` behaves similarly); a single `case` with 20,000 values takes ~0.4 s.

### Tokens

- **Identifiers**: ASCII letters, digits, and `_`, not starting with a digit.
- **Special prefixes**: `@name` is a compiler builtin (see [Builtins](#builtins)), `@trust` and `@check` are memory-checker markers, `$T` is a type parameter, and `$$n` is a compile-time number parameter.
- **Integer literals**: decimal (`42`), hexadecimal (`0xff`), and binary (`0b101`). A single `_` may separate digits (`1_000_000`, `0xff_ff`). There is no octal syntax: `017` is `17`.
- **Float literals**: `2.5`, `1e3`, `1.5e-3`. Digits are required on both sides of `.` (`.5` and `5.` are not floats), and only one exponent is allowed. Digits immediately after a `.` never parse a decimal point, so `t.0.1` reads element `1` of element `0` of a tuple (see [Tuples](#tuples)).
- **Character literals**: `'a'` is a single printable ASCII character or escape sequence and evaluates to its byte value as an untyped integer literal (for example `'a'` is `97`, so `b == 'a'` works for `b: u8` and `c := 'a'` is `i64` printing as `97`). Control bytes such as tab must be escaped (`'\t'`).
- **Text literals**: `"..."` with escapes `\n`, `\t`, `\r`, `\0`, `\e`, `\\`, `\"`, `\'`, and `\xNN` (two hex digits for a raw byte). Text literals may span lines; newlines and leading spaces are preserved literally, with `\r\n` normalized to `\n`. A `\` before a line break is an error: `'\' cannot continue a line; text literals preserve line breaks directly`. An unclosed literal points to its opening quote: `unterminated text literal; expected closing '"' before end of file`.
- **Multi-character symbols**: `== != <= >= += -= *= /= %= -> := .. ..= .* << >> ||`.
- **Call spacing**: `(` immediately following a callee with no intervening space forms a call: `twice(3)`, `@here()`, `u8(x)`, `steps[i](x)`, `pick(true)(x)`. After a keyword, `(` must be preceded by a space: `not (x)`, `if (a) and (b)`, `return (x)`. Breaking either rule is a syntax error showing the right form:
  - `twice (3)` -> `no space allowed before '(' in a call; write: twice(3)`
  - `for x in reverse(l)` -> `'reverse' is a keyword, not a function; put a space after it: reverse l`
  - In declarations, function and operator names keep `(` attached (`func twice(x: i32)`, `operator and(a: Set, b: Set)`, `operator for(t: Tree)`, `operator [](g: Grid, c: Cell)`), as does `func` in function types (`func(i32) -> bool`). A space there fails: `no space allowed before '(' in a declaration; write: func twice(x: i32)`. In operator declarations, `[]`, `[]=`, and `[:]` are single tokens.
- **Disallowed C operators**: `!` and `&&` are rejected (`'!' is not an operator; use 'not'`), and `||` is only for untagged unions (`i32||f32`; see [Unions](#unions)). Address-of and dereference are `ptr(x)` and `p.*`, not `&x` and `*p`: `'&' is not an address-of operator; use: ptr(x)` and `'*' is not a prefix dereference operator; write: p.*`.

### Builtins

Calls starting with `@` are evaluated by the compiler at compile time. Arguments are types wherever a builtin expects a type.

| Builtin | Result |
|---|---|
| `@sizeof(T)` | Size of `T` in bytes as an untyped integer literal (see [Sizes](#sizes)). |
| `@offset_of(T, field)` | Byte offset of `field` in a struct or fat pointer, as an untyped integer literal. |
| `@can_assign(A, B)` | Compile-time `bool`: whether a value of type `B` can be assigned to `A` (see [where](#where)). |
| `@linux()`, `@macos()`, `@windows()`, `@wasm()`, `@x86_64()`, `@arm64()`, `@riscv64()`, `@wasm32()`, `@wasm64()` | Compile-time `bool`: whether the build targets that OS or CPU architecture. |
| `@here()` | Source location as `&strview` (e.g. `"main.flg:12:5"`). In a parameter default, evaluates to the call site. |
| `@embed("path")` | Contents of a file embedded at compile time as `&strview` (see [Files in the program](#files-in-the-program)). |
| `@argc()`, `@argv()` | Raw C `main` arguments: `i32` count and `*bytes` pointer array. On WebAssembly, what WASI's `args_get` gives, asked the first time. |
| `@compile_error("text")` | Aborts compilation with `"text"` at the call site that instantiated the enclosing function (see [where](#where)). |

Before 0.6, `sizeof`, `offset_of`, and `can_assign` had no `@` prefix; calling them without `@` fails with `sizeof is a compiler builtin; prefix it with '@': @sizeof(i64)` unless you declare your own function with that name. Any unknown `@name` fails with `unknown compiler builtin @foo`.

Built-in runtime operations (`sum`, `min`, `max`, `abs`, `sqrt`, `select`, `any`, `all` on `simd`, and `get`, `set`, `add`, `swap`, `compare_swap` on `atomic`) execute at runtime and do not use `@`; prefixing them with `@` fails with `@get is a runtime operation, not a '@' compiler builtin; write: get(hits)`.

`ptr`, `print`, and `eprint` are also built-in language names (no function, type, or constant may declare them; see [Modules](#modules)), alongside the literal keywords `true`, `false`, `null`, `inf`, and `nan`.

### Keywords

These 32 words are reserved and cannot name any variable, parameter, constant, function, type, field, or module:

```text
if elif else while for in reverse switch case break continue return assert defer with pass
and or xor not is as try trust parallel iferror ifnull true false null inf nan
```

```text
error: 'return' is a reserved keyword and cannot be used as a name
```

`reverse` is valid only after `in` in a `for` header. Reversing a list in place uses `flip(items)`; calling `reverse(l)` produces `'reverse' is only valid after 'in' in a 'for' loop (for x in reverse items); use flip(items) to reverse in place`.

Declaration-introducer words (`func`, `type`, `module`, `use`, `extern`, `context`, `shared`, `inherited`, `operator`, `priv`, `packed`, `export`, `inline`, `noinline`, `enum`, `fatptr`, `error`, `where`) are contextual: they may name variables or fields elsewhere. At the start of a line, `where` opens a `where` block when followed by a condition (see [where](#where)), and acts as an identifier when followed by a symbol (such as `where := 6` or `where.x = 1`) other than a space-separated `(`. In a parameter type position, `type` introduces a type parameter (`kind: type $T`; see [Type parameters](#type-parameters)).

## Types in memory

### Sizes

| Type | Size and alignment (bytes) |
|---|---|
| `bool`, `i8`, `u8` | 1 |
| `i16`, `u16` | 2 |
| `i32`, `u32`, `f32` | 4 |
| `i64`, `u64`, `f64`, pointers, function values, `Thread[T]` | 8 |
| `simd[T, n]` | 16 or 32, aligned to 16 |
| `atomic[T]` | Same as `T` |
| Void types, `null`, `IterationEnd` | 0 |

`bytes` has no size and exists only behind a pointer (`*bytes` is C's `void *` and supports byte arithmetic; see [Arithmetic](#arithmetic)). Declaring a value of type `bytes` is an error: `variable x cannot have type 'bytes' (unknown size)`.

No type may exceed **2,305,843,009,213,693,952 bytes** ($2^{61}$), guaranteeing that sizes and alignments never overflow `i64`: `type array[i64, 2305843009213693952] exceeds the maximum size of 2305843009213693952 bytes`, where its size wrapped around to 0.

`@sizeof(T)` returns an untyped integer literal, so it adapts to its target type (`n := @sizeof(Point)` is `i64`; `n: i32 = @sizeof(Point)` is `i32`). `@offset_of(T, field)` does the same for a single field of a struct or fat pointer (`@offset_of(&str, len)` is `8`; for nested structs, write `@offset_of(Car.motor, power)`).

Structs, unions, and arrays are value types: assigning, passing, or returning them copies their bytes. Copies larger than **128 bytes** call `memcpy`. A struct, array, or union of 48 bytes or less whose address only reaches loads, stores, copies, and constant offsets lives in registers, piece by piece, on every target, also through the copies that an inlined call makes and when a function returns it; with `-debug` it stays in memory. In a positional struct literal (`V{1.5, 2.5}`, not named `V{x = 1.5, y = 2.5}`), adjacent sub-8-byte constant numbers, `bool`s, and enums that fall within one 8-byte word are packed into a single 8-byte store so subsequent copies avoid store-forwarding stalls; words containing runtime values are stored field by field.

### Structs

Fields are laid out in declaration order, each aligned to its own alignment, and total struct size is rounded up to the largest field alignment (matching C):

```flag
type S:
    a: u8   # offset 0
    b: i32  # offset 4
    c: u8   # offset 8, total size 12
```

A nested struct field is stored inline and named `Outer.field` (or `mod.Outer.field` when imported from `mod`).

#### Field defaults

Any field of a struct, fat pointer, or context may have a default value (e.g., `size: i32 = 640`). Defaults are written whenever a value is created, after zeroing and writing union tags (see [Unions](#unions)), and before applying explicit literal fields:

- Applies to local variables, omitted fields in literals (`Settings{}` and `s: Settings` both get all defaults), array elements, newly inserted map entries, thread contexts at thread start, and static data for constants and `shared context`.
- Does **not** apply to manually zeroed memory (`calloc`) or reinterpreted bytes (`as`).
- **Allowed default values**: compile-time constants (numbers, text, constants, enum variants, `null`, function names, literals of those, `{}`, compile-time `a if c else b`) and the empty owners `heap([])`, `heap("")`, and `heap({})` (see [Who owns what](#who-owns-what)).
- Defaults see constants and type parameters in scope at the type declaration, not local variables at the instantiation site. Runtime expressions fail with `field default value must be a compile-time constant (literals, constants, or empty heap([]), heap(""), heap({})), not a runtime expression`.
- Defaults cannot read sibling fields: `default value of field area cannot read field width; defaults must be compile-time constants (literals, constants, or empty heap([]), heap(""), heap({}))`.
- Text literals cannot default-initialize a `&str` field because copying requires `heap(...)` at runtime (see [Fat pointers](#fat-pointers)).
- Shared-storage (`@`) fields cannot have defaults; they inherit the initial bytes of the field they overlay: `field low cannot have a default value; it shares storage with whole and uses whole's initial value`.
- Defaults in generic types are checked per instantiation (e.g., `value: $T = 1.5` succeeds for `Box[f32]` and fails for `Box[i32]` with a note at the instantiation site).

#### Required fields (no zero value)

Fat pointers have no valid zero value (see [Fat pointers](#fat-pointers)), and neither do structs or arrays containing a fat pointer field without a default:

- Omitting such a field in a literal fails: `missing required field text in Token (&strview has no default zero value); set it in the literal or add a field default`.
- A variable of that struct is uninitialized until assigned, and a context cannot hold one without a default (see [Contexts](#contexts)).
- Typical defaults are empty owners (`heap([])`, `heap("")`, `heap({})`) or empty views (`""`, `[]`), e.g. `names: &list[&strview] = heap([])`.
- A field whose type is a generic parameter `$T` starts as `$T` does: a fat pointer instantiated there starts with no memory (as with a new map entry), so `Box[&str]{}` compiles and growing its `&str` stops at runtime (see [Runtime errors](#runtime-errors)).

#### Shared storage (`@`) and `packed type`

- **`@other`** overlays a field onto the bytes of an earlier field. The overlaid field cannot be larger than `other`, and a literal may specify at most one of the two. If uninitialized, it holds `other`'s initial bytes (so an overlaid union without `null` is invalid until assigned). When a literal initializes the smaller overlaid field, the remaining bytes retain `other`'s initial value (including defaults): with `whole: u32 = 0xdead_beef` and `low: u16 @whole`, `W{low = 5}` holds `0xdead_0005`, even in a constant. In a typed top-level constant, this works when `other` is a number or zero-initialized; over a struct or non-zero-initialized union, it fails with `typed top-level constants must be compile-time constants (literals and constants), not runtime expressions; omit the type annotation to evaluate it at each use` (an untyped constant evaluates at each use instead). Printing skips `@` fields, and structs with `@` fields cannot be passed to C by value.
- **`packed type`** removes all padding: every field starts immediately after the previous one with alignment `1` (`a: u8` + `b: i64` is 9 bytes). `@` fields are allowed. Packed structs cannot be passed to C by value (pass a pointer).

#### Untyped `{...}` literals

A struct literal without a type name (`{1, 2}` or `{x = 1}`) takes its type from context: a typed variable, assignment, `return`, outer literal field, or typed array element.

- If the target type is a union containing exactly one struct, `{...}` targets that struct (`return {1, 2}` works for `Point?`, and `m["a"] = {1, 2}` works for a map of points). Likewise, `[...]` targets the single array or `simd` in a union (`v: simd[f32, 4]? = [1.0, 2.0, 3.0, 4.0]`), and `{k: v}` targets the single `fixmap`.
- With no target type (such as `p := {1, 2}` or a call argument), positional `{...}` with 2+ elements constructs a tuple (see [Tuples](#tuples)). If the target type has no matching struct or more than one, it fails with `untyped '{...}' literal needs a target struct or array type: p: Point = {1, 2}`.
- `{}` produces a value with every field at its default or zero (failing if any required field lacks a default).
- Positional literals must provide every field (`struct P has 2 fields, got 1 value`); named literals may omit fields that have a default or zero value.

### Tuples

A tuple is an anonymous struct written as a parenthesized list of two or more types: `(i32, bool)`. Tuples are structurally typed, so `(i32, bool)` is the same type everywhere. A single parenthesized type `(T)` is just grouping, as in `(func(i32) -> bool)?`.

- **Layout and operations**: identical to a struct with those field types. Elements are accessed and assigned by index: `t.0`, `t.1`, `t.0.1`. Tuples work everywhere a struct works: fields, array elements, parameters, union members, `$T` or `($A, $B)` matching, `is (i32, bool)`, `t as u64`, printing (`(0, 4)`), structural `==` and `hash` (see [Operators](#operators)), and C ABI by value.
- **Construction**: `{0, n}` (using target type or standalone inference, e.g. `{1, "a"}` is `(i64, &strview)`) or explicitly typed `(i32, bool){0, true}`. Without a target type, single-element or named braces fail:
  - `single-element '{...}' literal needs a target type (tuples require at least two elements): w: Wrapper = {1}`
  - `untyped '{...}' literal with named fields needs a target struct type: p: Point = {x = 1, y = 2}`
- **Parentheses vs braces**: `(a, b)` is always a tuple *type*, never a value. It is valid where a type is expected (`make((i32, bool))`), and fails in expression position: `(a, b) is a tuple type; write tuple values with braces: {a, b}`.
- **Multiple returns and unpacking**: `return a, b` returns a tuple (see [Returns](#returns)), and `lo, hi := split(text)` unpacks it (see [Assignments](#assignments)).
- **No error types inside tuples**: errors go in a union alongside the tuple (`-> (i32, bool)|FsError`). Putting an error inside a tuple, as in `(i32, bool, FsError)`, fails: `tuples cannot contain error types; return errors in a union alongside the values: -> (i32, bool)|FsError`. `try`, `iferror`, `ifnull`, and `trust` unwrap the union and yield the tuple for unpacking (`lo, hi := try split(text)`).

### Arrays

`array[T, N]` stores `N` contiguous elements of `T` with `T`'s alignment. Arrays are copied by value. `a.len` is `N` as an untyped integer literal (like `@sizeof`). `N` must fit in `i64`: `array[u8, 0x8000_0000_0000_0000]` fails with `count 9223372036854775808 exceeds the maximum i64 value (9223372036854775807)`.

An array whose element type has no zero value (such as fat pointers or structs with required fields) must be assigned as a whole before indexing: after `names: array[&strview, 3]`, writing `names[0]` before `names = ["a", "b", "c"]` is an error, and `array[&strview, 2]{}` fails with `array[&strview, 2] cannot be empty-initialized because &strview has no default zero value`.

### Fixed maps

`fixmap[K, V, N]` is a value struct holding `len` and inline storage for up to `N` key-value pairs in insertion order. Lookups scan linearly, so `fixmap` is meant for small maps. `@sizeof(fixmap[i32, i32, 2])` is `24`.

- A map literal produces a `fixmap` without allocating. With a target `fixmap` type it adopts that type (`colors: fixmap[Color, u32, 4] = {Red: 0xff0000}`). Standalone, its capacity equals its entry count: `{1: 2}` is `fixmap[i64, i64, 1]` and `ages := {"bo": 25}` is a full `fixmap[&strview, i64, 1]`. Because `operator []=` on `fixmap` returns `MapFull?`, `ages["ana"] = 30` does not compile (use `try put(...)`).
- `{}` has type `emptyfixmap` (analogous to `[]` / `emptyarray`). Neither has a concrete type alone: `x := {}` fails with `cannot infer type of x from an empty literal; specify its type explicitly: x: &map[&strview, i32] = heap({})`.
- To create a growable `&map`, wrap a literal or `fixmap` in `heap(...)` or `arena(a, ...)` (see [Who owns what](#who-owns-what)).

### Unions

#### Tagged unions (`A|B`)

- **Layout**: a 4-byte tag at offset 0 followed by the payload at offset 8 (`i32?` is 16 bytes; `&strview?` is 24 bytes). A `simd` payload is placed at offset 16 to satisfy 16-byte alignment (`simd[f32, 4]?` is 32 bytes). A union of only void types has no payload and takes 4 bytes (which keeps enums compact).
- **Tags**: built-in types have fixed low tags (`null` is always `0`, followed by `bool`, `i8`, ..., `bytes`, `cstr`, `wstr`, ..., `IterationEnd`, `never`). Every other type's tag is a hash of its module and name, so tags match across binaries and shared libraries regardless of declaration order. A `priv` type uses its file as its module and does not cross libraries. A standard library module shadowed by a user module uses `lib$<name>` (e.g., `lib$math`; see [Modules](#modules)), so its tags differ from unshadowed builds. Tags identify names, not field layouts; `load_library` verifies matching shapes separately (see [Libraries](#libraries)). A rare hash collision within one union fails: `Open and Ajar hash to the same union tag and cannot be distinguished in a union; rename one of them`. Tags are stable across edits, but persist names rather than raw tags to disk.
- **Default initial value**:
  - An `enum` starts as the first variant listed (even if another enum declared some of those variants earlier). A union with the same set of variants is the same type and shares that start; declaring two enums with identical variant sets is an error (use an alias like `type Paint = Color` instead).
  - Any other union starts as its member with the lowest internal ID: built-in types first (so `T?` always starts as `null`), then void types in declaration order, then other types in compiler creation order. For unions of two payload types (`P|Q`), do not rely on the default; initialize explicitly.
  - The compiler writes the initial tag whenever a value is created (variables, fields, elements, literals; an explicit field default overrides it). Arrays of non-null unions are initialized by a runtime loop; per-thread `context` values are initialized when each thread starts (and before `main`, or on the first `export func` call in a library or C-spawned thread; see [Libraries](#libraries)); constants and `shared context` carry their initial tags in static data.
  - Manually zeroed memory (such as `calloc`) is valid `null` for nullable unions, but holds an invalid tag for unions without `null`: assign before reading, or printing/switching on it crashes with an illegal instruction.
- **Normalization and grouping**: unions flatten, deduplicate, and sort members, so `(i32|null)|f64?` is `null|i32|f64` (the form diagnostics print). Parenthesize a union inside a pointer or untagged union: `*(null|i32)`, `i64||(i32|f32)`.

#### Untagged unions (`A||B`)

An untagged union has no tag, takes the size of its largest member, and starts zeroed (any member containing a non-nullable tagged union is invalid until assigned). Read a member by converting to it (`f32(u)` or `x: f32 = u`). Because there is no runtime tag, printing one is an error: `cannot format untagged union i32||f32 because it has no runtime tag; cast to the active member first, like: i32(...)`.

### Fat pointers

A fat pointer stores a data pointer followed by metadata fields:

| Type | Layout | Size (bytes) |
|---|---|---|
| `&strview`, `&listview[T]` | `data`, `len` | 16 |
| `&str`, `&list[T]` | `data`, `len`, `cap`, `arena` | 32 |
| `&map[K, V]` | `data`, `len`, `cap`, `arena` | 32 |

`&` applies only to fat pointer types: `x: &i32` fails with `i32 is not a fat pointer type; '&i32' is invalid`.

`len` and `cap` are `i64` (so `l.len - 3` with 2 elements is `-1`). A `&map` is an open-addressing hash table of `cap` slots (hash, key, value), where hash `0` marks an empty slot and `cap` is a power of two of at least `16`.

#### Initialization and memory

A fat pointer has no valid zero value: an owner (`&str`, `&list`, `&map`) gets its allocator from `heap(...)` or `arena(a, ...)`, and a view (`&strview`, `&listview`) gets its data from what it views. `heap([])`, `heap("")`, and `heap({})` create empty heap owners that do not allocate until grown; `""` and `[]` are empty views.

- Using a variable that no control-flow path initialized fails at compile time:

```text
error: names is used before being initialized (&list[&strview] has no default zero value); initialize it: names: &list[&strview] = heap([])
```

- Flow tracking follows `if` and `switch` branches (branches do not see each other; after merging, the variable counts as initialized if any path initialized it) and loops (seeing assignments before the loop and earlier in the body).
- If only some paths initialize an owner, compilation succeeds, `-memory` warns (`append cannot grow names; it may not have allocated memory to grow (initialize with heap([]))`; see [The memory checker](#the-memory-checker)), and growing an uninitialized owner at runtime aborts with `flag: this &list has no memory to grow in: give it heap([]) or arena(a, [])` (and the equivalent for `&str` and `&map`; see [Runtime errors](#runtime-errors)).
- Struct fields, array elements, and contexts obtain their initializers at creation (see [Structs](#structs)). A newly created map value starts at zero, so initialize an owner stored in a map before growing it: `groups[k] = heap([])` (see [Assignments](#assignments)).

#### Ownership, views, and copying

- **Allocator tag (`arena`)**: owners store either a `*Arena`, `1` (the heap marker), or `0` (uninitialized) (see [Arenas](#arenas)). `free` preserves `arena`, so a freed owner can grow again in the same allocator.
- **Zero-copy view conversion**: because views share the prefix layout of owners, `&str` converts to `&strview` and `&list[T]` converts to `&listview[T]` with no copy.
- **Raw data address**: `x.*` is the underlying data of a fat pointer, so `ptr(x.*)` is its `*bytes` address.
- **Read-only text**: `&strview` is read-only. Writing `v[i] = x`, `v[i] += x`, or passing `v[i]` to a pointer parameter fails with `&strview is read-only; mutate the owning &str instead`. `&listview`, `&str`, and `&list` are mutable. Text literals are `&strview`s pointing into read-only static memory (deduplicated across identical literals), as are `@embed` values; mutating their bytes through `items(v)`, `ptr(v[i])`, `ptr(v.*)`, `as`, or C stops the program with `flag: invalid memory access`, except on WebAssembly, whose memory has no protection, where it changes the literal for every use (see [WebAssembly](#webassembly)). Copy to a `&str` first to mutate.
- **Shallow copy on assignment/pass**: assigning or passing a fat pointer copies only its header (address and fields), not its elements. Both handles share the backing buffer; if one grows and reallocates, the other holds a stale length or dangling pointer. Functions that grow a fat pointer therefore take `*&` (see [Passing them to functions](Overview.md#passing-them-to-functions)).
- **View-to-owner copy**: assigning a view to an **already initialized** owner copies into the owner's allocator (`t = v` for `&str`/`&strview`, `l = lv` for `&list[T]`/`&listview[T]` or `*array[T, N]`). Initializing a **new** owner from a view, `[]`, `{}`, an array, or a `fixmap` (in a declaration, `return`, literal field, or `T(x)`), or assigning a view to an owner that has no value yet on any path, fails because the new owner has no allocator: `new &str has no allocated memory to copy &strview into; wrap the value in heap(...) or arena(a, ...)`. Wrap it in `heap(...)` or `arena(a, ...)`. Function calls never perform this copy, so a view cannot be passed to an owner parameter.

## Numbers

Flag has signed integers `i8`, `i16`, `i32`, `i64`; unsigned integers `u8`, `u16`, `u32`, `u64`; and floats `f32`, `f64`.

Lengths, counts, indexes, sizes, and C's `size_t` are `i64` (see [Types](#types)). Raw addresses are `*bytes`, and 64-bit unsigned values (hashes, OS handles) are `u64`. There is no `usize`: `Flag has no 'usize' type; use i64 for lengths, counts, and C's size_t, or *bytes for raw addresses`.

### Literals

An integer literal is untyped until used. It adopts its target type if it fits; standalone, it defaults to `i64` (or `u64` if above `i64` max):

```flag
a: u8 = 200                   # fits
b: u8 = 300                   # error: 300 does not fit in u8 (maximum is 255)
c := 5                        # i64
d := 0xffff_ffff_ffff_ffff    # u64
```

Character literals (`'a'`), `@sizeof`, `@offset_of`, array `.len`, `$$n`, and the counter of a range with two literal bounds are also untyped integer literals. Once bound to a variable (`n := 5`), `n` has concrete type `i64` and does not implicitly narrow to `i32` (write `n: i32 = 5` or `i32(n)`). Call arguments do not propagate element types into array literals either (see [Rough edges](#rough-edges)), so `f(ptr([1, 2]))` passes `*array[i64, 2]`.

- **Exact literal arithmetic**: arithmetic on untyped integer literals is evaluated at compile time with exact values (no wrapping): `x: u8 = 300 - 100` is `200`, and `0x7fff_ffff_ffff_ffff + 1` is `u64` literal `0x8000_0000_0000_0000`. Division by literal `0` or results exceeding 64 bits are compile errors.
- **Target type preference**: float literals default to `f64` and fit `f32` or `f64`. An integer literal can target a float type, but integers win over floats: `x: i32|f64 = 3` is `i32`, and `f(3)` picks `f(i32)` over `f(f64)`. Among integer union members, `i64` wins if present, then the first fitting type in union order (`i8`, `i16`, `i32`, `u8`, `u16`, `u32`, `u64`), so `x: u8|i8 = 5` is `i8` and `w: u16|u64 = 70000` is `u64`.
- **Ties**: a literal that fits two union members equally is an error. Only an untagged union inside a tagged one can tie, because it fits as well as its best member: `x: (u8||f32)|u8 = 5` fails with `literal 5 matches multiple members of u8|(u8||f32) equally; specify the type explicitly: u8(5)`.
- **`inf` and `nan`**: reserved float literals matching `print` and `parse_float` (`LOW = -inf`, `x: f32 = inf`). Arithmetic on finite literals must produce a finite float (`1.0 / 0.0` fails with `constant floating-point expression is not finite; write 'inf' or 'nan' explicitly`, and overflowing literals fail with `float literal 1e999 overflows f64 (maximum is 1.7976931348623157e+308); write 'inf' for infinity`). Operations involving `inf` or `nan` follow IEEE 754 (`inf - inf` is `nan`).

### Arithmetic

- **Mixing integer types**: two integers of different types mix when one contains every value of the other (the implicit widening rule), and the operation uses the wider type (`u8 + i32` -> `i32`, `u32 + i64` -> `i64`, `x += y` for `i64` and `i32`, while `u8 + u8` stays `u8`). If neither contains the other (same width with different sign, or signed with unsigned of equal or greater width), it fails: `cannot apply '+' to u64 and i64 because neither type contains all values of the other; convert one side with u64(...) or i64(...)`. Integers and floats never mix: `'+' requires operands of the same numeric type, got i32 and f64; convert one side with i32(...) or f64(...)`. A literal adapts to the other operand's type.
- **Integer comparisons**: any two integer types compare mathematically by value (`i64(-1) < u64(0)` is `true`, and `i < l.len` works for `i: i32`). A literal must still fit the other operand's type (`x < 300` for `x: u8` is an error).
- **Overflow and division**:
  - Integer arithmetic always wraps modulo the type's width (`+`, `-`, `*`, unary `-`, and `MIN / -1` -> `MIN`, with `MIN % -1` -> `0`, even when `-1` is a compile-time value such as `LAST: i32 = 0xffff_ffff as i32`).
  - Integer `/` truncates toward zero; `%` takes the sign of the left operand (`-7 / 2 == -3`, `-7 % 2 == -1`). By a power of two that the compiler knows, they are shifts and a mask that round the same way, also for signed numbers.
  - Integer division or remainder by zero stops the program with `flag: division by zero`. Float division is unchecked (`inf` or `nan`).
  - `%` is integer-only; use `fmod` from `math` for floats.
  - Unary `-` works only on signed integers and floats.
- **Float equality**: follows IEEE 754. `nan` equals nothing (including itself), so `x != x` tests for `nan` and `x == nan` is always `false`, including per lane in `simd`.
- **Bitwise and shift operators**:
  - `<<` and `>>` return the left operand's type; `>>` is arithmetic (sign-extending) on signed integers. The shift count may be any integer type (`u8(1) << n` for `n: i32` is `u8`). At runtime, counts wrap modulo 32 for types up to 4 bytes and modulo 64 for 8-byte types, with `u8` and `u16` shifted as 32-bit values (`u8(1) << 9` is `0`, `u8(1) << 33` is `2`, `x << 64` is `x`, and negative counts use their low bits so `i32(1) << -1` is `-2147483648`). Between two literals, shifts are exact and require a count in `0..=63`: `shift count must be between 0 and 63, got 64`.
  - On integers, `and`, `or`, `xor`, and `not` are bitwise (`0xf0 and 0x3c` is `0x30`, and `not` flips the bits within the width of the type: `not u8(1)` is `254`). On `bool`, they are logical, and `and`/`or` short-circuit.
  - `not` of an integer literal stays untyped and adapts to its target (`mask: u32 = not 0xff` is `0xffff_ff00`; `flags and not 1` clears bit 0 for any integer type; standalone `not 5` is `i64` `-6`).
- **Pointer arithmetic**: only `*bytes` supports `+`, `-`, and `+=`. `p + n` and `p - n` offset by `n` bytes (any integer type), and `q - p` returns the byte distance as `i64`. Two pointers of the same type compare unsigned by address (`==`, `<`, etc.). Arithmetic on any other `*T` requires casting through `*bytes` (`((ptr(x) as *bytes + 8) as *i64).*`) and otherwise fails: `no overload of + matches (*i64, i64); pointer arithmetic is only allowed on *bytes: p as *bytes + n`. `*bytes` arithmetic is not bounds-checked.
- **Absent operators**: there is no `**`, `~`, `^`, `&`, or `|`.

### Precedence

From loosest (1) to tightest (11):

| Level | Operators |
|---|---|
| 1 | `a if c else b` (see [a if c else b](#a-if-c-else-b)) |
| 2 | `or` |
| 3 | `and`, `xor` |
| 4 | `not` |
| 5 | `is` |
| 6 | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| 7 | `iferror`, `ifnull` |
| 8 | `..`, `..=`, `<<`, `>>` |
| 9 | `+`, `-` |
| 10 | `*`, `/`, `%` |
| 11 | `as` |

The remaining unary operators (`-`, `try`, `trust`, `parallel`) bind tighter than level 11 and apply to the entire postfix chain of calls, field accesses, and indexes after them: `try get().x` means `try (get().x)`. To access a field of the unwrapped result, parenthesize it; otherwise the error explains: `'try' applies to all of 'get().x', and union Bad|P has no field x; parenthesize it: (try get()).x`.

Additional precedence rules:

- **`not`**: binds looser than comparisons and `is`, like Python (`not x == 3` is `not (x == 3)`, and `if not line is &strview:` narrows like the `else` of `is`). On the right of a tighter operator, parenthesize it: `x == not y` fails with `'not' has lower precedence than '=='; wrap it in parentheses: (not y)`. Followed by arithmetic, `as`, or a shift, it requires parentheses: `'not x + 1' requires parentheses; write '(not x) + 1' to invert x, or 'not (x + 1)'`.
- **No chained comparisons**: putting a comparison on either side of another without parentheses is an error (even on `bool`): `comparisons cannot be chained; write 'a < b and b < c' or '(a < b) < c'`.
- **Bitwise vs comparisons**: `and`, `or`, and `xor` bind looser than comparisons, so bit tests need parentheses. Writing `flags and MASK != 0` parses as `flags and (MASK != 0)` and fails with `'and' has lower precedence than '!=', mixing u32 with a bool; parenthesize it: (flags and MASK) != 0`. Any other mix of integer and `bool` fails with `'or' requires two integers or two bools, got u32 and bool`.

## Conversions

### Implicit ones

When passing a call argument, a value converts implicitly only in these cases:

1. Identical type.
2. **Lossless integer widening**: to a larger integer of the same sign, or from unsigned to a larger signed type (`u32` -> `i64`).
3. **Owner to view**: `&str` -> `&strview`, `&list[T]` -> `&listview[T]`.
4. **Array pointer to view**: `*array[T, N]` -> `&listview[T]` (or `&strview` when `T` is `u8`), keeping the pointer and setting `len = N`. Owners (`&list`) never accept `*array` implicitly (see [Fat pointers](#fat-pointers)).
5. **Empty array literal `[]`**: converts to an empty view or to any zero-initialized `array[T, N]` (`a: array[i64, 3] = []` is `[0, 0, 0]`). Standalone `[]` and `{}` have no type (`x := []` is an error). A generic parameter `$T` may bind to `emptyarray`, but no composite type other than a pointer may be built from it: `cannot construct a type from emptyarray; an empty literal needs an explicit type first`.
6. **Empty owners of `never`**: `&list[never]` and `&map[never, never]` (returned by `heap([])`, `heap({})`, `arena(a, [])`, `arena(a, {})`) convert to any `&list[T]` or `&map[K, V]`. Standalone they have no type (`x := heap([])` fails with `cannot infer type of x from an empty literal; specify its type explicitly: x: &list[i32]`), and they do not bind a generic `$T`.
7. **Fitting literal**: an untyped literal that fits the target type.
8. **Text literal to `cstr`**: see [Text](#text).
9. **Into a tagged union**: any member (or smaller union) converts into a larger union. If a value converts to one or more members, it enters via the best-matching member (see [Choosing an overload](#choosing-an-overload)): `&str` into `&strview|i32`, `i32` into `i64|&strview`, `&list[i32]` into `&listview[i32]?`, and `i16` into `i32|i64` as `i32`. For the same reason, array literals drop element types that implicitly convert to another element type: `["a", s]` (with `s: &str`) is `array[&strview, 2]`, and `[a, b]` (`i32` and `i64`) is `array[i64, 2]`.
10. **Untagged union**: a member converts into or out of an untagged union.

Assignments, variable declarations, and `return` support all of the above plus:

- **Union narrowing by common conversion**: a union converts to a type when every member converts (`i32|i64` -> `i64`).
- **`operator =`** (see [Operators](#operators)), including `core`'s copy of a view into an existing owner (assignment only; a new owner has no allocator).

For example, `f(x)` with `x: i32` and `func f(n: i64)` widens `x` to `i64`, just like `n: i64 = x`.

**Never implicit**: signed-to-unsigned, same-width sign changes (`u64` to `i64`), narrowing integers, integer-to-float or float-to-integer, `f32` to `f64`, or pointer-to-pointer. These rules apply to a typed constant too (`y: f32 = RATIO` with `RATIO: f64 = 0.1` is an error). Narrowing or sign-changing integers fails with `cannot implicitly narrow or change sign from i64 to i32; convert explicitly with i32(...)`. `null` is never a pointer; a nullable pointer is `*T?`.

### T(x)

`T(x)` performs the conversions of an assignment (`t: T = x`), and when `T` is a numeric type it also performs explicit numeric conversions:

- **Integer to integer**: truncates or extends, sign-extending if the source is signed (`u64(x)` for `x: i32 = -1` is `18446744073709551615`).
- **Float to integer**: truncates toward zero and saturates at the target limits (`i32(1e300)` is `2147483647`; `nan` yields `0`).
- **`bool` to number**: `1` or `0`. There is no `bool(x)`; write `x != 0`.
- **Range-checked literals**: `i8(300)` is a compile error.

For non-numeric sources, a numeric `T(x)` falls back to assignment conversions: your `operator =` (e.g., with `operator =(x: f64, c: Celsius) -> f64`, both `back: f64 = hot` and `f64(hot)` work) or a union whose members all convert (`i64(x)` for `x: i32|i64`). Otherwise it fails: `i32() only converts numeric or bool values, not &strview, and no 'operator =' makes i32 from it`.

`simd` types convert numbers, arrays, and other `simd` values the same way (see [Lanes, literals and conversions](#lanes-literals-and-conversions)). Calling a struct or type alias name with one argument runs assignment conversion unless a function shares that name.

### as

`as` reinterprets the raw bytes of a value without converting:

- **Same-size untagged types**: numbers, `bool`, pointers, function values, structs, arrays, `simd`, and fat pointers of equal byte size. Examples: `f32(1.5) as u32` reads the float bits; `p as u64` reads an address; for `type V2` of two `f32`s, `V2{1.0, 2.0} as u64` is `0x400000003f800000` and `0x400000003f800000 as V2` recovers it. Struct padding bytes are read as-is. An untyped integer literal on the left takes the integer type of the target's byte size (signed if it fits): `0x3f800000 as f32` is `1.0`, `-1 as u8` is `255`, and `300 as bool` fails with `300 does not fit in u8`.
- **Unchecked union extraction**: extracts a member or smaller union from a tagged union without checking the tag. Extracting an inactive member produces invalid bytes.
- **Untagged unions**: reads or writes a member of an untagged union.

`as` never writes a union tag, so nothing can be cast into a tagged union and two tagged unions of equal size cannot be cast to each other. Any other invalid cast fails with `cannot reinterpret i32 as i64; types have different byte sizes`.

A `bool` whose byte is neither `0` nor `1` (whether from `as` on a `u8` or a one-byte struct, an untagged union like `u8||bool`, a pointer, or C) is not a valid Flag value: `if`, `==`, and `not` have undefined behavior on it.

### Pointers into fat pointers

Assigning a `*bytes` or a pointer to the element type directly to a fat pointer sets only its data pointer; you must set `len` yourself. The standard library uses this to construct views over raw memory. `cstr` does not assign this way; use `view(c)` (see [Text](#text)).

```flag
buf: array[u8, 4] = [104, 111, 108, 97]
text: &strview
text = ptr(buf) as *bytes
text.len = 4
print(text) # hola
```

## Unions and narrowing

`is` checks a tagged union's runtime tag. The left operand must be a tagged union, and any check that is statically always true or always false is a compile error.

Narrowing works **only on local variables and parameters**, not on struct fields. Accessing a union member's field through a struct field fails with `null|Card has no field rank; 'is' and 'case' only narrow local variables, so copy it to a variable first`. Copy the field to a local variable first:

```flag
found := user.best
if found is Card:
    print(found.rank)
```

### Narrowing rules

- **`if` / `elif` / `else`**: `if x is T:` narrows `x` to `T` in the `if` body and removes `T` in `elif` and `else`.
- **Void comparisons**: `x == V` and `x != V` (or `V == x`), where `V` is a void member such as `null` or `Red`, narrow identically to `x is V` and `not x is V` (`if x != null:` narrows the block; `if x == null: return` removes `null` afterward).
- **Short-circuit logic**: `a and b` checks `b` with `a`'s narrowing; `a or b` checks `b` with `not a`'s narrowing (`x is null or x > 3`).
- **Negation**: `not` flips narrowing (`if not x is T:` narrows like the `else` branch of `if x is T:`).
- **Inline conditional**: `a if c else b` checks `a` under `c` and `b` under `not c` (`x.n if x is P else 0`).
- **`while`**: `while x is T:` narrows inside the loop body, not after the loop.
- **`switch`**: each `case` narrows `x` to its listed member types, and default `case:` narrows to the remaining types (see [switch](#switch)).
- **Exiting branches**: after an `if`, `x` holds the union of the types from branches that do not exit via `return`, `break`, `continue`, or a `-> never` call such as `panic` (see [Returns](#returns)).
- **`assert`**: `assert c` narrows subsequent code identically to `if not c: panic(...)`, so `assert x is T` leaves `x` a `T` and `assert x != null` removes `null` (see [assert](#assert)).
- **Reassignment**: assigning a value fitting the narrowed type preserves the narrowing; assigning any other member restores the variable's declared union type. Inside a loop, widening a variable that was narrowed *before* the loop is an error because the next iteration assumes the narrowed type: `cannot assign &strview to v inside this loop because it was narrowed to i32 before the loop`. The right-hand side is typed against the variable's declared type (so with `w: i32?` narrowed to `i32`, `w = null if w > 3 else w` restores `null`, and `{...}` targets the declared type).
- **Pointer writes are untracked**: mutating a narrowed variable through a pointer taken earlier (e.g., `p := ptr(x)` before `if x is *P:`, then `p.* = 7`) does not undo narrowing and can read a different member's bytes as the narrowed type. Always modify narrowed variables by name.

### Equality on unions

`==` and `!=` between a union and one of its void members compare the tag (`x == null`, `color == Red`). Comparing against a void type the union can never hold is an error (`Green|Blue can never be Red; this comparison is always false`, or always true for `!=`).

Two values of the same union compare equal when they hold the same member and equal values (see [Operators](#operators)). Two void-only unions compare when every member of one is in the other (such as an enum and a narrowed subset of it). To compare a payload member with a plain value, narrow first: `x is i32 and x == 3`.

## Functions and calls

### Which functions a call sees

A call resolves functions from:

1. Its own module.
2. Global scope (files without a `module` declaration).
3. Modules imported via `use`.
4. **Type-associated modules**: modules declaring any type that appears anywhere in an argument's type, even without `use`. Nested types count too: `a.Table[b.Door]` brings in both `a` and `b`, and `&listview[b.Door]`, `*b.Door`, `b.Door?`, and `func(b.Door) -> bool` all bring in `b`. This allows generic functions in one module to call helpers declared alongside the user's type, including finding a better-matching overload there.

### Arguments

Positional arguments fill parameters in order; named arguments fill parameters by name. Once a named argument is used, all remaining arguments must be named, and no parameter may be filled twice.

- **Default evaluation**: default values are evaluated at each call site, after all explicitly written arguments (see [Order](#order)).
- **What defaults can reference**: earlier parameters (already converted to their parameter types), the function's `$T` parameters (`n: i64 = @sizeof($T)`), constants, contexts, and functions, for example `func slice_of(text: &strview, from: i64 = 0, to: i64 = text.len)`. Referencing a later parameter fails: `default value of parameter a cannot reference b; defaults can only reference earlier parameters`. Defaults are type-checked where the function is declared, even if never called.
- **No control flow in defaults**: because defaults execute in the caller, they cannot contain `try`, `return`, `break`, or `continue`:
  - `default parameter values are evaluated in the caller, so 'try' cannot be used here; handle the error with 'iferror' or 'trust'`
  - `default parameter values are evaluated in the caller, so 'return' cannot be used here; provide a value after 'ifnull'`
- **Call-site `@here()` parameters**: `@here()` in a default evaluates to the call site (`"main.flg:12:5"`). Such a parameter can only be passed by name (`place = p`) so positional arguments are never accidentally swallowed by it: `write(stdout, "name: ", name)` fails with `no overload of write matches (File, &strview, &strview); call-site parameter place must be passed by name: place = ...`.
- **Required parameters after defaults**: allowed, and passed by name:

```flag
func pick(a: i32 = 1, b: i32) -> i32:
    return a * 10 + b

func main():
    print(pick(b = 2)) # 12
```

### Extra arguments

The final parameter may gather any number of trailing arguments of one type as `..items: T`. Inside the function, `items` is a `&listview[T]`. At the call site, the arguments are stored in a temporary array in the caller's frame that lives until the enclosing block ends (like a `ptr` temporary). Each argument converts implicitly to `T` (never via `operator =`). Passing zero extra arguments gives an empty view:

```flag
func sum(..items: i64) -> i64:
    total: i64 = 0
    for x in items:
        total += x
    return total

func main():
    small: i32 = 4
    print(sum(1, 2, small), " ", sum()) # 7 0
```

- **Placement and calls**: `..items` must be last and cannot have a default (`variadic parameter ..items must be the last parameter`). Calls to variadic functions accept only positional arguments (`functions with a variadic '..items' parameter only accept positional arguments`), so an earlier `place: &strview = @here()` parameter always receives the call site.
- **Spreading an existing view**: pass `..values` (last and alone) with a `&list[T]` or `&listview[T]`. The element type must match `T` exactly with no implicit conversion: `no overload of sum matches (&list[i32]); '..' spread has element type i32, which must match '..items: T' without implicit conversion`.
- **Generic variadics**: `..items: $T` infers `$T` from the variadic arguments, so calling it with zero arguments does not match (`no overload of biggest matches (); biggest infers $T from its variadic arguments, but none were passed`).
- **Threads**: `parallel sum(1, 2, 3)` copies the extra arguments into the new thread's heap block so the thread can outlive the caller's stack frame.
- **No function value**: variadic functions cannot be taken as function values (`variadic function sum cannot be used as a function value; wrap it in a non-variadic function`).
- **`print` / `eprint` and C variadics**: `print` and `eprint` are built into the language because their arguments mix types. C variadic functions use `...` instead (see [Variadic functions](#variadic-functions)).

### Choosing an overload

Each viable overload receives a score for each argument:

| Match kind | Score |
|---|---|
| Exact type match (including integer literal to `i64`) | `2` |
| Implicit conversion (`&str` -> `&strview`, text literal -> `cstr`, etc.) | `1` |
| Integer literal to another fitting integer | Slightly `< 1`, decreasing in order `i8`, `i16`, `i32`, `u8`, `u16`, `u32`, `u64` (so `f(3)` picks `f(i16)` over `f(u8)`) |
| Integer literal to float | Near `0` (`f64` slightly above `f32`), so integers always beat floats (`f(3)` picks `f(i32)` over `f(f64)`) |
| Lossless integer widening | Slightly `< 1`, higher for smaller targets (`i16` prefers `f(i32)` over `f(i64)`; equal-width targets tie, so `u8` with `f(i16)` and `f(u16)` is ambiguous) |
| Value entering a tagged union | Score against the best-matching union member |
| Unbound bare `$T` or `type $T` | `0` (binds `$T` to the argument) |
| Parameterized type (`&listview[$T]`, `*$T`, `func($T) -> bool`) | `1 + inner score` (or `inner score + small bonus` when reached via conversion from `&list` or `*array`, still beating bare `$T`) |
| Value `T` auto-addressed for `*T` parameter | `score(T) - 1` |

#### Auto-addressing for `*T` parameters

When a parameter has concrete pointer type `*T` (not a bare `*$T`) and the argument has exact type `T` (no integer widening: `bump(x)` with `x: i16` and `func bump(p: *i32)` fails with `no overload of bump matches (i16)`), the compiler passes its address automatically.

If the argument is not a variable, the compiler copies it into a temporary, and what the function changes there is lost. The temporary lives until the enclosing block ends (just like `ptr(...)` on a non-variable), so `s: &listview[i32] = ptr([1, 2, 3])` and returned interior pointers remain valid for that block. In a loop, the next iteration overwrites the temporary; `-memory` warns if a pointer to it is used past the block (see [The memory checker](#the-memory-checker)).

#### Dominance, inference order, and tie-breaking

1. **Non-literals first, then parameterized `$T`**: non-literal arguments are matched before literals (`biggest(2, u8(7))` binds `$T = u8`). Parameterized `$T` (such as `&list[$T]` or `type $T`) binds `$T` before bare `$T`, so `append(l, k)` with `l: &list[i64]` and `k: i32` widens `k` to `i64`. Otherwise, a bare `$T` (or `$T` in a union) picks the widest compatible integer (`biggest(a, b)` with `a: i32` and `b: i64` is an `i64` in either order; with `i32` and `u32` neither holds the other, and nothing matches).
2. **`where` filtering**: overloads whose `where` clause evaluates to false are removed.
3. **Module-private priority**: if the calling module defines a `priv` function of that name, it selects among its own functions first (see [Modules](#modules)).
4. **Pareto dominance**: an overload wins only if it scores at least as high on *every* argument as every other overload and strictly higher on at least one. Scores are never summed: trading off one argument against another is ambiguous:

```flag
func f(a: i32, b: $T) -> &strview:
    return "fixed"

func f(a: $T, b: i32|&strview) -> &strview:
    return "union"

func main():
    n: i32 = 4
    print(f(n, n)) # error: ambiguous call to f with (i32, i32): no overload is best for all arguments
```

5. **Tie-breakers**: if two overloads tie on every argument, a non-variadic overload beats a variadic one (`f(a: i64)` beats `f(..rest: i64)`), and then closer scope wins: own module > global > imported module. Remaining ties fail with notes pointing to each candidate, and when every candidate comes from a module the file uses, the error says to qualify the call:

```text
error: ambiguous call to show with (i32): multiple overloads match equally
error: ambiguous call to size with (i32): multiple overloads match equally; qualify with a module name: a.size or b.size
```

Declaration order does not matter: functions may call others declared later or in other files. Declaring two functions with the same parameter types and `where` clause in the same module is an error, with a note that points to the other declaration (parameter names, return types, and C symbol names are ignored):

```text
error: main is already declared with the same parameter types
```

A text literal never converts to a `&str` parameter (since `&str` owns its buffer); take `&strview` for read-only text parameters, which accept both `&strview` and `&str`.

### Function values

A function's name evaluates to a function value: an 8-byte function pointer whose type omits parameter names: `func(i32, i32) -> bool`, or `func(i32)` if it returns nothing.

```flag
func double(x: i32) -> i32:
    return x * 2

func apply(f: func(i32) -> i32, x: i32) -> i32:
    return f(x)

func main():
    f := double
    print(f(4), " ", apply(double, 5)) # 8 10
```

- **Optional function types**: `->` binds everything to its right, so `func(i32) -> bool?` returns `bool?`. Write `(func(i32) -> bool)?` (or `func(i32)?` for void return) for a nullable function value, tested with `is`.
- **Overloads and generics**: when a target type is present (typed variable, assignment, `return`, literal field, typed array element, or a union with one function member), the compiler picks the overload with those exact parameter types, instantiating a generic if needed (`top: func(f64, f64) -> f64 = biggest`). Without a target type (`f := show`, call argument), the name must refer to a single non-generic function: `show is overloaded and its type cannot be inferred here; assign it to a typed function variable first`.
- **Exact signatures**: parameters (including defaults) must match exactly; there are no implicit conversions between function types, and return types never select overloads. Functions with `type $T` parameters cannot be function values: `function make has a 'type $T' parameter and cannot be used as a function value; wrap it in a concrete function`.
- **Calling**: `(` calls any expression of function type (`f(x)`, `button.on_click(x)`, `steps[i](x)`, `pick(true)(x)`). `name(...)` invokes a local variable `name` only if that variable has function type; otherwise `name` resolves to functions of that name. A constant holding a function is callable when no function shares its name. Call arguments convert implicitly as usual, but function values have no parameter names or defaults.
- **Uninitialized function values**: calling an empty function value panics with `flag: called an uninitialized (empty) function value`. Passing an empty function value as an argument panics at the call site with `flag: passed an uninitialized (empty) function value as an argument` (so `sort(l, before)` blames your call rather than the library).
- **Equality, printing, and generics**: `==` and `!=` compare values of the same function type; `print` shows the address. A generic parameter such as `less: func($T, $T) -> bool` infers `$T` from the passed function.
- **C functions and closures**: C functions (`extern` or `export func`) have type `extern func(...)`, never `func(...)`: `C function sqrt has type extern func(f64) -> f64, expected Flag function type func(f64) -> f64` (see [C function types](#c-function-types)). Flag `func(...)` values cannot cross the C ABI. There are no closures, nested functions, or anonymous functions; pass state via explicit arguments.

### Inlining

During IR emission, direct calls to small functions are inlined into their callers unless `-debug` is active.

#### Automatic thresholds

A callee is inlined automatically when all of the following hold:

- At most **35 surviving instructions of the backend** (after inlining its own callees), excluding loads, stores, and copies of stack slots of 48 bytes or less (which the backend promotes to registers; see [The backend](#the-backend)). For example, `operator +` on a three-`f32` struct has 23 instructions but only 4 surviving ones (three adds and `ret`), so small-struct operators inline cleanly while functions copying structs over 48 bytes count those copies.
- At most **4 KiB** of local stack variables (an inlined body's variables take room in the caller's frame even when the call never runs, so a function with an array of a million numbers would make every caller's frame that big, and on Windows, whose stack is 1 MiB, the program would stop with `flag: stack overflow`).
- Into a **recursive** caller (direct or mutual), at most **32 bytes** of local stack variables (roughly the cost of the call itself; otherwise 30,000 recursion levels calling a 256-byte helper would consume 8 MiB).
- The caller has at most **500 instructions** (inlining into huge functions slows the backend with little gain).

#### Explicit `inline` and `noinline`

`inline func` and `inline operator` force inlining at every direct call regardless of size (including every instantiation of a generic):

```flag
inline func clamp(x: $T, low: $T, high: $T) -> $T:
    if x < low:
        return low
    if x > high:
        return high
    return x
```

`noinline func` and `noinline operator` are never inlined, keeping their own frame in runtime error stack traces and profilers (calls inside their body still inline normally). Both modifiers go after `priv`. A function cannot have both (`a function cannot be both 'inline' and 'noinline'`), and `extern` functions cannot have either (`an 'extern' function cannot be 'noinline'; its body lives in the external library`).

#### Calls that are never inlined

- **Recursive functions** (direct or mutual). Marking one `inline` is an error with a note on the cycle: `recursive function fact cannot be 'inline'; remove 'inline'`.
- **`main`** (the global entry point; a `main` with parameters or inside a module is an ordinary function), **`export func`**, and **`extern`** functions (`inline` on any of these is an error).
- **`panic`**, whose frame anchors runtime stack traces.
- **Indirect calls** through runtime function values (taking an `inline` function's value keeps an out-of-line copy; calling a constant holding a function is still a direct call).
- **`parallel` calls**, where the spawned thread calls the function out of line.

Inlined calls preserve full call semantics: copied parameters, early returns, `defer`, and call-site `@here()` / runtime error locations. Multiple inlined calls within one caller reuse stack slots, so loops making millions of inlined calls use constant stack space. Debug info attributes lines inside the same file to the inlined body, and lines from other files to the call site (the backend tracks one file per function). Any function whose calls are all inlined and whose address is never taken is omitted from the binary.

## Generics and where

### Type parameters

A `$T` or `$$n` in the return type that no parameter binds fails in the signature, even if nothing calls the function: `unbound type parameter $T; bind it in a parameter, like: kind: type $T`. One that appears only in the body fails at that line: `unbound type parameter $T; type parameters must be bound by a function parameter`. Target types at the call site never infer `$T` (`p: Point = make()` cannot infer `$T`).

#### Explicit `type $T` parameters

To pass a type explicitly, declare a parameter `name: type $T`:

```flag
type Point:
    x: i32
    y: i32

func make(kind: type $T) -> $T:
    x: $T
    return x

func main():
    p := make(Point)
    n := make(i64)
    print(p, " ", n) # Point{x = 0, y = 0} 0
```

- **Accepted arguments**: any type (`Point`, `m.Point`, `Box[i32]`, `Car.motor`, `&list[i32]`, `i32?`, `func(i32) -> bool`, `(i32, bool)`). Variables and constants are values, not types (`no overload of make matches (i64)`).
- **Syntax restrictions**: must be a bare `$T` with no wrapper and no default (`'type' parameter must be a bare type parameter with no default value, like: kind: type $T`), and is allowed only on `func` with a body (not operators, `extern func`, function types, fields, or variables).
- **Zero runtime cost**: `kind` is not a runtime value (`type parameter kind is not a runtime value; refer to the type as $T`); its name exists only for named arguments (`make(kind = Point)`). Inside the body, `$T` is a normal type (`x: $T`, `$T(x)`, `$T{1, 2}`), enabling patterns like `func convert(x: $A, to: type $B) -> $B: return $B(x)`, called as `convert(3, f64)`.
- **Overload resolution**: behaves like a parameterized `$T` (scores `0` when free, fixes `$T` before bare `$T`), so `func check(x: $T, kind: type $T)` widens `u8` in `check(u8(3), i64)` and rejects `check(i64(3), u8)`. Because a void type is both a type and its own value, `make(Red)` matches both `type $T` and `$T` equally (ambiguous if both overloads exist). Contexts are values, not types.
- Functions with `type $T` cannot be function values (see [Function values](#function-values)), but work with `parallel` (`parallel make(Point)`).
- `$$n` cannot be passed by hand; it is always inferred from a sized type such as `array[$T, $$n]` and acts as an untyped integer literal inside the function.

#### Inference and monomorphization

- `$T` is also inferred through fat-pointer conversions (`&list[i32]` or `*array[i32, 3]` -> `&listview[$T]`, allowing `sort(ptr(values))`), generic type aliases (`type Opt[$T] = $T|null`), function types (`func($T, $T) -> bool`), and unions where one member carries `$T` (`$T|null`, `(func($T) -> bool)?`; passing `null` succeeds if another parameter fixes `$T`).
- Each distinct set of type/number arguments generates one concrete instance; repeated sets reuse it. Generic types (`type Pair[$A, $B]`) work the same way, except generic structs cannot contain inline nested struct fields (declare the inner struct separately).
- Non-terminating generic recursion is rejected:
  - Growing function instantiation (`f(x: $T)` calling `f(ptr(x))`): `recursive generic instantiation of f does not terminate (each call produces a larger type)`.
  - Growing type instantiation: `recursive generic type Pair expands infinitely with larger type arguments`, or `type array[...] exceeds the maximum size of 2305843009213693952 bytes` when growth happens inside an array.
- Instantiation errors include a note at the call site: `while instantiating find for this call with $T = Mark`. When a standard library generic fails, the primary error points to your call and a note points inside the library: `failed here inside library generic less with $T = P`.

### where

A `where` clause filters overloads or compiles conditional blocks using compile-time expressions:

- `$T is i8|i16` (type membership in a union)
- `$T == i32` and `$T != i32` (`$T` on either side)
- `@can_assign(&strview, $T)`
- Platform builtins: `@linux()`, `@macos()`, `@windows()`, `@wasm()`, `@x86_64()`, `@arm64()`, `@riscv64()`, `@wasm32()`, `@wasm64()`
- `not`, `and`, `or`, and parentheses
- Any compile-time boolean over types, `$$n`, and constants (`@sizeof($T) >= 8`, `$$n <= 16`, `SCALE > 1.0` with `SCALE: f32 = 2.5`)

Runtime parameter values are not known during overload selection, so `where x > 0` fails: `'where' condition must be a compile-time constant, but x is not known at compile time`.

Platform builtins and `@can_assign` also evaluate to compile-time `bool`s in ordinary expressions (`if @windows():`, `wide := @x86_64() or @arm64()`). Note that `if` type-checks **both** branches on every target, so a branch can only call what every system has, whereas a compile-time `a if c else b` checks only the active side (see [a if c else b](#a-if-c-else-b)).

#### `where` blocks

A `where <condition>:` block is parsed on every target (catching syntax errors everywhere) but type-checked and emitted only when `<condition>` is true, so a C function of one system can be called inside it. There is no `else`; write non-overlapping conditions.

- **Top-level `where` blocks** may contain any declaration except `module` or `use`. Because they are evaluated before declarations are registered, their conditions may test only OS and CPU builtins with `not`, `and`, `or` (same rule as `extern ... where`): `top-level 'where' clauses can only test the OS and CPU, like: @linux() and not @arm64()`.

```flag
where @windows():
    LINE_END = "\r\n"

where not @windows():
    LINE_END = "\n"
```

- **Function-body `where` blocks** hold statements and may use any `where` condition (including `$T`). Inactive blocks emit no code, so each platform block can return cleanly; if no block holds on a target, normal return checking reports `not all control paths return a value`:

```flag
func errno() -> i32:
    where @linux():
        return __errno_location().*
    where @macos():
        return __error().*
    where @windows():
        return _errno().*
```

- **`@compile_error("text")`** aborts compilation with `"text"` at the call site that instantiated the function. Uncalled functions do not trigger it, making it useful for rejecting invalid overloads or unsupported targets inside `where` blocks.

## Operators

Overloadable operators:

- **Expression operators**: `+ - * / % == < << >> and or xor not`
- **Assignment and formatting**: `=` and `+=`
- **Protocol operators**: `[] []= [:] for reverse next with hash` (see [Protocol operators](#protocol-operators))

`!=`, `>`, `<=`, and `>=` are derived from `==` and `<`; `-=`, `*=`, `/=`, and `%=` are derived from `-`, `*`, `/`, and `%`. Declaring a derived or non-overloadable operator fails:

```text
error: operator >= is derived from <; declare 'operator <' instead
```

Unknown operators fail with `operator foo cannot be overloaded; overloadable operators are: + - * / % == < << >> and or xor not = += [] []= [:] for reverse next with hash`.

#### Rules for operator declarations

- **Arity**: binary operators take 2 parameters; `not` takes 1 (it is the only overloadable unary expression operator, e.g. `operator not(b: *Bits[$$w]) -> Bits[$$w]`). Protocol operators take the parameters required by their syntax (`operator [:] expects three parameters (x, a, and b in x[a:b])`). Additional defaulted parameters (such as `place: &strview = @here()`) are permitted.
- **Built-in operators cannot be shadowed**: operators on same-type numbers, `not` on `bool`/integers/`simd`, `*bytes` `+`/`-`, and unreachable mixed-numeric operators are rejected (`'operator +' for i32 and i64 is unreachable; numeric operations are built into the language`). `==` and `<` must return `bool`.
- **`a += b`**: evaluates `b` first, then adds `b` to whatever `a` holds afterward (except on `&str`, where `+=` appends). Unlike `z = z + bump(ptr(z))` (which reads `z` before `bump`, giving `1 + 11 = 12` when `z = 1` and `bump` adds `10`), `z += bump(ptr(z))` adds `11` to the updated `z`, giving `22` (see [Order](#order)). The only declarable `operator +=` is `operator +=(s: *&str, x: T)` for formatting/printing. In `s += a + b`, the right side is formatted first, so `s += "x" + s` appends the previous contents of `s`.
- **Auto-generated `==`**: structs (field by field in order, stopping at the first unequal field, with text by content and a nested struct by its own `==`; `@` shared fields are skipped, their bytes compared through the field they share), tagged unions (matching tag and equal payload), and void types (always true) automatically receive one generated `==` per type. Structs containing an untagged union require a custom `==` (`while generating default '==' for struct Bits, field raw failed`). Arrays, lists, and listviews compare element-by-element; text compares by bytes; `<` (which `sort` uses) is never auto-generated. Types with inline capacity and a `len` field (like `fixmap`) define custom `==` and `operator hash` to compare only active entries.
- **`hash(x)` and `operator hash`**: `hash(x) -> u64` invokes `operator hash`. Default hashes are generated for `bool`, pointers, function values, structs (mixing field hashes), tagged unions (mixing tag and active payload), and arrays. Types with a custom `==` do **not** get an auto-generated hash so equality and hashing stay consistent: `Key defines 'operator ==' but not 'operator hash'; equal values must have equal hashes, so define 'operator hash' for Key`. Unhashable types fail with `&list[i32] is not hashable; define: operator hash(x: &list[i32]) -> u64`. Declaring `func hash` is forbidden: `cannot declare 'func hash'; hash(x) calls 'operator hash', so declare 'operator hash' instead`.
- **`operator =(dest: T, src: U) -> T`**: defines conversions used in assignments, declarations, and `T(x)` (never in function calls). `core`'s `operator =` for `&str`/`&strview` and `&list`/`&listview` copies into `dest`'s existing allocator and runs only in assignments, not new-owner initializations (see [Fat pointers](#fat-pointers)).

### Protocol operators

Language constructs dispatch to operators named by their syntax:

| Syntax | Dispatches to |
|---|---|
| `x[i]` | `operator [](x, i)`. Returning a pointer makes `x[i]` the element, an assignable lvalue; returning a value makes `x[i]` that value when read (e.g., `V|null` on maps), and assignment goes through `operator []=`. |
| `x[i] = v` | When `operator []` returns a value: writes via `operator []=(x, i).* = v` if a 2-param `operator []=` returns a pointer, else calls `operator []=(x, i, v)`. `x[i] += v`, `x[i].f = v`, `x[i][j] = v`, and pointer arguments `f(x[i])` use the same protocol (see [Assignments](#assignments)). |
| `x[a:b]` | `operator [:](x, a, b)`; open-ended `x[a:]` passes `x.len`. |
| `for v in x` | `operator for(x)` once, then `operator next(ptr(state))` each iteration. |
| `for v in reverse x` | `operator reverse(x)` once, then `operator next(ptr(state))`. |
| `with x:` | `operator with(x)` when exiting the block. |
| `{k: v}` | `operator []=`. |
| `hash(x)` | `operator hash(x)`. |
| `print(a, b)` | Formats each argument via `operator +=` into one line, then `write(stdout, line)`. |
| `"a" + x`, `s += x` | `operator +=(s: *&str, x: T)`. |
| `a != b`, `a > b`, `<=`, `>=` | `operator ==` and `operator <`. |
| `t: T = u` | `operator =(dest: T, u: U) -> T`. |

```flag
type Grid:
    cells: array[i32, 9]

operator [](g: *Grid, i: i32) -> *i32:
    return ptr(g.cells[i])
```

Operators are distinct from functions and cannot be called by name, leaving `at`, `put`, `iter`, `next`, and `close` free as ordinary function names. Types that support manual calls keep a regular function beside the operator (e.g., `close(f: File)` called by `operator with(f: File)`, and `put(m, k, v)` beside `operator []=` on maps). `func next` and `operator next` coexist independently; `hash(x)` always invokes `operator hash`.

Before 0.6, these protocols used fixed function names (`at`, `put`, `slice`, `iter`, `reverse_iter`, `next`, `close`, `hash`). Using the old conventions produces migration hints:
- `no overload of [] matches (Grid, i64); x[i] now calls 'operator []' instead of 'at', replace 'func at' with 'operator []'`
- `Tree does not define 'operator for' and cannot be iterated over`
- `'at' has been replaced by 'operator []'; use x[i] for the element or ptr(x[i]) for its pointer`
- `'slice' has been replaced by 'operator [:]'; use x[a:b]`

Integer indexing and `for` iteration on arrays, lists, listviews, and text are built into the compiler and cannot be overridden (even with `$I` or a union containing an integer): `integer indexing x[i] on &list is built into the language; 'operator []' cannot be declared for it`. Neither can `operator for` or `operator reverse` for an array, a list, a listview or text. Custom non-integer indexing (like `l["name"]` on a list) is allowed. `ptr(x[i])` takes an element's address, and `for x in p` iterates through pointers to arrays, lists, listviews, or text.

### Order

Expressions evaluate strictly **left to right**: binary operands (even across precedence, as in `a - b * c`), container before index, callee before arguments, map literal keys and values, user-defined binary operators, call arguments in written order (named or positional) followed by defaults in declaration order, and struct/array literal fields and elements in written order.

There are two exceptions:
1. **Assignments** evaluate the right-hand side before resolving the left-hand target.
2. **`a if c else b`** evaluates `c` first, then only the chosen branch.

Each subexpression reads its storage in turn. If a later call modifies the same variable, the earlier read already captured the prior value (loaded for numbers, copied for aggregates when followed by a call): with `x = 1` and `bump` adding `10` through a pointer and returning `11`, `x + bump(ptr(x))` is `1 + 11`, `pick(x, bump(ptr(x)))` passes `1` and `11`, and `[x, bump(ptr(x))]` is `[1, 11]`.

- Arguments passed by auto-address to `*T` (`append(l, f())`) refer to the variable's storage directly, so the function sees it as every earlier call left it, including calls in later arguments (`append(l, f(ptr(l)))`, where `f` appends `99` and returns `7`, gives `[1, 99, 7]`).
- Views are read in turn too, so `show(l, grow(ptr(l)))` passes a view of `l` captured *before* `grow` (which may dangle after reallocation).
- The container of `x[i]` is read together with its element *after* `i` is evaluated, so `l[grow(ptr(l))]` indexes the list as `grow` left it.

### Assignments

Assignments evaluate the right-hand side first, then compute the target address (`l[0] = grow(l)` writes into the grown list; `a[i()] = v()` runs `v()` before `i()`). Only variables, fields, elements, and dereferenced pointers (`p.*`) are assignable.

#### Multi-assignment and tuple unpacking

Multiple targets can be assigned or declared together using `=` or `:=` (e.g., `a, b = b, a` or `l[i], l[j] = l[j], l[i]`):

- All right-hand values are evaluated before any left-hand target is modified, then assigned left to right with normal assignment conversions (`x, y = 1, 2.5` for `x: u8`, `y: f32`). In `i, a[i] = 1, 5`, `i` becomes `1` first, so `a[1]` is assigned `5`.
- Target types propagate to the right-hand side for variables, contexts, fields, dereferences, and array/list/map elements (`pos, vel = {1.0, 2.0}, {0.5, 0.0}`), but not for targets reached through calls or custom `operator []` (`g[1], n = P{3, 4}, 5`).
- The right-hand side may be comma-separated values or a single tuple (`lo, hi = split(text)`, `lo, hi := split(text)`). Use `_` to discard a position (`_, found := lookup(key)`).
- Counts must match (`expected 3 values for 3 variables, but tuple (i32, i32) has 2`), and only tuples can be unpacked (`cannot unpack Bad|(i64, i64) (only tuples can be unpacked); handle the error first with 'try', 'iferror', or 'trust'`).
- `for i, x in pairs` binds the loop counter and element; it does not unpack tuples.

#### Assigning and mutating through copy-returning `x[i]` (maps)

When `operator [](x, i)` returns a value rather than a pointer (as on `&map` and `fixmap`):

- **`x[i] = v`**: if a 2-parameter `operator []=(x, i)` returns a pointer to the slot (inserting a zeroed entry if missing, as `&map` does), writes through that pointer; otherwise calls `operator []=(x, i, v)`. The target type propagates to `v`, so `m["a"] = show` resolves overloads.
- **`x[i] += v` and nested mutation**: with a pointer-returning `operator []=(x, i)`, `x[i] += v`, `players[name].score += 10`, and `pairs[a][b] += 1` look up the key once and mutate in place (missing keys start at zero, so `counts[word] += 1` works; owners stored as values still need an allocator before growing, e.g. `lines[k] = heap("")`, or `lines[k] += text` panics at runtime). Because the right-hand side runs first and the pointer is used immediately, a right-hand side that grows the map is safe. Without a pointer-returning `operator []=`, `x[i] += v` reads the copy (converting `null` to zero), adds, and writes back, while mutating fields of `x[i]` is disallowed.
- **Fixed maps**: `operator []=` on `fixmap` returns `MapFull?`, which assignment cannot propagate, so `m[k] = v` and `m[k] += v` do not compile; call `try put(m, k, v)` instead.
- **Passing `x[i]` to a `*T` parameter**: copies the element (zeroing `null`), passes a pointer to the copy, and writes the modified copy back to the same key after the call returns (`append(groups[k], v)` works once `groups[k] = heap([])` has run; `-memory` warns if no entry was initialized). The map is untouched during the call, so growing the map inside the callee is safe. Without `operator []=`, passing `x[i]` to a pointer parameter fails.
- **`ptr(x[i])`**: with pointer-returning `operator []=(x, i)` (`&map`), inserts a missing key at zero and returns a pointer directly into the hash table (`q := ptr(m["a"]); q.* = P{50}`). The pointer invalidates if the map later grows (reported by `-memory`). Without it (`fixmap`), `ptr(x[i])` points to a temporary copy that is never written back.

## SIMD

`simd[T, n]` packs `n` lanes of numeric type `T` into one or two 16-byte vector registers:

- **16-byte types**: `simd[f32, 4]`, `simd[i32, 4]`, `simd[u32, 4]`, `simd[f64, 2]`, `simd[i64, 2]`, `simd[u64, 2]`, `simd[i16, 8]`, `simd[u16, 8]`, `simd[i8, 16]`, `simd[u8, 16]`.
- **32-byte types**: `simd[f32, 8]`, `simd[i32, 8]`, `simd[u32, 8]`, `simd[f64, 4]`, `simd[i64, 4]`, `simd[u64, 4]`, `simd[i16, 16]`, `simd[u16, 16]`, `simd[i8, 32]`, `simd[u8, 32]`.
- Any other total byte size is rejected: `simd[f32, 16] has size 64, but simd must be 16 or 32 bytes (one or two 16-byte registers); use simd[f32, 8], or an array of them`.

#### Masks

A SIMD mask explicitly specifies lane width: `simd[bool8, n]`, `simd[bool16, n]`, `simd[bool32, n]`, or `simd[bool64, n]`. Each lane is stored as a 1-, 2-, 4-, or 8-byte integer with all bits set (`true`) or zero (`false`), matching the width of the lanes compared (e.g., comparing `simd[f32, 8]` or `simd[i32, 8]` yields `simd[bool32, 8]`; comparing `simd[f64, 2]` yields `simd[bool64, 2]`). Indexing a mask lane returns a `bool`.

`bool8` through `bool64` may only appear as `simd` lane types (`bool32 is a simd mask lane type and can only appear inside simd: simd[bool32, 4]`), and plain `bool` is rejected (`simd[bool, 4] cannot use 'bool' lanes; a simd mask must specify lane width: simd[bool32, 4] or simd[bool64, 4]`).

#### Alignment and slice transfers

All `simd` types have 16-byte alignment (matching C `__m128` for 16-byte vectors, so a struct with a 16-byte `simd` has the layout C gives it; 32-byte vectors are also 16-byte aligned, unlike C `__m256`'s 32-byte alignment). Lists, `heap`, and arenas return 16-byte-aligned memory, and hardware loads do not require alignment.

A `simd` loads from a slice via `core`'s `operator =` and stores back into a slice via slice assignment:

```flag
v: simd[f32, 4] = values[i:i + 4] # or simd[f32, 4](values[i:i + 4])
values[i:i + 4] = v * k + 1.0
```

- Both loads and stores check slice length at runtime and panic on mismatch: `flag: simd with 4 lanes requires a slice of length 4, got 5`.
- Lane and slice element types must match (`cannot store simd[f32, 4] into a slice of i32; lane and element types must match`), and no non-`simd` value may be assigned to a slice (`slices are views and cannot be assigned directly (except from a matching simd); assign elements individually`).
- Unchecked transfers can cast an element pointer: `chunk := ptr(values[i]) as *simd[f32, 4]`, `chunk.* = chunk.* * k`.

### Operators on lanes

Operators act elementwise between two `simd` values of the same type, or between a `simd` and a matching scalar broadcast to every lane (`v * k`, `1.0 - v`, `v < 0.0`):

- **`+`, `-`, `*`, unary `-v`**: all numeric lanes; integers wrap on overflow.
- **`/`**: floating-point lanes only. Integer SIMD has no `/` or `%`: `'/' is not supported on integer simd (i32 lanes) because hardware lacks integer vector division; convert to an array instead`.
- **`and`, `or`, `xor`, `not`**: integer lanes (bitwise) and mask lanes. Mask operations never short-circuit, and the mask must appear on the left (`b and m` with `b: bool` first treats `b` as a boolean condition).
- **`<<`, `>>`**: shifts every integer lane by one scalar shift count (`v << 3`), following scalar shift rules.
- **`<`, `<=`, `>`, `>=`, `==`, `!=`**: return a mask of matching lane width, with exact IEEE `NaN` behavior.

Built-in lanewise operators cannot be redeclared (`lane-wise 'operator +' for simd[f32, 4] and f32 is built into the language and cannot be redeclared`), though operators mixing `simd` with user-defined types are allowed (`operator *(m: Mat4, v: simd[f32, 4])`).

Because `==` produces a mask rather than a `bool`:
- Conditions reject masks (`if v > 0.0:` errors and points to `any(...)` and `all(...)`).
- Structs (including those with arrays of `simd`) and unions containing a `simd` receive no auto-generated `==` or `hash`: `cannot generate '==' for struct Contact because it has a simd field whose '==' returns a mask; declare 'operator ==' for Contact`.
- Arrays of `simd` cannot compare directly: `cannot compare array[simd[f32, 4], 2] directly because simd '==' produces a lane mask; compare elements with all(a[i] == b[i])`.
- `simd` values cannot be hashed directly as map keys: `simd[i32, 4] cannot be hashed because simd '==' produces a lane mask; convert it to an array: array[i32, 4](v)`.

### Lanes, literals and conversions

- **Lane indexing**: `v[2]` and `v[2] = x` require a compile-time constant index (literal or constant, typed or untyped): `simd lane index must be a compile-time constant; convert to an array for dynamic indexing`. A mask lane evaluates to a `bool` copy (like a map element), so `ptr(m[0])` points to a copy and mutating calls are rejected.
- **Literals and shuffles**: `[...]` adopts a target `simd` type (including inside `T(...)`, e.g., `zero: simd[f32, 4] = [0.0, 0.0, 0.0, 0.0]`, checking element ranges like `300 does not fit in u8`). Elements may read lanes of other vectors to shuffle: `t: simd[f32, 4] = [r0[0], r1[0], r0[1], r1[1]]`.
- **`T(x)` conversions**:
  - Scalar broadcast: `simd[f32, 4](k)`.
  - Array <-> `simd`: `simd[f32, 4](a)` and `array[f32, 4](v)`.
  - `simd` <-> `simd` with equal lane counts: converts each lane like scalar `T(x)` (float-to-int truncates toward zero and saturates; supports width changes between 16 and 32 bytes, e.g., `simd[f64, 4](v)` from `simd[f32, 4]` or `simd[i32, 8](w)` from `simd[i16, 8]`). Mask-to-mask conversion preserves each lane's `bool`.
  - Other sources fall back to `operator =` (such as `core`'s slice load `simd[f32, 4](values[4:8])`), failing otherwise with `simd[f32, 4]() only converts numbers, arrays, and simd values, not &strview, and no 'operator =' makes simd[f32, 4] from it`.
- **`as`**: bitcasts between `simd` and `array` types of the same byte size (`v as simd[u32, 4]`).
- **Built-in vector operations**: `select`, `any`, `all`, `sum`, `min`, `max`, `abs`, and `sqrt` (including `math.sqrt`) are built into the language with portable semantics across platforms (`min`/`max` match Flag scalar comparisons; `sum` reduces in a fixed order; see [simd](Library.md#simd)). Calls with a different arity or qualified via a user module resolve normally (e.g., a custom `func max(v: simd[f32, 4]) -> f32`).

### In the processor

On `x86_64` (SSE up to SSE4.2 / `x86-64-v2`), `arm64` (NEON) and WebAssembly (SIMD128), most operations compile to one or a few vector instructions (two for 32-byte vectors held in two 16-byte registers), keeping values in registers including across inlined calls. For 32-byte vectors, `sum`, `any`, and `all` fold the two 16-byte halves with one vector instruction first. `v[i]` reads or updates a lane directly in the register with a single instruction.

- **Scalarized operations**: `*` on 8-bit and 64-bit lanes, `abs` on `i8` and `i64`, `<<`/`>>` on 8-bit lanes, `>>` on `i64`, all lane conversions except `i32` -> `f32`, shuffles, `sum`, `any`, and `all` operate lane by lane.
- **`riscv64`**: has no vector backend here and executes every operation lane by lane with identical results.
- **WebAssembly**: a register is a `v128`. A shift by a count past the bits of a lane gives `0`, or the sign for `>>` of a signed lane, as on the processors, where SIMD128 alone would take the count modulo the bits, and the `min` and `max` of floats are `pmin` and `pmax`, which give the first lane unless the second is less, or greater, as Flag does.
- **Startup CPU check (`x86_64`)**: `x86-64-v2` (SSE4.2) is present on Intel CPUs since 2008 and AMD CPUs since 2011-2013. Any program using `simd` checks CPUID at startup and exits with code `132` if unsupported: `flag: this program needs an x86-64-v2 processor (SSE4.2)`.
- **Register allocation notes**: taking `ptr(v)` keeps `v` in memory. A `simd` alive across a call goes to the stack on Linux and macOS, where a call keeps no whole vector register (`arm64` keeps only the low half of `v8` to `v15`), and can stay in `xmm6` to `xmm15` on Windows, which a callee keeps whole. A struct of 48 bytes or less that a function returns stays in registers and goes to memory at the `return`. `simd` values cannot cross the C ABI by value (pass a pointer).

## Iteration

### Ranges

- `a..b` iterates `a` up to `b - 1`; `a..=b` iterates `a` up to `b`.
- `reverse a..b` iterates `b - 1` down to `a`; `reverse a..=b` iterates `b` down to `a`.
- If `b <= a` (for `..`) or `b < a` (for `..=`), the loop body runs zero times.
- Bounds are evaluated once before the loop. The loop never increments past the final value, so `a..=b` safely reaches a type's maximum (`for b in 0..=last` with `last: u8 = 255` runs 256 times, whereas `0..last + 1` wraps to `0..0`).
- Both bounds must be integers (`range bounds must be integers, got f64`). The loop variable takes the non-literal bound's type, `i64` if both are literals, or the wider integer when mixed (`i32` and `i64` -> `i64`). Incompatible bounds fail: `range bounds i32 and u32 cannot be mixed because neither contains all values of the other; convert one end with i32(...) or u32(...)`.
- The loop variable is read-only (`cannot assign to loop counter i; copy it to another variable if you need to modify it`), and `ptr(i)` or a `*T` parameter receives a copy. Because the counter cannot be mutated, `a[i]` elides bounds checks when `a` is an array and the range has literal bounds within `a`'s length (`0..4` or `0..=3` on `array[i32, 4]`).
- Adding an index variable to a range loop is an error (`cannot add an index variable to a range 'for' loop; the range value is already the index`), and ranges may appear only in `for` headers and `switch` `case` labels (`ranges can only be used in a 'for' loop header or a 'case' label`).

### Everything else

For arrays, lists, listviews, and text, the compiler generates the loop directly, capturing the data pointer and length once before iterating. For any other type, `for x in value` calls `operator for(value)` once (or `operator reverse(value)` with `reverse`), stores the iterator state, and calls `operator next(ptr(state))` each iteration until it returns the built-in void type `IterationEnd` (allowing elements to be `null` or any union).

`for i, x in ...` adds a read-only `i64` counter:

- Over arrays, lists, and listviews, `i` is the element index, including under `reverse`, where it counts down (`2 30`, `1 20`, `0 10` for `[10, 20, 30]`).
- Over text, `i` counts characters from `0` (not bytes); over custom iterators, it counts iterations. Because neither knows the reverse index up front, `reverse` with a counter is rejected: `'reverse' over &strview cannot provide index i (only arrays, lists, and listviews have reverse indices); track the count in a separate variable`.

Container-specific details:

- **Text**: yields each UTF-8 character as a `&strview` (e.g., `"é"` has `len == 2`). Invalid bytes (including bytes of overlong encodings, surrogates, or code points above U+10FFFF) are yielded one byte at a time, identically in forward and `reverse` order. Compiler diagnostics decode source text the same way (rendering stray bytes as `\xc0`). Use `items(text)` to iterate raw `u8` bytes.
- **Maps**: `&map` yields `entry[K, V]` copies (`.key`, `.value`) in hash-table order and cannot be reversed (`&map[&strview, i32] does not define 'operator reverse' and cannot be iterated over`). `fixmap` yields entries in insertion order.
- **Arrays and pointers**: arrays iterate by address without copying and support `reverse`. Pointers to arrays, lists, listviews, or text iterate through the pointee automatically.
- **Mutable vs read-only loop variables**:
  - Over an array, list, or listview variable, `x` aliases the element in place: `x = P{7}`, `x.hp -= 2`, `ptr(x)`, `*T` calls, and `for s in names: free(s)` all modify the container directly. `x` still narrows with `is` and `case`.
  - Assigning to non-writable loop variables fails with specific guidance:
    - Text: `cannot assign to loop variable c; it is a read-only view into the text (build a new &str instead)`
    - Map / custom iterator: `cannot assign to map loop variable e; it is a copy and would not update m (assign directly via m[e.key] = v)`
    - Non-variable array/list (constant, call result, or slice): `cannot assign to loop variable x because the iterated expression is not a variable; assigning would only mutate a temporary copy`
- **Mutating the container mid-loop**: because length and pointer are captured at loop entry, elements appended during the loop are not visited, and growing the list reallocates its buffer, leaving `x` dangling. Overwriting a union element through the list (`u[i] = "text"` or a mutating call) can also invalidate an `is` narrowing on `x`. `-memory` warns about both (see [The memory checker](#the-memory-checker)).

## Control flow

### a if c else b

Evaluates `c` first, then evaluates only the selected branch (`n := 100 / x if x != 0 else 0` never divides by zero). Narrowing applies from `c` into `a` and from `not c` into `b` (`name := user.name if user is User else "nobody"`).

`a if c else b` has the lowest precedence (`a or b if c else d` means `(a or b) if c else d`) and associates to the right (`a if c else b if d else e` means `a if c else (b if d else e)`). Omitting `else` fails: `unexpected end of line; inline 'if' requires an 'else' branch: a if c else b`.

Both branches take the type the context expects: a variable, field, return value, or a parameter on which every overload of that call agrees on one number type (`f(1 if x else -1)` passes an `i32` to `f(v: i32)`). With overloads that differ there, the literals stay `i64`. A text conditional assigned to an existing `&str` copies into its buffer like `s = "a"` does; a new `&str` still needs `heap(...)` or `arena(a, ...)`.

#### Result typing

Result types follow the same rules as `ifnull` (see [Errors](#errors)):

- **With a target type** (`x: i64 = 1 if c else 2`, `return`): both branches convert to the target. When targeting a union, the expression's type includes only the members actually produced (so `x: i32? = 1 if c else 2` has type `i32` and keeps a narrowed variable narrowed, while `null if c else 2` is `i32?`).
- **Without a target type**: a literal branch adapts to the other branch's type; otherwise `a`'s type wins if `b` converts to it; two compatible integers take the wider type; otherwise the result is the union of both (`0 if c else small` -> `u8`, `small if c else big` -> `i64`, `1 if c else 2` -> `i64`, `1 if c else "one"` -> `i64|&strview`).
- **Diverging branch (`-> never`)**: takes the type of the other branch (`n := given if given is i64 else panic("no number")`).
- **Void branch**: rejected (`a void function call cannot be used as a value`).

#### Compile-time conditions

When `c` is a compile-time constant, only the chosen branch is type-checked and emitted; the other branch is parsed for syntax only (like an inactive `where` block). Compile-time conditions include literals, typed or untyped constants, platform builtins (`@linux()`, etc.), `@can_assign`, compile-time arithmetic/conversions/comparisons (`@sizeof($T) == 8`, `$$n > 4`, `LEVEL * 2 > 3`, `SCALE > 1.0`), and `not`/`and`/`or` over those.

Consequently:
- `19 if @linux() else 8` evaluates to an untyped integer literal (`19` on Linux, `8` elsewhere), usable for `i32` parameters and array lengths (`N = 19 if @linux() else 8`).
- `windows_name() if @windows() else "elsewhere"` compiles on Linux even though `windows_name` exists only on Windows.
- `1 if @linux() else 2.5` has type `i64` on Linux and `f64` elsewhere.
- Short-circuit `and`/`or` with a compile-time left operand fold the same way: `@windows() and f()` evaluates to `false` on Linux without type-checking `f()`.

### switch

Only the first matching `case` runs; there is no fallthrough. The default `case:` must come last. The subject evaluates once, and case values evaluate in order until one matches (skipping remaining cases and remaining items in `case a, b`). `break` inside a `switch` exits the enclosing loop.

A `switch` on an integer does not test its literal values in order, which gives the same result since a literal evaluates to nothing else, and a range of literals that holds from 1 to 128 numbers counts as those numbers. Any other value, like a variable, a call, or a range that is empty or holds more, is still tested in its place: the literals before it are searched first, and those after it once it fails. Where 8 or more literals are dense, filling at least a third of the numbers from the smallest to the largest, the `switch` jumps through a table: one load and one jump whatever their number. A loop that calls a `switch` of 256 cases takes 47% fewer cycles than by halves on random values, 45% fewer on values in order. The table holds distances within the code, so it needs no relocation, also in a `-shared` library, and on WebAssembly it is a `br_table`, which also checks the bounds of a subject of 32 bits or fewer. Elsewhere it finds them by halves while more than 32 remain, then one by one: while 8 or more remain, first those on the short side of the widest gap between two of them, so that a few values far from the rest leave the rest to a table (`'\n'`, `' '` and `'a'..='z'` take two comparisons and a table). A table at an index known when compiling, like that of an inlined call with a constant, is a plain jump.

#### Value switches

Over any non-union type, each case expression is compared via `==`. Cases may contain arbitrary expressions, ranges (`a..b` tests `>= a` and `< b`; `a..=b` tests `>= a` and `<= b`, e.g., `case 'a'..='z':` or `case 128..=255:` on `u8`), and comma-separated combinations. A `switch` on `bool` with both `case true:` and `case false:` is exhaustive for return checking.

#### Tagged-union switches

Over a tagged union, cases name member types: single types, comma-separated lists, or unions (`A|B`). Inside each case, a local variable or parameter subject is narrowed to those member types (the default `case:` gets all remaining members). After the `switch`, the variable holds the union of members from cases that do not exit. Non-variable subjects are matched the same way without narrowing.

```flag
func area(s: Circle|Square|null) -> f64:
    switch s:
    case Circle:
        return 3.14 * s.r * s.r
    case Square:
        return s.side * s.side
    case null:
        return 0.0
```

Union `switch` rules:
- Every listed type must still be possible: `unreachable 'case': i32|f64 can never be null`.
- No member may appear twice: `duplicate 'case' for null; each member type can only be handled once`.
- Exhaustiveness is required: `non-exhaustive 'switch': unhandled null|i32 and no default 'case:'`. A default with no remaining members fails: `unreachable default 'case:'; all members of Red|Green|Blue are already handled above`.
- Values and variables are rejected as cases on unions: `'switch' over union null|i32 matches member types, not values; match the type in 'case' and compare the value inside`; `fav is a variable, but 'switch' over union Red|Green|Blue matches member types; compare values with '==' in an 'if'`.
- Type cases are rejected on non-unions: `type 'case' is only allowed when switching on a tagged union, not i32`.

Case items are interpreted once the subject's type is known (and per instantiation in generics), so `Red`, `m.Thing`, `Car.motor`, and `Pair[i32]` parse as types on a union subject, while `limit`, `p.x`, and `t[0]` parse as values on a numeric subject.

**Code generation**: a tagged-union `switch` loads the tag once and finds the explicit case tags as a `switch` on integers finds its values: through a table where they are dense, as the fixed tags of the built-in types are, and by halves while more than 32 remain, then one by one as there (about 17 comparisons for 120 types instead of 60). Default-case members are left out of the search, so matching a few variants plus a default on a large enum takes only a few checks. Debug line info attributes the dispatch to the `switch` line.

### break and continue

`break` and `continue` target the innermost loop (equivalent to `break 1` / `continue 1`). There are no loop labels; pass a literal integer depth to target outer loops (`break 2`, `continue 2`).

- Zero fails: `'break 0' is invalid; loop depth starts at 1 (the innermost loop)`.
- Exceeding active loop depth fails: `'break 3' exceeds the number of enclosing loops (maximum is 2)`.
- Outside any loop fails: `'break' can only be used inside a loop`.

### defer

- Deferred statements execute in LIFO (reverse) order whenever their enclosing block exits, normally or via `return`, `break`, or `continue`. Inside a loop body, they run at the end of every iteration.
- A `defer` evaluates at exit time, reading the values and narrowed types variables hold then (e.g., after `defer x = "text"`, `x` is no longer narrowed to `i32`).
- It can see only variables declared before the `defer` (plus variables declared inside the `defer`).
- It cannot contain `return`, `break`, `continue`, or `try` (even inside a nested loop), and cannot itself be a variable declaration or another `defer`.
- Error handling applies normally: discarding an error fails with `deferred statement discards an error that 'defer' cannot propagate; explicitly ignore it with 'defer _ = ...' or run it without 'defer' and handle the error`. Write `defer _ = f()` to discard deliberately.
- A deferred assignment into an error-bearing variable (`defer x = f()`) must still be read after the `defer` runs, so writing it in `x`'s own block fails because nothing follows to read `x`.

### with

`with value:` invokes `operator with(value)` when the block exits, including on early exit (see [Protocol operators](#protocol-operators)). When `value` is a variable, it evaluates what the variable holds at block exit (like `defer`).

Any non-error return value of `operator with` is discarded, but an error return is forbidden: `'operator with' on Lock can return an error, which 'with' would discard; close it manually and handle the error`. Types lacking the operator fail with `Thing does not define 'operator with' and cannot be used in a 'with' block`. `with scratch():` uses this mechanism (see [Memory](#memory)).

### Returns

- **Exhaustive returns**: every path through a non-void function must return (`not all control paths return a value`). An infinite `while true` loop with no `break` counts as never reaching its end.
- **Unreachable statements**: code after `return`, `break`, or `continue` in the same block fails with `unreachable statement; the preceding code always exits this block`.
- **`main`**: takes no parameters, must be global (outside any module), and returns nothing (exit code `0`) or an integer (exit code). It is the C `main` entry point and cannot be called or taken as a value: `'main' is the program entry point and cannot be called or used as a value`. A `main` whose `where` does not hold on the target is not the entry point, so `func main() where @linux():` and `func main() where @windows():` each build for their system; two that hold fail: `main is declared twice for this target; make the 'where' conditions of the two exclusive`.
- **`-> never`**: marks functions that never return (like `panic` or `exit`). Calls to them act as path-ending statements (narrowing like `return`), may appear on the right of `ifnull`/`iferror` (`port := parse_int(text) ifnull panic("not a number")`) or as one branch of `a if c else b`, and are rejected anywhere else a value is required (`a call to a function returning 'never' is a statement, not a value`). A `-> never` body cannot `return` or fall off the end (`function returning 'never' can reach the end of its body; every path must end in a diverging call like exit() or panic()`). `extern` C functions may return `never`. `parallel` of a `-> never` function returns `Thread[null]`. `never` cannot be a variable, parameter, or union member type (`'never' can only be a function return type, not a value type`), except inside empty fat pointers (`&list[never]`, `&map[never, never]`).
- **Returning stack addresses**: returning `ptr(...)` of a local variable, parameter, field/element thereof (without an intervening pointer), or temporary, even through `as`, fails immediately: `cannot return a pointer to stack memory; it becomes invalid when this function returns`. Indirect escapes (`p := ptr(x); return p`) are caught by `-memory`.
- **Multiple returns**: `return 0, n` constructs and returns the tuple `{0, n}` (see [Tuples](#tuples)). Inside brackets, `return` after `ifnull`/`iferror` takes only a single expression so commas still separate call arguments (`sum(parse_int(t) ifnull return -1, 2)`). Returning multiple values from a single-return function fails: `cannot return multiple values; this function returns i64`.
- **Discarding values**: `_ = value` explicitly discards a value (even if a variable named `_` is in scope). Non-call expression statements (like `x == 2` or `a[0]`) fail with `unused expression value` (and for `x == 2`, adds `; did you mean assignment '=' instead of '=='?`).

## Errors

An error type is declared with `type error`: a void type (`type error NotFound`), an enum (`type error enum FileError = CannotOpen|CannotRead`), or a struct carrying payload fields:

```flag
type error BadLine:
    line: i64
    why: &strview
```

Error structs behave like normal structs: they can be generic, constructed with `BadLine{3, "too long"}`, printed (`BadLine{line = 3, why = too long}`), compared with `==`, and narrowed via `is` or `case`. A union can hold multiple error types with or without data, and `try`, `iferror`, and `trust` handle all of them uniformly. Type aliases cannot be marked `error`; mark the underlying type.

If an error payload owns memory (such as a `&str` field), the handler is responsible for freeing it; discarding the error with `_ =` or `iferror` leaks that memory (`-memory` warns).

#### Mandatory handling

- Discarding an error-bearing expression statement fails:

```text
error: discarded error value; test with 'is', propagate with 'try', or explicitly ignore with '_ ='
```

- Binding an error-bearing value to a variable requires checking it with `is` before using its value, and reading it; unread error variables fail with `error-bearing variable x is never checked; test with 'is', propagate with 'try', or discard with '_ ='`. (The check verifies that the variable is read at least once, not every reassignment; overwriting before reading silently loses the earlier error.) An unread `Thread` variable similarly fails with `thread handle t is never awaited or used; call wait(), or detach it with a standalone 'parallel f(x)'`.
- A `switch` over the value handles its errors in explicit cases or the default.

#### `try`, `trust`, `iferror`, and `ifnull`

- **`try`**: unwraps the non-error value and immediately returns any error from the enclosing function. Every error member must be in the function's return type:
  - `'try' propagates error Bad, which is not in this function's return type i32`
  - `'try' only handles error types, and i32|f64 contains no error`
  - `'try' only handles error types, and null|i64 contains no error; propagate null with: ifnull return null`
- **`trust`**: strips all error members (or `null` if there are no errors) *without* checking the tag. Using `trust` on an actual error yields garbage and can crash.
- **`a iferror b` / `a ifnull b`**: evaluates to `a` without its errors (or `null`), or lazily evaluates `b` if `a` held an error (or `null`).
  - If `b` is a void call, the whole construct acts as a statement running `b` on failure.
  - Otherwise, if `b` converts to `a`'s unwrapped type, that type is used; compatible integers take the wider type (`i32?` with `i64` -> `i64`); else the result is their union.
  - Precedence is tighter than comparisons and looser than arithmetic: `m[k] ifnull 0 > 3` compares `(m[k] ifnull 0) > 3`, while `m[k] ifnull 0 + 1` means `m[k] ifnull (0 + 1)`.
- **Control flow on the right**: the last link of an `ifnull`/`iferror` chain may be `return`, `break`, or `continue`, removing the error or `null` from the expression's type:

```flag
cut := find(line, "=") ifnull return BadLine{i + 1, "it has no ="}
n := parse_int(text) ifnull return null # passes the null up
for item in texts:
    total += parse_int(item) ifnull continue
```

  `return` consumes the rest of the expression as its value. In a `while` condition, `break`/`continue` target that `while`; in a `for` header (evaluated once before the loop), they target the enclosing loop. Anywhere else in an expression, control-flow statements are rejected: `'return' is a control-flow statement and cannot be used as a value (except directly after 'ifnull' or 'iferror')`.
- **Assignment on the right**: when `a ifnull b` or `a iferror b` is an entire statement, the final link may be an assignment (`parse_int(text) ifnull bad += 1`). In a value context it fails: `an assignment after 'ifnull' produces no value, leaving n uninitialized`.

### assert

`assert c` stops the program when `c` is false with exit code `134`, source location, and stack trace (like `panic`). Its automatic message formats failing values using `print`:

- **Comparisons**: prints each non-constant runtime operand (`flag: total == 4 is false, total is 3`; `flag: first(l) == last is false, first(l) is 1 and last is 2`).
- **`x is T`**: prints the actual active member and its value (`flag: f is FsError, not File: cannot open config.txt: No such file or directory`; `flag: last is null, not i32`).
- **Other expressions**: `flag: ready is false`, or `flag: assertion failed` for `assert false`.

`assert c, message` prints `message` (any printable value, evaluated only on failure) instead: `assert total > 0, "the total cannot be empty"`.

Subsequent code is narrowed as if `if not c: panic(...)` had executed (see [Unions and narrowing](#unions-and-narrowing)):

```flag
f := open_file("config.txt")
assert f is File
print(trust read_line(f))
```

Unlike `panic`, `assert false` does not terminate control-flow checking, so a non-void function must still return afterward (use `panic` for unreachable code). `assert` is a reserved keyword.

### Runtime errors

Fatal runtime checks and OS faults write to stderr and terminate the process:

| Message | Exit code |
|---|---|
| `flag: index 5 out of bounds for length 3` | 134 |
| `flag: slice 2:9 out of bounds for length 3` | 134 |
| `flag: division by zero` | 134 |
| `flag: total == 4 is false, total is 3` (or custom message from `assert`; see [assert](#assert)) | 134 |
| `flag: <text>` (from `panic`) | 134 |
| `flag: called an uninitialized (empty) function value` | 134 |
| `flag: passed an uninitialized (empty) function value as an argument` | 134 |
| `flag: File{-1} is not an open file descriptor` | 134 |
| `flag: cannot send on a closed channel` | 134 |
| `flag: deadlock: channel blocked while holding its own queue lock ('with lock(c.queue)'), preventing other threads from making progress` | 134 |
| `flag: 'wait' called on an uninitialized (never started) Thread` | 134 |
| `flag: 'done' called on an uninitialized (never started) Thread` | 134 |
| `flag: 'wait' called on a Process that is not running (never started or already waited)` (same for `done`, `kill`) | 134 |
| `flag: cannot read random bytes: ...`, when the system gives none | 134 |
| `flag: failed to start thread: OS refused thread creation` | 134 |
| `flag: this &list has no memory to grow in: give it heap([]) or arena(a, [])` (same for `&str`, `&map`; see [Fat pointers](#fat-pointers)) | 134 |
| `flag: out of memory` | 9 |
| `flag: cannot write the standard output: No space left on device` (at program exit) | 1 |
| *(no message)* on Linux/macOS when writing to a closed pipe (`program \| head`): `SIGPIPE` terminates the process (sockets and child-process pipes return errors instead) | 141 |
| `flag: invalid memory access at 0x0` | 139 |
| `flag: stack overflow` | 139 |
| `flag: illegal instruction` | 132 |
| `flag: arithmetic error` | 136 |
| `flag: aborted` (C `abort()`, e.g., double free, after libc's message, no place) | 134 |
| `flag: the test takes more than 10 s` (in `flagc test`) | 142 |
| `flag: this program needs an x86-64-v2 processor (SSE4.2)` (startup check for `simd`) | 132 |

#### Panic format and `on_panic`

Exit-134 errors print the location (`  --> main.flg:12:11`) followed by the call chain of your functions (with declaration line numbers). Uninitialized-owner growth, channel queue-lock deadlocks, and `panic(message, place = "")` omit the location line and go straight to the call chain; for an owner that grows without memory, the chain starts at your function that grew it:

```text
flag: index 5 out of bounds for length 3
  --> main.flg:12:11
  in parse_line, main.flg:9
  in load, main.flg:20
  in main, main.flg:31
```

All exit-134 errors route through `core`'s `panic`. Calling `on_panic(f)` registers a hook `func(message: &strview, place: &strview)` that runs after the message and call chain are printed and before termination (a new `on_panic` replaces the previous one):

```flag
func save(message: &strview, place: &strview):
    eprint("saving the game before ", message)
    save_game()

func main():
    on_panic(save)
```

- The hook runs on the failing thread while other failing threads wait, so it must not acquire locks another thread might hold.
- It may terminate with `exit(code)`; a panic inside the hook aborts immediately with `134`.
- `on_panic` is **not** called for `out of memory`, hardware crashes, or test timeouts.
- Compiler-generated runtime checks (bounds, division by zero, `assert`, empty function values) always call `core`'s `panic`, even if you declare your own `func panic`.

#### Out of memory and stdout errors

`flag: out of memory` prints no location or call chain, flushes Flag's buffered output without allocating, and exits with `9`. It ends the process at once, so C `atexit` functions do not run and C `stdio` buffers are not flushed. Negative sizes (such as `reserve(l, -1)`) also trigger it. Use `try_reserve` to receive `NoMemory` instead of aborting (see [Memory](Library.md#memory)).

`flag: cannot write the standard output: ...` is printed at exit if buffered stdout writes failed and no `flush(stdout)` already reported the error.

#### Hardware crashes and call chains

The four crash rows (`invalid memory access`, `stack overflow`, `illegal instruction`, `arithmetic error`) are caught from OS signals/exceptions:

- An `invalid memory access` prints the faulting address; an `illegal instruction` most often comes from a union with an invalid tag (such as zeroed `calloc` memory; see [Unions](#unions)), whose `switch` reaches the trap instruction (`ud2`, `udf`, or `unimp`) that the compiler puts where no code should run; deep recursion produces `stack overflow`.
- Crashes print the call chain without a location line. Exit codes on Unix are `128 + signal` (table shows Linux; `SIGBUS` is `135` on Linux and `138` on macOS). On Windows, the exit code is the exception code (e.g., `0xc0000005`). Because the handler exits cleanly rather than re-raising, no core dump is created; run under a debugger to stop at the fault (see [Debugging](#debugging)).

Call chain behavior:
- Shows only user functions (library functions are omitted because the location already points to your call).
- Collapses repeated recursive frames: `in walk, tree.flg:3, 52341 times`.
- For crashes inside C (e.g., `strlen` on a bad `cstr`), unwinds from the return address or frame pointers back to your caller (C libraries built without frame pointers, such as Ubuntu 22.04's libc, yield no chain for crashes deep inside functions like `malloc` or `printf`).
- Stops at any C frame above your functions (such as C callbacks).
- Prints at most **20 lines**, summarizing the rest as `  ... and 12 more`.
- Omits inlined functions (declare `noinline` to preserve them; see [Inlining](#inlining)).
- Shows only the failing thread's stack, ending at the `parallel` entry function.

Every function preserves frame pointers, and a function table maps return addresses to names and locations (~50 bytes per emitted function; ~1% of compiler binary size including frame pointers). On Windows each function also carries the unwind information of the system (`.seh_*`), which Windows and its debuggers read to find the caller and the registers kept for it from anywhere in the function. Crash handlers are installed before `main`, and every thread (including `main`) keeps a **64 KiB** alternate signal stack so `stack overflow` can always be reported. The crash reporter never allocates and takes no locks; a fault inside the reporter exits immediately with the same code. A function that can reach more than a page below the stack pointer before touching the stack, counting its frame and what its calls take below it (their arguments on the stack, and on Windows the 32 bytes reserved for the callee), lowers the stack a page at a time and touches each page, so an overflow hits the thread's guard page rather than corrupting another thread's stack, as Windows also needs to grow a stack. Installing custom handlers for `SIGSEGV`, `SIGBUS`, `SIGILL`, or `SIGFPE` overrides Flag's crash handler.

#### Indexing, slicing, and bounds-check elision

- **Negative compile-time indexes**: on built-in arrays, lists, listviews, and text, any compile-time negative index (literal, constant, or compile-time expression like `0xffff_ffff as i32`) counts from the end (`l[-k]` is `l[l.len - k]`, panicking if before index `0`).
- **Runtime negative indexes**: always out of bounds (`flag: index -1 out of bounds for length 2`). Because `%` keeps the left sign, wrap with `l[(i + n - 1) % n]` rather than `l[(i - 1) % n]`. Bounds checks use a single unsigned comparison for any integer index type.
- **Custom types and library positions**: user `operator []` / `operator [:]`, `insert`, `remove`, `swap`, and `ring`'s `operator []` receive negative indexes as written (library functions panic on negative positions).
- **Slicing**: `s[a:b]` requires `a <= b <= len` (after resolving compile-time negative bounds), reporting `a` and `b` as written. Open bounds default to `0` (`s[:b]`) and `len` (`s[a:]`, `s[:]`); on user types without a `len` field, `x[a:]` fails with `open slice x[a:] requires a 'len' field on Grid; specify the end bound explicitly`. Compiler-inserted bounds (`len` or end-relative offsets) adapt to the other bound (promoting both to `i64` if either is signed, so `s[i:]`, `s[u:-1]` for `u: u8`, and `s[-3:]` all work). Slicing an array produces a `&listview` of its elements (even for `u8`).
- **Bounds-check elision**: checks on `a[i]` for an `array` are omitted when `i` is a valid compile-time index (including negative from end), or when `i` is the unmodified loop variable of a literal range fitting inside `a`.

## Constants and contexts

Top-level file scope allows only declarations (`func`, `type`, constants, `context`, `extern`, and top-level `where` blocks). There are no global mutable variables, and no code executes before `main`.

### Constants

`N = 3` at the top level declares an untyped constant. Arithmetic between untyped literals folds at compile time and remains untyped, adapting to each use site:

```flag
N = 3
BIG = N * 1000000000 # still an untyped literal

func main():
    small: u8 = N
    print(small, " ", BIG) # 3 3000000000
```

- **Macro-like re-evaluation**: any non-literal expression is re-evaluated at each use site (`NOW = now()` calls `now()` every time). Untyped expressions without a fixed type (`PAIR = {3, 4}`, `SHOW = show`) adopt each use site's target type (`PAIR` becomes a tuple when standalone). Names inside a constant resolve in the constant's file-root scope, unaffected by local variables of the same name at the use site. An error triggered at a specific use adds a note: `constant PAIR is referenced here`.
- **Static read-only storage**: a constant evaluating to a compile-time array or struct (`TABLE = [3, 1, 4, 1, 5]`, `ORIGIN = Point{0, 0}`, the same values permitted in [Constants with a type](#constants-with-a-type)) is placed once in static data and read from there rather than rebuilt at each use. Distinct target types or generic values (`PAIR` read as `Point` vs `Size`, or a constant using `@sizeof($T)` in two instances) each get their own static copy. Taking `ptr(TABLE)`, passing to `*T`, or slicing (`TABLE[1:3]`) operates on a copy; mutating an element fails with `cannot assign to an element of a temporary value; assigning would only mutate a copy`. Static data resides in `.data.rel.ro` on Linux, `.rdata` on Windows, and `__DATA,__const` on macOS (because Mach-O `__TEXT,__const` rejects loader relocations; pure text literal bytes have none and stay in `__TEXT,__const`). Whether a write to `__DATA,__const` stops the program depends on the linker.
- **Array lengths and cycles**: any compile-time integer constant works as an array length: literals, compile-time `a if c else b` (`NAME_AT = 19 if @linux() else 8`), array `.len` (`COUNT = NAMES.len`), and typed integer constants. Cyclic definitions fail: `cyclic constant definition: A depends on itself`.
- **Target typing through `ptr`**: array literal constants adopt element types from `array[T, N]`, `*array[T, N]`, or `&listview[T]` targets, so `words: &listview[u32] = ptr(SHADER)` stores `SHADER = [0x0723_0203, 3]` as `u32` words (out-of-range literals report at the constant with a note at the use). Function call arguments do not propagate element types (`f(ptr(SHADER))` passes `*array[i64, 2]`; bind to a typed variable first; see [Rough edges](#rough-edges)).

### Files in the program

`@embed("path")` embeds a file at compile time as a read-only `&strview`.

- `path` is a text literal (escapes allowed), resolved with `/` relative to the directory of the source file containing `@embed`.
- Bytes are included directly via assembler `.incbin`, keeping compile time low (a 1.5 MB file builds in ~0.06 s vs ~0.66 s and 5x memory as a text literal).
- A file embedded twice by the same path is in the program once. Unreadable files fail at compile time:

```text
error: @embed failed (cannot read the size of game/assets/logo.png: No such file or directory); paths are resolved at compile time relative to this file's directory
```

- Embedded data is 8-byte aligned and followed by a `0` byte, so reinterpreting it as aligned numeric views works (`words: &listview[u32]; words = ptr(SHADER.*) as *bytes; words.len = SHADER.len / 4`). Defining `LOGO = @embed("logo.png")` shares one copy across all uses. Writing into embedded bytes crashes like writing into a text literal, or changes them on WebAssembly.

### Constants with a type

`name: T = value` declares a typed top-level constant checked with declaration conversion rules (`LIMIT: u8 = 300` fails with `300 does not fit in u8 (maximum is 255)`, and `HALF: i32 = 1.5` is rejected):

```flag
limit: i32 = 10
names: array[&strview, 3] = ["one", "two", "three"]
origin: Point = {0, 0}
handler: func(i32) -> i32 = twice
```

- **Allowed compile-time values**: numbers, `bool`, characters, text literals (`&strview` or `cstr`), other constants, `null`, enum variants, function names, `ptr` of a `shared context` or of a field or constant-index element in it, empty owners `heap([])`/`heap("")`/`heap({})` (stored with the heap mark), struct/array literals of the above, `{}`, and compile-time `a if c else b`.
- **Disallowed values**: map literals (`fixmap` runs `put` at runtime to deduplicate equal keys, so write untyped `AGES = {"ana": 30}`), text literals into `&str` (use `&strview` or a context field filled in `main`), and any runtime expression. These are checked even if the constant is unused:

```text
error: typed top-level constants must be compile-time constants (literals and constants), not runtime expressions; omit the type annotation to evaluate it at each use
```

- **Fixed type and compile-time folding**: a typed numeric constant has a concrete type (`limit` is always `i32`; `ratio: f64 = 0.1` does not convert to `f32`). It is still known at compile time for array lengths (`array[u8, limit]`), `where`, `a if c else b`, and constant folding (integers wrap to their type's width; floats round like the hardware, including conversions such as `i64(scale * 4.0)`). Typed constant arrays and structs live in static read-only data.
- **Immutability and syntax errors**:
  - Assigning to a constant: `cannot assign to constant limit; for mutable state, use a 'context' or 'shared context'` (`ptr(limit)` points to a copy).
  - Missing value: `top-level constant count needs a value; for mutable state, use a 'context' or 'shared context'`.
  - Using `:=` at top level: `':=' declares a local variable, not a top-level constant; write 'count = 0', or use a 'context' or 'shared context' for mutable state`.
  - Colliding with a function in the same module: `limit is declared as both a constant and a function`.
  - Using a constant as a type: `limit is a constant, not a type`.

### Contexts

A `context` is structured global state accessible from any function that sees its module. It is also a type: you can print it, take `ptr(app.depth)`, and define nested struct types (`app.inner`). Local variables and parameters cannot shadow a visible context (`cannot declare variable app because it shadows an existing context`).

Fields start at zero or at their default values (see [Structs](#structs)). Because no user code runs before thread start, any fat-pointer field, or a field whose type holds one, requires a default (`context field app.bag has no default value, and its type cannot be zero-initialized`), and so does an owner held by a field of a type parameter, like `Lock[&list[i64]]`:
- `context field app.names has no default value, and &list[&strview] cannot be zero-initialized; add a default: names: &list[&strview] = heap([])`
- `context field app.runs.value starts without allocated memory; initialize it in the context field default: runs: Lock[&list[i64]] = {value = heap([])}`

There are three kinds of context (the modifier goes before `context` and after `priv`):

```flag
context app:             # per-thread, starts fresh in each new thread
    depth: i32

inherited context trace: # per-thread, copied from parent thread on 'parallel'
    request: i64
    level: i32 = 1

shared context stats:    # single instance shared across all threads
    count: Lock[i64]
    limit: i32 = 10
```

| Kind | Scope | Initialization | Notes |
|---|---|---|---|
| `context` | Per thread | Fresh defaults/union tags at thread start (and before `main`) | Zero binary size except Windows TLS; C pre-`main` constructors see zeros; `ptr(...)` valid while thread runs. Used by `core` for scratch memory. |
| `inherited context` | Per thread | Bitwise copy from parent thread when `parallel` runs (after evaluating call arguments) | Shallow copy (an owned `&list` becomes shared between threads). `main` and C-spawned threads calling `export func` start fresh. Copied on every `parallel`, so keep small. |
| `shared context` | Whole process | Static data written at compile time (nothing runs before `main`) | Address valid for whole program; visible as a global in debuggers. Unsynchronized: guard shared mutable state with `Lock[T]` or `atomic[T]` (e.g., `math`'s RNG). Writes in `main` before its first `parallel` are visible to spawned threads. |

A per-thread context is thread-local data of the system: TLS on Linux (initial-exec in a `-shared` library; see [Runtime and ABI details](#runtime-and-abi-details)), thread-local variables on macOS, where one that starts at zero takes no room in the program file, and the `.tls$` section on Windows, where its address is the block of the thread at `_tls_index` in the table at `gs:88`. On macOS and Windows, where that address costs a call or a lookup, each function reads it once, after its parameters.

To change a context temporarily for a call subtree, save and restore with `defer`:

```flag
func visit(levels: i32):
    old := app.depth
    app.depth = old + 1
    defer app.depth = old
    if levels > 0:
        visit(levels - 1)
```

Using `shared` or `inherited` incorrectly fails with `'shared' can only precede a 'context' declaration: shared context name:`, `'inherited' can only precede a 'context' declaration: inherited context name:`, or `a context cannot be both 'shared' and 'inherited'; it is either shared across all threads or copied per thread`. Outside declarations, both are ordinary identifiers (see [Keywords](#keywords)).

## Threads

`parallel f(x)` launches `f(x)` on a new OS thread and returns a `Thread[T]` (where `T` is `f`'s return type, or `Thread[null]` for void / `-> never`). `wait(t)` blocks until the thread finishes and returns `T`; `done(t)` polls completion without blocking.

```flag
func sum(from: i64, to: i64) -> i64:
    total: i64 = 0
    for i in from..to:
        total += i
    return total

func main():
    a := parallel sum(0, 500000)
    b := parallel sum(500000, 1000000)
    print(wait(a) + wait(b))
```

#### Rules and lifecycle

- **Call target**: `parallel` must directly precede a call (`'parallel' must be followed by a function call: parallel f(x)`). Overload resolution, generics, defaults, `@here()`, and `*T` auto-addressing work normally; arguments are evaluated in the caller and copied to the new thread. Flag function values work (`parallel steps[i](x)`), while builtins, conversions, and C functions are rejected (`'parallel' requires a function call, not builtin print; wrap it in a function`).
- **Precedence and `wait`**: `parallel` is a unary operator with the same precedence as `try`. `wait(t: Thread[$T]) -> $T` is a `core` function written in Flag, but the one for `Thread` belongs to the language and cannot be redeclared (`'wait' on Thread is built into the language and cannot be redeclared`; custom `wait` overloads are allowed for other types, like `os`'s `Process`; see [Other programs](Library.md#other-programs)). Calling `wait(t)` as a statement discards non-error results and still requires handling errors (see [Errors](#errors)). `x := wait(t)` on `Thread[null]` evaluates to `null`, allowing generic `return wait(t)`.
- **Unread handles and detached threads**:
  - A local `Thread` variable that is never read fails at compile time: `thread handle t is never awaited or used; call wait(), or detach it with a standalone 'parallel f(x)'`.
  - A standalone `parallel f(x)` statement detaches the thread. Detached functions cannot return errors: `detached 'parallel' call would silently drop errors from i32|Bad; bind the thread handle and await it: t := parallel f(x)`.
- **Representation and double-wait**: `Thread[T]` is an 8-byte pointer to a heap block holding arguments and return value. `wait(t)` frees the block (wait once; unawaited handles leak, detected by `-memory`). Detached threads free their own block on exit. Calling `wait` or `done` on an uninitialized `Thread` panics: `flag: 'wait' called on an uninitialized (never started) Thread`, `flag: 'done' called on an uninitialized (never started) Thread`.
- **Program exit and crashes**: when `main` returns (in any program that uses `parallel`), before `main`'s `defer`s execute, buffered files are flushed and all still-running detached threads are awaited (so detached threads can safely access `main`'s variables), followed by a final flush. Shared libraries have no `main` and do not await detached threads (see [Libraries](#libraries)). Calling `exit()`, panicking, or crashing in any thread terminates the entire process (the first failing thread prints its report while others wait so output never interleaves).
- **Stacks**: every `parallel` thread gets an **8 MiB** stack on all platforms. The `main` thread uses the OS default (8 MiB on Linux/macOS, 1 MiB on Windows).
- **Contexts and scratch memory**: each thread has its own contexts (see [Contexts](#contexts)) and scratch arena. `wait(t)` adopts the finished thread's scratch blocks into the caller's scratch arena (propagating out of helper functions that call `wait`), keeping temporary return values alive until the caller's `with scratch():` exits. Detached threads and threads whose return type contains no pointers free their scratch on exit. Arguments and `inherited context` values are shallow-copied, so temporaries passed into a thread still belong to the parent's scratch; pass owned `&str` instead.
- **`Lock[T]`**: wraps a guarded value in its `value` field, acquired via `with lock(x):` (`Lock[null]` guards code only). Omitting the type parameter fails: `Lock requires one type argument for the guarded value: Lock[i64], or Lock[null] when guarding code only`. Unsynchronized access to `x.value` is not prevented at runtime, but `-memory` warns about concurrent unlocked access (see [Races](#races)).

#### Memory visibility (happens-before)

Writes in one thread are guaranteed visible to another thread **only** across these synchronization edges:

1. Parent writes before `parallel f(x)` (including `inherited context` copies) -> child thread.
2. Child thread writes -> thread that calls `wait(t)` (or `main`'s `defer`s after awaiting detached threads).
3. Writes before releasing a `Lock` -> next thread acquiring that `Lock`.
4. Writes before `Channel` `send` or `close` -> thread receiving that value or observing close.
5. Writes before atomic `set`, `add`, `swap`, or `compare_swap` -> thread observing that value via `get` (see [Atomics](#atomics)).

`done(t)` does **not** establish visibility; read the thread's writes only after `wait(t)`. Any other concurrent unsynchronized access where at least one thread writes is a data race: aligned reads up to 8 bytes see one of the written values, while larger values (`&strview`, `&str`, unions) can tear and produce corrupt memory. The compiler may also hoist memory reads out of loops, so never poll a plain variable; always use `Lock`, `Channel`, or `atomic`. `-memory` detects races at compile time (see [Races](#races)).

`wait`, `done`, `Lock[T]`, and `Channel[T, N]` live in `core`, and `cores()` lives in `os` (see [Threads](Library.md#threads)).

#### Implementation details

- Every `parallel` creates a fresh OS thread that exits when the function returns (no thread pool): `pthread` / `pthread_mutex_t` / `pthread_cond_t` on Linux and macOS; `CreateThread` / `SRWLOCK` / `CONDITION_VARIABLE` on Windows. Linux links `-lpthread`.
- `Lock` identifies its holder by OS thread ID (`pthread_self` / `GetCurrentThreadId`), so host programs and shared libraries agree on recursive re-entry. Both `Lock` and `Channel` work zero-initialized (on macOS, where zeroed mutexes are invalid, the library writes the initializer signature lazily on first use).
- The compiler generates a start trampoline per thread signature (excluded from call chains) that initializes thread-local contexts, installs the 64 KiB crash stack, unpacks arguments and inherited contexts, invokes the function, stores the result, and notifies `core` on completion. Per-thread runtime memory (scratch arena and `print` line buffer) is freed automatically on thread exit via `pthread` destructors (Linux), `_tlv_atexit` (macOS), or FLS callbacks (Windows), including for C-created threads.
- `print` formats an entire line before acquiring the file's write lock, so lines from concurrent threads never interleave and a custom `operator +=` that calls `print` never deadlocks. Files keep separate read and write locks so blocked readers do not stall writers; best-effort flushes (on wait, `flush()`, exit, panic, or crash) skip files currently locked for writing.

### Atomics

`atomic[T]` holds an `i32`, `u32`, `i64`, `u64`, or pointer for lock-free concurrent access. Any other `T` fails: `atomic only supports i32, u32, i64, u64, and pointers, not f64`. It has the size and alignment of `T`, starts at `0`, and always resides in memory (variables, fields, elements, contexts), never as a register value.

Built-in atomic operations (no `@` prefix):

- `get(a) -> T`: atomic load.
- `set(a, v)`: atomic store.
- `add(a, n) -> T`: wrapping atomic fetch-add, returning the previous value (`add(a, -1)` decrements). Rejected on pointer atomics: `atomic 'add' only works on integer atomics, not pointer atomic atomic[*Node]; use set, swap, or compare_swap`.
- `swap(a, v) -> T`: atomic exchange, returning the previous value.
- `compare_swap(a, old, new) -> bool`: atomically writes `new` iff the current value equals `old`, returning whether the swap occurred.

```flag
shared context stats:
    served: atomic[i64]

func serve(requests: i64):
    for i in 0..requests:
        add(stats.served, 1)

func main():
    a := parallel serve(1000)
    b := parallel serve(1000)
    wait(a)
    wait(b)
    print(get(stats.served)) # 2000, whereas plain += loses updates
```

#### Ordering and usage rules

- **Sequential consistency (`seq_cst`)**: all atomic operations participate in a single total order (no weaker orderings). An atomic flag can therefore publish non-atomic data written before `set(ready, 1)` to a thread observing `get(ready) == 1`, and wait loops always re-read memory on each iteration. Note: `-memory` treats atomic operations themselves as race-free, but does not track happens-before for separate non-atomic data published through an atomic (see [What asks for @trust](#what-asks-for-trust)).
- **No direct value reads or writes**:
  - Reading directly (`x := hits`, `print(hits)`, `hits + 1`): `cannot read atomic hits (atomic[i64]) directly as a value; read it with: get(hits)`.
  - Assigning or initializing directly (`hits = 5`, `hits += 1`, `hits: atomic[i64] = 0`): `cannot assign directly to atomic[i64]; atomics start at 0 and must be updated with set, add, swap, or compare_swap`.
- **Passing to functions**: pass by pointer (`func bump(hits: *atomic[i64])`, accessed as `add(hits.*, 1)`; `bump(hits)` auto-addresses like any `*T`, and generics accept `*atomic[$T]`). Passing by value fails: `parameter hits cannot take atomic[i64] by value; atomics cannot be copied, so take '*atomic[i64]'`. So does declaring a function `-> atomic[i64]`, even one that never returns: `cannot return atomic[i64] by value; atomics cannot be copied, so return '*atomic[i64]'`, and a function type that takes or returns one: `a function cannot take atomic[i64] by value; atomics cannot be copied, so take '*atomic[i64]'`.
- **Inside structs**: a struct containing an atomic remains a value type. Copying it reads the 4- or 8-byte atomic whole without a race (`-memory` does not warn), and `print` reads the field via `get`. `core`'s `Lock` stores its owning thread ID in an `atomic[u64]` read this way.
- **Pointer atomics in `-memory`**: pointers stored in `atomic[*T]` (and returned by `swap`/`compare_swap`) are tracked like struct fields.

#### Hardware instructions

Every operation compiles inline without library calls, and prevents the compiler from caching escaped memory in registers across the atomic:

| Operation | `x86_64` | `arm64` (macOS / ARMv8.1) | `arm64` (Linux / ARMv8.0) | `riscv64` |
|---|---|---|---|---|
| `get` | `mov` | `ldar` | `ldar` | `fence`, `lw`/`ld`, `fence` |
| `set` | `xchg` | `stlr` | `stlr` | `fence`, `sw`/`sd` |
| `add` | `lock xadd` | `ldaddal` | `ldaxr`/`stlxr` loop | `amoadd.aqrl` |
| `swap` | `xchg` | `swpal` | `ldaxr`/`stlxr` loop | `amoswap.aqrl` |
| `compare_swap` | `lock cmpxchg` | `casal` | `ldaxr`/`stlxr` loop | `lr.aqrl`/`sc.rl` loop |

All Apple Silicon Macs support ARMv8.1 atomics, while Linux `arm64` targets include ARMv8.0 CPUs (such as Raspberry Pi 3/4 Cortex-A53/A72), which use LL/SC loops.

## Modules

- **Header placement**: `module name` goes on line 1; `use` statements precede the first declaration. Files inside a module already see their own module without `use`.
- **Imports**: `use a` exposes both unqualified `name` and qualified `a.name`; `use a as m` exposes only `m.name`. Accessing `a.name` without `use` is an error. Nested types qualify as `a.Car.motor`.
- **Quoted package imports**: `use "vendor/vec"` loads the directory (see [Files, folders and the library](#files-folders-and-the-library)) and imports it like `use vec` (or `use "vendor/vec" as v` for `v.name`). The final directory segment must be a valid module name and match a `module` declaration inside it:
  - `'flag-vec' is not a valid module name; the last directory in a quoted 'use' names the module, so rename the directory`
  - `directory vendor/vec has no file declaring 'module vec'; the directory name must match the module name`
- **Multi-file modules and uniqueness**: a module consists of all files declaring `module name`, across any directories (other files in a quoted `use` folder may belong to other modules or none). Module names are flat (one namespace for the program, one for the standard library): two quoted `use` directories declaring the same module conflict (`module name conflict: both vendor/a/util and vendor/b/util provide module util; module names must be unique`), whereas files and folders given on the command line that share `module name` merge into one module.
- **Privacy (`priv`)**: hides declarations outside the module. In a file without a `module` line, `priv` restricts visibility to that file (`helper is private to module a.flg and only visible within a.flg`) and shadows global names of the same name within that file.
- **Private-function priority**: when a module (or module-less file) defines a `priv` function of name `f`, calls to `f` inside that module resolve exclusively among its own `f` overloads first (winning even over a closer external overload), preventing user code from hijacking library helpers.
- **Ambiguity and locality**: using an unqualified name (a type, a constant or a variable) declared in two imported modules fails (`Thing is declared in both a and b; qualify it with the module name: b.Thing`). Functions overload across modules and only fail when candidates match equally (`ambiguous call to size with (i32): multiple overloads match equally; qualify with a module name: a.size or b.size`). Equal-score ties prefer own module > global > imported module, while a strictly better-matching overload wins from any module (allowing user `operator +=` or `==` to be called from library generics).
- **Shadowing rules**:
  - Variables, parameters, and constants cannot shadow an imported module name or alias: `bits is already a module name in this file and cannot be shadowed by a variable or constant`.
  - Locals, parameters, `for` variables, and `for` counters cannot shadow any visible local, constant, or context (including `core` and unqualified `use`d modules): `cannot declare variable stdout because it shadows an existing constant` (with a note at the declaration). Functions and types do not block local variable names (`twice := 4` still allows calling `twice(3)`). Standard library files are isolated from user contexts.
  - `main` must be global; a `main` inside a module is not the program entry point.
  - Built-in names `ptr`, `print`, and `eprint` cannot be declared (`print is built into the language and cannot be redeclared`), and `func hash` is reserved for `operator hash`.
- **Shadowing `core` and standard library modules**:
  - User global types and constants shadow `core`'s (`File`, `Integer`, `FsError`, `stdout`, etc.), and so does a user function with the same parameters as a `core` one (one with other parameters is just another overload), in user code while `core` and compiler checks continue using `core`'s own definitions (referred to as `core.File` in diagnostics).
  - A user module with the same name as a standard library module (e.g., `module math`) shadows it in user files, while standard library files (such as `raylib.flg`) continue using the standard library module (called `lib$math` in diagnostics). User files cannot reach the shadowed library module (`sqrt is in standard library module math, which is shadowed by this program's own module math`).
- **Missing `use` hints**: referencing a name from an unimported module (including unloaded standard library modules) names the module: `T belongs to module a; add: use a`, `one belongs to module a; access it through its alias: m.one`, `sqrt belongs to module math; add: use math`. `priv` names are not revealed.

The standard library modules are `fs`, `os`, `json`, `net`, `math`, `utf8`, `bits`, `ring`, and `raylib` (see [Library](Library.md)).

## Memory

Flag has no garbage collector; heap allocations and arenas are freed explicitly.

### Who owns what

- **Owners**: `&str`, `&list`, and `&map` own their buffers and are released with `free`.
- **Non-owning views and literals**: `&strview`, `&listview`, text literals (read-only static data), `[1, 2]` (`array`), `[]` (`emptyarray`), `{k: v}` (`fixmap`), and `{}` (`emptyfixmap`) perform no allocation.
- **`heap(...)` for owners**: `core` provides `heap` overloads creating heap-owned copies: `&str` from text, `&list` from array/list/listview, `&map` from `fixmap`/`&map`. Literals retain their own element types rather than adopting the target type (`heap([1, 2])` is `&list[i64]`; write `heap([u8(1), 2])` for `&list[u8]`).
- **Empty heap owners**: `heap([])`, `heap("")`, and `heap({})` allocate nothing until grown; they simply set the `arena` field to the heap marker (`1`). `heap([])` (`&list[never]`) and `heap({})` (`&map[never, never]`) convert to any list or map type and adopt the target type directly. Because the compiler emits them without a runtime call, they are valid in field defaults and typed constants.
- **Uninitialized owners**: an owner variable declared without an initializer has no allocator and must receive `heap(...)` or `arena(a, ...)` before growing (see [Fat pointers](#fat-pointers)).
- **`heap(value)` for pointers**: copies any other value to the heap and returns `*T`, freed with `free(p)`. Calling `free(p)` on a pointer to `&str`, `&list`, `&map`, or `Arena` invokes that type's own `free` (releasing its contents); to free the outer heap block itself, pass `free(p as *bytes)`.
- **Arenas**: `arena(a, ...)` allocates inside arena `a`, released all at once by `free(a)`.
- **Nested ownership**: freeing a `&list[&str]` does not free its element strings; free elements first. A `&map[&str, V]` *does* own its keys: inserting copies `&str` or `&strview` keys (`m["host"] = 1`, `counts[word] += 1`), and `remove`, `clear`, and `free` free the keys automatically. Map values are not freed automatically.
- **Immediate stack errors**: single-expression stack mistakes are compile errors without `-memory`:
  - `return ptr(x)` (variable, parameter, or temporary): `cannot return a pointer to stack memory; it becomes invalid when this function returns` (see [Returns](#returns)).
  - `free(ptr(x))` or `free(ptr(x) as *bytes)` when freeing the pointer itself (not freeing a list/text/map/arena's contents): `cannot free stack memory x; free() only releases heap allocations`.
  Multi-step mistakes are caught by `-memory`.

### Temporary memory

Functions that return temporary views (`+` on text, `cstr`, `split`, `replace`, `read_line`, etc., marked *temporary* in [Library](Library.md)) allocate from a per-thread scratch arena made of reused **64 KiB** blocks.

`with scratch():` marks the current scratch position and rewinds to it when the block exits, releasing all temporaries allocated inside. Copy any temporary you want to keep into an owner before the block ends:

```flag
kept: &str = heap("")
defer free(kept)
with scratch():
    line := "id " + 42
    kept = line # copies into heap memory owned by kept
print(kept)
```

Without `with scratch():`, temporaries live until their thread exits (fine for short programs; wrap long-running loop bodies in `with scratch():`). The scratch arena lives in a per-thread `core` context (freed automatically on thread exit, including C threads), and its allocation verb is `tmp` (e.g., `tmp(text)`).

### Arenas

An `Arena` (a `core` struct whose `blocks` list lives on the heap) sub-allocates from **64 KiB** heap blocks (or larger blocks for oversized requests) and frees everything at once. Allocation verbs are always explicit (`heap`, `tmp`, `arena`); `heap` always uses the heap.

```flag
level: Arena
defer free(level)
boss := arena(level, Enemy{hp = 100})    # *Enemy in level
enemies: &list[Enemy] = arena(level, []) # empty list backed by level
append(enemies, Enemy{hp = 10})          # grows in level
name: &str = arena(level, "boss")        # &str backed by level
name += " final"                         # grows in level too
```

- **Alignment and overloads**: `arena(a, v)` copies `v` into `a` with **16-byte alignment** (the maximum alignment, needed for `simd`) and returns `*T`. Overloads for text, arrays, listviews, `fixmap`s, and maps return arena-backed `&str`, `&list`, or `&map` (literals keep their own types, e.g., `bytes: &list[u8] = arena(a, [u8(1), 2])`). Empty `arena(a, [])` (`&list[never]`) and `arena(a, {})` (`&map[never, never]`) require a target type (`cannot infer type of x from an empty literal; specify its type explicitly: x: &list[i32]`). `a` may be `Arena` or `*Arena`; other types fail (`no overload of arena matches (i64, array[i64, 2])`).
- **`clear(a)` vs `free(a)`**: `clear(a)` invalidates all allocations while retaining blocks for reuse (ideal for per-frame game memory); `free(a)` returns all blocks to the heap. Both leave `a` empty and reusable.
- **Growth and `free` of arena-backed owners**: arena-backed `&str`, `&list`, and `&map` store `a` in their `arena` field. When growing, `&str` and `&list` expand in place if they occupy the end of the current block, otherwise copy into a new slice; `&map` always allocates a new table, and a `for` over the map keeps reading the old one. Old slices are reclaimed when the arena is cleared or freed, and `reserve(m, n)` before filling a map avoids its old tables. Calling `free` on an arena-backed owner empties it without freeing memory while preserving its `arena` field so it can grow again in the same arena. Assigning a raw pointer (`text = ptr(buf) as *bytes`) updates only the address.
- **Cautions**:
  - A `*T` from `arena(a, v)` does not track its arena; calling `free` on it is an error caught by `-memory`.
  - `&ring` from module `ring` is always heap-allocated.
  - Always pass arenas as `*Arena`: copying an active `Arena` aliases its block list (`-memory` warns about copies and by-value `Arena` parameters).
  - Arenas are not thread-safe; give each thread its own or guard every use (including growing arena-backed owners) with a `Lock`.

## The memory checker

`-memory` analyzes the program's IR across all paths to verify memory safety at compile time.

**Guarantee**: outside `@trust`, a program that compiles without `-memory` warnings does not leak memory, double-free, free non-heap memory, use freed/expired memory, or have data races, apart from the known blind spots in [What it misses](#what-it-misses). This covers heap blocks, arenas (after `free`/`clear`), scratch temporaries (after `with scratch():`), stack variables/temporaries (after block or function exit), and `parallel` threads. Temporaries and arena allocations never leak individually because their scratch block, thread exit, or arena releases them. `-memory` tracks memory only, not open files, sockets, or shared libraries.

**Trust boundaries** (where safety is your responsibility):
- `@trust` regions (see [@trust and @check](#trust-and-check)).
- `trust` and `as`: `as` reads a union member without checking its tag.
- `*bytes` arithmetic: offset pointers remain tracked to their backing block for use-after-free, but bounds are not checked.
- Mutating a narrowed variable through a pointer taken before `is`: what is then freed or read as the narrowed member may be the other one (see [Unions and narrowing](#unions-and-narrowing)).
- C code: `extern` functions and `extern func` calls are assumed read-only, and C-spawned threads are unseen (synchronize `export func`s called from C threads).
- Uninitialized null `*T` pointers: dereferencing near zero faults in unmapped low memory (`flag: invalid memory access at 0x8`; first 64 KiB on Windows and most Linux systems; first 4 GiB on macOS), while larger field offsets can reach arbitrary memory.

Where the checker cannot prove safety, it warns conservatively; [What asks for @trust](#what-asks-for-trust) lists valid patterns that require `@trust`.

### What it warns about

#### Heap, stack, and owner lifetime

- **Leaks** (never freed, or leaked on some path): `memory leak: the memory allocated here for names is never freed`, `memory leak: the memory that make allocates here may never be freed`.
- **Replaced context memory**: a call that overwrites a block a context surely holds, without freeing it: `memory leak: keep replaces cache.last while it still holds memory; free it first`.
- **Double free**: `free cannot release p; it may already be freed (possible double free)`.
- **Use after free or reallocation**: `use after free: first points to memory that may already be freed` (including after a list grows and moves, or after `with scratch():` exits).
- **Freeing or growing non-heap memory**:
  - Non-heap pointer (such as an indirect stack pointer `p := ptr(x); free(p)`): `free cannot release p; it is not heap-allocated`. (Direct `free(ptr(x))` is already a compile error: `cannot free stack memory x; free() only releases heap allocations`.)
  - Interior heap pointer: `free cannot release p; pointer is inside a heap block, not at its start`.
  - Scratch temporary: `free cannot release t; it is scratch memory released by 'with scratch()'`.
  - Growing a non-heap owner or `realloc` on stack memory: `append cannot grow l; it is not heap-allocated`, `realloc cannot grow p; it is not heap-allocated`.
- **Possibly uninitialized owner**: `append cannot grow names; it may not have allocated memory to grow (initialize with heap([]))` when some path leaves an owner without an allocator (one that no path gave memory is a compile error; see [Fat pointers](#fat-pointers)) (variables, fields, contexts, parameters, or generic `Box[&str]{}`). List elements and map values are tracked collectively, so `append(groups[k], v)` warns only if no key ever received memory; an uninitialized new key alongside initialized keys panics at runtime (see [Runtime errors](#runtime-errors)).
- **Block-scoped dangling pointers** (including across loop iterations, since a loop body is a block):
  - Local variable: `dangling pointer: keep points to pair after its enclosing block ended; allocate on the heap or declare pair outside the block`.
  - Temporary (`ptr([...])`, `*T` auto-address, `..items`): `dangling pointer: keep points to a temporary whose enclosing block ended; allocate on the heap or store it in a variable declared outside the block`.
- **Escaping stack pointers**: returning a stack pointer indirectly, through a variable or a struct (`cannot return a pointer to stack memory; it becomes invalid when this function returns`, the same text as the error of `return ptr(x)`) or storing one into a context or caller memory (`pointer into the stack of fill cannot escape fill`; for `..items`, the warning names the caller).
- **List modification during `for`**: modifying a list after narrowing its loop variable (`u[i] = v` or mutating call) warns `v may no longer hold i32 because its enclosing list was modified after narrowing; re-check it with 'is'`, or `use after free: v points to memory that may already be freed` if the list grew.
- **Evaluation-order stale view**: `show(l, grow(ptr(l)))` warns `use after free: l points to memory that may already be freed` with a note at `grow` (see [Order](#order)).
- **Aliased free/reallocate and read**: `add_view may free or reallocate memory that it also reads through another argument` (or `this call may free or reallocate memory...` for indirect/operator calls). Built-in `&str` `+=`, `append` of a view, and view-to-owner `=` copy overlapping views first, so `t += t` and `l = l[1:]` are clean.

#### Arenas, locks, channels, and threads

- **Arenas**:
  - Use after `free(a)` or `clear(a)` (even inside a helper like `reset(level)`): `use after free: boss points to memory that may already be freed` (with a note at `free`/`clear`).
  - Freeing an arena-allocated pointer: `free cannot release boss; it belongs to arena level (release with free(level))`, or `free cannot release level.boss; it belongs to arena level.arena (release with free(level.arena))`. (Calling `free` on an arena-backed `&str`, `&list`, or `&map` is clean because it merely empties the handle.)
  - Leaked arena: `memory leak: the memory allocated here for level is never freed` (reported at its first allocation).
- **Copying an active `Arena`, `Lock`, or `Channel`** (directly, in a struct, or in a list; zero-initialized copies like `w := World{}` or `append(worlds, World{})` are clean):
  - `copying level, an Arena that may have given memory: the copy gives out the same blocks; pass or store a pointer instead`
  - `copying the Channel jobs in pool, that may be in use: the copy is another channel; pass or store a pointer instead`
  - Passing by value to a user function: `passing level by value to make copies an Arena; change the parameter type to *Arena`
- **Threads**:
  - `parallel f(x)` is checked as calling `f(x)` at the launch point.
  - Unawaited `Thread` leaks; double `wait` warns `wait cannot release t; it may already be freed (possible double free)`; `done(t)` after `wait(t)` is use after free.
  - Stack pointer passed to a returned `Thread` warns `cannot return a pointer to stack memory; it becomes invalid when this function returns`; passed to a detached thread or non-local `Channel` warns `stack pointer to x cannot escape start because x lives on its stack` (clean in `main`, which awaits detached threads).
  - Growing an `inherited context` `&list` in a child thread, or copying a context (`app = given`), aliases ownership and warns `use after free: this uses memory that may already be freed` (note at `parallel`).
  - Detached thread in a `-shared` library: `detached thread in a shared library may outlive library unload and execute unmapped code; store the handle and wait() for it in an 'export func' before unloading`.
- **`Channel` ownership transfer**: `send` transfers ownership of `&str`/`&list`/`&map`, heap pointers at block start, and `Thread` handles to the receiver (using or freeing them afterward warns as use-after-free / double-free). Views, arenas, and interior pointers stay yours.
- **Temporaries outlived by threads**:
  - Passed to `parallel f(x)` and not awaited inside the same `with scratch():`: `thread may still use this temporary after its with scratch() releases it: wait for the thread inside it, or give it a &str` (note at `with`).
  - Passed to a detached thread outside `main`: `thread may still use this temporary after the with scratch() of a caller releases it: give the thread a &str`.
  - Sent via `send`: `receiving thread may still use this temporary after 'with scratch()' releases it; wait() for that thread inside the block or send an owned &str`.
  - Returned temporaries move into the waiter's scratch (propagating through helpers calling `wait`), and threads with pointer-bearing results keep all temporaries until the waiter's scratch ends. Threads with non-pointer results free their scratch on exit, so a temporary written out through a pointer or context warns `use after free: got points to memory that may already be freed` (note at `parallel`).
- **Data races** (see [Races](#races)): `data race: count is accessed while a concurrent thread writes it with no synchronization; guard it with Lock[T] or wait() before accessing` and `data race: count.value is accessed outside 'with lock(count)' while another thread uses that lock; acquire the lock around this access`.

#### Untrackable pointers and checker limits

- **Untrackable pointers** (subsequently reported as leaks):
  - Cast to integer, alone or inside a struct (`W{p} as u64`): `-memory cannot track x after converting it to an integer; keep it in a pointer type`. The distance between two places of one block (`ptr(p.b) as i64 - p as i64`) is a plain number and does not warn.
  - Stored into C-allocated memory (directly or via a helper): `-memory cannot track a pointer after it is stored into C-allocated memory`.
- **Complexity limits**: a function exceeding **1024 tracked blocks** (~1000 user allocations, temporaries, and pointers that parameters and contexts reach; words that hold numbers take none) or failing to converge stops analysis for that function (still reporting findings so far):
  - `-memory stopped checking f because it has more than 1024 blocks of memory to follow; checks for this function are incomplete`
  - `-memory stopped checking f because its memory state did not converge; checks for this function are incomplete`
  Every caller (and transitive caller, even if `f` is `@trust`) also warns: `-memory stopped checking f because it has more than 1024 blocks of memory to follow; this call is not checked` (`@trust` on the call line silences it). Split large functions to stay within the limit.

#### Naming and notes

Messages name your variable and field path (`level.boss`), or `this memory` when anonymous. Allocations take the name of the first variable storing them (`memory leak: the memory allocated here for s`), or the allocating function when unnamed (`the memory that make allocates here`, including direct arguments like `append(names, heap("ana"))` or union variables like `t := read_file(path)`). Container elements take the container's name. User-written calls are named (`drop cannot release path; it is scratch memory released by 'with scratch()'`), while compiler-synthesized calls (`print`, `+`, `+=`, `for`, conversions) say `this call`.

Notes point to the related site: `it may be freed here` (the `free` or reallocating call) or `it is still not freed when this returns` (the `return` or function header):

```flag
func main():
    names: &list[i32] = heap([])
    defer free(names)
    append(names, 1)
    first := ptr(names[0])
    append(names, 2)
    print(first.*)
```

```text
warning: use after free: first points to memory that may already be freed
  --> main.flg:7:5
   |     print(first.*)
   |     ^
note: it may be freed here
  --> main.flg:6:5
   |     append(names, 2)
   |     ^
```

### Elements

Elements of a list, array, or map allocated at one source line are tracked collectively as **none**, **some**, or **all** freed.

A **full walk** gives each iteration a distinct element: `for x in l`, `for x in l[:]` or `l[0:l.len]` of a list or an array, `for i in 0..l.len` indexing `l[i]`, `for i in 0..n` over `array[T, n]`, and `for e in m`. Freeing the element on every path of the body without `break` transitions the elements to **all freed**, provided every pointer-bearing list/array indexed by `i` in the loop is the walked container (indexing `names[i]` under `0..other.len` or `for i, x in other` marks only *some* freed):

```flag
for s in names:
    free(s)
free(names)
```

`for i in 0..names.len: free(names[i])`, `for e in m: free(e.value)`, moving elements to another list that frees them later, and awaiting a list of threads while freeing each returned string are all clean.

- **After all freed**: reading an element warns `use after free: names points to memory that may already be freed`, and freeing again (such as manually freeing `&map[&str, V]` keys before `free(m)`) warns `free cannot release m; it may already be freed (possible double free)`.
- **Unfreed or partially freed**: freeing the container without freeing elements warns `memory leak: the memory allocated here for s is never freed`; freeing only on some paths or before `break`, conditionally appending a block allocated once (`if keep: append(names, s)`), or appending past `a.len` during `for i in 0..a.len` warns `may never be freed`.
- **Views across mutating walks**: a view of an element taken *before* a walk that frees or grows elements (`best := players[1].name[0:3]`, then `p.name += "!"` in `for p in players`) warns `use after free: best points to memory that may already be freed` (note at `for`). A view taken *inside* the walk of its own round's element stays valid after the loop.
- **Aliased elements**: storing one allocation in multiple elements (`a[0] = s; a[1] = s`, `m[1] = s; m[2] = s`, or one shared `stats` in loop-created jobs) breaks per-element distinctiveness: concurrent threads warn `data race: multiple threads started here write stats concurrently; guard it with Lock[T] or give each thread its own copy and merge after wait()`, and freeing in a walk warns `free cannot release j.stats; it may already be freed (possible double free)`.
- **`clear`, fixed index, `pop`/`remove`, and transfer**:
  - `clear(l)` removes existing elements (unfreed ones warn `is never freed`; later elements start fresh).
  - `free(names[0])` at a known index marks elements as potentially freed (subsequent reads warn `use after free: names points to memory that may already be freed`) while leaving the rest leaked.
  - `pop` or `remove` followed by `free` marks all elements freed without triggering UAF on remaining reads.
  - Storing an element into another list, array, map, or context marks all elements stored there.

### Races

Two threads race when they access the same memory concurrently without a happens-before edge from [Threads](#threads) and at least one writes (or frees). The memory can be a variable passed by pointer, a field of a `shared context`, or an element of a list both reach.

The checker tracks every live thread from `parallel` until `wait` (including handles returned to callers, stored in lists walked with `wait` in the same function, or passed to a helper that awaits them; detached threads run until `main` exits) and summarizes reads/writes across callees. Operations inside matching `with lock(x):` blocks (propagated into callees) and atomic operations (against each other or plain reads, though not against `free`, moves, or non-atomic writes) do not race:

```flag
func add(count: *i64):
    for i in 0..1000:
        count.* += 1

func main():
    count: i64
    a := parallel add(ptr(count))
    add(ptr(count))
    wait(a)
    print(count)
```

```text
warning: data race: count is accessed while a concurrent thread writes it with no synchronization; guard it with Lock[T] or wait() before accessing
  --> main.flg:8:5
   |     add(ptr(count))
   |     ^
note: the thread starts here
  --> main.flg:7:19
   |     a := parallel add(ptr(count))
   |                   ^
```

The five race warnings:
1. Reading/accessing while a concurrent thread writes (including after `done(t)` before `wait(t)`): `data race: result is accessed while a concurrent thread writes it with no synchronization; guard it with Lock[T] or wait() before accessing`.
2. Modifying or freeing while a concurrent thread reads: `data race: items is modified here while a concurrent thread reads it with no synchronization; guard it with Lock[T], or modify it before 'parallel' or after wait()`.
3. Multiple threads spawned by one `parallel` in a loop writing shared memory: `data race: multiple threads started here write total concurrently; guard it with Lock[T] or give each thread its own copy and merge after wait()`.
4. Accessing a lock-guarded value outside `with lock(...)` while another thread uses the lock: `data race: count.value is accessed outside 'with lock(count)' while another thread uses that lock; acquire the lock around this access`.
5. Holding the lock locally while another concurrent thread accesses the value unlocked: `data race: another concurrent thread accesses count.value outside 'with lock(count)'; acquire the lock in that thread`.

Clean concurrent patterns: writes before `parallel` and reads after `wait`; per-element threads launched in a walk (`parallel add(ptr(totals[k]), k)` over `0..4` or `parallel double(v)` over `values`, with writes to `totals[k]` clean before `parallel` and warning after); `Channel` `send` ownership handoff; per-thread argument/result blocks; per-thread `context`s; and `core`'s internally synchronized I/O and runtime state.

### Paths and unions

`-memory` explores all control-flow paths (ignoring general boolean conditions, and branching over every function taken as a value of the callee's type for indirect calls), but **does** follow `is` checks (after `if node is *Node:` it knows the other path has no node to free) and a test of what `realloc` returned against null (on that path the old block is still yours) and tracks each union member's fields separately, even when two members store pointers at the same byte offset (`Shape` and `Bad` below), when the paths that made the union meet, or when only one member writes a field (`&str|null`):

```flag
type Shape:
    points: &list[i32] = heap([])

type error Bad:
    reason: &strview

func load(text: &strview) -> Shape|Bad:
    if text.len == 0:
        return Bad{"empty"}
    s: Shape
    append(s.points, 1)
    return s

func main():
    s := load("abc")
    if s is Bad:
        print(s.reason) # no leak: this branch has no Shape
        return
    free(s.points) # frees Shape's list, not Bad's reason
```

### What asks for @trust

Because `-memory` is conservative, these valid programs produce false-positive warnings (silence them with `@trust` on the line):

#### Control flow, pointers, and aliasing

- Freeing in `if c:` and again in `if not c:` (warns both double free and possible leak).
- Freeing one element at a constant index (`free(names[0])`) and reading or freeing another through the list, also when the list holds a union and each element is narrowed first, or when the elements grew through a runtime index (`append(groups[i % 2], x)`, then `free(groups[0])` and `free(groups[1])`).
- Calling a function that grows what one argument points to and reads another with pointers to two elements of one list or array (`merge(ptr(sets[i]), ptr(sets[j]))`): the elements are tracked together, so it warns `merge may free or reallocate memory that it also reads through another argument` even when `i` and `j` differ.
- Linked lists and trees built in loops or recursively (all allocations from one line collapse into two blocks: newest vs rest, so freeing a node via a pointer that may be either is not proven to free all).
- Recursive tree `release` walking children first, when the nodes own memory (`release may free or reallocate memory that it also reads`; a tree of plain `*Tree?` nodes gives no warning).
- Pointers that may refer to one of several struct fields when storing or freeing through them.
- Double-buffered owners swapped in a branch or loop (`cur, next = next, cur`, warning double free and leak), or a conditional arena pointer (`cur := ptr(a) if c else ptr(b)` followed by `clear(cur)` -> `clear cannot grow cur; it is not heap-allocated`).
- Storing `ptr(a)` of a `main` local variable in a context (`stack pointer to a cannot escape main because a lives on its stack`, even though `main` lives for the whole program).

#### Collections, unions, and structs

- A function freeing partial allocations on an error return (`-> &list[&str]|Bad` or `-> *User|Bad`), making the caller think the success path may also be freed.
- A function initializing a list/map element (`items[k] = heap([])`) and then growing it (`append(items.*[k], v)`): caller does not see the element initialization and warns it may lack memory.
- Joining a `pop`/`remove` result with a fresh allocation (`a := remove(l, 0) if l.len > 0 else heap("")`), or conditionally appending a variable and later reassigning it (freeing it counts as freeing the list's elements).
- Freeing an owner element in a walk and regrowing it later, or replacing it in place (`free(names[i]); names[i] = heap("x")`).
- Splitting allocations from one loop line across two lists (`append(good, s)` vs `append(bad, s)`): both lists appear to alias the same block, causing double-free warnings (`free cannot release n; it may already be freed (possible double free)`), view invalidations, and cross-list race warnings.
- Rebuilding a list in a loop and reading `v` after `append(next, v)` (`use after free: v points to memory that may already be freed`).
- Emptying a list of lists with `pop`/`remove` (inner elements warn double free and leak).
- Emptying a list with `l.len = 0` after freeing its elements, then filling it again: only `clear(l)` takes the elements out of the list for `-memory`, so the new ones seem to share the freed blocks (`use after free: p.marks points to memory that may already be freed`). Write `clear(l)`.
- `append` to a zeroed list (from `calloc`) whose capacity was reserved by hand: the checker does not see the capacity and warns `this call cannot grow data; it may not have allocated memory to grow`.
- Returning owners nested two levels deep built in an inner loop (list of rows or map of lists: `memory leak: the memory that heap allocates here may never be freed` with note at `return`).
- Calling `args()` in two places (checker thinks the list may reallocate between calls, invalidating earlier views).
- Assigning to keys while walking a map (`scores[e.key] = e.value * 2`, which could rehash in general).
- Walking `for i in 0..names.len` to free `names[i]` while also indexing another pointer list/array (`labels[i]`): counts as only *some* freed.
- Freeing a list through slices with a literal bound (`drop(names[0:1])` and `drop(names[1:2])`, or `drop(names[0:2])` on a list of two): the checker neither adds up the pieces nor knows the length (`memory leak: the memory that heap allocates here may never be freed`).
- Freeing a list's elements in an inner walk inside the loop that fills it, before the round's `append`, then returning (`memory leak: the memory that heap allocates here is never freed`).
- Growing an owner field with default `heap([])` inside an array of more than **16** structs (`append cannot grow bs; it may not have allocated memory to grow (initialize with heap([]))`), because arrays over 16 initialize defaults in a loop the checker assumes might run zero times.

#### Arenas

- An owner backed by the heap on one path and a local arena on another (`free` of the arena warns non-heap).
- A function that both clears/frees and allocates in an arena received via pointer, field, or context (`memory leak: the memory allocated here for a is never freed` at the call).
- Copying, listing, or heap-allocating a struct larger than **4096 bytes** that contains an `Arena`: copies above 4096 bytes merge words, causing `free(g.level)` to warn `free cannot release g.level; it is not heap-allocated`.
- Returning a struct containing an active `Arena` or `Lock` when the struct exceeds **128 bytes** or the function has a `defer`, or calling `append(levels, load(i))`: treated as copying (`copying the Arena mem in l, that may have given memory: the copy gives out the same blocks; pass or store a pointer instead`).
- An indexed walk reading an arena-allocated value from `levels[i]` and then calling `free(levels[i].mem)` (other elements' arenas appear potentially freed).

#### Threads, locks, channels, and atomics

- Temporaries passed to a thread awaited after `with scratch():` (even if read earlier), temporaries sent via `send` while any thread runs, or stack pointers sent on a non-local `Channel` (even if receivers exit before the function returns).
- Non-atomic data published through an `atomic` flag (`set(ready, 1)` / `get(ready) == 1`) or ordered indirectly by a `Channel` message without being sent through the channel.
- A lock picked at a runtime index in the function that uses it (`with lock(locks[i])`; a function that takes `ptr(locks[i])` and locks its parameter is clean), or memory written under one lock and read under a different lock with external ordering.
- Threads stored in a list and awaited via a helper function or `pop` loop (treated as potentially still running).
- Threads given disjoint slices of one list (`fill(values[0:4], 1)` and `fill(values[4:8], 2)`), or updating distinct runtime indexes received from a `Channel` (`results[j]`): only a walk proves per-thread element disjointness.
- Threads launched into a list by a helper function returning or filling the list, where each thread returns an allocation freed on `wait` (`memory leak: the memory that start allocates here may never be freed`; clean when launched in the awaiting function).
- Threads in a list each receiving a per-round `heap(i)` and returning it (`data race: this memory is accessed while a concurrent thread writes it with no synchronization; guard it with Lock[T] or wait() before accessing`).
- Threads each clearing/freeing `ptr(arenas[k])` from an array/list, or copying an `inherited context` arena outside `main` (`data race: multiple threads started here write arenas concurrently; guard it with Lock[T] or give each thread its own copy and merge after wait()`).
- Sending a block (`*Msg`) that owns nested blocks (`&str` field): `send` transfers only the top-level block, so the nested `&str` warns leaked.
- Growing or walking an owner received from a `Channel` (channel payloads are not tracked, so `got += "hi"` warns `this call cannot grow got; it may not have allocated memory to grow (initialize with heap(""))`, `append(got, 1)` warns `append cannot grow got; it may not have allocated memory to grow (initialize with heap([]))`, and list elements may appear freed).
- A thread with a pointer-bearing result leaving a temporary outside its return value (via pointer or context) when another thread is awaited inside a `with scratch():` first (that `wait` appears to absorb the temporary).

#### C interop, function values, and indirection depth

- Memory passed to a C function or `extern func` that retains or frees it (assumed read-only, so reported as leaked).
- Walking a value by reflection, recursing through `*bytes` at offsets read from a table: two offsets may be the same place, so each call seems to free what the last one freed.
- Calling a function value that came through a parameter, a field or another variable (`g(p)`) when *any* function of that type taken as a value frees its argument. A local assigned only named functions calls just those.
- Modifying or dereferencing deeper than **3 pointer indirections** from a function's parameters (`free(p.a.b.c.d.e)`, `append(p.a.b.c.names, x)`): past 3 hops a block represents everything reachable from it, so frees/reads taint all deeper memory, and mutations/allocations stop tracking at the call (`-memory stopped checking grow because it modifies memory more than three pointer indirections from its parameters; this call is not checked`). `core`'s internal contexts (`print`, files) are exempt.

### What it misses

These real bugs currently produce **no** `-memory` warning:

#### Collections and loops

- Copying one element onto another slot in the same list (`l[0] = l[1]`): two elements then hold one block, and a walk that frees them frees it twice. The checker takes it as the first half of a swap (`t := l[0]`, `l[0] = l[1]`, `l[1] = t`), which is right.
- A walk freeing elements from a loop allocation when some rounds never stored the block (`if keep: append(names, s)`), an element was copied to another list (`append(other, names[0])`), or only some elements reach the block (`free(b.value)` in `for b in boxes`): the walk marks the whole allocation line freed, hiding leaks of unstored blocks.
- Freeing an element at an unknown runtime index outside a walk (e.g., a map value) marks all elements freed: reading one at a known index warns, but another read or free at an unknown index does not.
- Keeping a pointer into a list's buffer or an arena inside a list or map at a runtime index: stays marked live after list growth or arena `clear`/`free`.
- Moving only *some* elements to another list in a walk (`if` or `break`) before freeing the original list leaks the unmoved elements.
- Freeing part of a list through a function, when the slice has only bounds known at run time (`drop(names[:n])`, which may be half of a valid split with `names[n:]`), or when a function calls itself on a slice that skips elements (`release(v[2:])`): it counts as freeing every element.
- Keeping `ptr(x)` of a loop element across iterations in a field or a list instead of a variable.
- Overwriting (`l[i] = v`, `m[k] = v`), `clear`ing, or dropping `pop`/`remove`d owner elements without freeing them when a later walk frees the container's elements.
- Rearranging a list of structs with multiple owners via `sort`, `insert`, view `append`, `heap(l[:])`, or `l = items` merges fields across elements (freeing one owner hides leaks of the others and warns double-free if you free them too).
- Freeing a list after freeing only *some* elements via `pop`/`remove` (hides leaks of remaining elements).
- Reading a view of a list taken before `clear(l)` after `clear(l)` if an element was freed beforehand.
- Keeping a view of a `&map[&str, V]` key inside a list after the map frees its keys.
- Writing a number over a pointer through memory cast to another type (`(ptr(cache.last) as *u64).* = 0`): `-memory` follows only the words that the written type says hold pointers, so it still sees the old pointer there and reports no leak of its block.

#### Trees, contexts, channels, stack, and arenas

- Leaking deeper levels of a recursively built tree when only the first level is freed (2-block summary).
- Overwriting a heap block stored in a context by a call in a loop or on only some paths, or leaking elements a helper appends to a context list (values stored in contexts are not considered leaked). Two calls in a row do warn.
- Anything sent through a `Channel`: `receive` and `for v in c` are untracked, missing leaks, double frees, UAF, and races through sent pointers.
- Freeing an earlier iteration's block through a carried variable (`free(prev); prev = j`) while that iteration's thread may still use it: the `free` gets no warning, and the last iteration's block may be reported as never freed.
- A pointer conditionally pointing to one of two local variables (`if`/`else` or running best) where one variable's block ended (only single-variable block pointers are tracked precisely).
- Copying an active `Arena` received via pointer (`copy := a.*` for `a: *Arena`) or via `inherited context` inside a function taking `*Arena`.

### @trust and @check

`@trust` suppresses `-memory` warnings for code you have audited:

- **End of line**: covers that line and any block indented beneath it.
- **Standalone line**: covers all following lines at that indentation until indentation dedents (at the top of a file, covers the entire file).

`@check` re-enables warnings inside a trusted region; a `@check` function continues reporting warnings even when called from trusted code. `-memory` skips trusted functions that no untrusted code depends on, speeding up checks (in `flagc`'s own source the frontend is `@trust`, while the backend and the WebAssembly writer are checked).

## Debugging

`-debug` emits DWARF debug information for `gdb` and `lldb`, supporting source stepping, stack traces with line numbers, and typed variable inspection:

```sh
flagc main.flg -o main -debug
gdb ./main
```

- **Units, functions, and breakpoints**: each source file is a compile unit, and each function has its name, its declaration line and its code. Generic instantiations share their base name (`break sort` stops in all instances). Breaking on a function stops at the first line of its body with parameters initialized. Your `main` is C's `main`, so `break main` stops once at the first line of its body, after runtime startup (args and crash handler). `break panic` stops before a runtime error prints, with full `bt`; hardware crashes stop automatically at the faulting instruction.
- **Variables and contexts**: locals and parameters live in stack slots scoped to their lexical blocks (separate loops' `i` variables are distinct). `shared context` is a global variable; per-thread `context` and `inherited context` are thread-local variables (showing the active thread's values). Constants are not variables and are invisible to debuggers. On Linux `arm64` and `riscv64`, thread-local contexts have no DWARF location because those targets lack the required TLS relocation (gcc and clang omit them too).
- **Type representation** (compile units declare C as the language so values format cleanly as `{x = 3, y = 4}`):
  - Numbers, `bool`, and pointers map to C types; `*u8` maps to C strings (so a text's `data` field displays its characters and any trailing bytes).
  - User structs, enums, and type aliases emit unqualified typedefs so `ptype Point`, `ptype Color`, and `ptype Shape` work directly.
  - Fat pointers appear as structs (`data`, `len`, `cap`, `arena`); tuples appear as structs with numeric fields (`{0 = 5, 1 = true}`); function values as function pointers; `Thread[T]` as a struct holding `block`.
  - Enums appear as C enumerations whose values are their union tags (see [Unions](#unions)).
  - Tagged unions appear as a struct with `tag` (an enum of members) and a DWARF variant part (as in Rust): `{tag = (Point), Point = {x = 1, y = 2}}`, or `{tag = null}` for void members. Payload member enumerators are parenthesized (`(Point)`) to avoid C namespace collisions with the struct type `Point`. In `gdb`, printing an array of unions shows all variants per element, while `print shapes[1]` shows only the active variant. `lldb` only renders variant parts for Rust compile units and may display only the tag.
  - Untagged unions appear as C unions.
- **Text and lists in `gdb`**: `lib/flag-gdb.py`, also at the top of the release package, shows a `&str` or `&strview` as text and a `&list` or `&listview` with its elements (`names = {"ana", "luz"}`). Load it with `gdb -x flag-gdb.py ./main`, or with a `source` line in `~/.gdbinit`. `print/r` shows the raw fields, `print l.data[1]` one element, and a `&map` stays raw.
- **Calling functions from the debugger**: functions taking and returning only numbers, `bool`, pointers, and function values match the C ABI and can be called directly (`print twice(21)`), as can any `export func`. Functions taking or returning structs, unions, arrays, or fat pointers use Flag's own ABI (passing aggregates in memory or by address for >128 bytes and fat pointers) and are marked `DW_CC_nocall`: `gdb` refuses to call them (`Cannot call the function 'area' which does not follow the target calling convention`) and `finish` omits their return value. In `lldb`, call only C-ABI-compatible functions.
- **Codegen changes under `-debug`**: inlining is disabled (see [Inlining](#inlining)), all variables remain in stack slots without register promotion or struct slot cutting, by-address parameters get a slot holding the pointer, and basic blocks keep source order with line-table entries at every block start so stepping through loops revisits the header each iteration. Because blocks keep source order, a block's variables cover one range of addresses. The moves of registers that go between two blocks take the line of the block they enter, so `next` stops at the line after a loop, and each function describes its frame and the registers it saves in `.debug_frame`. A program built with `-debug` is a little slower and bigger; `flagc` itself is ~3% larger and compiles ~9% faster (no inlined code for the backend and the assembler).
- **macOS and Windows**: on macOS, the linker leaves DWARF in object files, so `-debug` assembles to `program.o`, links, runs `dsymutil` to produce `program.dSYM`, and deletes `program.o` unless `-keep` is set (if `dsymutil` cannot run: `cannot run dsymutil: ..., -debug on macOS keeps the DWARF in main.dSYM with dsymutil`). On Windows, `gdb` and `lldb` read the emitted DWARF; Visual Studio's PDB format is not emitted.

## Tests

`flagc test` compiles all test functions in the input files (and any packages loaded via quoted `use`, excluding the standard library) into a test binary and runs them.

- **Test discovery**: a test is any non-generic `func test_name()` with no parameters and no return value. Functions starting with `test_` that take parameters or return a value are treated as ordinary helpers. Files with no tests fail:

```text
error: no tests found; a test is a non-generic 'func test_name()' with no parameters and no return value
```

- **Naming and filtering**: a test's name is its function's name, after its module when it has one (`mathx.test_gcd`); a `priv` test in a file without a module takes the file as its module (`parse.flg.test_lines`). Words after `--` filter tests by substring match (e.g., `-- parse` runs `test_parse_int` and `test_parse_float`).
- **Execution and isolation**: the test binary starts in a `core` runner rather than your `main` (your `main` is still type-checked and callable from tests; write tests calling generic functions to check their instantiations). Each test runs in its own child process, up to `cores()` in parallel, with an empty stdin and stdout/stderr merged into a pipe (shown only on failure). Detached threads spawned by a test are awaited when it returns, as in `main`. `args()` returns the test runner's arguments.
- **Pass, fail, and timeout**: exit code `0` passes; `assert`, `panic`, runtime errors, crashes, non-zero `exit`, or timeouts fail. Timeouts default to `10` seconds (override with `-timeout`) and terminate with exit code `142` and a crash-style stack trace (implemented via `SIGALRM` on Unix, so a test installing its own `SIGALRM` handler or calling `alarm` overrides it, and a watchdog timer thread on Windows):

```text
flag: the test takes more than 10 s
  in test_hangs, tests/gcd_test.flg:15
```

- **Reporting**: after all tests complete, failures are printed to stdout in file order with declaration location, exit code (in hex for Windows exceptions, e.g., `exit code 0xc0000005`), and captured output, followed by pass/fail counts. `flagc test` exits `1` if any test failed:

```text
FAIL mathx.test_gcd_of_zero, tests/gcd_test.flg:7, exit code 134
checking gcd(7, 0)
flag: gcd(7, 0) == 0 is false, gcd(7, 0) is 7
  --> tests/gcd_test.flg:9:5
  in test_gcd_of_zero, tests/gcd_test.flg:7
3 passed, 1 failed
```

- **Debugging a single test**: the test binary accepts `program name [seconds]`. Pass `-o` (and `-debug`) to keep it and run one test under a debugger:

```sh
flagc test src -o tests -debug -- test_parse
gdb --args ./tests parse.test_parse
```

## Calling C

### extern

An `extern` block declares C functions and global variables and specifies how to link their library:

- `"c"`: the C standard library (always linked).
- `"Metal.framework"`: a macOS framework linked with `-framework Metal`. On non-macOS targets, fails with `Metal.framework is a macOS framework and cannot be linked on this target; guard the 'extern' block with 'where @macos()'`.
- **Library filenames** (ending in `.so`, `.so.<ver>`, `.a`, `.dylib`, or `.dll`): that exact file, so a versioned library needs no unversioned link. On Linux, linked as `-l:libfreetype.so.6` (searched in `-L` and system directories); on macOS and Windows, `flagc` searches `-L` directories (the standard library directory, source directories, and `-L` flags after `-cc`) and links the found path, failing with `library file libfreetype.dylib not found in search directories: lib/, ./` if missing. Do not include a leading `:` (`omit the leading ':' in 'extern' library file names: extern "libm.so.6"`).
- **Any other name**: linked as `-lname`.

```flag
extern "m":
    func cbrt(x: f64) -> f64

extern "Cocoa.framework" where @macos():
```

- **Conditional blocks (`where`)**: an `extern` block may carry a `where` clause testing OS and CPU builtins (same rules as top-level `where`; see [where](#where)). The library is linked and its symbols exist only on matching targets, so two systems can give one function two signatures (calling a function from an inactive block fails with `unknown function GetLastError`). An empty `extern` block simply adds the link dependency, which is how a library says that it needs another one:

```flag
extern "c" where not @windows():
    func socket(domain: i32, kind: i32, protocol: i32) -> i32

extern "ws2_32" where @windows():
    func socket(domain: i32, kind: i32, protocol: i32) -> u64 # Windows SOCKET is 64-bit

extern "m":
```

- **Overloading and duplicate C symbols**: two `extern` functions with the same parameter types in one module fail with `labs is already declared with the same parameter types` (regardless of return type or C symbol). Two `extern` functions with different parameters overload normally if they bind to different C symbols; binding both to the same C symbol without distinct Flag names fails: `labs is declared more than once, but C function labs can only have one signature; rename one declaration and link it with = "labs"`.
- **Custom C symbol names (`= "symbol"`)**: place `= "symbol"` after the return type (and before any `where`) to bind a Flag name to a different C symbol, useful for renaming collisions or typing polymorphic C functions like `objc_msgSend`:

```flag
extern "objc":
    func send_id(self: *bytes, sel: *bytes) -> *bytes = "objc_msgSend"
    func send_rect(self: *bytes, sel: *bytes) -> Rect = "objc_msgSend"
    func wait_child(status: *i32) -> i32 = "wait"
```

  Symbol names may contain letters, digits, `_`, `.`, and `$` and cannot start with a digit or `$`. Only `extern` declarations accept `= "symbol"` (`only 'extern' functions can specify a C symbol name with = "symbol"`).
- **C global variables**: declare as `name: *T`. The variable takes the `where` of its block and accepts `= "symbol"` like a function. `name` is a constant pointer to the C variable, read and written via `.*`, routed through a loader-populated word so it also works in `-shared` libraries. Omitting the pointer fails: `C variable optind in an 'extern' block is accessed by address; declare it as a pointer: optind: *i32`.

```flag
extern "c":
    index: *i32 = "optind"
    func fputs(text: cstr, file: *bytes) -> i32

extern "c" where @linux():
    c_stderr: **bytes = "stderr"

extern "c" where @macos():
    c_stderr: **bytes = "__stderrp"

func main():
    index.* = 1
    fputs("to the stderr of C\n", c_stderr.*)
```

  If a C global collides with a `core` name (like `stderr`), give it a different Flag name and bind it via `= "stderr"`. (On Windows, `stderr` is a function call `__acrt_iob_func(2)`, not a global variable.)
- **C source, object, and archive files**: pass `.c`, `.o`, and `.a` files after `-cc` (`flagc game.flg -cc vec.c`).

### export

`export func` emits a symbol with its plain Flag name using the C calling convention, even if nothing in Flag calls it. It cannot be generic, overloaded, `inline`, or named `main`, and cannot collide with a C symbol already used by the standard library (`'export' name close conflicts with an existing C symbol in the library; choose a different name`).

Its name evaluates to an `extern func(...)` callback value:

```flag
extern "c":
    func qsort(base: *bytes, count: i64, size: i64, compare: extern func(*i32, *i32) -> i32)

export func ascending(a: *i32, b: *i32) -> i32:
    return a.* - b.*

func main():
    values: array[i32, 3] = [3, 1, 2] # standalone would be array[i64, 3]
    qsort(ptr(values) as *bytes, 3, 4, ascending)
    print(values) # [1, 2, 3]
```

`ptr(name)` on an `export func` yields its address as `*bytes` (for C APIs taking `void *`). Taking `ptr` of a non-exported function is an error.

### C function types

`extern func(i32, Rect) -> Rect` is an 8-byte C function pointer that uses the platform C calling convention (including for structs). It is distinct from Flag's `func(...)` type.

- **Producing values**:
  - From an `extern` or `export func` name (`f := cbrt` has type `extern func(f64) -> f64`).
  - By casting a raw C address with `as`: from `os.symbol` (see [Libraries](#libraries)), `GetProcAddress`, OpenGL/Vulkan loaders, or C vtables (such as COM interfaces):

```flag
type Unknown:
    vtable: *Calls

type Calls: # COM vtable layout
    query: extern func(*Unknown, *bytes, **bytes) -> i32
    add_ref: extern func(*Unknown) -> u32
    release: extern func(*Unknown) -> u32

func drop(object: *Unknown):
    object.vtable.release(object)
```

- **ABI and behavior**: crosses the C ABI as a parameter, struct field, or variadic argument. A zero `extern func` is C `NULL` (passing it to C is allowed; calling it panics with `flag: called an uninitialized (empty) function value`). Supports `==` and `print` like `func`.
- **Restrictions**:
  - Cannot run directly in `parallel`: `'parallel' cannot run C function value extern func(f64) -> f64 directly; wrap the call in a Flag function`.
  - Non-exported Flag functions cannot convert to `extern func`: `Flag function double cannot be used as C function type extern func(i32) -> i32; mark it 'export func'`.
  - Variadic C functions have no function value.
  - Parameter and return types must be C-ABI-compatible where the type is written: `&str cannot cross the C ABI; only integers, floats, bool, pointers, structs, and untagged unions of those are allowed`.

### Types

| Flag | C |
|---|---|
| `i8`, `u8`, `bool` | `signed char`, `unsigned char`, `bool` |
| `i16`, `u16` | `short`, `unsigned short` |
| `i32`, `u32` | `int`, `unsigned int` |
| `i64`, `u64` | `long long`, `unsigned long long` |
| `f32`, `f64` | `float`, `double` |
| `*T`, `cstr`, `wstr` | Pointers |
| `extern func(T) -> U` | Function pointer |
| Structs, tuples, arrays inside structs, untagged unions | Matching struct or union by value |

- Use `i64` for C's `size_t` (`memcpy(to, from, l.len)`; a C size never goes past the largest `i64`), `u64` for OS handles (`pthread_t`, `HANDLE`), and `p as u64 == 0` to test for a C `NULL` pointer.
- A `bool` returned from C is `0` or `1` by its ABI, and one that C writes through a pointer has to be too (see [as](#as)).
- **Disallowed across C ABI**: tagged unions, fat pointers, Flag `func` values, and `simd` (pass fields or pointers, e.g., `ptr(text.*)` and `text.len`): `&strview cannot cross the C ABI; only integers, floats, bool, pointers, structs, and untagged unions of those are allowed`.
- Remember that C `long` is 64-bit on Linux/macOS and 32-bit on Windows.

### Variadic functions

An `extern` function may end with `...` for C varargs (`'...' is only for C variadic functions; Flag functions use '..items: T'`; conversely, `extern` functions cannot use `..items`).

Extra arguments must be integers, floats, `bool`, pointers, or `extern func` values (not Flag `func` values). C promotions apply: `f32` promotes to `f64`, fitting integer literals pass as `i32` (`printf("%d", 7)`), and text literals pass as `cstr` (`printf("%s\n", "hola")`). Other text requires `cstr(s)` or `c_text(s)`.

### Text

- **`cstr`** (null-terminated C string, distinct from `*u8`; a `*u8` parameter takes no text: `Flag text does not convert to *u8; C text uses 'cstr'`):
  - Text literals, text constants, and `@embed` values convert implicitly to `cstr` with no copy (all are statically null-terminated, and text literals work in constants/defaults like `TITLE: cstr = "window"`).
  - `&str` requires `cstr(s)` (temporary copy) or `c_text(s)` (writes trailing `0` in place when capacity permits): `convert &str to C text with cstr(s) (temporary copy) or c_text(s) (appends a trailing 0 byte)`. Other `&strview` values require `cstr(s)`.
  - `*u8` requires an explicit cast (`view(ptr(buf) as cstr)`): `*u8 requires an explicit 'as cstr' cast to promise null-terminated C text`. Cast back with `c as *u8`.
  - `view(c)` converts `cstr` to `&strview` (empty for `NULL`); implicit conversion fails with `convert cstr to &strview with view(c) (computes length)`.
  - `print(c)` and `"" + c` format the text (empty for `NULL`), while `c as u64` reads the address and `==` compares pointer addresses.
  - `c.*` reads the first byte; writing through `c.*` is disallowed (`cannot write through c.* because cstr may point to read-only literal memory; cast explicitly: c as *u8`).
- **`wstr`** (null-terminated UTF-16 string for Windows `W` APIs): no implicit conversion (not even from literals). Convert with `wstr(s)` (temporary UTF-16 copy) and `tmp(w)` (temporary UTF-8). `print(w)` displays UTF-8 text; `as` casts behave like `cstr`.
- **Stdio buffering**: C's `printf` buffers separately from Flag's `print`. Call `flush()` before C output and C's `fflush(NULL)` afterward when redirecting to a pipe or file.

## Libraries

`-shared` builds a dynamically loadable library (`.so` on Linux, `.dylib` on macOS, `.dll` on Windows). A library has no `main` (any `func main()` is an ordinary function) and exposes its `export func`s:

```flag
export func game_step(state: *State, dt: f64):
    state.time += dt
```

```sh
flagc game.flg -shared -o game.so
```

Load and call symbols via `os` (`load_library`, `symbol`, `close`; see [Libraries](Library.md#libraries)):

```flag
use os

func main():
    game := load_library("./game.so")
    if game is LoadError:
        panic("" + game)
    step := trust symbol(game, "game_step") as extern func(*State, f64)
    state: State
    step(ptr(state), 0.016)
```

#### Runtime and ABI details

- **Lazy per-thread initialization**: the first time any thread (including a C-created thread such as an audio callback) calls an `export func`, the library initializes its thread-local contexts' union tags and defaults (see [Unions](#unions)); constants and `shared context` are already in static data. If no context has defaults or non-null unions, initialization has zero overhead. (`export func`s in executables do the same for C threads.)
- **Isolated `core`, shared allocator and types**: each library carries its own `core` (scratch arena, stdio buffers, `shared context` including `on_panic`). Both sides share C's `malloc`, so heap blocks allocated by one side can be freed by the other, while arenas and scratch temporaries belong to the side that created them. Each calling thread gets a library scratch arena freed on thread exit (and the closing thread frees its own at `close`). Structs, pointers, fat pointers, unions (with deterministic tags), `Lock`s (using OS thread IDs), and `Channel`s are compatible across both sides when both declare matching types: a thread that holds a `Lock` in the program takes it again in the library without waiting for itself, and the same goes for a `Channel`.
- **Type-shape verification on load**: a library and the program can come from two versions of `flagc` as long as the types both of them made have one shape. Both embed a table of every non-`priv` type they constructed (including standard library types and generic instances) with a structural layout hash (size, alignment, field names/offsets/types, or alias/enum members). `load_library` compares all shared types (even those passed only behind pointers, such as hot-reloaded `State`) and rejects mismatches:
  - `cannot load ./door.so: its Door has other fields than the Door of this program`
  - `cannot load ./door.so: its Mood has other variants than the Mood of this program`
  Types used by only one side are ignored, and C libraries have no table.
- **Crashes and panics**: the host program owns hardware crash handlers (library frames are omitted from host crash traces like C frames; library threads still install the 64 KiB crash stack). Panics inside the library are reported by the library with its own call chain.
- **Stdio buffering**: library writes buffer separately from host writes (flushed on terminal newlines, `flush()`, `close`, and exit). Call `flush()` on each side before switching output on pipes or files.
- **Arguments and threads**: `args()` is empty inside a library. Because a library has no `main`, its detached threads are not awaited: one ends when the program does, and one still running when the program closes the library runs code that is gone and crashes; store their `Thread` handles and `wait` them in an `export func` before `close` (`-memory` warns about detached threads in shared libraries; see [The memory checker](#the-memory-checker)).
- **Linux TLS and linking**: on Linux, a library links with `-fPIC -Wl,-Bsymbolic`, so its calls and its function addresses go to its own functions, never to same-named ones in the program or another library. Library contexts use initial-exec TLS. glibc reserves ~1.5 KiB of static TLS across all dynamically loaded libraries; each library's `core` uses 136 bytes, and exceeding the pool fails with `cannot load ./game.so: cannot allocate memory in static TLS block` (reclaimed on `close`). Keep large context data behind pointers.
- **Hot reloading**: closing and reloading from the same path loads the updated library on Linux and Windows. macOS never unloads a `.dylib` containing thread-local data (which every Flag library has), so write each reloaded version to a fresh filename on macOS. See `tests/reload.flg` for a complete hot-reload example keeping `State` in the host.

## Platforms

| Target | Backend | Tier |
|---|---|---|
| `linux-x86_64` | `amd64_sysv` | First class |
| `linux-arm64` | `arm64` | Second class |
| `linux-riscv64` | `rv64` | Second class |
| `macos-x86_64` | `amd64_apple` | Second class |
| `macos-arm64` | `arm64_apple` | First class |
| `windows-x86_64` | `amd64_win` | First class |
| `wasm-wasm32` | `wasm` | Second class |
| `wasm-wasm64` | `wasm` | Second class |

Every backend is part of `flagc`: the native ones are in `src/backend/` (see [The backend](#the-backend)), and `src/wasm.flg` writes a WebAssembly module itself. Every native target has a bootstrap seed in `boot/`, so `./build.sh` starts from it on any of them (see [Building the compiler](#building-the-compiler)).

- **First-class targets** have release archives and CI building the compiler and running all tests natively.
- **Second-class targets** are compiled and assembled on every change (compiler, tests, and examples via `./build.sh cross`), without native execution in CI. A target moves to the first class when CI runs on it. `wasm-wasm32` and `wasm-wasm64` have their own tests, which `./test.sh` runs under `node` when it finds one, and for `wasm64` one with 64-bit memory (see [WebAssembly](#webassembly)).

By default, `flagc` targets the host platform. On `x86_64`, programs using `simd` require `x86-64-v2` (see [SIMD](#simd)). Use platform builtins (in a `where` or in any expression) and `where` blocks to specialize code per platform, as the standard library does.

### Cross compiling

Pass `-target` together with a cross-capable C compiler/linker via `CC`. For example, to target Windows from Linux using [zig](https://ziglang.org):

```sh
CC="zig cc -target x86_64-windows-gnu" flagc -target windows-x86_64 game.flg -o game.exe
```

### Windows

- Output binaries default to `.exe` (`a.exe`), and `flagc run` places temporary executables in `%TEMP%`.
- Standard input and output open in binary mode (`print` emits `\n`; `read_line` strips `\r` from `\r\n` on every OS).
- Paths returned by `os` use `/`; `fs` accepts both `/` and `\`.
- Paths, command-line arguments, and environment variables convert to/from UTF-16 at the Win32 boundary, so Flag always sees UTF-8 regardless of the system code page: a path with `ñ` or `字` works.
- `run(command)` and `output(command, into)` invoke `cmd.exe`. `run(program)` and `start(program)` quote arguments directly without a shell, treating extensionless names as `.exe` and rejecting `.bat`/`.cmd` (which `cmd.exe` would re-parse). Child processes inherit only stdin/stdout/stderr and do not spawn a console window from GUI programs. With a `folder` argument, relative executables like `./tool` resolve relative to the caller's current directory on Windows (unlike Linux and macOS, which resolve relative to `folder`).
- `net` uses Winsock; listening on a non-loopback address (other than `127.0.0.1`) may trigger a Windows Firewall prompt on first run.
- Any MinGW-w64 GCC works as `cc`, including the one in Strawberry Perl, and programs keep ASLR.
- A Flag function keeps the registers that the x64 convention of Windows has a callee keep, `xmm6` to `xmm15` included, so a callback that C calls leaves the floats of the C code around it alone.
- Only Linux `x86_64` ships a bundled `libraylib.a`; on Windows and macOS, pass the folder of your own after `-cc`: `flagc game.flg -cc -Lfolder`.
- The Windows console displays text using its active code page; run `chcp 65001` for UTF-8 output.

### WebAssembly

`-target wasm-wasm32` and `-target wasm-wasm64` make a WebAssembly module (`a.wasm`) that `flagc` writes itself, with no assembler and no C compiler. Flag is the same on both and on the other targets: a pointer and a length take 8 bytes, as `@sizeof(*i64)` says, and the library is the same. `wasm32` has the 32-bit memory of every WebAssembly engine, up to 4 GiB, and its memory instructions take the low word of an address, as Go does on WebAssembly. `wasm64` has 64-bit memory (Memory64), which Node 22 runs with `node --experimental-wasm-memory64`. Both use bulk memory, SIMD128, and the conversions that saturate. `@wasm()` holds on both, `@wasm32()` and `@wasm64()` on one.

- **The host, through WASI preview 1**: a program exports `memory`, `_start`, which runs `main` and then calls `proc_exit` with its exit code, the `export func`s of the program, and `__indirect_function_table` when the code has function values. It imports from `wasi_snapshot_preview1` only the functions its code reaches: `proc_exit`, `fd_write` for `print`, `eprint`, and `write`, `fd_read` when it reads `stdin`, and `args_sizes_get` and `args_get` when it calls `@argc()` or `@argv()`, which ask the host the first time. So a `wasm32` program runs unchanged on a WASI host: `wasmtime program.wasm one two`, or Node's `node:wasi`, with `new WASI({version: "preview1", args, returnOnExit: true})`, then `wasi.start(instance)`. A failed call of the host gives an `FsError` with the WASI error number, which prints as `WASI error 8`.
- **WASI on `wasm64`**: no WASI standard covers 64-bit memory, so the host must give the same functions with 64-bit pointers and sizes: each pointer and each size it takes is an `i64`, a `BigInt` in JavaScript, each iovec in memory is two `u64`s, the address and the length, and `args_get` writes 8-byte pointers. `tests/wasm.js` gives them on both widths.
- **Hosts tested**: the `wasm32` test programs give the same output and exit code on `tests/wasm.js`, Node 22's `node:wasi`, and wasmtime 27, apart from how each host reports a trap, with two exceptions. Wasmtime ends with `1` when `proc_exit` gives `126` or more, so a panic (`134`) and a stack overflow (`139`) end with `1` there, while a trap still ends with `134`. Node's `node:wasi` refuses a pointer argument past 2 GiB with error 28 (`EINVAL`), so `@argc()` is `0` when the heap passed 2 GiB before the first `@argv()`.
- **Imports of your own**: an `extern "module":` block imports its functions from that module, so `extern "env":` asks the host for more. Two declarations of one function of the host must take and give the same types: `fd_write is declared for the host with other types than before; a WebAssembly module imports one function of each name, so declare it with the same types`.
- **Values across the host**: integers, floats, `bool`, and pointers, which are addresses into `memory`. On `wasm64` a pointer crosses as an `i64`, a `BigInt`. On `wasm32` it crosses as an `i32`, a number in JavaScript that is below `0` past 2 GiB, and it keeps only the low word of the 8-byte pointer, as the memory instructions do. A JavaScript host reads an address of either width with `typeof at == "bigint" ? Number(at) : at >>> 0`. An `i64` or a `u64` is a `BigInt` on both widths, signed, so a `u64` of `2^63` or more arrives below `0`, and `BigInt.asUintN(64, x)` reads it unsigned. An `extern func` value crosses as an `i64`, its place in `__indirect_function_table`, and the host calls it with `table.get(Number(f))(...)` and the types the code uses inside, where a pointer is an `i64` on both widths. A struct does not cross by value (`Point cannot cross to the host on wasm-wasm32 by value; pass a pointer to it`), and neither do variadic C functions (`variadic extern functions are not supported on wasm-wasm32; ...`) or C variables.
- **`-shared`**: a library without `main` and `_start` that exports `memory`, its `export func`s, and the table when it has one, and imports only what its code reaches, so a library that only computes imports nothing.
- **The code**: each function goes through the passes of the backend that every target runs (SSA, slot promotion with the cutting of small structs, load and copy elimination, global value numbering and code motion, if-conversion into `select`, and division by a power of two as shifts), and `src/wasm.flg` writes it from that SSA: a value that the next instruction reads stays on the stack, a phi is a local that each jump to its block sets, and the jumps become blocks and loops. That always works, since Flag has no `goto` and the passes only drop, merge, or skip jumps, so every loop is entered at its head. A dense `switch` jumps through a `br_table`, a `simd` is a `v128`, a blit is `memory.copy`, and `sqrt`, `floor`, `ceil`, and `trunc` are instructions.
- **Memory**: static data starts at `0x1000`, above where `null` points, and is followed by an 8 MiB shadow stack, the size of a native main thread's stack, for the slots whose address escapes; the other values live in WebAssembly locals. A function whose frame would pass the end of the shadow stack writes what waits to go out, reports `flag: stack overflow`, and exits with `139`, as on Linux; in a `-shared` library it traps. `malloc`, `calloc`, `realloc`, `free`, `memcmp`, `memchr`, `strlen`, and `strtod` are Flag functions of `core` there, `memcpy`, `memmove`, and `memset` are instructions, and the heap grows `memory` a page at a time. `calloc` fills with zeros only the memory that the heap used before, since memory that grows starts at zero. On `wasm32` an allocation that would pass 4 GiB fails, and `malloc` gives `null`. Pointer arithmetic must not wrap below `0`, since WebAssembly adds constant field offsets without wrapping, and on `wasm32` `p + 4 GiB` reaches the same byte as `p`.
- **No protection**: WebAssembly memory has no read-only or unmapped pages below its size. Writing through `null` changes the first 4 KiB of memory, which later reads through `null` see (they read zeros until then), and writing into a text literal or `@embed` bytes changes them for every use, so `print("hello")` can print `Jello`. Only an address past the end of `memory` traps.
- **One thread**: `parallel` fails to compile (`threads ('parallel') are not supported on wasm-wasm32; ...`), a `Lock` never waits, a channel that would wait panics with a deadlock, and `atomic` operations are plain loads and stores.
- **What is not there**: `fs`, `os`, and `net` call C and do not build. WASI preview 1 reaches files only below the folders the host opens for the module, with error numbers of its own and no current folder, and has no processes and no outgoing connections, so they stay out. `snprintf` is missing, and `flagc run` and `flagc test` refuse the target: `'run' executes a program, but -target wasm-wasm32 builds a WebAssembly module; build with '-o' and run it in a WebAssembly host that gives WASI`.
- **Panics, traps, and output**: a panic writes its message and place and exits with `134`, but without the chain of calls, since WebAssembly hides its stack from the code. A trap, such as an address past the end of `memory` or the stack of the engine running out, ends the module in the host; with `-debug` the module carries the names of its functions, which the host shows in its trace. Output goes to the host a line at a time. A panic, `exit`, the end of `main`, running out of memory, and a stack overflow write what waits first, but a trap ends the module before its code runs again, so the text written since the last newline is lost.
- **Floats**: `print` and `parse_float` write and read the exact decimal of a double, the same text as on Linux, and `math` is fdlibm's algorithms in Flag (each within one unit in the last place, `exp` correctly rounded), with `sqrt`, `floor`, `ceil`, and `trunc` as instructions.
- `tests/wasm.js` is the host of the tests, and a small example of one: it runs `_start` in a thread with a 256 MiB stack, gives the WASI functions above on both widths, reading every address unsigned, and `env.apply(f, x)`, which calls back the function value `f` through the table, ends with the code of `proc_exit`, reports a trap with exit code `134`, and for a library prints what it imports and exports and what each function without parameters returns, with an `n` after a `BigInt`. `examples/mandelbrot.flg` says how to build and run a program with it.

## Editor support

These flags serve editors and language servers without producing a binary:

- **`-check`**: checks code and prints diagnostics quickly.
- **`-overlay file`**: substitutes unsaved buffer contents for the files listed in `file`. Each entry consists of three parts: a line with the file path, a line with the byte length, and that exact number of raw bytes. A listed `.flg` file that is not on disk yet counts as part of the folder of a quoted `use` it sits in, when that folder exists, and of a folder given on the command line it sits in at any depth.
- **`-at file:line:column`**: queries symbol information at a 1-based line and byte column, printing a `definition` line with where the symbol is declared and `hover` lines with its type or declaration. Out-of-range positions exit with code `2`: `error: -at main.flg:1:18 is past end of line (last column is 13)`.

```sh
flagc main.flg -check -at main.flg:12:5
```

```text
definition main.flg:3:6
hover func add(a: i32, b: i32) -> i32:
```

## Diagnostics

Errors, warnings, and notes print to stderr with file location, source line, and a `^` caret under the column:

```text
error: struct Point has no field z
  --> main.flg:7:13
   |     print(p.z)
   |             ^
```

- Only the first compile error is shown, followed by any related `note:` entries. The messages always say what is wrong and why; one that does not is a compiler bug. Most say what failed first and the rule or the fix after `;`.
- Locations use 1-based `file:line:column` where `column` counts **bytes** (so a preceding `ñ` adds 2), while `^` aligns visually under the character. Unprintable bytes display as escapes (`invalid character '\e'`, or `\x7f` if unnamed).
- Unexpected end-of-line errors point immediately after the last token:

```text
error: unexpected end of line; expected ':'
  --> main.flg:2:13
   |     if ready
   |             ^
```

## Building the compiler

`./build.sh` bootstraps `flagc` in four stages, with no tools but `gzip`, `cmp`, and the C compiler, which only assembles and links:

1. Packs `lib/` and `docs/` into an object file.
2. Builds stage 0 (`build/flagc0`) from the seed of the host, `boot/<system>-<machine>.s.gz`: the assembly of `flagc` for that target, compressed with `gzip`.
3. Compiles `src` through stage 0 -> stage 1 -> stage 2 -> stage 3 (`build/flagc`).
4. Verifies that stage 2 and stage 3 emit byte-identical assembly.

#### Build targets

- `./build.sh seed`: regenerates the six seeds in `boot/`, one per native target, using `-target`. A seed is rewritten only when its assembly changes, since `gzip` on another system may compress the same text to other bytes. The seeds are refreshed for a release or when one can no longer build `src`, since git cannot store a compressed seed as a difference and each refresh adds about 9 MB to the history.
- `./build.sh package`: creates `build/flag-<version>-<system>-<machine>.tar.gz`.
- `./build.sh cross`: compiles `flagc`, all tests, and all examples for second-class targets with `-target` and assembles them with `clang` (`linux-arm64` and `linux-riscv64` on Linux; `macos-x86_64` on macOS). CI runs it, so every system's `where` blocks and `rv64`'s lane-by-lane path compile on every change.

#### Running the test suite (`./test.sh build/flagc`)

| File pattern in `tests/` | Test behavior |
|---|---|
| `name.flg` + `name.out` | Must compile silently and print `name.out`. Receives `FLAGC` in its environment (used by tests like `reload.flg` to build helper libraries). |
| `name.flg` + `name.err` | Must fail compilation with `name.err` contained in the output. |
| `name.flg` + `name.warn` | Compiled with `-memory`; must contain `name.warn`, or emit no warnings if `name.warn` is empty. |
| `name.gdb` + `name.out` | Runs under `gdb` (when available) and compares output to `name.out` (stripping `libthread_db` lines and Windows `Thread 1 hit ` prefixes). |
| `name.args` | Extra flags passed to `flagc`, one build of the test per line, each of which must give the expected result (the WebAssembly tests build for `wasm32` and for `wasm64`); if `-check` is included, compares compiler stdout/stderr to `name.out` without running (used for `-at`). With `-target wasm-wasm32`, `node` runs the module through `tests/wasm.js`; with `-target wasm-wasm64`, a `node` with 64-bit memory, with `--experimental-wasm-memory64` when that `node` needs it. Without such a `node` the test is skipped. |
| `name.env` | Environment `NAME=value` pairs; runs an isolated copy of `flagc` in `build/tests/bin` that unpacks its embedded library (tests missing `cc` and cache failures). |
| `name.*` auxiliary / folder / no-file | Auxiliary files (like `name.overlay`) sit beside the test; folders test multi-file programs; tests with no `.flg` or folder test CLI usage errors. |

`./test.sh` also type-checks every file in `examples/`.

`./test.sh build/flagc windows-x86_64`, with `CC` set to a cross compiler such as `zig cc -target x86_64-windows-gnu`, builds each test for that target and runs it, which under WSL starts each `.exe` through interop. It skips the tests that start `flagc` itself, those whose error repeats the command line, and the `gdb` ones. From a WSL folder a few tests cannot pass: `cmd.exe` refuses a UNC folder, Unix tools like `printf` are missing, and the standard output and error arrive through separate pipes, out of order.

#### The backend

The backend lives in `src/backend/` and is Flag's own. It started as a port of [QBE](https://c9x.me/compile/) to Flag, whose license is in `LICENSE.qbe`, and has its own passes, data, and targets since. The frontend writes each function as a list of instructions of the backend, by their numbers and classes (`src/ir.flg`), and the backend reads them from memory, with no text form in between.

- `src/backend/fn.flg`: the data of a function (its blocks, temporaries, phis, immediates, and sets of temporaries), the instructions and what each one does, the order of the blocks, dominators and loops, SSA construction and its checks, and `lower`, which cuts a function of the frontend into blocks.
- `src/backend/opt.flg`: the optimizations that every target runs.
- `src/backend/regs.flg`: liveness, spilling, register allocation, and the parallel moves between blocks.
- `src/backend/amd64.flg`, `src/backend/arm64.flg`, `src/backend/rv64.flg`: for each processor, its calling conventions, instruction selection, and assembly: `amd64` for System V, Apple, and Windows, `arm64` for Linux and Apple, and `rv64` for Linux.
- `src/backend/asm.flg`: the order of the passes, the data of the program, and the end of the assembly file.

`compile` in `src/backend/asm.flg` takes each function through these passes:

1. `lower_narrow`: the 8- and 16-bit numbers that a call passes or returns are extended by the caller and the callee on Apple `arm64`, and are `i32` with any high bits elsewhere.
2. `optimize`, the same on every target: `chop` cuts each stack slot of 48 bytes or less whose address only reaches loads, stores, blits, and constant offsets into one slot for each piece (not with `-debug`), `promote` makes a slot that is only loaded and stored whole a temporary, `ssa` builds the SSA form, `forward_loads` replaces a load with the value that a store or a load before it gave, `result_slots` gives each call that returns a struct its slot, `coalesce` makes slots that are never alive at once share their place, `number_values` merges the instructions that compute one value and folds those on constants, `simplify_flow` merges blocks, `move_code` takes each instruction to the block that runs least among those it can go to, and on `amd64` and WebAssembly `convert_ifs` makes an `if` that only chooses values `select` instructions.
3. `hoist_thread_data`: on macOS and Windows, where the address of thread data costs a call or a lookup, each function reads it once.
4. `lower_calls`: the calling convention of the target for the parameters, the calls, and the returns.
5. `simplify`: a division by a power of two becomes shifts, a blit becomes loads and stores, and a jump table loads the address of its target.
6. `select`: the instructions of the processor, with its addressing modes and the registers that some instructions take.
7. `fill_live`, `fill_loops`, `fill_costs`, and `spill`: the temporaries that do not fit in registers go to the stack, which keeps in registers those needed again soonest.
8. `assign_registers`: each temporary gets its register, and the edges between blocks get the moves that connect them.
9. `skip_jumps`: a block that only jumps on is skipped, and the blocks are laid out in reverse postorder, or in the order of the source with `-debug`.
10. `emit_function`: the assembly.

A WebAssembly module runs passes 1 and 2 and the division of pass 5, and `src/wasm.flg` writes it from that SSA (see [WebAssembly](#webassembly)).

The passes take time and memory close to linear in the size of a function, so long chains of `elif`, `switch`es of thousands of cases, and big array literals do not slow them down with their square.

## Rough edges

Behaviors that are easy to trip over:

- **Mutating text literals**: writing into a text literal via `items`, `ptr`, `as`, or C compiles and crashes at runtime (`flag: invalid memory access`; see [Fat pointers](#fat-pointers)), and on WebAssembly changes the literal instead (see [WebAssembly](#webassembly)). Copy to a `&str` first.
- **Overloaded/generic function values as arguments**: function call arguments do not propagate target types to function values, array literals, or untyped struct literals. `f(ptr([1, 2]))` passes `*array[i64, 2]` even when `f` expects `&listview[i32]`, and `f({1, 2})` passes a tuple even when `f` expects `Point`. Bind to a typed variable first, or name the type (`f(Point{1, 2})`).
- **Enum variants in a literal**: `[Hidden, Local]` is an array of the union `Local|Hidden`, not of their enum, so a generic that takes it makes `Set[Local|Hidden]`, which does not go where `Set[Flag]` goes. Name the enum on the first element: `[Flag(Hidden), Local]`.
- **`trust` on a real error**: skips the tag check, producing garbage or a crash.
- **Stale views in argument lists**: views evaluate left to right, so `show(l, grow(ptr(l)))` passes `l`'s pre-`grow` buffer to `show` (see [Order](#order)). Compute `grow` on a separate line (`-memory` warns if `show` reads it).
- **Ambiguous `null` on `T?` collections**: `Channel` `receive`, list `pop`, and map `m[k]` / `remove` flatten `null|T?` into `T?` (see [Unions](#unions)), so a stored `null` looks identical to empty/closed/missing. Use `for v in channel`, check `l.len` before `pop`, or check `contains(m, k)` before `m[k]`.
- **Fresh contexts in new threads**: a new thread (from `parallel`, or a C thread calling an `export func`) starts with fresh default `context` values, not the parent's. Pass values explicitly, mark the context `inherited`, or use `shared context`.
- **Memory checker trade-offs**: `-memory` produces some false positives ([What asks for @trust](#what-asks-for-trust)) and has documented blind spots ([What it misses](#what-it-misses)).
- **Mutating a list during `for`**: loop elements alias the container in place. Modifying the list can invalidate narrowed types, and growing it dangles the loop variable (`-memory` warns; see [Iteration](#iteration)).
- **Shallow fat-pointer assignment**: `a = b` between two `&str` or two `&list` values aliases their buffer rather than copying (see [Fat pointers](#fat-pointers)). Only view-to-owner assignment copies (`a = view(b)`, `a = b[:]`).
- **Reassigning a `with` variable**: `with f:` closes whatever `f` holds at block exit, so reassigning `f = ...` inside leaks the previous file (`-memory` tracks memory, not files; see [with](#with)).
- **Separate stdio buffers in `-shared` libraries**: into a pipe or file, host and library output interleave by buffer flush rather than call order (see [Libraries](#libraries)).
- **Casting C function pointers to `func`**: casting a C address via `as func(...)` compiles, but calls it with Flag's aggregate ABI rather than C's; always cast C function pointers to `extern func(...)`.
