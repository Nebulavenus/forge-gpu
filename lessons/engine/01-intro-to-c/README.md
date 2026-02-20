# Engine Lesson 01 — Intro to C with SDL

Learn the fundamentals of the C programming language using SDL's cross-platform
APIs for output and memory management.

## What you'll learn

- C's basic types (`int`, `float`, `double`, `char`, `bool`) and what `sizeof` reports for each
- Formatted output with `SDL_Log` and why we use it instead of `printf`
- Arithmetic operators and explicit type casting between `int` and `float`
- Control flow: `if`/`else`, `for` loops, `while` loops, and `switch` statements
- Declaring and calling functions, forward declarations, and pass-by-value semantics
- Arrays (zero-indexed, no bounds checking) and C strings (null-terminated `char` arrays)
- Pointers: address-of (`&`), dereference (`*`), pointer arithmetic, and `NULL`
- Structs: grouping related data with `typedef struct` and accessing members with `.` and `->`
- Alignment and padding: why the compiler inserts invisible bytes inside structs, how `offsetof` reveals the true layout, and how member order affects memory usage
- Dynamic memory with `SDL_malloc`, `SDL_calloc`, `SDL_free`, and `SDL_memcpy` — and why we prefer SDL's memory functions over the C standard library versions
- Undefined behavior: what it means, why the compiler's assumptions make it dangerous, and how to defend against it

## Why this matters

Every GPU lesson in this series is written in C. Before you can create a window,
upload vertices, or write a shader, you need to be comfortable with the language
those lessons are built on: variables, functions, pointers, structs, and memory
allocation.

This lesson walks through each of those fundamentals in a single program. It
uses SDL's APIs for logging and memory wherever they improve portability or
safety — and explains why those SDL functions exist alongside the C standard
library equivalents. By the end, you will be able to read and modify the code in
every GPU lesson without guessing what the C language constructs mean.

## Result

The example program prints a guided tour of C fundamentals. Each section
demonstrates a concept with output you can inspect.

**Example output:**

