# Engine Lesson 09 — HLSL Shared Headers

Share constants, structs, and utility functions between HLSL shaders
using `.hlsli` header files — the same `#include` pattern you already
know from C.

## What you'll learn

- How `#include` works in HLSL (the same preprocessor directive as C)
- The `.hlsli` file extension convention for shared HLSL headers
- Include guards in HLSL (`#ifndef` / `#define` / `#endif`)
- The `-I` search path flag for `dxc` (the HLSL compiler)
- What belongs in a `.hlsli` file (constants, structs, utility functions)
- How HLSL's compilation model differs from C (no linker, no ODR)

## Why this matters

As shaders grow in complexity, you start sharing constants and functions
between them. GPU Lesson 26 (Procedural Sky) has two shaders that both
need the same horizon-fade constants:

- `sky.frag.hlsl` — the per-pixel atmosphere ray march
- `multiscatter_lut.comp.hlsl` — the multi-scattering LUT compute pass

Without a shared header, those constants are copy-pasted into each file.
Change one, forget the other, and the LUT and sky pass disagree — a
subtle, hard-to-find bug. The `.hlsli` shared header solves this the
same way `.h` files solve it in C: define once, include everywhere.

## Result

```text
INFO: === Engine Lesson 09: HLSL Shared Headers ===
INFO:
INFO: ------------------------------------------------------------
INFO:   1. The Problem: Duplicated Constants
INFO: ------------------------------------------------------------
INFO: Imagine two shaders that both need the same constants:
INFO:
INFO:   // sky.frag.hlsl
INFO:   static const float HORIZON_FADE_SCALE = 10.0;
INFO:   static const float HORIZON_FADE_BIAS  = 0.1;
INFO:
INFO:   // multiscatter_lut.comp.hlsl
INFO:   static const float HORIZON_FADE_SCALE = 10.0;  // copy-pasted!
INFO:   static const float HORIZON_FADE_BIAS  = 0.1;   // copy-pasted!
INFO:
INFO: If you change BIAS in one file but forget the other, the sky
INFO: pass and LUT compute disagree — a subtle, hard-to-find bug.
INFO:
INFO: ------------------------------------------------------------
INFO:   2. The Solution: A Shared Header
INFO: ------------------------------------------------------------
INFO: Define the constants once in a shared header:
INFO:
INFO:   // atmosphere_params.hlsli
INFO:   #ifndef ATMOSPHERE_PARAMS_HLSLI
INFO:   #define ATMOSPHERE_PARAMS_HLSLI
INFO:   static const float HORIZON_FADE_SCALE = 10.0;
INFO:   static const float HORIZON_FADE_BIAS  = 0.1;
INFO:   #endif
INFO:
INFO: Then each shader just includes it:
INFO:   #include "atmosphere_params.hlsli"
INFO:
INFO: This is the .hlsli pattern — HLSL's equivalent of a .h file.
INFO:
INFO: ------------------------------------------------------------
INFO:   3. Both Files Share the Same Constants
INFO: ------------------------------------------------------------
INFO: This C program mirrors that pattern.  main.c and sky_pass.c
INFO: both include shared_params.h:
INFO:
INFO:   main.c includes shared_params.h:
INFO:   main.c     sees HORIZON_FADE_SCALE = 10.0
INFO:   main.c     sees HORIZON_FADE_BIAS  = 0.1
INFO:
INFO:   sky_pass.c sees HORIZON_FADE_SCALE = 10.0
INFO:   sky_pass.c sees HORIZON_FADE_BIAS  = 0.1
INFO:
INFO: Both files see the same values — defined once, used everywhere.
INFO:
INFO: Using the shared constants (horizon fade formula):
INFO:   cos_zenith =  1.0 -> fade = 1.00  (looking up, fully lit)
INFO:   cos_zenith =  0.0 -> fade = 0.10  (horizon)
INFO:   cos_zenith = -0.5 -> fade = 0.00  (below horizon, shadowed)
INFO:
INFO: ------------------------------------------------------------
INFO:   4. How This Maps to HLSL
INFO: ------------------------------------------------------------
INFO: The C and HLSL patterns are nearly identical:
INFO:
INFO:   C header file:     shared_params.h
INFO:   HLSL header file:  atmosphere_params.hlsli
INFO:
INFO:   C include:         #include "shared_params.h"
INFO:   HLSL include:      #include "atmosphere_params.hlsli"
INFO:
INFO:   C include guard:   #ifndef SHARED_PARAMS_H
INFO:   HLSL include guard: #ifndef ATMOSPHERE_PARAMS_HLSLI
INFO:
INFO:   C search path:     -I directory  (gcc/clang/MSVC)
INFO:   HLSL search path:  -I directory  (dxc)
INFO:
INFO: The preprocessor directive #include works the same way in both
INFO: languages.  The -I flag tells the compiler where to search for
INFO: included files.  compile_shaders.py passes -I to dxc:
INFO:   dxc -spirv -I shaders/ -T ps_6_0 -E main sky.frag.hlsl
INFO:
INFO: ------------------------------------------------------------
INFO:   5. How HLSL Differs from C
INFO: ------------------------------------------------------------
INFO: HLSL has a simpler compilation model than C:
INFO:
INFO:   1. No linker step
INFO:      C:    main.c -> main.o -+-> executable (linker combines)
INFO:            sky_pass.c -> sky_pass.o -+
INFO:      HLSL: sky.frag.hlsl -> sky_frag.spv  (standalone)
INFO:            lut.comp.hlsl -> lut_comp.spv  (standalone)
INFO:      Each shader compiles independently to its own bytecode.
INFO:
INFO:   2. No ODR (one-definition rule) concerns with 'static const'
INFO:      The ODR says each symbol can have only one definition
INFO:      across all .o files the linker combines.
INFO:      In C, a non-static global in a header causes 'multiple
INFO:      definition' linker errors (see Engine Lesson 05).
INFO:      In HLSL, there is no linker, so 'static const' in a
INFO:      .hlsli just works — each shader gets its own copy.
INFO:
INFO:   3. Utility functions do not need 'static inline'
INFO:      In C headers, functions must be 'static inline' to avoid
INFO:      ODR violations.  In HLSL headers, plain functions work
INFO:      fine because each shader is compiled alone.
INFO:
INFO: ------------------------------------------------------------
INFO:   Summary
INFO: ------------------------------------------------------------
INFO: HLSL shared header checklist:
INFO:   [1] File extension:  .hlsli (convention, not enforced)
INFO:   [2] Include guard:   #ifndef NAME_HLSLI / #define / #endif
INFO:   [3] Include:         #include "name.hlsli"
INFO:   [4] Search path:     dxc -I shader_directory/
INFO:   [5] Contents:        constants, structs, utility functions
INFO:
INFO: The pattern is the same one you learned in Engine Lesson 05
INFO: for C header-only libraries.  The only difference is that HLSL
INFO: has no linker, so the rules are simpler — no ODR to worry about.
INFO:
INFO: === All sections complete ===
```

