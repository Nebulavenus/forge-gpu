/*
 * Engine Lesson 01 — Intro to C with SDL
 *
 * A tour of C fundamentals through the lens of graphics programming.
 * Covers types, functions, control flow, arrays, strings, pointers,
 * structs, alignment, and dynamic memory — using SDL's cross-platform
 * APIs where they improve portability.
 *
 * Why SDL for a C lesson?
 *   SDL provides cross-platform replacements for common C standard
 *   library functions.  SDL_Log works the same on Windows, macOS, and
 *   Linux (printf can behave differently).  SDL_malloc/SDL_free track
 *   allocations in debug builds.  Using SDL from the start means fewer
 *   surprises when you move on to GPU lessons.
 *
 * Concepts introduced:
 *   - Basic types (int, float, char, bool)
 *   - Formatted output with SDL_Log
 *   - Arithmetic operators and type casting
 *   - Control flow (if/else, for, while, switch)
 *   - Functions (declaration, definition, parameters, return values)
 *   - Arrays and C strings
 *   - Pointers (address-of, dereference, pointer arithmetic)
 *   - Structs (grouping related data)
 *   - Alignment and padding (offsetof, struct layout)
 *   - Dynamic memory (SDL_malloc, SDL_free)
 *   - sizeof and memory awareness
 *   - Undefined behavior (what it means, why it matters, how to avoid it)
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>

#include <stddef.h>
/* ── Section 1: Functions ─────────────────────────────────────────────── */
/*
 * In C, every function must be declared before it is called.  A forward
 * declaration (prototype) tells the compiler the function's name, return
 * type, and parameter types.  The actual body comes later.
 *
 * This is different from languages like Python or JavaScript where you
 * can define functions in any order.
 */

/* Forward declarations — the compiler needs to know these exist before
 * main() calls them. */
static void demo_types(void);
static void demo_arithmetic(void);
static void demo_control_flow(void);
static void demo_functions(void);
static void demo_arrays_and_strings(void);
static void demo_pointers(void);
static void demo_structs(void);
static void demo_alignment(void);
static void demo_memory(void);
static void demo_undefined_behavior(void);

/* ── Helper function ──────────────────────────────────────────────────── */
/* A simple function that takes two floats and returns their average.
 * We define it here so demo_functions() can call it below. */
static float average(float a, float b)
{
    return (a + b) / 2.0f;
}

/* The 'static' keyword on a function means it is only visible in this
 * file.  In a single-file program that doesn't matter, but it is good
 * practice — it tells the reader "this is an internal helper, not part
 * of a public API." */

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* Silence "unused parameter" warnings.  main() receives argc and
     * argv from the operating system, but this lesson doesn't use
     * command-line arguments. */
    (void)argc;
    (void)argv;

    /* SDL_Init(0) initializes core SDL state without enabling any
     * subsystem (video, audio, etc.).  We get SDL_Log, SDL_malloc, and
     * SDL_GetError — everything this console program needs.
     *
     * Pass SDL_INIT_VIDEO when you need a window (GPU lessons do this).
     *
     * SDL_Init returns true on success, false on failure.  Always check
     * the return value — this is a pattern you will see in every lesson. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Engine Lesson 01: Intro to C with SDL ===");
    SDL_Log(" ");

    demo_types();
    demo_arithmetic();
    demo_control_flow();
    demo_functions();
    demo_arrays_and_strings();
    demo_pointers();
    demo_structs();
    demo_alignment();
    demo_memory();
    demo_undefined_behavior();

    SDL_Log("=== End of Lesson 01 ===");

    SDL_Quit();
    return 0;
}

/* ── Section 2: Types ─────────────────────────────────────────────────── */

