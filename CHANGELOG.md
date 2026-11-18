# Changelog

## 0.6.0 (not released yet)

Changed
- What goes to the C compiler goes after `-cc`, until the `--` of `run` or `test`: `flagc game.flg -cc vec.c -lm`. An argument that flagc doesn't know is an error that gives the command line it expected, `run has to be the first argument: flagc run -memory hi.flg`, where before it went to the C compiler and `flagc -memory run hi.flg` ended in `ld: cannot find run`
- A test is a `func test_name()` with no parameters and no result. A function whose name starts with `test_` and that has parameters or a result is a helper, where before it was an error
- `error:`, `warning:` and `note:` are in color only when the errors go to a terminal, and never with `NO_COLOR`
- The indentation can mix tabs and spaces when the blocks don't depend on how wide a tab is, like in Python: a tab goes to the next column that is a multiple of 8 and also counts one, and when the two measures give different blocks it's an error, `this indentation mixes tabs and spaces, and its block depends on how wide a tab is`. Before, a tab and a space counted one each, so `\t\t` and `\t ` made one block and `\t` and 8 spaces didn't
- A text literal can cross lines, with its line breaks and the indentation of its lines, and a `\r\n` of the file is a `\n` in it. A `\` before a line break is an error, `'\' cannot continue a line, a text literal keeps its line breaks`, and a text that never closes expects its `"` before the end of the file
- `not` goes between `and` and `is`, like in Python, where it was tighter than every operator: `not a == b` is `not (a == b)`, and `if not line is &strview:` narrows without parentheses. On the right of a tighter operator it goes in parentheses, `not is looser than ==, write (not y)`, and a `not` followed by arithmetic, `as` or a shift is an error that gives both readings: `not x + 1 needs parentheses, write (not x) + 1 to flip the bits of x, or not (x + 1)`
- The comparisons are one level and don't chain: `a < b < c`, `a == b == c` and `a < b == c` are errors, `comparisons do not chain, write a < b and b < c, or (a < b) < c`. Before, `<` was tighter than `==`, and with three `false` `a == b == c` compiled to `false`
- `complement(b)` of `bits` is `not b`
- A `==` or `!=` with a void type that the other side can never be is an error, like `is`: `Green|Blue can never be Red, so this is always false`, where before it was `no overload of == matches (Green|Blue, Red)`
- A test of bits without parentheses says why it fails and writes them, `and is looser than !=, so it mixes the bits of u32 with a bool: write (flags and MASK) != 0`, and any other integer next to a `bool` is `or expects two integers or two bool but got u32 and bool`. Before both were `no function named and is visible here`
- `try get().x` says that `try` takes the whole chain, `try applies to the whole of get().x, and Bad|P has no field x: write (try get()).x`, and so does `trust`, where before the error spoke of narrowing
- `try` on a union with `null` and no error proposes how to pass the `null` up: `null|i64 never fails, there is no error to try for: pass the null up with ifnull return null`
- `wait` is a function of core written in Flag, `wait(t: Thread[$T]) -> $T`, and no longer a keyword or an operator: `wait t` is `wait(t)`, and `wait a + wait b` is `wait(a) + wait(b)`. `x := wait(t)` of a `Thread[null]` is `null`, where it was `a function that returns nothing is a statement, not a value`, so a generic function can `return wait(t)` for any thread. `join` is no longer a protocol: a type of yours is waited by declaring a `wait` for it, and the `Process` of `os` has `wait(p: *Process) -> i32`, that was `join`. A `wait` for a `Thread` cannot be declared, `wait of Thread belongs to the language, it cannot be declared again`, and `wait` of a value that is not a thread is `no overload of wait matches (i64)`
- `assert` is a statement and a keyword, no longer a function of core: `assert c` and `assert c, message`, where it was `assert(c, message)`. It narrows what follows like `if not c: panic(...)`, so `assert f is File` leaves `f` a `File` and `assert x != null` leaves `x` without `null`. Its message says what was false and what was there, where it was `assertion failed`: a comparison gives the sides computed when the program runs, `total == 4 is false, total is 3`, `x is T` what `x` holds, `f is FsError, not File: cannot open config.txt: No such file or directory`, and any other condition itself, `ready is false`. The message after the comma can be any value that `print` writes, and runs only when the condition is false. Unlike `panic`, `assert false` doesn't end the path
- `reverse` is a keyword like the others, and only goes after the `in` of a `for`, so it cannot name a field, a function or a type either. The function of core that turns a list around in place is `flip(items)`, that was `reverse(items)`, and `reverse(l)` says so: `reverse goes only after the in of a for, as in for x in reverse items, and flip(items) turns the items around in place`
- In `for i, x in reverse l` over an array, a list or a slice, `i` is the index of `x`, so it goes down: `2 30`, `1 20`, `0 10`, where it counted the rounds from 0. Over text and over what `next` gives, `reverse` with a counter is an error, `reverse over &strview has no index to give i, only an array, a list or a slice knows the index of each element: count in a variable`
- A `(` right after a name makes a call, and after a keyword it goes after a space. `twice (3)` is `a call writes its ( right after what it calls: twice(3)`, and `not(x)`, `if(x):`, `assert(x)` and `for x in reverse(l)` are `'reverse' is a keyword of the language, not a function, so a space goes after it: reverse l`. Before, a space inside a line never changed anything. The name of an operator keeps its `(` right after it, `operator and(a: Set, b: Set)`
- A call with `@` is one the compiler answers while it compiles, so `sizeof`, `offset_of` and `can_assign` are `@sizeof(T)`, `@offset_of(T, field)` and `@can_assign(A, B)`, next to `@here()` and `@embed`. The old spelling says so, `sizeof is a builtin of the compiler, write @sizeof(i64)`, and the three names are free for functions of yours. The operations of a `simd` and the values `true`, `false` and `null` take no `@`
- `ptr`, `print` and `eprint` belong to the language, so declaring a function, a type or a constant with one of those names is an error, `print belongs to the language, it cannot be declared again`, where before a `func ptr` of yours was quietly never called
- A `$T` or a `$$n` in the result of a function and in none of its parameters is an error in the signature, also when nothing calls the function, where before it failed only once something named it: `$T is not bound here, a generic gets its type from a parameter: take it as kind: type $T`
- `@linux()`, `@macos()`, `@windows()`, `@x86_64()`, `@arm64()`, `@riscv64()` and `@can_assign(A, B)` are a `bool` that the compiler knows, in any expression: `if @windows():`, `wide := @x86_64() or @arm64()`. An `if` on them is an `if` like any other and checks both branches on every system. Before they went only in a `where`, `@windows() cannot appear outside a where, it chooses between overloads`
- A local variable, a parameter, the variable of a `for` and its counter cannot take a name that is visible there: another local variable, as before, and now also a constant, a file variable or a context, of your files, of core like `stdout` or of a module used without an alias like `E` of `math`. `N := 5` with a constant `N` is `N is a constant, a variable cannot take its name`, with a note where `N` is declared, where before the variable hid the constant. Functions and types don't count, so `twice := 4` still leaves `twice(3)` to the function, and the library doesn't see the names of your files for this
- `inf` and `nan` are keywords, so a variable called `inf` or `nan` needs another name
- The `where` of an `extern` block reads only the system and the processor, like a `where` block at the root, and the functions of a block that doesn't hold don't exist: calling one is `no function named GetLastError is visible here`. Before each function took the `where` of its block, which could read anything a `where` reads
- A float literal too big for `f64` says so and names infinity, `1e999 does not fit in f64, the largest f64 is 1.7976931348623157e+308: write inf for infinity`, and arithmetic between literals that is not finite is `the result of this literal arithmetic is not finite: write inf or nan`. Both were `this float literal has no finite value`
- The messages say `expects` and not `takes`, and don't talk to the reader: `this func(i64) expects i64 here but got i32`, `no overload of show is func(f64), a function value is the overload with exactly those parameters`, `close of Lock can fail, and with would drop the error: call close without with and handle what it gives`, and `-memory` says `level goes by value to make, a copy of an Arena: declare the parameter *Arena`
- A float goes to QBE as its bits, an integer that QBE reads in a float place, where it went as 17 digits that QBE read back: the number is the same, `inf` and `nan` go too, and an `f32` literal is the `f64` rounded once
- Two integers of different types mix when one holds every value of the other, and the operation is done in the wider one: `u8 + i32` is an `i32`, and `x += y` works with `x: i64` and `y: i32`, where every mix was an error. `u8 + u8` is still an `u8`, and a pair where neither holds the other is `+ cannot mix u64 and i64, neither holds every value of the other: convert one side with u64(...) or i64(...)`
- Comparisons take any two integers and compare their values: `i64(-1) < u64(0)` is `true`, and `i < l.len` works with `i: i32`, where it was `< expects two numbers of one type`. A literal still has to fit the other side
- The result of `<<` and `>>` has the type of the left side, and the amount can be of any integer type: `u8(1) << n` with `n: i32` is an `u8`. A literal amount keeps its own type and no longer has to fit the left side
- An integer argument grows like in an assignment: `f(x)` with `x: i32` calls `func f(n: i64)`, also through a function value, and an integer grows into a union, in a call, an assignment or a `return`: `y: i64|&strview = x`, `return x` in a function that returns `i64?`. The exact type still wins, and between two growths the shorter one: an `i16` goes to `f(i32)` before `f(i64)`. A call that went to a generic `g(v: $T)` can go to a `g(n: i64)` now
- A bare `$T` that integers give takes the widest of them, in any order: `biggest(a, b)` with `a: i32` and `b: i64` is an `i64`, and `pair(i32(1), u8(2))` matches `pair(a: $T, b: $T)`. A type with a `$T` inside, like `&list[$T]` or `type $T`, fixes `$T` first, and a value for a bare `$T` converts to it: `append(l, k)` with `l: &list[i64]` and `k: i32`, and `check(u8(3), i64)` with `check(x: $T, kind: type $T)`, which were errors
- An overload is chosen argument by argument: it wins when it fits each argument at least as well as every other overload and one better. Before the scores of the arguments were added, so a call that one overload won by one argument and lost by another is ambiguous now: `ambiguous call to f, no overload matches (i32, i32) at least as well as the others in every argument`
- The element type of an array literal leaves out an integer that grows into another one in it: `[a, b]` with `a: i32` and `b: i64` is an `array[i64, 2]`, where it was an `array[i32|i64, 2]`
- An `operator +` between two numbers of different types says why it never runs, `the language handles every pair of numbers`, where it said that they don't mix
- An integer that does not grow to its place says how to convert it, `cannot assign i64 to i32, an integer only grows by itself: convert it with i32(...)`, where it said `and no operator = makes i32 from it`. `x += y` with `x: i32` and `y: i64` is that error now
- An integer literal with no place that gives it a type is an `i64`, or an `u64` above the largest `i64`, like the `int` of Go, where it was an `i32`, or an `i64` or `u64` when it didn't fit. So are `'a'`, `@sizeof`, `@offset_of`, the `len` of an array, a `$$n`, the variable of a range with two literal ends, the elements of `[1, 2]` and the keys and values of `{1: 2}`. A variable takes the type of its literal, so `n := 5` followed by `f(n)` with `func f(x: i32)` is `no overload of f matches (i64)`: write `n: i32 = 5`, or `f(i32(n))`. `hex(-1)` gives 16 `f`, `1 if c else 2` is an `i64`, and `f(ptr([1, 2]))` is a `*array[i64, 2]`, that goes to a `&slice[i64]` and not to a `&slice[i32]`
- An integer literal goes to an integer before a float, and among integers to `i64` if there is one, and else to the first where it fits, in the order `i8`, `i16`, `i32`, `u8`, `u16`, `u32`, `u64`: `x: i32|f64 = 3` is an `i32`, `f(3)` with `f(i32)` and `f(f64)` calls `f(i32)`, and `x: u8|i8 = 5` is an `i8`, where it was `5 fits more than one member of i8|u8 equally`
- In the extra arguments of a C function with `...`, an integer literal goes as an `i32`, the `int` of C, when it fits: `printf("%d", 7)`
- With no error, data that goes to C as bytes changes size: `values := [5, 3, 9, 1, 7]` is an `array[i64, 5]` now, so `qsort(ptr(values) as *bytes, 5, 4, descending)` with a `descending(a: *i32, b: *i32)` sorts the halves of each number and prints garbage. Give the array the type C reads, `values: array[i32, 5] = [5, 3, 9, 1, 7]`, also for `memcpy` and for bytes written to a file
- Between two integer types, `a if c else b`, `ifnull` and `iferror` give the wider one in either order: `small if c else big` with `small: i32` and `big: i64` is an `i64`, where it was an `i32|i64` and the other order an `i64`. The two ends of a range of different integer types take the wider one too, `for i in a..b` counts in `i64`, where it was `a range expects both ends of one type`, and when neither holds the other it's `a range cannot mix i32 and u32, neither holds every value of the other: convert one end with i32(...) or u32(...)`

