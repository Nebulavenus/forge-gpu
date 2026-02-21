/*
 * Engine Lesson 04 — Pointers & Memory
 *
 * A hands-on tour of how C manages memory, and why it matters for GPU
 * programming.  Covers:
 *   - Pointers: address-of (&), dereference (*), NULL
 *   - Stack vs heap allocation
 *   - malloc / free (and SDL_malloc / SDL_free)
 *   - Pointer arithmetic and array equivalence
 *   - sizeof — measuring type and object sizes
 *   - offsetof — finding struct member positions
 *   - Putting it all together: building a vertex buffer in CPU memory
 *     exactly the way GPU lessons do before uploading to the GPU
 *
 * Why this lesson exists:
 *   GPU lessons create vertex structs, compute offsets with offsetof,
 *   allocate transfer buffers, and memcpy data into mapped GPU memory.
 *   Every one of those operations is a pointer and memory operation.
 *   Understanding them here makes GPU lessons much clearer.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>  /* offsetof */

/* ── Forward declarations ────────────────────────────────────────────────── */

static void demo_pointers_basics(void);
static void demo_stack_vs_heap(void);
static void demo_malloc_free(void);
static void demo_pointer_arithmetic(void);
static void demo_sizeof(void);
static void demo_offsetof(void);
static void demo_vertex_buffer_upload(void);
static void demo_common_bugs(void);

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Print a horizontal divider to separate sections in the output. */
static void print_divider(const char *title)
{
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("  %s", title);
    SDL_Log("------------------------------------------------------------");
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* SDL_Init(0) initializes core SDL state without enabling any subsystem
     * (video, audio, etc.).  We get SDL_Log and SDL_GetError — everything
     * this console program needs. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Engine Lesson 04: Pointers & Memory ===");

    demo_pointers_basics();
    demo_stack_vs_heap();
    demo_malloc_free();
    demo_pointer_arithmetic();
    demo_sizeof();
    demo_offsetof();
    demo_vertex_buffer_upload();
    demo_common_bugs();

    SDL_Log(" ");
    SDL_Log("=== All sections complete ===");

    SDL_Quit();
    return 0;
}

/* ── Section 1: Pointer basics ───────────────────────────────────────────── */
/*
 * A pointer is a variable whose value is a memory address.
 *
 *   int  x  = 42;    // x holds the value 42
 *   int *p  = &x;    // p holds the ADDRESS of x
 *   int  y  = *p;    // y gets the value AT that address (42)
 *
 * Two operators do all the work:
 *   &  (address-of)  — gives you the address of a variable
 *   *  (dereference)  — reads or writes the value at an address
 *
 * A pointer that does not point to valid memory should be NULL.
 * Dereferencing NULL is undefined behavior and usually crashes.
 */
static void demo_pointers_basics(void)
{
    print_divider("1. Pointer Basics");

    int x = 42;
    int *p = &x;   /* p now holds the address of x */

    SDL_Log("x  = %d", x);
    SDL_Log("&x = %p  (address of x in memory)", (void *)&x);
    SDL_Log("p  = %p  (pointer p stores the same address)", (void *)p);
    SDL_Log("*p = %d  (dereferencing p gives us x's value)", *p);

    /* Modifying *p changes x — they refer to the same memory. */
    *p = 99;
    SDL_Log(" ");
    SDL_Log("After *p = 99:");
    SDL_Log("  x  = %d  (x changed because p points to x)", x);
    SDL_Log("  *p = %d  (same value, same memory)", *p);

    /* NULL pointer — the universal "points to nothing" value. */
    int *null_ptr = NULL;
    SDL_Log(" ");
    SDL_Log("null_ptr = %p  (NULL — does not point to valid memory)",
            (void *)null_ptr);
    SDL_Log("Dereferencing NULL would crash the program (undefined behavior).");

    /* Why pointers matter for GPU programming:
     *
     * When you call SDL_MapGPUTransferBuffer(), the GPU driver returns a
     * void* — a raw pointer to a block of memory that the GPU can read.
     * You write your vertex data into that pointer, then unmap it so the
     * GPU can use it.  Understanding pointers is the foundation. */
}