static void demo_types(void)
{
    SDL_Log("--- 1. Types and Variables ---");

    /* C is a statically typed language: every variable has a fixed type
     * chosen at compile time.  The type determines how much memory the
     * variable occupies and how the bits inside it are interpreted. */

    /* int — a signed integer, typically 32 bits on modern platforms.
     * Used for counts, indices, and whole numbers. */
    int lives = 3;

    /* float — a 32-bit floating-point number.  Used for positions,
     * colors, and anything that needs fractional precision.  The 'f'
     * suffix marks a float literal (without it, 1.0 is a double). */
    float speed = 5.5f;

    /* double — a 64-bit floating-point number with more precision than
     * float.  GPU shaders usually work with floats, so graphics code
     * prefers float for data that will reach the GPU. */
    double pi = 3.14159265358979;

    /* char — a single byte, often used to hold an ASCII character.
     * Character literals use single quotes. */
    char grade = 'A';

    /* bool — true or false.  In C99, you need <stdbool.h> for bool,
     * true, and false.  SDL includes it for you through SDL.h, so
     * you can use bool directly in any SDL program. */
    bool is_running = true;

    /* SDL_Log is SDL's cross-platform logging function.  It works like
     * printf but routes output through SDL's log system, which behaves
     * consistently on Windows, macOS, Linux, and even mobile platforms.
     *
     * Format specifiers:  %d = int,  %f = float/double,  %c = char,
     *                     %s = string,  %p = pointer address
     *
     * We use SDL_Log instead of printf throughout forge-gpu. */
    SDL_Log("  int:    lives = %d", lives);
    SDL_Log("  float:  speed = %.1f", speed);
    SDL_Log("  double: pi    = %.15f", pi);
    SDL_Log("  char:   grade = %c", grade);
    SDL_Log("  bool:   running = %s", is_running ? "true" : "false");

    /* sizeof tells you how many bytes a type or variable occupies.
     * This matters in graphics programming — when you upload vertex
     * data to the GPU, you must specify exact byte sizes. */
    SDL_Log(" ");
    SDL_Log("  Type sizes on this platform:");
    SDL_Log("    sizeof(char)   = %d byte",  (int)sizeof(char));
    SDL_Log("    sizeof(int)    = %d bytes", (int)sizeof(int));
    SDL_Log("    sizeof(float)  = %d bytes", (int)sizeof(float));
    SDL_Log("    sizeof(double) = %d bytes", (int)sizeof(double));
    SDL_Log("    sizeof(bool)   = %d byte",  (int)sizeof(bool));
    SDL_Log(" ");
}

/* ── Section 3: Arithmetic ────────────────────────────────────────────── */

static void demo_arithmetic(void)
{
    SDL_Log("--- 2. Arithmetic and Type Casting ---");

    int a = 10;
    int b = 3;

    /* Standard arithmetic operators work as expected. */
    SDL_Log("  %d + %d = %d", a, b, a + b);
    SDL_Log("  %d - %d = %d", a, b, a - b);
    SDL_Log("  %d * %d = %d", a, b, a * b);

    /* Integer division truncates toward zero — no rounding. */
    SDL_Log("  %d / %d = %d  (integer division truncates)", a, b, a / b);

    /* Modulo gives the remainder after division. */
    SDL_Log("  %d %% %d = %d (remainder)", a, b, a % b);

    /* Type casting: converting between types.  When you divide two
     * integers, C performs integer division.  To get a float result,
     * cast at least one operand to float. */
    float result = (float)a / (float)b;
    SDL_Log("  (float)%d / (float)%d = %.4f  (float division)", a, b, result);

    /* Implicit conversion: assigning a float to an int truncates the
     * fractional part.  The compiler may warn about this. */
    float precise = 3.7f;
    int truncated = (int)precise;
    SDL_Log("  (int)3.7f = %d  (truncated, not rounded)", truncated);
    SDL_Log(" ");
}

/* ── Section 4: Control Flow ──────────────────────────────────────────── */