New
- `./build.sh cross` compiles flagc, the tests and the examples for `linux-arm64`, `linux-riscv64` and `macos-x86_64`, the targets of the second class, and assembles them. CI runs it on every change
- Tests that pin the order of evaluation: left to right, named arguments as written and then the default values, and the cases of a `switch` until one matches
- `a..=b`, the range that ends at `b`, in a `for` and in a `case`, with the precedence of `..`. The loop never computes `b + 1`, so it reaches the end of the type: with `last: u8 = 255`, `for b in 0..=last` runs 256 times, where `0..last + 1` didn't run, and `case 128..=255:` fits a `u8`
- `not` of an integer flips its bits with the width of its type, `not u8(1)` is `254`, and `not` of a literal takes the type of its place like the literal does: `mask: u32 = not 0xff` is `0xffff_ff00`
- `operator not`, with one parameter, the only unary operator a type can declare
- `a if c else b`, the loosest operator. `c` runs first and then only the side it chose, `a` sees what `c` narrowed and `b` what `not c` narrowed, and it nests to the right. Its type follows the rule of `ifnull`, and a side can be a call that never returns: `n := given if given is i64 else panic("no number")`
- `return`, `break` and `continue` go as the last link of `iferror` and `ifnull`, and leave like a call that never returns, so the value has no error or `null` after them: `cut := find(line, "=") ifnull return BadLine{i + 1}`, `n := parse_int(text) ifnull return null`, `total += parse_int(text) ifnull continue`. A default value cannot hold them, it runs in the caller: `a default value runs in the caller, so return would return from the caller: write a value after ifnull`
- When `a ifnull b` or `a iferror b` is a whole statement, `b` can be an assignment, that runs when `a` failed: `parse_int(text) ifnull bad += 1`. Where a value is needed it's an error, `an assignment on the right of ifnull is a statement of its own, n would be left without a value`
- `x == null` and `x != null`, and `==` and `!=` with any void type of the union, narrow like `is`, in both orders: `if x != null:` narrows the block, `if x == null: return` leaves `x` without `null`, and `c == Red` narrows an enum
- A parameter `kind: type $T` takes a type, so a call gives a `$T` by hand: `func make(kind: type $T) -> $T` and `p := make(Point)`. The argument is any type, `make(&list[i32])`, `make(Box[u8])` or `make(Car.motor)`, it takes nothing when the program runs, and its name is not a value. It goes only in a parameter of a `func` with a body, as a bare `$T` with no default, the function has no function value, and it's chosen like a `$T`, so a void type, which is also a value, goes to both and `make(Red)` with the two overloads is ambiguous. `$$n` is not given by hand
- A call takes a type as an argument, written like anywhere else: `@sizeof(i32?)`, `@can_assign(Socket|Server, $T)` or `make(Pair[i32, f64])`. In the place of a value a type is an error, `&list[i32] is a type, not a value`, and the `&x` and `*p` of C write what Flag has, `'&' is not an operator, write: ptr(x)` and `'*' does not go before a value, write: p.*`
- `where c:` at the start of a line opens a block that exists only where `c` holds, and elsewhere it's read but not checked or generated, like a function whose `where` doesn't hold. At the root it holds declarations, so each system can have its own type `Stat` or its own value of a constant, and its condition reads only the system and the processor, decided before anything is declared: `a where outside a function reads only the system and the processor, like @linux() and not @arm64()`. Inside a function it holds statements, so the code that calls a C function of one system goes there and not in an overload, its condition reads what any `where` reads, the `$T` of a generic too, and a block that doesn't hold can follow a `return`. There is no `else`. `where := 6` and `where: *i32` are still names
- `inf` and `nan` are float literals, written as `print` writes them and `parse_float` reads them, with the value the compiler knows: `LOW = -inf`, `x: f32 = inf`, `best: f64 = inf` at the root. `inf - inf` is `nan`, `nan == nan` is `false` like when the program runs, and arithmetic between finite literals that gives one of them is still an error

