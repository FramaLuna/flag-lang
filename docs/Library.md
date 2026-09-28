Every function of the standard library, module by module.

`core` is always there, without `use`. The other modules come with `use name`, flagc finds them in its library and you don't add any file.

- [How to read this](#how-to-read-this)
- [core](#core)
- [fs](#fs), files and folders
- [os](#os), the program, other programs and the time
- [json](#json)
- [net](#net), TCP sockets
- [math](#math)
- [utf8](#utf8)
- [bits](#bits), sets of numbers as bits
- [ring](#ring), a queue you can grow at both ends
- [raylib](#raylib), graphics, input and audio

## How to read this
Each function is written with its parameters and what it returns, and `$I` is any integer type. Functions with the same name and different parameters are overloads, the call takes the one that fits.

What a function gives you is
- **yours** when you own it and release it with `free`
- **temporary** when it lives in the scratch memory, until the `with scratch():` around the call ends (see [Temporary memory](Reference.md#temporary-memory)). Copy it to a `&str` to keep it
- **a view** when it points inside what you passed, so it lives as long as that

A function that can fail gives its errors in a union, and each module says which error types it has.

The functions that take an index also have a last parameter `place: &strview = @here()` that is left out here, it's how an index error points at your call.

## core
### The program
- `print(values...)` writes the values one after the other to the standard output, and then a new line
- `eprint(values...)` the same to the error output
- `write(fd: i32, text: &strview)` writes the text to a file descriptor, 1 is the standard output and 2 the error output
- `panic(message: &strview)` writes `flag: <message>` and the place of the call to the error output, and ends the program with exit code 134
- `assert(condition: bool, message: &strview = "assertion failed")` panics with the message when the condition is false
- `exit(code: i32)` ends the program with that exit code, the deferred statements don't run
- `arg_count() -> usize` how many arguments the program got, the first one is the program
- `arg(i: usize) -> &strview` the argument `i`, a view. It's not checked, keep `i` below `arg_count()`

### Memory
- `heap(v: $T) -> *$T` copies the value to the heap, yours, you release it with `free(p as *bytes)`
- `ptr(x)` the address of a variable, a field or an element, and of an `export func` (see [Calling C](Overview.md#calling-c))
- `sizeof(T)` the size of a type in bytes, as an integer literal
- `malloc`, `calloc`, `realloc`, `free(at: *bytes)`, `memcpy`, `memcmp`, `strlen`, `write`, `snprintf` and `strtod` are the C functions, with `*bytes` for `void *` and `cstr` for `char *`

### Temporary memory
- `scratch() -> Scratch` marks the scratch memory, and `close` of the mark releases every temporary made after it. `with scratch():` does both
- `temp(size: usize) -> *bytes` that many bytes of temporary memory
- `temp(text: &strview) -> &strview` a temporary copy of the text, with a zero byte after it
- `temp(items: &slice[$T]) -> &slice[$T]` a temporary copy of the elements, to return what you built in a list that you free

### Lists and slices
A `&slice[$T]` is a view of elements, and a `&list[$T]` is a list that you own and that grows. Both start empty, and a list goes wherever a slice is asked.
- `s[i]` the element `i`, you can also assign to it. A negative `i` counts from the end, and out of bounds stops the program
- `s[a:b]` the elements from `a` to `b - 1`, a view
- `for x in s`, `for i, x in s` and `for x in reverse s`
- `==` compares element by element, and `print` writes `[1, 2, 3]`

On lists
- `append(l: *&list[$T], v: $T)` adds `v` at the end. The first time it makes room for 8 elements, and then it doubles
- `insert(l: *&list[$T], at: $I, v: $T)` puts `v` at the index `at` and moves the rest one place forward, `at` can be `l.len`
- `pop(l: *&list[$T]) -> $T|null` takes out the last element, `null` when the list is empty
- `remove(l: *&list[$T], at: $I) -> $T` takes out the element at `at` and moves the rest one place back
- `free(l: *&list[$T])` releases the list and leaves it empty. The elements are not released, free them first when they own memory

On slices, so also on lists
- `find(items: &slice[$T], v: $T) -> usize|null` the index of the first element `== v`
- `contains(items: &slice[$T], v: $T) -> bool`
- `binary_search(items: &slice[$T], v: $T) -> usize|null` the index of `v` in a sorted slice, using `<`
- `sort(s: &slice[$T])` sorts in place using `<`, in O(n log n). It's a merge sort, so equal elements keep their order, and it allocates room for half the slice while it sorts
- `reverse(items: &slice[$T])` turns the elements around in place
- `swap(s: &slice[$T], i: usize, j: usize)` swaps two elements

An array goes to these functions by its address: `sort(ptr(values))`.

### Arrays
Two arrays of the same type work with the operators element by element, and give a new array
- `+`, `-`, `*` and `/` for numbers, and `%`, `and`, `or`, `xor`, `<<` and `>>` for integers: `[1, 2] + [10, 20]` is `[11, 22]`
- `a == b` and `a != b` compare every element

### Text
A `&strview` is a view of text and a `&str` is text that you own and that grows. A text literal is a `&strview`, and a `&str` goes wherever a `&strview` is asked. Text is bytes, and the library reads it as UTF-8.
- `s[i] -> u8` the byte `i`, a negative `i` counts from the end
- `s[a:b] -> &strview` the bytes from `a` to `b - 1`, a view
- `for c in s` gives each UTF-8 character as a `&strview`, also with `reverse`
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
- `contains(s: &strview, part: &strview) -> bool`
- `starts_with(s: &strview, head: &strview) -> bool` and `ends_with(s: &strview, tail: &strview) -> bool`
- `trim(s: &strview) -> &strview` without the spaces, tabs, `\n` and `\r` at both ends, a view. `trim_start` and `trim_end` do one end
- `split(s: &strview, separator: &strview) -> &slice[&strview]` the parts between the separators, as views of `s` in a temporary slice. There is always one part at least, and an empty separator gives `s` whole
- `replace(s: &strview, old: &strview, new: &strview) -> &strview` every `old` changed to `new`, temporary
- `join(parts: &slice[$T], separator: &strview) -> &strview` the parts with the separator between them, temporary. The parts can be anything that can be printed
- `lower(s: &strview) -> &strview` and `upper(s: &strview) -> &strview` change the ASCII letters, temporary
- `items(s: &strview) -> &slice[u8]` the bytes, a view

Numbers from text, `null` when the text is not a number
- `parse_int(s: &strview) -> i64|null` decimal digits with an optional `+` or `-`, and `null` when it doesn't fit in `i64`
- `parse_float(s: &strview) -> f64|null` what C's `strtod` reads: `1.5`, `-2e3`, `inf`, `nan`
- `parse_hex(s: &strview) -> u64|null` 1 to 16 hexadecimal digits, without `0x`

None of them skip spaces, the whole text must be the number.

### Printing and formatting
`print`, `eprint`, `+` and `+=` on text format each value with `operator +=(s: *&str, x: T)`, and the library has one for
- integers, in decimal, and `bool`, as `true` or `false`
- floats, with the fewest digits that read back as the same number (`0.1`, `2.0`), and with an exponent when they are very big or very small (`1e+16`, `1e-05`)
- pointers, in hexadecimal
- slices and lists `[1, 2]`, maps and fixed maps `{a: 1, b: 2}`

And two helpers
- `fixed(value: f64, places: i32) -> Fixed`, also for `f32`, prints the value with that many decimals, and the halves round to even: `fixed(2.5, 0)` is `2`
- `hex(value: u64) -> Hex` prints `0xff`

The compiler prints the rest: structs as `Point{x = 1, y = 2}`, arrays as `[1, 2]`, a union as the value it holds and a void type as its name. Declare `operator +=` to print your type another way (see [Printing](Overview.md#printing)).

### Maps
A `&map[$K, $V]` is a hash map that you own. It starts empty, and `{"a": 1}` makes one.
- `m[k] -> $V|null` is `get`, and `m[k] = v` is `put`
- `put(m: *&map[$K, $V], key: $K, value: $V)` adds the key, or changes its value
- `get(m: &map[$K, $V], key: $K) -> $V|null`
- `contains(m: &map[$K, $V], key: $K) -> bool`
- `remove(m: *&map[$K, $V], key: $K) -> $V|null` takes out the key and gives its value, `null` when it wasn't there
- `clear(m: *&map[$K, $V])` removes every key and keeps the memory
- `free(m: *&map[$K, $V])` releases the map and leaves it empty. The keys and values are not released, free them first when they own memory
- `for e in m` gives each entry, with `e.key` and `e.value`, in the order of the table
- `hash(key) -> u64` the hash of a key. There is one for integers, one for floats (`0.0` and `-0.0` are the same key), one for text and one for any other type that reads its bytes

A key needs `==` and `hash`. The hash of the bytes is right for `bool`, enums and pointers. A struct needs your `==`, and then declare its `hash` too, so that two keys that are `==` give the same hash.

### Fixed maps
A `fixmap[$K, $V, $$max]` keeps up to `max` entries inside itself, in the order they were put, so it doesn't allocate and you don't free it. Looking for a key reads the entries one by one, so it's for small maps.
- `put(m: *fixmap[$K, $V, $$max], key: $K, value: $V) -> MapFull?` adds the key or changes its value, and gives the error `MapFull` when there is no room for a new key
- `get`, `m[k]`, `contains` and `clear` like a map
- `remove` like a map, and the entries after it keep their order
- `for e in m` gives the entries in the order they were put

### C text
- `cstr` is the type `*u8`, a text that ends with a zero byte
- `cstr(text: &strview) -> cstr` a copy with the zero, temporary
- `c_text(s: *&str) -> cstr` writes a zero after the text of `s` and gives its address, without copying, so it's valid until `s` changes
- `view(c: cstr) -> &strview` a view of a C text, empty for a null pointer

## fs
Files and folders.
```flag
type error enum FsError = CannotOpen|CannotRead|CannotWrite|CannotRemove|CannotCreate|CannotRename
```
The files are read and written in binary, the bytes are the same on every system. On Windows the paths can use `/` or `\`.

### Whole files
- `read_text_file(path: &strview) -> &str|FsError` all the file, yours, or `CannotOpen` or `CannotRead`
- `write_file(path: &strview, text: &strview) -> usize|FsError` makes the file, or empties it, writes the text and gives how many bytes it wrote, or `CannotOpen` or `CannotWrite`. With `from: &slice[u8]` it writes bytes
- `append_file(path: &strview, text: &strview) -> usize|FsError` the same, but it writes at the end of the file and makes it when it doesn't exist

### Files by parts
A `File` is an open file, close it with `close` or with `with file:`.
- `open(path: &strview) -> File|CannotOpen` opens the file to read
- `create(path: &strview) -> File|CannotCreate` makes the file, or empties it, to write
- `read(f: File, into: *&str, size: usize) -> usize|CannotRead` appends up to `size` bytes to `into` and gives how many, 0 at the end of the file
- `read_line(f: File) -> &strview|null` the next line without the `\n` or `\r\n`, temporary, `null` at the end of the file
- `write(f: File, text: &strview) -> CannotWrite?`
- `close(f: File)`

### Folders and paths
- `exists(path: &strview) -> bool` if there is a file or a folder there
- `is_dir(path: &strview) -> bool`
- `file_size(path: &strview) -> usize|FsError` in bytes, or `CannotOpen` or `CannotRead`
- `list_dir(path: &strview) -> &slice[&strview]|CannotOpen` the names inside the folder, sorted and without `.` and `..`, temporary
- `walk(folder: &strview) -> &slice[&strview]|CannotOpen` the paths of every file inside the folder and the folders inside it, sorted, temporary
- `make_dir(path: &strview) -> CannotCreate?` makes the folder and the ones above it, and a folder that already exists is not an error
- `remove(path: &strview) -> CannotRemove?` removes a file or an empty folder
- `rename(from: &strview, to: &strview) -> CannotRename?` renames or moves
- `join(first: &strview, second: &strview) -> &strview` the two paths with one `/` between them, temporary
- `base_name(path: &strview) -> &strview` the part after the last `/`, a view: `base_name("docs/a.txt")` is `a.txt`
- `extension(path: &strview) -> &strview` the name from its last `.`, a view: `.txt`. It's empty when there is no `.`, and a name that starts with `.` has no extension
- `dir_of(path: &strview) -> &strview` the part up to the last `/`, with it, a view: `docs/`. It's `./` when there is no folder

## os
The program, other programs and the time.

### The program
- `args() -> &slice[&strview]` the arguments of the program, the first one is the program itself. The slice is temporary, the texts are views that live all the program
- `env(name: &strview) -> &strview|null` the value of an environment variable, `null` when it's not set
- `executable() -> &strview` the full path of the running program, temporary
- `current_dir() -> &strview` the current folder, temporary
- `change_dir(path: &strview) -> bool` `true` when it worked
- `system_name() -> &strview` `Linux`, `Darwin` or `Windows`
- `read_line() -> &strview|null` the next line of the standard input without the `\n` or `\r\n`, temporary, `null` at the end

On every system the paths that `os` gives use `/`.

### Other programs
- `run(command: &strview) -> i32` runs the command with the shell (`sh`, or `cmd.exe` on Windows) and gives its exit code
- `run(program: &list[&strview]) -> i32` runs `program[0]` with the other elements as its arguments, without a shell, so each argument gets there exactly as it is. The program is found in `PATH`
- `output(command: &strview) -> &strview` runs the command with the shell and gives what it wrote to the standard output, temporary
- `output(program: &list[&strview]) -> &strview` the same without a shell

On Linux and macOS a program killed by a signal gives 128 plus the signal, and one that cannot start gives 127.

### Time
- `now() -> f64` the seconds since 1970, with decimals
- `clock() -> f64` the seconds from some start, it never goes back, so it's the one to measure how long something takes
- `date() -> Date` the local date and time
- `sleep(seconds: f64)` stops the program that long

`Date` has `year`, `month` (1 to 12), `day` (1 to 31), `hour`, `minute`, `second` and `weekday` (0 is Sunday), all of them `i32`.

## json
Reads JSON, and writes its strings.
```flag
use json

func main():
    d := parse("{\"user\": {\"name\": \"ana\", \"tags\": [\"a\", \"b\"]}}")
    if d is NotJson:
        return
    defer free(d)
    print(text(d, get(d, 0, "user.name"))) # ana
    for tag in children(d, get(d, 0, "user.tags")):
        print(text(d, tag))
    print(integer(d, get(d, 0, "user.age")) ifnull 0) # 0, there is no age
```
`parse` doesn't copy the text, a `Document` is a list of nodes over it, so the text must live while you use the document. A node is its index in that list, the first value of the text is the node 0, and the functions take the node as `usize|null`. A missing node is `null`, and for it the functions give `null` or empty, so you can chain them and check only at the end.
- `parse(text: &strview) -> Document|NotJson`
- `free(d: Document)` releases the nodes, not the text
- `get(d: Document, at: usize|null, path: &strview) -> usize|null` the value of a key of the object `at`, and `.` goes into the objects inside it: `"user.name"`
- `children(d: Document, at: usize|null)` for a `for`: the values of an array or an object, in order, and nothing for anything else
- `count(d: Document, at: usize|null) -> usize` how many values an array or an object has
- `item(d: Document, at: usize|null, index: usize) -> usize|null` the value at `index` of an array or an object. It walks from the start, so to go through all of them use `children`
- `key(d: Document, at: usize|null, index: usize) -> &strview` the key at `index` of an object, temporary
- `kind(d: Document, at: usize|null) -> Kind` what the value is, one of `Nothing`, `Boolean`, `Number`, `Text`, `Array` and `Object`. `null` and a missing node are `Nothing`
- `text(d: Document, at: usize|null) -> &strview` a string with its escapes decoded, temporary, and empty when it's not a string
- `number(d: Document, at: usize|null) -> f64|null`, `integer(d: Document, at: usize|null) -> i64|null` and `boolean(d: Document, at: usize|null) -> bool|null` give `null` when the value is not of that kind. `integer` is also `null` for a number with decimals or an exponent
- `raw(d: Document, at: usize|null) -> &strview` the JSON text of the value, a view
- `quoted(out: *&str, text: &strview)` appends the text to `out` as a JSON string, with the quotes and the escapes

To write JSON you build the text with `+=`, and `quoted` for the strings.

## net
TCP sockets.
```flag
type error enum NetError = CannotResolve|CannotConnect|CannotListen|CannotAccept|CannotSend|CannotReceive
```
A `Socket` is a connection or a server, close it with `close` or with `with socket:`.
- `connect(host: &strview, port: u16) -> Socket|NetError` connects to a name or an address. `CannotResolve` when the name has no address, and `CannotConnect` when no address answers
- `listen(port: u16) -> Socket|NetError` a server on that port, on every IPv4 address of the machine. With the port 0 the system chooses a free one, and `port` tells you which
- `accept(server: Socket) -> Socket|NetError` waits for the next connection
- `send(s: Socket, text: &strview) -> CannotSend?` sends all the text
- `receive(s: Socket) -> &str|CannotReceive` what arrived in one read, up to 64 KiB, yours. It's empty when the other side closed. A message can arrive in several reads, so read until you have all of it
- `close(s: Socket)`
- `port(s: Socket) -> u16` the local port of the socket

Sending to a socket that the other side closed gives `CannotSend`, it doesn't stop the program. On Windows the module uses Winsock and starts it for you.

## math
It links the C math library.
- `PI`, `TAU` and `E`
- `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `log2`, `log10`, `floor`, `ceil`, `round` and `trunc` take one float, and `atan2(y, x)`, `pow(x, y)`, `hypot(x, y)` and `fmod(x, y)` take two. They are the C functions for `f64`, and each one has an `f32` version that gives an `f32`. The angles are in radians, `log` is the natural one and `round` takes the halves away from zero
- `abs(x: $T) -> $T` for signed integers and floats
- `min(a: $T, b: $T) -> $T`, `max(a: $T, b: $T) -> $T` and `clamp(x: $T, low: $T, high: $T) -> $T`, for anything with `<`
- `lerp(a: $T, b: $T, t: $T) -> $T` for floats, `a` when `t` is 0 and `b` when it's 1

### Random numbers
They are the same sequence on every run until you call `seed`, and they are not for secrets.
- `seed(n: u64)` starts the sequence from `n`. For a different one each run use the time: `seed(u64(now() * 1000000.0))`, with `now` from `os`
- `random() -> f64` from 0 to 1, without the 1
- `random(low: i64, high: i64) -> i64` from `low` to `high`, both of them included

## utf8
- `code(s: &strview) -> u32|null` the code of the first character, `null` when the text is empty or doesn't start with valid UTF-8: `code("é")` is 233
- `count(s: &strview) -> usize` how many characters, the same that `for c in s` counts
- `valid(s: &strview) -> bool` if all the text is valid UTF-8

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
- `b[i] -> bool` if `i` is in the set, and `b[i] = true` or `false` adds or removes it. A number past the room stops the program
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
