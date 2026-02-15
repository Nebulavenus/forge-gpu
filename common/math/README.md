# forge-gpu Math Library

A learning-focused math library for graphics and game programming.

## Quick Start

```c
#include "math/forge_math.h"

// Create vectors
vec3 position = vec3_create(0.0f, 1.0f, 0.0f);
vec3 velocity = vec3_create(1.0f, 0.0f, 0.0f);

// Add vectors
vec3 new_position = vec3_add(position, velocity);

// Create a rotation matrix
mat4 rotation = mat4_rotate_z(FORGE_PI / 4.0f);  // 45° rotation

// Transform a vector
vec4 v = vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
vec4 rotated = mat4_multiply_vec4(rotation, v);
```

## What's Included

### Types

- **`vec2`** — 2D vectors (x, y) — maps to HLSL `float2`
- **`vec3`** — 3D vectors (x, y, z) — maps to HLSL `float3`
- **`vec4`** — 4D vectors (x, y, z, w) — maps to HLSL `float4`
- **`mat3`** — 3×3 matrices (column-major) — maps to HLSL `float3x3`
- **`mat4`** — 4×4 matrices (column-major) — maps to HLSL `float4x4`

**Note on naming:** We use `vec2/vec3/vec4` instead of HLSL's `float2/float3/float4`
to keep the math library portable and follow C math library conventions. The mapping
is straightforward: when passing data to HLSL shaders, `vec3` in C becomes `float3`
in the shader.

### Scalar Helpers

- **Interpolation:** `forge_lerpf(a, b, t)` — scalar lerp
- **Bilinear interpolation:** `forge_bilerpf(c00, c10, c01, c11, tx, ty)` — blend 4 grid values
- **Logarithm:** `forge_log2f(x)` — base-2 logarithm (mip level count)
- **Clamping:** `forge_clampf(x, lo, hi)` — clamp scalar to range
- **Trilinear interpolation:** `forge_trilerpf(c000..c111, tx, ty, tz)` — blend 8 cube corners

### Vector Operations

Each vector type supports:

- **Construction:** `vec3_create(x, y, z)`
- **Arithmetic:** `add`, `sub`, `scale`
- **Dot product:** `vec3_dot(a, b)`
- **Length:** `vec3_length(v)`, `vec3_length_squared(v)`
- **Normalization:** `vec3_normalize(v)`
- **Interpolation:** `vec3_lerp(a, b, t)`
- **Bilinear interpolation:** `vec3_bilerp(...)`, `vec4_bilerp(...)` — blend 4 grid values in 2D
- **Trilinear interpolation:** `vec3_trilerp(...)`, `vec4_trilerp(...)` — blend 8 cube corners in 3D
- **Cross product:** `vec3_cross(a, b)` (3D only)

### mat3 Operations

- **Construction:** `mat3_create(9 floats)` (row-major input), `mat3_identity()`
- **Multiplication:** `mat3_multiply(a, b)`, `mat3_multiply_vec3(m, v)`
- **Analysis:** `mat3_transpose(m)`, `mat3_determinant(m)`, `mat3_inverse(m)`
- **2D Transforms:** `mat3_rotate(angle)`, `mat3_scale(vec2)`

### mat4 Operations

- **Construction:** `mat4_identity()`, `mat4_from_mat3(m)`
- **Multiplication:** `mat4_multiply(a, b)`, `mat4_multiply_vec4(m, v)`
- **Analysis:** `mat4_transpose(m)`, `mat4_determinant(m)`, `mat4_inverse(m)`
- **Transformations:**
  - `mat4_translate(offset)`
  - `mat4_scale(scale)`, `mat4_scale_uniform(s)`
  - `mat4_rotate_x/y/z(angle_radians)`
- **Projection:**
  - `mat4_perspective(fov, aspect, near, far)` — Perspective projection (3D scenes)
  - `mat4_orthographic(l, r, b, t, near, far)` — Orthographic projection (2D, shadow maps)
- **Camera:**
  - `mat4_look_at(eye, target, up)` — View matrix from camera parameters

### Constants

- `FORGE_PI` — π (3.14159...)
- `FORGE_TAU` — 2π (6.28318...)
- `FORGE_DEG2RAD` — Convert degrees to radians
- `FORGE_RAD2DEG` — Convert radians to degrees

## Coordinate System

**Right-handed, Y-up:**

```text
     +Y (up)
      |
      |
      +----> +X (right)
     /
    /
  +Z (forward, toward camera)
```

**Winding order:** Counter-clockwise (CCW) for front-facing triangles.

