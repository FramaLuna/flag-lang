# Changelog

## 0.3.0

A release about errors that say what happened and unions you handle with `switch`, with a standard library that is much faster and has one way to do each thing.

New
- `switch` over a tagged union names the types of its members: `case i32:`, `case A|B:`, `case f64, null:`. In each case the variable has that type, like after `is`, and the default gets the types no case named. A switch over a `bool` with `case true:` and `case false:` is complete
- Open slices, `x[a:]`, `x[:b]` and `x[:]`, on text, lists, slices, arrays and your types with a `len` field
- Structs and unions without their own `==` compare field by field, and get a `hash` the same way, so a struct with text fields works as the key of a map
- `m[k] += v`, and the other assignments with an operator, on maps and on any type whose `x[i]` gives a copy. A key that is not there counts as zero, and on a map the key is looked up once
- Errors with data, `type error BadLine:` with fields, are documented and work everywhere: `try`, `iferror`, `switch`, `print`
- `File` is in core: `stdin`, `stdout` and `stderr`, and `open`, `create`, `read`, `read_line`, `write`, `flush` and `close` for any file. `create(path, append = true)` writes at the end without emptying it, and `flush()` sends what waits in every file
- A `Socket` of `net` reads and writes like a file, with `read`, `read_line`, `write`, `flush` and `close`, and `timeout(s, seconds)` makes a slow read or write fail with the code `TIMED_OUT` on every system. A server is a `Server`, its own type, so a file, a server and a connection never go one where the other goes
- What the system refuses is an `FsError` with the code, what failed and the path, and printing it says all of it: `cannot open notes.txt: No such file or directory`. `net` fails with a `NetError` of the same fields: `cannot connect to example.com:80: Connection refused`. `NotJson` says where and why: `not JSON at line 3, column 14: expected ':' after the key`. With `errno()` your own C calls fail the same way
- A name you declare that core also declares, like `File`, `Integer` or `stdout`, is yours in your files, and the library keeps its own, shown as `core.File` in messages
- A field or an element of a value in a map changes in place, the key found once and added with a zero value when it is missing: `players[name].score += 10`, `pairs[a][b] += 1`
- `words(text)` splits on spaces, tabs and line ends: `for w in words(text): counts[w] += 1`
- `append(list, items)`, `clear` and `reserve` for lists, `clear` for rings, `find_last`, `pad`, `view(bytes)`, `free(p)` for what `heap` gives, the `Integer` alias and `json.key(d, value)`
- A value goes into a union through the member it converts to: a `&str` into `&strview|i32`, a `&list[T]` into `&slice[T]?`, and `{}` or `{1, 2}` into a union with one struct, also `m[k] = {}`
- `a += b` on a `&strview` joins like `a = a + b`, and a `for` walks through a pointer to an array, a list, a slice or text
- `examples/grep.flg`, files line by line with the errors of each step

Faster
- The output is buffered: printing a million lines is 7 to 10 times faster, and `print` allocates nothing. It goes out line by line on a terminal
- `for c in text` is written by the compiler and is 57 times faster, `find` 39 times, floats print 6 times faster, `read_line` 4 times, `split` 3 times, counting words in a map 2 times, and `json.parse` 2 times. A line of 80 MB reads in 0.13 s, it took 3.2 s
- The compiler checks itself with half the instructions and builds itself with 38 % fewer: the overload a call chooses is remembered, and no code is generated to be thrown away
- Structs, unions and arrays bigger than 128 bytes are copied with `memcpy` and passed by address. A struct of hundreds of kilobytes passed by value compiled for minutes, now it's instant
- The `==` and `hash` that the compiler makes are one function per type, so big and nested structs compile at once
- A `context` takes no room in the program file, the tags of its unions are written when the program starts
- QBE is linear on big functions: a loop over an enum of 1500 variants compiles in 0.2 s, it took 37 s. Long chains are faster too: 20000 `elif` compile in 8 s, they took 38 s
- A run of `and` or of `or` is tested in one block after another, so what the left side narrowed is known once: 10000 terms compile in 3.5 s, they took 42 s and 1.8 GB
- The compiler uses less stack: blocks, expressions and types nest up to 500 levels (blocks were about 150), and chains of `elif`, `ifnull`, `iferror` or of one operator are not nesting
- A connection sends at once (`TCP_NODELAY`)

