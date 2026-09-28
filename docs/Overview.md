This is an introduction to the language, I assume that you have basic programming knowledge, and we will see how to do the basic things in the language. The exact rules are in the [Reference](Reference.md), and every function of the standard library is in the [Library](Library.md).

## Hello world
This is really simple, it's like Python
```flag
func main():
    print("hello world")
```
`main` can also return an `i32`, that is the exit code of the program
```flag
func main() -> i32:
    print("hello world")
    return 0
```
To run it, save it as `hello.flg` and use `flagc run`, it compiles the program and runs it
```sh
flagc run hello.flg
```
Or compile it to keep the program
```sh
flagc hello.flg -o hello
./hello
```

## Comments
You can comment with #, there are no multiline comments
```flag
# this is a comment
a := 3 # this too
```

## Variables and Assignments
Variables are valid in the scope where they are declared, so they must be declared inside some scope (a function, an if, a loop...)
```flag
a: i32 # declares 'a' to be of type 'i32', variables start at zero
b: u32 = 3 # declares 'b' to be of type 'u32' and sets it to 3
if true:
    a: u32 # error, 'a' is already declared in this scope
```
You can't declare a name that is already visible, not even inside an inner scope.

The type of a variable can also be inferred with ':='
```flag
a := 3 # declares 'a' to be of type 'i32' and sets it to 3
b := 2.5 # 'b' is 'f64'
```
Assignments can do some transformations between types, for example:
```flag
view: &strview = "hello" # a text literal is a '&strview'
text: &str = view # the text is copied into a new '&str' that you own
back: &strview = text # a '&str' can be used as a '&strview' without copying
n: i64 = a # smaller integers grow to bigger ones
maybe: i32|null = a # a value goes into a union that has its type
b: *array[i32, 2] = heap([1, 2])
l: &list[i32] = b # here '*array[i32, 2]' is transformed to '&list[i32]' at assignment
```
The other conversions are written like a call to the type
```flag
c := f64(a) / 2.0
d := u8(a)
```