/* ── Section 2: Stack vs heap ────────────────────────────────────────────── */
/*
 * C has two main regions of memory:
 *
 * STACK — fast, automatic, limited size
 *   - Local variables live here
 *   - Allocated when a function is called, freed when it returns
 *   - Typical size: 1-8 MB (varies by OS)
 *   - You do NOT call free() on stack memory
 *
 * HEAP — large, manual, slower
 *   - Allocated with malloc() / SDL_malloc()
 *   - Freed with free() / SDL_free()
 *   - Survives beyond the function that allocated it
 *   - Can hold gigabytes of data
 *
 * Rule of thumb:
 *   - Small, short-lived data → stack (local variables)
 *   - Large or long-lived data → heap (malloc/free)
 */
static void demo_stack_vs_heap(void)
{
    print_divider("2. Stack vs Heap");

    /* Stack allocation — automatic, fast, scoped to this function. */
    int stack_var = 10;
    float stack_array[4] = { 1.0f, 2.0f, 3.0f, 4.0f };

    SDL_Log("Stack variable:  &stack_var   = %p  (value = %d)",
            (void *)&stack_var, stack_var);
    SDL_Log("Stack array:     &stack_array = %p  (4 floats, %u bytes)",
            (void *)stack_array, (unsigned)(sizeof(stack_array)));

    /* Heap allocation — manual, persists until you free it. */
    float *heap_array = (float *)SDL_malloc(4 * sizeof(float));
    if (!heap_array) {
        SDL_Log("SDL_malloc failed!");
        return;
    }
    heap_array[0] = 1.0f;
    heap_array[1] = 2.0f;
    heap_array[2] = 3.0f;
    heap_array[3] = 4.0f;

    SDL_Log("Heap array:      heap_array   = %p  (4 floats, %u bytes)",
            (void *)heap_array, (unsigned)(4 * sizeof(float)));

    /* Notice the addresses — stack and heap are in different regions. */
    SDL_Log(" ");
    SDL_Log("Stack addresses are typically high (near top of address space).");
    SDL_Log("Heap addresses are typically lower.");
    SDL_Log("The exact values vary by platform and run, but the pattern");
    SDL_Log("is consistent: stack and heap occupy different regions.");

    /* Key difference: stack memory is freed automatically when the function
     * returns.  Heap memory persists until you explicitly free it. */
    SDL_free(heap_array);

    SDL_Log(" ");
    SDL_Log("SDL_free(heap_array) released the heap memory.");
    SDL_Log("stack_var and stack_array will be freed automatically");
    SDL_Log("when this function returns.");
}

/* ── Section 3: malloc and free ──────────────────────────────────────────── */
/*
 * malloc(size) allocates `size` bytes on the heap and returns a pointer.
 * free(ptr) releases that memory back to the allocator.
 *
 * SDL_malloc / SDL_free are SDL's wrappers around the system allocator.
 * They work the same way but allow SDL to track allocations in debug builds.
 * forge-gpu uses SDL_malloc / SDL_free throughout.
 *
 * Patterns:
 *   float *data = (float *)SDL_malloc(count * sizeof(float));
 *   if (!data) { handle error }
 *   ... use data ...
 *   SDL_free(data);
 *
 * Common mistakes:
 *   - Forgetting to free → memory leak
 *   - Using after free → use-after-free (undefined behavior)
 *   - Freeing twice → double free (undefined behavior)
 *   - Wrong size in malloc → buffer overflow
 */
