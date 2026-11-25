# Flag

Flag is a compiled programming language, a personal mix of ideas from many languages. This repository holds `flagc`, its compiler.

## Quick example

```flag
type error NotFound:
    name: &strview

func index_of(names: &listview[&strview], name: &strview) -> i64|NotFound:
    for i, n in names:
        if n == name:
            return i
    return NotFound{name}

func main():
    names := ["bob", "alice"]
    for name in ["alice", "ada"]:
        found := index_of(ptr(names), name)
        switch found:
        case i64:
            print(name, " is at ", found)
        case NotFound:
            print("there is no ", found.name)
```

Run it directly with `flagc run`:

```sh
flagc run example.flg
```

```text
alice is at 1
there is no ada
```

## Features

### Language

- **Python-like syntax**, statically typed.
- **Data-oriented design:** rich support for defining memory layouts and data structures.
- **Errors as values:** errors can carry data and are handled with `is`, `try`, `trust`, `iferror`, or `switch`; dropping an error fails to compile.
- **Tagged unions and fat pointers:** unions narrow automatically under `is` and `switch` cases, and fat pointers are built into the syntax.
- **Automatic struct comparison and hashing:** structs compare and hash themselves, so they work directly as map keys.
- **Generics and overloading:** supports generic types and functions, function and operator overloading with `where` clauses to choose overloads at compile time, and function values (without closures).
- **Automatic contexts:** pass state across functions with `context`. A context is per-thread by default, `inherited` by threads it starts, or `shared` across all threads, and nothing runs before `main`.

### Memory and concurrency

- **Manual memory without a garbage collector:** clean up resources with `defer`, or allocate in arenas that free everything at once while using the same lists, text, and maps.
- **Optional compile-time memory safety:** pass `-memory` to check for leaks, double frees, use-after-free, dangling pointers, and data races (at the cost of slower compilation).
- **Built-in concurrency:** spawn threads with `parallel f(x)` and join them with `wait(t)`, guard data with `Lock[T]` (which wraps what it protects and works when zero-initialized), and communicate through bounded `Channel`s.

### Performance and interop

- **Fast compilation:** `flagc` uses its own backend, originally ported from QBE to Flag; the compiler builds itself in under three seconds.
- **Inlining and registers:** small functions are inlined automatically, `inline func` inlines at every direct call, and small structs live in registers.
- **Portable SIMD:** `simd[T, n]` holds lanes of numbers in one or two registers with lane-wide operators on SSE, NEON, and WebAssembly SIMD128.
- **C interop and hot reloading:** two-way C ABI FFI with typed C function values, plus shared libraries that a program can load and reload while running.

### Tooling, library, and platforms

- **Debugging and traces:** runtime errors and crashes print the call stack of your code, and `-debug` emits debug info for `gdb` and `lldb`.
- **Built-in test runner:** `flagc test` runs every `func test_name()` in its own process, in parallel and with a timeout.
- **Small standard library:** text, lists, maps, buffered files, folders, other programs, JSON, TCP, UDP, and raylib.
- **Native and WebAssembly targets:** runs on Linux, macOS, and Windows, and builds WebAssembly modules (`-target wasm-wasm32` or `-target wasm-wasm64`) without a C compiler, optimized by the same backend, for WASI hosts such as wasmtime or Node.

## Documentation

- [Overview](docs/Overview.md): a tour of the language
- [Reference](docs/Reference.md): the exact rules, limits, and how the compiler works
- [Library](docs/Library.md): every function of the standard library
- [Examples](examples): small programs showing language features

The compiler embeds the documentation for its own version. Run `flagc -docs` to print the folder that holds it, so you can read or search it offline and know it matches your installed compiler:

```sh
grep -n '^## ' "$(flagc -docs)/Reference.md"
```

## Installation

Download the archive for your platform from [releases](https://github.com/FramaLuna/flag-lang/releases):

| Platform | Archive |
| --- | --- |
| Linux (`x86_64`) | `flag-0.7.0-linux-x86_64.tar.gz` |
| Linux (`arm64`) | `flag-0.7.0-linux-arm64.tar.gz` |
| macOS (`arm64`) | `flag-0.7.0-macos-arm64.tar.gz` |
| macOS (`x86_64`) | `flag-0.7.0-macos-x86_64.tar.gz` |
| Windows (`x86_64`) | `flag-0.7.0-windows-x86_64.tar.gz` |

Unpack the archive and add its `bin` folder to your `PATH`:

```sh
tar -xzf flag-0.7.0-linux-x86_64.tar.gz
export PATH="$PWD/flag-0.7.0-linux-x86_64/bin:$PATH"
flagc -version
```

`flagc` is a single file with the standard library embedded inside it. Native builds also need a C compiler; see [Requirements](#requirements).

- **macOS:** if macOS reports that it cannot verify the developer, clear the quarantine attribute (use `x86_64` on Intel Macs):
  ```sh
  xattr -d com.apple.quarantine flag-0.7.0-macos-arm64/bin/flagc
  ```
- **Windows:** unpack the archive the same way from the MSYS2 shell, or with `tar -xzf` in PowerShell.

For diagnostics, hover, and go-to-definition in your editor, see [flag-lsp](https://github.com/FramaLuna/flag-lsp).

## Requirements

For native targets, `flagc` compiles your code to assembly and links it with a C compiler, so you need one installed (WebAssembly targets need no C compiler):

- **Linux:** `gcc` or `clang`, usually already installed (`sudo apt install gcc` on Debian and Ubuntu).
- **macOS:** the Xcode Command Line Tools (`xcode-select --install`).
- **Windows:** `gcc` from [MSYS2](https://www.msys2.org); in its UCRT64 shell, run `pacman -S mingw-w64-ucrt-x86_64-gcc`.

If your C compiler is not called `cc`, set the `CC` environment variable (for example, `CC=gcc`). [Zig](https://ziglang.org) works on all three systems with `CC="zig cc"` and can also cross-compile for the other two.

## Build from source

Clone the repository with `git` and run `build.sh`. Besides the C compiler from [Requirements](#requirements), it only needs `gzip` and `cmp` (on Windows, MSYS2 includes `gzip`, and `cmp` comes with `diffutils`):

```sh
git clone https://github.com/FramaLuna/flag-lang.git
cd flag-lang
./build.sh
```

The built compiler is placed at `build/flagc` (`build/flagc.exe` on Windows).

On Windows, run `build.sh` from the MSYS2 UCRT64 shell with `CC=gcc`. `build.sh` uses `cmp` to verify that the compiler built by itself emits the exact same code twice.

## Status

Flag is **not** a production-ready programming language.

Except for the lexer (`src/lex.flg`), which sets the code style, all of the code in the compiler was rewritten by AI.

## Contributing

- **Ideas:** if you have cool ideas to make the language better, open an issue and I may add them.
- **Pull requests:** open an issue first and link it from the pull request, and I will review it.
- **AI-assisted code:** you can use AI, just use a good frontier model and review what it writes (or have another model review it) before publishing.