## Key concepts

- **`.hlsli`** — The conventional file extension for HLSL header files
  ("HLSL Include"). Not enforced by the compiler, but universally used
  to distinguish shared headers from compilable shader files
  (`.vert.hlsl`, `.frag.hlsl`, `.comp.hlsl`).

- **Include guard** — The `#ifndef` / `#define` / `#endif` pattern that
  prevents a header from being processed more than once in the same
  compilation. Works identically in C and HLSL.

- **Search path (`-I`)** — A compiler flag that tells `dxc` (or
  `gcc`/`clang`/`cl`) where to look for files referenced by `#include`.
  Without it, `#include "atmosphere_params.hlsli"` fails with
  "cannot open file."

- **Preprocessor directive** — A line beginning with `#` that is
  processed before compilation: `#include`, `#ifndef`, `#define`,
  `#endif`. The C and HLSL preprocessors share the same syntax.

## The details

### The duplication problem

Consider two shaders that both need horizon-fade constants:

```hlsl
// sky.frag.hlsl — per-pixel atmosphere ray march
static const float HORIZON_FADE_SCALE = 10.0;
static const float HORIZON_FADE_BIAS  = 0.1;

float earth_shadow(float cos_zenith) {
    return saturate(cos_zenith * HORIZON_FADE_SCALE + HORIZON_FADE_BIAS);
}
```

```hlsl
// multiscatter_lut.comp.hlsl — LUT precomputation
static const float HORIZON_FADE_SCALE = 10.0;  // copy-pasted
static const float HORIZON_FADE_BIAS  = 0.1;   // copy-pasted

float earth_shadow(float cos_zenith) {          // copy-pasted
    return saturate(cos_zenith * HORIZON_FADE_SCALE + HORIZON_FADE_BIAS);
}
```

This is the same problem that C headers solve: constants defined in
multiple places will eventually go out of sync. In shaders, the symptom
is typically a visual artifact that only appears under specific
conditions — because the fragment shader and compute shader disagree on
a value used in the same formula.