static void demo_malloc_free(void)
{
    print_divider("3. malloc / free");

    /* Allocate an array of 6 floats on the heap.
     * This is similar to what GPU lessons do when building vertex data. */
    int count = 6;
    float *data = (float *)SDL_malloc((size_t)count * sizeof(float));
    if (!data) {
        SDL_Log("SDL_malloc failed!");
        return;
    }

    SDL_Log("Allocated %d floats (%u bytes) at address %p",
            count, (unsigned)((size_t)count * sizeof(float)), (void *)data);

    /* Fill the array. */
    for (int i = 0; i < count; i++) {
        data[i] = (float)i * 1.5f;
    }

    /* Print contents. */
    SDL_Log("Contents:");
    for (int i = 0; i < count; i++) {
        SDL_Log("  data[%d] = %.1f  (at address %p)", i, data[i],
                (void *)&data[i]);
    }

    /* SDL_calloc: like malloc but zeros the memory.  Useful when you need
     * all bytes initialized to 0. */
    int *zeroed = (int *)SDL_calloc(4, sizeof(int));
    if (!zeroed) {
        SDL_Log("SDL_calloc failed!");
        SDL_free(data);
        return;
    }

    SDL_Log(" ");
    SDL_Log("SDL_calloc(4, sizeof(int)) gives zeroed memory:");
    for (int i = 0; i < 4; i++) {
        SDL_Log("  zeroed[%d] = %d", i, zeroed[i]);
    }

    /* Always free what you allocate. */
    SDL_free(data);
    SDL_free(zeroed);
    SDL_Log(" ");
    SDL_Log("Both allocations freed. No memory leaked.");
}

/* ── Section 4: Pointer arithmetic ───────────────────────────────────────── */
/*
 * In C, pointer arithmetic is scaled by the size of the pointed-to type.
 *
 *   int *p = ...;
 *   p + 1    → moves forward by sizeof(int) bytes, not 1 byte
 *
 * This is why array indexing works:
 *   arr[i]  is equivalent to  *(arr + i)
 *
 * The compiler multiplies i by sizeof(element) automatically.
 *
 * For GPU programming, this means:
 *   Vertex *verts = ...;
 *   verts + 3  →  points to the 4th vertex (skips 3 * sizeof(Vertex) bytes)
 */
static void demo_pointer_arithmetic(void)
{
    print_divider("4. Pointer Arithmetic");

    int arr[5] = { 10, 20, 30, 40, 50 };
    int *p = arr;  /* p points to arr[0] */

    SDL_Log("Array base address: %p", (void *)p);
    SDL_Log("sizeof(int) = %u bytes", (unsigned)sizeof(int));
    SDL_Log(" ");

    /* Each step of p+1 moves forward by sizeof(int) bytes. */
    for (int i = 0; i < 5; i++) {
        SDL_Log("  p + %d = %p -> value = %d  (arr[%d])",
                i, (void *)(p + i), *(p + i), i);
    }

    SDL_Log(" ");
    SDL_Log("Notice: each address increases by %u (sizeof(int)).",
            (unsigned)sizeof(int));
    SDL_Log("The compiler scales pointer arithmetic by the element size.");

    /* Pointer subtraction gives the number of elements between two pointers. */
    int *first = &arr[0];
    int *last  = &arr[4];
    ptrdiff_t distance = last - first;
    SDL_Log(" ");
    SDL_Log("Distance from arr[0] to arr[4]: %td elements", distance);
    SDL_Log("  (that is %td bytes in raw address difference)",
            (ptrdiff_t)((char *)last - (char *)first));

    /* void* — the generic pointer.  No arithmetic allowed because the
     * compiler does not know the element size.  Used by SDL_MapGPUTransferBuffer
     * and SDL_memcpy since they work with raw bytes. */
    void *generic = arr;
    SDL_Log(" ");
    SDL_Log("void *generic = %p  (same address, no type information)", generic);
    SDL_Log("You must cast void* to a typed pointer before using it.");
    SDL_Log("This is exactly what happens after SDL_MapGPUTransferBuffer().");
}

/* ── Section 5: sizeof ───────────────────────────────────────────────────── */
/*
 * sizeof returns the size (in bytes) of a type or expression.
 * It is evaluated at compile time — no runtime cost.
 *
 * Critical uses in GPU programming:
 *   - Allocating the right amount of memory:  malloc(n * sizeof(Vertex))
 *   - Telling the GPU how big a buffer is:    .size = sizeof(vertices)
 *   - Computing vertex stride:                .pitch = sizeof(Vertex)
 */