Fixed
- `-memory` follows the temporaries of a thread through a function that waits for it: `s = finish(t)` inside a `with scratch():`, with a `finish` of yours that returns `wait(t)`, and `print(s)` after the `with` warns `s points to memory that may be freed already`, like a `wait` in place did
- QBE stopped with an assertion on rv64 when a big function spilled the condition of a branch more than 2 KiB below its frame pointer, and so did flagc with `-target linux-riscv64`: the address of the slot goes through a register, as for a load. QBE has forty-seven changes
- `-memory` took the text `join` of core for the wait of a thread, so it lost the temporary that `join(words, "-")` gives, and a `with scratch():` that released it before a read said nothing
- `func f(a: i32, a: i32)` compiled, and `f(1, 2)` gave 2: two parameters with one name are `variable a is already declared and still in scope`, and so is `for x, x in l`, where the counter hid the element
- A type with a `$T` inside matches only the type it names: `count(m: &map[$K, &strview])` took a `&map[&strview, &str]` and then failed with `cannot assign &map[&strview, &str] to &map[&strview, &strview]`, and `*$T`, `array[$T, $$n]` and `Thread[$T]` took another type than the `$T` that a parameter before them bound. Now they are `no overload of count matches (&map[&strview, &str])`

Faster
- Checking the compiler takes 4 % fewer instructions, 918 M where it took 958 M: a call to a function without `$T` makes its instance without matching its arguments again, which choosing the overload already did