**Matrix layout:** Column-major storage and math (matches HLSL shaders).

See [DESIGN.md](DESIGN.md) for detailed explanations.

## Learning Resources

Every function in `forge_math.h` includes:

- Clear documentation
- Usage examples
- Geometric intuition
- References to math lessons

### Math Lessons

Standalone programs teaching each concept in depth:

- `lessons/math/01-vectors/` — Vectors, dot product, cross product, normalization
- `lessons/math/02-coordinate-spaces/` — Coordinate spaces, view and projection matrices
- `lessons/math/03-orthographic-projection/` — Orthographic vs perspective projection
- `lessons/math/04-bilinear-interpolation/` — Bilinear interpolation, LINEAR texture filtering
- `lessons/math/05-mipmaps-and-lod/` — Mip chains, trilinear interpolation, LOD selection
- `lessons/math/06-matrices/` — Matrix math, multiplication, basis vectors, transpose, determinant, inverse

Each lesson includes a demo program and README explaining the math.

### Where It's Used

See how these functions are used in real code:

- `lessons/gpu/02-first-triangle/` — Uses `vec2` and `vec3` for vertices and colors
- `lessons/gpu/03-uniforms-and-motion/` — Uses `mat4_rotate_z` for animation
- `lessons/gpu/05-mipmaps/` — Uses `forge_log2f` for mip level count

## Design Philosophy

1. **Readability over performance** — This code is meant to be learned from
2. **Header-only** — Just include `forge_math.h`, no build config needed
3. **Well-documented** — Every function explains what, why, and where it's used
4. **No dependencies** — Works in any C project, not just SDL GPU
5. **Extensible** — Add new operations via `/math-lesson` skill

## Adding New Math

Need a function that doesn't exist yet?

1. **Check the lessons** — The concept might already be implemented
2. **Use `/math-lesson` skill** — Creates a lesson + updates the library
3. **Or add manually** — Follow the documentation style in `forge_math.h`

Every new math operation should have:

- A clear use case (which lesson/project needs it?)
- A corresponding lesson teaching the concept
- Comprehensive documentation with examples

## HLSL Shader Integration

Our C types map directly to HLSL types when uploading data to shaders:

| C Type | HLSL Type | Memory Layout |
|--------|-----------|---------------|
| `vec2` | `float2` | 8 bytes (2 floats) |
| `vec3` | `float3` | 12 bytes (3 floats) |
| `vec4` | `float4` | 16 bytes (4 floats) |
| `mat3` | `float3x3` | 36 bytes (9 floats, column-major) |
| `mat4` | `float4x4` | 64 bytes (16 floats, column-major) |

**Example: Vertex struct in C and HLSL**

C code (CPU side):

```c
typedef struct Vertex {
    vec2 position;  // 8 bytes
    vec3 color;     // 12 bytes
} Vertex;

Vertex v = {
    .position = vec2_create(0.5f, 0.5f),
    .color = vec3_create(1.0f, 0.0f, 0.0f)
};
```

HLSL shader (GPU side):

```hlsl
struct VSInput {
    float2 position : TEXCOORD0;
    float3 color    : TEXCOORD1;
};
```

The memory layout matches perfectly — no conversion needed when uploading to the GPU.

## Examples

### Creating and transforming a point

```c
// Create a point at the origin
vec4 point = vec4_create(0.0f, 0.0f, 0.0f, 1.0f);

// Move it right by 5 units
mat4 translation = mat4_translate(vec3_create(5.0f, 0.0f, 0.0f));
point = mat4_multiply_vec4(translation, point);  // (5, 0, 0, 1)

// Rotate 90° around Z axis
mat4 rotation = mat4_rotate_z(FORGE_PI / 2.0f);
point = mat4_multiply_vec4(rotation, point);  // (0, 5, 0, 1)
```

### Normalizing a direction vector

```c
vec3 direction = vec3_create(3.0f, 4.0f, 0.0f);
float length = vec3_length(direction);  // 5.0

vec3 normalized = vec3_normalize(direction);  // (0.6, 0.8, 0.0)
// Now normalized has length 1.0 and points in the same direction
```

### Building a coordinate frame

```c
// Given an up vector and a forward direction, compute right
vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
vec3 forward = vec3_create(0.0f, 0.0f, 1.0f);
vec3 right = vec3_cross(up, forward);  // (1, 0, 0)

// The cross product is perpendicular to both input vectors
```

## License

[zlib](../../LICENSE) — Same as SDL and the rest of forge-gpu.
