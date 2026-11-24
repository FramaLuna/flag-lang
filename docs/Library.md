# Standard Library

Every function of the standard library, module by module.

`core` is always available without `use`. Every other module is loaded with `use name`; `flagc` finds it in its built-in library, so you do not pass any extra file. If your program declares a module with the same name as a standard module, your files see yours while the standard library keeps its own. External packages are loaded with `use "path"` (see [Files, folders and the library](Reference.md#files-folders-and-the-library)).

- [How to read this](#how-to-read-this)
- [core](#core): [The program](#the-program), [Files](#files), [Threads](#threads), [Memory](#memory), [Temporary memory](#temporary-memory), [Arenas](#arenas), [Lists and listviews](#lists-and-listviews), [Arrays](#arrays), [Numbers](#numbers), [simd](#simd), [Text](#text), [Printing and formatting](#printing-and-formatting), [Maps](#maps), [Fixed maps](#fixed-maps), [Indexes of your own types](#indexes-of-your-own-types), [C text](#c-text)
- [fs](#fs): files, folders, and paths
- [os](#os): program environment, [other programs](#other-programs), random bytes, [libraries](#libraries), and time
- [json](#json): reading and writing JSON
- [net](#net): TCP connections, [UDP](#udp), and waiting on multiple sockets
- [math](#math): floating-point math and random numbers
- [utf8](#utf8): UTF-8 character helpers
- [bits](#bits): fixed-size bit sets
- [ring](#ring): double-ended queue
- [raylib](#raylib): graphics, input, and audio

---

## How to read this

Each function is listed with its parameters and return type. `$I` stands for any integer type: the `Integer` type alias from `core`, which you can also use in your own `where` clauses (`where $I is Integer`). Functions with the same name and different parameter types are overloads; each call picks the overload that fits best.

**Memory ownership** of returned values is marked with four terms:

| Term | Lifetime and ownership |
|---|---|
| **yours** | Allocated on the heap; you own it and must release it with `free`. |
| **temporary** | Allocated in scratch memory; lives until the enclosing `with scratch():` block ends (see [Temporary memory](Reference.md#temporary-memory)). Copy it to a `&str` or `&list` to keep it longer. |
| **in the arena** | Allocated in the `Arena` you passed; lives until `free` or `clear` is called on that arena (see [Arenas](#arenas)). |
| **a view** | Points into memory you passed in; valid only while that source lives and does not move. |

Functions that can fail return their error type in a union. Each module documents the error types it uses.

Functions that abort the program on an invalid argument, such as an out-of-bounds index, a `File` that is not open (its descriptor is negative), or an uninitialized `Thread`, also have a trailing `place: &strview = @here()` parameter. That parameter is omitted from most signatures below so they stay concise; it lets the runtime error point at your call site and can only be passed by name (`place = p`).

---

## core

### The program

- `print(values...)`: writes the values in order to `stdout`, followed by a newline. All arguments are evaluated first (just like a regular call), formatted into one line, and written with a single `write(stdout, line)` so lines from concurrent threads never interleave. Because `print` accepts mixed types, it is built into the language; user functions collect extra arguments of a single type with `..items: T` (see [Extra arguments](Reference.md#extra-arguments)).
- `eprint(values...)`: same as `print`, written to `stderr`.
- `panic(message: &strview, place: &strview = @here()) -> never`: writes `flag: <message>`, the source location, and the call stack to `stderr`, then exits with code `134` (see [Runtime errors](Reference.md#runtime-errors)). A function with its own `place` parameter can forward it (`panic(message, place = place)`) to blame its caller; passing `place = ""` omits the location line and prints only the call stack.
- `on_panic(hook: func(&strview, &strview))`: registers a callback that `panic` runs after printing the message and call stack and before exiting. It receives `(message, place)` for every exit-134 runtime error, including compiler-generated checks. Calling `on_panic` again replaces the previous hook, and the hook may terminate the program with `exit`. A `-shared` library has its own `core`, so the host program's `on_panic` hook does not run for runtime errors inside a loaded library.
- `exit(code: i32) -> never`: terminates the program with `code`. Pending file output is flushed, but `defer` statements do not run.

Command-line arguments live in `args()` in [os](#os).

### Files

A `File` represents an open file descriptor. `stdin`, `stdout`, and `stderr` are always open; they are constants of `core`, so no local variable may shadow their names. To open files by path, use `open_file` and `create_file` in [fs](#fs).

#### Buffering and flushing

Writes are buffered and sent to the OS in larger chunks so many small writes stay fast. Buffered output is flushed automatically:

- when 64 KiB have accumulated,
- when you call `flush` or `close`,
- before starting another program,
- when the program ends (returning from `main`, `exit`, `panic`, or a failed `assert`),
- before the program blocks to wait: reading from any file, `ready`, `accept` or `receive` in [net](#net), reading from an empty `Channel`, `wait` on a thread or process, `sleep`, or `main` ending while threads still run, so prompts and logs appear before waiting, even when redirected to a file,
- at every newline when the file is a terminal or `stderr`.

`stdout` and `stderr` preserve the order of your calls, and both flush before reading from `stdin`. A crash flushes pending output before printing the crash report, whereas a signal such as Ctrl-C discards buffered output. If another thread is actively writing to a file, automatic flushes (`flush()`, waits, exit, runtime errors, and crashes) flush every other file without blocking on that one, so a thread stuck sending to a slow peer cannot stall the rest of the program.

#### File functions

- `write(f: File, value: $T)`: formats `value` (as `print` does, without a newline) and writes it to `f`: `write(stdout, "name? ")`. Write raw bytes as text with `write(f, view(bytes))`. Values are formatted in thread-local temporary memory first and handed to the file whole, so concurrent writes from different threads never interleave.
- `read_line(f: File) -> &strview|null|FsError`: reads the next line without its trailing `\n` or `\r\n`, **temporary**, or `null` at end of file. All other bytes are preserved, including zero bytes. Because every line allocates temporary memory, wrap line-reading loops in `with scratch():` (see the example below).
- `read(f: File, into: *&str, size: i64) -> i64|FsError`: appends up to `size` bytes to `into` and returns the count read (`0` at end of file, or immediately when `size` is `0`). A negative `size` aborts the program.
- `flush(f: File) -> FsError?`: flushes buffered writes now and returns the first write error since the previous `flush`. Failed writes do not prevent later writes, and writes that the OS cannot accept yet block until it can; call `try flush(out)` (or check its return value) when you need to confirm all output succeeded.
- `flush()`: flushes every open file that is not currently being written by another thread.
- `close(f: File)`: flushes buffered writes and closes `f`. `with file:` closes it automatically via `operator with`. `close` does not report write errors (call `flush` first to check them). If another thread is blocked reading `f`, `close` waits for that read to finish (on a `Socket`, closing interrupts the read, which sees end of input). Because a `File` holds a raw OS file descriptor like C, the OS gives that number to the next file that opens, and the old `File` then reads and writes that file; do not use a `File` after closing it.
- `ready(f: File, seconds: f64 = 0.0) -> bool`: reports whether reading `f` will return data or end of file without blocking. Waits up to `seconds` (`0.0` polls immediately; a negative value waits indefinitely). Flushes pending writes first.
- `ready(files: &listview[File], seconds: f64 = 0.0) -> &listview[i64]`: waits until at least one file in `files` is ready or `seconds` elapses, returning the indices of the ready files (**temporary**, empty on timeout).
- `File{fd}`: wraps a file descriptor obtained from C. A negative descriptor aborts the program: `flag: File{-1} is not an open file descriptor`.
- `file(fd: i32, path: &strview) -> File`: wraps `fd` and attaches `path` so errors name it (`cannot write notes.txt: No space left on device`). This is how `open_file` constructs its `File`.

#### Non-blocking reads, standard output errors, and pipes

- **Non-blocking reads**: `ready` lets a game loop or server poll a child process's output or `stdin` without stalling a frame (when `ready(p.output)` is true, `_ = read(p.output, log, 65536)` does not wait). A `read` after `ready` will not block, though `read_line` still waits until a full line or end of file arrives. On Windows, only sockets support OS waiting, so `ready` re-checks pipes and consoles every millisecond; a console becomes ready on any keypress, though reading it may still wait for Enter.
- **Unchecked `stdout` errors**: If writing to `stdout` fails and no `flush(stdout)` consumed the error, the program prints `flag: cannot write the standard output: No space left on device` at exit and changes a `0` exit code (including `exit(0)`) to `1`. Non-zero exit codes are kept as-is.
- **Broken pipes**: On Linux and macOS, writing to a pipe whose reader has closed (such as `program | head` once `head` exits) terminates the program silently with `SIGPIPE`, matching C (see [Runtime errors](Reference.md#runtime-errors)). Sockets in `net` and pipes to child processes you launch return an error instead.
- **Read-ahead and inheritance**: `stdin` buffers ahead, so a child process started after reading `stdin` will not see bytes already buffered. Files opened by the standard library are not inherited by child processes, even when another thread spawns a process concurrently.

```flag
use fs

func main():
    out := create_file("squares.txt")
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

    input := open_file("squares.txt")
    if input is FsError:
        eprint(input)
        return
    with input:
        while true:
            with scratch(): # each line is temporary; release it before reading the next
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

#### `FsError` and OS error codes

OS failures in `core`, `fs`, and `os` return `FsError`:

```flag
type error FsError:
    code: i32        # system error number (errno)
    action: &strview # what failed: open, create, read, write, list, make, remove, run...
    path: &strview   # the path, program, or address passed in
```

Printing an `FsError` formats all three fields using the OS message for `code`, for example `cannot open notes.txt: No such file or directory`. For a failed `read`, `read_line`, or `flush`, `path` is a **temporary** copy of the path the file was opened or created with, or that `file` gave, or `the standard input`, `the standard output`, or `the error output` for the standard streams. A negative `code` represents an error with no `errno` equivalent (such as a Windows error code that does not map to `errno`).

- `errno() -> i32`: the error code of the last failed C call. Useful after your own `extern` calls: `eprint(FsError{errno(), "lock", path})`.
- `windows_errno(code: u32) -> i32`: maps a Windows `GetLastError()` code to its C `errno`, or returns `-i32(code)` when no `errno` matches: `FsError{windows_errno(GetLastError()), "lock", path}`.

### Threads

`parallel f(x)` and `Thread[T]` are built into the language (see [Threads](Reference.md#threads)). `core` provides synchronization and thread control:

#### Waiting on threads

- `wait(t: Thread[$T]) -> $T`: waits for `t` to finish and returns its result (`null` for `Thread[null]`). It frees the thread's control block (so each thread may be waited only once) and transfers the thread's temporaries into the waiting thread's scratch memory. Declaring your own `wait` for `Thread` is a compile error (`'wait' on Thread is built into the language and cannot be redeclared`), while `wait` for any other type is a normal overload. Calling `wait` on a zero-initialized `Thread` aborts: `flag: 'wait' called on an uninitialized (never started) Thread`.
- `done(t: Thread[$T]) -> bool`: returns `true` if `t` has finished, without blocking. A subsequent `wait(t)` returns immediately. Data written by the thread is only safe to read after `wait(t)`, not after `done(t)` (reading in between is a data race, and `-memory` warns). Calling `done` on an uninitialized `Thread` aborts: `flag: 'done' called on an uninitialized (never started) Thread`.

#### Locks (`Lock[$T]`)

`Lock[$T]` guards a value stored in its `.value` field so only one thread accesses it at a time. Use `Lock[null]` to guard a section of code with no associated value. Writing bare `Lock` is a compile error: `Lock requires one type argument for the guarded value: Lock[i64], or Lock[null] when guarding code only`.

A `Lock` works at zero with no initialization (`count: Lock[i64]`, then `count.value += 1` inside `with lock(count):`). Locks are reentrant: a thread that already holds a lock can acquire it again without deadlocking, including across calls between a host program and a loaded Flag library.

Pass locks by pointer (`*Lock[$T]`), since copying a `Lock` produces an independent lock. Under `-memory`, the checker warns if `.value` is accessed outside `with lock(x):` while another thread uses that lock (locks held by a caller count for functions it calls), or if an active lock is copied.

- `lock(l: *Lock[$T]) -> Locked`: waits until no other thread holds `l`, then acquires it.
- `operator with(held: Locked)`: releases the lock when the `with lock(x):` block exits.

#### Channels (`Channel[$T, $$size]`)

`Channel[$T, $$size]` is a fixed-capacity, thread-safe queue holding up to `size` values inline (no heap allocation, ready to use at zero; declare it where it lives and pass `*Channel`). `size` must be at least 1: `send`, `receive` and `for` on a channel of size `0` fail to compile with `a channel holds at least one value, its size cannot be 0`.

Its internal lock is exposed as `c.queue`. If a thread holds `with lock(c.queue):` and then blocks in `send` or `receive`, the program aborts rather than deadlocking: `flag: deadlock: channel blocked while holding its own queue lock ('with lock(c.queue)'), preventing other threads from making progress`.

- `send(c: *Channel[$T, $$size], v: $T)`: appends `v`, blocking while the channel is full. Sending on a closed channel aborts: `flag: cannot send on a closed channel`.
- `receive(c: *Channel[$T, $$size]) -> $T|null`: removes and returns the oldest value, flushing pending output before blocking if empty. Returns `null` once the channel is closed and empty. On a channel of `T?`, a sent `null` looks identical to closure; iterate with `for v in c` instead.
- `close(c: *Channel[$T, $$size])`: closes `c`. Queued values can still be received; afterward `receive` returns `null`. `with c:` calls `close(c)` when the block exits.
- `for v in c`: receives values until `c` is closed and empty.

#### Atomics (`atomic[T]`)

`atomic[T]` holds an `i32`, `u32`, `i64`, `u64`, or pointer updated lock-free by concurrent threads. Like `simd`, it and its operations belong to the language (see [Atomics](Reference.md#atomics)):

- `get(a) -> T`: atomic load
- `set(a, v)`: atomic store
- `add(a, n) -> T`: atomic fetch-and-add (returns the previous value)
- `swap(a, v) -> T`: atomic exchange (returns the previous value)
- `compare_swap(a, old, new) -> bool`: writes `new` only if `a` equals `old`, returning whether it succeeded

All `core` file functions (including `print`) are safe to call from any thread.

```flag
use os

shared context totals:
    counter: Lock[i64]

func add(times: i32):
    for i in 0..times:
        with lock(totals.counter):
            totals.counter.value += 1

func square_all(jobs: *Channel[i64, 8], results: *Channel[i64, 8]):
    for n in jobs:
        send(results, n * n)
    close(results)

func main():
    workers: &list[Thread[null]] = heap([])
    defer free(workers)
    for k in 0..cores():
        append(workers, parallel add(1000))
    for w in workers:
        wait(w)
    print(totals.counter.value) # 1000 for each core

    jobs: Channel[i64, 8]
    results: Channel[i64, 8]
    t := parallel square_all(jobs, results)
    for n in 1..4:
        send(jobs, n)
    close(jobs)
    for r in results:
        print(r) # 1, 4, and 9
    wait(t)
```

### Memory

#### Heap allocation

- `heap(v: $T) -> *$T`: copies `v` onto the heap (**yours**).
- `heap(text: &strview) -> &str`: allocates an owning `&str` copy on the heap (**yours**), e.g. `heap("text")`.
- `heap(items: &listview[$T]) -> &list[$T]`: allocates an owning `&list` copy (**yours**).
- `heap(items: array[$T, $$n]) -> &list[$T]`: allocates an owning `&list` copied from an array or literal (**yours**). The literal determines the element type: `heap([u8(1), 2])` is `&list[u8]`.
- `heap(pairs: fixmap[$K, $V, $$n]) -> &map[$K, $V]`: allocates an owning `&map` copied from a fixed map or map literal (**yours**), e.g. `heap({"a": 1})`.
- `heap(pairs: &map[$K, $V]) -> &map[$K, $V]`: allocates a copy of a `&map` (**yours**).
- `heap(items: emptyarray) -> &list[never]`, `heap(pairs: emptyfixmap) -> &map[never, never]`, and `heap("")`: empty heap-backed owners that do not allocate until they grow (`names: &list[&strview] = heap([])`). `&list[never]` converts to any `&list[T]`, and `&map[never, never]` to any `&map[K, V]`.
- `free(p: *$T)`: releases a single heap allocation returned by `heap(v: $T)`. If `p` points to a `&str`, `&list`, `&map`, or `Arena` (for example from `heap(Arena{})`), `free(p)` invokes that type's `free` instead; cast to `free(p as *bytes)` to free the outer heap block itself. Memory allocated in an `Arena` is freed by its arena, not `free(p)`. Freeing a stack variable is a compile error: `cannot free stack memory x; free() only releases heap allocations`.

#### Language primitives

- `ptr(x)`: address of a variable, field, element, or `export func` (see [Calling C](Overview.md#calling-c)).
- `@sizeof(T)`: size of `T` in bytes as an integer literal. Both belong to the language, not `core` (see [Builtins](Reference.md#builtins)).

#### C memory and string functions

`core` exposes these C standard library functions directly (`*bytes` corresponds to `void *`, `cstr` to `char *`):

| Function | Signature |
|---|---|
| Allocation | `malloc(size: i64) -> *bytes`, `calloc(count: i64, size: i64) -> *bytes`, `realloc(at: *bytes, size: i64) -> *bytes`, `free(at: *bytes)` |
| Memory ops | `memcpy(to: *bytes, from: *bytes, size: i64) -> *bytes`, `memmove(to: *bytes, from: *bytes, size: i64) -> *bytes`, `memset(to: *bytes, byte: i32, size: i64) -> *bytes`, `memcmp(a: *bytes, b: *bytes, size: i64) -> i32`, `memchr(at: *bytes, byte: i32, size: i64) -> *bytes` |
| C strings & parsing | `strlen(s: cstr) -> i64`, `snprintf(into: *u8, size: i64, format: cstr, ...) -> i32`, `strtod(text: cstr, end: *bytes) -> f64` |

`*bytes` supports byte-level pointer arithmetic with `+` and `-`, so `memcpy(to + 8, from, n)` writes 8 bytes past `to` (see [Arithmetic](Reference.md#arithmetic)).

#### Out of memory and `try_reserve`

- `out_of_memory() -> never`: flushes pending output without allocating, writes `flag: out of memory` to `stderr`, and exits immediately with code `9`. The library calls it when allocation fails, and you can call it after your own `malloc`.

Programs that must recover from allocation failure (such as a game engine loading a level) can preallocate with `try_reserve`, which returns `NoMemory` instead of exiting, and then populate the container without allocating. `try_reserve` is available on `&list`, `&str`, `&map`, and `Arena`.

```flag
type Enemy:
    hp: i32

func load(enemies: *&list[Enemy], count: i64) -> NoMemory?:
    try try_reserve(enemies, count)
    for i in 0..count:
        append(enemies, Enemy{hp = 10}) # room is already reserved; append will not allocate
    return null
```

`try_reserve` allocates a new block and copies existing data into it, leaving the original data intact if allocation fails. A negative size or a byte count that overflows `i64` also returns `NoMemory` without asking the OS, whereas `reserve` aborts with `flag: out of memory`.

### Temporary memory

- `scratch() -> Scratch`: marks the current position in thread-local scratch memory. `with scratch():` releases every temporary created inside the block when it exits.
- `tmp(size: i64) -> *bytes`: allocates `size` bytes in scratch memory (**temporary**).
- `tmp(text: &strview) -> &strview`: copies `text` into scratch memory with a trailing zero byte (**temporary**).
- `tmp(items: &listview[$T]) -> &listview[$T]`: copies elements into scratch memory (**temporary**), useful for returning a result built in a local `&list` that you then `free`.

### Arenas

An `Arena` carves allocations from large heap blocks and frees them all at once. It starts empty and allocates blocks of 64 KiB (or larger when a single request exceeds 64 KiB). A `&str`, `&list`, or `&map` created in an arena grows inside that arena, and calling `free` on it only empties it without freeing memory.

- `arena(a: *Arena, v: $T) -> *$T`: copies `v` **in the arena**, like `heap(v)`.
- `arena(a: *Arena, text: &strview) -> &str`: copies `text` **in the arena**.
- `arena(a: *Arena, items: &listview[$T]) -> &list[$T]`: copies a listview **in the arena**.
- `arena(a: *Arena, items: array[$T, $$n]) -> &list[$T]`: copies an array or literal **in the arena**.
- `arena(a: *Arena, pairs: fixmap[$K, $V, $$n]) -> &map[$K, $V]`: copies a fixed map or map literal **in the arena**.
- `arena(a: *Arena, pairs: &map[$K, $V]) -> &map[$K, $V]`: copies a map **in the arena**.
- `arena(a: *Arena, items: emptyarray) -> &list[never]` and `arena(a: *Arena, pairs: emptyfixmap) -> &map[never, never]`: empty arena-backed owners (`enemies: &list[Enemy] = arena(level, [])`; see [Arenas](Reference.md#arenas)).
- `clear(a: *Arena)`: resets the arena, releasing all allocations while keeping its blocks for reuse.
- `free(a: *Arena)`: frees all blocks and resets the arena to empty, ready to reuse.
- `reserve(a: *Arena, size: i64)`: ensures room for `size` bytes so subsequent allocations up to that size do not request a new heap block. Each allocation is rounded up to the arena's alignment.
- `try_reserve(a: *Arena, size: i64) -> NoMemory?`: same as `reserve`, returning `NoMemory` if the OS is out of memory.

`Arena` fields: `count` (blocks currently in use), `used` (bytes used in the active block), and `blocks` (all allocated blocks). Printing an `Arena` shows `count` and `used`, never the contents of its blocks.

An `Arena` is not thread-safe without synchronization: give each thread its own arena, or guard shared access with a `Lock`.

### Lists and listviews

A `&listview[$T]` is a non-owning view of elements; a `&list[$T]` is an owning, growable list. Lists get their backing memory from `heap` or `arena` (`heap([])` for an empty heap list), `[]` is the empty listview, and any `&list[$T]` converts implicitly to `&listview[$T]`. Growing an uninitialized list aborts: `flag: this &list has no memory to grow in: give it heap([]) or arena(a, [])` (see [Fat pointers](Reference.md#fat-pointers)).

#### Built-in operations (lists and listviews)

- `s[i]`: reads or assigns element `i`. A compile-time-known negative index (literal or constant) counts from the end (`s[-1]` is the last element). Any out-of-bounds index, including a runtime-computed negative index, aborts the program (see [Runtime errors](Reference.md#runtime-errors)).
- `s[a:b]`: **a view** of elements `a` through `b - 1`. Compile-time negative bounds count from the end; `s[a:]` runs to the end and `s[:b]` starts at `0`. Invalid bounds (`a > b` or out of range) abort.
- `for x in s`, `for i, x in s`, and `for x in reverse s`: iterate over elements.
- `a == b`: compares element by element; `print` formats as `[1, 2, 3]`.

#### Mutating lists (`&list[$T]`)

- `append(l: *&list[$T], v: $T)`: appends `v`. The first allocation reserves room for 8 elements, and capacity doubles thereafter.
- `append(l: *&list[$T], items: &listview[$T])`: appends all elements of `items` (safe even when `items` views `l` itself).
- `reserve(l: *&list[$T], want: i64)`: ensures capacity for at least `want` elements so appends up to `want` do not allocate.
- `try_reserve(l: *&list[$T], want: i64) -> NoMemory?`: same, returning `NoMemory` without modifying `l` if allocation fails (see [Memory](#memory)).
- `insert(l: *&list[$T], at: $I, v: $T)`: inserts `v` at `at` (up to `l.len`), shifting later elements right. Like all position-taking functions, `at` counts from `0` and a negative index aborts.
- `pop(l: *&list[$T]) -> $T|null`: removes and returns the last element, or `null` if empty. On a `&list[T?]`, a stored `null` looks the same as empty; check `l.len` first.
- `remove(l: *&list[$T], at: $I) -> $T`: removes and returns the element at `at`, shifting later elements left.
- `clear(l: *&list[$T])`: sets length to `0`, keeping allocated capacity.
- `free(l: *&list[$T])`: releases the list's memory and resets it to empty (in an arena, resets it without freeing). Does not free elements that own memory; free those first.
- `l = items`: when `items` is a `&listview[$T]`, copies its elements into `l`, reusing `l`'s buffer. Assigning one `&list` to another shares the same backing buffer instead of copying; write `l = other[:]` to copy into `l`, or `heap(items)` for a new list.

#### Searching, sorting, and reordering (`&listview[$T]` and `&list[$T]`)

- `find(items: &listview[$T], v: $T) -> i64|null`: index of the first element where `element == v`.
- `find(items: &listview[$T], fits: func($T) -> bool) -> i64|null`: index of the first element for which `fits` returns `true`.
- `contains(items: &listview[$T], v: $T) -> bool`: whether `v` appears in `items`.
- `binary_search(items: &listview[$T], v: $T) -> i64|null`: index of `v` in a sorted view, ordered by `<`.
- `sort(s: &listview[$T])`: stable O(n log n) in-place merge sort using `<`. Temporarily allocates space for half the elements while sorting.
- `sort(s: &listview[$T], before: func($T, $T) -> bool)`: stable in-place merge sort using a custom comparator (`before(a, b)` returns `true` when `a` comes before `b`).
- `flip(items: &listview[$T])`: reverses the elements in place.
- `swap(s: &listview[$T], i: $I, j: $I)`: swaps elements `i` and `j`.

Pass an array to any listview function via `ptr`: `sort(ptr(values))`.

### Arrays

- `a == b` and `a != b`: compare element by element.

Arrays do not support element-wise arithmetic; use [simd](#simd) for lane-wise operations.

### Numbers

- `min(a: $T, b: $T) -> $T`: smaller of `a` and `b` (for any type with `<`).
- `max(a: $T, b: $T) -> $T`: larger of `a` and `b` (for any type with `<`).
- `clamp(x: $T, low: $T, high: $T) -> $T`: restricts `x` to `[low, high]`.
- `clamp(x: simd[$T, $$n], low: $T, high: $T) -> simd[$T, $$n]`: clamps every lane of `x` between scalar `low` and `high`: `clamp(v, 0.0, 1.0)`.
- `abs(x: $T) -> $T`: absolute value for signed integers (`i8`, `i16`, `i32`, `i64`) and floats (`f32`, `f64`). For the most negative value of a signed integer type there is no positive counterpart, so it wraps back to itself (`abs(i8(-128))` is `-128`). For floats, `abs` clears the sign bit like C's `fabs`, so `abs(-0.0)` is `0.0` and a negative NaN becomes positive.
- `lerp(a: $T, b: $T, t: $T) -> $T`: linear interpolation for `f32` and `f64` (`a` when `t` is `0.0`, `b` when `t` is `1.0`).

Transcendental and rounding functions (`sqrt`, `sin`, `floor`, etc.) are in [math](#math).

### simd

These lane-wise operations are built into the language alongside `simd` operators (see [SIMD](Reference.md#simd)):

- `select(m, a, b)`: picks each lane from `a` where mask `m` is `true` and from `b` where `false`. Either `a` or `b` may be a scalar broadcast to all lanes.
- `any(m) -> bool` and `all(m) -> bool`: whether any lane, or every lane, of mask `m` is `true`.
- `sum(v)`: sums the lanes in a fixed reduction order, adding the upper half onto the lower half until one value remains: `(v[0] + v[2]) + (v[1] + v[3])` for 4 lanes.
- `min(a, b)`, `max(a, b)`, `abs(v)`, and `sqrt(v)`: apply the corresponding [Numbers](#numbers) or `math.sqrt` operation lane by lane. `min` evaluates `b if b < a else a` and `max` evaluates `b if a < b else a`, so if either lane is NaN the result is `a`.

`core` adds slice loading:

- `operator =(dest: simd[$T, $$n], src: &listview[$T], place: &strview = @here()) -> simd[$T, $$n]`: loads `$$n` elements from a slice: `v: simd[f32, 4] = values[i:i + 4]` or `simd[f32, 8](values[0:8])`. A mismatched slice length aborts: `flag: simd with 4 lanes requires a slice of length 4, got 5`. Storing back into a slice (`values[i:i + 4] = v`) is built into the language (see [SIMD](Reference.md#simd)).

### Text

A `&strview` is a read-only view of bytes; a `&str` is owning, growable text. Text literals have type `&strview`, and `&str` converts implicitly to `&strview`. Text stores raw bytes, and the library interprets it as UTF-8.

#### Built-in operations

- `s[i] -> u8`: byte at index `i` (compile-time negative indices count from the end). Assigning `s[i] = b` works on `&str`, while `&strview` is read-only (`&strview is read-only; mutate the owning &str instead`).
- `s[a:b] -> &strview`: **a view** of bytes `a` through `b - 1` (`s[a:]` to the end, `s[:b]` from `0`).
- `for c in s` and `for c in reverse s`: yields each UTF-8 character as a `&strview`. Any byte that does not belong to a valid UTF-8 sequence (including overlong encodings, surrogates, and code points above `U+10FFFF`) is yielded alone as a 1-byte view. Forward and reverse iteration produce the exact same pieces, matching `count_chars`, `valid_utf8`, and `first_code` in [utf8](#utf8).
- `a == b` and `a < b`: compare byte by byte.
- `"a" + x`: concatenates text with any printable value (**temporary**).

#### Building and modifying `&str`

- `s += x`: appends any printable value to `s`.
- `s = text`: when `text` is a `&strview`, copies its bytes into `s`, reusing `s`'s buffer. Assigning one `&str` to another shares the buffer instead of copying; write `s = view(other)` to copy into `s`, or `heap(text)` for a new `&str`.
- `add_byte(s: *&str, b: u8)`: appends a single byte.
- `add_char(s: *&str, code: u32)`: appends the Unicode code point encoded as UTF-8, substituting `U+FFFD` for surrogates and values above `0x10FFFF`.
- `reserve(s: *&str, want: i64)`: ensures capacity for at least `want` bytes so appends up to `want` do not allocate.
- `try_reserve(s: *&str, want: i64) -> NoMemory?`: same, returning `NoMemory` on allocation failure.
- `view(s: &str) -> &strview`: **a view** of `s`.
- `free(s: *&str)`: releases `s`'s memory and leaves it empty (in an arena, empties it without freeing).

#### Searching, slicing, and transforming

- `find(s: &strview, part: &strview) -> i64|null`: byte index of the first occurrence of `part`.
- `find_last(s: &strview, part: &strview) -> i64|null`: byte index of the last occurrence of `part`.
- `contains(s: &strview, part: &strview) -> bool`: whether `part` occurs in `s`.
- `starts_with(s: &strview, head: &strview) -> bool` and `ends_with(s: &strview, tail: &strview) -> bool`: prefix and suffix checks.
- `trim(s: &strview) -> &strview`, `trim_start(s: &strview) -> &strview`, and `trim_end(s: &strview) -> &strview`: **a view** with ASCII whitespace (`' '`, `'\t'`, `'\n'`, `'\r'`) stripped from both ends, the start, or the end.
- `split(s: &strview, separator: &strview) -> &listview[&strview]`: parts separated by `separator`, as views of `s` inside a **temporary** listview. Always returns at least one part; an empty separator returns `[s]`.
- `words(s: &strview) -> &listview[&strview]`: non-empty runs separated by `' '`, `'\t'`, `'\n'`, and `'\r'`, as views of `s` inside a **temporary** listview, for example to count them with `counts[w] += 1` in `for w in words(text):`.
- `replace(s: &strview, old: &strview, new: &strview) -> &strview`: replaces every occurrence of `old` with `new` (**temporary**).
- `join(parts: &listview[$T], separator: &strview) -> &strview`: joins any printable elements with `separator` between them (**temporary**).
- `lower(s: &strview) -> &strview` and `upper(s: &strview) -> &strview`: converts ASCII letters to lowercase or uppercase (**temporary**).
- `items(s: &strview) -> &listview[u8]`: **a view** of the underlying bytes. Writing through it into a text literal crashes the program because literals live in read-only memory.
- `view(bytes: &listview[u8]) -> &strview`: **a view** reinterpreting bytes as text.

#### Parsing numbers

All three parse the entire string with no leading or trailing whitespace, returning `null` if invalid:

- `parse_int(s: &strview) -> i64|null`: decimal digits with an optional `+` or `-`; returns `null` if the value overflows `i64`.
- `parse_float(s: &strview) -> f64|null`: decimal float with optional sign, decimal point, and exponent, or `inf`/`nan` (e.g. `1.5`, `-2e3`, `.5`, `-inf`).
- `parse_hex(s: &strview) -> u64|null`: 1 to 16 hexadecimal digits without a `0x` prefix.

### Printing and formatting

`print`, `eprint`, `+`, and `+=` on text format values using `operator +=(s: *&str, x: T)`. `core` provides formatters for:

- **integers** (decimal) and **`bool`** (`true` or `false`),
- **floats** (the fewest digits that round-trip to the same float, such as `0.1` and `2.0`, switching to scientific notation for very large or small numbers like `1e+16` and `1e-05`; non-finite values print as `inf`, `-inf`, and `nan`),
- **pointers** (hexadecimal),
- **lists and listviews** (`[1, 2]`), and **maps and fixed maps** (`{a: 1, b: 2}`).

Three formatting helpers wrap values for `print` and `+`:

- `fixed(value: f64, places: i32) -> Fixed` and `fixed(value: f32, places: i32) -> Fixed`: formats with `places` decimal digits, rounding ties to even (`fixed(2.5, 0)` is `2`). Values rounding to zero drop the minus sign (`fixed(-0.4, 0)` is `0`), and negative `places` count as `0`.
- `hex(value: $I, digits: i32 = 0) -> Hex`: formats an integer in hexadecimal with a `0x` prefix and no leading zeros, using the bit width of its type for negative numbers (`hex(255)` is `0xff`, `hex(i8(-1))` is `0xff`, and `hex(-1)` is `0xffffffffffffffff` because an untyped literal is `i64`). `digits` zero-pads to at least that many hex digits without truncating longer values (`hex(u8(7), 2)` is `0x07`).
- `pad(value: $T, width: i32) -> Padded[$T]`: pads any printable value with spaces to `width` characters on the left, or on the right when `width` is negative (`pad(42, 5)` is `   42`, `pad("ab", -4)` is `ab  `). Longer values are never truncated, and helpers compose (`pad(fixed(x, 2), 8)`).

The compiler formats all other types automatically: structs as `Point{x = 1, y = 2}`, tuples as `(1, 2)`, arrays as `[1, 2]`, tagged unions as their active member, and void types by name. Declare your own `operator +=` to customize formatting (see [Printing](Overview.md#printing)).

### Maps

A `&map[$K, $V]` is an owning hash map backed by `heap` or `arena` (`heap({})` for an empty map, `heap({"a": 1})` from a literal).

#### Map operations

- `m[k] -> $V|null`: looks up `k` via `operator [](m: &map[$K, $V], key: $K) -> $V|null`, returning `null` if absent. On a `&map[K, T?]`, use `contains` to distinguish a missing key from a stored `null`.
- `put(m: *&map[$K, $V], key: $K, value: $V)`: inserts or overwrites `key` (same as `m[k] = v`).
- `m[k] += v`, `m[k].field = v`, and `m[k][j] = v`: look up `k` once and update the value in place, inserting a zero-initialized entry if `k` is missing (`counts[word] += 1`, `players[name].score += 10`). Because zero-initialized `&str`, `&list`, and `&map` values have no backing memory, initialize a new entry before growing it (`groups[k] = heap([])`, then `pairs[a][b] += 1` or `append(groups[k], v)`). Passing `m[k]` to a pointer parameter copies the value (zero if missing) and writes it back after the call, so `append(groups[k], v)` updates the list inside the map.
- `ptr(m[k])`: returns a pointer to the value slot in `m`, inserting a zero-initialized entry if `k` is absent. Invalidated if the map grows.
- `contains(m: &map[$K, $V], key: $K) -> bool`: whether `key` is present.
- `reserve(m: *&map[$K, $V], want: i64)`: ensures room for at least `want` keys so `put` calls up to `want` do not allocate.
- `try_reserve(m: *&map[$K, $V], want: i64) -> NoMemory?`: same, returning `NoMemory` on allocation failure.
- `remove(m: *&map[$K, $V], key: $K) -> $V|null`: removes `key` and returns its value, or `null` if absent.
- `clear(m: *&map[$K, $V])`: removes all entries while keeping capacity (and frees owned `&str` keys).
- `free(m: *&map[$K, $V])`: releases the table and leaves the map empty (in an arena, empties it without freeing). Does not free values or non-`&str` keys that own memory; free those first. Owned `&str` keys are freed automatically.
- `for e in m`: yields each `entry[$K, $V]` (with `e.key` and `e.value`) in hash-table order.
- `hash(key) -> u64`: computes a key's hash via `operator hash`. `core` defines `operator hash` for integers, floats (`0.0` and `-0.0` hash identically), and text; the compiler generates it for other hashable types (see [Operators](Reference.md#operators)).

#### Owned text keys (`&map[&str, $V]`)

A `&map[&str, $V]` owns its keys and provides dedicated overloads accepting `&strview`:

- `m[k] = v`, `put(m: *&map[&str, $V], key: &strview, value: $V)`, `m[k] += v`, and `ptr(m[k])` copy new keys into the map's allocator (even from a literal or `&strview`, e.g. `m["port"] = 80`).
- `m[k]`, `contains(m: &map[&str, $V], key: &strview) -> bool`, and `remove(m: *&map[&str, $V], key: &strview) -> $V|null` accept a `&strview` directly (`contains(m, line[:4])`, `remove(m, "port")`).
- `remove`, `clear`, and `free` automatically free the map's `&str` keys.

#### Key requirements

Map keys must support `==` and `operator hash`. Numbers, text, `bool`, enums, pointers, arrays, and structs or tagged unions composed of them support both automatically. If a struct defines a custom `operator ==`, it must also define `operator hash` so equal keys hash equally; using it as a map key without one fails to compile with `Key defines 'operator ==' but not 'operator hash'; equal values must have equal hashes, so define 'operator hash' for Key`.

Because float `nan` is not `==` to itself, inserting `nan` as a key adds a new entry each time and can never be looked up again.

### Fixed maps

A `fixmap[$K, $V, $$max]` stores up to `max` entries inline in insertion order, with no heap allocation and no `free`. An untyped map literal produces a fixed map sized to its entries (`{"a": 1}` is `fixmap[&strview, i64, 1]`). Lookups scan linearly, so fixed maps are meant for small collections.

- `put(m: *fixmap[$K, $V, $$max], key: $K, value: $V) -> MapFull?`: inserts or updates `key`, returning `MapFull` if full. Because an assignment cannot propagate an error, `m[k] = v` does not compile on a fixed map; write `try put(m, k, v)`.
- `m[k] -> $V|null`: looks up `k`.
- `contains(m: *fixmap[$K, $V, $$max], key: $K) -> bool`: whether `key` is present.
- `remove(m: *fixmap[$K, $V, $$max], key: $K) -> $V|null`: removes `key` and returns its value, preserving the order of the remaining entries.
- `clear(m: *fixmap[$K, $V, $$max])`: removes all entries.
- `for e in m`: yields entries in insertion order.

### Indexes of your own types

Indexing a user-defined type (`x[i]`) invokes your `operator []` with the index as written, including negative values (see [Indexing](Overview.md#indexing)). Use `out_of_bounds` to report bounds errors in the same format as built-in containers:

- `out_of_bounds(i: $I, len: i64) -> never`: aborts with `flag: index 5 out of bounds for length 3`.
- `out_of_bounds(from: $I, to: $I, len: i64) -> never`: aborts with `flag: slice 2:9 out of bounds for length 3`.

```flag
type Grid:
    cells: array[i32, 9]

operator [](g: *Grid, i: $I, place: &strview = @here()) -> *i32 where $I is Integer:
    if i < 0 or i >= 9:
        out_of_bounds(i, 9, place = place)
    return ptr(g.cells[i])
```

### C text

#### Null-terminated UTF-8 (`cstr`)

- `cstr`: built-in C string type (pointer to zero-terminated bytes; see [Text](Reference.md#text)). Text literals convert to `cstr` automatically.
- `cstr(text: &strview) -> cstr`: **temporary** zero-terminated copy. C functions stop at the first zero byte, so embedded zero bytes truncate paths or arguments passed to C.
- `c_text(s: &str) -> cstr`: writes a trailing zero byte after `s` in place without copying (valid until `s` is mutated); falls back to a **temporary** copy if `s` has no spare capacity.
- `view(c: cstr) -> &strview`: **a view** of a C string, or `""` for a null pointer.
- `operator +=(s: *&str, c: cstr)`: appends `c` (so `print(c)` works); appends nothing for a null pointer.

#### Null-terminated UTF-16 (`wstr`, Windows)

- `wstr`: built-in Windows UTF-16 zero-terminated string type, used by Win32 `*W` APIs.
- `wstr(text: &strview) -> wstr`: converts UTF-8 to **temporary** zero-terminated UTF-16 (`LoadLibraryW(wstr(path))`). Invalid UTF-8 bytes become `U+FFFD`.
- `tmp(w: wstr) -> &strview`: converts UTF-16 to **temporary** UTF-8, or `""` for a null pointer. Unpaired surrogates become `U+FFFD`.
- `operator +=(s: *&str, w: wstr)`: appends `w` converted to UTF-8 (so `print(w)` works).

---

## fs

Files, folders, and paths. Streaming I/O uses `File` from [core](#files), and failures return `FsError` from `core`.

All files are opened in binary mode, so bytes are identical across platforms. On Windows, paths accept `/` or `\` and are passed to the OS as UTF-16, so Unicode names like `ñ` or `字` work everywhere.

### Reading and writing files

- `open_file(path: &strview) -> File|FsError`: opens an existing file for reading.
- `create_file(path: &strview, append: bool = false) -> File|FsError`: creates or truncates a file for writing. With `append = true`, preserves existing content and appends to the end.
- `read_file(path: &strview) -> &str|FsError`: reads the entire file (**yours**).
- `write_file(path: &strview, text: &strview) -> FsError?`: creates or truncates the file and writes `text`. Write raw bytes with `write_file(path, view(bytes))`.
- `append_file(path: &strview, text: &strview) -> FsError?`: appends `text` to the file, creating it if it does not exist.

### Folders and paths

#### Filesystem queries and operations

- `exists(path: &strview) -> bool`: whether a file or folder exists at `path`.
- `is_dir(path: &strview) -> bool`: whether `path` is a folder (works even without read permission on the folder).
- `file_size(path: &strview) -> i64|FsError`: size in bytes without opening the file (works even if unreadable). Returns an error for folders.
- `modified_time(path: &strview) -> f64|FsError`: last modification time in seconds since 1970 (matching `now()` in `os`), with OS resolution (nanoseconds on Linux/macOS, 100 ns on Windows). Useful for hot-reloading assets: `if modified_time(path) iferror 0.0 != loaded_at:`.
- `list_dir(path: &strview) -> &listview[&strview]|FsError`: sorted entry names inside `path`, excluding `.` and `..` (**temporary**).
- `walk_dir(folder: &strview) -> &listview[&strview]|FsError`: sorted paths of all files under `folder` recursively (**temporary**). Symlinks to folders (and junctions on Windows) are listed as entries and not traversed. Fails only if `folder` itself cannot be listed; unreadable subdirectories are skipped.
- `make_dir(path: &strview) -> FsError?`: creates `path` and any missing parent folders; a folder that already exists is not an error. On failure, the error names the first folder that could not be created.
- `remove_file(path: &strview) -> FsError?`: removes a file, an empty folder, or a symbolic link itself.
- `rename_file(from: &strview, to: &strview) -> FsError?`: renames or moves `from` to `to`, replacing `to` if it is an existing file on every system.

#### Path helpers

- `join_path(first: &strview, second: &strview) -> &strview`: joins two paths with a single `/` (**temporary**): `join_path("a/", "/b")` is `a/b`. Returns `second` when `first` is empty.
- `base_name(path: &strview) -> &strview`: **a view** of the part after the last `/`: `base_name("docs/a.txt")` is `a.txt`.
- `extension(path: &strview) -> &strview`: **a view** from the last `.` onward (`.txt`), or `""` if none. Leading dots on the filename are ignored, so `.bashrc` and `..` have no extension while `.config.json` has `.json`.
- `dir_of(path: &strview) -> &strview`: **a view** up to and including the last `/` (`docs/`), or `./` if there is no directory component.

---

## os

Program environment, child processes, cryptographic random bytes, dynamic libraries, and time.

### Program and environment

- `args() -> &listview[&strview]`: command-line arguments, starting with the program name. Valid for the lifetime of the program.
- `env(name: &strview) -> &strview|null`: value of an environment variable (**a view**), or `null` if unset.
- `executable() -> &strview`: full path to the running executable (**temporary**), or `""` if unavailable.
- `current_dir() -> &strview`: current working directory (**temporary**), or `""` if unavailable.
- `change_dir(path: &strview) -> FsError?`: changes the current working directory to `path`.
- `system_name() -> &strview`: `"Linux"`, `"Darwin"`, or `"Windows"`.
- `cores() -> i64`: number of online CPU cores, useful for sizing worker thread pools.

Standard input is `read_line(stdin)` in [core](#files). On all platforms, paths returned by `os` use `/`, and on Windows paths, arguments, and environment variables are converted from UTF-16 so your program always sees UTF-8.

### Other programs

All buffered file output is flushed before launching a child process. If a process fails to start, these functions return an `FsError` with action `run` (`cannot run gcc: No such file or directory`). Every function takes an optional trailing `folder` parameter for the child's working directory (`""` inherits yours): `run(ptr(["git", "status"]), "../site")`. When `folder` is given, errors include it (`cannot run git in ../site: No such file or directory`), including when `folder` does not exist. On Linux and macOS, a process terminated by a signal returns `128 + signal`.

#### Running to completion

- `run(command: &strview, folder: &strview = "") -> i32|FsError`: runs `command` through the shell (`/bin/sh -c`, or `cmd.exe /d /s /c` on Windows) and returns its exit code. A command not found by the shell returns the shell's exit code (`127` for `sh`).
- `run(program: &listview[&strview], folder: &strview = "") -> i32|FsError`: runs `program[0]` directly without a shell, searching `PATH` and passing the remaining elements as exact arguments. An empty listview fails with `FsError`. On Windows, a name without an extension resolves as `.exe`, and `.bat`/`.cmd` files return `FsError` because `cmd.exe` would re-parse their arguments (run batch files via `run(command)` instead).
- `output(command: &strview, into: *&str, folder: &strview = "") -> i32|FsError`: runs `command` via the shell, appends its combined `stdout` and `stderr` (with original line endings) to `into`, and returns its exit code. Both streams share one pipe (like `2>&1`) so output stays ordered; note that C programs buffer `stdout` when writing to a pipe until full or exiting.
- `output(program: &listview[&strview], into: *&str, folder: &strview = "") -> i32|FsError`: same as above, without a shell.

```flag
use os

func main():
    text: &str = heap("")
    defer free(text)
    code := output(ptr(["git", "status", "--short"]), text)
    if code is FsError:
        eprint(code)
    elif code == 0:
        print(text)
```

#### Interactive processes (`Process`)

To communicate with a child process while it runs:

- `start(program: &listview[&strview], folder: &strview = "") -> Process|FsError`: spawns `program` without a shell (like `run(program)`) and returns immediately with a `Process`. The child's `stderr` goes to yours.
- `p.input` and `p.output`: `File` handles for the child's `stdin` and `stdout`, supporting `write`, `read_line`, `read`, `ready`, and `close`. Call `close(p.input)` to send end of input, and `close(p.output)` when done reading. Writes to `p.input` are buffered like any file and flush on `flush`, reads, `ready`, `done`, and `wait`; writing after the child exits yields a write error from `flush` rather than killing your process with `SIGPIPE`.
- `done(p: Process) -> bool`: returns `true` if the child has exited, without blocking.
- `wait(p: *Process) -> i32`: waits for the child to exit, returns its exit code, and resets `p.id` to `0`. A process can only be waited once. `wait` does not close `p.input` or `p.output`, so you can still drain remaining output; conversely, a child that reads until EOF will wait for `close(p.input)`, and a child writing more than a pipe buffer (64 KiB) will block until you read `p.output`.
- `kill(p: Process)`: terminates the child immediately; `wait` then returns `137` (`128 + SIGKILL`) on every system.
- `p.id`: OS process identifier (PID on Unix, handle on Windows). Calling `done`, `wait`, or `kill` when `p.id == 0` (never started or already waited) aborts: `flag: 'kill' called on a Process that is not running (never started or already waited)`. Waiting a copy of an already-waited `Process` also aborts on Linux/macOS (on Windows the handle may have been reused, so never copy a `Process` you wait on).

```flag
use os

func main():
    p := start(ptr(["sort"]))
    if p is FsError:
        eprint(p)
        return
    write(p.input, "pear\napple\n")
    close(p.input)
    while true:
        line := read_line(p.output)
        if not line is &strview:
            break
        print(line) # apple, then pear
    close(p.output)
    print(wait(p)) # 0
```

### Random bytes

- `random_bytes(into: &listview[u8])`: fills `into` with cryptographically secure random bytes from the OS (`getrandom` on Linux, `getentropy` on macOS, `BCryptGenRandom` on Windows), suitable for keys and tokens: `key: array[u8, 32]` and `random_bytes(ptr(key))`. If the OS cannot provide random bytes, the program aborts with `flag: cannot read random bytes: ...` rather than returning weak data. (The pseudo-random generator in [math](#random-numbers) is not for secrets.)

### Libraries

A shared library loaded at runtime, such as one compiled with `flagc -shared` (see [Libraries](Reference.md#libraries)), is represented by `Library`. Loading failures return `LoadError`, containing either the OS error (`cannot load ./game.so: cannot open shared object file: No such file or directory`) or a type-layout mismatch (`cannot load ./game.so: its State has other fields than the State of this program`).

```flag
type Library:
    handle: *bytes

type error LoadError:
    path: &strview   # the path passed to load_library
    reason: &strview # OS or type-check message, temporary
```

- `load_library(path: &strview) -> Library|LoadError`: loads a shared library using `dlopen` on Linux/macOS (resolving all symbols immediately, kept local from subsequently loaded libraries) or `LoadLibraryW` on Windows. A path without `/` searches the system library paths; write `./game.so` for the current folder.
- `symbol(library: Library, name: &strview) -> *bytes?`: returns the address of an exported function or variable, or `null` if not found. Cast function pointers with `as`: `step := trust symbol(game, "game_step") as extern func(*State, f64)`.
- `close(library: Library)`: flushes the library's output, frees the closing thread's library scratch memory, and unloads it. `with library:` calls `close` when the block exits. Unloading invalidates all code and data pointers from the library, so close it only after every thread has left its code and no references remain.

```flag
use os

func main():
    game := load_library("./game.so")
    if game is LoadError:
        print(game)
        return
    version := symbol(game, "version")
    if version is *bytes:
        print("version ", (version as extern func() -> i32)())
    close(game)
```

### Time

- `now() -> f64`: wall-clock seconds since the Unix epoch (1970-01-01), with fractional seconds.
- `clock() -> f64`: monotonic seconds from an arbitrary start; never jumps backward, so use it to measure elapsed time.
- `date() -> Date`: local calendar date and time.
- `sleep(seconds: f64)`: pauses the calling thread for `seconds` (not cut short by signals). Flushes pending file output first.

`Date` fields (all `i32`):

| Field | Range / meaning |
|---|---|
| `year` | full year (e.g. `2026`) |
| `month` | `1` to `12` |
| `day` | `1` to `31` |
| `hour` | `0` to `23` |
| `minute` | `0` to `59` |
| `second` | `0` to `59` (or `60` on a leap second) |
| `weekday` | `0` (Sunday) to `6` (Saturday) |

---

## json

Parses RFC 8259 JSON without copying the source text, and formats JSON strings.

```flag
use json

func main():
    d := parse_json("{\"user\": {\"name\": \"ana\", \"tags\": [\"a\", \"b\"]}}")
    if d is NotJson:
        eprint(d)
        return
    defer free(d)
    print(text(d, get(d, 0, "user.name"))) # ana
    for tag in children(d, get(d, 0, "user.tags")):
        print(text(d, tag))
    print(integer(d, get(d, 0, "user.age")) ifnull 0) # 0, there is no age
```

`parse_json` builds a flat list of nodes referencing slices of the input text, so the input text must stay alive while you use the `Document`. Each node is identified by its index (`i64|null`), starting at `0` for the root value. Missing or out-of-range indices yield `null` or empty results across every accessor, letting you chain lookups and check only at the end.

#### Parsing and freeing

- `parse_json(text: &strview) -> Document|NotJson`: parses strict RFC 8259 JSON. Rejects leading zeros (`01`), trailing dots (`1.`), invalid escapes, unescaped control characters/newlines in strings, unpaired surrogates (`\ud83d`), trailing commas, and nesting deeper than 512 arrays/objects. Does not validate UTF-8 bytes inside strings; use `valid_utf8` in [utf8](#utf8) if needed.
- `free(d: *Document)`: frees the node list (not the source text) and empties `d`.

#### Navigating nodes

- `get(d: Document, at: i64|null, path: &strview) -> i64|null`: looks up a key inside object `at`, following `.`-separated segments into nested objects (`"user.name"`). Keys are matched after decoding escapes. Keys containing literal `.` cannot be looked up with `get`.
- `children(d: Document, at: i64|null)`: iterable for `for`, yielding the value node indices of an array or object in order (empty for any other kind).
- `key(d: Document, at: i64|null) -> &strview`: decoded key name of object value `at` (**temporary**), or `""` if `at` is not an object value.
- `count(d: Document, at: i64|null) -> i64`: number of elements in an array or entries in an object.
- `item(d: Document, at: i64|null, index: i64) -> i64|null`: `index`-th value of an array or object. Walks from the start in O(n); use `children` to iterate all items.

#### Reading values

- `kind(d: Document, at: i64|null) -> Kind`: value kind: `Nothing`, `Boolean`, `Number`, `Text`, `Array`, or `Object`. JSON `null` and missing nodes both report `Nothing`.
- `text(d: Document, at: i64|null) -> &strview|null`: decoded string value (**temporary**), or `null` if not `Text`.
- `number(d: Document, at: i64|null) -> f64|null`: floating-point value, or `null` if not `Number`.
- `integer(d: Document, at: i64|null) -> i64|null`: integer value, or `null` if not `Number`, if it has a decimal point or exponent, or if it overflows `i64`.
- `boolean(d: Document, at: i64|null) -> bool|null`: boolean value, or `null` if not `Boolean`.
- `raw(d: Document, at: i64|null) -> &strview`: **a view** of the raw JSON slice for `at`.

Because `text`, `number`, `integer`, and `boolean` return `null` on kind mismatch or missing nodes, defaults chain cleanly: `text(d, get(d, 0, "name")) ifnull "anonymous"`.

#### Iterating object keys and writing JSON

- `quoted(text: &strview) -> Quoted`: formats `text` as a properly quoted and escaped JSON string for `+`, `+=`, and `print`.

Build JSON output with `+=` and `quoted`; Flag's standard number formatting already produces valid JSON (except `nan` and `inf`, which JSON does not allow):

```flag
use json

func show_and_build(d: Document, name: &strview, age: i64) -> &str:
    for v in children(d, get(d, 0, "user")):
        print(key(d, v), " is ", raw(d, v))
    out: &str = heap("")
    out += "{\"name\": " + quoted(name) + ", \"age\": " + age + "}"
    return out
```

#### `NotJson` error and `Document` layout

```flag
type error NotJson:
    reason: &strview
    line: i64
    column: i64
```

`line` and `column` are 1-based, with `column` counting bytes like compiler diagnostics. Printing `NotJson` outputs e.g. `not JSON at line 3, column 14: expected ':' after the key`.

`Document` exposes `text: &strview` and `nodes: &list[Node]`. Each `Node` has `kind: Kind`, `from: i64`, `to: i64` (`d.text[from:to]` is its raw JSON text), `size: i64` (total nodes covered including descendants), and `escaped: bool` (whether a `Text` node contains escapes). Inside an `Object`, each key is stored as a `Text` node immediately preceding its value node.

---

## net

TCP connections, TCP servers, and UDP datagrams. `Socket` (one TCP connection), `Server` (a listening TCP socket), and `Udp` (a UDP port) are distinct types, separate from `File`.

#### TCP connections and servers

- `connect(host: &strview, port: u16) -> Socket|NetError`: opens a TCP connection to a hostname or IP address, trying each resolved address until one succeeds (`cannot connect to example.com:80: Connection refused`).
- `listen(port: u16, host: &strview = "0.0.0.0") -> Server|NetError`: listens on `port` at `host`: `"0.0.0.0"` binds all IPv4 interfaces, `"127.0.0.1"` binds loopback only, and `"::"` binds all IPv6 interfaces. A hostname binds its first resolved address. Port `0` lets the OS pick an ephemeral port; read it with `port(server)`.
- `accept(server: Server) -> Socket|NetError`: flushes pending file output and blocks until a client connects.
- `s.peer`: the remote [Address](#udp) of a `Socket` (the address `connect` reached or `accept` received); prints as `127.0.0.1:52144`.
- `port(s: Server) -> u16`: the port `s` is listening on.
- `close(s: Server)`: stops listening (`with server:` calls `close` automatically).
- `timeout(s: Socket, seconds: f64)`: sets read and write timeouts on `s`. Exceeding the timeout returns a `NetError` with code `TIMED_OUT`. `0.0` (or less), `inf`, or any value of `1000000.0` seconds or more waits forever (the default for new sockets).
- `write(s: Socket, value: $T)`, `read(s: Socket, into: *&str, size: i64) -> i64|NetError`, `read_line(s: Socket) -> &strview|null|NetError`, `flush(s: Socket) -> NetError?`, and `close(s: Socket)` (and `with s:`): behave like the corresponding [File](#files) operations, returning `NetError` instead of `FsError`.

Writes to a `Socket` are buffered and flushed automatically on `flush` or before blocking, so a request always goes out before you wait for the reply. Writing to a peer that has closed returns `NetError` rather than terminating the program. On Windows, `net` initializes Winsock automatically.

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

#### `NetError`

```flag
type error NetError:
    code: i32         # system error number (errno)
    action: &strview  # connect to, listen on, accept a connection, read, write, open the UDP port, find, send to, receive
    address: &strview # host:port for connect/listen/send, host for address, port for udp, empty otherwise
```

`NetError` formats like `FsError`. DNS lookup failures in `connect`, `listen`, or `address` carry a negative `getaddrinfo` code; on Windows, error codes are negated Winsock codes. `TIMED_OUT` (defined in `core`) is the portable code for an expired `timeout` on every system and prints as `cannot read: the timeout ran out`.

### UDP

An `Udp` socket sends and receives datagrams, each one whole or not at all, over IPv6 with IPv4 mapping enabled, so one socket communicates with both IPv4 and IPv6 peers.

- `udp(port: u16) -> Udp|NetError`: binds `port` on all interfaces (`0` lets the OS pick a free port).
- `address(host: &strview, port: u16) -> Address|NetError`: resolves a hostname or literal IP (`127.0.0.1`, `::1`) to its first `Address`.
- `send(u: Udp, to: Address, data: &strview) -> NetError?`: sends `data` as one datagram (up to 65507 bytes).
- `receive(u: Udp, into: *&str) -> Address|NetError`: flushes pending output, waits for the next datagram, appends its payload to `into`, and returns the sender's `Address`.
- `port(s: Udp) -> u16`, `timeout(s: Udp, seconds: f64)`, and `close(s: Udp)` (and `with u:`): behave like their TCP counterparts.

`Address` has `ip: array[u8, 16]` (IPv4 addresses are stored as `::ffff:a.b.c.d`) and `port: u16`. It supports `==`, works as a map key, and prints as `127.0.0.1:5000` or `[::1]:5000`.

```flag
use net

func main():
    u := udp(5000)
    if u is NetError:
        eprint(u)
        return
    got: &str = heap("")
    defer free(got)
    while true:
        got.len = 0
        from := receive(u, got)
        if from is Address:
            _ = send(u, from, got) # echo each datagram back to its sender
```

### Waiting for many

- `ready(waiting: &listview[$T], seconds: f64 = 0.0) -> &listview[i64]` (where `@can_assign(Socket|Server|Udp, $T)`): waits up to `seconds` until at least one entry is ready to read without blocking (data or EOF on a `Socket`, an incoming connection on a `Server`, or a datagram on an `Udp`), returning the ready indices (**temporary**). `0.0` polls immediately; a negative value waits indefinitely. Elements can be any single type or union of `Socket`, `Server`, and `Udp`.
- `ready(x: $T, seconds: f64 = 0.0) -> bool` (where `@can_assign(Socket|Server|Udp, $T)`): same for a single socket, server, or UDP port.

```flag
use net

func serve(server: Server):
    waiting: &list[Socket|Server] = heap([])
    defer free(waiting)
    append(waiting, server)
    while true:
        with scratch():
            for i in ready(waiting, -1.0):
                one := waiting[i]
                switch one:
                case Server:
                    client := accept(one)
                    if client is Socket:
                        append(waiting, client)
                case Socket:
                    line := read_line(one)
                    if line is &strview:
                        print(line)
```

---

## math

Links the C math library (`-lm`).

#### Constants

- `PI`, `TAU`, and `E`: constants of `math` (in a file that uses `math` without an alias, a variable cannot take these names). Infinity is the language literal `inf`.

#### Functions

Each function comes in both `f64` (direct C math calls) and `f32` (returning `f32`) overloads. Angles are in radians, `log` is the natural logarithm, and `round` rounds halfway cases away from zero.

| Category | Functions |
|---|---|
| One argument (`f64 -> f64`, `f32 -> f32`) | `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `log2`, `log10`, `floor`, `ceil`, `round`, `trunc` |
| Two arguments (`(f64, f64) -> f64`, `(f32, f32) -> f32`) | `atan2(y, x)`, `pow(x, y)`, `hypot(x, y)`, `fmod(x, y)` |

`min`, `max`, `clamp`, `abs`, and `lerp` live in [core](#numbers). `sqrt` on a `simd` is built into the language (see [simd](#simd)) and can also be called as `math.sqrt`.

### Random numbers

`math` provides a single program-wide splitmix64 pseudo-random sequence stored in an `atomic[u64]`. Every run starts from the same state until you call `seed`, and it is **not** suitable for cryptographic secrets (use `random_bytes` in [os](#random-bytes) instead).

All threads share the generator via atomic `add` (lock-free), so no two threads draw the same step and a `seed` in `main` affects all threads. With a fixed seed, the sequence is reproducible, though which thread receives each value depends on scheduling.

- `seed(n: u64)`: resets the sequence to `n` for all threads. To vary by run, seed from the clock: `seed(u64(now() * 1000000.0))` (with `now` from `os`).
- `random() -> f64`: uniform float in `[0.0, 1.0)`.
- `random(low: i64, high: i64) -> i64`: an integer from `low` to `high`, both included, in either order.

---

## utf8

- `first_code(s: &strview) -> u32|null`: Unicode code point of the first character (`first_code("é")` is `233`), or `null` if `s` is empty or starts with invalid UTF-8.
- `count_chars(s: &strview) -> i64`: number of characters, matching the iteration count of `for c in s`.
- `valid_utf8(s: &strview) -> bool`: `true` if the entire text is valid UTF-8 (no invalid standalone bytes in `for c in s`).

Iterate characters with `for c in text`, and encode code points with `add_char` in [core](#text).

---

## bits

Fixed-size sets of non-negative integers stored as one bit per element.

```flag
use bits

func main():
    seen: Bits[2] # room for 128 numbers, 0 to 127
    seen[3] = true
    seen[70] = true
    for n in seen:
        print(n)
```

`Bits[$$words]` holds integers from `0` to `words * 64 - 1` in `words` 64-bit words.

- `b[i] -> bool` and `b[i] = on`: tests, sets (`true`), or clears (`false`) bit `i`. `i` may be any integer type; a negative value or one at or above `words * 64` aborts the program.
- `a or b` (union), `a and b` (intersection), `a - b` (difference: in `a` but not `b`), `a xor b` (symmetric difference), and `not b` (complement).
- `a == b` and `a != b`: equality comparison.
- `empty(b: *Bits[$$w]) -> bool`: `true` if no bits are set.
- `count(b: *Bits[$$w]) -> i64`: number of bits set.
- `for n in b`: yields set bit indices (`i64`) in ascending order. `print` formats as `{3, 70}`.

---

## ring

A `&ring[$T]` is a growable circular buffer (deque) that pushes and pops at both ends in O(1) without shifting elements. Rings always allocate on the heap (arenas do not create rings).

```flag
use ring

func main():
    q := ring(i32)
    defer free(q)
    append(q, 1)
    append(q, 2)
    prepend(q, 0)
    print(take(q), " ", q) # 0 [1, 2]
```

- `ring(kind: type $T) -> &ring[$T]`: creates an empty ring (**yours**).
- `append(r: *&ring[$T], v: $T)`: adds `v` to the back.
- `prepend(r: *&ring[$T], v: $T)`: adds `v` to the front.
- `pop(r: *&ring[$T]) -> $T|null`: removes and returns the last element, or `null` if empty.
- `take(r: *&ring[$T]) -> $T|null`: removes and returns the first element, or `null` if empty.
- `clear(r: *&ring[$T])`: removes all elements while keeping allocated capacity.
- `free(r: *&ring[$T])`: releases the ring's memory and leaves it empty.
- `r[i]`: element `i` from the front (readable and assignable); negative or out-of-range indices abort the program.
- `for v in r`: iterates from front to back; `print` formats as `[1, 2]`.

---

## raylib

Windows, 2D and 3D drawing, input, textures, and audio via [raylib](https://www.raylib.com).

The standard library bundles a prebuilt `libraylib.a` for Linux x86_64; on other systems, pass the folder of your own `libraylib.a` after `-cc`: `flagc game.flg -cc -Lfolder`. The module automatically links raylib's platform dependencies (`m` everywhere, `dl` on Linux, macOS frameworks `Cocoa`, `IOKit`, `CoreVideo`, `OpenGL`, and `opengl32`, `gdi32`, `winmm` on Windows).

Functions keep raylib's names and parameter order, so the [raylib cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html) applies directly. String parameters take `cstr` (pass text literals directly, or wrap a `&str` with `c_text(label)`). `GetClipboardText` returns a `cstr` that is `NULL` when the clipboard has no text; `view` reads that as empty text. `LoadFontEx` takes `0 as *i32` for raylib's default character set.

#### Raylib functions

| Category | Functions |
|---|---|
| Window | `InitWindow`, `CloseWindow`, `WindowShouldClose`, `SetConfigFlags`, `SetTargetFPS`, `SetWindowTitle`, `SetWindowMinSize`, `SetWindowSize`, `SetWindowPosition`, `GetWindowPosition`, `MaximizeWindow`, `MinimizeWindow`, `RestoreWindow`, `IsWindowMaximized`, `IsWindowMinimized`, `IsWindowResized`, `GetWindowScaleDPI`, `EnableEventWaiting`, `DisableEventWaiting`, `PollInputEvents`, `SwapScreenBuffer`, `WaitTime`, `ToggleFullscreen`, `GetScreenWidth`, `GetScreenHeight`, `GetFrameTime`, `GetTime`, `GetFPS`, `GetRandomValue`, `TakeScreenshot` |
| Drawing | `BeginDrawing`, `EndDrawing`, `ClearBackground`, `BeginMode2D`, `EndMode2D`, `BeginMode3D`, `EndMode3D`, `BeginTextureMode`, `EndTextureMode`, `BeginScissorMode`, `EndScissorMode` |
| Keyboard & mouse | `IsKeyPressed`, `IsKeyPressedRepeat`, `IsKeyDown`, `IsKeyReleased`, `GetKeyPressed`, `GetCharPressed`, `SetExitKey`, `IsMouseButtonPressed`, `IsMouseButtonDown`, `IsMouseButtonReleased`, `GetMousePosition`, `GetMouseDelta`, `GetMouseWheelMove`, `GetMouseWheelMoveV`, `IsCursorOnScreen`, `SetMouseCursor` |
| Clipboard | `GetClipboardText`, `SetClipboardText` |
| 2D shapes | `DrawPixel`, `DrawLine`, `DrawLineV`, `DrawLineEx`, `DrawCircle`, `DrawCircleV`, `DrawCircleLines`, `DrawCircleSector`, `DrawRing`, `DrawRectangle`, `DrawRectangleV`, `DrawRectangleRec`, `DrawRectanglePro`, `DrawRectangleGradientV`, `DrawRectangleGradientH`, `DrawRectangleGradientEx`, `DrawRectangleLines`, `DrawRectangleLinesEx`, `DrawRectangleRounded`, `DrawRectangleRoundedLines`, `DrawRectangleRoundedLinesEx`, `DrawTriangle` |
| Text & fonts | `DrawText`, `MeasureText`, `DrawFPS`, `GetFontDefault`, `LoadFont`, `LoadFontEx`, `LoadFontFromMemory`, `UnloadFont`, `DrawTextEx`, `MeasureTextEx` |
| 3D | `DrawCube`, `DrawCubeWires`, `DrawSphere`, `DrawPlane`, `DrawGrid` |
| Images & textures | `LoadImage`, `LoadImageFromScreen`, `ExportImage`, `GenImageColor`, `UnloadImage`, `LoadTexture`, `LoadTextureFromImage`, `UnloadTexture`, `GenTextureMipmaps`, `SetTextureFilter`, `LoadRenderTexture`, `UnloadRenderTexture`, `DrawTexture`, `DrawTextureV`, `DrawTextureEx`, `DrawTextureRec`, `DrawTexturePro` |
| Collisions | `CheckCollisionRecs`, `CheckCollisionCircles`, `CheckCollisionCircleRec`, `CheckCollisionPointRec`, `CheckCollisionPointCircle`, `GetCollisionRec` |
| Colors | `Fade`, `ColorFromHSV` |
| Audio | `InitAudioDevice`, `CloseAudioDevice`, `LoadSound`, `UnloadSound`, `PlaySound`, `StopSound`, `SetSoundVolume`, `LoadMusicStream`, `UnloadMusicStream`, `PlayMusicStream`, `StopMusicStream`, `UpdateMusicStream` |

#### Types

`Vector2`, `Vector3`, `Color`, `Rectangle`, `Image`, `Texture`, `RenderTexture`, `GlyphInfo`, `Font`, `Camera2D`, `Camera3D`, `AudioStream`, `Sound`, and `Music`, with raylib's field names converted to `snake_case`.

#### Constants

- **Colors**: `LIGHTGRAY`, `GRAY`, `DARKGRAY`, `YELLOW`, `GOLD`, `ORANGE`, `PINK`, `RED`, `MAROON`, `GREEN`, `LIME`, `DARKGREEN`, `SKYBLUE`, `BLUE`, `DARKBLUE`, `PURPLE`, `VIOLET`, `DARKPURPLE`, `BEIGE`, `BROWN`, `DARKBROWN`, `WHITE`, `BLACK`, `BLANK`, `MAGENTA`, `RAYWHITE`
- **Keys**: `KEY_A` to `KEY_Z`, `KEY_SPACE`, `KEY_ESCAPE`, `KEY_ENTER`, `KEY_TAB`, `KEY_BACKSPACE`, `KEY_DELETE`, `KEY_RIGHT`, `KEY_LEFT`, `KEY_DOWN`, `KEY_UP`, `KEY_PAGE_UP`, `KEY_PAGE_DOWN`, `KEY_HOME`, `KEY_END`, `KEY_LEFT_SHIFT`, `KEY_RIGHT_SHIFT`, `KEY_LEFT_CONTROL`, `KEY_RIGHT_CONTROL`, `KEY_LEFT_ALT`, `KEY_RIGHT_ALT`, `KEY_LEFT_SUPER`, `KEY_RIGHT_SUPER`
- **Mouse buttons**: `MOUSE_BUTTON_LEFT`, `MOUSE_BUTTON_RIGHT`, `MOUSE_BUTTON_MIDDLE`
- **Mouse cursors** (`SetMouseCursor`): `MOUSE_CURSOR_DEFAULT`, `MOUSE_CURSOR_ARROW`, `MOUSE_CURSOR_IBEAM`, `MOUSE_CURSOR_CROSSHAIR`, `MOUSE_CURSOR_POINTING_HAND`, `MOUSE_CURSOR_RESIZE_EW`, `MOUSE_CURSOR_RESIZE_NS`, `MOUSE_CURSOR_RESIZE_NWSE`, `MOUSE_CURSOR_RESIZE_NESW`, `MOUSE_CURSOR_RESIZE_ALL`, `MOUSE_CURSOR_NOT_ALLOWED`
- **Texture filters** (`SetTextureFilter`): `TEXTURE_FILTER_POINT`, `TEXTURE_FILTER_BILINEAR`, `TEXTURE_FILTER_TRILINEAR`
- **Config flags** (`SetConfigFlags`): `FLAG_FULLSCREEN_MODE`, `FLAG_WINDOW_RESIZABLE`, `FLAG_WINDOW_UNDECORATED`, `FLAG_WINDOW_TRANSPARENT`, `FLAG_WINDOW_HIDDEN`, `FLAG_WINDOW_MAXIMIZED`, `FLAG_WINDOW_UNFOCUSED`, `FLAG_WINDOW_TOPMOST`, `FLAG_WINDOW_HIGHDPI`, `FLAG_MSAA_4X_HINT`, `FLAG_VSYNC_HINT`

#### Flag additions

- `+` and `-` between two `Vector2` or two `Vector3` values, and `*` multiplying a `Vector2` or `Vector3` by an `f32`.
- `length(v: Vector2) -> f32` and `normalize(v: Vector2) -> Vector2`.

Any raylib function not listed above can be declared directly in your own `extern "raylib":` block.