```text
INFO: === Engine Lesson 01: Intro to C with SDL ===
INFO:
INFO: --- 1. Types and Variables ---
INFO:   int:    lives = 3
INFO:   float:  speed = 5.5
INFO:   double: pi    = 3.141592653589790
INFO:   char:   grade = A
INFO:   bool:   running = true
INFO:
INFO:   Type sizes on this platform:
INFO:     sizeof(char)   = 1 byte
INFO:     sizeof(int)    = 4 bytes
INFO:     sizeof(float)  = 4 bytes
INFO:     sizeof(double) = 8 bytes
INFO:     sizeof(bool)   = 1 byte
INFO:
INFO: --- 2. Arithmetic and Type Casting ---
INFO:   10 + 3 = 13
INFO:   10 - 3 = 7
INFO:   10 * 3 = 30
INFO:   10 / 3 = 3  (integer division truncates)
INFO:   10 % 3 = 1 (remainder)
INFO:   (float)10 / (float)3 = 3.3333  (float division)
INFO:   (int)3.7f = 3  (truncated, not rounded)
INFO:
INFO: --- 3. Control Flow ---
INFO:   health = 75
INFO:   -> Status: healthy
INFO:
INFO:   For loop (counting to 5):
INFO:     i = 1
INFO:     i = 2
INFO:     i = 3
INFO:     i = 4
INFO:     i = 5
INFO:
INFO:   While loop (halving until < 1):
INFO:     value = 16.0
INFO:     value = 8.0
INFO:     value = 4.0
INFO:     value = 2.0
INFO:     value = 1.0
INFO:
INFO:   Switch (weapon = 2):
INFO:     -> Bow
INFO:
INFO: --- 4. Functions ---
INFO:   average(10.0, 20.0) = 15.0
INFO:   C passes arguments by value (copies)
INFO:   To modify the caller's data, pass a pointer
INFO:
INFO: --- 5. Arrays and Strings ---
INFO:   Array of 4 floats:
INFO:     scores[0] = 95.0
INFO:     scores[1] = 87.5
INFO:     scores[2] = 92.0
INFO:     scores[3] = 78.5
INFO:   Arrays are zero-indexed: first element is [0]
INFO:   C does NOT check array bounds -- be careful!
INFO:
INFO:   C strings:
INFO:     greeting = "Hello, GPU!"
INFO:     SDL_strlen("Hello, GPU!") = 11
INFO:     greeting[11] = 0 (null terminator '\0')
INFO:     SDL_snprintf -> "Score: 95.0"
INFO:
INFO: --- 6. Pointers ---
INFO:   int x = 42
INFO:   int *ptr = &x
INFO:   ptr  (address) = 0x7ffd...
INFO:   *ptr (value)   = 42
INFO:   After *ptr = 100: x = 100
INFO:
INFO:   Pointer arithmetic:
INFO:     *(p + 0) = 1.0  (address 0x7ffd...)
INFO:     *(p + 1) = 2.0  (address 0x7ffd...)
INFO:     *(p + 2) = 3.0  (address 0x7ffd...)
INFO:   Each step advances by 4 bytes (sizeof(float))
INFO:
INFO:   NULL pointer: 0x0
INFO:   Always check for NULL before dereferencing!
INFO:
INFO: --- 7. Structs ---
INFO:   Vertex v:
INFO:     position = (0.0, 0.5)
INFO:     color    = (1.0, 0.0, 0.0)
INFO:     sizeof(Vertex) = 20 bytes
INFO:
INFO:   Triangle (3 vertices, 60 bytes total):
INFO:     [0] pos=(0.0, 0.5) color=(1.0, 0.0, 0.0)
INFO:     [1] pos=(-0.5, -0.5) color=(0.0, 1.0, 0.0)
INFO:     [2] pos=(0.5, -0.5) color=(0.0, 0.0, 1.0)
INFO:
INFO:   Arrow operator: vp->x = 1.0 (same as (*vp).x)
INFO:
INFO: --- 8. Alignment and Padding ---
INFO:   Same fields, different order:
INFO:     sizeof(EntityPadded)  = 12 bytes  (char, int, char)
INFO:     sizeof(EntityCompact) = 8 bytes  (int, char, char)
INFO:     Payload is 6 bytes either way — the rest is padding
INFO:
INFO:   offsetof reveals padding in EntityPadded:
INFO:     active at offset 0
INFO:     health at offset 4  (not 1 — 3 bytes of padding after active)
INFO:     level  at offset 8
INFO:
INFO:   offsetof in EntityCompact:
INFO:     health at offset 0
INFO:     active at offset 4
INFO:     level  at offset 5  (no padding between two chars)
INFO:
INFO:   Vertex (all floats — no padding):
INFO:     sizeof(Vertex) = 20 bytes (5 floats x 4 = 20, no waste)
INFO:     x at offset 0
INFO:     y at offset 4
INFO:     r at offset 8
INFO:
INFO:   Array of 100 entities:
INFO:     EntityPadded[100]  = 1200 bytes
INFO:     EntityCompact[100] = 800 bytes
INFO:     Savings: 400 bytes (33% smaller)
INFO:
INFO:   Rule: order struct members from largest to smallest
INFO:   This minimizes padding and keeps GPU data tightly packed
INFO:
INFO: --- 9. Dynamic Memory ---
INFO:   Allocated 5 floats (20 bytes) on the heap
INFO:     scores[0] = 10.0
INFO:     scores[1] = 20.0
INFO:     scores[2] = 30.0
INFO:     scores[3] = 40.0
INFO:     scores[4] = 50.0
INFO:   Freed the allocation (scores = NULL)
INFO:
INFO:   SDL_calloc(3, sizeof(int)) -> all zeros:
INFO:     zeroed[0] = 0
INFO:     zeroed[1] = 0
INFO:     zeroed[2] = 0
INFO:
INFO:   SDL_memcpy copied 12 bytes:
INFO:     dst[0] = 1.0
INFO:     dst[1] = 2.0
INFO:     dst[2] = 3.0
INFO:
INFO:   Stack vs Heap:
INFO:     Stack: automatic, fast, limited size, dies with scope
INFO:     Heap:  manual (malloc/free), large, lives until freed
INFO:     GPU lessons use heap memory for vertex and index data
INFO:
INFO: --- 10. Undefined Behavior ---
INFO:   Signed integer overflow:
INFO:     INT_MAX = 2147483647
INFO:     INT_MAX + 1 is UNDEFINED for signed int
INFO:     UINT_MAX + 1 = 0 (unsigned wrap is defined)
INFO:
INFO:   Integer division by zero:
INFO:     Skipped: divisor is 0 (would be UB)
INFO:
INFO:   Use-after-free:
INFO:     *data = 42 (before free)
INFO:     After free: data = (nil) (set to NULL for safety)
INFO:
INFO:   Uninitialized variables:
INFO:     int safe_var = 0; -> 0 (predictable)
INFO:     int x;  (no initializer) -> UB to read, could be anything
INFO:
INFO:   Why undefined behavior is dangerous:
INFO:     1. The compiler assumes UB never happens
INFO:     2. It may REMOVE your safety checks based on that assumption
INFO:     3. Code may work in debug builds but break in release
INFO:     4. Symptoms often appear far from the actual bug
INFO:
INFO:   Defenses:
INFO:     - Initialize every variable at declaration
INFO:     - Check array bounds before indexing
INFO:     - Check for NULL before dereferencing pointers
INFO:     - Check divisors before dividing
INFO:     - Set pointers to NULL after freeing
INFO:     - Compile with warnings: -Wall -Wextra -Wpedantic
INFO:     - Use sanitizers: -fsanitize=address,undefined
INFO:
INFO: === End of Lesson 01 ===
```