static void demo_control_flow(void)
{
    SDL_Log("--- 3. Control Flow ---");

    /* -- if / else -- */
    int health = 75;
    SDL_Log("  health = %d", health);

    if (health > 50) {
        SDL_Log("  -> Status: healthy");
    } else if (health > 20) {
        SDL_Log("  -> Status: wounded");
    } else {
        SDL_Log("  -> Status: critical");
    }

    /* -- for loop -- */
    /* The most common loop in C.  Used constantly in graphics code to
     * iterate over vertices, pixels, entities, etc. */
    SDL_Log(" ");
    SDL_Log("  For loop (counting to 5):");
    for (int i = 1; i <= 5; i++) {
        SDL_Log("    i = %d", i);
    }

    /* -- while loop -- */
    /* Runs as long as the condition is true.  The main loop of a game
     * is typically a while loop that runs until the player quits. */
    SDL_Log(" ");
    SDL_Log("  While loop (halving until < 1):");
    float value = 16.0f;
    while (value >= 1.0f) {
        SDL_Log("    value = %.1f", value);
        value /= 2.0f;
    }

    /* -- switch -- */
    /* Useful when comparing a variable against several known values.
     * Each case falls through to the next unless you break. */
    SDL_Log(" ");
    int weapon = 2;
    SDL_Log("  Switch (weapon = %d):", weapon);
    switch (weapon) {
    case 1:
        SDL_Log("    -> Sword");
        break;
    case 2:
        SDL_Log("    -> Bow");
        break;
    case 3:
        SDL_Log("    -> Staff");
        break;
    default:
        SDL_Log("    -> Unknown weapon");
        break;
    }
    SDL_Log(" ");
}

/* ── Section 5: Functions ─────────────────────────────────────────────── */

static void demo_functions(void)
{
    SDL_Log("--- 4. Functions ---");

    /* Functions in C take typed parameters and return a typed value.
     * 'void' means "no value" — a void function returns nothing. */
    float avg = average(10.0f, 20.0f);
    SDL_Log("  average(10.0, 20.0) = %.1f", avg);

    /* Functions pass arguments by value: the function receives a copy.
     * Modifying the copy does not affect the original.  This is
     * different from languages where objects are passed by reference.
     *
     * To let a function modify the caller's variable, pass a pointer
     * to it (we will see this in the pointers section). */
    SDL_Log("  C passes arguments by value (copies)");
    SDL_Log("  To modify the caller's data, pass a pointer");
    SDL_Log(" ");
}

/* ── Section 6: Arrays and Strings ────────────────────────────────────── */

/* Named constants for array sizes — no magic numbers.  Using #define
 * makes the bounds explicit and ensures the declaration, loop, and any
 * future bounds check all agree on the same value. */
#define SCORE_COUNT  4
#define BUFFER_SIZE 32
#define DATA_COUNT   3

static void demo_arrays_and_strings(void)
{
    SDL_Log("--- 5. Arrays and Strings ---");

    /* An array is a contiguous block of elements of the same type.
     * In graphics, arrays hold vertex positions, colors, indices —
     * all the data that eventually reaches the GPU. */
    float scores[SCORE_COUNT] = {95.0f, 87.5f, 92.0f, 78.5f};

    SDL_Log("  Array of %d floats:", SCORE_COUNT);
    for (int i = 0; i < SCORE_COUNT; i++) {
        SDL_Log("    scores[%d] = %.1f", i, scores[i]);
    }

    /* Array indices start at 0.  Accessing scores[4] would read past
     * the end of the array — C does NOT check bounds for you.  This
     * is a common source of bugs (and security vulnerabilities). */
    SDL_Log("  Arrays are zero-indexed: first element is [0]");
    SDL_Log("  C does NOT check array bounds -- be careful!");

    /* -- C Strings -- */
    /* A "string" in C is just an array of chars ending with a null
     * terminator '\0'.  The null byte tells functions like SDL_Log
     * where the string ends. */
    SDL_Log(" ");
    SDL_Log("  C strings:");
    const char *greeting = "Hello, GPU!";
    SDL_Log("    greeting = \"%s\"", greeting);

    /* SDL_strlen counts characters (excluding the null terminator).
     * We use SDL_strlen instead of strlen from <string.h> because
     * SDL's string functions are available everywhere SDL is, and
     * they handle edge cases consistently across platforms. */
    size_t len = SDL_strlen(greeting);
    SDL_Log("    SDL_strlen(\"%s\") = %d", greeting, (int)len);

    /* Show the null terminator.  The byte after the last visible
     * character is '\0' (value 0). */
    SDL_Log("    greeting[%d] = %d (null terminator '\\0')", (int)len, greeting[len]);

    /* SDL provides several cross-platform string functions:
     *   SDL_strlen   — length (like strlen)
     *   SDL_strlcpy  — safe copy with size limit (like strlcpy)
     *   SDL_strlcat  — safe concatenate with size limit (like strlcat)
     *   SDL_strcmp    — compare two strings (like strcmp)
     *   SDL_snprintf — formatted print to a buffer (like snprintf)
     *
     * These are safer and more portable than the C standard versions. */
    char buffer[BUFFER_SIZE];
    SDL_snprintf(buffer, sizeof(buffer), "Score: %.1f", scores[0]);
    SDL_Log("    SDL_snprintf -> \"%s\"", buffer);
    SDL_Log(" ");
}