### The `.hlsli` solution

GPU Lesson 26 solves this with `atmosphere_params.hlsli`:

```hlsl
/* atmosphere_params.hlsli -- Shared atmosphere tuning parameters
 *
 * Included by both sky.frag.hlsl and multiscatter_lut.comp.hlsl so that
 * constants only need to be defined once. */

#ifndef ATMOSPHERE_PARAMS_HLSLI
#define ATMOSPHERE_PARAMS_HLSLI

static const float HORIZON_FADE_SCALE = 10.0;
static const float HORIZON_FADE_BIAS  = 0.1;

#endif /* ATMOSPHERE_PARAMS_HLSLI */
```

Each shader replaces its local constants with a single include:

```hlsl
#include "atmosphere_params.hlsli"

float earth_shadow(float cos_zenith) {
    return saturate(cos_zenith * HORIZON_FADE_SCALE + HORIZON_FADE_BIAS);
}
```

Now there is one source of truth. Change `HORIZON_FADE_BIAS` in the
`.hlsli` file, recompile both shaders, and they automatically agree.

### Include guards in HLSL

The `#ifndef` / `#define` / `#endif` pattern prevents double inclusion:

```hlsl
#ifndef ATMOSPHERE_PARAMS_HLSLI    /* 1. Is this symbol defined? */
#define ATMOSPHERE_PARAMS_HLSLI    /* 2. No — define it now      */

/* ... contents ...               */

#endif /* ATMOSPHERE_PARAMS_HLSLI */ /* 3. End of guarded block   */
```

If a shader includes the file twice (directly or through another
header), the second inclusion sees that `ATMOSPHERE_PARAMS_HLSLI` is
already defined and skips the entire block. Without the guard, the
compiler reports `redefinition of 'HORIZON_FADE_SCALE'`.

This is exactly the same mechanism as C include guards (see
[Engine Lesson 05](../05-header-only-libraries/)).

### The `-I` search path

When `dxc` encounters `#include "atmosphere_params.hlsli"`, it needs to
find that file on disk. The `-I` flag tells it where to search:

```bash
dxc -spirv -I shaders/ -T ps_6_0 -E main sky.frag.hlsl
```

This says: "when resolving `#include` directives, also look in the
`shaders/` directory." Without `-I`, `dxc` only searches relative to
the source file itself.

In this project, `compile_shaders.py` automatically passes
`-I shader_dir` for each lesson:

```python
# compile_shaders.py (simplified)
spirv_cmd = [
    dxc_path,
    "-spirv",
    "-I", shader_dir,     # <-- search path for .hlsli files
    "-T", profile,
    "-E", "main",
    shader_path,
]
```

This means any `.hlsli` file placed in a lesson's `shaders/` directory
is automatically findable by all shaders in that directory.

### What goes in a `.hlsli` file

A `.hlsli` file can contain anything you would put in a C header:

| Content | Example | Notes |
|---------|---------|-------|
| Constants | `static const float PI = 3.14159;` | Shared tuning parameters |
| Structs | `struct VertexOutput { float4 pos : SV_Position; };` | Shared between vertex and fragment |
| Utility functions | `float3 gamma_correct(float3 c) { ... }` | No `static inline` needed |
| `#define` macros | `#define NUM_STEPS 32` | Preprocessor constants |

**What does NOT go in a `.hlsli` file:**

- Entry points (`main` function with shader semantics)
- Resource bindings (`register(b0)`, `register(t0)`)
- Stage-specific code (vertex vs fragment logic)

### Side-by-side: C header vs HLSL header

| Feature | C (`.h`) | HLSL (`.hlsli`) |
|---------|----------|-----------------|
| Include directive | `#include "file.h"` | `#include "file.hlsli"` |
| Include guard | `#ifndef FILE_H` | `#ifndef FILE_HLSLI` |
| Constants | `static const float X = 1.0f;` | `static const float X = 1.0;` |
| Functions | `static inline float f(...)` | `float f(...)` |
| Structs | `typedef struct { ... } Name;` | `struct Name { ... };` |
| Search path | `-I dir` (compiler flag) | `-I dir` (dxc flag) |

The preprocessor syntax is identical. The differences are in the
language features (HLSL struct syntax, no `typedef` needed) and the
compilation model.

### How HLSL differs from C

The key difference is that **HLSL has no linker.** Each shader file
compiles independently to its own bytecode (SPIR-V or DXIL):

