# Flag

This is the compiler for the Flag programming language, my own language, a mix of ideas from lots of programming languages.

## What the language has

- Python-like syntax
- Statically typed
- Built for Data-Oriented Design (lots of features for data structure definitions)
- Manual memory management with `defer`, no garbage collector
- Optional Memory-safety at comptime using -memory flag (makes compiling slower)
- Errors as values, with `trust` and `try` for error handling
- Built-in syntax for tagged unions and fat pointers
- Automatic context management across functions with the `context` keyword
- Generics support
- Function and operator overloading, with `where` to choose an overload at compile time
- ABI FFI to interop with other languages (both ways)
- QBE backend for really fast compilation, for example, the compiler builds itself in under a second
- Support for Linux, macOS and Windows

## Quick Code Example

```flag
type error NotFound

func index_of(names: &list[&strview], name: &strview) -> usize|NotFound:
    for i, n in names:
        if n == name:
            return i
    return NotFound

func main() -> i32:
    names: &list[&strview]
    defer free(names)
    append(names, "bob")
    append(names, "alice")
    print(index_of(names, "alice"), " ", index_of(names, "ada"))
    return 0
```

## Status

Important: this is not a production-ready programming language, so be aware.

Except for the lexer which is the base of the code style, all the code in the compiler was rewritten by AI.

## Docs

- [Overview](docs/Overview.md), a tour of the language
- [Reference](docs/Reference.md), the exact rules, the limits and how the compiler works
- [Library](docs/Library.md), every function of the standard library
- [Examples](examples)

## Installation

Download the archive for your system from the [releases](https://github.com/FramaLuna/flag-lang/releases): `flag-0.1.0-linux-x86_64.tar.gz`, `flag-0.1.0-macos-arm64.tar.gz` or `flag-0.1.0-windows-x86_64.tar.gz`. Unpack it and put its `bin` folder in your `PATH`

```sh
tar -xzf flag-0.1.0-linux-x86_64.tar.gz
export PATH="$PWD/flag-0.1.0-linux-x86_64/bin:$PATH"
flagc -version
```

`flagc` is a single file, the standard library is inside it. You also need a C compiler, see [Requirements](#requirements).
- On macOS, if the system says it cannot check the developer, run `xattr -d com.apple.quarantine flag-0.1.0-macos-arm64/bin/flagc`
- On Windows, unpack it the same way from the MSYS2 shell, or with `tar -xzf` in PowerShell

For errors, hover and go to definition in your editor, see [flag-lsp](https://github.com/FramaLuna/flag-lsp).

## Requirements

flagc turns your code into assembly and links it with a C compiler, so you need one
- Linux: gcc or clang, usually already installed (`sudo apt install gcc` on Debian and Ubuntu)
- macOS: the Xcode command line tools, `xcode-select --install`
- Windows: gcc from [MSYS2](https://www.msys2.org), in its UCRT64 shell `pacman -S mingw-w64-ucrt-x86_64-gcc`

If yours is not called `cc`, set the `CC` variable, for example `CC=gcc`. [zig](https://ziglang.org) works on the three systems with `CC="zig cc"`, and it can also build for the other two.

## Build from source

Clone the repo and run `build.sh`, it only needs the C compiler of the requirements. On Windows run it from the MSYS2 UCRT64 shell

```sh
cd flag-lang
./build.sh
```

The compiler ends up in `build/flagc`.

## Contributing

If you have cool ideas to make the language better, open an issue and I may add them.

You can also open pull requests, and I will review them. Just make sure to open an issue first and link it from the pull request

You can use AI, just make sure to use a good frontier model, and review what it writes or have another model review it before publishing.