## Literals
- Integers: `3`, `-3`, `0xff`, `0b101`, and `_` can separate the digits, `1_000_000`. They take the type they are used with, and alone they are `i32` (or `i64`/`u64` if they don't fit)
- Floats: `2.5`, `1e3`. Alone they are `f64`
- Characters: `'a'` is the number of that character (97), so it works with `u8`
- Text: `"hello\n"`, a text literal is a `&strview`. The escapes are `\n \t \r \0 \\ \" \' \e`, and `\x41` is the byte with that hexadecimal number
- `true`, `false` and `null`
- Arrays: `[1, 2]` is an `array[i32, 2]`, and if the elements have different types the array holds their union, so `[1, "hola"]` is an `array[i32|&strview, 2]`. `[]` is empty, it becomes an empty list, slice or text (see [Lists](#lists))
- Maps: `{"ana": 30, "bo": 25}` is a `&map[&strview, i32]` and `{}` is an empty map (see [Maps](#maps))
- Structs: `{1, 2}` or `{x = 1}` build the struct that the place expects (see [Structured Types](#structured-types))

When you give the type, the literal takes it
```flag
a: array[u8, 3] = [1, 2, 3]
ages: &map[&strview, u8] = {"ana": 30}
```

## Constants
A constant is a symbol that the compiler will replace at compilation. They must be declared at the root of the file so they cannot be inside scopes
```flag
MYCONST = "hello"
NUM = 2
MYSECONDCONST = 2 + 2 + NUM # this computes at comptime to 6
```
Arithmetic between literals is computed by the compiler, any other expression is written again where you use the constant, like a macro.

## Operators
- Arithmetic: `+`, `-`, `*`, `/`, and `%` for integers
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `and`, `or` and `not` on `bool`. `and` and `or` don't run the right side when the left side already decides
- Bits: `and`, `or` and `xor` on integers work on the bits, and `<<` and `>>` shift them
- Assignments: `=`, and `+=`, `-=`, `*=`, `/=`, `%=`
```flag
flags := 0b1010 and 0b0110 # 0b0010
big := 1 << 20
if x > 0 and not done:
    count += 1
```
Both sides must have the same type, a literal takes the type of the other side and anything else you convert (see [Variables and Assignments](#variables-and-assignments)). Integers wrap around when they overflow, and dividing by zero stops the program with an error.

## Types
### Builtin types
- `bool`
- `i8`, `i16`, `i32`, `i64` and `u8`, `u16`, `u32`, `u64`, `usize`
- `f32`, `f64`
- `bytes`, raw memory of unknown size, only used behind a pointer

`sizeof(T)` gives the size of a type in bytes.

In flag you can declare 3 kinds of types, and aliases
### Void Types
A void type is a type that contains no data, all the void types can be used as values
```flag
type Red

func main():
    color: Red = Red
```
### Structured Types
These are types that declare a memory layout
```flag
type User:
    name: &str
    age: u32
```
You can build them with the fields in order, or naming them. The fields you don't name start at zero
```flag
a := User{"ana", 30}
b := User{age = 30}
```
When the place already says the type, like a variable with its type, a return, a field or an element of an array, you can leave the name out
```flag
c: User = {"bo", 25}
d: User = {age = 25}
```
You can nest structured types
```flag
type Car:
    model: &strview
    motor: # this generates the type Car.motor
        hp: u32

func main():
    motor: Car.motor = Car.motor{hp = 12}
    car: Car = Car{"mini", motor}
    other: Car = {"fiat", {hp = 90}} # the inner {} is a Car.motor
    car.motor.hp += 10
```
A field can share its memory with a field declared above it with '@'
```flag
type Record:
    code: u32
    id: u32 @code # 'id' uses the same bytes as 'code'
```
`packed type` puts every field right after the one before, without padding, for binary formats and hardware registers
```flag
packed type Header:
    kind: u8
    size: u64 # starts at byte 1, and the struct takes 9 bytes instead of 16
```
Structured types are values, assigning them or passing them to a function copies them.
### Aliases
An alias is another name for a type, the two names are the same type
```flag
type Id = u32

func main():
    user: Id = 7
    n: u32 = user
```
### Union Types
Union types are tagged unions and are separated by '|', you give them a name with an alias
```flag
type Num = i32|f32
```
`T?` is a short way to write `T|null`
```flag
found: usize? = null
```
You check which type a union holds with `is`, and inside an `if` it narrows the type (see [If](#if))
```flag
n: Num = 3
if n is i32:
    print("an integer ", n)
```
There are also untagged unions with '||' (but they are not recommended, they have no tag to check and are read with `as`)
#### Enums
In flag an enum is a union type, there are 2 ways to declare an enum
1. Union like
```flag
type enum Color = Red|Green|Blue
# it will define Red, Green and Blue as void types (if they are already defined as void types it will just use them)
```
2. Enum like
```flag
type enum Color:
    Red, Green
    Blue
# is equivalent to 1.
```
### Generic types
Types can take type parameters with `$`
```flag
type Pair[$T]:
    a: $T
    b: $T

func main():
    p: Pair[i32]
    p.a = 1
```
And numbers with `$$`
```flag
type Ring[$T, $$n]:
    items: array[$T, $$n]
    start: usize

func main():
    r: Ring[u8, 16]
```
### Reinterpreting with as
`as` reads the bytes of a value as another type of the same size, it does not convert
```flag
bits := f32(1.5) as u32 # the bits of the float
```
It can also take a type out of a union, but it does not check the tag, so check it with `is` first.

## Pointers
A pointer is written with '*' before the type. `ptr(x)` gives you the address of `x` and `.*` reads or writes what the pointer points to
```flag
x := 5
p := ptr(x) # 'p' is '*i32'
p.* = 6 # now x is 6
```
Fields are reached through the pointer without `.*`
```flag
type Counter:
    n: i32

func bump(c: *Counter):
    c.n += 1

func main():
    c := Counter{0}
    bump(ptr(c))
    bump(c) # the same, a '*Counter' parameter takes a Counter variable and gets its address
    h := heap(Counter{10}) # heap copies a value to the heap and gives you a '*Counter'
    h.n += 1
    free(h as *bytes)
```
When a function takes `*T` and you pass a variable of type `T`, and there is no overload that takes `T`, the compiler passes its address for you.

`null` is not a pointer, a pointer that can be missing is `*T?`.

## Fat pointers
A fat pointer is a pointer with extra data, in flag we indicate it with '&' before the type.
For example '&list[i32]' is a pointer to the elements of a list with the fields 'len: usize' and 'cap: usize'
```flag
a: &list[i32]
```
The library has these fat pointers
- `&strview`, a view of text
- `&str`, text that you own and can grow
- `&slice[T]`, a view of elements
- `&list[T]`, a list that you own and can grow
- `&map[K, V]`, a hash map that you own

A fat pointer starts empty, you don't need to create it, just use it
```flag
a: &list[i32]
append(a, 1)
```
The ones you own (`&str`, `&list` and `&map`) must be released with `free` when you don't need them anymore, `defer` helps with that. A `&str` can be used where a `&strview` is expected, and a `&list` where a `&slice` is expected.

You can declare your own fat pointers with `type fatptr`
```flag
type fatptr ring[$T]:
    len: usize
    start: usize
```
### Passing them to functions
When you pass a fat pointer to a function, the function gets a copy of it. The elements are shared, but if the function makes the copy grow, the original does not see it.

To let a function change the fat pointer itself, declare the parameter as `*&something`. You still call it passing the fat pointer, the compiler passes `ptr(something)` for you like with any pointer (see [Pointers](#pointers)). Indexing works through the pointer, like fields
```flag
func add_two(l: *&list[i32]):
    append(l, 2)
    l[0] += 1

func main():
    nums: &list[i32]
    append(nums, 1)
    add_two(nums) # the same as add_two(ptr(nums))
    print(nums) # [2, 2]
```
This is how `append`, `free`, `put` and `+=` work.

## Control Flow
### If
Allows for conditional evaluation of code
```flag
if condition:
    print("is true")
```
Can follow with elif and else
```flag
if cond1:
    print("1 is true")
elif cond2:
    print("2 is true")
else:
    print("all are false")
```
You can also use the if statement to narrow unions
```flag
type enum Color = Red|Green|Blue

color: Color = Red
if color is Red:
    # the type of color is Red here
elif color is Blue:
    # the type of color is Blue here
else:
    # the type of color is Green here
    color = get_random_color() # color goes back to Red|Green|Blue from here
```
To check several types at once use `is` with a union
```flag
if color is Red|Blue:
    # the type of color is Red|Blue here
```
### While
```flag
while condition:
    print("again")
```
### For
For lets you iterate over an iterable object
```flag
for element in iterable:
    print(element)

for index, element in iterable:
    print(index, " ", element)
```
To iterate over a range you can do (the end is not included):
```flag
for i in 0..10:
    print(i)
```
Iterating a text gives you each character as a `&strview`, and iterating a map gives you each entry with `.key` and `.value`
```flag
for c in "hello":
    print(c)
```
The iterables can be reversed with reverse (depends on the iterable, ranges, arrays, lists, slices and text can)
```flag
for element in reverse iterable:
    print(element)

for i in reverse 0..10: # from 9 to 0
    print(i)
```
With `for index, element in reverse iterable` the index still counts 0, 1, 2... from the first element you get.

You can make your own types iterable. `for` calls `iter` with your value and then `next` with a pointer to what `iter` gave, until `next` gives `IterationEnd`. `reverse` calls `reverse_iter` instead of `iter`
```flag
type Countdown:
    from: i32

type Down:
    at: i32

func reverse_iter(c: Countdown) -> Down:
    return Down{c.from}

func next(d: *Down) -> i32|IterationEnd:
    if d.at == 0:
        return IterationEnd
    d.at -= 1
    return d.at + 1

func main():
    for n in reverse Countdown{3}:
        print(n) # 3, 2, 1
```
### Switch
Switch compares a value with each case, only the first case that matches runs. A case can have several values and ranges, and `case:` alone is the default
```flag
switch n:
case 0:
    print("zero")
case 1..10, 100:
    print("small")
case:
    print("other")
```
It also works with text and with unions of void types, and a switch over a union must handle every type or have a default
```flag
switch color:
case Red:
    print("red")
case Green, Blue:
    print("not red")
```
### break, continue
`break` leaves the loop and `continue` goes to the next round
```flag
for i in 0..10:
    if i == 2:
        continue
    if i == 5:
        break
    print(i)
```
With a number they go out of more loops, `break 2` leaves the loop around this one too, and `continue 2` goes to its next round
```flag
for row in 0..10:
    for column in 0..10:
        if row * column == 42:
            break 2
```
### Defer
`defer` runs a statement when the block where you wrote it ends, also when you leave it with return, break or continue. They run in reverse order
```flag
func main():
    names: &list[&strview]
    defer free(names)
    append(names, "ana")
```
A defer cannot have return, break, continue or try inside.
### With
`with` calls `close` on the value when the block ends, also when you leave it early
```flag
type File:
    fd: i32

func close(f: File):
    print("closed ", f.fd)

func main():
    with File{7}:
        print("using the file")
```

## Functions
```flag
func add(a: i32, b: i32) -> i32:
    return a + b
```
### Parameters
The parameters are copies of what you pass. To let a function change your value, pass a pointer (see [Pointers](#pointers) and [Passing them to functions](#passing-them-to-functions)).
#### Named Arguments
You can name the arguments in any order, after a named argument the rest must be named too
```flag
area(height = 3, width = 5)
area(2, height = 10)
```
#### Default Values
```flag
func label(text: &strview, size: i32 = 20, bold: bool = false):
    print(text, " ", size, " ", bold)

func main():
    label("a")
    label("b", 10)
    label("c", bold = true)
```
The default is computed where you call the function, so `@here()` as a default gives the function the place of the call, that's how `assert` and the index errors know where they happened
```flag
func log(message: &strview, place: &strview = @here()):
    print(place, ": ", message)
```
#### Variadics
Only C functions can be variadic, with `...` at the end of the parameters (see [Calling C](#calling-c))
### Overloading
Functions can have the same name with different parameters, the call uses the one that fits best
```flag
func describe(n: i32) -> &strview:
    return "an integer"

func describe(t: &strview) -> &strview:
    return "a text"
```
### Operator overloading
You can overload operators for your types. `!=`, `>`, `<=` and `>=` come from `==` and `<`, and `a += b` uses `+`
```flag
type V:
    x: i32
    y: i32

operator +(a: V, b: V) -> V:
    return V{a.x + b.x, a.y + b.y}

operator ==(a: V, b: V) -> bool:
    return a.x == b.x and a.y == b.y
```
`operator =` makes your type from another one, and then assignments and `T(x)` convert for you
```flag
type Celsius:
    degrees: f64

operator =(c: Celsius, fahrenheit: f64) -> Celsius:
    return Celsius{(fahrenheit - 32.0) * 5.0 / 9.0}

func main():
    boiling: Celsius = 212.0 # 100 degrees
```
### Indexing
`x[i]` calls `at(x, i)`, so your types can be indexed too. When `at` gives a pointer you can also assign to `x[i]`
```flag
type Grid:
    cells: array[i32, 9]

func at(g: *Grid, i: i32) -> *i32:
    return ptr(g.cells[i])

func main():
    g: Grid
    g[4] = 7
```
The compiler calls other functions by their name too, like `iter` for `for` or `close` for `with`, they are all in [Protocol functions](Reference.md#protocol-functions).
### Generics
A parameter with a `$` type takes any type, and `$$` takes a number
```flag
func biggest(a: $T, b: $T) -> $T:
    if a > b:
        return a
    return b

func size(a: array[$T, $$n]) -> usize:
    return $$n
```
### Where
`where` chooses an overload at compile time, with conditions over the types and numbers of the call
```flag
func kind(v: $T) -> &strview where $T is u8|u16|u32:
    return "unsigned"

func kind(v: $T) -> &strview where $T is i32|i64:
    return "signed"

func wide(v: $T) -> bool where sizeof($T) >= 8:
    return true
```
It can also ask for the platform with `@linux()`, `@macos()`, `@windows()`, `@x86_64()`, `@arm64()` and `@riscv64()`, and if one type can be assigned to another with `can_assign(&str, $T)`.

`@compile_error` stops the compilation with your message, at the call that chose that overload
```flag
func small(n: $T) -> i32 where sizeof($T) > 4:
    @compile_error("small takes numbers of 4 bytes or less")
```

## Strings
There are two kinds of text: `&strview` is a view of some text, and `&str` is text that you own and can grow. A text literal is a `&strview`.
```flag
name: &str = "ana" # copies the text into a '&str'
defer free(name)
name += " and bo" # appends to the text you own
print(name.len, " ", name[0:3]) # 10 ana
```
`+` joins text with anything that can be printed, numbers are formatted
```flag
line := "hi " + name + ", you are " + 30
```
The result of `+`, and of functions like `upper`, `split`, `join` or `replace`, is a temporary text. Temporaries live in a scratch memory that you don't free, and with `with scratch():` everything temporary made inside the block is released when it ends
```flag
for i in 0..1000000:
    with scratch():
        print("line " + i)
```
Indexing a text `s[i]` gives you the byte as `u8`, and `s[a:b]` gives you a `&strview` of that part. Negative indexes count from the end, so `s[-1]` is the last byte and `s[-3:-1]` are the two before it.

The library has `find`, `contains`, `split`, `join`, `replace`, `trim`, `parse_int` and more, they are in [Text](Library.md#text).
### Printing
`print` and `eprint` (to the error output) take any number of values and print them one after the other. Structs, unions, arrays, lists and maps know how to print themselves. `fixed` prints a float with some decimals and `hex` an integer in hexadecimal
```flag
print(fixed(3.14159, 2), " ", hex(255)) # 3.14 0xff
```
To print your own type another way, overload `+=` for text
```flag
operator +=(s: *&str, p: Point):
    s.* += "(" + p.x + ", " + p.y + ")"
```

## Arrays
An array has a fixed size that is part of its type, it is a value like a struct, so assigning it copies it
```flag
a := [1, 2, 3] # array[i32, 3]
b: array[u8, 16] # starts at zero
c := array[i32, 2]{4, 5} # you can also name the type
a[0] = 10 # indexes are checked, going out stops the program with an error
print(a[-1]) # a negative index counts from the end, so this is 3
for x in a:
    print(x)
```
The functions of lists and slices, like `sort`, take an array by its address
```flag
sort(ptr(a))
```
Two arrays of the same type take the operators element by element: `[1, 2] + [10, 20]` is `[11, 22]`, and `==` compares every element.

## Lists
A `&list[T]` is a list that grows as you add elements. It starts empty, and you can also start it from an array on the heap
```flag
empty: &list[i32] = heap([])
start: &list[i32] = heap([1, 2, 3]) # the list owns the heap array, with len and cap 3
```
```flag
nums: &list[i32]
defer free(nums)
append(nums, 5)
append(nums, 3)
insert(nums, 0, 9) # [9, 5, 3]
sort(nums) # [3, 5, 9]
print(nums[0], " ", nums.len, " ", nums[0:2]) # 3 3 [3, 5]
print(nums[-1], " ", nums[-2]) # 9 5, negative indexes count from the end
last := pop(nums) # the last element, or null if the list is empty
first := remove(nums, 0) # takes out the element at 0
```
Also `find` (gives the index or null), `contains`, `reverse` (turns the list around in place) and `binary_search` for a sorted list, see [Lists and slices](Library.md#lists-and-slices). Two lists or slices compare with `==` element by element.

## Maps
A `&map[K, V]` is a hash map, you can start it empty or with a literal
```flag
ages := {"bo": 25} # &map[&strview, i32]
defer free(ages)
ages["ana"] = 30 # the same as put(ages, "ana", 30)
age := ages["ana"] # i32|null, the same as get(ages, "ana")
if age is i32:
    print("ana is ", age)
ages["bo"] = (ages["bo"] ifnull 0) + 1
for e in ages:
    print(e.key, " ", e.value)
```
Also `contains`, `remove` (gives the value or null) and `clear`. The keys can be numbers, text, `bool`, enums, pointers or anything that has `==` and `hash`.

A `fixmap[K, V, N]` is a fixed map, it keeps up to N entries inside itself in the order you put them, so it never allocates and you don't free it. `put` gives `MapFull` when there is no room for a new key
```flag
colors: fixmap[&strview, u32, 4] = {"red": 0xff0000}
if put(colors, "green", 0x00ff00) is MapFull:
    print("no room")
print(colors["red"], " ", colors.len)
```

## Contexts
A context is global state, the fields start at zero and every function can use them
```flag
context app:
    depth: u32
    names: &list[&strview]

func deeper():
    app.depth += 1
```

## Error handling
### The error type
An error is a void type declared with `type error`, and a function that can fail returns it in a union
```flag
type error NotFound
type error enum FileError = CannotOpen|CannotRead

func index_of(names: &list[&strview], name: &strview) -> usize|NotFound:
    for i, n in names:
        if n == name:
            return i
    return NotFound
```
You cannot forget an error, a statement that leaves an error without handling does not compile. You handle it with `is`, `try`, `trust`, or you ignore it on purpose with `_ =`
```flag
got := index_of(names, "ana")
if got is usize:
    print("found at ", got)
_ = index_of(names, "bo")
```
### try operator
`try` gives you the value, and if there is an error it returns it from the function, so the function must return that error too
```flag
func second(names: &list[&strview]) -> &strview|NotFound:
    i := try index_of(names, "bo")
    return names[i]
```
### trust operator
`trust` removes the errors (or the null if there are no errors) without checking. Use it only when you know it cannot fail, if it fails you get garbage or a crash
```flag
n := trust parse_int("42")
```
### iferror and ifnull
`iferror` gives the value, or what is on its right when there is an error. `ifnull` does the same with `null`. The right side only runs when it's needed
```flag
age := index_of(names, "zoe") iferror 0
port := parse_int(text) ifnull 8080
```
### When the program fails
Some errors stop the program, like an index out of bounds, a division by zero, `assert` or `panic`. They print what happened and where
```
flag: index 5 is out of bounds, length 3
  --> main.flg:12:11
```
`assert` checks a condition, and it can take a message: `assert(total > 0, "the total cannot be empty")`.

## Modules
When you compile you give the compiler a list of files or folders, compiling a folder is equivalent to compiling every `.flg` file inside the folder, including the files inside other folders. The code of all the files is compiled together, so all the files have access to all the functions, types and constants in the other files.
To restrict access there are modules. A module is declared at the start of the file, and means that all the things in the file will be available only to the files of the module and to the files that use it
```flag
module mymodulename
type Hello
```
To give access to a module from another file you use 'use'.
This gives a namespace so you can access things from the module and also includes them directly to use
```flag
use mymodulename
func main():
    a: Hello = Hello
    b: mymodulename.Hello = mymodulename.Hello
```
The namespace can also be renamed, but renaming removes the direct include
```flag
use mymodulename as m
func main():
    b: m.Hello = m.Hello
```
To hide a name from the files outside the module add priv at the start
```flag
priv MYCONST = 12
priv type T:
    x: i32
priv func foo():
    return
```

## The standard library
The functions of text, lists and maps are always there. The rest comes in modules that you get with `use`, without adding files
- `fs`, files and folders
- `os`, the arguments, the environment, other programs and the time
- `json`, reading JSON
- `net`, TCP sockets
- `math`, `sqrt`, `sin`, `min`, random numbers...
- `utf8`, characters of UTF-8 text
- `bits`, sets of numbers as bits
- `ring`, a queue that grows at both ends
- `raylib`, windows, drawing, input and audio

This program counts the lines of the file you give it
```flag
use fs
use os

func main() -> i32:
    names := args()
    if names.len < 2:
        eprint("usage: lines file")
        return 1
    text := read_text_file(names[1])
    if text is FsError:
        eprint("cannot read ", names[1], ": ", text)
        return 1
    defer free(text)
    lines := 0
    for b in items(text):
        if b == '\n':
            lines += 1
    print(lines, " lines")
    return 0
```
Every function is in the [Library](Library.md).

## Calling C
You declare C functions inside an `extern` block with the library they come from, `"c"` is the C library. A block can have a `where` like a function, and then its library is linked only on the systems where it holds
```flag
extern "c":
    func abs(x: i32) -> i32
    func snprintf(into: *u8, size: usize, format: *u8, ...) -> i32

extern "m":
    func sqrt(x: f64) -> f64

extern "ws2_32" where @windows():
    func WSAStartup(version: u16, into: *bytes) -> i32
```
A C string has the type `cstr` (it is a `*u8`), and a text literal or a `&str` is converted to a `cstr` for you when you pass it. Other text needs the function `cstr`, the copy lives in the temporary memory like the results of `+`, and `view` turns a `cstr` back into a `&strview`. Structs are passed like C does.
```flag
extern "c":
    func strlen(s: cstr) -> usize

func main():
    name: cstr = cstr("hola")
    print(strlen(name), " ", strlen("abc"), " ", view(name))
```
`export func` makes a function that C can call, with the plain name and the C calling convention. `ptr` of an export function gives you its address, that's how you pass callbacks to C
```flag
extern "c":
    func qsort(base: *bytes, count: usize, size: usize, compare: *bytes)

export func ascending(a: *i32, b: *i32) -> i32:
    return a.* - b.*

func main():
    values := [3, 1, 2]
    qsort(ptr(values) as *bytes, 3, 4, ptr(ascending))
```

## Memory safety
### The flag
Compiling with `-memory` checks the memory of your program at compile time, and warns about:
- memory that is never freed
- memory freed twice
- memory used after it was freed or after a list grew and moved it
- temporaries used after their `with scratch():` ended
- pointers to the stack of a function that leave it

It is optional because it makes the compilation slower.
```sh
flagc main.flg -o main -memory
```
### @trust @check
`@trust` hides the warnings of the code you already checked. At the end of a line it covers that line and the block it opens, and on a line of its own it covers everything after it at that indentation, so at the top of a file it covers all the file
```flag
func risky(p: *i32): @trust
    p.* = 3

func main():
    x := 0
    risky(ptr(x))
    print(x) @trust
```
`@check` does the opposite, inside a trusted part it brings the warnings back. `-memory` skips the trusted code that the rest of the program doesn't need, so trusting code also makes it faster.

## Compiling
```sh
flagc main.flg other.flg some_folder -o program
```
`flagc run` is the same but it also runs the program, and you don't need `-o`. What goes after `--` goes to the program
```sh
flagc run main.flg -- first second
```
- `-o` the name of the program (`a.out` if you don't give one, `a.exe` on Windows, with `run` the program is removed after it runs unless you give one)
- `-check` only checks the code, without building the program
- `-memory` checks the memory (see [Memory safety](#memory-safety))
- `-target` builds for another system, like `-target windows-x86_64` (see [Platforms](Reference.md#platforms))
- anything else goes to the C compiler, that only assembles and links the program, for example `-lm` to link a library or `-static`

The rest of the flags are in [The command line](Reference.md#the-command-line).

The C compiler is `cc`, if yours has another name put it in the `CC` variable
```sh
CC=gcc flagc main.flg
```