```text
C compilation:
  main.c      ──compile──> main.o    ──┐
  sky_pass.c  ──compile──> sky_pass.o──┤──link──> program
  libSDL3.a   ─────────────────────────┘

HLSL compilation:
  sky.frag.hlsl  ──compile──> sky_frag.spv     (standalone)
  lut.comp.hlsl  ──compile──> lut_comp.spv     (standalone)
```

This has two practical consequences:

1. **`static const` in HLSL headers has no ODR issue.** The
   one-definition rule (ODR) says each symbol can have only one
   definition across all `.o` files the linker combines. In C, putting
   a non-`static` variable in a header causes "multiple definition"
   linker errors when two `.c` files include it (Engine Lesson 05). In
   HLSL, there is no linker, so each shader simply gets its own copy
   of the constant.

2. **Functions in HLSL headers do not need `static inline`.** In C, a
   function defined in a header must be `static inline` to avoid ODR
   violations. In HLSL, a plain function in a `.hlsli` works fine
   because no linker ever sees two copies.

The simpler model means fewer things can go wrong — but you still need
include guards to prevent redefinition within a single shader file.

## Common errors

### `fatal error: cannot open file 'atmosphere_params.hlsli'`

The compiler cannot find the included file. This usually means the `-I`
search path is missing.

**Fix:** Add `-I shaders/` (or whatever directory contains the `.hlsli`
file) to the `dxc` command. In this project, `compile_shaders.py`
handles this automatically.

### `error: redefinition of 'HORIZON_FADE_SCALE'`

The header was included twice without an include guard.

**Fix:** Add the standard `#ifndef` / `#define` / `#endif` guard:

```hlsl
#ifndef MY_HEADER_HLSLI
#define MY_HEADER_HLSLI
/* ... contents ... */
#endif
```

### Constants out of sync between shaders

Two shaders define the same constant with different values. One shader
produces correct results; the other produces subtle visual artifacts.

**Fix:** Move the constant into a shared `.hlsli` file and `#include`
it in both shaders. This is the entire reason shared headers exist.

## Where it's used

- **GPU Lesson 26 — Procedural Sky:**
  [`atmosphere_params.hlsli`](../../gpu/26-procedural-sky/shaders/atmosphere_params.hlsli)
  shares horizon-fade constants between `sky.frag.hlsl` and
  `multiscatter_lut.comp.hlsl`.

- **`scripts/compile_shaders.py`:** Automatically passes `-I shader_dir`
  to `dxc`, so `.hlsli` files in each lesson's `shaders/` directory are
  found without manual configuration.

- **Engine Lesson 05 — Header-Only Libraries:** The C counterpart of
  this lesson. Covers include guards, `static inline`, and the
  one-definition rule in detail.

## Building

```bash
cmake -B build
cmake --build build --config Debug --target 09-hlsl-shared-headers
```

Run the program:

```bash
# Windows
build\lessons\engine\09-hlsl-shared-headers\Debug\09-hlsl-shared-headers.exe

# Linux / macOS
./build/lessons/engine/09-hlsl-shared-headers/09-hlsl-shared-headers
```

## Exercises

1. **Add a utility function to the shared header.** Add an
   `earth_shadow` function to `shared_params.h` that computes
   `saturate(cos_zenith * SCALE + BIAS)`. Call it from both `main.c`
   and `sky_pass.c`. Remember: in C you need `static inline`, but in
   HLSL you would not.

2. **Create a second `.hlsli` file.** Write a `math_constants.hlsli`
   with `PI` and `TWO_PI`. Add an include guard. Include it from an
   existing shader and verify it compiles with
   `python scripts/compile_shaders.py`.

3. **Break the include guard.** Remove the `#ifndef` / `#define` /
   `#endif` from `shared_params.h` and include it twice from `main.c`.
   Read the compiler error. Then restore the guard and verify the error
   disappears.

4. **Compile HLSL with and without `-I`.** Try compiling a shader that
   uses `#include` without the `-I` flag and observe the error message.
   Then add `-I shaders/` and confirm it compiles successfully.

## Further reading

- [Engine Lesson 05 — Header-Only Libraries](../05-header-only-libraries/)
  — The C counterpart: include guards, `static inline`, and the ODR
- [GPU Lesson 26 — Procedural Sky](../../gpu/26-procedural-sky/)
  — Real-world use of `atmosphere_params.hlsli`
- [DXC — DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler)
  — The HLSL compiler used by this project
- [HLSL Preprocessor](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-appendix-pre-if)
  — Microsoft's documentation on `#include`, `#define`, and `#if` in HLSL
