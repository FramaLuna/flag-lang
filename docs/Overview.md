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
Variables are declared inside a function and live in the block where they are declared, an `if`, a loop or the function itself. At the root of a file a name is a [constant](#constants)
```flag
a: i32 # declares 'a' to be of type 'i32', variables start at zero
b: u32 = 3 # declares 'b' to be of type 'u32' and sets it to 3
if true:
    a: u32 # error, 'a' is already declared in this scope
```
You can't declare a name that is already visible, not even inside an inner scope. That goes for parameters and the variables of a `for` too, and for the constants and contexts that a function sees, yours, core's like `stdout` and those of a module you `use` without an alias
```flag
N = 3

func main():
    N := 5 # error: N is a constant, a variable cannot take its name
```
A function or a type can share its name with a variable: `twice := 4` leaves `twice(3)` to the function.

The type of a variable can also be inferred with `:=`
```flag
a := 3 # declares 'a' to be of type 'i64' and sets it to 3
b := 2.5 # 'b' is 'f64'
```
Assignments can do some transformations between types, for example:
```flag
view: &strview = "hello" # a text literal is a '&strview'
text: &str = heap(view) # heap copies the text into a new '&str' that you own
back: &strview = text # a '&str' can be used as a '&strview' without copying
text = "bye" # copies into the memory text has
small: u8 = 7
n: i64 = small # smaller integers grow to bigger ones
maybe: i64|null = a # a value goes into a union that has its type
l: &list[i64] = heap([1, 2]) # a list in the heap with a copy of the array
```
A new owner, like `text`, gets its memory from `heap` or from an [arena](#arenas), so `text: &str = view` is an error that says so: `this &str is new and has no memory to copy &strview into: write heap(...) or arena(a, ...) around the value`.
The other conversions are written like a call to the type
```flag
c := f64(a) / 2.0
d := u8(a)
```

## Literals
- Integers: `3`, `-3`, `0xff`, `0b101`, and `_` can separate the digits, `1_000_000`. They take the type they are used with, and alone they are `i64` (`u64` above the largest `i64`), so after `n := 5` the `n` is an `i64`: write `n: i32 = 5` for an `i32`. Where an integer and a float could take it, it goes to the integer
- Floats: `2.5`, `1e3`. Alone they are `f64`. `inf` and `nan` are written like `print` writes them, `low := -inf`, and a computation between finite literals that gives one of them, like `1.0 / 0.0`, is an error
- Characters: `'a'` is the number of that character (97), an integer literal, so it works with `u8`
- Text: `"hello\n"`, a text literal is a `&strview`. The escapes are `\n \t \r \0 \\ \" \' \e`, and `\x41` is the byte with that hexadecimal number. A text can cross lines, and its line breaks and the spaces at the start of its lines are part of it
- `true`, `false` and `null`
- Arrays: `[1, 2]` is an `array[i64, 2]`, and if the elements have different types the array holds their union, so `[1, "hola"]` is an `array[i64|&strview, 2]`. `[]` is the empty one, an `emptyarray`, that becomes an empty array or listview, so the place must say which: `x := []` alone has no type
- Maps: `{"ana": 30, "bo": 25}` is a `fixmap[&strview, i64, 2]` that holds just those, and `{}` is the empty one, an `emptyfixmap` (see [Maps](#maps))
- Structs: `{1, 2}` or `{x = 1}` build the struct that the place expects (see [Structured Types](#structured-types)). Where nothing expects one, `{1, "a"}` is a tuple of its values, a `(i64, &strview)` (see [Several values](#several-values))

No literal asks for memory: a list, a text or a map that grows is made from one with `heap` or `arena`, `heap([1, 2])`, `heap("hello")` or `heap({"ana": 30})`.

When you give the type, the literal takes it
```flag
a: array[u8, 3] = [1, 2, 3]
ages: fixmap[&strview, u8, 4] = {"ana": 30}
```
`ptr` passes the type on to what it points to, so an array literal or a constant can be a listview of another integer type
```flag
SHADER = [0x0723_0203, 0x0001_0000, 0x0008_000b]

func main():
    words: &listview[u32] = ptr(SHADER)
```
A text that crosses lines keeps them as they are
```flag
USAGE = "usage: flagc file.flg
  -check   only check"
```

## Constants
A constant is a name for a value, declared at the root of a file, outside the functions
```flag
MYCONST = "hello"
NUM = 2
MYSECONDCONST = 2 + 2 + NUM # the compiler computes 6
```
Arithmetic between literals is computed by the compiler, any other expression is written again where you use the constant, like a macro. A constant array or struct of literals, like `TABLE = [3, 1, 4, 1, 5]`, is kept once in the data of the program and every use reads it there (see [Constants](Reference.md#constants)).

A constant can have a type, written like a variable. Its value is one the compiler knows: literals, constants, and structs and arrays of them
```flag
LIMIT: u8 = 10
names: array[&strview, 3] = ["one", "two", "three"]
```
It can't change, what changes goes in a [context](#contexts). Nothing runs before `main`, so all that's written outside a function is known when the program compiles.

## Operators
- Arithmetic: `+`, `-`, `*`, `/`, and `%` for integers, and `+` and `-` move a `*bytes` (see [Pointers](#pointers))
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`. They don't chain, `a < b < c` is an error that tells you to write `a < b and b < c`
- Logic: `and`, `or` and `not` on `bool`. `and` and `or` don't run the right side when the left side already decides
- Bits: `and`, `or`, `xor` and `not` on integers work on the bits, and `<<` and `>>` shift them. `and`, `or` and `xor` are looser than the comparisons, so a test of bits goes in parentheses: `(flags and MASK) != 0`, and the shifts are tighter
- Choosing: `a if c else b` is `a` when `c` is true and `b` when it's not, and only the side it chose runs
- Assignments: `=`, and `+=`, `-=`, `*=`, `/=`, `%=`
```flag
flags := 0b1010 and 0b0110 # 0b0010
even := 0b1011 and not 1   # 0b1010
big := 1 << 20
if x > 0 and not done:
    count += 1
mode := "-check" if check else "run"
```
An expression runs left to right, and a variable is read in its turn: with `x = 1`, `x + bump(ptr(x))` is `1 + 11` even when `bump` changes `x`. An assignment runs its right side first, so `x += bump(ptr(x))` adds to the `x` that `bump` left, 22.

Two integers of different types mix when one holds every value of the other, and the operation is done in the wider one: `u8 + i32` is an `i32`. Otherwise, like `u64` with `i64` or an integer with a float, you convert one side (see [Variables and Assignments](#variables-and-assignments)), and a literal takes the type of the other side. Comparisons take any two integers and compare their values, so `i64(-1) < u64(0)` is `true`. A shift has the type of its left side. Integers wrap around when they overflow, and dividing by zero stops the program with an error.

`not` takes everything up to the next `and`, `or` or `xor`, so `not x == 3` is `not (x == 3)`. Before arithmetic it needs parentheses, `(not x) + 1`, because `not x + 1` could mean either one.

## Types
### Builtin types
- `bool`
- `i8`, `i16`, `i32`, `i64` and `u8`, `u16`, `u32`, `u64`. A length, a count or a position is an `i64`, and there is no `usize`
- `f32`, `f64`
- `bytes`, raw memory of unknown size, only used behind a pointer

`@sizeof(T)` gives the size of a type in bytes. A call with `@` is one the compiler answers while it compiles, like `@here()`, the place in the code as text.

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
You can build them with all the fields in order, or naming them. The fields you don't name start at zero
```flag
a := User{heap("ana"), 30} # a &str field owns its text, so heap gives it one
b := User{age = 30, name = heap("bo")}
```
or at their default value, when they have one. A fat pointer is not a value at zero (see [Fat pointers](#fat-pointers)), so `User{age = 30}` is an error, `User expects a value for name, &str starts without one: give it in the literal, or give the field a default`. A default is written every time a value is born, and it's one the compiler knows: literals and constants, or an empty owner, `heap([])`, `heap("")` or `heap({})`
```flag
type Window:
    title: &strview = "untitled"
    width: i32 = 640
    height: i32 = 480
    tabs: &list[&strview] = heap([])

func main():
    w: Window # untitled, 640 by 480
    big := Window{width = 1920, height = 1080} # untitled too
```
When the place already says the type, like a variable with its type, a return, a field or an element of an array, you can leave the name out
```flag
c: User = {heap("bo"), 25}
d: User = {age = 25, name = heap("cy")}
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
A field can share its memory with a field declared above it with `@`
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
Structured types are values, assigning them or passing them to a function copies them. `==` compares them field by field, unless you declare your own (see [Operator overloading](#operator-overloading))
### Aliases
An alias is another name for a type, the two names are the same type
```flag
type Id = u32

func main():
    user: Id = 7
    n: u32 = user
```
### Union Types
Union types are tagged unions and are separated by `|`, you give them a name with an alias
```flag
type Num = i32|f32
```
`T?` is a short way to write `T|null`
```flag
found: i64? = null
```
You check which type a union holds with `is`, and inside an `if` it narrows the type (see [If](#if)), or with the cases of a `switch` (see [Switch](#switch))
```flag
n: Num = 3
if n is i32:
    print("an integer ", n)
```
There are also untagged unions with `||` (but they are not recommended, they have no tag to check and are read with `as`)
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
    start: i64

func main():
    r: Ring[u8, 16]
```
### Reinterpreting with as
`as` reads the bytes of a value as another type of the same size, it does not convert. Numbers, pointers, structs and arrays read as each other
```flag
type Size:
    w: u32
    h: u32

func main():
    raw := f32(1.5) as u32 # the bits of the float
    key := Size{640, 480} as u64 # the 8 bytes of a Size as one number
    back := key as Size # and the Size again
    one := 0x3f800000 as f32 # a literal is the integer of that size, so this is 1.0
```
It can also take a type out of a union, without checking the tag: when the union holds another type it reads those bytes as the one you asked for, so check it with `is` first.

## Pointers
A pointer is written with `*` before the type. `ptr(x)` gives you the address of `x` and `.*` reads or writes what the pointer points to
```flag
x := 5
p := ptr(x) # 'p' is '*i64'
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
    free(h)
```
When a function takes `*T` and you pass a value of type `T`, and there is no overload that takes `T`, the compiler passes its address for you. Pass a variable when the function changes it: with any other value it changes a temporary. A temporary lives like a variable declared in that line, until its block ends, so `l: &listview[i32] = ptr([1, 2, 3])` is a listview you can keep using in that block. In a loop the next round writes it again, so keep what each round makes in memory of its own, and `-memory` warns of a pointer to it used after its block.

`null` is not a pointer, a pointer that can be missing is `*T?`.

A `*bytes` points to raw memory, the `void *` of C, and it's the pointer that moves: `p + 8` is the address 8 bytes after `p`, and `q - p` is the distance in bytes. Any other pointer goes through `as *bytes` first
```flag
words: array[i64, 3] = [10, 20, 30]
second := ptr(words) as *bytes + 8
print((second as *i64).*) # 20
```

## Fat pointers
A fat pointer is a pointer with extra data, in flag we indicate it with `&` before the type.
For example `&list[i32]` is a pointer to the elements of a list with the fields `len: i64`, `cap: i64` and `arena`, where its memory comes from
```flag
a: &list[i32]
```
The library has these fat pointers
- `&strview`, a view of text
- `&str`, text that you own and can grow
- `&listview[T]`, a view of elements
- `&list[T]`, a list that you own and can grow
- `&map[K, V]`, a hash map that you own

A fat pointer gets its memory before you use it: one you own from `heap` or `arena`, and a view from what it sees. `heap([])`, `heap("")` and `heap({})` are the empty ones in the heap, they ask for no memory until they grow
```flag
a: &list[i32] = heap([])
append(a, 1)
```
Using one that no path gave a value is an error, `a is used before it gets a value, and &list[i32] starts without one: a: &list[i32] = heap([])`, and one you own that grows without memory, because only some path gave it one, stops the program: `flag: this &list has no memory to grow in: give it heap([]) or arena(a, [])`. `-memory` warns about it before: `append cannot grow a, it may have no memory to grow in: give it heap([])`.
The ones you own (`&str`, `&list` and `&map`) must be released with `free` when you don't need them anymore, `defer` helps with that, or they live in an [arena](#arenas) that releases them all at once. A `&str` can be used where a `&strview` is expected, and a `&list` where a `&listview` is expected. The other way is a copy: `heap(v)` or `arena(a, v)` makes a new owner with the text or the elements of a view, and assigning a view to an owner that has memory copies it there, `t = v`. A call doesn't copy, so a view doesn't go to a `&str` or a `&list` parameter.

You can declare your own fat pointers with `type fatptr`, like the `&ring` of the module `ring`
```flag
type fatptr ring[$T]:
    len: i64
    cap: i64
    first: i64
```
### Passing them to functions
When you pass a fat pointer to a function, the function gets a copy of it. The elements are shared, but if the function makes the copy grow, the original does not see it.

To let a function change the fat pointer itself, declare the parameter as `*&something`. You still call it passing the fat pointer, the compiler passes `ptr(something)` for you like with any pointer (see [Pointers](#pointers)). Indexing and `for` work through the pointer, like fields
```flag
func add_two(l: *&list[i32]):
    append(l, 2)
    l[0] += 1

func main():
    nums: &list[i32] = heap([])
    defer free(nums)
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
color: Color = Red
if color is Red:
    pass # the type of color is Red here
elif color is Blue:
    pass # the type of color is Blue here
else:
    # the type of color is Green here
    color = get_random_color() # color goes back to Red|Green|Blue from here
```
To check several types at once use `is` with a union
```flag
if color is Red|Blue:
    pass # the type of color is Red|Blue here
```
`==` and `!=` with `null` or another void type narrow like `is`, so `if x != null:` works too. `not` turns a check around, so `if not color is Red:` narrows to `Green|Blue`. A branch that leaves, with `return`, `break`, `continue` or a call that never returns like `panic`, takes its types away after the `if`
```flag
data := read_file("config.txt")
if data is FsError:
    panic("cannot read the config: " + data)
print(data.len) # data is a &str here
```
`assert` narrows the same way, and when its condition is false it stops the program and says what was there (see [When the program fails](#when-the-program-fails))
```flag
data := read_file("config.txt")
assert data is &str
print(data.len)
```
A block cannot be empty, `pass` is the statement that does nothing
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
For lets you iterate over an iterable object
```flag
for element in iterable:
    print(element)

for index, element in iterable:
    print(index, " ", element)
```
To iterate over a range you can do (`..` leaves the end out and `..=` takes it). The `i` counts the loop, like the `index` above, so you can read it but not assign it
```flag
for i in 0..10: # from 0 to 9
    print(i)

for i in 1..=10: # from 1 to 10
    print(i)
```
Iterating a text gives you each character as a `&strview`, and iterating a map gives you each entry with `.key` and `.value`
```flag
for c in "hello":
    print(c)
```
The iterables can be reversed with reverse (depends on the iterable, ranges, arrays, lists, listviews and text can)
```flag
for element in reverse iterable:
    print(element)

for i in reverse 0..10: # from 9 to 0
    print(i)
```
With `for index, element in reverse list` the index is the place of the element in the list, so it goes down: 2, 1, 0 for a list of three. Only arrays, lists and listviews know that place, so over text or a type of yours `reverse` with an index is an error.

Over an array, a list or a listview the element is the one in the list, not a copy, so assigning it or a field of it changes the list
```flag
for p in players:
    p.score += 10 # every player of the list gets 10 more
for s in names:
    free(s) # frees each text, and the list keeps them empty
```
Where there is nothing to change it's an error that says what to write: a character of a text is a piece of it, and an entry of a map is a copy, `e is a copy that next gives, assigning it does not change ages: write ages[e.key] = v`. Growing the list in its own `for` moves the elements, so don't.

You can make your own types iterable. `for` calls `operator for` with your value and then `operator next` with a pointer to what it gave, until `operator next` gives `IterationEnd`. `reverse` calls `operator reverse` instead of `operator for`
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
Switch compares a value with each case, only the first case that matches runs. A case can have several values and ranges, and `case:` alone is the default
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
It also works with text. Over a union the cases name its types, and inside each case the variable has those types, like after an `is`. A switch over a union must handle every type or have a default, and the default gets the types that are left
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
        print("not a circle") # shape is Square|null here
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
    names: &list[&strview] = heap([])
    defer free(names)
    append(names, "ana")
```
A defer cannot have return, break, continue or try inside, and it cannot be a declaration or another defer. A deferred call that can fail must say what happens to the error, `defer _ = remove_file(path)` drops it on purpose.
### With
`with` calls `operator with` on the value when the block ends, also when you leave it early
```flag
type Door:
    room: i32

operator with(d: Door):
    print("closed ", d.room)

func main():
    with Door{7}:
        print("inside the room")
```
An `operator with` that can return an error doesn't go in a `with`, the error would be lost, so close it with a function of yours and call it yourself.

## Functions
```flag
func add(a: i32, b: i32) -> i32:
    return a + b
```
A call writes its `(` right after the name, `add(1, 2)`, and a keyword leaves a space before it, `not (a and b)`, `if (x) or (y)`. So `add (1, 2)` and `not(x)` are errors that tell you the good form.
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
The default is computed where you call the function, so `@here()` as a default gives the function the place of the call, that's how `panic` and the index errors know where they happened
```flag
func log(message: &strview, place: &strview = @here()):
    print(place, ": ", message)
```
A default can read the parameters before it, so one function takes the whole text or a part of it
```flag
func slice_of(text: &strview, from: i64 = 0, to: i64 = text.len) -> &strview:
    return text[from:to]
```
#### Extra arguments
The last parameter can gather the arguments after the others, of one type, and inside the function they are a `&listview`. A view that you already have goes there with `..`
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
The extra arguments of a C function are `...` at the end of its parameters (see [Calling C](#calling-c))
### Several values
A function returns several values in a tuple, a struct without names whose type is the types of its values: `(i32, i32)`. `return a, b` builds it, and the values come apart when you declare them or assign them
```flag
func split(text: &strview) -> (i64, i64):
    return 0, text.len

func main():
    lo, hi := split("hola") # 0 4
    whole := split("abc")
    print(whole, " ", whole.1) # (0, 3) 3
    a, b := 1, 2
    a, b = b, a # swaps them
    _, last := split("xy")
```
An error doesn't go inside a tuple, it goes beside it: `-> (i64, i64)|ParseError`, and `lo, hi := try parse(text)` takes the tuple apart after `try`
### Functions that never return
A function that never comes back, like `panic` or `exit`, says so with `-> never`. The code after a call to it can never run, so it narrows like a `return`, and on the right of `ifnull` or `iferror` it leaves the value without the `null` or the error
```flag
func fail(why: &strview) -> never:
    eprint("error: ", why)
    exit(1)

func main():
    port := parse_int("80") ifnull fail("not a number") # port is an i64
```
### Overloading
Functions can have the same name with different parameters, the call uses the one that fits best: the one that fits each argument at least as well as the others, and one better. An integer grows to a bigger parameter like in an assignment, and the exact type wins
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
You can overload `+ - * / % == < << >> and or xor` for your types, with a function of two parameters, and `not` with a function of one. `!=`, `>`, `<=` and `>=` come from `==` and `<`, and `a += b` uses `+`
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
`T(x)` converts like an assignment, also to a number: with `operator =(x: f64, c: Celsius) -> f64`, `f64(boiling)` is `100.0`
### Indexing
`x[i]` calls `operator [](x, i)` with the index as written, a negative one too, so your types can be indexed too. When it gives a pointer you can also assign to `x[i]`
```flag
type Grid:
    cells: array[i32, 9]

operator [](g: *Grid, i: i32) -> *i32:
    return ptr(g.cells[i])

func main():
    g: Grid
    g[4] = 7
```
The language calls other operators for its syntax too, like `operator for` for `for` and `operator with` for `with`, they are all in [Protocol operators](Reference.md#protocol-operators). An operator is not called by its name, so a type that you also close by hand keeps a function beside it, like `close(f)` of a `File`.
### Generics
A parameter with a `$` type takes any type, and `$$` takes a number
```flag
func biggest(a: $T, b: $T) -> $T:
    if a > b:
        return a
    return b

func size(a: array[$T, $$n]) -> i64:
    return $$n
```
Given integers of different types, a `$T` takes the widest: `biggest(a, b)` with an `i32` and an `i64` is an `i64`. A `$T` inside a type like `&list[$T]` is fixed by it, so `append(l, k)` on a `&list[i64]` takes a `k: i32`.
A `$T` that no argument gives comes from a parameter that takes a type, `type $T`. The call names the type, and the parameter takes nothing when the program runs
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
`where` chooses an overload at compile time, with conditions over the types and numbers of the call
```flag
func kind(v: $T) -> &strview where $T is u8|u16|u32:
    return "unsigned"

func kind(v: $T) -> &strview where $T is i32|i64:
    return "signed"

func wide(v: $T) -> bool where @sizeof($T) >= 8:
    return true
```
It can also ask for the platform with `@linux()`, `@macos()`, `@windows()`, `@x86_64()`, `@arm64()` and `@riscv64()`, and if a `$T` can be assigned to a `&str` with `@can_assign(&str, $T)`. Those are a `bool` in any expression too, `if @windows():`, but an `if` checks both branches on every system. `a if c else b` with a `c` the compiler knows is only the side it chose, so `19 if @linux() else 8` is a literal that goes to an `i32`.

A `where` at the start of a line opens a block that exists only where it holds. Inside a function it holds statements, so code that calls a C function of one system goes there, and at the root it holds declarations, only for a system or a processor
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

`@compile_error` stops the compilation with your message, at the call that chose that overload
```flag
func small(n: $T) -> i32 where @sizeof($T) > 4:
    @compile_error("small takes numbers of 4 bytes or less")
```

### Function values
The name of a function is a value. Its type is the declaration without the names, `func(i32) -> i32`, so you can keep a function in a variable, pass it and call it
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
A struct can hold one in a field, and you call it through the field
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
A generic function takes one with a shape, and `$T` comes from it
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
When a function has overloads or is generic, give its name to a variable with the type, that's what chooses: `show_int: func(i32) = show`. A function value that can be missing is `(func(i32) -> i32)?`, checked with `is` like any other union.

There are no closures: a function cannot use the variables of the place where you took its name, so pass what it needs as an argument (see [Function values](Reference.md#function-values) for the exact rules).

### Inline
The compiler replaces a call to a small function with the body of the function, so small helpers and operators cost nothing. `inline` asks for it at every call, whatever the size
```flag
type V:
    x: f64
    y: f64

inline operator +(a: V, b: V) -> V:
    return V{a.x + b.x, a.y + b.y}

inline func scale(v: V, k: f64) -> V:
    return V{v.x * k, v.y * k}
```
A recursive function, the `main` that starts the program and an `export` function cannot be `inline`, and a call through a function value always calls the function out of line. `noinline func` is never replaced by its body, so it keeps its own line in the chain of a runtime error and in a profiler. Small structs like `V` live in registers, so `acc = acc + scale(V{1.0, 2.0}, 0.5)` in a loop runs as fast as the same loop in C. The rules and the sizes are in [Inlining](Reference.md#inlining).

## Strings
There are two kinds of text: `&strview` is a view of some text, and `&str` is text that you own and can grow. A text literal is a `&strview`.
```flag
name: &str = heap("ana") # copies the text into a '&str' in the heap
defer free(name)
name += " and bo" # appends to the text you own
print(name.len, " ", name[0:3]) # 10 ana
```
A `&strview` assigned to a `&str` is copied into the memory the `&str` has, `name = "bo"`, while two `&str` share theirs.
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
Indexing a text `s[i]` gives you the byte as `u8`, and `s[a:b]` gives you a `&strview` of that part. `s[i] = b` changes a `&str`, and a `&strview` only reads: a text literal is in the read-only memory of the program, so to change one copy it to a `&str`. Negative indexes count from the end, so `s[-1]` is the last byte and `s[-3:-1]` are the two before it. Without its start a slice starts at 0, and without its end it goes to the end, so `s[2:]` drops the first two bytes and `s[-3:]` keeps the last three.

The library has `find`, `contains`, `split`, `join`, `replace`, `trim`, `parse_int` and more, they are in [Text](Library.md#text).

`@embed("logo.png")` puts a file in the program and gives it as a `&strview`, like a text literal. The compiler reads it when it builds the program, from the folder of the source file
```flag
FONT = @embed("fonts/mono.ttf")
```
### Printing
`print` and `eprint` (to the error output) take any number of values and print them one after the other. Structs, unions, arrays, lists and maps know how to print themselves. `fixed` prints a float with some decimals, `hex` an integer in hexadecimal, with zeros up to some digits if you want, and `pad` fills a width with spaces
```flag
print(fixed(3.14159, 2), " ", hex(255), " ", hex(10, 4), " [", pad(7, 3), "]") # 3.14 0xff 0x000a [  7]
```
`write(stdout, x)` writes one value without the new line. Each `print` writes its line at once, so two threads never mix their lines. The standard output keeps what you write and sends it in big pieces, so printing a lot is fast, and it sends what it keeps before the program waits, for input, a connection, another thread or `sleep`, so a line shows while the program waits. `print` and `eprint` keep the order of your calls, and nothing is lost when the program ends, calls `exit`, stops with an error like an index out of bounds or crashes. A signal like Ctrl-C loses what waits (see [Files](Library.md#files)).
To print your own type another way, overload `+=` for text
```flag
operator +=(s: *&str, p: Point):
    s.* += "(" + p.x + ", " + p.y + ")"
```

## Arrays
An array has a fixed size that is part of its type, it is a value like a struct, so assigning it copies it
```flag
a := [1, 2, 3] # array[i64, 3]
b: array[u8, 16] # starts at zero
c := array[i32, 2]{4, 5} # you can also name the type
a[0] = 10 # indexes are checked, going out stops the program with an error
print(a[-1], " ", a.len) # a negative index you write counts from the end, so this is 3 3
for x in a:
    print(x)
```
The functions of lists and listviews, like `sort`, take an array by its address
```flag
sort(ptr(a))
```
Two arrays of the same type compare with `==`, every element. An array holds values and has no arithmetic: numbers that compute together go in a `simd`.

## SIMD
`simd[T, n]` is `n` numbers in one register of 16 bytes or two, and its operators work on every lane at once
```flag
a: simd[f32, 4] = [1.0, 2.0, 3.0, 4.0]
b := simd[f32, 4](0.5) # the same number in every lane
c := a * b + 1.0 # a number beside a simd goes in every lane
print(c) # [1.5, 2.0, 2.5, 3.0]
c[0] = 9.0 # a lane, chosen with a constant
m := a > 2.0 # a mask, simd[bool32, 4], with lanes as wide as those of a
print(m, " ", select(m, a, 0.0), " ", any(m), " ", sum(a)) # [false, false, true, true] [0.0, 0.0, 3.0, 4.0] true 10.0
```
The lanes are `f32`, `i32` and `u32` by 4, `f64`, `i64` and `u64` by 2, `i16` and `u16` by 8, and `i8` and `u8` by 16, in 16 bytes, and twice as many in 32: `simd[f32, 8]`. A comparison gives a mask, `select` takes each lane from one side or the other, and `min`, `max`, `abs` and `sqrt` work on each lane. On x86_64 and arm64 an operation is one or a few instructions of the processor, two for 32 bytes, and the values stay in registers, so code on 4 lanes runs about 4 times as fast as on one number. More lanes are an array of them, `array[simd[f32, 8], 4]`. A `simd` reads from a list and goes back into it through a slice of its length, `v: simd[f32, 4] = values[i:i + 4]` and `values[i:i + 4] = v * 2.0`. The details are in [SIMD](Reference.md#simd).

## Lists
A `&list[T]` is a list that grows as you add elements. It gets its memory from `heap` or `arena`: `heap([])` is an empty one, and `heap` of an array or a listview makes one with a copy
```flag
empty: &list[i64] = heap([])
start := heap([1, 2, 3]) # a &list[i64] in the heap
bytes := heap([u8(1), 2]) # the literal says the type of its elements, not the place
```
```flag
nums: &list[i32] = heap([])
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
Only a negative index the compiler knows, a literal or a constant, counts from the end. One computed when the program runs is out of bounds like any other, so `nums[i - 1]` with `i` at 0 stops the program, and functions like `insert` and `remove` take their position from the start.

Also `find` (gives the index or null), `contains`, `flip` (turns the list around in place) and `binary_search` for a sorted list, see [Lists and listviews](Library.md#lists-and-listviews). Two lists or listviews compare with `==` element by element.

## Maps
A `&map[K, V]` is a hash map, you can start it empty or from a literal, which alone is a `fixmap` that holds just its pairs
```flag
ages := heap({"bo": 25}) # &map[&strview, i64]
defer free(ages)
ages["ana"] = 30 # the same as put(ages, "ana", 30)
age := ages["ana"] # i64|null, null when there is no ana
if age is i64:
    print("ana is ", age)
ages["cy"] += 1 # a key that is not there counts as zero, so this counts
for e in ages:
    print(e.key, " ", e.value)
```
Also `contains`, `remove` (gives the value or null) and `clear`. A `&map[&str, V]` owns its keys: a new key is copied, also from a `&strview` like `ports[line[:4]] = 80`, and `remove`, `clear` and `free` free them. `ptr(ages["ana"])` points at the value inside the map, adding the key at zero when it is missing, and it goes bad when the map grows, like a pointer into a list. A field or an element of a value changes in place, and a key that isn't there is added with a zero value: `players[name].score += 10`, `pairs[a][b] += 1`. The zero of a list, a text or a map has no memory, so give a new key one before it grows, `groups[k] = heap([])`, and then `append(groups[k], v)`. The keys can be numbers, text, `bool`, enums, pointers, arrays, structs and unions of them, or anything that has `==` and `operator hash`.

A `fixmap[K, V, N]` is a fixed map, it keeps up to N entries inside itself in the order you put them, so it never allocates and you don't free it. `put` gives `MapFull` when there is no room for a new key
```flag
colors: fixmap[&strview, u32, 4] = {"red": 0xff0000}
if put(colors, "green", 0x00ff00) is MapFull:
    print("no room")
print(colors["red"], " ", colors.len)
```

## Arenas
An `Arena` is memory that you release all at once. It gives out pieces of big blocks, so allocating is cheap, and `free` of the arena releases everything in it
```flag
type Enemy:
    hp: i32

func main():
    level: Arena # starts empty
    defer free(level) # releases all of it
    boss := arena(level, Enemy{hp = 100}) # a *Enemy in level, like heap(v)
    enemies: &list[Enemy] = arena(level, []) # an empty list that lives in level
    append(enemies, Enemy{hp = 10}) # grows in level
    name: &str = arena(level, "boss") # text that lives in level
    name += " final" # grows in level too
    counts := arena(level, {"orc": 2}) # a &map[&strview, i64] in level
    print(boss.hp, " ", enemies.len, " ", name, " ", counts)
```
A list, a text or a map that lives in an arena remembers it and grows there, and `free` of it leaves it empty and releases nothing, the arena releases its memory. Everything else stays the same: `heap` uses the heap, like always.

`clear(a)` releases everything and keeps the blocks to use them again, which is what the memory of one frame of a game wants
```flag
frame: Arena
defer free(frame)
while running:
    clear(frame)
    visible: &list[*Enemy] = arena(frame, []) # this frame only
```
The scratch memory of the temporaries is an arena too, the one `with scratch():` marks, and each thread has its own. An arena you make is not shared between [threads](#threads) without a `Lock`. The rules are in [Arenas](Reference.md#arenas).

## Contexts
A context is global state of each thread: the fields start at zero or at their default value, every function can use them, and each thread has its own. Nothing runs to give a field a value, so a fat pointer there has a default
```flag
context app:
    depth: u32
    level: i32 = 3
    names: &list[&strview] = heap([])

func deeper():
    app.depth += 1
```
A thread that `parallel` starts begins with its contexts at their start, or with a copy of the ones of the thread that starts it when they are `inherited`. What all the threads share is a `shared context`, one for the whole program, whose start the compiler writes into the program
```flag
inherited context log:
    level: i32 = 1

shared context stats:
    visits: Lock[i64]
```

## Threads
`parallel f(x)` runs the call in a new thread and gives a `Thread[T]`, where `T` is what `f` returns. `wait(t)` waits for the thread to end and gives its result. A computation split in pieces
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
    n := cores() # the processors of the system
    for k in 0..n:
        append(pieces, parallel sum(1000000 * k / n, 1000000 * (k + 1) / n))
    total: i64 = 0
    for p in pieces:
        total += wait(p)
    print(total)
```
The arguments are computed where you call and copied to the new thread, and the result can be anything a function returns, text you own, an error to handle or a temporary: `wait` takes the temporaries of the thread into the ones of the thread that waits, as if a call had made them. `done(t)` asks if the thread ended without waiting, like a game that loads its level while it keeps drawing
```flag
level := parallel load("castle.map")
while not done(level):
    draw_loading_screen()
start(try wait(level))
```
`parallel f(x)` alone on its line lets the thread run on its own, and `main` waits for it when it ends, before its deferred statements, so the thread can use what `main` keeps. A thread you keep must be read, by `wait` or `done`, and `-memory` warns about one that is never waited. A function that can fail can't run alone, its error would be lost.

Each thread has its own contexts and its own temporary memory, and a `shared context` is shared. A `Lock[T]` lets one thread at a time change the value it guards, its `value`, and `Lock[null]` guards code and no value
```flag
shared context stats:
    visits: Lock[i64]

func visit():
    with lock(stats.visits):
        stats.visits.value += 1
```
A number or a pointer that threads change at once can be an `atomic[T]`, without a lock. It's not a value: `get` reads it, and `set`, `add`, `swap` and `compare_swap` change it, each with the instructions of the processor, in line
```flag
shared context stats:
    served: atomic[i64]

func serve():
    add(stats.served, 1)
```
A `Channel[T, N]` passes values between threads, and waits when it's full or empty. It keeps the values inside itself, so it lives where you declare it
```flag
func worker(jobs: *Channel[&str, 16]):
    for name in jobs: # until the channel is closed
        print("working on ", name)
        free(name)

func main():
    jobs: Channel[&str, 16]
    workers: &list[Thread[null]] = heap([])
    defer free(workers)
    for k in 0..4:
        append(workers, parallel worker(jobs))
    for name in ["a.txt", "b.txt", "c.txt"]:
        send(jobs, heap(name)) # the worker frees what it gets, so it gets text it owns
    close(jobs)
    for w in workers:
        wait(w)
```
What a thread writes another one sees after `parallel`, `wait`, a `Lock`, a `Channel` or an atomic, and `done` only says that the thread ended: what it wrote is read after `wait`. `-memory` checks threads too: two threads that may use the same memory at once while one of them changes it, outside a lock that both hold and outside an atomic, a thread never waited, or waited twice, a pointer to a local variable that goes to a thread that outlives the function, and a temporary that its `with scratch():` may release while a thread still uses it. It also warns about a copy of a `Lock`, a `Channel` or an `Arena` in use: pass a pointer to it.
```flag
func add(count: *i64):
    count.* += 1

func main():
    count: i64
    a := parallel add(ptr(count))
    add(ptr(count)) # -memory: count is changed by a thread that may run at the same time as this
    wait(a)
```

## Error handling
### The error type
An error is a type declared with `type error`, and a function that can fail returns it in a union
```flag
type error NotFound
type error enum FileError = CannotOpen|CannotRead

func index_of(names: &list[&strview], name: &strview) -> i64|NotFound:
    for i, n in names:
        if n == name:
            return i
    return NotFound
```
An error can also carry data, it's declared with fields like a struct, and narrowing it gives you its fields
```flag
type error BadLine:
    line: i64
    why: &strview

func check(line: i64, text: &strview) -> BadLine?:
    if text.len > 80:
        return BadLine{line, "too long"}
    return null
```
You cannot forget an error, a statement that leaves an error without handling does not compile. You handle it with `is`, `switch`, `try`, `iferror` or `trust`, or you ignore it on purpose with `_ =`
```flag
got := index_of(names, "ana")
if got is i64:
    print("found at ", got)
_ = index_of(names, "bo")
problem := check(3, line)
if problem is BadLine:
    print("line ", problem.line, ": ", problem.why)
```
### try operator
`try` gives you the value, and if there is an error it returns it from the function, so the function must return that error too
```flag
func second(names: &list[&strview]) -> &strview|NotFound:
    i := try index_of(names, "bo")
    return names[i]
```
`try` passes only errors, a `null` goes up with `ifnull return null` (see [iferror and ifnull](#iferror-and-ifnull)). It takes everything after it, so the field of what it gives goes after parentheses: `(try get()).x`
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
On the right can also go `return`, `break` or `continue`, and then the value has no error or `null` after it. When the line is only `a ifnull b`, `b` can be an assignment
```flag
port := parse_int(text) ifnull return null
for text in texts:
    parse_int(text) ifnull bad += 1
    total += parse_int(text) ifnull continue
```
### When the program fails
Some errors stop the program, like an index out of bounds, a division by zero, `assert` or `panic`. They print what happened, where, and the functions of yours that led there
```
flag: index 5 is out of bounds, length 3
  --> main.flg:12:11
  in parse_line, main.flg:9
  in load, main.flg:20
  in main, main.flg:31
```
`assert` checks a condition and says what was there when it's false: `assert total == 4` stops with `flag: total == 4 is false, total is 3`, and `assert f is File` with `flag: f is FsError, not File: cannot open config.txt: No such file or directory`. A message after a comma replaces that, `assert total > 0, "the total cannot be empty"`.

To log the failure or save something before the end, give `on_panic` a function: it gets the message and the place of every one of these errors, after they are printed (see [Runtime errors](Reference.md#runtime-errors)).

A crash prints the same chain: a null pointer is `flag: invalid memory access at 0x0`, and a recursion without end is `flag: stack overflow`. An error in a [thread](#threads) shows the functions of that thread, and a small function that the compiler put inside its caller (see [Inline](#inline)) has no line of its own, unless it's `noinline`.

To look inside the program while it runs, build it with `-debug` and run it in gdb or lldb, they show your variables with their types and stop at the lines you choose (see [Debugging](Reference.md#debugging)). With `-debug` nothing is inlined, so every function has its line.

## Modules
When you compile you give the compiler a list of files or folders, compiling a folder is equivalent to compiling every `.flg` file inside the folder, including the files inside other folders. The code of all the files is compiled together, so all the files have access to all the functions, types and constants in the other files.
To restrict access there are modules. A module is declared at the start of the file, and means that all the things in the file will be available only to the files of the module and to the files that use it
```flag
module mymodulename
type Hello
```
To give access to a module from another file you use `use`.
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
In a file without a module, priv hides the name from every other file.

A package, code of someone else or a part of your program that lives apart, is a folder that you use with its path in quotes, from the folder of the file. The last folder names the module, and one of its files says `module vec`. flagc loads the `.flg` files of that folder, not those of the folders inside it, so `flagc run app/main.flg` is enough. flagc never downloads anything: a package is a folder that you copy, or a git submodule
```flag
use "vendor/vec"
func main():
    a := vec.V2{3.0, 4.0}
    print(dot(a, a))
```
Two modules that you use can have a function with the same name, the arguments choose which one runs, and when both fit the same the error says to name the module: `ambiguous call to size, several overloads match (i32) equally: name the module, a.size or b.size`. A module of yours with the name of one of the library, like your own `math`, is the one your files see, and the library keeps using its own.

A type brings its functions: a call sees the functions of the modules that declare the types of its arguments, and the types inside them, like `b.Door` in `a.Table[b.Door]`, without `use`. So a generic function of one module can call a function declared next to the type it was made for.

## The standard library
The functions of text, lists, maps, numbers (`min`, `max`, `abs`...), files (`read_line`, `write`...) and threads (`Lock`, `Channel`, and `get`, `add`... of an `atomic`) are always there. The rest comes in modules that you get with `use`, without adding files, and a name of a module that a file uses without its `use` is an error that names it: `sqrt is in the module math: add use math`
- `fs`, opening files, whole files, folders and paths
- `os`, the arguments, the environment, other programs, libraries, the processors and the time
- `json`, reading and writing JSON
- `net`, TCP connections and UDP
- `math`, `sqrt`, `sin`, `PI`, random numbers...
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
    text := read_file(names[1])
    if text is FsError:
        eprint(text) # cannot open notes.txt: No such file or directory
        return 1
    defer free(text)
    lines := 0
    for b in items(text):
        if b == '\n':
            lines += 1
    print(lines, " lines")
    return 0
```
A file can also be read line by line, with `open_file` and `read_line`. Each line is temporary, so the loop releases it with `with scratch():`, and a file of any size runs in the same memory
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
Every function is in the [Library](Library.md).

## Calling C
You declare C functions inside an `extern` block with the library they come from, `"c"` is the C library. A block can have a `where` on the system, and then its library is linked and its functions exist only on the systems where it holds
```flag
extern "c":
    func abs(x: i32) -> i32
    func snprintf(into: *u8, size: i64, format: cstr, ...) -> i32

extern "m":
    func sqrt(x: f64) -> f64

extern "ws2_32" where @windows():
    func WSAStartup(version: u16, into: *bytes) -> i32
```
A C string has the type `cstr`, a pointer to a text that ends with a zero, and a text literal goes to a `cstr` by itself. Other text needs the function `cstr`, a copy that lives in the temporary memory like the results of `+`, or `c_text` for a `&str`, and `view` turns a `cstr` back into a `&strview`. `print` of a `cstr` shows its text. Structs are passed like C does.
```flag
extern "c":
    func strlen(s: cstr) -> i64

func main():
    name: cstr = cstr("hola")
    print(strlen(name), " ", strlen("abc"), " ", view(name))
```
A function can have another name in Flag than in C, after `=`, and so one C function can be declared once for each signature you call it with
```flag
extern "objc":
    func send_id(self: *bytes, sel: *bytes) -> *bytes = "objc_msgSend"
    func send_f64(self: *bytes, sel: *bytes) -> f64 = "objc_msgSend"
```
A variable of C goes in by its address, a constant that points to it, and the text of the block can name a framework of macOS or a file
```flag
extern "c":
    index: *i32 = "optind" # index.* = 1 writes optind

extern "Metal.framework" where @macos():

extern "libfreetype.so.6" where @linux():
```
`export func` makes a function that C can call, with the plain name and the C calling convention. Its name is a value of type `extern func(...)`, the function type of C, and that's how you pass callbacks to C
```flag
extern "c":
    func qsort(base: *bytes, count: i64, size: i64, compare: extern func(*i32, *i32) -> i32)

export func ascending(a: *i32, b: *i32) -> i32:
    return a.* - b.*

func main():
    values: array[i32, 3] = [3, 1, 2] # the size C reads, 4 bytes
    qsort(ptr(values) as *bytes, 3, 4, ascending)
```
An `extern func` is called like C calls, structs too, so it's also what a table of functions of C holds, and what an address that C gives you becomes with `as`: `f := address as extern func(f64) -> f64`. The function values of Flag, `func(...)`, don't cross to C.

`@offset_of(T, field)` gives the place of a field, like `@sizeof` gives the size, both known when the program compiles.

A library that a program loads is built with `-shared`, and the program loads it with `load_library` and `symbol` of [os](Library.md#libraries). That's how a game reloads its code while it runs: see [Libraries](Reference.md#libraries).

## Memory safety
### The flag
Compiling with `-memory` checks the memory of your program at compile time. Outside `@trust`, a program that it passes without warnings doesn't lose memory, doesn't free it twice or free what is not on the heap, doesn't use it after it was freed, and has no two threads that use the same memory at once while one of them changes it: its border is `trust`, `as`, moving a `*bytes`, a narrowed variable changed through a pointer and what C does (see [The memory checker](Reference.md#the-memory-checker)). It warns about:
- memory that is never freed
- memory freed twice
- memory used after it was freed or after a list grew and moved it
- temporaries used after their `with scratch():` ended, and memory of an arena used after `free` or `clear` of it
- `free` of a pointer that lives in an arena, and an arena that is never freed
- pointers to the stack of a function, or of a block, used after it ended
- an owner that may grow without memory, because only some path gave it one
- two threads that use the same memory at once while one of them changes it, outside a lock that both hold

It is optional because it makes the compilation slower.
```sh
flagc main.flg -o main -memory
```
It follows the elements of a list, an array or a map as none, some or all freed, so freeing them in a loop and then the list is clean, and freeing only some of them warns (see [Elements](Reference.md#elements)). It follows the member a union holds, so `if result is Bad: return` and then freeing what the other member holds is clean.
### @trust @check
`@trust` hides the warnings of the code you already checked. At the end of a line it covers that line and the block it opens, and on a line of its own it covers everything after it at that indentation, so at the top of a file it covers all the file
```flag
func risky(p: *i64): @trust
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
- `-shared` builds a library instead of a program (see [Libraries](Reference.md#libraries))
- `-check` only checks the code, without building the program
- `-memory` checks the memory (see [Memory safety](#memory-safety))
- `-debug` builds the program for gdb and lldb (see [Debugging](Reference.md#debugging))
- `-target` builds for another system, like `-target windows-x86_64` (see [Platforms](Reference.md#platforms))
- `-cc` passes what goes after it to the C compiler, that only assembles and links the program, for example `-cc -lm` to link a library or `-cc -static`

The rest of the flags are in [The command line](Reference.md#the-command-line).

### Tests
`flagc test` builds your files and runs their tests: every `func test_name()`. Each test runs in a process of its own, at the same time as the others, so a failed `assert`, a crash or a test that never ends doesn't stop the rest
```flag
func test_gcd():
    assert gcd(12, 18) == 6
```
```sh
flagc test src -- gcd
```
What goes after `--` chooses the tests: the ones that have one of those words in their name. A test that takes more than 10 seconds is stopped, `-timeout` changes that. At the end it says which tests failed, where, and what they wrote, and flagc exits with 1 when one failed
```
FAIL test_gcd, src/math.flg:3, exit code 134
flag: gcd(12, 18) == 6 is false, gcd(12, 18) is 4
  --> src/math.flg:4:5
  in test_gcd, src/math.flg:3
0 passed, 1 failed
```

The C compiler is `cc`, if yours has another name put it in the `CC` variable
```sh
CC=gcc flagc main.flg
```
