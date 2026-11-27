# Flag Overview

This is a tour of the Flag programming language. It assumes basic programming experience and walks through the core features with examples. For the exact language rules, see the [Reference](Reference.md). For every standard library function, see the [Library](Library.md).

## Contents

- [Hello world](#hello-world)
- [Comments](#comments)
- [Variables and assignments](#variables-and-assignments)
- [Literals](#literals)
- [Constants](#constants)
- [Operators](#operators)
- [Types](#types)
- [Pointers](#pointers)
- [Fat pointers](#fat-pointers)
- [Control flow](#control-flow)
- [Functions](#functions)
- [Strings](#strings)
- [Arrays](#arrays)
- [SIMD](#simd)
- [Lists](#lists)
- [Maps](#maps)
- [Arenas](#arenas)
- [Contexts](#contexts)
- [Threads](#threads)
- [Error handling](#error-handling)
- [Modules](#modules)
- [The standard library](#the-standard-library)
- [Calling C](#calling-c)
- [Memory safety](#memory-safety)
- [Compiling](#compiling)

## Hello world

Flag uses Python-style indentation for blocks:

```flag
func main():
    print("hello world")
```

`main` can also return an `i32`, which becomes the program's exit code:

```flag
func main() -> i32:
    print("hello world")
    return 0
```

Save the file as `hello.flg` and run it directly with `flagc run`, which compiles and executes the program:

```sh
flagc run hello.flg
```

Or compile it to keep the executable:

```sh
flagc hello.flg -o hello
./hello
```

## Comments

Comments start with `#` and run to the end of the line. There are no multiline comments.

```flag
# this is a comment
a := 3 # this too
```

## Variables and assignments

Variables are declared inside functions and live in the block where they are declared (an `if`, a loop, or the function itself). At the top level of a file, a name is a [constant](#constants).

Variables declared without an initial value start at zero:

```flag
a: i32 # declares 'a' as i32, initialized to 0
b: u32 = 3 # declares 'b' as u32 and sets it to 3
if true:
    a: u32 # error: variable a is already declared in this scope
```

You cannot shadow a name that is already visible, not even inside an inner block. This applies to local variables, parameters, `for` loop variables, and any constants or contexts visible to the function, whether your own, `core`'s (such as `stdout`), or those from a module imported with `use` without an alias:

```flag
N = 3

func main():
    N := 5 # error: cannot declare variable N because it shadows an existing constant
```

Functions and types can share names with variables: declaring `twice := 4` still leaves `twice(3)` calling the function.

Use `:=` to infer the type from the initial value:

```flag
a := 3 # 'a' has type i64
b := 2.5 # 'b' has type f64
```

Assignments perform a small set of implicit conversions:

```flag
view: &strview = "hello" # a text literal has type &strview
text: &str = heap(view) # heap copies the text into a new &str that you own
back: &strview = text # a &str converts to &strview without copying
text = "bye" # copies into the memory text already has
small: u8 = 7
n: i64 = small # smaller integers widen to larger ones
maybe: i64|null = a # a value enters a union that includes its type
l: &list[i64] = heap([1, 2]) # allocates a heap list with a copy of the array
```

A new owning container (such as `text: &str`) must get its memory from `heap` or an [arena](#arenas). Writing `text: &str = view` is a compile-time error:

```
error: new &str has no allocated memory to copy &strview into; wrap the value in heap(...) or arena(a, ...)
```

All other conversions are explicit and written like a call to the target type:

```flag
c := f64(a) / 2.0
d := u8(a)
```

## Literals

- **Integers**: `3`, `-3`, `0xff`, `0b101`. Underscores can separate digits (`1_000_000`). Integer literals adapt to the type expected by their context; on their own they default to `i64` (or `u64` above the maximum `i64`), so `n := 5` gives an `i64`; write `n: i32 = 5` for an `i32`. When either an integer or a float could accept an integer literal, it chooses the integer.
- **Floats**: `2.5`, `1e3`. Standalone float literals default to `f64`. `inf` and `nan` are written just as `print` outputs them (for example, `low := -inf`). A compile-time computation between finite literals that produces `inf` or `nan` (such as `1.0 / 0.0`) is an error.
- **Characters**: `'a'` is the numeric code of that character (`97`). It is an integer literal, so it works directly with `u8`.
- **Text**: `"hello\n"` has type `&strview`. Supported escapes are `\n`, `\t`, `\r`, `\0`, `\\`, `\"`, `\'`, `\e`, and `\x41` for a byte by two hexadecimal digits. Text literals may span multiple lines, preserving line breaks and leading spaces as written.
- **Booleans and null**: `true`, `false`, and `null`.
- **Arrays**: `[1, 2]` has type `array[i64, 2]`. If elements have different types, the array holds their union: `[1, "hola"]` is `array[i64|&strview, 2]`. `[]` is an `emptyarray`, which becomes either a zeroed array or an empty listview depending on the expected type; `x := []` alone has no type.
- **Maps**: `{"ana": 30, "bo": 25}` is a `fixmap[&strview, i64, 2]` holding only those entries, and `{}` is an `emptyfixmap` (see [Maps](#maps)).
- **Structs and tuples**: `{1, 2}` or `{x = 1}` constructs the struct expected by its context (see [Structured types](#structured-types)). Where no struct is expected, `{1, "a"}` is a tuple of type `(i64, &strview)` (see [Returning multiple values](#returning-multiple-values)).

No literal allocates memory. To create a growable list, string, or map from a literal, wrap it in `heap` or `arena`: `heap([1, 2])`, `heap("hello")`, or `heap({"ana": 30})`.

When you provide a target type, the literal adopts it:

```flag
a: array[u8, 3] = [1, 2, 3]
ages: fixmap[&strview, u8, 4] = {"ana": 30}
```

`ptr` forwards the target type to what it points to, so an array literal or constant can become a listview of another integer type:

```flag
SHADER = [0x0723_0203, 0x0001_0000, 0x0008_000b]

func main():
    words: &listview[u32] = ptr(SHADER)
```

A multiline text literal keeps its line breaks and indentation intact:

```flag
USAGE = "usage: flagc file.flg
  -check   only check"
```

## Constants

A constant is a name for a value, declared at the top level of a file outside any function:

```flag
MYCONST = "hello"
NUM = 2
MYSECONDCONST = 2 + 2 + NUM # computed at compile time as 6
```

Arithmetic on literals is folded at compile time. Any other untyped constant expression is expanded again at each use, like a macro. A constant array or struct of literals (such as `TABLE = [3, 1, 4, 1, 5]`) is stored once in program data and read from there on every use (see [Constants](Reference.md#constants)).

A constant can also have an explicit type, written like a variable declaration. Its value must be known at compile time: literals, constants, and structs or arrays built from them:

```flag
LIMIT: u8 = 10
names: array[&strview, 3] = ["one", "two", "three"]
```

Constants cannot change; mutable global state belongs in a [context](#contexts). Nothing executes before `main`, so everything declared outside functions is resolved at compile time.

## Operators

- **Arithmetic**: `+`, `-`, `*`, `/`, and `%` (for integers). `+` and `-` also move a `*bytes` pointer (see [Pointers](#pointers)).
- **Comparisons**: `==`, `!=`, `<`, `>`, `<=`, `>=`. Comparisons do not chain: `a < b < c` is an error that tells you to write `a < b and b < c`.
- **Logic**: `and`, `or`, and `not` on `bool`. `and` and `or` short-circuit and skip the right-hand side when the left-hand side already decides the result.
- **Bitwise**: `and`, `or`, `xor`, and `not` on integers operate bit by bit, and `<<` and `>>` shift bits. Because `and`, `or`, and `xor` have lower precedence than comparisons, wrap bit tests in parentheses: `(flags and MASK) != 0`. Shifts bind tighter than comparisons.
- **Conditional**: `a if c else b` evaluates to `a` when `c` is true and `b` otherwise; only the chosen branch runs. Both branches take the type the context expects, so `1 if x else -1` works as an `i32` variable, field, return value, or argument.
- **Assignments**: `=`, `+=`, `-=`, `*=`, `/=`, `%=`.

```flag
flags := 0b1010 and 0b0110 # 0b0010
even := 0b1011 and not 1   # 0b1010
big := 1 << 20
if x > 0 and not done:
    count += 1
mode := "-check" if check else "run"
```

### Evaluation order and numeric rules

- **Left-to-right evaluation**: expressions run left to right and each variable is read in its turn. Suppose `x` is `1` and `bump` adds `10` to `x` and returns the new value (`11`): `x + bump(ptr(x))` evaluates to `1 + 11` (`12`), even though `bump` modifies `x`. An assignment evaluates its right-hand side first, so `x += bump(ptr(x))` adds `11` to the updated `x`, producing `22`.
- **Mixing integer types**: two different integer types can be combined when one holds every possible value of the other, and the operation runs in the wider type (so `u8 + i32` produces `i32`). Otherwise (for example `u64` with `i64`, or an integer with a float), convert one side explicitly (see [Variables and assignments](#variables-and-assignments)). A literal adapts to the type of the other side.
- **Integer comparisons**: comparisons accept any two integer types and compare their mathematical values, so `i64(-1) < u64(0)` is `true`.
- **Shifts, overflow, and division**: a shift keeps the type of its left-hand operand. Integer overflow wraps around, and integer division by zero stops the program with a runtime error.
- **`not` precedence**: `not` binds everything up to the next `and`, `or`, or `xor`, so `not x == 3` means `not (x == 3)`. Before arithmetic it requires parentheses, `(not x) + 1`, because `not x + 1` would be ambiguous.

## Types

### Built-in types

- `bool`
- Signed integers: `i8`, `i16`, `i32`, `i64`
- Unsigned integers: `u8`, `u16`, `u32`, `u64`
- Floating-point numbers: `f32`, `f64`
- `bytes`: raw memory of unknown size, used only behind a pointer (`*bytes`)

Lengths, counts, and positions are always `i64`; Flag has no `usize`.

Calls starting with `@` are answered by the compiler at compile time; for example, `@sizeof(T)` returns the size of `T` in bytes, and `@here()` returns the current source location as text.

Flag lets you declare three kinds of user types (void, structured, and union/enum), plus type aliases.

### Void types

A void type carries no data. Every void type can be used directly as a value:

```flag
type Red

func main():
    color: Red = Red
```

### Structured types

Structured types define a memory layout of named fields:

```flag
type User:
    name: &str
    age: u32
```

Construct a struct either positionally (all fields in order) or by field name. Omitted fields start at zero or at their default value:

```flag
a := User{heap("ana"), 30} # a &str field owns its text, so heap allocates it
b := User{age = 30, name = heap("bo")}
```

Fat pointers have no valid zero value (see [Fat pointers](#fat-pointers)), so omitting `name` in `User{age = 30}` is a compile-time error:

```
error: missing required field name in User (&str has no default zero value); set it in the literal or add a field default
```

Field defaults are written whenever a value of the struct is created. A default must be known at compile time: literals, constants, or an empty owner (`heap([])`, `heap("")`, or `heap({})`):

```flag
type Window:
    title: &strview = "untitled"
    width: i32 = 640
    height: i32 = 480
    tabs: &list[&strview] = heap([])

func main():
    w: Window # "untitled", 640 by 480
    big := Window{width = 1920, height = 1080} # also "untitled"
```

When the surrounding context already specifies the type (a typed variable, a `return`, a struct field, or a typed array element), you can omit the struct's name:

```flag
c: User = {heap("bo"), 25}
d: User = {age = 25, name = heap("cy")}
```

Structured types can be nested inline:

```flag
type Car:
    model: &strview
    motor: # defines the type Car.motor
        hp: u32

func main():
    motor: Car.motor = Car.motor{hp = 12}
    car: Car = Car{"mini", motor}
    other: Car = {"fiat", {hp = 90}} # the inner {} is a Car.motor
    car.motor.hp += 10
```

A field can overlap the memory of an earlier field using `@`:

```flag
type Record:
    code: u32
    id: u32 @code # 'id' shares the same bytes as 'code'
```

`packed type` places each field immediately after the previous one with no padding, which is useful for binary formats and hardware registers:

```flag
packed type Header:
    kind: u8
    size: u64 # starts at byte 1; the struct is 9 bytes instead of 16
```

Structured types are values: assigning them or passing them to a function copies them. `==` compares structs field by field unless you define your own (see [Operator overloading](#operator-overloading)).

### Aliases

An alias gives another name to an existing type; the two names are completely interchangeable:

```flag
type Id = u32

func main():
    user: Id = 7
    n: u32 = user
```

### Union types

Union types are tagged unions whose members are separated by `|`. Give a union a name with an alias:

```flag
type Num = i32|f32
```

`T?` is shorthand for `T|null`:

```flag
found: i64? = null
```

Check which member a union currently holds with `is` (which narrows the type inside an `if`; see [If](#if)) or with `switch` (see [Switch](#switch)):

```flag
n: Num = 3
if n is i32:
    print("an integer ", n)
```

Flag also supports untagged unions written with `||`, though they are not recommended: they store no tag to check, and you read a member by converting to it (`f32(u)`).

#### Enums

An enum is a union of void types. There are two equivalent ways to declare one:

1. **Union syntax** (defines `Red`, `Green`, and `Blue` as void types, or reuses them if they are already defined as void types):

```flag
type enum Color = Red|Green|Blue
```

2. **Block syntax** (equivalent to the form above):

```flag
type enum Color:
    Red, Green
    Blue
```

### Generic types

Types can take type parameters prefixed with `$`:

```flag
type Pair[$T]:
    a: $T
    b: $T

func main():
    p: Pair[i32]
    p.a = 1
```

They can also take compile-time number parameters prefixed with `$$`:

```flag
type Ring[$T, $$n]:
    items: array[$T, $$n]
    start: i64

func main():
    r: Ring[u8, 16]
```

### Reinterpreting with `as`

`as` reinterprets the raw bytes of a value as another type of the same size without converting. Numbers, pointers, structs, and arrays can all be reinterpreted as one another:

```flag
type Size:
    w: u32
    h: u32

func main():
    raw := f32(1.5) as u32 # the raw bits of the float
    key := Size{640, 480} as u64 # the 8 bytes of a Size as one integer
    back := key as Size # and back to a Size
    one := 0x3f800000 as f32 # an integer literal adopts the target size, yielding 1.0
```

`as` can also extract a member from a union without checking its tag. If the union actually holds a different member, those bytes are read as the requested type anyway, so check with `is` first.

## Pointers

A pointer type is written `*T`. `ptr(x)` takes the address of `x`, and `p.*` reads or writes the value `p` points to:

```flag
x := 5
p := ptr(x) # 'p' has type *i64
p.* = 6 # x is now 6
```

Struct fields are accessed directly through a pointer without writing `.*`:

```flag
type Counter:
    n: i32

func bump(c: *Counter):
    c.n += 1

func main():
    c := Counter{0}
    bump(ptr(c))
    bump(c) # equivalent: a *Counter parameter automatically takes the address of a Counter
    h := heap(Counter{10}) # heap copies a value to the heap and returns *Counter
    h.n += 1
    free(h)
```

When a function expects `*T` and you pass a `T` (with no overload taking `T`), the compiler passes its address automatically:
- Pass a variable when the function mutates its argument; passing any other expression mutates a temporary.
- A temporary lives like a variable declared on that line until its enclosing block ends, so `l: &listview[i32] = ptr([1, 2, 3])` remains valid for the rest of that block.
- Inside a loop, the next iteration overwrites the temporary, so copy anything you need to keep into memory of its own. `-memory` warns if a pointer to a temporary is used after its block ends.

`null` is not a pointer; a pointer that may be absent has type `*T?`.

`*bytes` points to raw memory (like C's `void *`) and is the only pointer that supports pointer arithmetic: `p + 8` is the address 8 bytes after `p`, and `q - p` is the distance in bytes. Convert other pointers through `as *bytes` first:

```flag
words: array[i64, 3] = [10, 20, 30]
second := ptr(words) as *bytes + 8
print((second as *i64).*) # 20
```

## Fat pointers

A fat pointer is a pointer paired with extra metadata, written with `&` before the type. For example, `&list[i32]` points to its elements and carries `len: i64`, `cap: i64`, and `arena` (identifying where its memory lives):

```flag
a: &list[i32]
```

`core` provides five fat pointers:

| Type | Kind | Description |
|---|---|---|
| `&strview` | View | Read-only view of text |
| `&str` | Owner | Growable text that you own |
| `&listview[T]` | View | View of elements |
| `&list[T]` | Owner | Growable list that you own |
| `&map[K, V]` | Owner | Hash map that you own |

A fat pointer must receive memory before you use it: owners get theirs from `heap` or `arena`, and views get theirs from the data they reference. `heap([])`, `heap("")`, and `heap({})` create empty heap owners that do not allocate until they grow:

```flag
a: &list[i32] = heap([])
append(a, 1)
```

- Using a fat pointer that no path initialized is a compile-time error:
  `a is used before being initialized (&list[i32] has no default zero value); initialize it: a: &list[i32] = heap([])`
- Growing an owner that might lack memory (because only some paths initialized it) halts at runtime:
  `flag: this &list has no memory to grow in: give it heap([]) or arena(a, [])`
- `-memory` catches that ahead of time with a warning:
  `append cannot grow a; it may not have allocated memory to grow (initialize with heap([]))`

Ownership and view conversions follow three rules:
- **Releasing memory**: owned fat pointers (`&str`, `&list`, `&map`) must be released with `free` when you are done with them (`defer` helps), or live in an [arena](#arenas) that releases everything at once.
- **Owner to view**: a `&str` works wherever a `&strview` is expected, and a `&list` works wherever a `&listview` is expected, without copying.
- **View to owner**: going the other way copies: `heap(v)` or `arena(a, v)` creates a new owner with a copy of `v`, and assigning `t = v` copies into an owner that already has memory. Calls never copy implicitly, so a view cannot be passed to a `&str` or `&list` parameter.

You can define your own fat pointers with `type fatptr`, such as `&ring` in the `ring` module:

```flag
type fatptr ring[$T]:
    len: i64
    cap: i64
    first: i64
```

### Passing them to functions

Passing a fat pointer by value copies its pointer and metadata. The underlying elements are shared, but if the function grows the copy, the caller's original fat pointer will not see the change.

To let a function modify or grow the caller's fat pointer, declare the parameter as `*&T`. You still pass the fat pointer directly; the compiler takes `ptr(...)` automatically, just as with any pointer (see [Pointers](#pointers)). Indexing and `for` loops work directly through `*&T`, like fields:

```flag
func add_two(l: *&list[i32]):
    append(l, 2)
    l[0] += 1

func main():
    nums: &list[i32] = heap([])
    defer free(nums)
    append(nums, 1)
    add_two(nums) # equivalent to add_two(ptr(nums))
    print(nums) # [2, 2]
```

This is how `append`, `free`, `put`, and `+=` work.

## Control flow

### If

Conditional execution uses `if`, optionally followed by `elif` and `else`:

```flag
if condition:
    print("is true")
```

```flag
if cond1:
    print("1 is true")
elif cond2:
    print("2 is true")
else:
    print("all are false")
```

`if` also narrows union types using `is`:

```flag
color: Color = Red
if color is Red:
    pass # color has type Red here
elif color is Blue:
    pass # color has type Blue here
else:
    # color has type Green here
    color = get_random_color() # color widens back to Red|Green|Blue from here
```

Check multiple union members at once by writing a union after `is`:

```flag
if color is Red|Blue:
    pass # color has type Red|Blue here
```

- `==` and `!=` against `null` or any void type narrow just like `is`, so `if x != null:` works too.
- `not` flips narrowing: `if not color is Red:` narrows `color` to `Green|Blue`.
- A branch that exits (via `return`, `break`, `continue`, or a non-returning call like `panic`) removes its types from the variable after the `if`:

```flag
data := read_file("config.txt")
if data is FsError:
    panic("cannot read the config: " + data)
print(data.len) # data is a &str here
```

`assert` narrows the same way, and stops the program showing the evaluated values if its condition is false (see [When the program fails](#when-the-program-fails)):

```flag
data := read_file("config.txt")
assert data is &str
print(data.len)
```

Blocks cannot be empty; use `pass` as a no-op statement:

```flag
if color is Red:
    pass # nothing to do yet
else:
    print("not red")
```

### While

```flag
while condition:
    print("again")
```

### For

`for` iterates over any iterable, optionally with an index:

```flag
for element in iterable:
    print(element)

for index, element in iterable:
    print(index, " ", element)
```

Ranges use `..` (exclusive end) or `..=` (inclusive end). The loop counter `i`, like `index` above, is read-only:

```flag
for i in 0..10: # 0 through 9
    print(i)

for i in 1..=10: # 1 through 10
    print(i)
```

Iterating over text yields each UTF-8 character as a `&strview`, and iterating over a map yields each entry with `.key` and `.value`:

```flag
for c in "hello":
    print(c)
```

Use `reverse` to iterate backward. Whether it works depends on the iterable: ranges, arrays, lists, listviews, and text support it:

```flag
for element in reverse iterable:
    print(element)

for i in reverse 0..10: # 9 down to 0
    print(i)
```

With `for index, element in reverse list`, `index` is the element's actual index in the list, counting down (`2`, `1`, `0` for three elements). Only arrays, lists, and listviews know that index, so using an index with `reverse` over text or custom types is an error.

When iterating over an array, list, or listview, the loop variable refers directly to the element in place (not a copy), so modifying it or its fields modifies the container:

```flag
for p in players:
    p.score += 10 # adds 10 to every player in the list
for s in names:
    free(s) # frees each string and leaves it empty in the list
```

Where the loop variable cannot be mutated (such as a character view of text or a map entry copy), assigning to it is an error that explains what to write instead:

```
error: cannot assign to map loop variable e; it is a copy and would not update ages (assign directly via ages[e.key] = v)
```

Do not grow a list while iterating over it, because reallocation moves its elements.

To make your own type iterable, give it `operator for` and `operator next`. `for` calls `operator for` with your value, then calls `operator next` with a pointer to what it returned, until `operator next` returns `IterationEnd`. `reverse` calls `operator reverse` instead of `operator for`:

```flag
type Countdown:
    from: i32

type Down:
    at: i32

operator reverse(c: Countdown) -> Down:
    return Down{c.from}

operator next(d: *Down) -> i32|IterationEnd:
    if d.at == 0:
        return IterationEnd
    d.at -= 1
    return d.at + 1

func main():
    for n in reverse Countdown{3}:
        print(n) # 3, 2, 1
```

### Switch

`switch` tests a value against each `case` in order and runs only the first match (there is no fallthrough). A `case` can list multiple values and ranges, and a bare `case:` is the default:

```flag
switch n:
case 0:
    print("zero")
case 1..10, 100:
    print("small")
case 10..=99:
    print("two digits")
case:
    print("other")
```

`switch` also works on text. When switching on a union, the cases name its member types and narrow the variable inside each case just like `is`. A union `switch` must cover every member type or include a default `case:`, which narrows to the remaining types:

```flag
type enum Color = Red|Green|Blue
type Square
type Circle:
    r: f64
type Shape = Circle|Square|null

func show(color: Color, shape: Shape):
    switch color:
    case Red:
        print("red")
    case Green, Blue:
        print("not red")

    switch shape:
    case Circle:
        print("a circle of radius ", shape.r)
    case:
        print("not a circle") # shape has type Square|null here
```

### break and continue

`break` exits the innermost loop and `continue` jumps to its next iteration:

```flag
for i in 0..10:
    if i == 2:
        continue
    if i == 5:
        break
    print(i)
```

Pass a positive number to target outer loops: `break 2` exits two enclosing loops, and `continue 2` skips to the next iteration of the outer loop:

```flag
for row in 0..10:
    for column in 0..10:
        if row * column == 42:
            break 2
```

### Defer

`defer` schedules a statement to run when its enclosing block exits, including on `return`, `break`, or `continue`. Multiple `defer`s run in reverse order:

```flag
func main():
    names: &list[&strview] = heap([])
    defer free(names)
    append(names, "ana")
```

A `defer` cannot contain `return`, `break`, `continue`, or `try`, and cannot be a declaration or another `defer`. A deferred call that can return an error must explicitly handle it; for example, `defer _ = remove_file(path)` discards the error on purpose.

### With

`with` calls `operator with` on its value when the block ends, even if you leave the block early:

```flag
type Door:
    room: i32

operator with(d: Door):
    print("closed ", d.room)

func main():
    with Door{7}:
        print("inside the room")
```

An `operator with` that can return an error cannot be used with `with` because the error would be lost; write a regular closing function and call it yourself instead.

## Functions

```flag
func add(a: i32, b: i32) -> i32:
    return a + b
```

In a call, `(` must immediately follow the name with no space (`add(1, 2)`), whereas keywords must have a space before `(` (`not (a and b)`, `if (x) or (y)`). Writing `add (1, 2)` or `not(x)` produces an error showing the valid form.

### Parameters

Parameters are passed by value (copied). To let a function modify a caller's value, pass a pointer (see [Pointers](#pointers) and [Passing them to functions](#passing-them-to-functions)).

#### Named arguments

Arguments can be passed by name in any order. Once you use a named argument, all following arguments must be named too:

```flag
area(height = 3, width = 5)
area(2, height = 10)
```

#### Default values

```flag
func label(text: &strview, size: i32 = 20, bold: bool = false):
    print(text, " ", size, " ", bold)

func main():
    label("a")
    label("b", 10)
    label("c", bold = true)
```

Default expressions are evaluated at the call site. Because of this, `@here()` as a default captures the file, line, and column of the call, which is how `panic` and bounds errors report where they occurred:

```flag
func log(message: &strview, place: &strview = @here()):
    print(place, ": ", message)
```

A default can also refer to earlier parameters, letting one function handle both whole values and slices:

```flag
func slice_of(text: &strview, from: i64 = 0, to: i64 = text.len) -> &strview:
    return text[from:to]
```

#### Extra arguments

The last parameter can collect any number of trailing arguments of one type using `..`, which appears inside the function as a `&listview`. Spread an existing list or listview into it with `..`:

```flag
func sum(..items: i64) -> i64:
    total: i64 = 0
    for x in items:
        total += x
    return total

func main():
    values: &list[i64] = heap([10, 20])
    defer free(values)
    print(sum(1, 2, 3), " ", sum(), " ", sum(..values)) # 6 0 30
```

C variadic functions use `...` instead (see [Calling C](#calling-c)).

### Returning multiple values

A function returns multiple values as a tuple: an anonymous struct written with its element types in parentheses, such as `(i32, i32)`. `return a, b` constructs it, and declarations or assignments unpack it:

```flag
func split(text: &strview) -> (i64, i64):
    return 0, text.len

func main():
    lo, hi := split("hola") # 0 and 4
    whole := split("abc")
    print(whole, " ", whole.1) # (0, 3) 3
    a, b := 1, 2
    a, b = b, a # swaps a and b
    _, last := split("xy")
```

Errors cannot go inside a tuple; place them alongside it in a union (`-> (i64, i64)|ParseError`) and unpack after `try`: `lo, hi := try parse(text)`.

### Functions that never return

A function that never returns, such as `panic` or `exit`, declares `-> never`. Code after a call to it is unreachable, so it narrows unions like `return`, and on the right-hand side of `ifnull` or `iferror` it leaves the unwrapped value:

```flag
func fail(why: &strview) -> never:
    eprint("error: ", why)
    exit(1)

func main():
    port := parse_int("80") ifnull fail("not a number") # port has type i64
```

### Overloading

Functions can share a name with different parameter types. A call picks the overload that matches every argument at least as well as every other overload and at least one argument strictly better. Integers widen as in assignments, and exact type matches win:

```flag
func describe(n: i32) -> &strview:
    return "an integer"

func describe(t: &strview) -> &strview:
    return "a text"

func twice(n: i64) -> i64:
    return 2 * n

func main():
    small: i16 = 4
    print(describe(small), " ", twice(small)) # an integer 8
```

### Operator overloading

You can overload binary `+ - * / % == < << >> and or xor` (two parameters) and unary `not` (one parameter) for your types. `!=`, `>`, `<=`, and `>=` are derived automatically from `==` and `<`, and `a += b` uses `+`:

```flag
type V:
    x: i32
    y: i32

operator +(a: V, b: V) -> V:
    return V{a.x + b.x, a.y + b.y}

operator ==(a: V, b: V) -> bool:
    return a.x == b.x and a.y == b.y
```

`operator =` defines a conversion to your type, which assignments and `T(x)` use automatically:

```flag
type Celsius:
    degrees: f64

operator =(c: Celsius, fahrenheit: f64) -> Celsius:
    return Celsius{(fahrenheit - 32.0) * 5.0 / 9.0}

func main():
    boiling: Celsius = 212.0 # 100.0 degrees
```

`T(x)` converts the way an assignment does, which includes converting to numbers: with `operator =(x: f64, c: Celsius) -> f64`, `f64(boiling)` evaluates to `100.0`.

### Indexing

`x[i]` calls `operator [](x, i)` with the index exactly as written (including negative indexes). When it returns a pointer, you can also assign to `x[i]`:

```flag
type Grid:
    cells: array[i32, 9]

operator [](g: *Grid, i: i32) -> *i32:
    return ptr(g.cells[i])

func main():
    g: Grid
    g[4] = 7
```

Other syntax features call protocol operators too, such as `operator for` for `for` loops and `operator with` for `with` blocks, all documented in [Protocol operators](Reference.md#protocol-operators). Operators cannot be called by name, so types that you also close manually provide a regular function alongside them (such as `close(f)` on `File`).

### Generics

Use `$T` for type parameters and `$$n` for compile-time number parameters:

```flag
func biggest(a: $T, b: $T) -> $T:
    if a > b:
        return a
    return b

func size(a: array[$T, $$n]) -> i64:
    return $$n
```

When passed two different integer types, a bare `$T` resolves to the wider one: `biggest(a, b)` with an `i32` and an `i64` returns `i64`. When `$T` appears inside a compound type like `&list[$T]`, that type fixes `$T`, so `append(l, k)` on a `&list[i64]` accepts `k: i32`.

When no value argument determines `$T`, accept a type directly with `type $T`. The caller passes a type name, and the parameter has zero runtime cost:

```flag
type Point:
    x: f64
    y: f64

func make(kind: type $T) -> $T:
    x: $T
    return x

func main():
    p := make(Point)
    n := make(i64)
```

### Where

`where` selects overloads at compile time using conditions on the types and numbers of the call, and on constants:

```flag
func kind(v: $T) -> &strview where $T is u8|u16|u32:
    return "unsigned"

func kind(v: $T) -> &strview where $T is i32|i64:
    return "signed"

func wide(v: $T) -> bool where @sizeof($T) >= 8:
    return true
```

You can also test the target platform with `@linux()`, `@macos()`, `@windows()`, `@wasm()`, `@x86_64()`, `@arm64()`, `@riscv64()`, `@wasm32()`, and `@wasm64()`, or check assignability with `@can_assign(&str, $T)`.

These builtins evaluate to a compile-time `bool` and work in ordinary expressions too:

- `if @windows():` still type-checks both branches on every platform.
- `a if c else b` with a compile-time `c` only checks and evaluates the chosen side, so `19 if @linux() else 8` stays an integer literal that fits `i32`.

A `where` at the start of a line opens a conditional block that exists only when its condition holds. Inside a function it wraps statements (such as OS-specific C calls); at the top level it wraps declarations and can test only the OS or CPU:

```flag
where @windows():
    LINE_END = "\r\n"

where not @windows():
    LINE_END = "\n"

extern "c" where @linux():
    func __errno_location() -> *i32

extern "c" where @macos():
    func __error() -> *i32

extern "c" where @windows():
    func _errno() -> *i32

func errno() -> i32:
    where @linux():
        return __errno_location().*
    where @macos():
        return __error().*
    where @windows():
        return _errno().*
```

`@compile_error` stops compilation with your custom message at the call site that picked that overload:

```flag
func small(n: $T) -> i32 where @sizeof($T) > 4:
    @compile_error("small takes numbers of 4 bytes or less")
```

### Function values

A function's name is a value whose type is its signature without parameter names, such as `func(i32) -> i32`. You can store functions in variables or arrays, pass them as arguments, and call them:

```flag
func double(x: i32) -> i32:
    return x * 2

func half(x: i32) -> i32:
    return x / 2

func apply(f: func(i32) -> i32, x: i32) -> i32:
    return f(x)

func main():
    f := double
    print(f(4), " ", apply(half, 10)) # 8 5
    steps: array[func(i32) -> i32, 2] = [double, half]
    for step in steps:
        print(step(6))
```

Structs can store function values in fields and invoke them directly:

```flag
type Button:
    name: &strview
    on_click: func(&strview)

func hello(who: &strview):
    print("hello ", who)

func main():
    b := Button{"ok", hello}
    b.on_click(b.name)
```

Generic functions can infer `$T` from a function-value parameter:

```flag
func first_by(items: &listview[$T], before: func($T, $T) -> bool) -> $T:
    best := items[0]
    for v in items:
        if before(v, best):
            best = v
    return best

func shorter(a: &strview, b: &strview) -> bool:
    return a.len < b.len

func main():
    names := ["alice", "bob", "carol"]
    print(first_by(ptr(names), shorter)) # bob
```

- **Overloaded or generic functions**: assign the name to a typed variable to pick the right instance (`show_int: func(i32) = show`).
- **Optional function values**: wrap the function type in parentheses before `?`: `(func(i32) -> i32)?`, checked with `is` like any other union.
- **No closures**: functions cannot capture local variables from an outer scope; pass any needed data as arguments (see [Function values](Reference.md#function-values)).

### Inline

The compiler automatically inlines calls to small functions, making small helpers and operators free. Mark a function or operator `inline` to inline it at every direct call regardless of size:

```flag
type V:
    x: f64
    y: f64

inline operator +(a: V, b: V) -> V:
    return V{a.x + b.x, a.y + b.y}

inline func scale(v: V, k: f64) -> V:
    return V{v.x * k, v.y * k}
```

- Recursive functions, entry-point `main`, and `export` functions cannot be marked `inline`.
- Calls through a function value always call out of line.
- `noinline func` prevents inlining so the function keeps its own frame in runtime error traces and profilers.
- Small structs like `V` live in registers, so `acc = acc + scale(V{1.0, 2.0}, 0.5)` in a loop runs as fast as the equivalent loop in C.

See [Inlining](Reference.md#inlining) for the exact rules and size limits.

## Strings

Flag has two text types: `&strview` (a view of text) and `&str` (owned, growable text). Every text literal is a `&strview`.

```flag
name: &str = heap("ana") # copies the text into an owned &str on the heap
defer free(name)
name += " and bo" # appends to the owned text
print(name.len, " ", name[0:3]) # 10 ana
```

Assigning a `&strview` to a `&str` copies into the `&str`'s memory (`name = "bo"`), while assigning one `&str` to another makes both share the same memory.

`+` concatenates text with any printable value, formatting numbers automatically:

```flag
line := "hi " + name + ", you are " + 30
```

The results of `+` and functions like `upper`, `split`, `join`, and `replace` are temporary values living in scratch memory that you do not `free`. Inside a `with scratch():` block, all temporaries created in that block are released when the block exits:

```flag
for i in 0..1000000:
    with scratch():
        print("line " + i)
```

Indexing and slicing operate on bytes:
- `s[i]` returns a `u8` byte, and `s[a:b]` returns a `&strview` slice.
- `s[i] = b` mutates a `&str`. A `&strview` is read-only: text literals live in read-only program memory, so copy to a `&str` to modify one.
- Negative literal or constant indexes count from the end: `s[-1]` is the last byte and `s[-3:-1]` are the two before it.
- Omitting the start defaults to `0`, and omitting the end goes to the end: `s[2:]` skips the first two bytes and `s[-3:]` keeps the last three.

Functions like `find`, `contains`, `split`, `join`, `replace`, `trim`, and `parse_int` are documented in [Text](Library.md#text).

`@embed("logo.png")` reads a file at compile time (resolved relative to the source file's folder) and embeds it directly into the executable as a `&strview`, like a text literal:

```flag
FONT = @embed("fonts/mono.ttf")
```

### Printing

`print` (to standard output) and `eprint` (to standard error) take any number of values and print them consecutively followed by a newline. Structs, unions, arrays, lists, and maps all format themselves automatically.

Core provides three formatting helpers:
- `fixed(x, digits)`: formats a float with a fixed number of decimal places
- `hex(n)` / `hex(n, width)`: formats an integer in hexadecimal, optionally zero-padded to `width` digits
- `pad(value, width)`: pads a formatted value with spaces to `width`

```flag
print(fixed(3.14159, 2), " ", hex(255), " ", hex(10, 4), " [", pad(7, 3), "]") # 3.14 0xff 0x000a [  7]
```

- `write(stdout, x)` prints a single value without a trailing newline.
- Each `print` outputs its entire line at once, so concurrent threads never interleave lines.
- Standard output is buffered in large chunks for speed and flushed automatically before waiting (for input, a network connection, another thread, or `sleep`), so prompts and logs appear while the program waits.
- `print` and `eprint` preserve call order, and buffered output is flushed on normal exit, `exit`, runtime errors (such as out-of-bounds), and crashes. A signal such as Ctrl-C discards buffered output (see [Files](Library.md#files)).

To customize how your own type prints, overload `operator +=` for `*&str`:

```flag
operator +=(s: *&str, p: Point):
    s.* += "(" + p.x + ", " + p.y + ")"
```

## Arrays

An array has a fixed length that is part of its type. Like structs, arrays are values, so assigning one copies all of its elements:

```flag
a := [1, 2, 3] # array[i64, 3]
b: array[u8, 16] # starts zeroed
c := array[i32, 2]{4, 5} # explicit array type
a[0] = 10 # bounds-checked; out-of-bounds stops the program
print(a[-1], " ", a.len) # negative literal indexes count from the end: 3 3
for x in a:
    print(x)
```

List and listview functions (such as `sort`) accept an array by its address:

```flag
sort(ptr(a))
```

Two arrays of the same type compare element by element with `==`. Arrays hold values and do not support element-wise arithmetic; use `simd` for vectorized math.

## SIMD

`simd[T, n]` packs `n` numbers into one or two 16-byte registers and applies operators across all lanes at once:

```flag
a: simd[f32, 4] = [1.0, 2.0, 3.0, 4.0]
b := simd[f32, 4](0.5) # broadcasts 0.5 to every lane
c := a * b + 1.0 # a scalar next to a simd broadcasts to every lane
print(c) # [1.5, 2.0, 2.5, 3.0]
c[0] = 9.0 # individual lane access requires a compile-time constant index
m := a > 2.0 # mask of type simd[bool32, 4] with lanes matching the width of a
print(m, " ", select(m, a, 0.0), " ", any(m), " ", sum(a)) # [false, false, true, true] [0.0, 0.0, 3.0, 4.0] true 10.0
```

Supported lane layouts:

| Lane types | 16-byte `simd` | 32-byte `simd` |
|---|---|---|
| `f32`, `i32`, `u32` | 4 lanes | 8 lanes |
| `f64`, `i64`, `u64` | 2 lanes | 4 lanes |
| `i16`, `u16` | 8 lanes | 16 lanes |
| `i8`, `u8` | 16 lanes | 32 lanes |

- Comparisons return masks, `select` picks lanes from either side based on a mask, and `min`, `max`, `abs`, and `sqrt` operate lane by lane.
- On x86_64 and arm64, each operation compiles to one or a few hardware instructions (two for 32-byte vectors) while keeping values in registers, so 4-lane code runs roughly 4x faster than scalar code.
- Wider vectors are represented as arrays of `simd`, such as `array[simd[f32, 8], 4]`.
- Load from and store back to a list using a matching-length slice: `v: simd[f32, 4] = values[i:i + 4]` and `values[i:i + 4] = v * 2.0`.

See [SIMD](Reference.md#simd) for the complete rules.

## Lists

A `&list[T]` is a growable list backed by `heap` or an `arena`. `heap([])` creates an empty heap list, and `heap` on an array or listview creates a heap list with a copy of its elements:

```flag
empty: &list[i64] = heap([])
start := heap([1, 2, 3]) # &list[i64] on the heap
bytes := heap([u8(1), 2]) # element type comes from the literal, not the target variable
```

```flag
nums: &list[i32] = heap([])
defer free(nums)
append(nums, 5)
append(nums, 3)
insert(nums, 0, 9) # [9, 5, 3]
sort(nums) # [3, 5, 9]
print(nums[0], " ", nums.len, " ", nums[0:2]) # 3 3 [3, 5]
print(nums[-1], " ", nums[-2]) # 9 5 (negative literal indexes count from the end)
last := pop(nums) # removes and returns the last element, or null if empty
first := remove(nums, 0) # removes and returns the element at index 0
```

Only negative indexes known at compile time (literals or constants) count from the end. A negative index computed at runtime is out of bounds like any other (`nums[i - 1]` with `i == 0` stops the program), and position-taking functions like `insert` and `remove` always count from the start.

Other operations include `find` (returns index or `null`), `contains`, `flip` (reverses in place), and `binary_search` for sorted lists; see [Lists and listviews](Library.md#lists-and-listviews). Lists and listviews compare element by element with `==`.

## Maps

A `&map[K, V]` is a hash map. You can start it empty (`heap({})`) or copy from a map literal (which on its own is a `fixmap` that holds just its pairs):

```flag
ages := heap({"bo": 25}) # &map[&strview, i64]
defer free(ages)
ages["ana"] = 30 # equivalent to put(ages, "ana", 30)
age := ages["ana"] # i64|null (null if "ana" is missing)
if age is i64:
    print("ana is ", age)
ages["cy"] += 1 # a missing key starts at zero, so this counts
for e in ages:
    print(e.key, " ", e.value)
```

Key behaviors:
- Core also provides `contains`, `remove` (returns the removed value or `null`), and `clear`.
- **Owned string keys**: a `&map[&str, V]` owns its keys. Inserting a new key copies it (even from a `&strview`, as in `ports[line[:4]] = 80`), and `remove`, `clear`, and `free` release the keys automatically.
- **Pointers into a map**: `ptr(ages["ana"])` returns a pointer to the stored value, inserting a zero value if the key is missing. Like a pointer into a list, it becomes invalid if the map grows.
- **In-place updates**: mutating a field or element of a map entry updates it in place and inserts a zero value if the key is missing (`players[name].score += 10`, or `pairs[a][b] += 1` when the value type is valid at zero, such as an array). Because the zero value of a `&list`, `&str`, or `&map` has no allocated memory, give a new key memory before growing it: `groups[k] = heap([])`, then `append(groups[k], v)` (and similarly `pairs[a] = heap({})` before `pairs[a][b] += 1` for a nested `&map`).
- **Valid key types**: numbers, text, `bool`, enums, pointers, arrays, structs and unions made from them, or any type defining `==` and `operator hash`.

A `fixmap[K, V, N]` is a fixed-capacity map storing up to `N` entries inline in insertion order. It never allocates and does not need `free`. `put` returns `MapFull` if there is no room for a new key:

```flag
colors: fixmap[&strview, u32, 4] = {"red": 0xff0000}
if put(colors, "green", 0x00ff00) is MapFull:
    print("no room")
print(colors["red"], " ", colors.len)
```

## Arenas

An `Arena` allocates memory in large blocks and releases everything in it at once. Allocation is fast, and `free` on the arena frees every object created in it:

```flag
type Enemy:
    hp: i32

func main():
    level: Arena # starts empty
    defer free(level) # releases everything in level
    boss := arena(level, Enemy{hp = 100}) # *Enemy in level (like heap(v))
    enemies: &list[Enemy] = arena(level, []) # empty list backed by level
    append(enemies, Enemy{hp = 10}) # grows inside level
    name: &str = arena(level, "boss") # text backed by level
    name += " final" # grows inside level too
    counts := arena(level, {"orc": 2}) # &map[&strview, i64] in level
    print(boss.hp, " ", enemies.len, " ", name, " ", counts)
```

A `&list`, `&str`, or `&map` created in an arena remembers that arena and grows inside it. Calling `free` on such a container empties it without freeing memory; the arena handles release. `heap` continues to allocate from the heap as usual.

`clear(a)` releases all allocations inside the arena while keeping its blocks for reuse, which fits per-frame game allocations:

```flag
frame: Arena
defer free(frame)
while running:
    clear(frame)
    visible: &list[*Enemy] = arena(frame, []) # lives for this frame only
```

The scratch memory used by temporaries is also an arena (marked by `with scratch():`), with one per thread. An arena you create cannot be shared across [threads](#threads) without a `Lock`. See [Arenas](Reference.md#arenas) for the exact rules.

## Contexts

A `context` is per-thread global state accessible from every function. Its fields start at zero or at their default value, and each thread gets its own copy. Because no code runs to initialize fields, fat-pointer fields must specify a default:

```flag
context app:
    depth: u32
    level: i32 = 3
    names: &list[&strview] = heap([])

func deeper():
    app.depth += 1
```

There are two additional context variants for multithreaded programs:
- `inherited context`: each thread spawned with `parallel` starts with a copy of the spawning thread's context rather than fresh defaults.
- `shared context`: a single instance shared across all threads, with its initial state written directly into program data by the compiler.

```flag
inherited context log:
    level: i32 = 1

shared context stats:
    visits: Lock[i64]
```

## Threads

`parallel f(x)` runs `f(x)` in a new thread and returns a `Thread[T]`, where `T` is `f`'s return type. `wait(t)` waits for the thread to finish and returns its result:

```flag
use os

func sum(from: i64, to: i64) -> i64:
    total: i64 = 0
    for i in from..to:
        total += i
    return total

func main():
    pieces: &list[Thread[i64]] = heap([])
    defer free(pieces)
    n := cores() # number of system processors
    for k in 0..n:
        append(pieces, parallel sum(1000000 * k / n, 1000000 * (k + 1) / n))
    total: i64 = 0
    for p in pieces:
        total += wait(p)
    print(total)
```

Arguments are evaluated in the calling thread and copied to the new thread. The return value can be anything: owned text, an error, or even a temporary, since `wait` merges the child thread's scratch temporaries into the waiting thread's scratch memory as if a normal call had created them.

`done(t)` checks whether a thread has finished without blocking, so you can, for example, load a game level while drawing a loading screen:

```flag
level := parallel load("castle.map")
while not done(level):
    draw_loading_screen()
start(try wait(level))
```

Writing `parallel f(x)` as a standalone statement detaches the thread. `main` waits for all detached threads before running its `defer` statements, so they can safely reference data kept by `main`. Any stored `Thread` handle must be read (via `wait` or `done`), and `-memory` warns if one is never waited. A function that can return an error cannot be detached, because its error would be lost.

### Synchronization

Each thread has its own contexts and scratch memory, while a `shared context` is shared across threads. Flag provides three synchronization primitives:

1. **`Lock[T]`**: allows one thread at a time to access the guarded `value` field (`Lock[null]` guards code without a value):

```flag
shared context stats:
    visits: Lock[i64]

func visit():
    with lock(stats.visits):
        stats.visits.value += 1
```

2. **`atomic[T]`**: a number or pointer shared across threads without a lock. It is not read or written as a regular value; instead use `get`, `set`, `add`, `swap`, and `compare_swap`, each compiling to inline processor instructions:

```flag
shared context stats:
    served: atomic[i64]

func serve():
    add(stats.served, 1)
```

3. **`Channel[T, N]`**: a bounded queue that passes values between threads, blocking when full or empty. It keeps the values inside itself, so it lives where you declare it:

```flag
func worker(jobs: *Channel[&str, 16]):
    for name in jobs: # runs until the channel is closed
        print("working on ", name)
        free(name)

func main():
    jobs: Channel[&str, 16]
    workers: &list[Thread[null]] = heap([])
    defer free(workers)
    for k in 0..4:
        append(workers, parallel worker(jobs))
    for name in ["a.txt", "b.txt", "c.txt"]:
        send(jobs, heap(name)) # worker frees what it receives, so send owned text
    close(jobs)
    for w in workers:
        wait(w)
```

### Visibility and thread checks

Writes made by one thread become visible to another across `parallel`, `wait`, a `Lock`, a `Channel`, or an `atomic`. `done` only reports that the thread finished; read its writes only after calling `wait`.

`-memory` also checks multithreaded code for:
- Data races: two threads accessing the same memory concurrently while at least one modifies it, outside a shared lock or atomic.
- Threads that are never waited or are waited twice.
- Pointers to local variables passed to threads that outlive the function.
- Temporaries whose `with scratch():` block may release them while a thread still uses them.
- Copying an active `Lock`, `Channel`, or `Arena` (pass a pointer instead).

```flag
func add(count: *i64):
    count.* += 1

func main():
    count: i64
    a := parallel add(ptr(count))
    add(ptr(count))
    wait(a)
```

Compiling with `-memory` reports:

```
warning: data race: count is accessed while a concurrent thread writes it with no synchronization; guard it with Lock[T] or wait() before accessing
```

## Error handling

### Error types

Declare errors with `type error`, and return them in a union from functions that can fail:

```flag
type error NotFound
type error enum FileError = CannotOpen|CannotRead

func index_of(names: &list[&strview], name: &strview) -> i64|NotFound:
    for i, n in names:
        if n == name:
            return i
    return NotFound
```

Errors can carry data just like structs; narrowing the error gives access to its fields:

```flag
type error BadLine:
    line: i64
    why: &strview

func check(line: i64, text: &strview) -> BadLine?:
    if text.len > 80:
        return BadLine{line, "too long"}
    return null
```

You cannot silently drop an error: any statement that leaves an error unhandled fails to compile. Handle errors with `is`, `switch`, `try`, `iferror`, or `trust`, or discard them explicitly with `_ =`:

```flag
got := index_of(names, "ana")
if got is i64:
    print("found at ", got)
_ = index_of(names, "bo")
problem := check(3, line)
if problem is BadLine:
    print("line ", problem.line, ": ", problem.why)
```

### `try`

`try` evaluates to the non-error value, or immediately returns the error from the enclosing function (whose return type must include that error):

```flag
func second(names: &list[&strview]) -> &strview|NotFound:
    i := try index_of(names, "bo")
    return names[i]
```

`try` only propagates errors; to propagate `null`, use `ifnull return null` (see [`iferror` and `ifnull`](#iferror-and-ifnull)). Because `try` applies to everything after it, accessing a field on its result requires parentheses: `(try get()).x`.

### `trust`

`trust` strips errors (or `null`, if there are no errors) without checking. Use it only when you are certain the operation cannot fail; if it does fail, you get garbage or a crash:

```flag
n := trust parse_int("42")
```

### `iferror` and `ifnull`

`iferror` yields the left-hand value or falls back to the right-hand side on error; `ifnull` does the same for `null`. The right-hand side runs only when needed:

```flag
age := index_of(names, "zoe") iferror 0
port := parse_int(text) ifnull 8080
```

The right-hand side can also be `return`, `break`, or `continue`, which leaves the expression unwrapped. When `a ifnull b` (or `iferror`) is a standalone statement, `b` can also be an assignment:

```flag
port := parse_int(text) ifnull return null
for text in texts:
    parse_int(text) ifnull bad += 1
    total += parse_int(text) ifnull continue
```

### When the program fails

Unrecoverable errors, such as out-of-bounds indexing, division by zero, failed `assert`, or `panic`, halt the program and print what happened, where it happened, and the call chain of your functions:

```
flag: index 5 out of bounds for length 3
  --> main.flg:12:11
  in parse_line, main.flg:9
  in load, main.flg:20
  in main, main.flg:31
```

`assert` checks a condition and prints the evaluated values when it fails:
- `assert total == 4` halts with `flag: total == 4 is false, total is 3`.
- `assert f is File` halts with `flag: f is FsError, not File: cannot open config.txt: No such file or directory`.
- A message after a comma replaces the default output: `assert total > 0, "the total cannot be empty"`.

To log a failure or save state before exiting, pass a callback to `on_panic`: it receives the error message and location for every such runtime error after they are printed (see [Runtime errors](Reference.md#runtime-errors)).

Crashes print the same call chain: dereferencing a null pointer prints `flag: invalid memory access at 0x0`, and unbounded recursion prints `flag: stack overflow`. An error in a [thread](#threads) shows that thread's call chain. Small functions inlined into their caller (see [Inline](#inline)) do not get a separate line unless marked `noinline`.

To inspect a running program, build with `-debug` and run it under `gdb` or `lldb`: they display variables with their Flag types and stop at source lines (see [Debugging](Reference.md#debugging)). With `-debug`, inlining is disabled so every function appears in the call chain.

## Modules

When you invoke `flagc`, you pass a list of files or folders. Passing a folder compiles every `.flg` file inside it recursively. All compiled files share access to each other's top-level functions, types, and constants unless restricted with modules.

Declare a module on the first line of a file so its declarations are visible only within that module and to files that `use` it:

```flag
module mymodulename
type Hello
```

`use` grants access both through the module's namespace and directly without qualification:

```flag
use mymodulename
func main():
    a: Hello = Hello
    b: mymodulename.Hello = mymodulename.Hello
```

Renaming with `as` gives a shorter alias and disables unqualified access:

```flag
use mymodulename as m
func main():
    b: m.Hello = m.Hello
```

Prefix a declaration with `priv` to hide it outside its module (or from all other files when the file has no `module` declaration):

```flag
priv MYCONST = 12
priv type T:
    x: i32
priv func foo():
    return
```

### Packages and name resolution

A package (third-party code or a separate part of your project) is a folder imported by a quoted path relative to the current file. The final folder names the module, and one of its files declares `module vec`. `flagc` loads the `.flg` files directly inside that folder (not subfolders), so `flagc run app/main.flg` is enough. `flagc` never downloads packages; a package is simply a copied folder or a git submodule:

```flag
use "vendor/vec"
func main():
    a := vec.V2{3.0, 4.0}
    print(dot(a, a))
```

- **Overloads across modules**: two used modules can define functions with the same name; argument types pick the right one. If both match equally well, the error tells you to qualify the call:
  `ambiguous call to size with (i32): multiple overloads match equally; qualify with a module name: a.size or b.size`
- **Shadowing standard library modules**: if you define a module with the same name as a standard library module (such as `math`), your files see yours while the standard library continues using its own.
- **Types bring their functions**: a call automatically sees functions from the modules that declare its argument types and any nested type arguments (such as `b.Door` inside `a.Table[b.Door]`) without requiring `use`. This lets a generic function in one module call functions defined alongside the types passed to it.

## The standard library

Core functionality is always available without `use`: text, lists, maps, basic math (`min`, `max`, `abs`, ...), files (`read_line`, `write`, ...), and concurrency (`Lock`, `Channel`, and `atomic` operations `get`, `add`, ...).

Additional modules are imported with `use` (no extra files needed). Using a name from a module without its `use` produces an error naming the module: `sqrt belongs to module math; add: use math`.

| Module | Provides |
|---|---|
| `fs` | Opening files, whole-file I/O, folders, and paths |
| `os` | CLI arguments, environment variables, subprocesses, shared libraries, CPU cores, and time |
| `json` | Reading and writing JSON |
| `png` | Reading and writing PNG images |
| `net` | TCP connections and UDP |
| `math` | `sqrt`, `sin`, `PI`, random numbers, and more |
| `utf8` | UTF-8 character inspection |
| `ttf` | Reading TrueType fonts and rendering antialiased glyphs |
| `bits` | Fixed-size bitsets |
| `ring` | Double-ended growable queue |
| `draw` | 2D drawing on the GPU for games and app interfaces |
| `window` | Window creation and events on Linux, Windows, and macOS |
| `vulkan`, `d3d11`, `metal` | Full bindings for the GPU APIs |
| `objc`, `x11`, `win32`, `cocoa` | Objective-C runtime and window-system bindings for Linux, Windows, and macOS |

Here is a program that counts the lines in a file:

```flag
use fs
use os

func main() -> i32:
    names := args()
    if names.len < 2:
        eprint("usage: lines file")
        return 1
    text := read_file(names[1])
    if text is FsError:
        eprint(text) # e.g. cannot open notes.txt: No such file or directory
        return 1
    defer free(text)
    lines := 0
    for b in items(text):
        if b == '\n':
            lines += 1
    print(lines, " lines")
    return 0
```

You can also stream a file line by line with `open_file` and `read_line`. Each line is a temporary, so wrapping the loop body in `with scratch():` keeps memory usage constant regardless of file size:

```flag
use fs
use os

func main() -> i32:
    input := open_file(args()[1])
    if input is FsError:
        eprint(input)
        return 1
    with input:
        while true:
            with scratch():
                line := read_line(input)
                if line is &strview:
                    print(line.len)
                else:
                    break
    return 0
```

Every function is documented in the [Library](Library.md).

## Calling C

Declare C functions in an `extern` block naming the library (`"c"` is the C standard library). Adding a `where` condition on the target OS links the library and exposes its declarations only on matching systems:

```flag
extern "c":
    func abs(x: i32) -> i32
    func snprintf(into: *u8, size: i64, format: cstr, ...) -> i32

extern "m":
    func sqrt(x: f64) -> f64

extern "ws2_32" where @windows():
    func WSAStartup(version: u16, into: *bytes) -> i32
```

A C string has type `cstr` (a pointer to null-terminated text):
- Text literals convert to `cstr` automatically.
- Other text uses `cstr(s)` (a temporary null-terminated copy in scratch memory) or `c_text(s)` (for a `&str`).
- `view(c)` converts a `cstr` back into a `&strview`, and `print` displays its text.
- Structs are passed using the C calling convention.

```flag
extern "c":
    func strlen(s: cstr) -> i64

func main():
    name: cstr = cstr("hola")
    print(strlen(name), " ", strlen("abc"), " ", view(name))
```

Use `= "symbol"` to bind a Flag declaration to a different C symbol name, which also lets you declare one C function under multiple typed signatures:

```flag
extern "objc":
    func send_id(self: *bytes, sel: *bytes) -> *bytes = "objc_msgSend"
    func send_f64(self: *bytes, sel: *bytes) -> f64 = "objc_msgSend"
```

C global variables are declared by address as pointer constants. An `extern` block can also name a macOS framework or a specific library file:

```flag
extern "c":
    index: *i32 = "optind" # index.* = 1 writes optind

extern "Metal.framework" where @macos():

extern "libfreetype.so.6" where @linux():
```

`export func` exposes a Flag function to C using its plain name and the C calling convention. Its value has type `extern func(...)`, which is how you pass callbacks to C:

```flag
extern "c":
    func qsort(base: *bytes, count: i64, size: i64, compare: extern func(*i32, *i32) -> i32)

export func ascending(a: *i32, b: *i32) -> i32:
    return a.* - b.*

func main():
    values: array[i32, 3] = [3, 1, 2] # 4-byte elements matching C's int
    qsort(ptr(values) as *bytes, 3, 4, ascending)
```

Calls through `extern func(...)` use the C ABI (including structs), so it represents C function tables and any address that C gives you, cast with `as`: `f := address as extern func(f64) -> f64`. Flag's own `func(...)` values do not cross to C.

`@offset_of(T, field)` gives a field's byte offset at compile time, just like `@sizeof` gives a type's size.

Build a dynamically loadable shared library with `-shared`, and load it at runtime with `load_library` and `symbol` from [os](Library.md#libraries). This is how a program or game hot-reloads its code while running; see [Libraries](Reference.md#libraries).

## Memory safety

### Checking with `-memory`

Passing `-memory` runs compile-time memory checking:

```sh
flagc main.flg -o main -memory
```

It is optional because it slows compilation.

Outside `@trust`, a program that passes `-memory` without warnings will not leak memory, double-free, free non-heap memory, use freed memory, or have data races. This guarantee stops at `trust`, `as`, `*bytes` pointer arithmetic, mutating a narrowed variable through a pointer, null/zero pointers, and C code, aside from a few documented blind spots (see [The memory checker](Reference.md#the-memory-checker)).

`-memory` warns about:
- memory that is never freed
- memory freed twice
- memory used after being freed or after a list grew and reallocated
- temporaries used after their `with scratch():` ended, and arena memory used after `free` or `clear`
- calling `free` on a pointer that lives in an arena, or failing to free an arena
- pointers to function or block stack variables used after the scope ended
- an owner that may grow without allocated memory because only some paths initialized it
- two threads accessing the same memory concurrently while one modifies it, outside a lock held by both

The checker tracks elements of a list, array, or map as none, some, or all freed, so freeing every element in a loop and then freeing the list is clean, whereas freeing only some elements warns (see [Elements](Reference.md#elements)). It also tracks active union members, so checking `if result is Bad: return` and then freeing the other member is clean.

### `@trust` and `@check`

`@trust` suppresses `-memory` warnings in code you have manually verified:
- At the end of a line, it covers that line and any block it opens.
- On its own line, it covers everything after it at that indentation (so placing `@trust` at the top of a file trusts the entire file).

```flag
func risky(p: *i64): @trust
    p.* = 3

func main():
    x := 0
    risky(ptr(x))
    print(x) @trust
```

`@check` re-enables warnings inside a trusted region. `-memory` skips the trusted code that the checked code does not need, so trusting verified code also speeds up checking.

## Compiling

Compile one or more files or folders into a binary:

```sh
flagc main.flg other.flg some_folder -o program
```

`flagc run` compiles and runs the program without requiring `-o`. Anything after `--` is passed as command-line arguments to the program:

```sh
flagc run main.flg -- first second
```

Common flags:

| Flag | Description |
|---|---|
| `-o name` | Output executable name (defaults to `a.out`, or `a.exe` on Windows; `flagc run` deletes the binary afterward unless `-o` is given) |
| `-shared` | Build a shared library instead of an executable (see [Libraries](Reference.md#libraries)) |
| `-check` | Type-check without generating a binary |
| `-memory` | Run the compile-time memory checker (see [Memory safety](#memory-safety)) |
| `-debug` | Emit debug information for `gdb` and `lldb` (see [Debugging](Reference.md#debugging)) |
| `-target target` | Cross-compile for another platform, e.g. `-target windows-x86_64`, or make a WebAssembly module with `-target wasm-wasm32` or `-target wasm-wasm64` (see [Platforms](Reference.md#platforms)) |
| `-cc ...` | Pass all remaining arguments directly to the C compiler (which assembles and links), e.g. `-cc -lm` or `-cc -static` |

See [The command line](Reference.md#the-command-line) for all flags.

`flagc` uses `cc` by default. To use a different C compiler, set the `CC` environment variable:

```sh
CC=gcc flagc main.flg
```

### Tests

`flagc test` builds your files and runs every `func test_name()`. Each test runs in its own process in parallel, so a failed `assert`, crash, or hang does not stop the remaining tests:

```flag
func test_gcd():
    assert gcd(12, 18) == 6
```

```sh
flagc test src -- gcd
```

Words after `--` filter tests to those whose name contains at least one of those words. Any test running longer than 10 seconds is stopped (`-timeout` changes this limit). At the end, `flagc` reports every failing test with its location and output, and exits with status `1` if any test failed:

```
FAIL test_gcd, src/math.flg:3, exit code 134
flag: gcd(12, 18) == 6 is false, gcd(12, 18) is 4
  --> src/math.flg:4:5
  in test_gcd, src/math.flg:3
0 passed, 1 failed
```