/* ── Section 7: Pointers ──────────────────────────────────────────────── */

static void demo_pointers(void)
{
    SDL_Log("--- 6. Pointers ---");

    /* A pointer is a variable that holds a memory address.
     *
     * Why do pointers matter for graphics programming?
     *   - GPU buffer uploads require a pointer to your data
     *   - SDL functions return pointers to resources (windows, devices)
     *   - Large data (meshes, textures) is passed by pointer, not copied
     *   - Dynamic memory allocation returns a pointer
     */

    int x = 42;

    /* The address-of operator (&) gives you the memory address of a
     * variable.  The result is a pointer (int*). */
    int *ptr = &x;

    SDL_Log("  int x = %d", x);
    SDL_Log("  int *ptr = &x");
    SDL_Log("  ptr  (address) = %p", (void *)ptr);

    /* The dereference operator (*) reads or writes the value at the
     * address a pointer holds. */
    SDL_Log("  *ptr (value)   = %d", *ptr);

    /* Modifying through a pointer changes the original variable.
     * This is how functions modify the caller's data in C. */
    *ptr = 100;
    SDL_Log("  After *ptr = 100: x = %d", x);

    /* -- Pointer arithmetic -- */
    /* When you add 1 to a pointer, it advances by sizeof(element),
     * not by one byte.  This is how arrays work under the hood:
     * arr[i] is equivalent to *(arr + i). */
    SDL_Log(" ");
    SDL_Log("  Pointer arithmetic:");
    float data[DATA_COUNT] = {1.0f, 2.0f, 3.0f};
    float *p = data;  /* array name decays to pointer to first element */

    for (int i = 0; i < DATA_COUNT; i++) {
        SDL_Log("    *(p + %d) = %.1f  (address %p)", i, *(p + i), (void *)(p + i));
    }
    SDL_Log("  Each step advances by %d bytes (sizeof(float))", (int)sizeof(float));

    /* -- NULL -- */
    /* NULL is a special pointer value meaning "points to nothing."
     * Always check for NULL before dereferencing a pointer returned
     * by a function that might fail (like SDL_malloc or SDL_CreateWindow). */
    SDL_Log(" ");
    int *nothing = NULL;
    SDL_Log("  NULL pointer: %p", (void *)nothing);
    SDL_Log("  Always check for NULL before dereferencing!");
    SDL_Log(" ");
}

/* ── Section 8: Structs ───────────────────────────────────────────────── */

/* A struct groups related data into a single type.  In graphics code,
 * structs represent vertices, colors, transforms, and more.
 *
 * typedef creates an alias so you can write 'Vertex' instead of
 * 'struct Vertex' everywhere. */
typedef struct {
    float x;
    float y;
    float r;
    float g;
    float b;
} Vertex;