## 0.5.0

A release about reaching past the program: numbers in lanes of one register, libraries that a program loads and reloads while it runs, the functions of C with their types, other programs that run beside yours, UDP, and tests that run each in a process of its own. Structs, `switch` and signed indexes got faster, and Windows takes text in UTF-16 everywhere.

New
- `simd[T, n]`: n lanes in one register of 16 bytes, with operators that work on every lane, comparisons that give a mask `simd[bool, n]`, `select`, `any`, `all`, `sum`, and `min`, `max`, `abs` and `sqrt` on each lane. On x86_64 and arm64 they are instructions of the processor and the values stay in registers: a contact solver in 4 lanes runs at 1.46 ns per contact, next to the 1.36 of C with SSE, where one number at a time takes 5.3
- A lane is read and changed with a constant index, `v[2]`, a literal takes the type of its place and can shuffle lanes, and `T(x)` converts: a number to every lane, an array to a `simd` and back, and lane by lane between two `simd`
- `Name[a, b](x)` converts to a type with two parameters, like `simd[f32, 4](x)` and `array[f32, 4](v)`
- `clamp(v, low, high)` takes a `simd` and two numbers
- `flagc test` runs the tests of your files, every `func test_name()`, each one in a process of its own, as many at once as there are cores, so a failed `assert`, a crash or a test that never ends fails only that test. A test reads an empty input, its output and errors come back through a pipe, and a test whose `where` doesn't hold on the target is not built. The words after `--` choose the tests by name, `-timeout` gives the seconds each one can take, 10 by default and up to 2000000, and a test out of time says where it was, with the chain of calls. At the end it writes each test that failed, with its place, its exit code and what it wrote, and `3 passed, 1 failed`, and exits with 1 when one failed. `-o` keeps the program of the tests, that runs the test it names, for a debugger
- `@embed("file")` puts a file in the program as a `&strview` of static data, read when it compiles from the folder of the source file, its path a text literal with its escapes. The bytes go to the assembler with `.incbin`, so 1.5 MB compile in 0.06 s
- `-> never` says that a function never returns, like `panic` and `exit`. The code after a call to it can never run, it narrows like a `return`, so after `if data is FsError: panic(...)` the variable is the value, and on the right of `ifnull` or `iferror` it leaves the value without the `null` or the error: `port := parse_int(text) ifnull panic("not a number")`
- `a.len` of an array is its count, an integer literal like `sizeof`
- `at`, `contains` and `remove` of a `&map[&str, V]` take a `&strview` too, so `m["port"]`, `m[line[:4]]` and `remove(m, "port")` work
- `-shared` builds a library that a program loads while it runs, on Linux and macOS as on Windows: no `main`, its `export` functions are what it gives, and on Linux its contexts are initial-exec thread data and it links with `-fPIC -Wl,-Bsymbolic`. Its name is `a.so`, `a.dylib` or `a.dll` without `-o`
- A library starts on its own: the first time a thread calls one of its `export` functions, its contexts get their tags, and the first time of all its file variables. An `export func` of a program does the same for a thread that C starts, which saw its contexts without tags
- A library doesn't catch crashes, the program that loads it does, with the chain of its calls
- `load`, `symbol` and `close` in `os`, with `dlopen` and `LoadLibraryW`, a `Library` and a `LoadError` with the words of the system: `cannot load ./game.so: cannot open shared object file: No such file or directory`
- `tests/reload.flg`: a program builds two versions of a library, loads the first one, replaces it with the second and keeps its state
- `extern func(T) -> U`, the type of a function of C: a call through it passes structs the way C does, it crosses to C as a parameter, a field and a variadic argument, and the name of a C function, of an `extern` block or an `export func`, is a value of it. Typed callbacks, the loaders of Vulkan and OpenGL and the tables of COM need no `*bytes`
- A function of an `extern` block takes the name of C after `=`: `func send_rect(self: *bytes, sel: *bytes) -> Rect = "objc_msgSend"`, one signature for each way you call a C function
- `offset_of(T, field)`, the place of a field as an integer literal, like `sizeof`
- Programs that run beside yours, in `os`: `start(program)` gives a `Process` whose `input` and `output` are files, `done(p)` asks if it ended, `wait p` waits and gives its exit code, and `kill(p)` ends it. Writing to a program that ended is a failed write, not a signal that ends yours
- `ready(f, seconds)` and `ready(files, seconds)` say which files have something to read, waiting up to that long, and `ready` of `net` does it for sockets, servers and UDP
- `random_bytes(into)` fills bytes from the system, `getrandom`, `getentropy` or `BCryptGenRandom`, for keys and tokens
- `modified(path)` of `fs` and `Status.modified`, the time a file last changed, in seconds with their fractions
- UDP in `net`: `udp(port)`, `address(host, port)`, `send`, `receive` and an `Address` for IPv4 and IPv6
- `try_reserve` of a list, a text, a map and an `Arena` gives `NoMemory` when the system has no more memory, where the program stopped, and `reserve` takes a map and an `Arena` too
- `wstr(text)` and `temp(w)` turn text into the UTF-16 of Windows and back, and `windows_errno(code)` gives the errno of an error of Windows