*(Pointer addresses will vary on your machine.)*

## Key concepts

- **Static typing** — Every variable in C has a fixed type chosen at compile time; the type determines the variable's size in memory and how its bits are interpreted
- **SDL_Log** — SDL's cross-platform logging function that behaves consistently on Windows, macOS, and Linux, unlike `printf` which can differ across platforms
- **Type casting** — Explicit conversion between types using `(float)x` or `(int)y`; integer division truncates, so cast to float when you need a fractional result
- **Pass by value** — C copies arguments into function parameters; to modify the caller's data, pass a pointer
- **Null-terminated strings** — C strings are `char` arrays ending with `'\0'`; `SDL_strlen` counts characters up to (but not including) the terminator
- **Pointers** — Variables that hold memory addresses; the `&` operator takes an address, `*` dereferences it, and adding to a pointer advances by `sizeof(element)` bytes
- **Structs** — Group related data into a single type with named members; use `.` for direct access and `->` when accessing through a pointer
- **Alignment and padding** — The compiler inserts invisible bytes inside structs so each member lands at a naturally-aligned address; `offsetof` gives the true byte offset of any member, which is essential for describing vertex layouts to the GPU
- **Heap allocation** — `SDL_malloc` allocates memory that persists until you call `SDL_free`; every allocation must be freed to avoid memory leaks
- **Undefined behavior** — When code violates the C standard's rules (signed overflow, null dereference, use-after-free), the compiler is free to do anything — including removing your safety checks; defend with initialization, bounds checks, and sanitizers

## The details

### Types and sizeof

C provides a small set of built-in types. The ones you will use most in graphics
code:

| Type     | Typical size | Use case                              |
|----------|--------------|---------------------------------------|
| `int`    | 4 bytes      | Counts, indices, loop variables       |
| `float`  | 4 bytes      | Positions, colors, shader data        |
| `double` | 8 bytes      | High-precision math (CPU-side)        |
| `char`   | 1 byte       | Characters, raw byte data             |
| `bool`   | 1 byte       | Flags (true/false)                    |

The `sizeof` operator returns the size of a type or variable in bytes. GPU
lessons use `sizeof` constantly — when you upload vertex data, you must tell the
GPU exactly how many bytes to read:

```c
/* From GPU Lesson 02 — uploading vertex data to the GPU */
SDL_GPUTransferBufferCreateInfo tbci = {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size  = sizeof(vertices),  /* sizeof tells us the byte count */
};
```

### Why SDL_Log instead of printf

`printf` is the standard C output function, but it has portability issues:

- On Windows, `printf` output may not appear in some IDE consoles without
  flushing
- On Android and iOS, `printf` goes nowhere — you need platform-specific logging
- Different platforms format specifiers slightly differently

`SDL_Log` routes output through SDL's logging system, which handles all these
cases. It uses the same `printf`-style format specifiers (`%d`, `%f`, `%s`,
etc.), so the syntax is identical:

```c
printf("Health: %d\n", health);      /* standard C — works on desktop */
SDL_Log("Health: %d", health);       /* SDL — works everywhere */
```

Note that `SDL_Log` adds a newline automatically — you do not need `\n`.

### Arithmetic and type casting

C arithmetic follows the usual rules, with one important detail: **integer
division truncates**. Dividing two `int` values discards the fractional part:

```c
int a = 10, b = 3;
int result = a / b;    /* result is 3, not 3.333 */
```

To get a floating-point result, cast at least one operand:

```c
float result = (float)a / (float)b;  /* result is 3.333... */
```

This matters in graphics when computing ratios, UV coordinates, or normalized
values from integer inputs (pixel coordinates, screen dimensions).

### Control flow

C provides the standard control flow structures:

- **`if`/`else if`/`else`** — Conditional execution. Used in every SDL error
  check (`if (!SDL_Init(...))`)
- **`for`** — Count-controlled loops. The workhorse of graphics code — iterating
  over vertices, pixels, entities
- **`while`** — Condition-controlled loops. Game main loops are while loops
- **`switch`** — Multi-way branching on integer values. Useful for handling
  SDL events by type

The `for` loop syntax packs initialization, condition, and increment into one
line:

```c
for (int i = 0; i < vertex_count; i++) {
    /* process vertex i */
}
```

### Functions and forward declarations

Every function in C must be declared before it is called. A **forward
declaration** (also called a prototype) tells the compiler the function's
signature without providing the body:

```c
static float average(float a, float b);  /* declaration */

int main(void) {
    float avg = average(10.0f, 20.0f);   /* call — compiler knows the types */
}

static float average(float a, float b) { /* definition */
    return (a + b) / 2.0f;
}
```

The `static` keyword on a function limits its visibility to the current file.
This is good practice for internal helpers — it tells both the compiler and the
reader that the function is not part of a public API.

C passes all arguments **by value**: the function receives a copy. To let a
function modify the caller's variable, pass a pointer to it:

```c
void add_health(int *hp, int amount) {
    *hp += amount;  /* modifies the caller's variable through the pointer */
}
```

### Arrays and C strings

An **array** is a contiguous block of same-type elements. Array indices start at
0, and C performs **no bounds checking** — reading past the end of an array is
undefined behavior (it might crash, return garbage, or appear to work and then
fail later).

```c
float positions[3] = {1.0f, 2.0f, 3.0f};
positions[0];  /* first element */
positions[2];  /* last element */
positions[3];  /* BUG: out of bounds — undefined behavior */
```

A **C string** is a `char` array terminated by the null character `'\0'` (byte
value 0). Every string function — `SDL_strlen`, `SDL_Log`, `SDL_strcmp` —
relies on this terminator to know where the string ends:

```c
const char *name = "SDL";
/* In memory: 'S' 'D' 'L' '\0'  (4 bytes for 3 characters) */
```

SDL provides cross-platform string functions that are safer and more portable
than their C standard library counterparts:

| SDL function     | C equivalent | Notes                                |
|------------------|--------------|--------------------------------------|
| `SDL_strlen`     | `strlen`     | Returns string length                |
| `SDL_strlcpy`    | `strncpy`    | Safe copy with size limit            |
| `SDL_strlcat`    | `strncat`    | Safe concatenate with size limit     |
| `SDL_strcmp`      | `strcmp`     | Compare two strings                  |
| `SDL_snprintf`   | `snprintf`   | Formatted print to buffer            |

### Pointers

A **pointer** is a variable that stores a memory address. Pointers are central
to C and to graphics programming:

- GPU buffer uploads require a pointer to your vertex data
- SDL functions return pointers to resources (`SDL_Window *`, `SDL_GPUDevice *`)
- Large data structures are passed by pointer to avoid expensive copies

Three operators to remember:

| Operator | Name        | Example           | Meaning                          |
|----------|-------------|-------------------|----------------------------------|
| `&`      | Address-of  | `int *p = &x;`   | Get the address of `x`           |
| `*`      | Dereference | `int val = *p;`   | Read the value at address `p`    |
| `->`     | Arrow       | `ptr->member`     | Access a struct member via pointer |

**Pointer arithmetic** advances by the size of the pointed-to type, not by one
byte. This is why `*(arr + i)` is equivalent to `arr[i]` — adding `i` advances
by `i * sizeof(element)` bytes.