static void demo_structs(void)
{
    SDL_Log("--- 7. Structs ---");

    /* Initialize a struct with named values.  This is exactly the
     * pattern used to define vertices in GPU lessons — each vertex
     * has a position and color packed into a struct. */
    Vertex v = {
        .x = 0.0f,
        .y = 0.5f,
        .r = 1.0f,
        .g = 0.0f,
        .b = 0.0f
    };

    SDL_Log("  Vertex v:");
    SDL_Log("    position = (%.1f, %.1f)", v.x, v.y);
    SDL_Log("    color    = (%.1f, %.1f, %.1f)", v.r, v.g, v.b);
    SDL_Log("    sizeof(Vertex) = %d bytes", (int)sizeof(Vertex));

    /* An array of structs — this is how vertex buffers are built.
     * The GPU receives a block of memory containing packed structs. */
    Vertex triangle[3] = {
        { .x =  0.0f, .y =  0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f },
        { .x = -0.5f, .y = -0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f },
        { .x =  0.5f, .y = -0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f },
    };

    SDL_Log(" ");
    SDL_Log("  Triangle (3 vertices, %d bytes total):", (int)sizeof(triangle));
    for (int i = 0; i < 3; i++) {
        SDL_Log("    [%d] pos=(%.1f, %.1f) color=(%.1f, %.1f, %.1f)",
                i,
                triangle[i].x, triangle[i].y,
                triangle[i].r, triangle[i].g, triangle[i].b);
    }

    /* The dot operator (.) accesses members of a struct.  When you
     * have a pointer to a struct, use the arrow operator (->) instead:
     *   Vertex *ptr = &v;
     *   ptr->x = 1.0f;   // same as (*ptr).x = 1.0f
     */
    Vertex *vp = &v;
    vp->x = 1.0f;
    SDL_Log(" ");
    SDL_Log("  Arrow operator: vp->x = %.1f (same as (*vp).x)", vp->x);
    SDL_Log(" ");
}

/* ── Section 9: Alignment and Padding ──────────────────────────────────── */

/* The CPU reads memory most efficiently when data sits at an address
 * that is a multiple of the data's size.  A 4-byte int is fastest at
 * an address divisible by 4; an 8-byte double, at an address divisible
 * by 8.  This requirement is called the type's *alignment*.
 *
 * The compiler enforces alignment automatically inside structs by
 * inserting invisible *padding bytes* between members.  sizeof reports
 * the total size including padding, and the struct's overall size is
 * rounded up to a multiple of its strictest member's alignment so that
 * arrays of the struct stay aligned.
 *
 * These two structs hold the same three fields in different order.
 * The order changes how much padding the compiler inserts. */

typedef struct {
    char  active;   /* 1 byte                                            */
                    /* 3 bytes padding — int needs a 4-byte-aligned addr */
    int   health;   /* 4 bytes                                           */
    char  level;    /* 1 byte                                            */
                    /* 3 bytes padding — struct rounds to 4-byte multiple */
} EntityPadded;     /* total: 12 bytes  (6 payload + 6 padding)          */

typedef struct {
    int   health;   /* 4 bytes                                           */
    char  active;   /* 1 byte                                            */
    char  level;    /* 1 byte                                            */
                    /* 2 bytes padding — struct rounds to 4-byte multiple */
} EntityCompact;    /* total: 8 bytes   (6 payload + 2 padding)          */

#define ENTITY_ARRAY_LEN 100