static void demo_sizeof(void)
{
    print_divider("5. sizeof");

    /* Fundamental types. */
    SDL_Log("Fundamental types:");
    SDL_Log("  sizeof(char)   = %u byte",  (unsigned)sizeof(char));
    SDL_Log("  sizeof(int)    = %u bytes", (unsigned)sizeof(int));
    SDL_Log("  sizeof(float)  = %u bytes", (unsigned)sizeof(float));
    SDL_Log("  sizeof(double) = %u bytes", (unsigned)sizeof(double));
    SDL_Log("  sizeof(void *) = %u bytes (pointer size)",
            (unsigned)sizeof(void *));

    /* Pointer size is the same regardless of what it points to.
     * On a 64-bit system, all pointers are 8 bytes.  On 32-bit, 4 bytes. */
    SDL_Log(" ");
    SDL_Log("All pointer types have the same size:");
    SDL_Log("  sizeof(int *)    = %u", (unsigned)sizeof(int *));
    SDL_Log("  sizeof(float *)  = %u", (unsigned)sizeof(float *));
    SDL_Log("  sizeof(char *)   = %u", (unsigned)sizeof(char *));

    /* sizeof on arrays gives the TOTAL size, not the element count. */
    float arr[10];
    SDL_Log(" ");
    SDL_Log("float arr[10]:");
    SDL_Log("  sizeof(arr)         = %u bytes (entire array)",
            (unsigned)sizeof(arr));
    SDL_Log("  sizeof(arr[0])      = %u bytes (one element)",
            (unsigned)sizeof(arr[0]));
    SDL_Log("  sizeof(arr)/sizeof(arr[0]) = %u elements (array length)",
            (unsigned)(sizeof(arr) / sizeof(arr[0])));

    /* WARNING: sizeof on a pointer gives the pointer size, not the array size.
     * This is a common source of bugs when passing arrays to functions. */
    float *ptr = arr;
    SDL_Log(" ");
    SDL_Log("[!] sizeof(ptr) = %u -- this is the POINTER size, not the array!",
            (unsigned)sizeof(ptr));
    SDL_Log("    When an array decays to a pointer, sizeof information is lost.");
    SDL_Log("    Always pass the element count alongside the pointer.");
}

/* ── Section 6: offsetof ─────────────────────────────────────────────────── */
/*
 * offsetof(type, member) returns the byte offset of a member within a struct.
 *
 * This is essential for GPU programming because the vertex input layout
 * must tell the GPU exactly where each attribute sits inside the vertex struct.
 *
 * Example from GPU Lesson 02:
 *   typedef struct Vertex {
 *       vec2 position;   // offset 0,  size 8
 *       vec3 color;      // offset 8,  size 12
 *   } Vertex;            // total: 20 bytes
 *
 *   attrs[0].offset = offsetof(Vertex, position);  // 0
 *   attrs[1].offset = offsetof(Vertex, color);      // 8
 *
 * Without offsetof, you would have to manually count bytes — error-prone
 * and fragile if the struct changes.
 */

/* A simple vertex struct matching GPU Lesson 02. */
typedef struct SimpleVertex {
    float position[2];   /* 8 bytes: x, y        */
    float color[3];      /* 12 bytes: r, g, b    */
} SimpleVertex;          /* 20 bytes total        */

/* A more complex vertex with normals (like GPU Lesson 10). */
typedef struct LitVertex {
    float position[3];   /* 12 bytes: x, y, z    */
    float normal[3];     /* 12 bytes: nx, ny, nz */
    float uv[2];         /* 8 bytes: u, v        */
} LitVertex;             /* 32 bytes total        */

/* A struct demonstrating padding (the compiler inserts gaps for alignment). */
typedef struct PaddedExample {
    char  tag;           /* 1 byte                */
    /* 3 bytes of padding inserted by compiler    */
    float value;         /* 4 bytes (must be 4-byte aligned) */
    char  flag;          /* 1 byte                */
    /* 3 bytes of trailing padding               */
} PaddedExample;         /* 12 bytes total, not 6! */

