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

Run it with `flagc run`:

```sh
flagc run example.flg
```

```text
alice is at 1
there is no ada
```

## Features

- Python-like syntax
- Statically typed
- Built for Data-Oriented Design (lots of features for data structure definitions)
- Manual memory management with `defer`, no garbage collector, and arenas that release everything at once, with the same lists, text and maps
- Optional memory safety at comptime with the `-memory` flag (makes compiling slower)
- Runtime errors and crashes print the chain of calls of your code, and `-debug` builds for gdb and lldb
- `flagc test` runs every `func test_name()` in a process of its own, in parallel and with a timeout
- Errors as values that can carry data, handled with `is`, `try`, `trust`, `iferror` or a `switch`, and a dropped error doesn't compile
- Tagged unions, narrowed by `is` and by the cases of a `switch`, and fat pointers built into the syntax
- Structs compare and hash themselves, so they work as map keys
- Automatic context management across functions with the `context` keyword: a context for each thread, `inherited` by the threads it starts, or `shared` by all of them, and nothing runs before `main`
- Threads with `parallel f(x)` and `wait(t)`, a `Lock[T]` that wraps what it guards and works at zero, and bounded `Channel`s
- Generics support
- Function and operator overloading, with `where` to choose an overload at compile time
- Function values, without closures
- Small functions are inlined and small structs live in registers, `inline func` inlines at every direct call
- `simd[T, n]`, lanes of numbers in one or two registers with operators that work on all of them, on SSE and NEON
- C ABI FFI to interop with other languages (both ways), with typed C function values, and shared libraries that a program loads and reloads while it runs
- A small standard library: text, lists, maps, buffered files, folders, other programs, JSON, TCP, UDP and raylib
- The backend of QBE, ported to Flag, for really fast compilation; for example, the compiler builds itself in about a second and a half
- Support for Linux, macOS and Windows, and WebAssembly modules (`-target wasm-wasm64`) that `flagc` writes without a C compiler

## Documentation

- [Overview](docs/Overview.md): a tour of the language
- [Reference](docs/Reference.md): the exact rules, the limits, and how the compiler works
- [Library](docs/Library.md): every function of the standard library
- [Examples](examples): small programs showing language features

The compiler carries the documentation of its version. `flagc -docs` prints the folder that holds it, so it works offline and always matches what you installed:

```sh
grep -n '^## ' "$(flagc -docs)/Reference.md"
```

## Installation

Download the archive for your system from [releases](https://github.com/FramaLuna/flag-lang/releases):

| Platform | Archive |
| --- | --- |
| Linux (`x86_64`) | `flag-0.6.3-linux-x86_64.tar.gz` |
| macOS (`arm64`) | `flag-0.6.3-macos-arm64.tar.gz` |
| Windows (`x86_64`) | `flag-0.6.3-windows-x86_64.tar.gz` |

Unpack it and add its `bin` folder to your `PATH`:

```sh
tar -xzf flag-0.6.3-linux-x86_64.tar.gz
export PATH="$PWD/flag-0.6.3-linux-x86_64/bin:$PATH"
flagc -version
```

`flagc` is a single file with the standard library embedded inside it. You also need a C compiler; see [Requirements](#requirements).

- **macOS:** if the system says it cannot check the developer, run:
  ```sh
  xattr -d com.apple.quarantine flag-0.6.3-macos-arm64/bin/flagc
  ```
- **Windows:** unpack the archive the same way from the MSYS2 shell, or with `tar -xzf` in PowerShell.

For diagnostics, hover, and go-to-definition in your editor, see [flag-lsp](https://github.com/FramaLuna/flag-lsp).

## Requirements

`flagc` turns your code into assembly and links it with a C compiler, so you need one installed:

- **Linux:** `gcc` or `clang`, usually already installed (`sudo apt install gcc` on Debian and Ubuntu).
- **macOS:** the Xcode Command Line Tools (`xcode-select --install`).
- **Windows:** `gcc` from [MSYS2](https://www.msys2.org); in its UCRT64 shell, run `pacman -S mingw-w64-ucrt-x86_64-gcc`.

If your C compiler is not called `cc`, set the `CC` environment variable (for example, `CC=gcc`). [Zig](https://ziglang.org) works on all three systems with `CC="zig cc"`, and can also cross-compile for the other two.

## Build from source

Clone the repository with git and run `build.sh`. Besides the C compiler from [Requirements](#requirements), it only needs `cmp`, which Windows gets from `diffutils`.

```sh
git clone https://github.com/FramaLuna/flag-lang.git
cd flag-lang
./build.sh
```

The compiler ends up in `build/flagc` (`build/flagc.exe` on Windows).

On Windows, run it from the MSYS2 UCRT64 shell with `CC=gcc`. `build.sh` uses `cmp` to check that the compiler built by itself emits the same code twice.

## Status

Flag is **not** a production-ready programming language.

Except for the lexer (`src/lex.flg`), which is the base of the code style, all of the code in the compiler was rewritten by AI.

## Contributing

If you have cool ideas to make the language better, open an issue and I may add them.

You can also open pull requests, and I will review them. Just make sure to open an issue first and link it from the pull request.

You can use AI, just make sure to use a good frontier model, and review what it writes or have another model review it before publishing.