static void demo_alignment(void)
{
    SDL_Log("--- 8. Alignment and Padding ---");

    /* -- Padding changes struct size -- */
    /* Both structs hold the same data (char + int + char = 6 bytes of
     * payload), but the compiler inserts different amounts of padding
     * depending on member order. */
    SDL_Log("  Same fields, different order:");
    SDL_Log("    sizeof(EntityPadded)  = %d bytes  (char, int, char)",
            (int)sizeof(EntityPadded));
    SDL_Log("    sizeof(EntityCompact) = %d bytes  (int, char, char)",
            (int)sizeof(EntityCompact));
    SDL_Log("    Payload is 6 bytes either way — the rest is padding");

    /* -- offsetof reveals where members live -- */
    /* offsetof(Type, member) returns the byte offset of a member from
     * the start of the struct.  It accounts for padding, so it always
     * gives the true offset.
     *
     * This macro is critical in GPU programming: when you describe a
     * vertex layout to the GPU, you specify the byte offset of each
     * attribute (position, color, normal).  offsetof gives you the
     * correct value without manual counting. */
    SDL_Log(" ");
    SDL_Log("  offsetof reveals padding in EntityPadded:");
    SDL_Log("    active at offset %d", (int)offsetof(EntityPadded, active));
    SDL_Log("    health at offset %d  (not 1 — 3 bytes of padding after active)",
            (int)offsetof(EntityPadded, health));
    SDL_Log("    level  at offset %d", (int)offsetof(EntityPadded, level));

    SDL_Log(" ");
    SDL_Log("  offsetof in EntityCompact:");
    SDL_Log("    health at offset %d", (int)offsetof(EntityCompact, health));
    SDL_Log("    active at offset %d", (int)offsetof(EntityCompact, active));
    SDL_Log("    level  at offset %d  (no padding between two chars)",
            (int)offsetof(EntityCompact, level));

    /* -- The Vertex struct has no internal padding -- */
    /* All five members are float (4 bytes, 4-byte alignment).  Since
     * every member has the same alignment, no padding is needed between
     * them.  This is the ideal case for GPU vertex data — the struct
     * layout in memory exactly matches what the shader expects. */
    SDL_Log(" ");
    SDL_Log("  Vertex (all floats — no padding):");
    SDL_Log("    sizeof(Vertex) = %d bytes (5 floats x 4 = 20, no waste)",
            (int)sizeof(Vertex));
    SDL_Log("    x at offset %d", (int)offsetof(Vertex, x));
    SDL_Log("    y at offset %d", (int)offsetof(Vertex, y));
    SDL_Log("    r at offset %d", (int)offsetof(Vertex, r));

    /* -- Why it matters: arrays multiply the waste -- */
    /* An array of 100 padded structs wastes 100 x 4 = 400 bytes
     * compared to the compact version.  For vertex buffers with
     * thousands of entries, layout matters. */
    SDL_Log(" ");
    int padded_total  = ENTITY_ARRAY_LEN * (int)sizeof(EntityPadded);
    int compact_total = ENTITY_ARRAY_LEN * (int)sizeof(EntityCompact);
    SDL_Log("  Array of %d entities:", ENTITY_ARRAY_LEN);
    SDL_Log("    EntityPadded[%d]  = %d bytes", ENTITY_ARRAY_LEN, padded_total);
    SDL_Log("    EntityCompact[%d] = %d bytes", ENTITY_ARRAY_LEN, compact_total);
    SDL_Log("    Savings: %d bytes (%.0f%% smaller)",
            padded_total - compact_total,
            100.0f * (float)(padded_total - compact_total) / (float)padded_total);

    /* -- Practical rule -- */
    SDL_Log(" ");
    SDL_Log("  Rule: order struct members from largest to smallest");
    SDL_Log("  This minimizes padding and keeps GPU data tightly packed");
    SDL_Log(" ");
}

/* ── Section 10: Dynamic Memory ───────────────────────────────────────── */