**NULL** is a special pointer value meaning "points to nothing." Always check
for NULL before dereferencing a pointer returned by a function that might fail:

```c
float *data = (float *)SDL_malloc(count * sizeof(float));
if (!data) {
    SDL_Log("Allocation failed: %s", SDL_GetError());
    return;
}
```

### Structs

A **struct** groups related variables into a single type. In graphics code,
structs represent vertices, materials, transforms, and application state:

```c
typedef struct {
    float x, y;       /* position */
    float r, g, b;    /* color    */
} Vertex;
```

`typedef` creates an alias so you can write `Vertex` instead of
`struct Vertex` throughout your code. C99 designated initializers make the
field assignments readable:

```c
Vertex v = { .x = 0.0f, .y = 0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f };
```

Access members with `.` for a struct value, or `->` for a pointer to a struct:

```c
v.x = 1.0f;          /* direct access */
Vertex *vp = &v;
vp->x = 1.0f;        /* pointer access (same as (*vp).x) */
```

An array of structs is exactly how vertex buffers are organized — the GPU reads
a contiguous block of packed `Vertex` structs.

### Alignment and padding

Every type in C has a **natural alignment** — the address it should start at for
the CPU to access it most efficiently. Typically, a type's alignment equals its
size: a 4-byte `int` wants a 4-byte-aligned address, an 8-byte `double` wants
an 8-byte-aligned address.

When you put different-sized types inside a struct, the compiler inserts
invisible **padding bytes** between members to satisfy each member's alignment
requirement:

```c
typedef struct {
    char  active;   /* 1 byte                                     */
                    /* 3 bytes padding — int needs 4-byte address  */
    int   health;   /* 4 bytes                                     */
    char  level;    /* 1 byte                                      */
                    /* 3 bytes trailing padding                     */
} EntityPadded;     /* sizeof = 12  (6 payload + 6 padding)        */
```

The same fields in a different order produce less padding:

```c
typedef struct {
    int   health;   /* 4 bytes                                     */
    char  active;   /* 1 byte                                      */
    char  level;    /* 1 byte                                      */
                    /* 2 bytes trailing padding                     */
} EntityCompact;    /* sizeof = 8   (6 payload + 2 padding)        */
```

Both structs hold the same data, but `EntityCompact` is 4 bytes smaller. In an
array of 1000 entities, that saves 4000 bytes — and for vertex buffers with tens
of thousands of entries, the difference is even more significant.

**The `offsetof` macro** returns the byte offset of a struct member from the
start of the struct. It accounts for padding automatically, making it the safe
way to compute member positions:

```c
#include <stddef.h>  /* or just include SDL.h, which provides offsetof */

offsetof(EntityPadded, active);   /* 0 */
offsetof(EntityPadded, health);   /* 4 — not 1, because of padding */
offsetof(EntityPadded, level);    /* 8 */
```

In GPU programming, `offsetof` is essential. When you describe a vertex layout
to the GPU, you specify the byte offset of each attribute (position, color,
normal). Manual counting is error-prone because of padding — `offsetof` always
gives the correct value:

```c
/* From GPU Lesson 02 — vertex attribute descriptions */
SDL_GPUVertexAttribute attrs[] = {
    { .offset = offsetof(Vertex, x) },  /* position */
    { .offset = offsetof(Vertex, r) },  /* color    */
};
```

**Practical rule:** Order struct members from largest to smallest. This
minimizes the padding the compiler needs to insert. When all members are the
same type (like the `Vertex` struct with five floats), there is no internal
padding at all.

### Dynamic memory: SDL_malloc and SDL_free

Variables declared inside a function live on the **stack** — they are allocated
automatically when the function is entered and released when it returns. Stack
memory is fast but limited.

For data whose size is determined at runtime, or that must outlive a function
call, use **heap** allocation:

```c
float *buffer = (float *)SDL_malloc(count * sizeof(float));
/* ... use buffer ... */
SDL_free(buffer);
buffer = NULL;
```

We use SDL's memory functions instead of the C standard `malloc`/`free` for
three reasons:

1. **Consistent behavior** — SDL_malloc uses the same allocator on every
   platform, avoiding subtle differences between system allocators