static void demo_offsetof(void)
{
    print_divider("6. offsetof");

    /* SimpleVertex — the layout from GPU Lesson 02. */
    SDL_Log("SimpleVertex (GPU Lesson 02 pattern):");
    SDL_Log("  sizeof(SimpleVertex) = %u bytes",
            (unsigned)sizeof(SimpleVertex));
    SDL_Log("  offsetof(position)   = %u",
            (unsigned)offsetof(SimpleVertex, position));
    SDL_Log("  offsetof(color)      = %u",
            (unsigned)offsetof(SimpleVertex, color));
    SDL_Log(" ");
    SDL_Log("  Layout: [position: 8 bytes][color: 12 bytes] = 20 bytes");
    SDL_Log("  No padding needed — float arrays are naturally aligned.");

    /* LitVertex — a common layout for lit meshes. */
    SDL_Log(" ");
    SDL_Log("LitVertex (GPU Lesson 10 pattern):");
    SDL_Log("  sizeof(LitVertex) = %u bytes",
            (unsigned)sizeof(LitVertex));
    SDL_Log("  offsetof(position) = %u",
            (unsigned)offsetof(LitVertex, position));
    SDL_Log("  offsetof(normal)   = %u",
            (unsigned)offsetof(LitVertex, normal));
    SDL_Log("  offsetof(uv)       = %u",
            (unsigned)offsetof(LitVertex, uv));
    SDL_Log(" ");
    SDL_Log("  Layout: [position: 12][normal: 12][uv: 8] = 32 bytes");

    /* PaddedExample — showing the compiler's alignment rules. */
    SDL_Log(" ");
    SDL_Log("PaddedExample (alignment padding demo):");
    SDL_Log("  sizeof(PaddedExample) = %u bytes (not 6!)",
            (unsigned)sizeof(PaddedExample));
    SDL_Log("  offsetof(tag)   = %u",
            (unsigned)offsetof(PaddedExample, tag));
    SDL_Log("  offsetof(value) = %u  (3 bytes of padding after tag)",
            (unsigned)offsetof(PaddedExample, value));
    SDL_Log("  offsetof(flag)  = %u",
            (unsigned)offsetof(PaddedExample, flag));
    SDL_Log(" ");
    SDL_Log("  The compiler inserts padding to keep 'float value' at a");
    SDL_Log("  4-byte-aligned address. Unaligned access is slow (or");
    SDL_Log("  illegal) on many CPUs. GPU vertex attributes have similar");
    SDL_Log("  alignment requirements.");

    /* Why this matters for GPU programming:
     *
     * When you configure SDL_GPUVertexAttribute, you set:
     *   .offset = offsetof(Vertex, member)
     *
     * The GPU reads vertex data as raw bytes from a buffer.  If the
     * offset is wrong, it reads garbage.  offsetof guarantees correctness
     * even if the compiler adds padding you did not expect. */
    SDL_Log(" ");
    SDL_Log("Key takeaway: always use offsetof() for vertex attribute offsets.");
    SDL_Log("Never hard-code byte offsets — padding can change between");
    SDL_Log("compilers and platforms.");
}

/* ── Section 7: Vertex buffer upload (GPU integration) ───────────────────── */
/*
 * This section simulates the CPU side of a GPU vertex buffer upload — the
 * exact pattern used in GPU Lesson 02 (First Triangle).
 *
 * The real GPU upload sequence is:
 *   1. Define a Vertex struct with position, color, etc.
 *   2. Create an array of vertices (the mesh data)
 *   3. Allocate a transfer buffer (CPU-accessible staging area)
 *   4. Map the transfer buffer to get a void* pointer
 *   5. memcpy vertex data into the mapped pointer
 *   6. Unmap the buffer so the GPU can read it
 *   7. Record a copy command from transfer buffer to GPU buffer
 *
 * Steps 4-5 are pure pointer and memory operations.  We simulate them
 * here without a real GPU to demonstrate the memory mechanics.
 */

/* A vertex struct matching GPU Lesson 02. */
typedef struct Vertex {
    float px, py;        /* position: 8 bytes  */
    float r, g, b;       /* color:    12 bytes */
} Vertex;                /* total:    20 bytes */