Changed
- The operators of arrays are gone, except `==`: an array holds values, and arithmetic on lanes is a `simd`. `a + b` between two arrays now says so
- `abs` of a float clears the sign bit, like `fabs`, so it also takes the sign off a NaN
- On x86_64 a program whose code names `simd` needs x86-64-v2 (SSE4.2) and says so when it starts
- Arenas give pieces aligned to 16, and a stack slot aligned to 16 is aligned again: QBE could lay it out 8 bytes off after smaller ones. A `simd` in a tagged union goes at byte 16, so `simd[f32, 4]?` takes 32 and the `simd` is aligned there too
- `hash` of a `simd` is an error that says why, `simd[i32, 4] has no hash, its == answers each lane: use an array, array[i32, 4](v)`, so a map with `simd` keys says it first. So is the `==` of a struct, a union or an array that holds `simd`, and an `operator` between a `simd` and a number, that belongs to the language. An `operator` between a `simd` and a type of yours is yours, like `m * v` of a `Mat4`
- `select`, `any`, `all`, `sum`, `min`, `max`, `abs` and `sqrt` with a `simd` are the language's with their number of arguments and no module, or `math`. With other arguments a function of yours with that name is called, like a `max(v)` that gives the largest lane
- `never` is only what a function returns: a variable, a parameter or a member of a union cannot be `never`
- `not (x is T)` narrows, like the `else` of `x is T`
- An array literal takes its element type from a `*array[T, N]` or a `&slice[T]` through `ptr`, so a constant of integers can be a `&slice[u32]`: `words: &slice[u32] = ptr(SHADER)`
- `[...]` takes the one array or `simd` of a union, and `{k: v}` its one map, like `{...}` takes its one struct: `v: simd[f32, 4]? = [1.0, 2.0, 3.0, 4.0]`
- Your `main` is the `main` of C, so `break main` stops once, in your code, and nothing in the program can call `main` or take it as a value
- `panic`, `exit`, `out_of_memory` and `out_of_bounds` return `never`
- The tag of a union member is a hash of its type, the same in every program and library, where it was the id of that compilation: a union crosses between a program and a library it loads. The builtin types keep the first tags, `null` is still 0, and two types whose hashes meet can't be in one union
- A function whose `where` doesn't hold on the target is not checked, it's checked when you build for its system: code of one system can call helpers that only that system has
- The `where` of an `extern` block holds for each of its functions, not only for its library: compiling for Windows takes the Windows signature
- Two functions with the same parameters in one module are `declared twice`, also when they return different types or, in an `extern` block, call different symbols: no call could choose between them. An `extern` block dropped the second one without a word, and with different parameters they are overloads
- The name of a C function is a value of an `extern func` type, where it was an error, and the error of a call through a function value says `this func(i64) takes i64 here but the call gives i32`
- `wait x` of a value that is not a thread calls `join(x)`, so `wait p` waits for a `Process` and a type of yours can be waited too. A `Process` is waited once, like a thread: on Linux and macOS waiting a copy waited already stops the program
- On Windows text goes to the system in UTF-16: files, folders, the environment, the arguments, other programs, the messages of errors and the paths of the compiler take letters outside ASCII, where they went through the code page of the system. `rename` gives an errno, where it gave the code of Windows
- `run` and `output` of a command start `/bin/sh -c` or `cmd.exe /d /s /c` the way they start a program, so a program you run gets only its three files, also when another thread opens a file or starts another program at that moment: Linux opens pipes and sockets close on exec, macOS starts programs with `posix_spawn` and `POSIX_SPAWN_CLOEXEC_DEFAULT`, and Windows with `CreateProcessW` and the list of the three handles, where `_spawnvp`, `system`, `popen` and a `dup2` of the standard output were not safe with threads
- The parameters of an `extern func(...)` type and what it returns are checked where the type is written, like those of an `extern` function, and a `= "symbol"` is a C name of letters, digits, `_`, `.` and `$`, where any text went to QBE and failed there
- `-memory` warns about a thread that runs alone in a library, where nothing waits for it and the program may close the library while it runs
- `-memory` names the function as you wrote it, `release cannot release p`, where a C function with a symbol of its own was named `free`
- `test.sh` gives the program of a test the compiler in `FLAGC`
- gdb goes from the line of a `switch` on a union to the case that runs, the search for it is on the line of the `switch`
- QBE has fifteen more changes, forty-six in all: the class `v` and its operations for amd64 and arm64, with 16-byte pieces of `v` in the cut of small slots, whose lanes are read and written in the register, `cpuid`, slots of `alloc` laid out to keep 16 aligned, structs aligned to 16 on arm64, `extern thread` on arm64 and rv64, the offset of an address from the GOT added after it is read, copies of 4 bytes out of stack slots that are not a multiple of 8, its files opened with `_wfopen` on Windows, and the fixes below