static void demo_memory(void)
{
    SDL_Log("--- 9. Dynamic Memory ---");

    /* Variables declared inside a function live on the stack.  The
     * stack is fast but limited in size, and stack variables disappear
     * when the function returns.
     *
     * For data that must outlive a function, or whose size is not known
     * at compile time, we allocate on the heap.
     *
     * C standard library provides malloc/free.  SDL provides SDL_malloc
     * and SDL_free, which we prefer for several reasons:
     *
     *   1. Consistent behavior — SDL_malloc uses the same allocator on
     *      every platform, avoiding subtle differences between system
     *      allocators.
     *
     *   2. SDL integration — SDL itself uses SDL_malloc internally.
     *      If you ever replace the allocator (SDL_SetMemoryFunctions),
     *      all allocations go through the same path.
     *
     *   3. Debugging — Some SDL builds track allocations, helping you
     *      find leaks.
     *
     * Rule of thumb: if your program uses SDL, use SDL_malloc/SDL_free.
     */

    /* Allocate an array of 5 floats on the heap. */
    int count = 5;
    float *scores = (float *)SDL_malloc(count * sizeof(float));

    /* Always check if the allocation succeeded.  SDL_malloc returns
     * NULL if the system is out of memory (rare, but possible). */
    if (!scores) {
        SDL_Log("  SDL_malloc failed!");
        return;
    }

    SDL_Log("  Allocated %d floats (%d bytes) on the heap",
            count, (int)(count * sizeof(float)));

    /* Fill the array. */
    for (int i = 0; i < count; i++) {
        scores[i] = (float)(i + 1) * 10.0f;
    }

    /* Print the values. */
    for (int i = 0; i < count; i++) {
        SDL_Log("    scores[%d] = %.1f", i, scores[i]);
    }

    /* SDL_free releases the memory back to the system.  Forgetting to
     * call SDL_free is a "memory leak" — the program keeps consuming
     * memory until it runs out.
     *
     * Rule: every SDL_malloc must have a matching SDL_free.
     *       every SDL_calloc must have a matching SDL_free.
     *
     * After freeing, set the pointer to NULL to avoid accidentally
     * using freed memory (a "use-after-free" bug). */
    SDL_free(scores);
    scores = NULL;
    SDL_Log("  Freed the allocation (scores = NULL)");

    /* -- SDL_calloc: allocate and zero-initialize -- */
    /* SDL_calloc is like SDL_malloc, but it sets every byte to zero.
     * Useful when you need a clean starting state. */
    SDL_Log(" ");
    int *zeroed = (int *)SDL_calloc(3, sizeof(int));
    if (zeroed) {
        SDL_Log("  SDL_calloc(3, sizeof(int)) -> all zeros:");
        for (int i = 0; i < 3; i++) {
            SDL_Log("    zeroed[%d] = %d", i, zeroed[i]);
        }
        SDL_free(zeroed);
    }

    /* -- SDL_memcpy: copy raw bytes -- */
    /* Copies a block of memory from one location to another.
     * Used constantly in graphics to copy vertex data, textures, etc.
     * into GPU-visible buffers. */
    SDL_Log(" ");
    float src[3] = {1.0f, 2.0f, 3.0f};
    float dst[3];
    SDL_memcpy(dst, src, sizeof(src));
    SDL_Log("  SDL_memcpy copied %d bytes:", (int)sizeof(src));
    for (int i = 0; i < 3; i++) {
        SDL_Log("    dst[%d] = %.1f", i, dst[i]);
    }

    /* -- Stack vs Heap summary -- */
    SDL_Log(" ");
    SDL_Log("  Stack vs Heap:");
    SDL_Log("    Stack: automatic, fast, limited size, dies with scope");
    SDL_Log("    Heap:  manual (malloc/free), large, lives until freed");
    SDL_Log("    GPU lessons use heap memory for vertex and index data");
    SDL_Log(" ");
}

/* ── Section 11: Undefined Behavior ───────────────────────────────────── */