static void demo_vertex_buffer_upload(void)
{
    print_divider("7. Vertex Buffer Upload (Simulated)");

    /* Step 1: Define the mesh data (3 vertices for a triangle). */
    Vertex triangle[3] = {
        { .px =  0.0f, .py =  0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f },
        { .px = -0.5f, .py = -0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f },
        { .px =  0.5f, .py = -0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f },
    };

    size_t buffer_size = sizeof(triangle);
    SDL_Log("Vertex data: 3 vertices, %u bytes each, %u bytes total",
            (unsigned)sizeof(Vertex), (unsigned)buffer_size);

    /* Step 2: Simulate the GPU transfer buffer — a heap-allocated block
     * representing the mapped memory returned by SDL_MapGPUTransferBuffer. */
    void *mapped = SDL_malloc(buffer_size);
    if (!mapped) {
        SDL_Log("Allocation failed!");
        return;
    }
    SDL_Log("'Transfer buffer' allocated at %p (%u bytes)",
            mapped, (unsigned)buffer_size);

    /* Step 3: Copy vertex data into the mapped buffer.
     * This is exactly what GPU Lesson 02 does:
     *   void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
     *   SDL_memcpy(mapped, triangle_vertices, sizeof(triangle_vertices));
     *
     * SDL_memcpy copies raw bytes — it does not care about types.
     * The source and destination are both void* at this level. */
    SDL_memcpy(mapped, triangle, buffer_size);
    SDL_Log("SDL_memcpy copied %u bytes from triangle[] into mapped buffer",
            (unsigned)buffer_size);

    /* Step 4: Verify the data by casting the void* back to Vertex*.
     * In real GPU code, you would unmap the buffer at this point and
     * let the GPU read it.  Here we cast back to inspect the bytes. */
    Vertex *gpu_verts = (Vertex *)mapped;

    SDL_Log(" ");
    SDL_Log("Reading back from the 'transfer buffer':");
    for (int i = 0; i < 3; i++) {
        SDL_Log("  Vertex %d: pos(%.1f, %.1f) color(%.1f, %.1f, %.1f)",
                i, gpu_verts[i].px, gpu_verts[i].py,
                gpu_verts[i].r, gpu_verts[i].g, gpu_verts[i].b);
    }

    /* Step 5: Show the raw byte layout — this is what the GPU sees. */
    SDL_Log(" ");
    SDL_Log("Raw byte layout (what the GPU reads from the buffer):");
    const unsigned char *bytes = (const unsigned char *)mapped;
    for (int v = 0; v < 3; v++) {
        size_t start = (size_t)v * sizeof(Vertex);
        SDL_Log("  Vertex %d (offset %u):", v, (unsigned)start);

        /* Show position bytes — read raw bytes through our byte pointer,
         * then show the interpreted float values from gpu_verts for comparison. */
        size_t px_off = start + offsetof(Vertex, px);
        size_t py_off = start + offsetof(Vertex, py);
        SDL_Log("    position [%u..%u]: bytes %02x %02x %02x %02x | %02x %02x %02x %02x  => px=%.1f, py=%.1f",
                (unsigned)px_off,
                (unsigned)(py_off + sizeof(float) - 1),
                bytes[px_off], bytes[px_off+1], bytes[px_off+2], bytes[px_off+3],
                bytes[py_off], bytes[py_off+1], bytes[py_off+2], bytes[py_off+3],
                gpu_verts[v].px, gpu_verts[v].py);

        /* Show color bytes — same pattern: raw bytes then interpreted values. */
        size_t r_off = start + offsetof(Vertex, r);
        size_t g_off = start + offsetof(Vertex, g);
        size_t b_off = start + offsetof(Vertex, b);
        SDL_Log("    color    [%u..%u]: bytes %02x %02x %02x %02x | %02x %02x %02x %02x | %02x %02x %02x %02x  => r=%.1f, g=%.1f, b=%.1f",
                (unsigned)r_off,
                (unsigned)(b_off + sizeof(float) - 1),
                bytes[r_off], bytes[r_off+1], bytes[r_off+2], bytes[r_off+3],
                bytes[g_off], bytes[g_off+1], bytes[g_off+2], bytes[g_off+3],
                bytes[b_off], bytes[b_off+1], bytes[b_off+2], bytes[b_off+3],
                gpu_verts[v].r, gpu_verts[v].g, gpu_verts[v].b);
    }

    SDL_Log(" ");
    SDL_Log("This is the exact memory layout that a GPU vertex shader reads.");
    SDL_Log("The vertex input layout (configured with offsetof) tells the GPU");
    SDL_Log("where each attribute starts within each %u-byte vertex.",
            (unsigned)sizeof(Vertex));

    /* Clean up — in real GPU code, you would call
     * SDL_UnmapGPUTransferBuffer followed by SDL_ReleaseGPUTransferBuffer. */
    SDL_free(mapped);
}