Faster
- `v[i]` reads or writes one lane of a `simd` in its register, with one instruction on x86_64 and arm64, where it kept the `simd` in memory in all its function: a loop that sets a lane and then multiplies the whole `simd` is 4 times as fast, and one that reads two lanes 2.9 times. A shuffle in a literal, `sum`, `any`, `all` and the operations that go lane by lane, like `*` of `i64`, take the lanes the same way: a loop on the lanes of a `simd[i16, 8]` is 6.7 times as fast
- `x[i]` with a signed index costs about what it costs with `usize`: a negative index is turned around with two instructions and no jump, and the counter of a range that starts at a literal that is not negative is never negative, so `for i in 0..n` with `n: i64` indexes like `usize`. A loop over a million elements with an `i64` index takes 0.76 ms, it took 1.99
- A struct or array literal that gives its values in order puts small values together in a word only when they are all constants, which costs nothing, and writes a word with a computed value field by field, like a literal with names: positional literals take what named ones take, 24 ms where they took 66, and `json` parses as fast as before
- The inliner measures a function by what is left after its small slots are cut into registers, without the loads, stores and copies of small structs, and inlines up to 35: the integration of rigid bodies takes 155 ns per body, it took 400, 2.4 times C where it was 5.9, and the compiler builds itself with 1.4 % more instructions
- A `switch` on a union finds its case with a binary search on the tags, and compares up to 8 one after another at the end: walking a tree of 120 kinds of node is 10 % faster
- A copy out of a stack slot whose size is not a multiple of 8, a struct without fields of 8 bytes, goes 4 bytes at a time and doesn't wait for the stores that wrote it: the poses of a skeleton are 17 % faster

Fixed
- `x := f() iferror ""` and `ifnull ""` on a `&str` took the text made from the literal into a place that was not zero, so in a loop it wrote into the block of the round before and freed it twice
- An `operator +=` of a union that holds `&str` took the text that `+` made inside it and called itself until `flag: stack overflow`: text goes into text as text
- On Windows a parameter that comes on the stack, the fifth and the ones after it, was read as an integer, so an `f32` or `f64` there ended in a register of integers and the assembler refused the program
- `reserve` of a size that a 64-bit number can't hold wrapped around and wrote outside the list: more than the system can give at once is out of memory
- `accept` and `receive` with a timeout on Linux and macOS give `TIMED_OUT` when the time runs out, like a `Socket` did
- QBE on arm64 stopped with an assertion for a field of a context past 4 KiB, rv64 stopped with `no match for stores(s)` when a float went from a register to the stack and refused a shift of a `w` by a constant of 32 or more, and the register allocator stopped with `no more pm slots` with more than 64 values alive in a loop, like 64 floats or 16 `simd` on rv64, and arm64 with `invalid class` with more floats alive than its registers
- A positional literal whose fields share a word, like `Narrow{x, bump(ptr(x))}` with two `i32`, read `x` after `bump` changed it, so it gave another value than with names or with fields of 8 bytes
- `simd[f32, 4](m)` of a mask gave -1.0 for true on x86_64 and arm64, and `simd[u8, 16]([..., 300])` cut 300 to 44 where it is an error
- `start` with the standard input of yours closed gave the program no input