Changed
- An enum without a value starts as the first variant you wrote, also when another enum declared some of its variants before, and two enums with all the same variants are an error: `type Paint = Color` gives one a second name
- A case of a switch over a union is a type, never a value or a variable, and a default that can never run is an error
- A deferred call that can fail doesn't compile, `defer _ = f()` drops the error on purpose, and a `with` whose `close` can fail is an error
- A type with its own `==` and no `hash` can't be the key of a map, declare its `hash` too
- The counter of a range loop and the index of `for i, x in` can't be assigned
- `arg_count`, `arg`, `write(fd, text)` and `os.read_line()` are gone: `args()` of `os`, `write(stdout, x)` and `read_line(stdin)`
- Files moved from `fs` to core, and `read_text_file` is `read_file`. `close(f)` gives nothing, so `with file:` works, and `flush(f)` gives the first write that failed since the last `flush`. A write that is interrupted or refused for now is tried again
- `stdout` and `stderr` keep the order of your calls, and every file sends what waits before the program waits to read and before another program runs. A program whose standard output could not be written ends with `flag: cannot write the standard output: ...` and the exit code 1
- `send` and `receive` of `net` are `write` and `read`, `NetError` is a struct with the code, and `CannotRun` is gone: `os.run` fails with `FsError`
- `os.run` and `os.output` take a `&slice` and give `i32|FsError`, and `output(command, into)` appends to your text and gives the exit code. On Windows `run` of a list refuses `.bat` and `.cmd`, `cmd.exe` would read their arguments again
- Files, connections and pipes of the library are not passed to the programs you run
- `get` is gone from maps and fixed maps, `m[k]` is the way. `json.text` gives `null` when the value is not a string, `json.quoted(text)` is a value for `+` and `print`, and `json.free` takes the document by pointer
- `write_file`, `append_file` and `change_dir` give `FsError?`, `fs.join` is `join_path`, `swap` takes any integer, `bits` any integer index and `hex` any integer, printing the bits of its size
- `parse_float` takes only decimal numbers, `inf` and `nan`, and `fixed` prints `0`, not `-0`
- Text is read with one UTF-8 rule: an overlong form, a surrogate or a code above U+10FFFF comes byte by byte in `for c in text`, `utf8.count` counts those pieces and `utf8.valid` agrees
- `print` computes all its values before it writes
- An error inside a generic of the library points at your call, with a note for the line in the library
- No type can be made from an empty literal (`wrap([])` into `array[$T, 2]`), `heap([])` still works
- `at` for a list, a slice, an array or text can't take an index that can be an integer
- A module with a `priv` function keeps that name for itself, so an exact public overload next to a `priv` generic wins
- Passing an empty function value stops the program at that call
- `for` over text belongs to the language, like over lists, so `iter` for text can't be declared
- `-memory` takes a free through an element as the free of all the elements, so freeing them in a loop or with `pop` doesn't warn, and freeing only some of them is not reported
- Type names in messages read like code, `func(i32, i32) -> bool`, and many messages say more about why

Fixed
- `m["a"] = show` with `show` overloaded, `return {1, 2}` into a `Point?`, and `(usize(0) as func())()` skipping the check of an empty function value
- `-memory` false positives: unions whose members keep a pointer in the same place, reading a whole file and printing its error, errors with data after `try`, `is` on a union like `&str|FsError`, and print, read a file, print
- Using `net` ignored `SIGPIPE` for the whole program and the programs it ran
- `is_dir` of a folder you cannot read, `file_size` of a FIFO (it waited forever), `remove` of a link to a folder, `walk` listing locked folders as files, `executable()` after `change_dir`, and `list_dir` and `walk` on macos-x86_64
- `rename` replaces an existing file on Windows too, and `output(command)` keeps every byte on Windows
- `os.output` couldn't tell a program that doesn't start from one that prints nothing
- An `eprint` inside your `operator +=` stopped the program inside core, and `out_of_memory` could call itself
- `build.sh` rebuilds QBE when one of its headers changes

