# forge-gpu Math Library Design

## Overview

A header-only C math library for graphics and game programming, written to be
learned from. Every function is documented, every design decision explained.

**Target audience:** Both humans learning graphics math AND AI agents building
with it via skills.

## Design Principles

1. **Readability over performance** — This is teaching code
   - Clear function names, no cryptic abbreviations
   - Inline comments explaining the math, not just the code
   - Simple implementations that match textbook descriptions

2. **Header-only** — Easy to integrate, matches `forge.h` pattern
   - Single `forge_math.h` file (may split later if it grows)
   - No build configuration needed
   - Just `#include "math/forge_math.h"`

3. **C99, SDL conventions** — Consistent with the rest of forge-gpu
   - Prefix all public names: `vec2`, `vec3`, `mat4`, etc.
   - Functions: `vec3_add`, `mat4_rotate_z`, etc.
   - Lowercase with underscores (matching SDL's internal style)

4. **Well-documented** — Every function explains what, why, and where it's used
   - Usage examples in header comments
   - Links to corresponding math lessons
   - Geometric intuition, not just formulas

5. **No SDL dependencies** — Math library is standalone
   - Works in any C project, not just SDL GPU
   - Pure math, no graphics API coupling
   - Can be copied and used anywhere

## Directory Structure

```text
common/math/
├── DESIGN.md         # This file — design decisions and conventions
├── README.md         # User-facing: how to use the library
└── forge_math.h      # The library itself (header-only)
```

## Coordinate System

**Right-handed, Y-up** — Standard for most 3D graphics and game engines.

```text
     +Y (up)
      |
      |
      +----> +X (right)
     /
    /
  +Z (forward, toward camera)
```

**Why right-handed?**

- Matches OpenGL, Vulkan, and most game engines
- Cross product follows right-hand rule: `X × Y = Z`
- Intuitive for world space: X is right, Y is up, Z is forward

**Why Y-up?**

- Industry standard (Maya, Blender, Unity, Unreal)
- Z-up (e.g., CAD software) is less common in real-time graphics

**Important:** SDL GPU uses bottom-left origin for NDC (Normalized Device Coordinates):

- X: -1 (left) to +1 (right)
- Y: -1 (bottom) to +1 (top)
- Z: 0 (near) to 1 (far) — matches Vulkan/D3D12, NOT OpenGL

Our world space is Y-up, but NDC has its own conventions (this is normal in graphics).

## Matrix Layout

**Column-major storage, column-major math** — Matches HLSL, GLSL, and mathematical notation.

### Storage

A `mat4` stores 16 floats in **column-major** order in memory:

```c
float m[16] = {
    m0,  m1,  m2,  m3,   // column 0 (index 0–3)
    m4,  m5,  m6,  m7,   // column 1 (index 4–7)
    m8,  m9,  m10, m11,  // column 2 (index 8–11)
    m12, m13, m14, m15   // column 3 (index 12–15)
};
```

As a matrix (mathematical notation):

```text
| m0  m4  m8   m12 |   // row 0
| m1  m5  m9   m13 |   // row 1
| m2  m6  m10  m14 |   // row 2
| m3  m7  m11  m15 |   // row 3
```

**For a transform matrix:**

- Column 0 (m[0-3]): X-axis direction + scale
- Column 1 (m[4-7]): Y-axis direction + scale
- Column 2 (m[8-11]): Z-axis direction + scale
- Column 3 (m[12-15]): Translation (position)

### Multiplication

**Column-major math:** `v' = M * v`

```c
vec4 mat4_multiply_vec4(mat4 m, vec4 v) {
    // Each component is a dot product of a matrix row with the vector
    return vec4_create(
        m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z  + m.m[12]*v.w,  // x'
        m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z  + m.m[13]*v.w,  // y'
        m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]*v.w,  // z'
        m.m[3]*v.x + m.m[7]*v.y + m.m[11]*v.z + m.m[15]*v.w   // w'
    );
}
```

**Matrix multiplication:** `C = A * B` means "apply B first, then A"

```c
mat4 mat4_multiply(mat4 a, mat4 b) {
    // Result[col][row] = sum over k of A[k][row] * B[col][k]
    // (Standard matrix multiplication, column-major storage)
}
```

### Why Column-Major?

**Advantages:**

1. **Matches HLSL** — Our shader language uses column-major by default
2. **Matches math notation** — `v' = M * v` reads naturally
3. **Efficient for GPU** — Matrices are often uploaded as-is to shaders
4. **Cache-friendly for matrix×vector** — Reading columns is sequential

(Note: GLSL also uses column-major, so this layout is widely compatible.)

**Caveat:** C programmers often expect row-major (C's multidimensional arrays
are row-major). We accept this trade-off for shader compatibility.

## Winding Order and Culling

**Counter-clockwise (CCW) front faces** — When viewed from the camera, triangles
with vertices in counter-clockwise order are facing toward you.

```text
Looking at a triangle from the front:
    v0
    /\
   /  \
  /____\
v1      v2

Vertices v0→v1→v2 go counter-clockwise → front-facing
Vertices v0→v2→v1 go clockwise → back-facing
```

**Why CCW?**

- SDL GPU default: `SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE`
- OpenGL convention
- Right-hand rule: curl fingers in vertex order, thumb points toward camera = front

**Backface culling:** When enabled, back-facing triangles are discarded before
rasterization (common optimization for closed meshes where you never see the inside).

In forge-gpu lessons, we typically use `SDL_GPU_CULLMODE_NONE` for 2D content
(triangles, quads) where both sides are visible. For 3D models, we'll enable
`SDL_GPU_CULLMODE_BACK` and ensure meshes are modeled with CCW winding.

## Initial Type Set

Based on current lesson needs (Lessons 02-03):

### Core types

```c
typedef struct vec2 { float x, y; } vec2;    /* HLSL: float2 */
typedef struct vec3 { float x, y, z; } vec3; /* HLSL: float3 */
typedef struct vec4 { float x, y, z, w; } vec4; /* HLSL: float4 */

typedef struct mat4 {
    float m[16];  /* Column-major storage (see Matrix Layout above) */
} mat4; /* HLSL: float4x4 */
```

**Type naming:** We use `vec2/vec3/vec4` instead of HLSL's `float2/float3/float4`
because:

1. Standard in C math libraries (GLM, cglm, etc.)
2. Portable across shader languages (HLSL, GLSL, Metal Shading Language)
3. Clear type names in C (no confusion with C's `float` primitive)
4. Math library is independent of graphics API

The mapping to HLSL is straightforward and well-documented. Memory layout is
identical, so data can be uploaded to shaders without conversion.

### Constructors

```c
vec2 vec2_create(float x, float y);
vec3 vec3_create(float x, float y, float z);
vec4 vec4_create(float x, float y, float z, float w);
mat4 mat4_identity(void);
```

### Vector operations (Lesson: lessons/math/01-vectors)

```c
/* Addition, subtraction, scaling */
vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_scale(vec3 v, float s);

/* Length and normalization */
float vec3_length(vec3 v);
float vec3_length_squared(vec3 v);  /* Faster, avoids sqrt */
vec3  vec3_normalize(vec3 v);

/* Products */
float vec3_dot(vec3 a, vec3 b);
vec3  vec3_cross(vec3 a, vec3 b);

/* Utilities */
vec3 vec3_lerp(vec3 a, vec3 b, float t);  /* Linear interpolation */
```

Similar functions for `vec2` and `vec4`.

### Matrix operations (Lesson: lessons/math/02-coordinate-spaces)

```c
/* Basic operations */
mat4 mat4_multiply(mat4 a, mat4 b);
vec4 mat4_multiply_vec4(mat4 m, vec4 v);

/* Transformations */
mat4 mat4_translate(vec3 translation);
mat4 mat4_scale(vec3 scale);
mat4 mat4_rotate_x(float radians);
mat4 mat4_rotate_y(float radians);
mat4 mat4_rotate_z(float radians);

/* Common matrices for graphics */
mat4 mat4_perspective(float fov_y, float aspect, float near, float far);
mat4 mat4_look_at(vec3 eye, vec3 target, vec3 up);

/* Projection (Lesson: lessons/math/06-projections) */
mat4 mat4_orthographic(float left, float right, float bottom, float top, float near, float far);
```

## Naming Conventions

| Pattern | Example | Meaning |
|---------|---------|---------|
| `typeN_verb` | `vec3_add` | Operation on type (N = dimension) |
| `typeN_create` | `vec2_create` | Constructor |
| `typeN_property` | `vec3_length` | Query a property |
| `mat4_verb_noun` | `mat4_rotate_z` | Matrix operation with qualifier |

**No abbreviations** except standard math terms:

- `vec` (vector), `mat` (matrix) — universally understood
- `lerp` (linear interpolation) — industry standard
- Otherwise spell it out: `normalize` not `norm`, `multiply` not `mul`

## Documentation Format

Every function gets:

1. **Summary** — One sentence explaining what it does
2. **Parameters** — What each parameter means
3. **Returns** — What the result represents
4. **Usage example** — Concrete code snippet
5. **Math lesson reference** — Link to the lesson teaching this concept
6. **Geometric intuition** — What this means visually (when applicable)

Example:

```c
/* Compute the dot product of two vectors.
 *
 * The dot product measures how much two vectors point in the same direction.
 * If the result is:
 *   - Positive: vectors point somewhat in the same direction
 *   - Zero:     vectors are perpendicular
 *   - Negative: vectors point in opposite directions
 *
 * Magnitude: |a| * |b| * cos(θ), where θ is the angle between a and b.
 *
 * Usage:
 *   vec3 a = vec3_create(1.0f, 0.0f, 0.0f);
 *   vec3 b = vec3_create(0.0f, 1.0f, 0.0f);
 *   float d = vec3_dot(a, b);  // 0.0 — perpendicular
 *
 * See: lessons/math/01-vectors for a detailed explanation and demo.
 */
static inline float vec3_dot(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
```

## Integration with Lessons

### GPU lessons use the math library

- Lesson 02 (First Triangle) → use `vec2_create` for positions, `vec3_create` for colors
- Lesson 03 (Uniforms & Motion) → use `mat4_rotate_z` for rotation
- Future lessons → always prefer math library over bespoke code

### Math lessons teach and extend the library

- Each math lesson demonstrates one concept (dot product, rotation, etc.)
- The lesson includes a small program showing the math in action
- The lesson also **adds the implementation to `forge_math.h`**
- Cross-references: math lesson → GPU lessons using it, GPU lesson → math lesson explaining it

Use `/math-lesson` skill to create a new math lesson + update library.

## Constants

Provide commonly used constants:

```c
#define FORGE_PI      3.14159265358979323846f
#define FORGE_TAU     6.28318530717958647692f  /* 2π, full circle */
#define FORGE_DEG2RAD 0.01745329251994329576f  /* π/180 */
#define FORGE_RAD2DEG 57.2957795130823208768f  /* 180/π */
```

Prefix with `FORGE_` to avoid collisions with system headers.

## What NOT to Include (Initially)

Keep the library focused. Don't add:

- Quaternions (until we have a lesson teaching them)
- Complex transforms (compose them from primitives)
- SIMD/optimization (readability first, optimize in real projects if needed)
- Floating-point utilities beyond basic math (keep it pure linear algebra)

Add features when lessons need them, not speculatively.

## Testing Strategy

Each math lesson includes a program that:

- Demonstrates the concept visually or numerically
- Provides examples for users to understand usage

Additionally, there is an automated test suite in `tests/math/` covering all vector and matrix operations. These tests verify correctness and are integrated with CTest for CI/CD. Run tests with:

```bash
ctest -C Debug --output-on-failure
# Or directly: build/tests/math/Debug/test_math.exe
```

Math lessons serve as examples and demos, while the automated unit and integration tests ensure the library works correctly.

## Evolution Path

As the project grows:

1. **More types** — `quat` (quaternions), `plane`, `ray`, etc.
2. **More operations** — As lessons need them
3. **Potential split** — If `forge_math.h` exceeds ~1000 lines, split into:
   - `forge_math.h` — Include-all header
   - `forge_vec.h` — Vector operations
   - `forge_mat.h` — Matrix operations
   - `forge_quat.h` — Quaternions
   - etc.

But start simple: one header file, clear and documented.

## License

Same as the rest of forge-gpu: **zlib** (matches SDL).

---

**Next steps:**

1. Create `README.md` explaining how to use the library
2. Implement `forge_math.h` with initial types and operations
3. Create math lessons for vec2/vec3 and matrices
4. Refactor existing GPU lessons to use the math library