static void demo_undefined_behavior(void)
{
    SDL_Log("--- 10. Undefined Behavior ---");

    /* Undefined behavior (UB) means the C standard places no requirements
     * on what the program does.  It might crash, return garbage, appear
     * to work — or, most dangerously, let the compiler optimize away
     * your safety checks entirely.
     *
     * UB is NOT "implementation-defined" (where each platform picks a
     * behavior and documents it).  UB means *anything can happen*, and
     * the compiler is allowed to assume it never occurs.  This assumption
     * is what makes UB so treacherous: the compiler can remove code paths
     * that would only execute if UB had occurred. */

    /* -- Example 1: Signed integer overflow -- */
    /* Adding 1 to INT_MAX is undefined behavior for signed integers.
     * The compiler assumes this never happens and may optimize based on
     * that assumption.  For example, a loop condition like (i + 1 > i)
     * can be optimized to "always true" because signed overflow "can't
     * happen."
     *
     * Unsigned integers are different: they wrap around with modular
     * arithmetic, and that wrapping IS defined by the standard. */
    int big = SDL_MAX_SINT32;  /* 2,147,483,647 */
    SDL_Log("  Signed integer overflow:");
    SDL_Log("    INT_MAX = %d", big);
    SDL_Log("    INT_MAX + 1 is UNDEFINED for signed int");

    Uint32 u = SDL_MAX_UINT32;
    Uint32 wrapped = u + 1;  /* defined: wraps to 0 */
    SDL_Log("    UINT_MAX + 1 = %u (unsigned wrap is defined)", wrapped);

    /* -- Example 2: Integer division by zero -- */
    /* Dividing an integer by zero is undefined behavior.  Floating-point
     * division by zero produces infinity (IEEE 754 defines this), but
     * integer division has no such safety net.  Always validate the
     * divisor before dividing. */
    SDL_Log(" ");
    SDL_Log("  Integer division by zero:");
    int divisor = 0;
    if (divisor != 0) {
        SDL_Log("    10 / %d = %d", divisor, 10 / divisor);
    } else {
        SDL_Log("    Skipped: divisor is 0 (would be UB)");
    }

    /* -- Example 3: Use-after-free -- */
    /* After SDL_free(ptr), the memory at that address is no longer yours.
     * Reading or writing it is undefined behavior — it might return the
     * old value, garbage, or crash.  Worse, the allocator may have given
     * that memory to something else, so writing through a freed pointer
     * can silently corrupt unrelated data.
     *
     * The safe pattern: set the pointer to NULL immediately after free.
     * Then a NULL check will catch any accidental reuse. */
    SDL_Log(" ");
    SDL_Log("  Use-after-free:");
    int *data = (int *)SDL_malloc(sizeof(int));
    if (data) {
        *data = 42;
        SDL_Log("    *data = %d (before free)", *data);
        SDL_free(data);
        data = NULL;  /* prevents accidental use-after-free */
        SDL_Log("    After free: data = %p (set to NULL for safety)",
                (void *)data);
    }

    /* -- Example 4: Uninitialized variables -- */
    /* Using a variable before assigning a value is undefined behavior.
     * The variable holds whatever bits were left on the stack — but the
     * compiler may also optimize as if the read never occurs, producing
     * surprising results that change between debug and release builds.
     *
     * Safe pattern: always initialize at the point of declaration. */
    SDL_Log(" ");
    SDL_Log("  Uninitialized variables:");
    int safe_var = 0;  /* always initialize */
    SDL_Log("    int safe_var = 0; -> %d (predictable)", safe_var);
    SDL_Log("    int x;  (no initializer) -> UB to read, could be anything");

    /* -- Summary: why UB matters and how to defend against it -- */
    SDL_Log(" ");
    SDL_Log("  Why undefined behavior is dangerous:");
    SDL_Log("    1. The compiler assumes UB never happens");
    SDL_Log("    2. It may REMOVE your safety checks based on that assumption");
    SDL_Log("    3. Code may work in debug builds but break in release");
    SDL_Log("    4. Symptoms often appear far from the actual bug");

    SDL_Log(" ");
    SDL_Log("  Defenses:");
    SDL_Log("    - Initialize every variable at declaration");
    SDL_Log("    - Check array bounds before indexing");
    SDL_Log("    - Check for NULL before dereferencing pointers");
    SDL_Log("    - Check divisors before dividing");
    SDL_Log("    - Set pointers to NULL after freeing");
    SDL_Log("    - Compile with warnings: -Wall -Wextra -Wpedantic");
    SDL_Log("    - Use sanitizers: -fsanitize=address,undefined");
    SDL_Log(" ");
}