## 0.4.0

A release about threads, arenas, speed and failing well: a program uses every core with two words, `parallel` and `wait`, memory can be released all at once, with the same lists, text and maps as always, small functions and structs cost what they cost in C, and a program that fails says the chain of calls that led there. Flag still checks what it checked, the types, the errors that can't be dropped and `-memory`.

New
- `parallel f(x)` runs the call in a new thread and gives a `Thread[T]`, `wait t` waits for it and gives what `f` returned, and `done(t)` asks without waiting. The call is chosen like any other, the arguments are copied to the new thread, and the result can be text you own, an error to handle or a temporary, that `wait` takes into the scratch of the thread that waits. `parallel f(x)` alone on its line lets the thread run on its own, and `main` waits for it when it ends, before its deferred statements
- A thread you keep must be read, a function that can fail can't run alone, and `-memory` warns about a thread never waited, waited twice or used after `wait`, about a pointer to a local variable that leaves with a thread, also one that runs alone outside `main` or one sent through a channel, about a temporary that its `with scratch():` may release while a thread still uses it, and about using what you sent through a channel. `send` gives what the value owns, not its views
- `Lock`, that works at zero and is taken with `with lock(x.lock):`, `Channel[T, N]`, a bounded queue of at least one value that lives where you declare it, with `send`, `receive`, `close` and `for v in c`, and `cores()`
- Variables at the root of a file, `count: i32` or `names: array[&strview, 3] = ["a", "b", "c"]`, shared by every function and every thread. They start at zero or at a value the compiler knows, in the data of the program, so a table that never changes is not built again each time. One cannot have the name of a function of its module, like a constant
- `Arena`, a struct of core that starts empty and gives out pieces of big blocks. `arena(a, v)` copies a value into it and gives a pointer, like `heap(v)`, `arena(a, "text")` gives a `&str`, and `arena(a, [])`, `arena(a, [1, 2])`, `arena(a, {})` and `arena(a, {k: v})` build a list or a map that lives in the arena, with the type of the place where they land. `free(a)` releases everything and `clear(a)` releases everything and keeps the blocks, for the memory of each frame. A small allocation takes 76 instructions in an arena, and 400 with `heap` and `free`
- A `&str`, `&list` or `&map` that lives in an arena grows there, a text or a list in its place when it's the last piece of the block, and its `free` leaves it empty without releasing anything, so a function that frees what it gets works with any list
- `-memory` knows arenas: what lives in one is not a leak, using it after `free` or `clear` of its arena warns like a temporary after its `with scratch():`, also through a function, `free` of a pointer from an arena says `free cannot release boss, it lives in level, free(level) releases it`, and an arena that is never freed is a leak
- `-memory` warns about a copy of an `Arena`, a `Lock` or a `Channel` in use, also inside a struct, and about a function of yours that takes one by value: `this copies level, an Arena that may have given memory: the copy gives out the same blocks, use a pointer to it`
- `memset` with the C functions of core
- `inline func` and `inline operator` replace every direct call with the body of the function, whatever its size, also in each instance of a generic one. A recursive function, the `main` that starts the program, an `export` function and a C function cannot be `inline`, and the error of a recursive one points to the call that leads back
- A runtime error writes the chain of calls after its place, one line for each function of yours with the line where it's declared: `in parse_line, main.flg:9`. The library is left out, a function called again and again is one line with the count, after 20 lines the rest is counted, and an error in a thread writes the chain of that thread. When two threads fail at once, one report comes out whole
- A crash says what happened and writes the chain too: `flag: invalid memory access at 0x0`, `flag: stack overflow`, `flag: illegal instruction` and `flag: arithmetic error`, in any thread, also inside a function of C like `strlen`. On Linux and macOS a signal handler runs on a stack that each thread keeps for it, and on Windows a vectored exception handler. It asks for no memory and waits for no lock, so a crash inside `malloc` is told too. The exit code is 128 plus the signal, or the code of the exception on Windows
- `-debug` puts DWARF in the program, for gdb and lldb: the variables in their blocks, the types (structs, enums, tagged unions as variant parts, fat pointers, threads, function values), the variables of the files, the contexts as thread-local variables, the lines and the frames, and the names of your structs, enums and aliases as typedefs. A function that takes or returns a struct, a union, an array or a fat pointer is marked `DW_CC_nocall`, it doesn't pass them like C, so gdb doesn't call it. `break panic` stops at a runtime error. Nothing is inlined in it. On macOS it runs `dsymutil` and leaves `program.dSYM`
- `examples/threads.flg`, the Mandelbrot set drawn by a thread for each processor, and primes counted by workers that share a channel
- `examples/arena.flg`, the enemies of a level and the memory of each frame in two arenas

