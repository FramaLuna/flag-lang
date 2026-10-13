# Changelog

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