2. **SDL integration** — SDL uses SDL_malloc internally; if you replace the
   allocator with `SDL_SetMemoryFunctions`, all allocations go through the
   same path
3. **Debugging** — Some SDL builds track allocations, helping you find leaks

Additional SDL memory functions:

| SDL function   | Purpose                                           |
|----------------|---------------------------------------------------|
| `SDL_malloc`   | Allocate uninitialized memory                     |
| `SDL_calloc`   | Allocate zero-initialized memory                  |
| `SDL_realloc`  | Resize an existing allocation                     |
| `SDL_free`     | Release allocated memory                          |
| `SDL_memcpy`   | Copy a block of bytes from one address to another |
| `SDL_memset`   | Fill a block of memory with a byte value          |

**Rule:** Every `SDL_malloc` or `SDL_calloc` must have a matching `SDL_free`.
After freeing, set the pointer to NULL to prevent use-after-free bugs.

### Undefined behavior

**Undefined behavior (UB)** means the C standard places no requirements on what
the program does. It might crash, produce garbage, appear to work — or, most
dangerously, cause the compiler to optimize away your safety checks entirely.

This is the single most important concept to understand about C. In languages
like Python or Java, a bug usually produces a predictable error. In C, a bug
that triggers UB can do *anything*, and the symptoms may appear far from the
actual cause.

**Why the compiler cares:** Modern compilers assume UB never occurs. This lets
them make powerful optimizations — but it also means that if your code *does*
trigger UB, the compiler may transform your program in ways that seem impossible.
A classic example: the compiler may remove a NULL check because it "proved" the
pointer cannot be NULL (based on an earlier dereference that was itself the bug).

**Common sources of UB in C:**

| Undefined behavior         | What goes wrong                                    | Defense                             |
|----------------------------|----------------------------------------------------|-------------------------------------|
| Signed integer overflow    | `INT_MAX + 1` — compiler assumes this never happens | Use unsigned types for wrap-around; check before arithmetic |
| Integer division by zero   | Program may crash or produce garbage               | Check the divisor before dividing   |
| Out-of-bounds array access | Reads/writes memory that is not yours              | Validate indices against array size |
| Null pointer dereference   | Segfault, or compiler removes your NULL checks     | Always check before dereferencing   |
| Use-after-free             | Freed memory may be reused; silent data corruption | Set pointers to NULL after freeing  |
| Uninitialized variables    | Contains leftover stack data; may vary per build   | Initialize at declaration           |

**Compiler warnings and sanitizers** are your best defense:

```bash
# Warnings catch many UB-prone patterns at compile time
gcc -Wall -Wextra -Wpedantic main.c

# Sanitizers catch UB at runtime (slower, but finds bugs tests miss)
gcc -fsanitize=address,undefined -g main.c -o main
./main  # crashes immediately at the first UB with a clear report
```

AddressSanitizer catches out-of-bounds access, use-after-free, and memory leaks.
UndefinedBehaviorSanitizer catches signed overflow, null dereference, shift
errors, and more. Use both during development — they have saved countless hours
of debugging in graphics codebases where memory bugs are common.

## Common errors

### Forgetting to initialize a variable

**What you see:**

```text
warning: variable 'x' is uninitialized when used here
```

Or worse — no warning, and the program uses a garbage value silently.

**Why it happens:** C does not initialize local variables to zero. An `int x;`
on the stack contains whatever bits were already in that memory.

**How to fix it:** Always assign a value when you declare a variable:

```c
int x = 0;        /* initialized */
float speed;      /* BUG: uninitialized — could be any value */
```

### Integer division surprise

**What you see:** A calculation returns 0 or an unexpectedly truncated result.

```c
float ratio = 1 / 3;  /* ratio is 0.0, not 0.333 */
```

**Why it happens:** Both `1` and `3` are `int` literals, so C performs integer
division (result: 0) and then converts the 0 to a float.

**How to fix it:** Make at least one operand a float:

```c
float ratio = 1.0f / 3.0f;  /* 0.333... */
float ratio = (float)a / (float)b;  /* when using variables */
```

### Array out-of-bounds access

**What you see:** A crash, garbage values, or a security vulnerability — C does
not tell you what went wrong.

