This is the reference of the language

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
- [Iteration](#iteration)
- [Control flow](#control-flow)
- [Errors](#errors)
- [Constants, file variables and contexts](#constants-file-variables-and-contexts)
- [Threads](#threads)
- [Modules](#modules)
- [Memory](#memory)
- [The memory checker](#the-memory-checker)
- [Debugging](#debugging)
- [Calling C](#calling-c)
- [Platforms](#platforms)
- [Editor support](#editor-support)
- [Diagnostics](#diagnostics)
- [Building the compiler](#building-the-compiler)
- [Rough edges](#rough-edges)

## How the compiler works
The compiler is one program, `flagc`, written in Flag. When you compile it goes through these steps
1. The lexer cuts every file into tokens (`src/lex.flg`)
2. The parser builds the tree of the program (`src/parse.flg`)
3. The generator checks the types and builds the program as a list of QBE instructions in memory (`src/types.flg`, `src/gen.flg` and `src/call.flg`)
4. That list is written as QBE IL, the text language of the [QBE](https://c9x.me/compile/) backend (`src/ir.flg`), with the calls to small functions replaced by their bodies (see [Inlining](#inlining)) unless you build for a debugger, and QBE, that is built into flagc, turns the IL into assembly
5. The C compiler assembles it and links it with the C library

With `-memory` there is one more step between 3 and 4, `src/mem.flg` reads the instructions and checks the memory (see [The memory checker](#the-memory-checker)). It runs before the inlining, so its messages name the functions you wrote. With `-debug` there is one between 4 and 5, `src/dwarf.flg` writes what a debugger reads at the end of the assembly (see [Debugging](#debugging)).

The generator starts from `main` and only the code that `main` can reach ends up in the program. But every function that is not generic is checked, even if nothing calls it, so an error in a function you don't use still stops the compilation. The C functions of an `extern` block are checked too, their types must exist and be types that cross to C. Generic functions are checked when a call makes an instance of them.

The compiler stops at the first error. It checks a function, then the instances its calls made, each one with the instances it made before the next, so of two failing calls in one function the first one is reported.

The IL and the assembly are written next to the program, `program.ssa` and `program.s`, and they are removed when the program links, unless you pass `-keep`. If QBE or the link fails they stay there so you can look at them.

## The command line
```
flagc [run] file.flg folder ... [-o program] [-keep] [-check] [-memory] [-debug] [-target system-machine] [-overlay file] [-at file:line:column] [-version] [anything else goes to cc] [-- arguments for run]
```
- An argument that ends in `.flg` is a file to compile, and a folder adds every `.flg` inside it and inside its folders, sorted by name, without going into links to folders. The order of the files doesn't matter, all of them are compiled together
- `run` has to be the first argument. It compiles and then runs the program with what goes after `--`. Without `-o` the program is written in the temporary folder (`TEMP`, or `/tmp` if it's not set) as `flag-run-<number>.out` (`.exe` on Windows) and removed after it runs. The exit code of flagc is the exit code of the program
- `-o` names the program, `a.out` if you don't give one, `a.exe` on Windows
- `-check` stops after checking the code, it writes nothing and it doesn't need a `main`
- `-memory` runs the memory checker, the warnings don't change the exit code
- `-debug` builds the program for gdb and lldb (see [Debugging](#debugging))
- `-keep` keeps the `.ssa` and `.s` files, and the `.o` of `-debug` on macOS
- `-target` builds for another system (see [Platforms](#platforms)). A target that doesn't exist is a wrong command line
- `-overlay` and `-at` are for editors (see [Editor support](#editor-support))
- `-version` prints the version and the commit it was built from
- Anything else goes to the C compiler, in the same order: `.c`, `.o` and `.a` files, `-l` flags, `-static`...

flagc exits with 0 when everything worked, 1 when there is an error in the code or when a step fails, and 2 when the command line is wrong. It's wrong when there is no file to compile, when a folder has no `.flg` file inside, when `-o`, `-target`, `-overlay` or `-at` is the last argument, and when `-at` is not a place inside one of the files
```
error: -target expects a value after it
error: the folder notes has no .flg file to compile
```

### The C compiler
The C compiler is `cc`, or the one in the `CC` variable. `CC` is split on spaces, so `CC="zig cc"` works. The link is always
```
cc -o program program.s -L<library folder> -L<folder of each input> -l<each extern library> <anything else>
```
The `-L` of the folders of your files means that you can put a `libsomething.a` next to your code and use it with `extern "something"`.

If the C compiler cannot run you get
```
error: cannot run cc: No such file or directory, flagc links with a C compiler and the CC variable chooses which
```

## Files, folders and the library
`core.flg` is always loaded first. It has no module, so everything in it that is not `priv` is visible everywhere, it's where `print`, lists, maps and text live.

The library is the `lib` folder next to the folder of flagc (`bin/../lib`). If it's not there flagc uses the copy of the library that is built inside it, and unpacks it the first time into a cache folder: `$XDG_CACHE_HOME/flag/<version>-<number>`, or `%LOCALAPPDATA%\flag\<version>-<number>` on Windows, or `$HOME/.cache/flag/<version>-<number>`, or `/tmp/flag/<version>-<number>`, where the number changes when the library does. So a single `flagc` file is enough to compile, and if you keep the `lib` folder next to it you can read and change the library.

When a file says `use something` and no file you gave declares `module something`, flagc loads `something.flg` from the library, a module of the library is the file of its name.

A file can start with the UTF-8 mark (the BOM) that some editors write, it's skipped.

A file given twice is loaded once. The paths in the errors are the ones you gave, and the library is shown relative to the current folder when it's inside it.

## Syntax
### Lines and blocks
- A block is the lines after a `:` that are more indented than the line with the `:`. The width of the block is the indentation of its first line, and a line deeper than that is an error. Tabs and spaces count one each, so don't mix them
- A line that goes back to a column where no block started is an error too: `unexpected indentation, this line matches no block open above it`. The fields of a type, the variants of an enum block and the functions of an `extern` block are blocks like any other
- A `:` always needs an indented block after it, there is no `if x: print(x)` in one line. `pass` is the statement that does nothing, for a block that must be empty. Only an `extern` block can have nothing after its `:`
- Declarations (`func`, `type`, constants, `extern`...) start at the first column, and code goes inside functions
- `module` goes in the first line of the file, and `use` goes before the first declaration
- Inside `()`, `[]` and `{}` the lines continue, so you can break long calls and literals at any indentation. There is no `\` to continue a line
- `#` comments until the end of the line, there are no multiline comments. Files with `\r\n` line ends work, and a `\r` that is not before a `\n` is an error
- Blocks, expressions and types nest up to 500 levels: `this code nests more than 500 levels deep, move the inner part to a function or a variable`. A chain is not nesting, so a chain of `elif` or `case`, or of operators like `a - b - c`, `a and b and c` or `a ifnull b ifnull c`, has no limit. A long chain costs compile time and memory in proportion to its length: 2500 `elif` on a value known only when the program runs, each with a `print`, compile in 0.3 s, 20000 in 2.8 s with 0.5 GB of memory, and so does a switch of 20000 cases. A `case` with 20000 values takes 0.3 s

### Tokens
- Names are letters, digits and `_`, and don't start with a digit
- `@name` is a builtin (`@linux()`, `@argc()`...), and `@trust`/`@check` are markers for the memory checker
- `$T` is a type parameter and `$$n` a number parameter
- Integers: `42`, `0xff`, `0b101`, and one `_` can go between two digits to read them better, `1_000_000`, `0xff_ff`. There are no octal numbers
- Floats: `2.5`, `1e3`, `1.5e-3`. A float needs digits on both sides of the `.` (`.5` and `5.` are not floats) and has one exponent
- Characters: `'a'` is one ASCII character or one escape, and it's the number of that character. A tab or any other control byte is written as its escape, `'\t'`
- Text: `"..."` in one line, with the escapes `\n \t \r \0 \\ \" \' \e` and `\x41`, a byte by its two hexadecimal digits
- The two character symbols are `== != <= >= += -= *= /= %= -> := .. .* << >> ||`

`!`, `&&` and `||` are not the logical operators, it's `not`, `and` and `or`. The compiler tells you: `'!' is not an operator, write: not`.

### Keywords
These words are the language, so nothing you declare can have one as its name, not a variable, a parameter, a constant, a function, a type, a field or a module
```
if elif else while for in switch case break continue return defer with pass
and or xor not is as try trust parallel wait iferror ifnull true false null
```
```
error: 'return' is a keyword of the language, it cannot be used as a name
```
`reverse` cannot name a variable, a parameter or a constant, because a `for` reads it as the direction, but it's also the function `reverse(items)`.

The words that start a declaration (`func`, `type`, `module`, `use`, `extern`, `context`, `operator`, `priv`, `packed`, `export`, `inline`, `enum`, `fatptr`, `error`, `where`) only mean something there, so `type` or `module` can be the name of a variable or of a field.

## Types in memory
### Sizes
| Type | Size and alignment |
|---|---|
| `bool`, `i8`, `u8` | 1 |
| `i16`, `u16` | 2 |
| `i32`, `u32`, `f32` | 4 |
| `i64`, `u64`, `usize`, `f64`, pointers, function values, `Thread[T]` | 8 |
| void types, `null`, `IterationEnd` | 0 |

`bytes` has no size, it only exists behind a pointer (`*bytes` is the `void *` of C). A variable of type `bytes` is an error: `variable x needs a size, bytes is a run of unknown length`.

`sizeof(T)` gives the size as an integer literal, so `n := sizeof(i64)` makes an `i32`. Give the type when you want a `usize`: `n: usize = sizeof(i64)`.

Structs, unions and arrays are values, so assigning one, passing it or returning it copies it. A copy of more than 128 bytes is a call to `memcpy`. A literal that gives its values in order, `V{1.5, 2.5}` and not `V{x = 1.5, y = 2.5}`, puts the numbers, `bool`s and enums smaller than 8 bytes that share a word of 8 bytes together, and writes that word at once, so a copy of the value reads each word from one write and doesn't wait for the processor to join the pieces.

### Structs
The fields are in the order you write them, each one aligned to its own size, and the size of the struct is rounded up to its biggest alignment, like C does
```flag
type S:
    a: u8   # offset 0
    b: i32  # offset 4
    c: u8   # offset 8, and the size is 12
```
A nested field is inline in the struct, and its type is named after the struct and the field: `Car.motor`, or `cars.Car.motor` when it comes from the module `cars`.

A field with `@other` uses the same bytes as a field declared above it, it cannot be bigger than that field, and a literal can fill only one of the two. It starts with the same zero bytes as the field above, so when it holds a union without `null` it's not a value until you assign it. Printing skips the shared fields, and a struct with shared fields cannot be passed to C by value.

A `packed type` has no padding, each field starts where the one before ends and its alignment is 1, so `packed type T:` with `a: u8` and `b: i64` takes 9 bytes. Fields can share storage with `@` there too. A packed struct cannot be passed to C by value, pass a pointer.

A struct literal without the name, `{1, 2}` or `{x = 1}`, takes the type from where it goes: a variable with its type, an assignment, a `return`, a field of another literal or an element of an array with its type. When that type is a union with one struct in it, the literal takes that struct, so `return {1, 2}` works in a function that returns `Point?`, and so does `m["a"] = {1, 2}` on a map of points. Anywhere else there is no type to take: `a {...} without a type needs a place that gives it one: p: Point = {1, 2}`. `{}` there is the value with everything at zero.

### Arrays
`array[T, N]` is N elements one after the other, with the alignment of T. Arrays are values, assigning one copies all of it.

### Fixed maps
`fixmap[K, V, N]` is a struct with `len` and room for N entries, each one the key and the value, in the order they were put. Looking for a key goes through the entries, so they are for small maps. `sizeof(fixmap[i32, i32, 2])` is 24.

### Unions
A tagged union is a 4 byte tag at the start and the value at byte 8, so `i32?` takes 16 bytes and `&strview?` takes 24. A union of void types is only the tag, 4 bytes, that's why enums are small.

The tag says which member the union holds. It comes from the id the type has inside that compilation, so it can change when you change the program, don't save it to a file. The tag of `null` is always 0.

An enum that gets no value starts as the first variant you wrote, also when another enum declared some of its variants before. A union written with the same variants is the same type, so it starts there too, and two enums with all the same variants would be one type with two starts, so that's an error: `type Paint = Color` gives an enum a second name. Any other union starts as its member with the lowest id: the builtin types first, then the types without data in the order they are declared, and then the others in the order the compiler makes them. So `T?` starts as `null`. Between two types with data, like `P|Q`, it follows the declarations most of the time but it's not something to rely on, give the value when it matters. The compiler writes that tag every time it makes the value: a variable, a field, an element, a literal. An array of them is filled by a loop when the program runs, a `context` when each thread starts, the first one before `main`, and a file variable without a value before `main`. Memory that you zero yourself, like what `calloc` gives, is `null` for a union that has `null`, and it's not a value at all for a union that doesn't: assign to it before you read it, printing it stops the program with an illegal instruction.

The members are flattened, repeated ones removed and sorted, so `(i32|null)|f64?` is `null|i32|f64`, and that's how the errors write it. A union inside a pointer or inside the other kind of union is written in parentheses, `*(null|i32)` and `i64||(i32|f32)`.

An untagged union (`i32||f32`) is as big as its biggest member and has no tag. It starts at zero, and a member that holds a union without `null` is not a value until you assign it.

### Fat pointers
A fat pointer is the address of the data and then its fields
| Type | Layout | Size |
|---|---|---|
| `&strview`, `&slice[T]` | address, len | 16 |
| `&str`, `&list[T]` | address, len, cap, arena | 32 |
| `&map[K, V]` | address, len, cap, arena | 32 |

A fat pointer starts with everything at zero, that's the empty one, and `append`, `put` and `+=` allocate the memory the first time. The map is a table of `cap` slots (the hash, the key and the value) with open addressing, a hash of 0 is a free slot, `cap` is a power of two and at least 16.

`arena` is a `*Arena`, the arena the memory lives in, and zero when it lives in the heap (see [Arenas](#arenas)). The views start like the ones that own, so a `&str` is a `&strview` and a `&list` a `&slice` without copying anything.

`x.*` is the data of a fat pointer, so `ptr(x.*)` is its address as a `*bytes`.

Assigning a fat pointer copies its address and fields, not the data, so the two share the elements. If one of them grows, the other one doesn't see the new length, and if the grow moved the data the other one points to freed memory. That's why functions that grow a fat pointer take `*&` (see [Passing them to functions](Overview.md#passing-them-to-functions)).

## Numbers
### Literals
An integer literal has no type until you use it. It takes the type of where it goes if it fits, and alone it's an `i32`, or an `i64` or `u64` if it doesn't fit
```flag
a: u8 = 200     # fits
b: u8 = 300     # error: 300 does not fit in u8, the largest u8 is 255
c := 5000000000 # i64
```
Arithmetic between literals is done by the compiler with the exact value, it doesn't wrap: `2147483647 + 1` is the `i64` literal `2147483648`. Dividing a literal by the literal `0` is an error, and so is a result that doesn't fit in 64 bits.

A float literal is `f64` alone and fits any float. An integer literal also goes to a float type when needed.

### Arithmetic
- Two numbers of different types never mix, not even in comparisons, you convert one of them: `+ expects two numbers of one type but got u64 and u32, convert one side with u64(...) or u32(...)`. A literal takes the type of the other side
- Integers wrap around when they overflow
- `/` on integers rounds toward zero, and `%` has the sign of the left side: `-7 / 2` is `-3` and `-7 % 2` is `-1`
- Dividing an integer by zero stops the program with `flag: division by zero`. Floats are not checked, you get `inf` or `nan`
- `%` is only for integers, for floats there is `fmod` in `math`
- `-x` only works on signed integers and floats
- `<<` and `>>` shift, `>>` keeps the sign on signed integers. The amount must be less than the size in bits, the compiler checks it only when both sides are literals
- `and`, `or` and `xor` on integers work on the bits: `0xf0 and 0x3c` is `0x30`. On `bool` they are logic, and `and` and `or` skip the right side when the left side already decides
- There is no `**`, `~`, `^`, `&` or `|`

### Precedence
From the loosest to the tightest
1. `or`
2. `and`, `xor`
3. `is`
4. `==`, `!=`
5. `<`, `>`, `<=`, `>=`
6. `iferror`, `ifnull`
7. `..`, `<<`, `>>`
8. `+`, `-`
9. `*`, `/`, `%`
10. `as`

The unary operators (`-`, `not`, `try`, `trust`, `parallel`, `wait`) are tighter than all of them. So `not x == 3` is `(not x) == 3`, write `x != 3` or `not (x == 3)`. And `x and 3 == 2` is `x and (3 == 2)`, put the parentheses when you mix bits and comparisons.

## Conversions
### Implicit ones
When you pass an argument, the value is converted only if it's
- the same type
- a `&str` going to a `&strview`, or a `&list[T]` going to a `&slice[T]`
- a `*array[T, N]` going to a fat pointer of `T` (a list, a slice, or text if T is `u8`), it keeps the address and sets `len` and `cap` to N
- `[]` going to any fat pointer, it becomes the empty one, and `{}` going to a map. Alone they have no type, `x := []` is an error. A generic can take one as its `$T`, but then no type can be made from that `$T` except a pointer: `heap([])` is a pointer to an empty array that becomes a list where it lands, and a generic that returns `array[$T, 2]` or `Box[$T]` for it is an error, `no type can be made from [], an empty literal has no type of its own`
- a literal that fits the type
- a text literal, or a `&str`, going to a `cstr`
- a value going to a union that has its type, or a union going to a bigger union. A value that converts to exactly one member goes in through it: a `&str` into `&strview|i32`, a `&list[i32]` into `&slice[i32]?`. That's also why `["a", s]` with `s: &str` is an `array[&strview, 2]`: the element type of a literal leaves out a type that converts to another one in it
- a member going in or out of an untagged union

Assignments, declarations and `return` do all of that and also
- grow integers: to a bigger type of the same sign, or from unsigned to a bigger signed (`u32` to `i64`)
- take a union into one of its members when every member converts (`i32|i64` into `i64`)
- use your `operator =` (see [Operators](#operators))

So `f(x)` with `x: i32` and `func f(n: i64)` doesn't find `f`, write `f(i64(x))`, but `n: i64 = x` works.

Never implicit: signed to unsigned, types of the same size (`u64` and `usize` too), making a number smaller, integer to float and back, `f32` to `f64`, and a pointer to another pointer. `null` is not a pointer, a pointer that can be missing is `*T?`.

### T(x)
Calling a number type converts
- between integers it cuts or extends, extending with the sign of the source: `usize(-1)` from an `i32` is 18446744073709551615
- a float to an integer rounds toward zero and stays at the limits: `i32(1e300)` is 2147483647, and `nan` gives 0
- a `bool` gives 1 or 0. There is no `bool(x)`, write `x != 0`
- a literal is checked: `i8(300)` is an error

Calling a struct or an alias with one value does the same conversion as an assignment, unless there is a function with that name.

### as
`as` reads the bytes of a value as another type, it never converts
- two scalars of the same size: `f32(1.5) as u32` gives the bits, `p as usize` gives the address
- two structs or arrays of the same size
- a union to one of its members or to a smaller union, without checking the tag
- a member in or out of an untagged union

`as` never writes a tag, so nothing becomes a union with it, and two unions of the same size don't read as each other. Anything else is `cannot reinterpret i32 as i64, they do not have the same byte size`.

### Pointers into fat pointers
Assigning a `*bytes`, or a pointer to the element type, to a fat pointer sets only its address, and you set `len` yourself. It's how the library builds views over memory
```flag
buf: array[u8, 4] = [104, 111, 108, 97]
text: &strview
text = ptr(buf) as *bytes
text.len = 4
print(text) # hola
```

## Unions and narrowing
`is` checks the tag. It needs a tagged union on its left, and a check that is always true or always false is an error, so you know when a type changed and the check is useless.

Narrowing only works on local variables and parameters, not on fields, and reading a field of a member through the union says so: `Card|null has no field rank, and is and case narrow only variables: copy it to one first`. Copy the field to a variable first
```flag
found := user.best
if found is Card:
    print(found.rank)
```
The rules
- `if x is T:` narrows the block, and the `elif` and `else` get what is left
- `a and b` checks `b` with what `a` narrowed, and `a or b` checks `b` with what `not a` narrowed, so `x is null or x > 3` works
- `not (x is T)` doesn't narrow anything
- `while x is T:` narrows the body, not after the loop
- `switch x:` narrows each case to the types it names, and the default to the rest (see [switch](#switch))
- after an `if`, the variable has the types of the branches that don't leave. So `if x is null: return` leaves `x` without `null` after it
- assigning a value that fits the narrowed type keeps it narrowed, any other value brings back the whole type. Inside a loop that is an error, because the loop reads the variable with the narrowed type every round: `v cannot take &strview inside this loop, it was narrowed to i32 outside the loop and the loop reads it that way every round`

`==` between a union and one of its void types compares the tag: `x == null`, `color == Red`. Two values of one union compare by member and value (see [Operators](#operators)), and two unions made only of void types compare too when every member of one is in the other, like an enum and a narrowed part of it. To compare the value of a member with a value of its type, narrow first: `x is i32 and x == 3`.

## Functions and calls
### Which functions a call sees
A call sees the functions of its own module, the global ones (from files without `module`) and the ones of the modules it `use`s. It also sees the functions of the module that declares the type of an argument, even without `use`, so a type brings its functions with it.

### Arguments
The arguments fill the parameters in order, and named arguments fill their parameter. After a named argument the rest must be named too, and a parameter can't be filled twice.

A default value is computed at the call, every time it's used. It can use constants, file variables, contexts and functions, not the other parameters. It runs in the caller, so it cannot have `try`: `a default value runs in the caller, so try would return from the caller: handle the error with iferror or trust`. `@here()` in a default is the place of the call as text, `"main.flg:12:5"`. A parameter without default can come after one with default, and then you name it
```flag
func pick(a: i32 = 1, b: i32) -> i32:
    return a * 10 + b

func main():
    print(pick(b = 2)) # 12
```

### Choosing an overload
Every overload that can take the arguments gets a score, and the highest one wins
- the same type gives 2
- a conversion gives 1 (`&str` to `&strview`, a literal to another type, a value into a union...)
- an integer literal to `f32` gives 0, so `f64` wins
- a `$T` that is still free gives 0 and takes the type of the argument
- a type with a `$T` inside, like `&slice[$T]`, `*$T` or `func($T) -> bool`, gives 1 plus what its inside gives. When the argument gets there through a conversion (a `&list[i32]` or a `*array[i32, 3]` going to `&slice[$T]`) it gives only what its inside gives
- a `*T` parameter with a value of type `T` gives one less than `T` would, and passes the address of the value. When the value is not a variable the function works on a temporary, and what it changes there is lost. The temporary lasts until the function that makes the call returns, like the copy `ptr` makes of a value that is not a variable, so `s: &slice[i32] = ptr([1, 2, 3])` and a pointer that the call gives back stay good. It doesn't work with a bare `*$T`, that one takes any pointer.

When two overloads have the same score, the one that takes more arguments through a shape like `&slice[$T]`, instead of a bare `$T`, wins.

The arguments that are not literals are matched first, so `$T` comes from your variables and the literals adapt to it: `biggest(2, u8(7))` makes `$T` an `u8`.

Then `where` removes the overloads whose condition is false.

A module that has a `priv` function with the name of the call chooses among its own functions before the others (see [Modules](#modules)).

If they are still tied the nearest one wins: the one of your module, then a global one, then one of another module. And if that doesn't decide either it's an error, with a note for each one
```
error: ambiguous call to show, several overloads match (i32) equally
```

The order of the declarations doesn't matter, a function can call one declared below it or in another file.

Declaring the same function twice in a module is an error, with a note that points to the other one. Two functions are the same when everything from the name to the `:` is equal, and the names of the parameters don't count
```
error: main is declared twice with the same parameters
```

A text literal doesn't go to a `&str` parameter, because a `&str` is text that you own. Take `&strview` when you only read the text, a `&str` also goes there.

### Function values
The name of a function is a value, and its type is the declaration without the names: `func(i32, i32) -> bool`, or `func(i32)` when it returns nothing. It takes 8 bytes, the address of the function.
```flag
func double(x: i32) -> i32:
    return x * 2

func apply(f: func(i32) -> i32, x: i32) -> i32:
    return f(x)

func main():
    f := double
    print(f(4), " ", apply(double, 5)) # 8 10
```
- `->` takes everything after it, so `func(i32) -> bool?` returns a `bool?`. A function that may be missing is `(func(i32) -> bool)?`, or `func(i32)?` when it returns nothing, and it's checked with `is` like any union
- when the place says the type (a variable with its type, an assignment, a `return`, a field of a literal, an element of an array with its type), the name gives the overload with exactly those parameters. A union says it only when it has one function type, and a generic function gets its types from them: `top: func(f64, f64) -> f64 = biggest`
- when nothing says the type, like in `f := show` or in an argument, the name must have one function and it cannot be generic: `show names several functions and nothing here says which: give it to a variable that says its function type first`
- the parameters must be exactly those, defaults included, and there is no conversion between function types. Overloads are chosen by their parameters, never by what they return
- a call is `(` after any value: `f(x)`, `button.on_click(x)`, `steps[i](x)`, `pick(true)(x)`. `name(...)` calls the variable `name` only when it holds a function, any other variable leaves the functions of that name reachable. A constant that holds a function is called by its name too, when no function has that name
- the arguments convert like in any call (an `i32` doesn't go to an `i64` parameter here either), but a function value has no parameter names and no defaults
- a function value that got no value is empty, and calling it stops the program with `flag: call of an empty function value`. Passing it to a function stops the program at that call, with `flag: an argument is an empty function value`, so `sort(l, before)` with an empty `before` points at your line and not at the library
- `==` and `!=` compare two values of the same function type. Printing one shows its address
- a parameter like `less: func($T, $T) -> bool` takes a function with that shape and `$T` gets its type from it
- C functions (`extern` and `export`) cannot be function values, and a function type cannot be in the signature of one. A C callback is still an `export func` passed with `ptr`
- there are no closures, no functions inside functions and no functions without a name. A function gets what it needs as arguments

### Inlining
When the program is written, a direct call to a small function is replaced by the body of that function. A function is small when it has at most 45 instructions of IL, after the calls inside it were replaced, and at most 4 KiB of variables, and the function that makes the call has at most 500 instructions: inlining into big functions makes QBE slow, and a big function gains little from it. The variables of an inlined body take room in the frame of the caller even when the call never runs, so a function with an array of a million numbers would make every frame that calls it that big, and on Windows, whose stack is 1 MiB, the program would stop with `flag: stack overflow`. Into a function that calls itself, directly or through others, a function is inlined on its own only with at most 32 bytes of variables, about what the call takes: each level of the recursion keeps them, and a recursion of 30000 levels that calls one with an array of 256 bytes would need 8 MiB. `inline func` and `inline operator` replace every direct call, whatever the sizes, and a generic one does it in each instance
```flag
inline func clamp(x: $T, low: $T, high: $T) -> $T:
    if x < low:
        return low
    if x > high:
        return high
    return x
```
`inline` is a word of the declaration, like `export`, and it goes after `priv`. These calls are never inlined
- to a function that calls itself, directly or through others. Declaring one `inline` is an error, with a note at the call that leads back: `fact is inline and recursive, inlining it would never end: declare it without inline`
- to `main`, nothing is inlined into the start of the program. `inline func main()` is an error, and so is `inline export func`, other languages call an export function by its name, and an `inline` C function in an `extern` block. A `main` with parameters or in a module is not the start, it's a function like any other
- to `panic`, its own frame is where the chain of calls starts
- through a function value that is read when the program runs, from a variable, a field or an element. A function value of an inline function points to the function out of line, which is kept for that. A constant that holds a function is written again where you use it, so calling it is a direct call
- that `parallel` makes: the new thread calls the function out of line, `parallel f(x)` with an inline `f` too

The inlined body behaves like the call: its parameters are copies, it can return from the middle, and its `defer`s run when it returns. It doesn't change the place of a runtime error, that is computed at the call like `@here()`, and it doesn't grow the stack, the inlined bodies of a function share the places of their variables, so a loop of millions of inlined calls runs in the same frame. A debugger shows the lines of the inlined body when it comes from the same file, and the line of the call when it comes from another one: QBE knows one file for each function. A function whose calls were all inlined, and whose name is not used as a value, is not written to the program.

### Protocol functions
The compiler calls some functions by their name, so you make your types work with the language by declaring them
| You write | It calls |
|---|---|
| `x[i]` | `at(x, i)`. If it returns a pointer, `x[i]` is the element and you can assign to it, if it returns a value that's what you read, like `V|null` for a map |
| `x[i] = v` | when `at` returns a value, `put(x, i, v)`. `x[i] += v` is `put(x, i).* += v` when a `put(x, i)` gives a pointer to the value, and `put(x, i, at(x, i) + v)` when not. `x[i].f = v` and `x[i][j] = v` go through that pointer too. `f(x[i])` with a pointer parameter passes a copy and calls `put(x, i, copy)` after `f` |
| `x[a:b]` | `slice(x, a, b)`, and `x[a:]` is `slice(x, a, x.len)` |
| `for v in x` | `iter(x)` and then `next(ptr(state))` |
| `for v in reverse x` | `reverse_iter(x)` and then `next` |
| `with x:` | `close(x)` |
| `{k: v}` | `put` |
| `print(a, b)` | formats the values with `+=` into one line, and `write(stdout, line)` |
| `"a" + x`, `s += x` | `operator +=(s: *&str, x: T)` |
| `a != b`, `a > b`... | `==` and `<` |
| `t: T = u` | `operator =(dest: T, u: U) -> T` |

For arrays, lists, slices and text the compiler writes `x[i]` with an integer itself, and the `for` too. They don't go through `at`, `iter` or `next`. So declaring `at` for an array, a list, a slice or text with an index that can be an integer is an error, also when the index is a `$I` or a union with an integer in it: `x[i] with an integer on &list belongs to the language, at cannot be declared for it`. So is `iter` or `reverse_iter` for an array, a list, a slice or text. An `at` with another kind of index is yours, `l["name"]` on a list calls it. A pointer to an element is `ptr(x[i])`, and `for x in p` walks the array, list, slice or text that `p` points to.

## Generics and where
### Type parameters
`$T` gets its type only from the parameters, there is no way to give it by hand (`ident[i32](3)` doesn't exist). A `$T` that is only in the return type is an error when you call the function: `$T is not bound here, a generic gets its type from a parameter`.

`$T` also comes through a conversion to a fat pointer: a `&list[i32]` or a `*array[i32, 3]` going to `&slice[$T]` makes `$T` an `i32`, so `sort(ptr(values))` sorts an array. It comes through a generic alias too (`func get(x: Opt[$T])` with `type Opt[$T] = $T|null`), and from inside a function type (`less: func($T, $T) -> bool`). In a union one member can carry it, `$T|null` or `(func($T) -> bool)?`, and a `null` given there is fine when another parameter already says what `$T` is.

`$$n` is a number, it comes from an array size (`array[$T, $$n]`) or from a type with a number parameter, and inside the function it's an integer literal.

Every different set of types makes a new instance of the function, and the same set reuses it. A generic type works the same way (`type Pair[$A, $B]`), and it cannot have a nested field, its `$T` would not reach the inner type: declare that type on its own. A generic that makes bigger types with every call doesn't compile: `f(x: $T)` calling `f(ptr(x))` gives `instancing f never ends, each call builds a bigger type than the last`, and a type that grows the same way gives `type Pair never ends, each instance builds a bigger one than the last`.

An instance is checked when a call makes it, so an error inside it comes with a note at that call: `the find that fails was made for this call, with $T = Mark`. When the generic is in the library the error points at your call instead, the one that led into the library, and a note shows the line inside it: `it fails here, inside less, with $T = P`.

### where
A `where` condition can be
- `$T is i8|i16`, true when the type of `$T` is inside that union
- `$T == i32` and `$T != i32`, with the type parameter on either side
- `can_assign(&strview, $T)`, true when a `$T` can be assigned to a `&strview`
- `@linux()`, `@macos()`, `@windows()`, `@x86_64()`, `@arm64()`, `@riscv64()`, for the system the program is built for
- `not`, `and`, `or` and parentheses
- anything else that the compiler can compute to `true` or `false` with the types and numbers of the call: `sizeof($T) >= 8`, `$$n <= 16`

The values of the parameters are not known when the call is chosen, so `where x > 0` is an error: `a where reads types, numbers and constants known when the call is chosen, and x is not one`.

The platform builtins and `can_assign` only work inside a `where`.

`@compile_error("text")` in a function stops the compilation with that text, pointing at the call that made that instance of the function. A function that nothing calls doesn't fail, so it's the way to say that an overload chosen by `where` can't be used.

## Operators
You can overload `+ - * / % == < << >> and or xor` for your types, and also `=` and `+=`, that are explained below. `!=`, `>`, `<=` and `>=` come from `==` and `<`, and `-=`, `*=`, `/=` and `%=` from `-`, `*`, `/` and `%`, and declaring them is an error that tells you which one to declare
```
error: operator >= follows from <, declare operator < instead
```
Any other operator is an error too: `operator foo cannot be declared, the operators to declare are + - * / % == < << >> and or xor = +=`.

An operator has two parameters, its left side and its right side, there are no unary operators to overload. It can have more parameters when they have a default value, like `place: &strview = @here()`. An operator between two numbers of one type belongs to the language and cannot be declared again, and one between two numbers of different types could never run, numbers of different types don't mix. `==` and `<` return `bool`.

`a += b` is `a = a + b` for everything except `&str`, where `+=` appends. That's why the way to print your type is `operator +=(s: *&str, x: T)`, and it's the only `operator +=` you can declare. In `s += a + b` the right side is put together first and appended once, so `s += "x" + s` appends the `s` of before.

A struct, a tagged union or a void type without its own `==` gets one from the compiler. Two structs are equal when every field is `==`, in order and stopping at the first that is not, so text fields compare by content and a nested struct by its own `==`. Shared fields (`@`) are skipped, their bytes are compared through the field they share. Two values of one tagged union are equal when they hold the same member and the two values are `==`, and two values of a void type are always equal. It's one function per type, and a nested struct calls the one of its type, so a big struct compiles the same whether it's compared in one place or in a hundred. An untagged union has no tag to know what to compare, so a struct that holds one needs your `==`: `Bits has no == of its own, so == compares its fields, and this is field raw`. Arrays, lists and slices compare element by element, text by content, and `sort` uses `<`, that is never made for you. The made `==` compares every field, so a type that keeps a `len` and a fixed array, like a `fixmap`, declares its own `==` and `hash` for the part in use.

`hash(x) -> u64` is made the same way when no `hash` takes the type, which is what maps need for their keys: `bool`, pointers and function values hash their bits, a struct mixes the hashes of its fields, a tagged union the tag and the member it holds, and an array its elements. A struct or a union with your own `==` doesn't get one, the made one would not agree with your `==`, so asking for it is an error that tells you to declare it: `Key declares == but not hash, and equal values need equal hashes: declare hash for Key too`. A type that has no way to hash, like a `&list`, says so: `&list[i32] has no hash, declare one for it: func hash(x: &list[i32]) -> u64`.

`operator =(dest: T, src: U) -> T` returns the `T` made from an `U`, and then assignments, declarations and `T(x)` convert for you. Calls don't use it, the argument must already be a `T`.

### Assignments
In an assignment the right side runs first, and then the place on the left is found. So `l[0] = grow(l)` writes into the list as `grow` left it, and in `a[i()] = v()` it's `v` that runs first. Only a variable, a field, an element or what a pointer points to can be assigned.

When `x[i]` gives a copy, like on a map, `x[i] = v` is `put(x, i, v)`, and `v` takes its type from what `x[i]` gives, so `m["a"] = show` picks the overload of `show` that the map holds. `x[i] += v` reads the copy, adds and puts it back. A copy that is `null`, a key that is not in the map, counts as zero like a new variable, so `counts[word] += 1` counts and `lines[k] += text` appends to a `&str` that starts empty. When the type has a `put(x, i)` that gives a pointer to the value and adds the key at zero when it is missing, like a map has, `x[i] += v` changes the value there and looks the key up once, and so does assigning a field or an element of it: `players[name].score += 10`, `pairs[a][b] += 1`. The right side runs first and the pointer is used at once, so a right side that grows the map is safe. The `put` of a map that gives the pointer is priv to core, because a pointer you keep goes bad at the next `put`. Without that `put`, like on a fixed map, a field of `x[i]` cannot be assigned, it would only change a copy. On a fixed map `put` gives `MapFull?` and an assignment cannot pass an error on, so `x[i] = v` and `x[i] += v` don't compile there: call `try put(m, k, v)`.

A call that takes `x[i]` where a pointer goes, when `x[i]` gives a copy, gets a pointer to a copy, `null` made zero, and after the call `put(x, i, copy)` puts it back with the key it read: `append(groups[k], v)` adds to a list kept in a map. The map is not touched while the call runs, so a call that grows it is safe. Without a `put(x, i, v)` that gives nothing it doesn't compile, the call would only change the copy.

There is no pointer arithmetic, `p as usize + 8` and back with `as` is the way.

## Iteration
### Ranges
`a..b` goes from `a` to `b - 1`, and `reverse a..b` from `b - 1` down to `a`. If `b <= a` it doesn't run. The variable has the type of the end that is not a literal, or `i32` if both are literals, and both ends must have the same type. The ends are computed once, before the loop.

The variable counts the loop, so it cannot be assigned: `i counts this loop, so it cannot be assigned: copy it to a variable to change it`. `ptr(i)` and a `*T` parameter get a copy of it, like for any value that is not a variable. That's why `a[i]` needs no bounds check when `a` is an array and the range has literal ends that fit in it, as in `for i in 0..4` over an `array[i32, 4]`.

A range with an index (`for i, x in 0..3`) is an error, and a range only goes in a `for` or in a `case`.

### Everything else
Over an array, a list, a slice or text the compiler writes the loop itself: it reads the address and the length once, and every round copies one element into `x`, or one character of the text. For anything else `for x in value` calls `iter(value)` once, keeps what it gives, and calls `next` with a pointer to it every round. `next` returns the element or `IterationEnd`, a builtin void type that ends the loop, so the elements can be `null`. The element can be a union, and `for i, x in` adds a counter that starts at 0, also with `reverse`. The counter counts the loop like the variable of a range, so it cannot be assigned either.

- Text gives each UTF-8 character as a `&strview` (`é` has `len` 2). A byte that is not part of a valid character comes alone, also each byte of an overlong form, a surrogate or a code above U+10FFFF, and `reverse` gives the same pieces. `items(text)` gives the bytes. The messages of the compiler read the code the same way, a byte that comes alone is shown as `\xc0`
- A map gives its entries (`.key`, `.value`) in the order of its table, and it cannot be reversed: `&map[&strview, i32] has no reverse_iter, so reverse cannot walk it`. A fixed map gives them in the order they were put
- An array gives its elements, the loop takes its address so it doesn't copy it, and it can be reversed
- A pointer to an array, a list, a slice or text walks what it points to
- The elements are copies, changing `x` doesn't change the list
- The length is read when the loop starts, what you append during the loop is not visited, and if the list grows the loop keeps reading the old memory, so don't grow a list you are walking

## Control flow
### switch
Only the first case that matches runs, there is no fallthrough. `case:` is the default and goes last.

Over a value that is not a tagged union, each case is compared with `==`, and a case can be any expression, a range (`>=` the start and `<` the end) or several of them separated by `,`. A switch over a `bool` with `case true:` and `case false:` handles every value, like a switch with a default, so a function can end with it when both cases return.

Over a tagged union the cases name its member types: one type, several separated by `,`, or a union `A|B`. A case runs when the union holds one of them, and inside it a local variable or a parameter has those types, like inside `if x is A|B:`. The default gets the members that no case names, and after the switch the variable has the types of the cases that don't leave, so `case null: return` leaves it without `null`. A subject that is not a variable is tested the same way, there is just nothing to narrow
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
- every type a case names is a member still there: `i32|f64 can never be null, so this case never runs`
- each member goes in one case: `this switch already handles null above, each type goes in one case`
- every member is handled or there is a default: `this switch never handles i32|null and has no default case`, naming every member left, and a default with nothing left is an error: `this default case never runs, the cases above handle every type of Red|Green|Blue`
- a value is not a case over a union: `a case over null|i32 names one of its types, and this is a value: name the type and compare the value inside its case`. A variable says so: `fav is a variable, and a case over Red|Green|Blue names one of its types: compare the values with == in an if`
- a type is not a case over anything else: `a case names a type only in a switch over a tagged union, and i32 is not one`

The cases are read when the switch is checked, with the type of the subject known: over a tagged union a case is a type and over anything else a value. So `Red`, `m.Thing`, `Car.motor` and `Pair[i32]` are types in a switch over a union, and `limit`, `p.x` and `t[0]` are values in a switch over a number. In a generic function every instance reads the cases with its own types, so a mistake in a case shows when a call makes that instance.

`break` inside a switch leaves the loop around the switch.

### break and continue
They work on the innermost loop, there are no labels, and with a number they reach out: `break 2` leaves this loop and the one around it, `continue 2` goes to the next round of the one around it. Outside a loop: `break goes inside a loop`, and a number bigger than the loops open: `break 3 reaches past the loops open here, it can count up to 2`.

### defer
- The deferred statements run when their block ends, in reverse order, also on `return`, `break` and `continue`. In a loop they run every round
- The statement is run at the end, so it reads the values the variables have then, and it's checked with the types they have then: after `defer x = "text"` runs, `x` is no longer the `i32` that an `is` left
- It only sees the variables declared before the `defer`, and the ones it declares itself
- It cannot have `return`, `break`, `continue` or `try` inside, not even in a loop inside it
- It cannot be a declaration or another `defer`, there would be nothing to run when the block ends
- It's checked like any other statement when it runs: one that drops an error doesn't compile, `this deferred statement drops an error and a defer cannot pass it up: drop it on purpose with defer _ = ..., or run it where you can handle it`. Write `defer _ = f()` when losing it is what you want
- A deferred `x = f()` into a variable that can hold an error must be read after it runs, the same rule as for a declaration, so `defer x = f()` in the block of `x` doesn't compile: nothing is left to read it

### with
`with value:` calls `close(value)` when the block ends, also when you leave it early. With a variable it's like a `defer close(x)`: what gets closed is what the variable holds then. A value that `close` returns is dropped, but an error cannot be, so a `close` that can fail doesn't go in a `with`: `close of Lock can fail, and with would drop the error: call close yourself where you can handle it`. `with scratch():` is the same thing with the scratch memory (see [Memory](#memory)).

### Returns
- A function that returns a value must return on every path: `not every path of this function returns a value`. A `while true` without `break` counts as never ending
- Code after `return`, `break` or `continue` in the same block is an error: `this code can never run, the block already left`
- `main` has no parameters, is outside every module, and returns nothing (exit code 0) or an integer, the exit code
- `_ = value` throws the value away, also when there is a variable named `_`
- A call can be a statement and its value is dropped, but a statement that only makes a value, like `x == 2` or `a[0]`, is an error: `this statement makes a value and never uses it`. For `x == 2` it goes on with `, an assignment is written with one =`

## Errors
An error is a type marked with `type error`: a void type (`type error NotFound`), an enum of them (`type error enum FileError = CannotOpen|CannotRead`), or a struct with fields that say what happened
```flag
type error BadLine:
    line: usize
    why: &strview
```
An error with fields is a struct in everything else: it's built with `BadLine{3, "too long"}`, it can be generic, it prints as `BadLine{line = 3, why = too long}`, `==` compares its fields, and after `is BadLine` or in a `case BadLine:` its fields are there, `r.line`. A union can hold several errors with and without data, and `try`, `iferror` and `trust` treat them all alike. An alias cannot be marked as an error, the error is the type it names.

When an error holds memory, like a `&str` field, whoever handles it frees it, and dropping it with `_ =` or `iferror` loses that memory: `-memory` tells you.

An expression statement that leaves an error without handling it doesn't compile
```
error: this statement drops an error: test it with is, pass it up with try or drop it with _ =
```
If you keep the result in a variable, you can't use its value until you check it with `is`, and a variable that keeps an error and is never read doesn't compile either: `x can hold an error and nothing reads it: test it with is, pass it up with try or drop it with _ =`. A `switch` over the value handles its errors, in the cases that name them or in the default.

`try` gives the value and returns the errors from the function, so each error must be in the return type of the function
- `try passes Bad up but this function returns i32`
- `null|i32 never fails, there is no error to try for`, for a union without errors

`trust` removes the errors without checking, or the `null` if there are no errors. If it was an error you get garbage, and printing it can crash the program.

`a iferror b` is the value of `a` without its errors, or `b` when `a` has an error, and `a ifnull b` is the same with `null`. `b` only runs when it's needed. When `b` gives nothing, like a call to a function that returns nothing, `a iferror b` is a statement that runs `b` when `a` failed. If `b` fits the type left from `a` the result has that type, and if not it's the union of both. They bind tighter than comparisons and looser than arithmetic, so `m[k] ifnull 0 > 3` compares the result and `m[k] ifnull 0 + 1` adds to the default, put parentheses when you mean the other one.

### Runtime errors
These stop the program, and write the message to the error output
| Message | Exit code |
|---|---|
| `flag: index 5 is out of bounds, length 3` | 134 |
| `flag: slice 2:9 is out of bounds, length 3` | 134 |
| `flag: division by zero` | 134 |
| `flag: assertion failed`, or the message of `assert(condition, message)` | 134 |
| `flag: <text>` (from `panic`) | 134 |
| `flag: call of an empty function value` | 134 |
| `flag: an argument is an empty function value` | 134 |
| `flag: File{-1} is not an open file` | 134 |
| `flag: send on a closed channel` | 134 |
| `flag: wait of a thread that was never started` | 134 |
| `flag: done of a thread that was never started` | 134 |
| `flag: cannot start a thread, the system refuses one more` | 134 |
| `flag: out of memory` | 9 |
| `flag: cannot write the standard output: No space left on device`, at the end | 1 |
| `flag: invalid memory access at 0x0` | 139 |
| `flag: stack overflow` | 139 |
| `flag: illegal instruction` | 132 |
| `flag: arithmetic error` | 136 |

The ones with exit code 134 write the place after the message (`  --> main.flg:12:11`), and then the chain of calls that led there, one line for each function of yours with the line where it's declared
```
flag: index 5 is out of bounds, length 3
  --> main.flg:12:11
  in parse_line, main.flg:9
  in load, main.flg:20
  in main, main.flg:31
```
`flag: out of memory` has no place and no chain, any allocation can be the one that fails. The one about the standard output comes when the program ends and what it printed could not be written, and no `flush(stdout)` told it.

The last four are crashes, what the system stops: a read or a write through a pointer to memory the program cannot touch, like a null pointer, says the address, an illegal instruction is most often a union that holds no valid tag, like memory you zeroed yourself (see [Unions](#unions)), and a recursion that goes too deep is a stack overflow. They write the chain too, without a place. The exit code is 128 plus the number of the signal, the table shows Linux, where a memory access can also be a bus error, 135, and macOS gives it 138. On Windows it's the code of the exception, like `0xc0000005`. The program ends with that code, not killed by the signal, so it leaves no core dump: to stop where it crashed, run it in a debugger (see [Debugging](#debugging)).

The chain
- shows only your functions: the ones of the library are left out, the place already points at your call into them
- writes the same function called again and again in one line, with the count: `in walk, tree.flg:3, 52341 times`
- of a crash inside a function of C, like `strlen` of a bad `cstr`, starts at the function of yours that called it: it finds it from the address where that function returns, while it keeps nothing on the stack, or from its frame pointers. A C library built without frame pointers, like the one of Ubuntu 22.04, gives no chain for a crash deep inside `malloc`
- stops at a function of C after your functions, so a callback that C calls ends the chain there
- has 20 lines at most, and the rest is counted: `  ... and 12 more`
- has no line for a function that is inlined into its caller, only its caller is there
- is the one of the thread that failed, and in a thread started by `parallel` it ends at the function that `parallel` runs

The program reads it from the frame pointers, that every function keeps, and a table in the program says which function each return address belongs to, with the name and place of each function of yours: about 50 bytes for each function written to the program, and with the frame pointers 1 % of the size of the compiler. The handler of the crashes is set when the program starts, before `main`, and every thread, the first one too, keeps a stack of 64 KiB for it, so a stack overflow can be told in any of them. It asks for no memory and waits for no lock, a crash can come from inside `malloc`, and a crash inside the report ends the program at once with the same code. A function whose frame can go past a page touches each page as it lowers the stack, so even a frame bigger than the guard page at the end of a stack stops there, and never writes over the stack of another thread. A program that sets its own handler for `SIGSEGV`, `SIGBUS`, `SIGILL` or `SIGFPE` replaces it.

A negative index counts from the end, `l[-1]` is the last element, and it's an error when it goes before the first one. A slice `s[a:b]` needs `a <= b <= len` after that, and its message has `a` and `b` as you wrote them. A slice can leave out its ends: `s[:b]` starts at 0, `s[a:]` ends at the length and `s[:]` is all of it. The length is `len` for text, lists and slices, the count of an array, and the field `len` of your own type, so `x[a:]` on a type without that field is an error: `this slice has no end, so it ends at the len of Grid, and it has no field len: write the end`. The length goes as an `i64` when the start is a signed integer that is not a literal, or a negative literal, and as an `usize` when not, and a start that is not a literal is converted to that type too, so `s[i:]` works with any integer `i` and `s[-3:]` keeps the last three. The slice of an array is a `&slice` of its elements, also when they are `u8`.

The compiler leaves out the check of `a[i]` on an array when it cannot fail: a literal index inside the array, or the variable of a `for` over a range of literals that fits in the array and that the body never assigns or passes to a function.

## Constants, file variables and contexts
### Constants
A constant goes at the root of a file. Arithmetic between literals is done by the compiler and the result is still a literal, so it takes the type of where you use it
```flag
N = 3
BIG = N * 1000000000 # an i64 literal

func main():
    small: u8 = N
    print(small, " ", BIG)
```
Any other expression is written again where you use the constant, like a macro, so `NOW = now()` calls `now()` every time, and a literal without a type, like `PAIR = {3, 4}` or `SHOW = show`, takes its type from each place that reads it. The names in it are the ones of the root of the file, so in `NEXT = LIMIT + 1` the `LIMIT` is the constant, also where a local variable `LIMIT` hides it. When the expression fails for one of them, a note says which: `PAIR is read here`.

A constant that gives an array or a struct whose value the compiler knows, like `TABLE = [3, 1, 4, 1, 5]` or `ORIGIN = Point{0, 0}`, is kept once in the read-only data of the program, and every use reads it there instead of building it. The values it knows are the ones a [file variable](#file-variables) can start with, and both are written by the same part of the compiler. There is one copy for each different value, so `PAIR = {3, 4}` read as a `Point` and as a `Size` has two, and so does a constant that uses `sizeof($T)` in two instances. It's static data like a text literal: `ptr(TABLE)` and a `*T` parameter get a copy, also of a field or an element of it, and so does a slice like `TABLE[1:3]`, and `TABLE[0] = 9` is an error, it would only change a copy. On Linux the data is in `.data.rel.ro` and on Windows in `.rdata`. On macOS it is in the writable data of the program, QBE cannot name a section of Mach-O. A constant can be the size of an array if it's an integer literal. A constant that uses itself is an error.

A constant or a file variable and a function of the same module cannot have the same name: `limit is declared as a constant and as a function, a name is one or the other`, and `handler is declared as a variable and as a function, a name is one or the other`.

### File variables
A variable at the root of a file, `name: T` or `name: T = value`, is one value that every function and every thread of the program shares. It starts at zero, or at a value that the compiler knows: numbers, `true` and `false`, characters, text literals, constants, `null` and enum variants, the name of a function, `ptr` of another file variable, and struct and array literals of them. The value is checked like the one of a local declaration, with the same conversions, and it goes in the data of the program, so it's there before `main`. Anything that would have to run is an error, nothing runs before `main` to compute it
```flag
limit: i32 = 10
names: array[&strview, 3] = ["one", "two", "three"]
origin: Point = {0, 0}
handler: func(i32) -> i32 = twice
count: i32
```
```
error: a file variable starts with a value the compiler knows, made of literals and constants, and this one is computed when the program runs
```
A text literal doesn't go to a `&str`, so a file variable of text you own starts empty and `main` gives it its text. A table that never changes is a file variable too, so it's not built again every time a function uses it.

Who sees it follows the module rules, like a function: `priv` keeps it in its module, and from another module it's `module.name`. A local variable or a parameter with the same name hides it. A file variable that holds a function is called by its name, `handler(3)`. It's not narrowed by `is` or a `case`, like a field: copy it to a local variable first. It's not a type, `x: count` is `count is a variable, not a type`.

The threads share the file variables and nothing protects them, so what more than one thread changes goes behind a `Lock` (see [Threads](#threads)).

### Contexts
A context is a value of each thread, with its fields at zero, that every function sees. Each thread has its own: a new thread starts with every context at zero and doesn't see what the thread that started it wrote there. That's why the scratch memory (see [Memory](#memory)) and the random numbers of `math` belong to the thread that uses them. A union without `null` inside it gets its tag when the thread starts, the first one before `main` (see [Unions](#unions)), so a context takes no room in the program file however big it is, except on Windows, which keeps the zeros of thread data in the file. C code that runs before `main`, like a constructor in a C file you link, would see those unions without their tag. A context has only fields, no values, and a nested block makes a type like `app.inner`. It's also a type and a value, so you can print it or take `ptr(app.depth)`, an address that is good while that thread runs. Who can see it follows the module rules, like a type.

## Threads
`parallel f(x)` runs the call `f(x)` in a new thread and gives a `Thread[T]`, where `T` is what `f` returns, `Thread[null]` when it returns nothing. `wait t` waits for that thread to end and gives its `T`, and `done(t)` says if it ended, without waiting
```flag
func sum(from: i64, to: i64) -> i64:
    total: i64 = 0
    for i in from..to:
        total += i
    return total

func main():
    a := parallel sum(0, 500000)
    b := parallel sum(500000, 1000000)
    print(wait a + wait b)
```
- `parallel` goes before a call and nothing else: `parallel goes before a call, it runs that call in a new thread: parallel f(x)`. The call is chosen like any other, with its overloads, generics, defaults, `@here()` and the address that a `*T` parameter takes, and its arguments are computed in the thread that starts it and copied to the new one. A function value runs too, `parallel steps[i](x)`. A builtin like `print` or a conversion, and a C function, don't run in a thread: wrap them in a func
- `parallel` and `wait` are unary operators like `try`, with its precedence. `wait t` alone on its line drops the value like a call does, so an error in it must be handled (see [Errors](#errors)), and `x := wait t` with a `Thread[null]` is `a function that returns nothing is a statement, not a value`. `wait` of anything else is `wait expects a thread on its right, and i32 is not one`
- A local variable that keeps a thread and that nothing reads doesn't compile: `t keeps a thread and nothing reads it: wait for it, or start it with parallel f(x) alone on its line to let it run on its own`
- `parallel f(x)` alone on its line starts a thread that nobody waits for. Its function can't fail, nothing would see the error: `this thread runs alone and nothing would see an error of i32|Bad, which its function gives: keep the thread and wait for it, t := parallel f(x)`
- `Thread[T]` is 8 bytes, the address of a block on the heap where the thread finds its arguments and leaves its result. `wait` frees the block, so a thread is waited once, and one that nobody waits is a block that is never freed, `-memory` tells you. The block of a thread that runs alone is freed when it ends. A `Thread` that never got a thread, like a field at zero, stops the program when it's waited or given to `done`: `flag: wait of a thread that was never started`, `flag: done of a thread that was never started`
- Every thread ends before the program does: when `main` ends, before its deferred statements run, it waits for the threads that still run, so a thread that runs alone can use the variables of `main` and what they own. Then the program writes what waits in the files. `main` waits only in a program that has a `parallel`. `exit()` in any thread ends all of them, and a runtime error or a crash in any thread ends the program with its message, its place and the chain of calls of that thread (see [Runtime errors](#runtime-errors)). When two threads fail at once, the first one writes its report and the others wait for the end, so two reports never mix
- Each thread that `parallel` starts has 8 MiB of stack, on every system. The first thread, the one of `main`, has the stack of the system: 8 MiB on Linux and macOS, and 1 MiB on Windows
- Each thread has its own contexts, at zero when it starts, and its own scratch memory. `wait` takes the blocks of the scratch of the thread that ended into the scratch of the thread that waits, as if a call had made those temporaries, so the result can be a temporary, like the path of an error of `read`, until the `with scratch():` around the `wait` ends. A thread that nobody waits frees its scratch when it ends, and so does one whose result holds no pointer. An argument is copied as it is, so a temporary given to a thread still lives in the scratch of the thread that started it: give the other thread text you own, a `&str`
- File variables are shared and nothing protects them, guard what two threads change with a `Lock`, an `Arena` too. `-memory` doesn't look for two threads that use the same memory at once

`Lock`, `Channel[T, N]`, `done` and `cores` are in the library (see [Threads](Library.md#threads)).

On Linux and macOS a thread is a `pthread`, and a `Lock` and the waits of a `Channel` are a `pthread_mutex_t` and a `pthread_cond_t`. On Windows they are `CreateThread`, an `SRWLOCK` and a `CONDITION_VARIABLE`. They all work at zero, so a `Lock` or a `Channel` needs no setup: on macOS, where the zero of a mutex is not ready, the library writes the signature of its initializer the first time. A Linux program links with `-lpthread`, that is empty on a recent C library.

The compiler writes a start function for each type of function that runs in a thread: it writes the tags of the contexts of the new thread, keeps the stack where the handler of a crash runs, takes the arguments from the block, calls the function, leaves the result in the block and tells the library that the thread ended, and if its result can point to its temporaries. The start function belongs to the library, so the chain of calls leaves it out. The contexts are thread data of QBE, so each thread has its own copy.

`print` formats all its values into one line and writes that line at once, so the lines of two threads never mix. A file has a lock for writing, that a write takes after the value is formatted, so an `operator +=` that prints never waits for its own lock, and another one for reading, so a thread that waits to read a file or a connection doesn't stop the ones that write. A read, `flush()`, the end of the program, a runtime error and a crash write what waits in every file whose lock for writing is free, and leave the others to the thread that holds it: so a thread stuck sending to a client that doesn't read stops nobody else.

## Modules
- `module name` goes in the first line and `use` goes before the first declaration. A file doesn't `use` its own module, it already sees it
- `use a` gives the names of `a` and `a.name`, `use a as m` gives only `m.name`. Without the `use`, `a.name` is an error. A nested type of a module is `a.Car.motor`
- `priv` hides a name from the files outside the module
- a variable, a parameter or a constant cannot have the name of a module the file uses, or of its alias: `bits names a module in this file, a variable or a constant cannot take it`
- in a file without a `module` line, `priv` hides the name from every other file. For its `priv` names that file is a module of its own, named like the file, and the messages say so: `helper is priv to module a.flg, only the files of a.flg see it`
- a `priv` name of a file without a module wins inside that file over a global one with the same name, so what a file keeps for itself never collides with the names of other files
- a module that has a `priv` function keeps its name: a call to that name from inside the module chooses among the functions declared in the module first (in the file, for a file without a `module` line), `priv` or not, and the best fit of them wins over any function from outside, even one with closer types. So a function of yours never replaces a helper of the library
- a name declared in two modules you use is an error when you use it without the module: `Thing is declared in both a and b, name the module: b.Thing`
- when two functions fit a call equally, the one of your module wins over a global one, and a global one over one of another module. A function that fits better wins wherever it is, that's how your `put_text` or `==` is called from inside the library
- `main` must be global, a `main` inside a module is not the start of the program
- the global names of the library, those of core like `File`, `Integer`, `FsError` or `stdout`, give way to yours. Declare a `type File` and your files see yours while the library keeps its own, which the messages then call `core.File`. A function of yours with the same parameters as one of core's is the one your files call, and core keeps calling its own

The library modules are `fs`, `os`, `json`, `net`, `math`, `utf8`, `bits`, `ring` and `raylib` (see the [Library](Library.md)).

## Memory
Flag doesn't have a garbage collector, you free what you allocate.

### Who owns what
- `&str`, `&list` and `&map` own their memory, you free them with `free`
- `&strview` and `&slice` are views, they don't own anything
- `heap(value)` copies a value to the heap and gives you a pointer, you free it with `free(p)`. A pointer to a `&str`, `&list`, `&map` or `Arena` takes their own `free`, which releases what they hold, so its block is freed with `free(p as *bytes)`
- `arena(a, value)` copies a value to the arena `a`, and `free(a)` releases it with everything else in `a`
- a list of `&str` owns each text, freeing the list doesn't free them, free them first

### Temporary memory
Many functions give you a temporary result, that lives in a scratch memory that you don't free: `+` on text, `cstr`, `split`, `replace`... The [Library](Library.md) says which ones. The scratch memory is made of blocks of 64 KiB that are reused.

`with scratch():` marks the scratch memory, and when the block ends everything temporary made inside it is released. Keep a temporary after that and it's garbage, so copy it to a `&str` if you need it later
```flag
kept: &str
defer free(kept)
with scratch():
    line := "id " + 42
    kept = line # copies the text into memory you own
print(kept)
```
A program that never uses `with scratch():` keeps every temporary until it ends. That's fine for small programs, and for loops that run a lot put the body inside `with scratch():`.

The scratch memory is an `Arena` of core, in a context, so each thread has its own, and `scratch()` and `close` mark it and go back to the mark.

### Arenas
An `Arena` gives out pieces of big blocks that it asks the heap for, 64 KiB each, or the size of a request that doesn't fit in one. It's a struct of core that starts empty, like a list, and every place that allocates has its verb: `heap(v)` the heap, `temp(...)` the scratch memory and `arena(a, ...)` the arena `a`. Nothing chooses an arena for you, `heap` always uses the heap.
```flag
level: Arena
defer free(level)
boss := arena(level, Enemy{hp = 100}) # a *Enemy in level
enemies: &list[Enemy] = arena(level, []) # an empty list that lives in level
append(enemies, Enemy{hp = 10}) # grows in level
name: &str = arena(level, "boss") # text that lives in level
name += " final" # grows in level too
```
- `arena(a, v)` copies the value into `a` and gives a pointer to it, like `heap(v)`. The pieces are aligned to 8 bytes, the most any type needs
- `arena(a, text)` with a `&strview` or a `&str` gives a `&str` with a copy of the text
- `arena(a, [...])` and `arena(a, {...})` build the literal as a `&list` or a `&map` that lives in `a`, with the type of the place where it lands, also of a union with one list or map in it: `bytes: &list[u8] = arena(a, [1, 2])`. Without one the literal gives its own, `arena(a, [1, 2])` is a `&list[i32]`, and an empty literal needs one: `this empty list lives in an arena and gets its type from where it lands, and nothing here gives a &list: x: &list[i32] = arena(a, [])`, and one that lands in another type is an error too: `this empty list lives in an arena as a &list, which cannot land in &ring.ring[i32]`. The compiler builds these, so `arena` with a list or map literal is always this one, also when you declare an `arena` of yours
- `a` is the `Arena` or a pointer to one, and anything else is an error: `arena(a, ...) expects the Arena it allocates in, or a pointer to one, but got i32`
- `clear(a)` releases everything in `a` and keeps its blocks to give them out again, for the memory of one frame of a game. `free(a)` releases everything and returns the blocks to the heap. After both the arena is empty and ready to use again

A `&str`, `&list` or `&map` keeps its arena in the field `arena` and grows there: it takes a new piece and copies, and the old piece stays until `free(a)` or `clear(a)`. A `&str` or a `&list` whose memory is the last piece of the block grows in its place, and a `&map` takes a new table each time it grows. `free` of one leaves it empty and releases nothing, so a function that frees what it is given works with a list from anywhere, and it keeps its arena, so growing it again uses the same arena. Assigning an address to it by hand (`text = ptr(buf) as *bytes`) changes only the address, like for any fat pointer.

The pointer of `arena(a, v)` doesn't know its arena, and `free` of it is a mistake that `-memory` reports. A `&ring` of the module `ring` always lives in the heap, `arena` doesn't make one. A copy of an `Arena` gives out the same blocks as the original, so pass a pointer to it, the functions that take one take `*Arena`. `-memory` warns about a copy of one that gave memory, and about a function of yours that takes one by value. An arena is not shared between threads without a `Lock`: give each thread its own, or guard every use of a shared one, also the lists and texts that grow in it.

## The memory checker
`-memory` reads the IL of your program and follows every allocation, and warns about
- memory that is never freed, or that some path doesn't free: `the memory allocated here for names is never freed`, `the memory that make allocates here may never be freed`
- memory freed twice: `free cannot release p, it may be freed already`
- memory used after it was freed, also after a list grew and moved: `first points to memory that may be freed already`
- freeing what is not a heap block: `free cannot release x, it is not on the heap`, `free cannot release p, it points inside a heap block, not at its start`, `free cannot release t, it is temporary memory: a with scratch() block releases it`
- temporaries used after their `with scratch():` ended: `kept points to memory that may be freed already`
- a pointer to a local variable that leaves its function: `a pointer to buf cannot leave name, buf lives on its stack`
- a function that frees one argument and reads another that may be the same memory, like `t += t`: `this call may free memory that it also reads through another argument`
- threads: `parallel f(x)` is checked as the call `f(x)` there, so what `f` frees counts at that point. A `Thread` that is never waited is memory that is never freed, waiting it twice frees it twice, `wait cannot release t, it may be freed already`, and `done(t)` after `wait t` reads freed memory. A pointer to a local variable that goes to a thread whose `Thread` leaves the function is `a pointer to x cannot leave start, x lives on its stack`, and so is any pointer to the stack kept in a block of the heap that leaves it, that goes with a thread that runs alone, or that goes to a `Channel` that is not a local variable of the function. `main` waits for the threads that run alone before its variables end, so there they are clean
- `send` of a `Channel` gives what the value owns to the thread that receives it: the memory of a `&str`, `&list` or `&map`, the heap block that a pointer points to the start of, and a `Thread`. It's not yours to free any more, and using it after is using memory that may be freed already. A view, the arena of a text and a pointer inside a block stay yours
- a temporary that a thread may still use when its `with scratch():` releases it, given to `parallel f(x)` or sent with `send`: `a thread may still use this temporary when its with scratch() releases it: wait for the thread inside it, or give it a &str`, with a note at the `with`. A thread waited inside the same `with scratch():` is clean. A thread that runs alone, in a function that is not `main`, can outlive any scratch: `a thread may still use this temporary when the with scratch() of a caller releases it: give the thread a &str`. What a thread gives back lives in the scratch of the thread that waits, until the `with scratch():` around the `wait`
- a copy of an `Arena`, a `Lock` or a `Channel` in use, also inside a struct or an element of a list: an `Arena` that gave memory, a `Lock` that was taken, a `Channel` that was used. `this copies level, an Arena that may have given memory: the copy gives out the same blocks, use a pointer to it` and `this copies the Channel jobs in pool, that may be in use: the copy is another channel, use a pointer to it`. A copy at zero, like `w := World{}` or `append(worlds, World{})`, is clean. A function of yours that takes one by value warns at every call: `make takes level by value, a copy of an Arena: make it take *Arena`
- memory of an arena used after `free` or `clear` of the arena, like a temporary after its `with scratch():`: `boss points to memory that may be freed already`, with a note at the `free` or `clear`. Through a function too, `reset(level)` that calls `clear`
- freeing what lives in an arena: `free cannot release boss, it lives in level, free(level) releases it`, and with the arena in a struct, `free cannot release level.boss, it lives in level.arena, free(level.arena) releases it`
- an `Arena` that is never freed: its blocks are on the heap, so `the memory allocated here for level is never freed`, at the first allocation in it

What lives in an arena belongs to the arena: it's never a leak, and freeing a list or a text that lives in one is clean too.

A message names your variable when there is one, with the fields you wrote after it, `level.boss`, and says `this memory` when there is none. An element of a list or an array takes the name of the list. It names a function only when you wrote the call: `drop cannot release path, it is temporary memory: a with scratch() block releases it`. The calls the compiler writes for you, for `print`, `+`, `for` or a conversion, are never named.

A warning has a note that points to the other place involved. `it may be freed here` points to the `free`, or to the call that freed or moved the memory, and `it is still not freed when this returns` points to the `return` that loses it, or to the name of the function when it ends without one
```flag
func main():
    names: &list[i32]
    defer free(names)
    append(names, 1)
    first := ptr(names[0])
    append(names, 2)
    print(first.*)
```
```
warning: first points to memory that may be freed already
  --> main.flg:7:5
   |     print(first.*)
   |     ^
note: it may be freed here
  --> main.flg:6:5
   |     append(names, 2)
   |     ^
```

### Elements
It sees all the elements of a list, an array or a map as one, so when you free one element it cannot tell which. It takes a free through an element as the free of all of them, and an element stored in another list, array, map or context as all of them stored there. So this is clean
```flag
for s in names:
    free(s)
free(names)
```
and so are `for i in 0..names.len: free(names[i])`, `for e in m: free(e.value)`, an element taken with `pop`, `remove` or `names[0]` and freed, elements moved to another list that frees them later, and the text that each thread of a list gives, freed as you wait them one by one. What the heap keeps of an element freed through a variable stays as it was, like the element in the list. Freeing the list and not its elements still warns, `the memory allocated here for s is never freed`, and so does using an element after you freed it through the same variable.

The price is what it doesn't see: freeing only some of the elements, freeing them twice, or reading one through the list after it was freed, give no warning.

### What it doesn't know
It doesn't follow the values of the conditions, it looks at every path. It does follow `is`: after `if node is *Node:` it knows that the other path has no node to free. And it keeps apart what each member of a union holds, also when two members keep a pointer in the same place, like `Shape` and `Bad` here, and when the paths that made the union meet
```flag
type Shape:
    points: &list[i32]

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
        print(s.reason) # the memory of a Shape is not lost here, this path has none
        return
    free(s.points) # this frees no text of Bad, s holds a Shape
```

These give warnings that are not real
- freeing in `if c:` and again in `if not c:` warns that it may be freed already and that it may never be freed
- linked lists and trees whose nodes are allocated in a loop or by a recursive function. It sees all the memory that one line allocates in a loop as two blocks, the newest one and all the others, and a node freed through a pointer that may be either of them is not surely freed
- it doesn't know when a thread reads what it gets: a temporary given to a thread that is waited after its `with scratch():` warns, also when the thread read it before. A temporary sent with `send` warns while any thread runs, it doesn't know which one receives it. And a pointer to the stack sent to a `Channel` that is not a local variable warns, also when every thread that receives it ends before the function
- a pointer that may point to one of several fields, when you store or free through it
- a union that a generic function copies, like `append` putting it in a list: inside the function it is only a value of type `$T`, so the checker mixes what the members hold, and after `if t is &str:` on an element taken back, `free(t)` can warn that it is not on the heap
- memory given to a C function that keeps it or frees it warns that it is never freed, a C function is taken as one that only reads what it gets. A call through a function value is taken the same way
- a text appended to itself, `t += t`: `+=` copies the text first when it is a view of the same memory, but the checker only sees that it may free what it reads
- memory that a function frees more than three pointers away from its parameters, like `free(p.a.b.c.d)`: the caller is told that it is never freed

`@trust` at the end of the line hides them (see below).

What it cannot follow it leaves alone, without warnings
- what a function reads or writes more than three pointers away from its parameters, like `p.a.b.c.d`. Only a `free` that far gives a warning, the one above
- what is past the first 4096 bytes of a struct or an array, when the index is a literal
- what you store in memory that a C function gave you

A pointer that you store as an integer is lost. The checker tells you, `-memory cannot follow x once it is stored as an integer, store it with a pointer type`, and then reports that memory as never freed.

And it gives up on a function that is too much for it. It still shows what it found there, but it doesn't see everything
- `-memory gives up on f, it has more than 512 blocks of memory to follow, so its checks are incomplete`. A block is every place that allocates, every temporary and everything a parameter points to, and the checker keeps a few for itself, so about 500 are yours. Split the function
- `-memory gives up on f, its memory use does not settle, so its checks are incomplete`

### @trust and @check
`@trust` hides the warnings of code you already checked
- at the end of a line it covers that line and the lines inside it
- on a line of its own it covers the lines after it at that indentation, until the indentation goes back, and at the top of a file it covers all the file

`@check` does the opposite inside a trusted part. A function under `@check` also keeps showing its warnings when trusted code calls it.

The checker skips the trusted functions that no untrusted code needs, so trusting code also makes `-memory` faster. The compiler itself is all trusted.

## Debugging
`-debug` puts DWARF in the program, what gdb and lldb read to show its variables, its types, the chain of calls with the line of each call, and to step line by line
```sh
flagc main.flg -o main -debug
gdb ./main
```
- each file is a compile unit, and each function has its name, the line where it's declared and its code. The instances of a generic share its name, so `break sort` stops in all of them. A breakpoint in a function stops at the first line of its body, with the parameters in their place
- `break main` stops twice, first in the `main` of C that starts the program and has no lines, then in yours: `break main.flg:main` stops only in yours
- `break panic` stops at a runtime error, before it writes its message, and `bt` shows the calls that led there with their arguments. A crash stops the program in the debugger by itself, at the instruction that failed
- the parameters and the variables are in their slot of the stack, inside the blocks of code where they are visible, so the `i` of one loop and the `i` of the next are two variables
- the variables at the root of a file are global variables, and the contexts are thread-local variables, so the debugger shows the ones of the thread it stops in. On Linux for arm64 and riscv64 a context has no place in the DWARF: no relocation of those systems gives the place of thread data to it, gcc and clang leave it out too
- numbers, `bool` and pointers are the types of C, and a `*u8` is a C string, so the `data` of a text shows the text, and what follows it too
- a struct has its fields, and a struct, an enum or an alias you declare is also a typedef of its name, without its module, so `ptype Point`, `ptype Color` and `ptype Shape` work, where C would want `struct Point` and `enum Color`. A fat pointer is a struct with `data` and its fields, like `len`, `cap` and `arena`, a function value is a pointer to a function, and a `Thread[T]` is a struct with the address of its `block`
- an enum is an enumeration with its variants, and their values are the tags of that compilation (see [Unions](#unions))
- a tagged union is a struct with its `tag`, an enumeration of its members, and the member it holds as a variant part, like Rust does: `{tag = (Point), Point = {x = 1, y = 2}}`, or `{tag = null}` for a member with no data. A member with data is `(Point)` in the enumeration, since in C an enumerator `Point` and the type `Point` share their names, and gdb would take one for the other. gdb shows that member, but in an array it shows every member of each union, and `print shapes[1]` the one it holds. lldb reads variant parts only in Rust code, so it may show only the tag
- an untagged union is a union of its members

The compile units say they are C, that's how both debuggers print a value: `{x = 3, y = 4}`.

A function whose parameters and result are numbers, `bool`, pointers and function values is called like a function of C, so `print twice(21)` runs it. One that takes or returns a struct, a union, an array or a fat pointer passes it its own way: QBE puts a struct, a union or an array in memory where C uses registers, and one of more than 128 bytes, or a fat pointer, goes by its address. The DWARF says so with `DW_CC_nocall`, so gdb doesn't call it, `Cannot call the function 'area' which does not follow the target calling convention`, and `finish` doesn't show what it returns. With lldb, call only the first kind. An `export func` passes everything like C, so both debuggers call it.

The program changes so that what the debugger reads stays true. Nothing is inlined (see [Inlining](#inlining)), every variable stays in its slot, QBE doesn't move it to a register or cut a small struct into pieces, and a parameter passed by its address, like a struct, gets a slot that holds that address. The blocks keep the order they have in the code, so a block of variables is one range of addresses, and each block starts a row of the table of lines, so stepping through a loop comes back to its line on each turn. A program with `-debug` is a little slower and bigger, the compiler is 3 % bigger with it, and it compiles 9 % faster, there is no inlined code for QBE and the assembler.

On macOS the linker leaves the DWARF in the object file, so `-debug` assembles the program into `program.o`, links it, runs `dsymutil`, that writes `program.dSYM` next to the program, and removes the object unless `-keep`. A `dsymutil` that cannot run is an error: `cannot run dsymutil: ..., -debug on macOS keeps the DWARF in main.dSYM with dsymutil`.

On Windows gdb and lldb read the DWARF too. The debugger of Visual Studio reads PDB, which flagc doesn't write.

## Calling C
### extern
An `extern` block declares C functions and says which library they come from. `"c"` is the C library, and any other name is linked with `-lname`
```flag
extern "m":
    func cbrt(x: f64) -> f64
```
A block can have a `where` like a function, and then its library is linked only when the condition holds. A block without functions just links its library, that's how a library says that it needs another one
```flag
extern "ws2_32" where @windows():
    func WSAStartup(version: u16, into: *bytes) -> i32

extern "m":
```
You can also give the compiler `.c`, `.o` and `.a` files, they go to the C compiler with the rest.

### export
`export func` is a function that C can call: it keeps its plain name as the symbol, uses the C calling convention and is built even if nothing in Flag calls it. It cannot be generic, overloaded, `inline` or `main`. `ptr(name)` of an export function is its address as a `*bytes`, for C functions that take a callback
```flag
extern "c":
    func qsort(base: *bytes, count: usize, size: usize, compare: *bytes)

export func ascending(a: *i32, b: *i32) -> i32:
    return a.* - b.*
```
`ptr` of a function that is not exported is an error, a callback needs the one fixed signature an export has. Inside Flag you don't need `ptr`, the name of a function is already a value (see [Function values](#function-values)), but that value doesn't cross to C.

### Types
| Flag | C |
|---|---|
| `i8`, `u8`, `bool` | `signed char`, `unsigned char`, `bool` |
| `i16`, `u16` | `short`, `unsigned short` |
| `i32`, `u32` | `int`, `unsigned int` |
| `i64`, `u64`, `usize` | `long long`, `unsigned long long`, `size_t` |
| `f32`, `f64` | `float`, `double` |
| `*T`, `cstr` | pointers |
| structs, arrays inside structs, untagged unions | the same struct or union by value |

Tagged unions, fat pointers and function values can't cross to C, pass their parts: `ptr(text.*)` and `text.len`. The signature of a C function is checked where you declare it, `&strview cannot cross the C ABI, only integers, floats, bool, pointers, structs and untagged unions of them do`. Be careful with `long`, it's 64 bits on Linux and macOS and 32 bits on Windows.

### Variadic functions
A C function can end with `...`. The extra arguments must be integers, floats, `bool` or pointers, not function values, `f32` goes as a `double` like C does, and text is not converted there, use `cstr`.

### Text
`cstr` is `*u8`, a text that ends with a zero byte
- a text literal goes as a `cstr` without copying, literals already end with a zero
- a `&str` goes as a `cstr` without copying when it has room for the zero after its text, and as a temporary copy when it's full. It never grows the text, so the other copies of that `&str` stay good
- any other `&strview` needs `cstr(text)`, that makes a temporary copy
- `view(c)` turns a `cstr` back into a `&strview`

C's `printf` keeps what it prints in a buffer of its own, so into a pipe or a file its lines and the ones of `print` come out of order. Call `flush()` before C prints, and C's `fflush(NULL)` after it.

## Platforms
Flag builds programs for Linux, macOS and Windows
| Target | QBE backend |
|---|---|
| `linux-x86_64` | `amd64_sysv` |
| `linux-arm64` | `arm64` |
| `linux-riscv64` | `rv64` |
| `macos-x86_64` | `amd64_apple` |
| `macos-arm64` | `arm64_apple` |
| `windows-x86_64` | `amd64_win` |

By default the target is the system flagc was built on. The code chooses between systems with `where` and the platform builtins, the standard library does it this way.

### Cross compiling
`-target` builds for another system, and you need a C compiler that links for that system. For example, from Linux to Windows with [zig](https://ziglang.org)
```sh
CC="zig cc -target x86_64-windows-gnu" flagc -target windows-x86_64 game.flg -o game.exe
```

### Windows
- Programs get `.exe`: `a.exe` by default, and `flagc run` writes its program in `%TEMP%`
- The standard input and output are binary, so `print` writes `\n` like on Linux, and `read_line` removes the `\r` of `\r\n` (it does it on every system)
- The paths that `os` gives you use `/`, and `fs` takes `/` and `\`
- `run(command)` and `output(command, into)` go through `cmd.exe`, and `run(program)` passes each argument quoted so the program gets it exactly. That's why it runs a name without an extension as a `.exe` and refuses a `.bat` or `.cmd`, which `cmd.exe` would read again
- `net` uses Winsock, and the first time a program listens the Windows firewall may ask you
- The `raylib` module only has the Linux build of raylib, for Windows and macOS put your own `libraylib.a` in the `lib` folder
- The console shows text with its code page, so accents may look wrong unless the console is in UTF-8 (`chcp 65001`)
- Paths, arguments and environment variables go through the ANSI code page of the system, so a name with letters outside ASCII works only when that code page is UTF-8: the setting "Use Unicode UTF-8 for worldwide language support" of Windows 10 and later

## Editor support
These are for editors and tools, they don't build the program.

`-check` checks the code and prints the errors, it's the fast way to see if your code is right.

`-overlay file` replaces the text of some files with the text in that file, so an editor can check what you are typing before you save. The file is a list of entries, and each one is the path in a line, the length in bytes in another line, and that many bytes of text.

`-at file:line:column` says what is at that place, it prints a `definition` line with where it's declared and `hover` lines with its type or its declaration. The column counts bytes, like the ones in the errors. A place that is not inside one of the files is a wrong command line: `-at main.flg:1:18 is past the end of the line, its last column is 13`
```sh
flagc main.flg -check -at main.flg:12:5
```
```
definition main.flg:3:6
hover func add(a: i32, b: i32) -> i32:
```

## Diagnostics
Errors, warnings and notes go to the error output with the place and the line, and a `^` under the column
```
error: Point has no field z
  --> main.flg:7:13
   |     print(p.z)
   |             ^
```
Only the first error is shown, and a note follows it when there is another place involved. The messages always say what is wrong and why, if one doesn't, it's a bug in the compiler.

The place is `file:line:column`, both start at 1 and the column counts bytes, so a `ñ` before it counts two. The `^` is drawn under the character anyway. A byte that cannot be shown is written as its escape, `invalid character '\x1b'`.

When a line ends before it should, the error points after its last word
```
error: the line ends here, ':' goes here
  --> main.flg:2:13
   |     if ready
   |             ^
```

## Building the compiler
`./build.sh` builds the compiler in stages
1. It builds QBE from `vendor/qbe` and packs the library into an object file
2. Stage 0 is built from the seed, `boot/<system>.ssa`, the IL of the compiler for that system
3. Stage 0 compiles the sources into stage 1, stage 1 into stage 2 and stage 2 into stage 3
4. Stage 2 and stage 3 must write the same IL, that's the proof that the compiler compiles itself right

`./build.sh seed` writes the three seeds, one for each system, with `-target`. When the seed of your system is older than the sources the build tells you: `boot/linux.ssa is older than src: ./build.sh seed refreshes it`.

`./build.sh package` makes `build/flag-<version>-<system>-<machine>.tar.gz` with the compiler.

`./build.sh sanitize` builds QBE with AddressSanitizer and UndefinedBehaviorSanitizer, and turns the three seeds into assembly for the six targets, with and without `-g`: a read out of an array, a memory freed twice or a shift that's too big stops it. CI runs it on Linux, since such a bug can pass on one system and break on another, as a read past an array of the slots that are cut did on macOS. QBE is built with `-fwrapv`: it adds offsets and folds constants with signed numbers that wrap around, as the target does, and C would leave that undefined.

`./test.sh build/flagc` runs every test in `tests/`
- `name.flg` with `name.out` must print that, and the compiler must build it without printing anything
- `name.flg` with `name.err` must fail with that text in the error
- `name.flg` with `name.warn` is compiled with `-memory` and must warn with that text, or not warn if the file is empty
- `name.gdb` runs the program in gdb with those commands, where there is a gdb, and `name.out` is what gdb prints, without its lines about `libthread_db` and the `Thread 1 hit` that comes before a breakpoint on Windows, where a program always has more than one thread
- `name.args` has more arguments for the compiler in that test. With `-check` among them nothing runs, and `name.out` is what the compiler must print, that's how `-at` is tested
- `name.env` has the environment of the compiler in that test, a `NAME=value` on each line, and the compiler runs the way it's installed, a copy alone in `build/tests/bin` that unpacks the library it carries. That's how a missing `cc` and a library that cannot be unpacked are tested
- a file the test needs, like the overlay of `name.overlay`, goes next to it with the same name
- a folder is a test with all its files
- a test with no `name.flg` and no folder gives the compiler no files, only its arguments, that's how the usage is tested
- and every example in `examples/` must compile

QBE is in `vendor/qbe` with thirty-one changes: its `main` returns instead of calling `exit`, so flagc can call it, a function that can go past a page of stack before it touches it lowers the stack a page at a time and touches each one, and a constant that takes the place of a 32 bit load is cut to 32 bits, like amd64 now cuts every constant to the size of its operand, as arm64 and rv64 did, so the assembler has nothing to warn about. The probes are how a stack overflow meets the guard page of its thread: Windows grows a stack that way, and on Linux and macOS a frame bigger than the guard of a thread, 4 KiB, jumped over it into the stack of another thread. A function counts its frame and the stack its calls take below it, the arguments that go on the stack and on Windows the 32 bytes kept for the callee, so one that doesn't reach a page is written as before. The fifth is for macOS: a 64-bit value stored far in a stack frame of more than 32 KiB was written as 32 bits, so a function with that many locals could read a wrong number.

The other three make big functions fast, on every target. Whether a block dominates another is read from numbers given to the dominator tree, instead of walking up the tree. A temporary defined once, in a block that dominates all its uses, needs no phi and is not searched for one. And the search for a path between two blocks clears only the blocks it visited. A loop that turns an enum of 1500 variants into text took 37 s in QBE and takes 0.2 s, and a function with 2400 `l[i]` took 2.5 s and takes 0.3 s.

Three more keep long chains from growing with their square. The dominators of a block are found from its last predecessor to its first, so the end of an `elif` chain meets them one step at a time. The walk that fills the dominance frontier stops at a block that already has it. And the constants of a function are found through a hash instead of a search. 20000 `elif` took 38 s and 8 s after them, a switch of 20000 cases 34 s and 7.6 s, a `case` with 20000 values 15.6 s and 0.3 s, and an array literal of 50000 numbers 9.3 s and 0.8 s.

Six more keep a long function linear, in memory and in time. A set of temporaries, like the ones liveness and the register allocator keep for every block, holds only its words that are not zero, by their index. A set had a bit for every temporary of the function, so the sets of a function took its blocks times its temporaries, and spilling made a new one at each instruction, where it now reuses two. Three walks no longer go through every block between two others, which in a chain of `elif` is the whole chain, because its dominator tree is as deep as the chain: the search for a jump that already decided a value stops before the first definition of the value, the common dominator of two blocks climbs from one side until it dominates the other, and the best block for an instruction jumps to the closest dominator in fewer loops. And amd64 clears, for each block, the entries of the temporaries it uses, instead of its table of all of them. The sixth is in the search for the phis of a temporary defined in more than one place, that walked every instruction of the function for each such temporary and now walks its definitions: the pieces of a cut slot, below, are defined at least twice, so a chain of 3000 copies of a struct took 1.1 s and takes 0.15 s. QBE writes the same program, and the compiler builds itself with 0.5 % fewer instructions after the first five, and 3.9 % fewer after the sixth. 20000 `elif` with a `print` each took 3.1 s and 1.9 GB, and take 1.6 s and 0.4 GB. When they test a value known only when the program runs, nothing folds: 5000 took 3.6 s and 1.8 GB, growing with the square, and 20000 take 2.8 s and 0.5 GB.

The eighteenth is for the contexts, that are thread data. `amd64_win` takes thread data: it goes in the section `.tls$`, and its address is the block of the thread in the table at `gs:88`, at `_tls_index`, plus the place of the symbol in it. And on Windows and macOS, where that address costs a lookup or a call, each function reads it once, after its parameters, and keeps it in a temporary. An address of thread data that a call or a `sel` takes is made there too, since the arguments of a call, and the two halves of a `sel`, stay together. On macOS that also fixes the address of thread data given as an argument of a call, whose call to the runtime broke the arguments already in their registers, a function whose only call is that one aligns the stack for it, and thread data that starts at zero goes in `__thread_bss`, so it takes no room in the program file. On Linux the address costs nothing: the compiler builds itself with the same instructions, 0.1 % more when it only checks.

The nineteenth and the twentieth come with the inlining. A small slot of the stack, of 32 bytes or less, whose address only reaches loads, stores, blits and additions of constants, is cut into one slot for each piece that is read or written, before `promote`, which then keeps the pieces in registers. That's how small structs, and the copies of them that inlined calls make, live in registers on every target. The pieces follow the edges of the accesses, carried from one side of a blit to the other, and again only through the blits of a slot that changed, so a chain of copies is not walked once for each link. A store of a constant is cut along them, a blit is copied piece by piece, and a piece that is read starts at zero, after the parameters, so no path reads it undefined. `qbe -g` keeps every slot whole, for the debugger. And rv64 copies a constant, or a value on the stack, into a temporary on the stack through `ft11`, where it stopped with `unimplemented`: the functions with cut slots need it.

Five more fix QBE itself. `promote` turned a `loadsw` of a slot written with `storew` into a copy, and the value lost its sign, and the forwarding of stores to loads kept the bits above a narrow store when a load that starts inside it took them shifted down: both happen in more slots since small slots are cut. GVN took two phis with the same arguments and different classes for the same value, and stopped with an assertion. The coalescing of slots shifted a mask by the offset of an access, that can be past its slot, which C leaves undefined. And on Windows a function keeps `xmm6` to `xmm15` for its caller, as the ABI asks: QBE took them as registers that any call changes, so a function of Flag that C calls, like a callback, could change the floats of the C around it.

The last six are for the chain of calls and for the debugger. On x86_64 every function keeps its frame pointer, as on arm64 and rv64. On Windows each function has its unwind information, `.seh_*`, which Windows and its debuggers read to find the end of the prologue, the caller and the registers kept for it. Those are saved right below `rbp` once `rbp` is set, `[rbp]` the `rbp` of the caller and `[rbp+8]` the return address, so they are found from `rbp` wherever the body moves `rsp`. `hlt` is an instruction that doesn't exist on every target, `udf` on arm64 and `unimp` on rv64 like `ud2` on amd64: it was a breakpoint there, and a crash handler doesn't take breakpoints. Each function ends with the label `.L<name>.end`, `L<name>.end` on macOS and Windows, which gives the table of functions and the DWARF its end. The instruction `%r =l frame` gives the frame pointer of the function. And `qbe -g` builds for a debugger
- `dbgslot %t, n` says where the `alloc` `%t` is, with `.set .Lslot<n>, <offset from the frame pointer>`, and that keeps it in memory, whole
- `dbgmark n` writes the label `.Lmark<n>` there, and the mark of a block that dies goes to the start of the block after it
- the blocks keep the order of the IL, and one is not merged into the block before it, so a scope is one range of code
- each function has its `.cfi` in `.debug_frame`, with the registers it saves, and its first `.loc`, the line of its declaration, before its prologue and not again after it: a debugger that skips the prologue stops at the next line, the first one of the body

## Rough edges
Things that work in a way that may surprise you, so be aware
- A text literal is static data that every use shares, and so do the literals with the same text and the constants kept in data (see [Constants](#constants)). Don't write into a view of a literal, copy it to a `&str` first
- `v: &strview = some_cstr` sets only the address and leaves `len` at 0 (see [Pointers into fat pointers](#pointers-into-fat-pointers)), `view(c)` is what reads the length
- A function value of an overloaded or generic function needs a place that says its type. An argument is not one, so give it to a variable with its type first
- `trust` on a real error gives garbage, and it can crash
- `receive` of a `Channel` gives `null` when the channel is closed and empty, so on a channel of `T?` a `null` that was sent looks the same: walk it with `for v in channel`, that ends only when it's closed
- A new thread starts with its contexts at zero, it doesn't see what the thread that started it put there: pass it what it needs, or share it in a file variable
- The memory checker has the false positives of [What it doesn't know](#what-it-doesnt-know), and it misses the element mistakes of [Elements](#elements)
- `a = b` between two `&str` doesn't copy the text, both share it, only a `&strview` is copied. Write `a = view(b)` to copy
