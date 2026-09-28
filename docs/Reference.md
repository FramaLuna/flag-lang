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
- [Constants and contexts](#constants-and-contexts)
- [Modules](#modules)
- [Memory](#memory)
- [The memory checker](#the-memory-checker)
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
4. That list is written as QBE IL, the text language of the [QBE](https://c9x.me/compile/) backend (`src/ir.flg`), and QBE, that is built into flagc, turns the IL into assembly
5. The C compiler assembles it and links it with the C library

With `-memory` there is one more step between 3 and 4, `src/mem.flg` reads the instructions and checks the memory (see [The memory checker](#the-memory-checker)).

The generator starts from `main` and only the code that `main` can reach ends up in the program. But every function that is not generic is checked, even if nothing calls it, so an error in a function you don't use still stops the compilation. Generic functions are checked when a call makes an instance of them.

The compiler stops at the first error.

The IL and the assembly are written next to the program, `program.ssa` and `program.s`, and they are removed when the program links, unless you pass `-keep`. If QBE or the link fails they stay there so you can look at them.

## The command line
```
flagc [run] file.flg folder ... [-o program] [-keep] [-check] [-memory] [-target system-machine] [-overlay file] [-at file:line:column] [-version] [anything else goes to cc] [-- arguments for run]
```
- An argument that ends in `.flg` is a file to compile, and a folder adds every `.flg` inside it and inside its folders, sorted by name. The order of the files doesn't matter, all of them are compiled together
- `run` has to be the first argument. It compiles and then runs the program with what goes after `--`. Without `-o` the program is written in the temporary folder (`TEMP`, or `/tmp` if it's not set) as `flag-run-<number>` and removed after it runs. The exit code of flagc is the exit code of the program
- `-o` names the program, `a.out` if you don't give one, `a.exe` on Windows
- `-check` stops after checking the code, it writes nothing and it doesn't need a `main`
- `-memory` runs the memory checker, the warnings don't change the exit code
- `-keep` keeps the `.ssa` and `.s` files
- `-target` builds for another system (see [Platforms](#platforms))
- `-overlay` and `-at` are for editors (see [Editor support](#editor-support))
- `-version` prints the version and the commit it was built from
- Anything else goes to the C compiler, in the same order: `.c`, `.o` and `.a` files, `-l` flags, `-static`...

flagc exits with 0 when everything worked, 1 when there is an error in the code or when a step fails, and 2 when the command line is wrong.

### The C compiler
The C compiler is `cc`, or the one in the `CC` variable. `CC` is split on spaces, so `CC="zig cc"` works. The link is always
```
cc -o program program.s -L<library folder> -L<folder of each input> -l<each extern library> <anything else>
```
The `-L` of the folders of your files means that you can put a `libsomething.a` next to your code and use it with `extern "something"`.

If the C compiler cannot run you get
```
error: cannot run cc, flagc links with a C compiler and the CC variable chooses which
```

## Files, folders and the library
`core.flg` is always loaded first. It has no module, so all of it is visible everywhere, it's where `print`, lists, maps and text live.

The library is the `lib` folder next to the folder of flagc (`bin/../lib`). If it's not there flagc uses the copy of the library that is built inside it, and unpacks it the first time into a cache folder: `$XDG_CACHE_HOME/flag/<version>`, or `%LOCALAPPDATA%\flag\<version>` on Windows, or `$HOME/.cache/flag/<version>`, or `/tmp/flag/<version>`. So a single `flagc` file is enough to compile, and if you keep the `lib` folder next to it you can read and change the library.

When a file says `use something` and no file you gave declares `module something`, flagc looks at every `.flg` in the library and loads the ones whose first line is `module something`.

A file given twice is loaded once. The paths in the errors are the ones you gave, and the library is shown relative to the current folder when it's inside it.

## Syntax
### Lines and blocks
- A block is the lines after a `:` that are more indented than the line with the `:`. The width of the block is the indentation of its first line, and a line deeper than that is an error. Tabs and spaces count one each, so don't mix them
- A `:` always needs an indented block after it, there is no `if x: print(x)` in one line
- Declarations (`func`, `type`, constants, `extern`...) start at the first column, and code goes inside functions
- `module` goes in the first line of the file, and `use` goes before the first declaration
- Inside `()`, `[]` and `{}` the lines continue, so you can break long calls and literals at any indentation. There is no `\` to continue a line
- `#` comments until the end of the line, there are no multiline comments. Files with `\r\n` line ends work

### Tokens
- Names are letters, digits and `_`, and don't start with a digit
- `@name` is a builtin (`@linux()`, `@argc()`...), and `@trust`/`@check` are markers for the memory checker
- `$T` is a type parameter and `$$n` a number parameter
- Integers: `42`, `0xff`, `0b101`, and `_` can go between digits to read them better, `1_000_000`, `0xff_ff`. There are no octal numbers
- Floats: `2.5`, `1e3`, `1.5e-3`. A float needs digits on both sides of the `.` (`.5` and `5.` are not floats)
- Characters: `'a'` is one ASCII character or one escape, and it's the number of that character
- Text: `"..."` in one line, with the escapes `\n \t \r \0 \\ \" \' \e` and `\x41`, a byte by its two hexadecimal digits
- The two character symbols are `== != <= >= += -= *= /= %= -> := .. .* << >> ||`

`!`, `&&` and `||` are not the logical operators, it's `not`, `and` and `or`. The compiler tells you: `'!' is not an operator, write: not`.

## Types in memory
### Sizes
| Type | Size and alignment |
|---|---|
| `bool`, `i8`, `u8` | 1 |
| `i16`, `u16` | 2 |
| `i32`, `u32`, `f32` | 4 |
| `i64`, `u64`, `usize`, `f64`, pointers | 8 |
| void types, `null`, `IterationEnd` | 0 |

`bytes` has no size, it only exists behind a pointer (`*bytes` is the `void *` of C). A variable of type `bytes` is an error: `variable x needs a size, bytes is a run of unknown length`.

`sizeof(T)` gives the size as an integer literal, so `n := sizeof(i64)` makes an `i32`. Give the type when you want a `usize`: `n: usize = sizeof(i64)`.

### Structs
The fields are in the order you write them, each one aligned to its own size, and the size of the struct is rounded up to its biggest alignment, like C does
```flag
type S:
    a: u8   # offset 0
    b: i32  # offset 4
    c: u8   # offset 8, and the size is 12
```
A nested field is inline in the struct, and its type is named after the struct and the field, `Car.motor`.

A field with `@other` uses the same bytes as a field declared above it, it cannot be bigger than that field, and a literal can fill only one of the two. Printing skips the shared fields, and a struct with shared fields cannot be passed to C by value.

A `packed type` has no padding, each field starts where the one before ends and its alignment is 1, so `packed type T:` with `a: u8` and `b: i64` takes 9 bytes. Fields can share storage with `@` there too. A packed struct cannot be passed to C by value, pass a pointer.

A struct literal without the name, `{1, 2}` or `{x = 1}`, takes the type from where it goes: a variable with its type, an assignment, a `return`, a field of another literal or an element of an array with its type. Anywhere else there is no type to take: `a {...} without a type needs a place that gives it one: p: Point = {1, 2}`. `{}` there is the value with everything at zero.

### Arrays
`array[T, N]` is N elements one after the other, with the alignment of T. Arrays are values, assigning one copies all of it.

### Fixed maps
`fixmap[K, V, N]` is a struct with `len` and room for N entries, each one with a `used` flag, the key and the value, in the order they were put. Looking for a key goes through the entries, so they are for small maps. `sizeof(fixmap[i32, i32, 2])` is 32.

### Unions
A tagged union is a 4 byte tag at the start and the value at byte 8, so `i32?` takes 16 bytes and `&strview?` takes 24. A union of void types is only the tag, 4 bytes, that's why enums are small.

The tag is the id of the type inside that compilation, it can change when you change the program, so don't save it to a file.

A union that starts at zero holds its member with the lowest id, the builtin types first and then your types in the order they are declared. So `T?` starts as `null`, and an enum starts as its first variant.

The members are flattened, repeated ones removed and sorted, so `(i32|null)|f64?` is `null|i32|f64`, and that's how the errors write it.

An untagged union (`i32||f32`) is as big as its biggest member and has no tag.

### Fat pointers
A fat pointer is the address of the data and then its fields
| Type | Layout | Size |
|---|---|---|
| `&strview`, `&slice[T]` | address, len | 16 |
| `&str`, `&list[T]` | address, len, cap | 24 |
| `&map[K, V]` | address, len, cap | 24 |

A fat pointer starts with everything at zero, that's the empty one, and `append`, `put` and `+=` allocate the memory the first time. The map is a table of `cap` entries (`used`, `key`, `value`) with open addressing, `cap` is a power of two and at least 16.

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
6. `..`, `<<`, `>>`
7. `+`, `-`
8. `*`, `/`, `%`
9. `as`

The unary operators (`-`, `not`, `try`, `trust`) are tighter than all of them. So `not x == 3` is `(not x) == 3`, write `x != 3` or `not (x == 3)`. And `x and 3 == 2` is `x and (3 == 2)`, put the parentheses when you mix bits and comparisons.

## Conversions
### Implicit ones
When you pass an argument, the value is converted only if it's
- the same type
- a `&str` going to a `&strview`, or a `&list[T]` going to a `&slice[T]`
- a `*array[T, N]` going to a fat pointer of `T` (a list, a slice, or text if T is `u8`), it keeps the address and sets `len` and `cap` to N
- `[]` going to any fat pointer, it becomes the empty one, and `{}` going to a map
- a literal that fits the type
- a text literal, or a `&str`, going to a `cstr`
- a value going to a union that has its type, or a union going to a bigger union
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

Anything else is `cannot reinterpret i32 as i64, they do not have the same byte size`.

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

Narrowing only works on local variables and parameters, not on fields. Copy the field to a variable first
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
- after an `if`, the variable has the types of the branches that don't leave. So `if x is null: return` leaves `x` without `null` after it
- assigning a value that fits the narrowed type keeps it narrowed, any other value brings back the whole type. Inside a loop that is an error, because the loop reads the variable with the narrowed type every round: `v cannot take &strview inside this loop, an is outside the loop narrowed it to i32 and the loop reads it that way every round`

`==` between a union and one of its void types compares the tag: `x == null`, `color == Red`. To compare the value of a member, narrow first: `x is i32 and x == 3`.

## Functions and calls
### Which functions a call sees
A call sees the functions of its own module, the global ones (from files without `module`) and the ones of the modules it `use`s. It also sees the functions of the module that declares the type of an argument, even without `use`, so a type brings its functions with it.

### Arguments
The arguments fill the parameters in order, and named arguments fill their parameter. After a named argument the rest must be named too, and a parameter can't be filled twice.

A default value is computed at the call, every time it's used. It can use constants, contexts and functions, not the other parameters. `@here()` in a default is the place of the call as text, `"main.flg:12:5"`. A parameter without default can come after one with default, and then you name it
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
- a `*T` parameter with a variable of type `T` gives one less than `T` would, and passes the address of the variable. It doesn't work with a bare `*$T`, that one takes any pointer

The arguments that are not literals are matched first, so `$T` comes from your variables and the literals adapt to it: `biggest(2, u8(7))` makes `$T` an `u8`.

Then `where` removes the overloads whose condition is false.

When two overloads have the same score the nearest one wins: the one of your module, then a global one, then one of another module. If they are still tied it's an error, with a note for each one
```
error: ambiguous call to show, several overloads match (i32) equally
```

The order of the declarations doesn't matter, a function can call one declared below it or in another file. Functions are not values, you can't store one in a variable.

A text literal doesn't go to a `&str` parameter, because a `&str` is text that you own. Take `&strview` when you only read the text, a `&str` also goes there.

### Protocol functions
The compiler calls some functions by their name, so you make your types work with the language by declaring them
| You write | It calls |
|---|---|
| `x[i]` | `at(x, i)`. If it returns a pointer, `x[i]` is the element and you can assign to it, if it returns a value that's what you read, like `V|null` for a map |
| `x[i] = v` | when `at` returns a value, `put(x, i, v)` |
| `x[a:b]` | `slice(x, a, b)` |
| `for v in x` | `iter(x)` and then `next(ptr(state))` |
| `for v in reverse x` | `reverse_iter(x)` and then `next` |
| `with x:` | `close(x)` |
| `{k: v}` | `put` |
| `print(x)`, `"a" + x` | `operator +=(s: *&str, x: T)` |
| `a != b`, `a > b`... | `==` and `<` |
| `t: T = u` | `operator =(dest: T, u: U) -> T` |

## Generics and where
### Type parameters
`$T` gets its type only from the parameters, there is no way to give it by hand (`ident[i32](3)` doesn't exist). A `$T` that is only in the return type is an error when you call the function: `$T is not bound here, a generic gets its type from a parameter`.

`$T` also comes through a conversion to a fat pointer: a `&list[i32]` or a `*array[i32, 3]` going to `&slice[$T]` makes `$T` an `i32`, so `sort(ptr(values))` sorts an array.

`$$n` is a number, it comes from an array size (`array[$T, $$n]`) or from a type with a number parameter, and inside the function it's an integer literal.

Every different set of types makes a new instance of the function, and the same set reuses it. A generic type works the same way (`type Pair[$A, $B]`). A generic that makes bigger types with every call (like `f(x: $T)` calling `f(Pair[$T, $T]{...})`) stops at 100000 instances: `instancing f never ends, each call builds a bigger type than the last`.

### where
A `where` condition can be
- `$T is i8|i16`, true when the type of `$T` is inside that union
- `$T == i32` and `$T != i32`, with the type parameter on the left
- `can_assign(&strview, $T)`, true when a `$T` can be assigned to a `&strview`
- `@linux()`, `@macos()`, `@windows()`, `@x86_64()`, `@arm64()`, `@riscv64()`, for the system the program is built for
- `not`, `and`, `or` and parentheses
- anything else that the compiler can compute to `true` or `false` with the types and numbers of the call: `sizeof($T) >= 8`, `$$n <= 16`

The values of the parameters are not known when the call is chosen, so `where x > 0` is an error.

The platform builtins and `can_assign` only work inside a `where`.

`@compile_error("text")` in a function stops the compilation with that text, pointing at the call that made that instance of the function. A function that nothing calls doesn't fail, so it's the way to say that an overload chosen by `where` can't be used.

## Operators
You can overload `+ - * / % == < << >> and or xor` for your types. `!=`, `>`, `<=` and `>=` come from `==` and `<`, and declaring them is an error that tells you which one to declare
```
error: operator >= follows from <, declare operator < instead
```
`a += b` is `a = a + b` for everything except `&str`, where `+=` appends. That's why the way to print your type is `operator +=(s: *&str, x: T)`.

`==` doesn't exist for structs until you declare it, the compiler doesn't compare them field by field. Text compares by content, and `sort` uses `<`.

`operator =(dest: T, src: U) -> T` returns the `T` made from an `U`, and then assignments, declarations and `T(x)` convert for you. Calls don't use it, the argument must already be a `T`.

There is no pointer arithmetic, `p as usize + 8` and back with `as` is the way.

## Iteration
### Ranges
`a..b` goes from `a` to `b - 1`, and `reverse a..b` from `b - 1` down to `a`. If `b <= a` it doesn't run. The variable has the type of the end that is not a literal, or `i32` if both are literals, and both ends must have the same type. The ends are computed once, before the loop.

A range with an index (`for i, x in 0..3`) is an error, and a range only goes in a `for` or in a `case`.

### Everything else
`for x in value` calls `iter(value)` once, keeps what it gives, and calls `next` with a pointer to it every round. `next` returns the element or `IterationEnd`, a builtin void type that ends the loop, so the elements can be `null`. The element can be a union, and `for i, x in` adds a counter that starts at 0, also with `reverse`.

- Text gives each UTF-8 character as a `&strview` (`é` has `len` 2). `items(text)` gives the bytes
- A map gives its entries (`.key`, `.value`) in the order of its table, and it cannot be reversed. A fixed map gives them in the order they were put
- An array gives its elements, the loop takes its address so it doesn't copy it, and it can be reversed
- A pointer to a list is not iterable, use `p.*`
- The elements are copies, changing `x` doesn't change the list
- The length is read when the loop starts, what you append during the loop is not visited, and if the list grows the loop keeps reading the old memory, so don't grow a list you are walking

## Control flow
### switch
Each case is compared with `==`, and a case can be any expression, a range (`>=` the start and `<` the end) or several of them separated by `,`. Only the first case that matches runs, there is no fallthrough. `case:` is the default and goes last.

A switch over a union must handle every member or have a default: `this switch never handles i32 and has no default case`. Only void types can be cases, so a union with data needs a default.

`break` inside a switch leaves the loop around the switch.

### break and continue
They work on the innermost loop, there are no labels, and with a number they reach out: `break 2` leaves this loop and the one around it, `continue 2` goes to the next round of the one around it. Outside a loop: `break goes inside a loop`, and a number bigger than the loops open: `break 3 reaches past the loops open here, it can count up to 2`.

### defer
- The deferred statements run when their block ends, in reverse order, also on `return`, `break` and `continue`. In a loop they run every round
- The statement is run at the end, so it reads the values the variables have then
- It only sees the variables declared before the `defer`
- It cannot have `return`, `break`, `continue` or `try` inside, not even in a loop inside it
- If the deferred call returns an error, the error is lost

### with
`with value:` keeps the value and calls `close(value)` when the block ends, also when you leave it early. What `close` returns is lost. `with scratch():` is the same thing with the scratch memory (see [Memory](#memory)).

### Returns
- A function that returns a value must return on every path: `not every path of this function returns a value`. A `while true` without `break` counts as never ending
- Code after `return`, `break` or `continue` in the same block is an error: `this code can never run, the block already left`
- `main` has no parameters, is outside every module, and returns nothing (exit code 0) or an integer, the exit code
- `_ = value` throws the value away. `_ := 3` is a real variable named `_`

## Errors
An expression statement that leaves an error without handling it doesn't compile
```
error: this statement left errors unconsumed, handle them with try, is or _ =
```
If you keep the result in a variable, you can't use its value until you check it with `is`, but if you never use the variable the error is lost.

`try` gives the value and returns the errors from the function, so each error must be in the return type of the function
- `try passes Bad up but this function returns i32`
- `null|i32 never fails, there is no error to try for`, for a union without errors

`trust` removes the errors without checking, or the `null` if there are no errors. If it was an error you get garbage, and printing it can crash the program.

`a iferror b` is the value of `a` without its errors, or `b` when `a` has an error, and `a ifnull b` is the same with `null`. `b` only runs when it's needed. If `b` fits the type left from `a` the result has that type, and if not it's the union of both. They bind tighter than comparisons and looser than arithmetic, so `get(m, k) ifnull 0 > 3` compares the result and `m[k] ifnull 0 + 1` adds to the default, put parentheses when you mean the other one.

### Runtime errors
These stop the program, and write the message and then the place (`  --> main.flg:12:11`) to the error output
| Message | Exit code |
|---|---|
| `flag: index 5 is out of bounds, length 3` | 134 |
| `flag: division by zero` | 134 |
| `flag: assertion failed`, or the message of `assert(condition, message)` | 134 |
| `flag: <text>` (from `panic`) | 134 |
| `flag: out of memory` | 9 |

A negative index counts from the end, `l[-1]` is the last element, and it's an error when it goes before the first one. A slice `s[a:b]` needs `a <= b <= len` after that.

The compiler leaves out the check of `a[i]` on an array when it cannot fail: a literal index inside the array, or the variable of a `for` over a range of literals that fits in the array and that the body never assigns or passes to a function.

## Constants and contexts
### Constants
A constant goes at the root of a file. Arithmetic between literals is done by the compiler and the result is still a literal, so it takes the type of where you use it
```flag
N = 3
BIG = N * 1000000000 # an i64 literal

func main():
    small: u8 = N
    print(small, " ", BIG)
```
Any other expression is written again where you use the constant, like a macro, so `NOW = now()` calls `now()` every time. A constant can be the size of an array if it's an integer literal. A constant that uses itself is an error.

### Contexts
A context is one global value with its fields at zero. It has only fields, no values, and a nested block makes a type like `app.inner`. It's also a type and a value, so you can print it or take `ptr(app.depth)`. Who can see it follows the module rules, like a type.

## Modules
- `module name` goes in the first line and `use` goes before the first declaration
- `use a` gives the names of `a` and `a.name`, `use a as m` gives only `m.name`
- `priv` hides a name from the files outside the module
- a name declared in two modules you use is an error when you use it without the module: `Thing is declared in both a and b, name the module: b.Thing`
- a function of your module wins over a global one, and a global one over one of another module
- `main` must be global, a `main` inside a module is not the start of the program

The library modules are `fs`, `os`, `json`, `net`, `math`, `utf8`, `bits`, `ring` and `raylib` (see the [Library](Library.md)).

## Memory
Flag doesn't have a garbage collector, you free what you allocate.

### Who owns what
- `&str`, `&list` and `&map` own their memory, you free them with `free`
- `&strview` and `&slice` are views, they don't own anything
- `heap(value)` copies a value to the heap and gives you a pointer, you free it with `free(p as *bytes)`
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

## The memory checker
`-memory` reads the IL of your program and follows every allocation, and warns about
- memory that is never freed, or that may never be freed: `the memory allocated here for names is never freed`
- memory freed twice: `free cannot release names, it may be freed already`
- memory used after it was freed, also after a list grew and moved: `names points to memory that may be freed already`
- freeing what is not on the heap, or a pointer to the middle of a block
- temporaries used after their `with scratch():` ended
- a pointer to a local variable that leaves its function: `a pointer to x cannot leave make, x lives on its stack`
- a function that frees one argument and reads another that may be the same memory, like `t += t`

A warning has a note that points to the other place involved, where it was freed or allocated.

### What it doesn't know
It doesn't follow the values of the conditions, it looks at every path. So these give warnings that are not real
- freeing in `if c:` and again in `if not c:` warns that it may be freed twice and may never be freed
- the elements of a list freed in a loop, `for s in names: free(s)`, because it sees all the elements of a list as one
- a value that goes into a map and is removed and freed later

It cannot follow a pointer once you store it as an integer, and it tells you: `-memory cannot follow x once it is stored as an integer, store it with a pointer type`. And if a function is too complex it gives up on it: `-memory gives up on f, its memory use does not settle, so its checks are incomplete`.

### @trust and @check
`@trust` hides the warnings of code you already checked
- at the end of a line it covers that line and the lines inside it
- on a line of its own it covers the lines after it at that indentation, until the indentation goes back, and at the top of a file it covers all the file

`@check` does the opposite inside a trusted part. A function under `@check` also keeps showing its warnings when trusted code calls it.

The checker skips the trusted functions that no untrusted code needs, so trusting code also makes `-memory` faster. The compiler itself is all trusted.

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
`export func` is a function that C can call: it keeps its plain name as the symbol, uses the C calling convention and is built even if nothing in Flag calls it. It cannot be generic, overloaded or `main`. `ptr(name)` of an export function is its address as a `*bytes`, for C functions that take a callback
```flag
extern "c":
    func qsort(base: *bytes, count: usize, size: usize, compare: *bytes)

export func ascending(a: *i32, b: *i32) -> i32:
    return a.* - b.*
```
`ptr` of a function that is not exported is an error, a callback needs the one fixed signature an export has.

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

Tagged unions and fat pointers can't cross to C, pass their parts. Be careful with `long`, it's 64 bits on Linux and macOS and 32 bits on Windows.

### Variadic functions
A C function can end with `...`. The extra arguments must be integers, floats, `bool` or pointers, `f32` goes as a `double` like C does, and text is not converted there, use `cstr`.

### Text
`cstr` is `*u8`, a text that ends with a zero byte
- a text literal goes as a `cstr` without copying, literals already end with a zero
- a `&str` goes as a `cstr` without copying, the compiler writes the zero after its text
- any other `&strview` needs `cstr(text)`, that makes a temporary copy
- `view(c)` turns a `cstr` back into a `&strview`

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
- `run(command)` and `output(command)` go through `cmd.exe`, and `run(list)` passes each argument quoted so the program gets it exactly
- `net` uses Winsock, and the first time a program listens the Windows firewall may ask you
- The `raylib` module only has the Linux build of raylib, for Windows and macOS put your own `libraylib.a` in the `lib` folder
- The console shows text with its code page, so accents may look wrong unless the console is in UTF-8 (`chcp 65001`)

## Editor support
These are for editors and tools, they don't build the program.

`-check` checks the code and prints the errors, it's the fast way to see if your code is right.

`-overlay file` replaces the text of some files with the text in that file, so an editor can check what you are typing before you save. The file is a list of entries, and each one is the path in a line, the length in bytes in another line, and that many bytes of text.

`-at file:line:column` says what is at that place, it prints a `definition` line with where it's declared and `hover` lines with its type or its declaration
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
error: no overload of iter matches (array[i32,3])
  --> main.flg:3:9
   |     for v in values:
   |         ^
```
Only the first error is shown, and a note follows it when there is another place involved. The messages always say what is wrong and why, if one doesn't, it's a bug in the compiler.

## Building the compiler
`./build.sh` builds the compiler in stages
1. It builds QBE from `vendor/qbe` and packs the library into an object file
2. Stage 0 is built from the seed, `boot/<system>.ssa`, the IL of the compiler for that system
3. Stage 0 compiles the sources into stage 1, stage 1 into stage 2 and stage 2 into stage 3
4. Stage 2 and stage 3 must write the same IL, that's the proof that the compiler compiles itself right

`./build.sh seed` writes the three seeds, one for each system, with `-target`. When the seed of your system is older than the sources the build tells you: `boot/linux.ssa is older than src: ./build.sh seed refreshes it`.

`./build.sh package` makes `build/flag-<version>-<system>-<machine>.tar.gz` with the compiler.

`./test.sh build/flagc` runs every test in `tests/`
- `name.flg` with `name.out` must print that
- `name.flg` with `name.err` must fail with that text in the error
- `name.flg` with `name.warn` is compiled with `-memory` and must warn with that text, or not warn if the file is empty
- a folder is a test with all its files
- and every example in `examples/` must compile

QBE is in `vendor/qbe` with two changes: its `main` returns instead of calling `exit`, so flagc can call it, and on Windows it probes the stack for functions with more than 4 KiB of locals.

## Rough edges
Things that work in a way that may surprise you, so be aware
- `m[k] += 1` on a map doesn't compile, `m[k]` is a value that can be `null`, write `m[k] = (m[k] ifnull 0) + 1`
- `m[k] = v` on a fixed map gives the `MapFull?` of `put`, so write `trust put(m, k, v)` when you know there is room, or `try put(m, k, v)`
- An error kept in a variable that you never use is lost, and so are the errors of a deferred call and of `close`
- `trust` on a real error gives garbage, and it can crash
- The memory checker has the false positives of [What it doesn't know](#what-it-doesnt-know)
- `a = b` between two `&str` doesn't copy the text, both share it, only a `&strview` is copied. Write `a = view(b)` to copy