/* ── Section 8: Common bugs ──────────────────────────────────────────────── */
/*
 * This section demonstrates (safely) the most common pointer and memory
 * bugs.  Each one is explained but not triggered — dereferencing bad
 * pointers would crash the program.
 */
static void demo_common_bugs(void)
{
    print_divider("8. Common Pointer & Memory Bugs");

    /* Bug 1: Memory leak — forgetting to free. */
    SDL_Log("Bug 1: Memory Leak");
    SDL_Log("  float *data = SDL_malloc(1000 * sizeof(float));");
    SDL_Log("  // ... use data ...");
    SDL_Log("  // Forgot SDL_free(data)!");
    SDL_Log("  // The 4000 bytes are lost until the program exits.");
    SDL_Log("  Fix: always pair every SDL_malloc with an SDL_free.");

    /* Bug 2: Use-after-free — using memory after freeing it. */
    SDL_Log(" ");
    SDL_Log("Bug 2: Use-After-Free");
    SDL_Log("  int *p = SDL_malloc(sizeof(int));");
    SDL_Log("  *p = 42;");
    SDL_Log("  SDL_free(p);");
    SDL_Log("  // p still holds the old address, but the memory is freed.");
    SDL_Log("  // *p = 10;  <-- undefined behavior! May crash or corrupt data.");
    SDL_Log("  Fix: set p = NULL after freeing. Then *p would crash immediately");
    SDL_Log("       (which is easier to debug than silent corruption).");

    /* Demonstrate the fix pattern. */
    int *safe = (int *)SDL_malloc(sizeof(int));
    if (safe) {
        *safe = 42;
        SDL_free(safe);
        safe = NULL;  /* Prevents accidental use-after-free. */
        SDL_Log("  Example: safe = %p after free + NULL assignment",
                (void *)safe);
    }

    /* Bug 3: Double free — freeing the same pointer twice. */
    SDL_Log(" ");
    SDL_Log("Bug 3: Double Free");
    SDL_Log("  int *p = SDL_malloc(sizeof(int));");
    SDL_Log("  SDL_free(p);");
    SDL_Log("  SDL_free(p);  <-- undefined behavior! Heap corruption.");
    SDL_Log("  Fix: set p = NULL after free. SDL_free(NULL) is safe (no-op).");

    /* Bug 4: Wrong sizeof in allocation. */
    SDL_Log(" ");
    SDL_Log("Bug 4: Wrong sizeof");
    SDL_Log("  // Intended: allocate 10 ints");
    SDL_Log("  int *arr = SDL_malloc(10);  <-- allocates 10 BYTES, not 10 ints!");
    SDL_Log("  Fix: int *arr = SDL_malloc(10 * sizeof(int));");
    SDL_Log("       Or better: SDL_malloc(10 * sizeof(*arr));");
    SDL_Log("       Using sizeof(*arr) is safer — it stays correct even if");
    SDL_Log("       you change the type of arr.");

    /* Bug 5: Dangling pointer from returning stack address. */
    SDL_Log(" ");
    SDL_Log("Bug 5: Dangling Pointer (Stack)");
    SDL_Log("  int *bad_function(void) {");
    SDL_Log("      int local = 42;");
    SDL_Log("      return &local;  <-- local is destroyed when function returns!");
    SDL_Log("  }");
    SDL_Log("  // The returned pointer is dangling — it points to freed stack.");
    SDL_Log("  Fix: return a heap-allocated value, or use an output parameter.");

    /* Bug 6: Buffer overflow — writing past the end of an allocation. */
    SDL_Log(" ");
    SDL_Log("Bug 6: Buffer Overflow");
    SDL_Log("  int *arr = SDL_malloc(5 * sizeof(int));");
    SDL_Log("  arr[5] = 99;  <-- writes past the end of the allocation!");
    SDL_Log("  // May corrupt adjacent heap metadata or other allocations.");
    SDL_Log("  Fix: always check bounds. Keep track of the allocation size.");
}