What may still surprise you is in [Rough edges](https://github.com/FramaLuna/flag-lang/blob/v0.3.0/docs/Reference.md#rough-edges).

## 0.2.0

A release to make the language stable, with two new things: function values and `pass`.

New
- Function values: the name of a function is a value, with the type `func(i32) -> bool`. They go in variables, fields, arrays and arguments, and they are called like any function. There are no closures
- `pass`, a statement that does nothing, for a block that has to be empty
- A generic alias works in a parameter (`func get(x: Opt[$T])`), and `$T` also comes from inside a function type
- `sort(items, before)` sorts with your own order and `find(items, fits)` finds the first element that a function accepts
- `{k: v}` fills any type that has `put`, not only maps
- An array can be sliced directly, `values[0:2]`

Faster
- `x[i]` on lists, slices and text, and `for` over arrays, lists and slices, are written by the compiler instead of calling the library. A loop that walks a list by index or with `for` runs about 7 times faster, and the compiler checks its own code in two thirds of the time
- The IL that goes to QBE is smaller: no empty blocks, no jumps to the next line

Changed
- The keywords of the language cannot be used as names anymore
- A union always holds a real tag. Memory that you zero yourself is `null` for a union that has `null`, and it's not a value for a union that doesn't (a variable, a field or a literal still starts as its first member)
- In an assignment the right side runs first, so `l[0] = grow(l)` and `s += "x" + s` do what they say
- A statement that only makes a value (`x == 2`) and a variable that keeps an error nobody reads are errors
- `priv` in a file without a module hides the name from the other files, and the helpers of the library are `priv` now
- `as` doesn't turn a value into a union or a union into another of the same size
- An operator between two numbers of one type cannot be declared, `==` and `<` must return `bool`, and `operator +=` is only the one that prints
- `at` with an integer index cannot be declared for lists, slices, arrays and text, nor `iter` for lists, slices and arrays, and `at(list, i)` is gone: a pointer to an element is `ptr(list[i])`
- Code can nest about 150 blocks deep, and a chain of `elif` or of one operator can have about 300 parts. More than that is an error now, where 0.1.0 crashed a bit further on
- A type with parameters cannot have a block of fields inside, and `a.Type` needs `use a`
- Several things that compiled by accident are errors: a function declared twice, a constant and a function with one name, `x := []`, `0x_ff`, `1e5e3`, a call that returns nothing used as a value (also on the right of `iferror`: `x := load() iferror stop()`), assigning to a slice or to an element of a temporary, `defer x := 1`, variants of an enum at different indentations
- The counter of a `for` over a range with literal ends is not passed as a pointer on its own, write `ptr(i)`. A function that changed it made the loop index an array out of bounds without a check
- A `priv` function that fits a call wins over the ones that are not `priv`, so a function of yours with the name of a helper of the library no longer replaces it
- A `&str` given to C as a `cstr` is never grown: when it has no room for the zero it goes as a temporary copy. Growing it left the other copies of that text pointing to freed memory
- The slice of an `array[u8, N]` is a `&slice[u8]`, it was text
- A module of the library is the file of its name, `use json` loads `lib/json.flg`
- The signature of a C function is checked where it is declared, and a fat pointer cannot be a parameter of one
- When two overloads tie, the one that takes its arguments through a shape like `&slice[$T]` wins over a bare `$T`
- `json.parse` takes exactly JSON, `fs.walk` doesn't follow links to folders, and several messages of runtime errors say more (`slice 4:2 is out of bounds, length 6`)

Fixed
- More than a hundred bugs: the compiler crashing or hanging on code it didn't expect, programs reading the wrong member of a union, `defer` and loops losing track of what a variable holds, text appended to itself, constants that name a field, wrong results in `fs`, `json`, `os` and the printing of floats, and many messages that pointed at the wrong place or didn't say what was wrong
- On macOS, a function with more than 32 KiB of locals could store a 64-bit number as 32 bits. It was a bug of QBE for Apple ARM64, and it made `-memory` never end on a struct bigger than 4096 bytes
- The memory checker: it reports the memory of a `&ring`, the free of a temporary and the real place of a free, and it no longer invents warnings in functions with many allocations

What may still surprise you is in [Rough edges](https://github.com/FramaLuna/flag-lang/blob/v0.2.0/docs/Reference.md#rough-edges).

## 0.1.0

The first release of Flag, to try the language. It is not ready for production yet.

- `flagc` for Linux x86_64, macOS arm64 and Windows x86_64, a single file with the standard library inside
- The standard library: core, `fs`, `os`, `json`, `net`, `math`, `utf8`, `bits`, `ring` and `raylib` (raylib only for Linux x86_64)
- `-memory`, the memory checker at compile time
- The docs: [Overview](https://github.com/FramaLuna/flag-lang/blob/v0.1.0/docs/Overview.md), [Reference](https://github.com/FramaLuna/flag-lang/blob/v0.1.0/docs/Reference.md) and [Library](https://github.com/FramaLuna/flag-lang/blob/v0.1.0/docs/Library.md)

What may surprise you is in [Rough edges](https://github.com/FramaLuna/flag-lang/blob/v0.1.0/docs/Reference.md#rough-edges).