Changed
- A `context` belongs to each thread: a new thread starts with its contexts at zero and its own scratch memory, and the random numbers of `math` start again in each one
- `parallel` and `wait` are keywords
- `-memory` names the fields you wrote, `free cannot release level.boss`, where it named only the variable, `level`
- `print` formats all its values into one line and writes it at once, so the lines of two threads never mix. An `eprint` inside the `+=` of a value now comes out before the line that prints that value, and `write(f, value)` formats the value before the file takes it
- Files can be used from any thread: each one has a lock for writing and another for reading, so a thread waiting to read a connection doesn't stop the ones that write. A read, `flush()`, `exit` and a crash leave a file that another thread is writing to that thread, so a thread stuck sending stops nobody else
- On Windows a context takes room in the program file, thread data of Windows keeps its zeros there
- A Linux program links with `-lpthread`
- `&str`, `&list` and `&map` take 32 bytes: a fourth word, `arena`, says where their memory lives, zero for the heap. The views, `&strview` and `&slice`, stay at 16
- The scratch memory of the temporaries is an `Arena`
- Every function keeps a frame pointer, on x86_64 too, so the chain can be read: a program is about 1 % bigger
- A function whose frame can go past a page of stack touches each page as it lowers the stack, on Linux and macOS as on Windows, so a stack overflow always stops at the guard page and is reported. A frame bigger than the 4 KiB guard of a thread jumped over it, into the stack of another thread
- QBE has twenty more changes, thirty-one in all: thread data on Windows, its address read once per function on Windows and macOS, and the thread data that starts at zero kept out of the file on macOS, small slots cut into pieces that live in registers, a fix for riscv64, six for the chain of calls and the debugger, six that keep a long function linear, and five fixes of QBE itself
- Building the compiler takes 32 % more instructions: most of it is the work QBE and the assembler do on the inlined code, and the rest is the code of threads, arenas and the debugger. The compiler that comes out is faster, it checks the sources of 0.3 with 7 % fewer instructions

Faster
- A direct call to a small function is replaced by its body when the program is written, after `-memory`, so the warnings still name your functions. The parameters are still copies, a runtime error keeps its place, the stack doesn't grow, and a function whose calls were all inlined is not written to the program. A function with more than 4 KiB of variables is not inlined on its own, they would take room in the frame of every caller, and into a recursive function, whose every level keeps them, one with more than 32 bytes
- Small structs live in registers: QBE cuts a slot of 32 bytes or less whose address stays in the function into its pieces. `acc = acc + scale(V{1.0, 2.0}, 0.5)` in a loop of 100 million rounds runs in 0.11 s, it took 0.69 s, as fast as C with `-O2`
- A struct or array literal that gives its values in order puts its small values, numbers, `bool` and enums, together in words of 8 bytes and writes each word at once, so a copy reads it from one write and doesn't wait: a loop that appends `V{x, y}` of two `f32` to a list takes half the time, with two `i32` 2.5 times less, and `json` parses 4 % faster
- A constant whose value the compiler knows, an array or a struct of literals, of enum variants or of other such constants, is kept once in the read-only data of the program, and every use reads it: a lookup in `TABLE = [3, 1, 4, 1, 5, 9, 2, 6]` in a loop is 2.8 times faster
- Text literals with the same text share their data, and `print` no longer builds `stdout` on the stack, so long functions compile much faster: 20000 `elif`, or a switch of 20000 cases, take 5 times fewer instructions
- A long function compiles in a time and a memory that grow with its length. QBE keeps in a set of temporaries only its words that are not zero, where the sets of each block had a bit for every temporary of the function, and three of its walks no longer climb the whole dominator tree, as deep as a chain of `elif`. 20000 `elif` with a `print` each compile in 1.6 s and 0.4 GB, they took 8.1 s and 1.1 GB, and on a value known only when the program runs, 20000 compile in 2.8 s and 0.5 GB where 5000 took 4.9 s and 2.2 GB. A temporary defined in several places finds its phis through its definitions, not every instruction of the function, so 6000 copies of a struct in a row compile in 0.37 s

Fixed
- The `==` and `hash` that the compiler writes for a type, and the data of a text literal, could take the name of a function of the program, like `sv`, and the program didn't assemble
- A constant that names another constant read the local variable with that name where it was used
- `-memory` warned that a call may free memory it also reads through another argument when both were memory of contexts, and no argument was involved
- `-memory` took a field that comes after an array in a struct for an element of the array, and warned about memory that was fine
- `-memory` warned that memory may be freed already when an element freed in a loop was also kept in a heap block, like the text that each thread of a list gives, freed as you wait them one by one
- QBE for riscv64 stopped with `unimplemented` when a constant or a value on the stack went into a temporary that lives on the stack
- On Windows a function of Flag that C calls, like a callback, could change the floats that the C around it kept in `xmm6` to `xmm15`, which Windows has every function keep

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
- A field or an element of a value in a map changes in place, the key found once and added with a zero value when it is missing: `players[name].score += 10`, `pairs[a][b] += 1`. Passed where a pointer goes, `m[k]` is copied and put back after the call: `append(groups[k], v)`
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
- `f(x[i])` with a pointer parameter, when `x[i]` gives a copy, puts the copy back with `put(x, i, v)` after the call, and without that `put` it doesn't compile: it changed a copy that nobody saw
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
- On Windows a connection and a file could get the same number and share what waits to be sent
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