```c
int arr[3] = {10, 20, 30};
int x = arr[3];  /* BUG: valid indices are 0, 1, 2 */
```

**Why it happens:** C arrays have no runtime bounds checking. Accessing `arr[3]`
reads whatever happens to be in memory after the array.

**How to fix it:** Always check that your index is within `[0, size - 1]`. Use
a named constant for the array size:

```c
#define COUNT 3
int arr[COUNT] = {10, 20, 30};
for (int i = 0; i < COUNT; i++) { /* safe */ }
```

### Dereferencing a NULL pointer

**What you see:**

```text
Segmentation fault (core dumped)
```

Or on Windows: "Access violation reading location 0x00000000."

**Why it happens:** You tried to read or write memory through a pointer that is
NULL — meaning it does not point to valid memory.

**How to fix it:** Check for NULL before using the pointer:

```c
float *data = (float *)SDL_malloc(count * sizeof(float));
if (!data) {
    SDL_Log("Allocation failed!");
    return;
}
/* Now safe to use data */
```

### Memory leak (forgetting to free)

**What you see:** The program's memory usage grows over time. Tools like
Valgrind or AddressSanitizer report "definitely lost" blocks.

**Why it happens:** Every `SDL_malloc` must have a matching `SDL_free`. If you
return from a function or take an early exit without freeing, the memory is
leaked.

**How to fix it:** Ensure every allocation path has a corresponding free. Set
pointers to NULL after freeing:

```c
SDL_free(data);
data = NULL;
```

### Forgetting the null terminator

**What you see:** `SDL_Log` prints your string followed by garbage characters,
or the program crashes.

**Why it happens:** String functions keep reading memory until they find
`'\0'`. Without a terminator, they read past the end of your buffer.

**How to fix it:** Use `SDL_strlcpy` (which always null-terminates) instead of
manual copies, and use `SDL_snprintf` instead of building strings by hand:

```c
char buf[32];
SDL_strlcpy(buf, source, sizeof(buf));  /* always terminates */
```

## Where it's used

In forge-gpu lessons:

- [GPU Lesson 02 — First Triangle](../../gpu/02-first-triangle/) defines a
  `Vertex` struct and uses `sizeof` to upload vertex data — the same struct and
  sizeof patterns from this lesson
- [GPU Lesson 04 — Textures](../../gpu/04-textures-and-samplers/) loads image
  data into heap-allocated buffers using pointer and memory patterns from this
  lesson
- Every GPU lesson uses `SDL_Log` for error reporting and `SDL_malloc`/
  `SDL_free` for dynamic data
- [Math Lesson 01 — Vectors](../../math/01-vectors/) uses `vec3` structs,
  `sizeof`, and array layouts — the same struct and memory patterns introduced
  here, applied to vector math

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\engine\01-intro-to-c\Debug\01-intro-to-c.exe

# Linux / macOS
./build/lessons/engine/01-intro-to-c/01-intro-to-c
```

## Exercises

1. **Add a new type.** Declare a `short` and an `unsigned int` variable. Print
   their values and sizes with `SDL_Log` and `sizeof`. How do they compare to
   `int`?

2. **Write a function that swaps two integers.** You will need to pass pointers
   (why won't pass-by-value work?). Verify the swap by printing before and
   after.

3. **Extend the Vertex struct.** Add a `float z` field for 3D positions. Create
   an array of 3D vertices, print them, and note how `sizeof(Vertex)` changes.

4. **Deliberately leak memory.** Comment out the `SDL_free` call in the dynamic
   memory section and run the program under Valgrind (`valgrind ./01-intro-to-c`)
   or AddressSanitizer (`-fsanitize=address`). Read the report and identify the
   leak.

## Further reading

- [GPU Lesson 01 — Hello Window](../../gpu/01-hello-window/) — your first SDL
  window and GPU device, building on the SDL initialization pattern from this
  lesson
- [GPU Lesson 02 — First Triangle](../../gpu/02-first-triangle/) — uses vertex
  structs, sizeof, and pointer-to-data patterns introduced here
- [C reference (cppreference.com)](https://en.cppreference.com/w/c) — the
  definitive C language reference
- [SDL3 API documentation](https://wiki.libsdl.org/SDL3/) — full reference for
  SDL_Log, SDL_malloc, and every other SDL function
