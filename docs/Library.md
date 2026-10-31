Every function of the standard library, module by module.

`core` is always there, without `use`. The other modules come with `use name`, flagc finds them in its library and you don't add any file.

- [How to read this](#how-to-read-this)
- [core](#core)
- [fs](#fs), whole files, folders and paths
- [os](#os), the program, other programs and the time
- [json](#json), reading and writing JSON
- [net](#net), TCP connections
- [math](#math)
- [utf8](#utf8)
- [bits](#bits), sets of numbers as bits
- [ring](#ring), a queue you can grow at both ends
- [raylib](#raylib), graphics, input and audio

## How to read this
Each function is written with its parameters and what it returns, and `$I` is any integer type, the alias `Integer` of core that your own `where` can use too: `where $I is Integer`. Functions with the same name and different parameters are overloads, the call takes the one that fits.

What a function gives you is
- **yours** when you own it and release it with `free`
- **temporary** when it lives in the scratch memory, until the `with scratch():` around the call ends (see [Temporary memory](Reference.md#temporary-memory)). Copy it to a `&str` to keep it
- **a view** when it points inside what you passed, so it lives as long as that

A function that can fail gives its errors in a union, and each module says which error types it has.

The functions that take an index or a `File` also have a last parameter `place: &strview = @here()` that is left out here, it's how an index error or a wrong `File` points at your call.

## core
### The program
- `print(values...)` writes the values one after the other to the standard output, and then a new line. It computes all the values first, like the arguments of a call, formats them into one line, and writes that line with `write(stdout, line)`, so the lines of two threads never mix
- `eprint(values...)` the same to the error output
- `panic(message: &strview)` writes `flag: <message>` and the place of the call to the error output, and ends the program with exit code 134
- `assert(condition: bool, message: &strview = "assertion failed")` panics with the message when the condition is false
- `exit(code: i32)` ends the program with that exit code. The deferred statements don't run, what waits in the files is written

The arguments of the program are `args()` of [os](#os).

### Files
A `File` is an open file. `stdin`, `stdout` and `stderr`, the standard input, output and error, are always open.

What you write waits in the file and goes to the system in big pieces, so many small writes are fast. It goes out
- when 64 KiB are waiting, when you call `flush` or `close`, before the program waits to read from any file, before another program runs, and when the program ends: returning from `main`, `exit`, `panic` and `assert` all write it
- at the end of each line, when the file is a terminal or the error output, so you see every line when it's printed

`stdout` and `stderr` keep the order of your calls, and both go out before the standard input is read. A crash or a signal, like Ctrl-C, loses what waits, so when you look for a crash use `eprint` or `flush`.

The functions
- `open(path: &strview) -> File|FsError` opens the file to read
- `create(path: &strview, append: bool = false) -> File|FsError` makes the file, or empties it, to write. With `append = true` it keeps what the file has and everything you write goes at its end
- `write(f: File, value: $T)` writes the value like `print` does, without the new line: `write(stdout, "name? ")`. Bytes go as text, `write(f, view(bytes))`. The value is formatted first, in the temporary memory of the thread, and the file takes it whole, so two threads that write to one file never mix their values
- `read_line(f: File) -> &strview|null|FsError` the next line without the `\n` or `\r\n`, temporary, `null` at the end of the file. Any other byte stays in the line, a zero byte too. Every line is a new temporary, so a loop that reads many lines goes inside `with scratch():`, like the example below
- `read(f: File, into: *&str, size: usize) -> usize|FsError` appends up to `size` bytes to `into` and gives how many, 0 at the end of the file
- `flush(f: File) -> FsError?` writes now what is waiting, and gives the first write that failed since the last `flush`. A failed write doesn't stop the next ones, a write that the system cannot take yet waits until it can, so when you need to know that everything got there, end with `try flush(out)` or check what it gives
- `flush()` writes now what waits in every file
- `close(f: File)` writes what is waiting and closes the file, and `with file:` closes it for you. It doesn't tell about a failed write, `flush` does
- `File{fd}` is the file of a descriptor that you got from C. A negative one stops the program like an index out of bounds: `flag: File{-1} is not an open file`

The standard output is checked for you: when what you printed could not be written and no `flush` told you, the program ends with `flag: cannot write the standard output: No space left on device` and the exit code 1. The standard input reads ahead, so a program that you run after reading it doesn't get what the file already read. The files that the library opens are not passed to the programs you run.

```flag
func main():
    out := create("squares.txt")
    if out is FsError:
        eprint(out)
        return
    for i in 1..4:
        write(out, i * i)
        write(out, "\n")
    failed := flush(out)
    if failed is FsError:
        eprint(failed) # cannot write squares.txt: No space left on device
    close(out)
    input := open("squares.txt")
    if input is FsError:
        eprint(input)
        return
    with input:
        while true:
            with scratch(): # each line is temporary, this releases it before the next one
                line := read_line(input)
                switch line:
                case null:
                    break
                case FsError:
                    eprint(line)
                    break
                case &strview:
                    print("read ", line)
    write(stdout, "your name? ")
    name := read_line(stdin)
    if name is &strview:
        print("hello ", name)
```

A function that fails because the system said no, in core, `fs` and `os`, gives an `FsError`
```flag
type error FsError:
    code: i32 # the error number of the system, errno
    action: &strview # what failed: open, create, read, write, list, make, remove, run...
    path: &strview # the path, the program or the address you gave
```
Printing it says all of it, with the words of the system for the code: `cannot open notes.txt: No such file or directory`. The `path` of a failed `read`, `read_line` or `flush` is a temporary copy of the path the file was opened with, and for the standard files it's `the standard input`, `the standard output` or `the error output`.

A negative `code` is an error that has no errno: on Windows the errors of `rename` are the codes of the system.
- `errno() -> i32` the error number of the last C call that failed. After your own `extern` call, `eprint(FsError{errno(), "lock", path})` says what happened like the library does

### Threads
`parallel f(x)`, `wait t` and `Thread[T]` are the language (see [Threads](Reference.md#threads)), these are the library
- `done(t: Thread[$T]) -> bool` true when the thread ended, without waiting for it. `wait t` after it gives the result at once
- `cores() -> usize` how many processors the system has online, to split a job in as many threads
- `Lock` lets one thread at a time into a part of the code. It works at zero, so declare it next to what it guards, and take it with `with lock(x.lock):`. The thread that holds it can take it again, inside, without waiting for itself
  - `lock(l: *Lock) -> Locked` waits until no other thread holds the lock, and takes it
  - `close(held: Locked)` lets it go, `with` calls it
- `Channel[$T, $$size]` a queue of up to `size` values between threads. It keeps them inside itself, like a fixed map, so it doesn't allocate and works at zero: declare it where it lives and pass it where a `*Channel` goes
  - `send(c: *Channel[$T, $$size], v: $T)` puts the value at the end, and waits while the channel is full. Sending on a closed channel stops the program: `flag: send on a closed channel`
  - `receive(c: *Channel[$T, $$size]) -> $T|null` takes the first value, and waits while the channel is empty. It gives `null` when the channel is closed and empty
  - `close(c: *Channel[$T, $$size])` closes it: what it holds can still be received, and after that `receive` gives `null`
  - `for v in c` receives until the channel is closed and empty

The files of core, `print` included, work from any thread.

```flag
type Counter:
    lock: Lock
    count: i64

counter: Counter

func add(times: i32):
    for i in 0..times:
        with lock(counter.lock):
            counter.count += 1

func square_all(jobs: *Channel[i32, 8], results: *Channel[i32, 8]):
    for n in jobs:
        send(results, n * n)
    close(results)

func main():
    workers: &list[Thread[null]]
    defer free(workers)
    for k in 0..cores():
        append(workers, parallel add(1000))
    for w in workers:
        wait w
    print(counter.count) # 1000 for each core

    jobs: Channel[i32, 8]
    results: Channel[i32, 8]
    t := parallel square_all(jobs, results)
    for n in 1..4:
        send(jobs, n)
    close(jobs)
    for r in results:
        print(r) # 1, 4 and 9
    wait t
```

### Memory
- `heap(v: $T) -> *$T` copies the value to the heap, yours
- `free(p: *$T)` releases what `heap` gave. A pointer to a text, a list or a map frees what it points to, like the other functions of lists, so write `free(p as *bytes)` for the pointer itself
- `ptr(x)` the address of a variable, a field or an element, and of an `export func` (see [Calling C](Overview.md#calling-c))
- `sizeof(T)` the size of a type in bytes, as an integer literal
- `malloc`, `calloc`, `realloc`, `free(at: *bytes)`, `memcpy`, `memmove`, `memcmp`, `memchr`, `strlen`, `snprintf` and `strtod` are the C functions, with `*bytes` for `void *` and `cstr` for `char *`
- `out_of_memory()` writes `flag: out of memory` to the error output and ends the program with exit code 9. The library calls it when the system gives no more memory, and you can call it after your own `malloc`

### Temporary memory
- `scratch() -> Scratch` marks the scratch memory, and `close` of the mark releases every temporary made after it. `with scratch():` does both
- `temp(size: usize) -> *bytes` that many bytes of temporary memory
- `temp(text: &strview) -> &strview` a temporary copy of the text, with a zero byte after it
- `temp(items: &slice[$T]) -> &slice[$T]` a temporary copy of the elements, to return what you built in a list that you free

### Lists and slices
A `&slice[$T]` is a view of elements, and a `&list[$T]` is a list that you own and that grows. Both start empty, and a list goes wherever a slice is asked.
- `s[i]` the element `i`, you can also assign to it. A negative `i` counts from the end, and out of bounds stops the program
- `s[a:b]` the elements from `a` to `b - 1`, a view. `a` and `b` can be negative like an index, and the slice must be inside the elements, with `a` not after `b`, or the program stops. `s[a:]` goes to the end and `s[:b]` starts at 0
- `for x in s`, `for i, x in s` and `for x in reverse s`
- `==` compares element by element, and `print` writes `[1, 2, 3]`

On lists
- `append(l: *&list[$T], v: $T)` adds `v` at the end. The first time it makes room for 8 elements, and then it doubles
- `append(l: *&list[$T], items: &slice[$T])` adds all the elements at the end, also from the list itself
- `reserve(l: *&list[$T], want: usize)` makes room for `want` elements, so the appends until then don't allocate
- `insert(l: *&list[$T], at: $I, v: $T)` puts `v` at the index `at` and moves the rest one place forward, `at` can be `l.len`
- `pop(l: *&list[$T]) -> $T|null` takes out the last element, `null` when the list is empty
- `remove(l: *&list[$T], at: $I) -> $T` takes out the element at `at` and moves the rest one place back
- `clear(l: *&list[$T])` removes every element and keeps the memory
- `free(l: *&list[$T])` releases the list and leaves it empty. The elements are not released, free them first when they own memory

On slices, so also on lists
- `find(items: &slice[$T], v: $T) -> usize|null` the index of the first element `== v`
- `find(items: &slice[$T], fits: func($T) -> bool) -> usize|null` the index of the first element that `fits` says yes to
- `contains(items: &slice[$T], v: $T) -> bool`
- `binary_search(items: &slice[$T], v: $T) -> usize|null` the index of `v` in a sorted slice, using `<`
- `sort(s: &slice[$T])` sorts in place using `<`, in O(n log n). It's a merge sort, so equal elements keep their order, and it allocates room for half the slice while it sorts
- `sort(s: &slice[$T], before: func($T, $T) -> bool)` sorts with your order, `before(a, b)` says if `a` goes before `b`
- `reverse(items: &slice[$T])` turns the elements around in place
- `swap(s: &slice[$T], i: $I, j: $I)` swaps two elements, the indexes can be negative like in `s[i]`

An array goes to these functions by its address: `sort(ptr(values))`.

### Arrays
Two arrays of the same type work with the operators element by element, and give a new array
- `+`, `-`, `*` and `/` for numbers, and `%`, `and`, `or`, `xor`, `<<` and `>>` for integers: `[1, 2] + [10, 20]` is `[11, 22]`
- `a == b` and `a != b` compare every element

### Text
A `&strview` is a view of text and a `&str` is text that you own and that grows. A text literal is a `&strview`, and a `&str` goes wherever a `&strview` is asked. Text is bytes, and the library reads it as UTF-8.
- `s[i] -> u8` the byte `i`, a negative `i` counts from the end
- `s[a:b] -> &strview` the bytes from `a` to `b - 1`, a view. `s[a:]` goes to the end and `s[:b]` starts at 0
- `for c in s` gives each UTF-8 character as a `&strview`, also with `reverse`. A byte that is not part of a valid character comes alone, so text that is not UTF-8 gives its wrong bytes one by one, also the bytes of an overlong form, a surrogate or a code above U+10FFFF. Both directions give the same pieces, and `count`, `valid` and `code` of `utf8` read the same ones
- `==` and `<` compare the bytes
- `"a" + x` joins the text with anything that can be printed, temporary

Building a `&str`
- `s += x` appends anything that can be printed
- `s = text` with a `&strview` copies the text into `s`, reusing its memory. Between two `&str` it doesn't copy, both share the text, so write `s = view(other)`
- `add_byte(s: *&str, b: u8)` appends one byte
- `add_char(s: *&str, code: u32)` appends the character with that code, as UTF-8
- `reserve(s: *&str, want: usize)` makes room for `want` bytes, so the appends until then don't allocate
- `view(s: &str) -> &strview` a view of the text
- `free(s: *&str)` releases the text and leaves it empty

Searching and cutting
- `find(s: &strview, part: &strview) -> usize|null` the byte index of the first `part`
- `find_last(s: &strview, part: &strview) -> usize|null` the byte index of the last `part`
- `contains(s: &strview, part: &strview) -> bool`
- `starts_with(s: &strview, head: &strview) -> bool` and `ends_with(s: &strview, tail: &strview) -> bool`
- `trim(s: &strview) -> &strview` without the spaces, tabs, `\n` and `\r` at both ends, a view. `trim_start` and `trim_end` do one end
- `split(s: &strview, separator: &strview) -> &slice[&strview]` the parts between the separators, as views of `s` in a temporary slice. There is always one part at least, and an empty separator gives `s` whole
- `words(s: &strview) -> &slice[&strview]` the parts between spaces, tabs, `\n` and `\r`, as views of `s` in a temporary slice, never an empty one: `for w in words(text): counts[w] += 1`
- `replace(s: &strview, old: &strview, new: &strview) -> &strview` every `old` changed to `new`, temporary
- `join(parts: &slice[$T], separator: &strview) -> &strview` the parts with the separator between them, temporary. The parts can be anything that can be printed
- `lower(s: &strview) -> &strview` and `upper(s: &strview) -> &strview` change the ASCII letters, temporary
- `items(s: &strview) -> &slice[u8]` the bytes, a view
- `view(bytes: &slice[u8]) -> &strview` the bytes as text, a view

Numbers from text, `null` when the text is not a number
- `parse_int(s: &strview) -> i64|null` decimal digits with an optional `+` or `-`, and `null` when it doesn't fit in `i64`
- `parse_float(s: &strview) -> f64|null` a decimal number with an optional sign, point and exponent, `inf` or `nan`: `1.5`, `-2e3`, `.5`, `-inf`
- `parse_hex(s: &strview) -> u64|null` 1 to 16 hexadecimal digits, without `0x`

None of them skip spaces, the whole text must be the number.

### Printing and formatting
`print`, `eprint`, `+` and `+=` on text format each value with `operator +=(s: *&str, x: T)`, and the library has one for
- integers, in decimal, and `bool`, as `true` or `false`
- floats, with the fewest digits that read back as the same number (`0.1`, `2.0`), and with an exponent when they are very big or very small (`1e+16`, `1e-05`). The ones that are not numbers are `inf`, `-inf` and `nan`
- pointers, in hexadecimal
- slices and lists `[1, 2]`, maps and fixed maps `{a: 1, b: 2}`

And three helpers
- `fixed(value: f64, places: i32) -> Fixed`, also for `f32`, prints the value with that many decimals, and the halves round to even: `fixed(2.5, 0)` is `2`. A value that rounds to zero has no sign, `fixed(-0.4, 0)` is `0`. Negative `places` are 0
- `hex(value: $I) -> Hex` prints the bits of the integer in hexadecimal, as many as its type has: `hex(255)` is `0xff`, and `hex(i8(-1))` is `0xff` too
- `pad(value: $T, width: i32) -> Padded[$T]` prints the value with spaces before it to fill `width` characters, or after it when `width` is negative: `pad(42, 5)` is `   42` and `pad("ab", -4)` is `ab  `. A longer value is not cut. It takes any value, so `pad(fixed(x, 2), 8)` works

The compiler prints the rest: structs as `Point{x = 1, y = 2}`, arrays as `[1, 2]`, a union as the value it holds and a void type as its name. Declare `operator +=` to print your type another way (see [Printing](Overview.md#printing)).

### Maps
A `&map[$K, $V]` is a hash map that you own. It starts empty, and `{"a": 1}` makes one.
- `m[k] -> $V|null` the value of the key, `null` when it isn't there, and `m[k] = v` is `put`. `m[k]` calls `at(m: &map[$K, $V], key: $K) -> $V|null`
- `put(m: *&map[$K, $V], key: $K, value: $V)` adds the key, or changes its value
- `m[k] += v`, `m[k].field = v` and `m[k][j] = v` change the value inside the map and find the key once, and a key that wasn't there is added with a zero value: `counts[word] += 1`, `players[name].score += 10`, `pairs[a][b] += 1`. `m[k]` passed where a pointer goes is copied, zero when the key is missing, and put back after the call: `append(groups[k], v)` adds to a list kept in a map
- `contains(m: &map[$K, $V], key: $K) -> bool`
- `remove(m: *&map[$K, $V], key: $K) -> $V|null` takes out the key and gives its value, `null` when it wasn't there
- `clear(m: *&map[$K, $V])` removes every key and keeps the memory
- `free(m: *&map[$K, $V])` releases the map and leaves it empty. The keys and values are not released, free them first when they own memory
- `for e in m` gives each entry, an `entry[$K, $V]` with `e.key` and `e.value`, in the order of the table
- `hash(key) -> u64` the hash of a key. There is one for integers, one for floats (`0.0` and `-0.0` are the same key) and one for text, and the compiler makes the others (see [Operators](Reference.md#operators))

A key needs `==` and `hash`. Numbers, text, `bool`, enums, pointers, arrays, and structs and unions of them have both. A struct with your own `==` needs your `hash` too, so that two keys that are `==` give the same hash, and using it as a key without one is an error: `Key declares == but not hash, and equal values need equal hashes: declare hash for Key too`.

A float `nan` is not `==` to itself, so as a key it's never found again, and every `put` of it adds one more entry.

### Fixed maps
A `fixmap[$K, $V, $$max]` keeps up to `max` entries inside itself, in the order they were put, so it doesn't allocate and you don't free it. Looking for a key reads the entries one by one, so it's for small maps.
- `put(m: *fixmap[$K, $V, $$max], key: $K, value: $V) -> MapFull?` adds the key or changes its value, and gives the error `MapFull` when there is no room for a new key. An assignment cannot pass that error on, so `m[k] = v` doesn't compile on a fixed map: write `try put(m, k, v)`
- `m[k]`, `contains` and `clear` like a map
- `remove` like a map, and the entries after it keep their order
- `for e in m` gives the entries in the order they were put

### Indexes of your own types
`x[i]` on your type calls your `at` (see [Indexing](Overview.md#indexing)). The library makes its own `at` with these two, so yours can take the same indexes and stop the program the same way
- `natural(i: $I, len: usize) -> usize` the index counted from the start, so a negative `i` becomes `len + i`. When `i` goes before the start the result is past `len`, so checking `n >= len` catches both ends
- `out_of_bounds(i: $I, len: usize)` stops the program with `flag: index 5 is out of bounds, length 3`, and `out_of_bounds(from: $I, to: $I, len: usize)` with `flag: slice 2:9 is out of bounds, length 3`
```flag
type Grid:
    cells: array[i32, 9]

func at(g: *Grid, i: $I, place: &strview = @here()) -> *i32 where $I is i32|usize:
    n := natural(i, 9)
    if n >= 9:
        out_of_bounds(i, 9, place)
    return ptr(g.cells[n])
```

### C text
- `cstr` is the type `*u8`, a text that ends with a zero byte
- `cstr(text: &strview) -> cstr` a copy with the zero, temporary
- `c_text(s: &str) -> cstr` writes a zero after the text of `s` and gives its address, without copying, so it's valid until `s` changes. When `s` has no room for the zero it's a temporary copy. It's what the compiler calls when a `&str` goes as a `cstr`
- `view(c: cstr) -> &strview` a view of a C text, empty for a null pointer

## fs
Whole files, folders and paths. The files read by parts, `File` and `open`, are in [core](#files), and the errors are the `FsError` of core.

The files are read and written in binary, the bytes are the same on every system. On Windows the paths can use `/` or `\`.

### Whole files
- `read_file(path: &strview) -> &str|FsError` all the file, yours
- `write_file(path: &strview, text: &strview) -> FsError?` makes the file, or empties it, and writes the text. Bytes go as text, `write_file(path, view(bytes))`
- `append_file(path: &strview, text: &strview) -> FsError?` the same, but it writes at the end of the file and makes it when it doesn't exist

### Folders and paths
- `exists(path: &strview) -> bool` if there is a file or a folder there
- `is_dir(path: &strview) -> bool`, also for a folder that you cannot read
- `file_size(path: &strview) -> usize|FsError` in bytes, without opening the file, so it works on one that you cannot read. A folder has no size, it's an error
- `list_dir(path: &strview) -> &slice[&strview]|FsError` the names inside the folder, sorted and without `.` and `..`, temporary
- `walk(folder: &strview) -> &slice[&strview]|FsError` the paths of every file inside the folder and the folders inside it, sorted, temporary. A link to a folder is one more path, `walk` doesn't go into it, and on Windows a junction neither. Only a `folder` that cannot be listed is an error, the folders inside it that cannot are skipped
- `make_dir(path: &strview) -> FsError?` makes the folder and the ones above it, and a folder that already exists is not an error
- `remove(path: &strview) -> FsError?` removes a file or an empty folder, and of a link the link
- `rename(from: &strview, to: &strview) -> FsError?` renames or moves, and replaces `to` when it's a file, on every system
- `join_path(first: &strview, second: &strview) -> &strview` the two paths with one `/` between them, temporary: `join_path("a/", "/b")` is `a/b`. An empty `first` gives `second`
- `base_name(path: &strview) -> &strview` the part after the last `/`, a view: `base_name("docs/a.txt")` is `a.txt`
- `extension(path: &strview) -> &strview` the name from its last `.`, a view: `.txt`. It's empty when there is no `.`, and the `.` that a name starts with don't count, so `.bashrc` and `..` have no extension and `.config.json` has `.json`
- `dir_of(path: &strview) -> &strview` the part up to the last `/`, with it, a view: `docs/`. It's `./` when there is no folder

## os
The program, other programs and the time.

### The program
- `args() -> &slice[&strview]` the arguments of the program, the first one is the program itself. They live all the program
- `env(name: &strview) -> &strview|null` the value of an environment variable, a view, `null` when it's not set
- `executable() -> &strview` the full path of the running program, temporary, empty when the system cannot tell
- `current_dir() -> &strview` the current folder, temporary, empty when the system cannot tell
- `change_dir(path: &strview) -> FsError?` makes `path` the current folder
- `system_name() -> &strview` `Linux`, `Darwin` or `Windows`

The standard input is `read_line(stdin)` of [core](#files).

On every system the paths that `os` gives use `/`.

### Other programs
A program that cannot start is an `FsError` with the action `run`: `cannot run gcc: No such file or directory`.
- `run(command: &strview) -> i32|FsError` runs the command with the shell (`sh`, or `cmd.exe` on Windows) and gives its exit code. A program that the shell cannot find is the exit code of the shell, 127 with `sh`
- `run(program: &slice[&strview]) -> i32|FsError` runs `program[0]` with the other elements as its arguments, without a shell, so each argument gets there exactly as it is. The program is found in `PATH`, and an empty slice cannot run. On Windows a name without an extension is a `.exe`, and a `.bat` or `.cmd` is an `FsError`, because `cmd.exe` would read its arguments again: a batch file goes with `run(command)`
- `output(command: &strview, into: *&str) -> i32|FsError` runs the command with the shell, appends what it wrote to the standard output to `into`, with its line ends, and gives its exit code
- `output(program: &slice[&strview], into: *&str) -> i32|FsError` the same without a shell

```flag
use os

func main():
    text: &str
    defer free(text)
    code := output(ptr(["git", "status", "--short"]), text)
    if code is FsError:
        eprint(code)
    elif code == 0:
        print(text)
```
Everything that waits in your files goes out before another program runs. On Linux and macOS a program killed by a signal gives 128 plus the signal.

### Time
- `now() -> f64` the seconds since 1970, with decimals
- `clock() -> f64` the seconds from some start, it never goes back, so it's the one to measure how long something takes
- `date() -> Date` the local date and time
- `sleep(seconds: f64)` stops the program that long, a signal that arrives doesn't cut it short

`Date` has `year`, `month` (1 to 12), `day` (1 to 31), `hour`, `minute`, `second` and `weekday` (0 is Sunday), all of them `i32`.

## json
Reads JSON, and writes its strings.
```flag
use json

func main():
    d := parse("{\"user\": {\"name\": \"ana\", \"tags\": [\"a\", \"b\"]}}")
    if d is NotJson:
        eprint(d)
        return
    defer free(d)
    print(text(d, get(d, 0, "user.name"))) # ana
    for tag in children(d, get(d, 0, "user.tags")):
        print(text(d, tag))
    print(integer(d, get(d, 0, "user.age")) ifnull 0) # 0, there is no age
```
`parse` doesn't copy the text, a `Document` is a list of nodes over it, so the text must live while you use the document. A node is its index in that list, the first value of the text is the node 0, and the functions take the node as `usize|null`. A missing node is `null`, and for it the functions give `null` or empty, so you can chain them and check only at the end.
- `parse(text: &strview) -> Document|NotJson` takes exactly the JSON of RFC 8259, anything else is `NotJson`: a number like `01` or `1.`, an escape that JSON doesn't have, a line end inside a string, a `\ud83d` without its pair, a comma before a `]`. A value inside more than 512 arrays and objects is `NotJson` too. The bytes of a string are not checked to be UTF-8, `valid` of `utf8` does that
- `free(d: *Document)` releases the nodes, not the text, and leaves the document empty
- `get(d: Document, at: usize|null, path: &strview) -> usize|null` the value of a key of the object `at`, and `.` goes into the objects inside it: `"user.name"`. The keys are compared with their escapes decoded. A key that has a `.` inside cannot be reached this way
- `children(d: Document, at: usize|null)` for a `for`: the values of an array or an object, in order, and nothing for anything else
- `key(d: Document, at: usize|null) -> &strview` the key of the value `at` when it's inside an object, temporary, and empty when it's not
- `count(d: Document, at: usize|null) -> usize` how many values an array or an object has
- `item(d: Document, at: usize|null, index: usize) -> usize|null` the value at `index` of an array or an object. It walks from the start, so to go through all of them use `children`
- `kind(d: Document, at: usize|null) -> Kind` what the value is, one of `Nothing`, `Boolean`, `Number`, `Text`, `Array` and `Object`. `null` and a missing node are `Nothing`
- `text(d: Document, at: usize|null) -> &strview|null` a string with its escapes decoded, temporary
- `number(d: Document, at: usize|null) -> f64|null`, `integer(d: Document, at: usize|null) -> i64|null` and `boolean(d: Document, at: usize|null) -> bool|null`
- `text`, `number`, `integer` and `boolean` give `null` when the value is not of that kind, so `text(d, get(d, 0, "name")) ifnull "anonymous"` works. `integer` is also `null` for a number with decimals or an exponent, or one that doesn't fit in `i64`
- `raw(d: Document, at: usize|null) -> &strview` the JSON text of the value, a view
- `quoted(text: &strview) -> Quoted` the text as a JSON string, with the quotes and the escapes, to use with `+`, `+=` and `print`

The keys of an object go with `key`
```flag
for v in children(d, get(d, 0, "user")):
    print(key(d, v), " is ", raw(d, v))
```
To write JSON you build the text with `+=`, and `quoted` for the strings. The numbers that `+=` writes are JSON, except `nan` and `inf`, that JSON doesn't have
```flag
out += "{\"name\": " + quoted(name) + ", \"age\": " + age + "}"
```
The error says where and what
```flag
type error NotJson:
    reason: &strview
    line: usize
    column: usize
```
`reason` says what is wrong, `line` and `column` start at 1 and the column counts bytes, like in the errors of the compiler. It prints as `not JSON at line 3, column 14: expected ':' after the key`.

A `Document` has `text`, the text you gave, and `nodes`, a `&list[Node]`. A `Node` has its `kind`, `from` and `to`, so that `d.text[from:to]` is its JSON text, `size`, how many nodes it takes with the ones inside it, and `escaped`, if it's a `Text` with escapes to decode. An object keeps each key as a `Text` node right before its value.

## net
TCP connections. A `Socket` is one connection and a `Server` waits for them, each a type of its own, so a file never goes where a connection goes.
- `connect(host: &strview, port: u16) -> Socket|NetError` connects to a name or an address, it tries each address of the name until one answers: `cannot connect to example.com:80: Connection refused`
- `listen(port: u16) -> Server|NetError` a server on that port, on every IPv4 address of the machine. With the port 0 the system chooses a free one, and `port` tells you which
- `accept(server: Server) -> Socket|NetError` waits for the next connection
- `port(server: Server) -> u16` the port the server listens on
- `close(server: Server)` stops listening
- `timeout(s: Socket, seconds: f64)` makes a read or a write that waits longer than that give a `NetError` with the code `TIMED_OUT`. 0 waits forever, like a new connection
- `write(s: Socket, value: $T)`, `read(s: Socket, into: *&str, size: usize) -> usize|NetError`, `read_line(s: Socket) -> &strview|null|NetError`, `flush(s: Socket) -> NetError?` and `close(s: Socket)` work like the ones of a [file](#files)

```flag
type error NetError:
    code: i32 # the error number of the system, errno
    action: &strview # what failed: connect to, listen on port, accept a connection, read, write
    address: &strview # the host and port of connect, the port of listen, empty for the rest
```
It prints like an `FsError`. A name that `connect` cannot find gives the negative code of `getaddrinfo`, and on Windows the codes are the negative ones of Winsock. `TIMED_OUT`, of core, is the code of a connection that waited longer than its `timeout`, the same on every system, and it prints as `cannot read: the timeout ran out`.

What you write waits in the connection like in a file, and it goes out with `flush`, or by itself before the program waits to read, so a request always goes before you wait for its answer
```flag
use net

func main():
    c := connect("example.com", 80)
    if c is NetError:
        eprint(c)
        return
    with c:
        write(c, "HEAD / HTTP/1.0\r\nHost: example.com\r\n\r\n")
        print(read_line(c)) # HTTP/1.0 200 OK
```
Writing to a connection that the other side closed gives a `NetError`, it doesn't stop the program. On Windows the module uses Winsock and starts it for you.

## math
It links the C math library.
- `PI`, `TAU` and `E`
- `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `log2`, `log10`, `floor`, `ceil`, `round` and `trunc` take one float, and `atan2(y, x)`, `pow(x, y)`, `hypot(x, y)` and `fmod(x, y)` take two. They are the C functions for `f64`, and each one has an `f32` version that gives an `f32`. The angles are in radians, `log` is the natural one and `round` takes the halves away from zero
- `abs(x: $T) -> $T` for signed integers and floats. The smallest integer of a type has no positive, so `abs(i8(-128))` is still -128
- `min(a: $T, b: $T) -> $T`, `max(a: $T, b: $T) -> $T` and `clamp(x: $T, low: $T, high: $T) -> $T`, for anything with `<`
- `lerp(a: $T, b: $T, t: $T) -> $T` for floats, `a` when `t` is 0 and `b` when it's 1

### Random numbers
They are the same sequence on every run until you call `seed`, and they are not for secrets.
- `seed(n: u64)` starts the sequence from `n`. For a different one each run use the time: `seed(u64(now() * 1000000.0))`, with `now` from `os`
- `random() -> f64` from 0 to 1, without the 1
- `random(low: i64, high: i64) -> i64` from `low` to `high`, both of them included, in either order

## utf8
- `code(s: &strview) -> u32|null` the code of the first character, `null` when the text is empty or doesn't start with valid UTF-8: `code("é")` is 233
- `count(s: &strview) -> usize` how many characters, the same that `for c in s` counts
- `valid(s: &strview) -> bool` if all the text is valid UTF-8, that is, no byte of it comes alone in `for c in s`

To walk the characters use `for c in text`, and to add a character from its code, `add_char` of core.

## bits
Sets of numbers, one bit for each number.
```flag
use bits

func main():
    seen: Bits[2] # room for 128 numbers, from 0 to 127
    seen[3] = true
    seen[70] = true
    for n in seen:
        print(n)
```
`Bits[$$words]` holds the numbers from 0 to `words * 64 - 1`, in `words` 64 bit words.
- `b[i] -> bool` if `i` is in the set, and `b[i] = true` or `false` adds or removes it. `i` is any integer, and one below 0 or past the room stops the program
- `a or b` the union, `a and b` the intersection, `a - b` the numbers of `a` that are not in `b`, and `a xor b` the ones in only one of them
- `a == b` and `a != b`
- `complement(b)` the numbers that are not in `b`
- `empty(b) -> bool` and `count(b) -> usize`
- `for n in b` gives the numbers from the smallest, and `print` writes `{3, 70}`

## ring
A `&ring[$T]` is a queue that grows as you add elements at either end, and takes them out of either end without moving the rest.
```flag
use ring

func main():
    q: &ring[i32]
    defer free(q)
    append(q, 1)
    append(q, 2)
    prepend(q, 0)
    print(take(q), " ", q) # 0 [1, 2]
```
- `append(r, v)` adds at the end, and `prepend(r, v)` at the start
- `pop(r) -> $T|null` takes out the last element, and `take(r) -> $T|null` the first
- `clear(r)` removes every element and keeps the memory
- `r[i]` the element `i` from the start, a negative `i` counts from the end
- `for v in r` from the start, `print` writes `[1, 2]`, and `free(r)`

## raylib
Windows, drawing in 2D and 3D, input, textures and audio with [raylib](https://www.raylib.com). The library comes with raylib built for Linux on x86_64, for other systems put your own `libraylib.a` in the `lib` folder.

The functions keep the names and the parameters of raylib, so its [cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html) explains them, and the text goes as `cstr`.
- Window: `InitWindow`, `CloseWindow`, `WindowShouldClose`, `SetConfigFlags`, `SetTargetFPS`, `SetWindowTitle`, `ToggleFullscreen`, `GetScreenWidth`, `GetScreenHeight`, `GetFrameTime`, `GetTime`, `GetFPS`, `GetRandomValue`, `TakeScreenshot`
- Drawing: `BeginDrawing`, `EndDrawing`, `ClearBackground`, `BeginMode2D`, `EndMode2D`, `BeginMode3D`, `EndMode3D`
- Keys and mouse: `IsKeyPressed`, `IsKeyDown`, `IsKeyReleased`, `GetKeyPressed`, `GetCharPressed`, `IsMouseButtonPressed`, `IsMouseButtonDown`, `IsMouseButtonReleased`, `GetMousePosition`, `GetMouseDelta`, `GetMouseWheelMove`
- Shapes: `DrawPixel`, `DrawLine`, `DrawLineV`, `DrawLineEx`, `DrawCircle`, `DrawCircleV`, `DrawCircleLines`, `DrawRectangle`, `DrawRectangleV`, `DrawRectangleRec`, `DrawRectangleLines`, `DrawRectangleLinesEx`, `DrawRectangleRounded`, `DrawTriangle`
- Text: `DrawText`, `MeasureText`, `DrawFPS`
- 3D: `DrawCube`, `DrawCubeWires`, `DrawSphere`, `DrawPlane`, `DrawGrid`
- Images and textures: `LoadImage`, `GenImageColor`, `UnloadImage`, `LoadTexture`, `LoadTextureFromImage`, `UnloadTexture`, `DrawTexture`, `DrawTextureV`, `DrawTextureEx`, `DrawTextureRec`, `DrawTexturePro`
- Collisions: `CheckCollisionRecs`, `CheckCollisionCircles`, `CheckCollisionCircleRec`, `CheckCollisionPointRec`, `CheckCollisionPointCircle`, `GetCollisionRec`
- Colors: `Fade`, `ColorFromHSV`
- Audio: `InitAudioDevice`, `CloseAudioDevice`, `LoadSound`, `UnloadSound`, `PlaySound`, `StopSound`, `SetSoundVolume`, `LoadMusicStream`, `UnloadMusicStream`, `PlayMusicStream`, `StopMusicStream`, `UpdateMusicStream`

The types are `Vector2`, `Vector3`, `Color`, `Rectangle`, `Image`, `Texture`, `Camera2D`, `Camera3D`, `AudioStream`, `Sound` and `Music`, with the fields of raylib written in `snake_case`.

The constants
- the colors: `LIGHTGRAY`, `GRAY`, `DARKGRAY`, `YELLOW`, `GOLD`, `ORANGE`, `PINK`, `RED`, `MAROON`, `GREEN`, `LIME`, `DARKGREEN`, `SKYBLUE`, `BLUE`, `DARKBLUE`, `PURPLE`, `VIOLET`, `DARKPURPLE`, `BEIGE`, `BROWN`, `DARKBROWN`, `WHITE`, `BLACK`, `BLANK`, `MAGENTA` and `RAYWHITE`
- the keys: `KEY_A` to `KEY_Z`, `KEY_SPACE`, `KEY_ESCAPE`, `KEY_ENTER`, `KEY_TAB`, `KEY_BACKSPACE`, `KEY_RIGHT`, `KEY_LEFT`, `KEY_DOWN`, `KEY_UP`, `KEY_LEFT_SHIFT` and `KEY_LEFT_CONTROL`
- the mouse buttons: `MOUSE_BUTTON_LEFT`, `MOUSE_BUTTON_RIGHT` and `MOUSE_BUTTON_MIDDLE`
- the flags of `SetConfigFlags`: `FLAG_FULLSCREEN_MODE`, `FLAG_WINDOW_RESIZABLE`, `FLAG_MSAA_4X_HINT` and `FLAG_VSYNC_HINT`

And what Flag adds: `+` and `-` between two `Vector2` or two `Vector3`, `*` of a vector by an `f32`, `length(v: Vector2) -> f32` and `normalize(v: Vector2) -> Vector2`.

A function of raylib that is not here you declare yourself, in an `extern "raylib":` block.
