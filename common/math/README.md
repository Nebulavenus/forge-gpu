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
- **`mat2`** — 2×2 matrices (column-major) — Jacobian analysis, anisotropy
- **`mat3`** — 3×3 matrices (column-major) — maps to HLSL `float3x3`
- **`mat4`** — 4×4 matrices (column-major) — maps to HLSL `float4x4`
- **`quat`** — Quaternions (w, x, y, z) for 3D rotations — pass as HLSL `float4`

**Note on naming:** We use `vec2/vec3/vec4` instead of HLSL's `float2/float3/float4`
to keep the math library portable and follow C math library conventions. The mapping
is straightforward: when passing data to HLSL shaders, `vec3` in C becomes `float3`
in the shader.

### Scalar Helpers

- **Interpolation:** `forge_lerpf(a, b, t)` — scalar lerp
- **Bilinear interpolation:** `forge_bilerpf(c00, c10, c01, c11, tx, ty)` — blend 4 grid values
- **Trilinear interpolation:** `forge_trilerpf(c000..c111, tx, ty, tz)` — blend 8 cube corners
- **Logarithm:** `forge_log2f(x)` — base-2 logarithm (mip level count)
- **Trig wrappers:** `forge_sinf(x)`, `forge_cosf(x)` — thin wrappers for sinf/cosf
- **Clamping:** `forge_clampf(x, lo, hi)` — clamp scalar to range
- **Comparison:** `forge_approx_equalf(a, b, tolerance)` — absolute tolerance,
  `forge_rel_equalf(a, b, tolerance)` — relative tolerance

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
- **Negate:** `vec3_negate(v)` (3D)
- **Reflect:** `vec3_reflect(incident, normal)` — reflect vector about a surface normal (3D)

### mat2 Operations

- **Construction:** `mat2_create(4 floats)` (row-major input), `mat2_identity()`
- **Multiplication:** `mat2_multiply(a, b)`, `mat2_multiply_vec2(m, v)`
- **Analysis:** `mat2_transpose(m)`, `mat2_determinant(m)`
- **Anisotropy:** `mat2_singular_values(m)` — pixel footprint axes, `mat2_anisotropy_ratio(m)`

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
  - `mat4_perspective(fov, aspect, near, far)` — Symmetric perspective projection (3D scenes)
  - `mat4_perspective_from_planes(l, r, b, t, near, far)` — Asymmetric perspective (VR, multi-monitor)
  - `mat4_orthographic(l, r, b, t, near, far)` — Orthographic projection (2D, shadow maps)
  - `vec3_perspective_divide(clip)` — Clip-space vec4 → NDC vec3 (divide by w)
- **Camera:**
  - `mat4_look_at(eye, target, up)` — View matrix from camera parameters

### Quaternion Operations

- **Construction:** `quat_create(w, x, y, z)`, `quat_identity()`
- **Properties:** `quat_dot(a, b)`, `quat_length(q)`, `quat_length_sq(q)`
- **Operations:** `quat_normalize(q)`, `quat_conjugate(q)`, `quat_inverse(q)`, `quat_negate(q)`
- **Composition:** `quat_multiply(a, b)` — compose rotations (apply b first)
- **Rotation:** `quat_rotate_vec3(q, v)` — rotate vector by quaternion
- **Conversions:**
  - `quat_from_axis_angle(axis, angle)` / `quat_to_axis_angle(q, &axis, &angle)`
  - `quat_from_euler(yaw, pitch, roll)` / `quat_to_euler(q)` — intrinsic Y-X-Z order
  - `quat_to_mat4(q)` / `quat_from_mat4(m)` — quaternion to/from rotation matrix
- **Interpolation:** `quat_slerp(a, b, t)`, `quat_nlerp(a, b, t)`
- **Direction extraction:** `quat_forward(q)`, `quat_right(q)`, `quat_up(q)` — extract basis vectors
- **View matrix:** `mat4_view_from_quat(position, orientation)` — camera view from quaternion
- **Rodrigues:** `vec3_rotate_axis_angle(v, axis, angle)` — rotate vector around arbitrary axis

### Color Space Transforms

Functions for converting between color spaces and applying tone mapping:

- **sRGB ↔ linear:** `color_srgb_to_linear(s)`, `color_linear_to_srgb(linear)`,
  `color_srgb_to_linear_rgb(srgb)`, `color_linear_to_srgb_rgb(linear)`
- **Luminance:** `color_luminance(linear_rgb)` — BT.709 relative luminance
- **HSL ↔ RGB:** `color_rgb_to_hsl(rgb)`, `color_hsl_to_rgb(hsl)`
- **HSV ↔ RGB:** `color_rgb_to_hsv(rgb)`, `color_hsv_to_rgb(hsv)`
- **CIE XYZ ↔ RGB:** `color_linear_rgb_to_xyz(rgb)`, `color_xyz_to_linear_rgb(xyz)`
- **CIE xyY ↔ XYZ:** `color_xyz_to_xyY(xyz)`, `color_xyY_to_xyz(xyY)`
- **Tone mapping:** `color_tonemap_reinhard(hdr)`, `color_tonemap_aces(hdr)`
- **Exposure:** `color_apply_exposure(hdr, exposure_ev)`

### Hash Functions

Integer hash functions for procedural generation, noise, and sampling:

- **Single-value:** `forge_hash_wang(key)`, `forge_hash_pcg(input)`, `forge_hash_xxhash32(h)`
- **Combining:** `forge_hash_combine(seed, value)` — boost-style hash combine
- **Coordinate hashing:** `forge_hash2d(x, y)`, `forge_hash3d(x, y, z)`
- **Hash to float:** `forge_hash_to_float(h)` → `[0, 1)`,
  `forge_hash_to_sfloat(h)` → `[-1, 1)`

### Gradient Noise

Perlin, simplex, and fractal noise for procedural content:

- **Fade curve:** `forge_noise_fade(t)` — quintic smoothstep
- **Gradient helpers:** `forge_noise_grad1d(hash, dx)`, `forge_noise_grad2d(hash, dx, dy)`,
  `forge_noise_grad3d(hash, dx, dy, dz)`
- **Perlin noise:** `forge_noise_perlin1d(x, seed)`, `forge_noise_perlin2d(x, y, seed)`,
  `forge_noise_perlin3d(x, y, z, seed)`
- **Simplex noise:** `forge_noise_simplex2d(x, y, seed)`
- **Fractal Brownian motion:** `forge_noise_fbm2d(x, y, seed, octaves, lacunarity, persistence)`,
  `forge_noise_fbm3d(x, y, z, seed, octaves, lacunarity, persistence)`
- **Domain warping:** `forge_noise_domain_warp2d(x, y, seed, warp_strength)`

### Low-Discrepancy Sequences

Quasi-random and blue noise sequences for sampling:

- **Halton:** `forge_halton(index, base)` — radical inverse sequence
- **R-sequences:** `forge_r1(index)`, `forge_r2(index, out_x, out_y)` — additive
  recurrence sequences
- **Sobol:** `forge_sobol_2d(index, out_x, out_y)` — 2D Sobol sequence
- **Blue noise:** `forge_blue_noise_2d(out_x, out_y, count, candidates, seed)` —
  Mitchell's best candidate algorithm
- **Measurement:** `forge_star_discrepancy_2d(xs, ys, count)` — star discrepancy

### Bezier Curves

Quadratic and cubic Bezier curve evaluation and utilities:

- **Evaluation:** `vec2_bezier_quadratic(p0, p1, p2, t)`,
  `vec2_bezier_cubic(p0, p1, p2, p3, t)` (also `vec3` variants)
- **Tangents:** `vec2_bezier_quadratic_tangent(...)`, `vec2_bezier_cubic_tangent(...)`
  (also `vec3` variants)
- **Arc length:** `vec2_bezier_quadratic_length(...)`, `vec2_bezier_cubic_length(...)`
- **Splitting:** `vec2_bezier_quadratic_split(...)`, `vec2_bezier_cubic_split(...)`
- **Degree elevation:** `vec2_bezier_quadratic_to_cubic(...)` — convert quadratic to cubic
- **Flatness test:** `vec2_bezier_quadratic_is_flat(...)`, `vec2_bezier_cubic_is_flat(...)`
- **Adaptive flattening:** `vec2_bezier_quadratic_flatten(...)`,
  `vec2_bezier_cubic_flatten(...)` — convert curves to line segments

### Constants

- `FORGE_PI` — π (3.14159...)
- `FORGE_TAU` — 2π (6.28318...)
- `FORGE_DEG2RAD` — Convert degrees to radians
- `FORGE_RAD2DEG` — Convert radians to degrees
- `FORGE_EPSILON` — Machine epsilon for 32-bit float (1.192e-7)
- `FORGE_BEZIER_DEGENERATE_EPSILON` — Degenerate Bezier threshold (1e-12)

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
- `lessons/math/03-bilinear-interpolation/` — Bilinear interpolation, LINEAR texture filtering
- `lessons/math/04-mipmaps-and-lod/` — Mip chains, trilinear interpolation, LOD selection
- `lessons/math/05-matrices/` — Matrix math, multiplication, basis vectors, transpose, determinant, inverse
- `lessons/math/06-projections/` — Perspective, orthographic, frustums, clip space, NDC
- `lessons/math/07-floating-point/` — IEEE 754, precision, epsilon, z-fighting
- `lessons/math/08-orientation/` — Quaternions, Euler angles, axis-angle, rotation matrices, slerp
- `lessons/math/09-view-matrix/` — Camera as inverse transform, look-at, MVP pipeline
- `lessons/math/10-anisotropy/` — Jacobian, singular values, anisotropic filtering, noise, friction
- `lessons/math/11-color-spaces/` — sRGB, linear, HSL, HSV, CIE XYZ, tone mapping
- `lessons/math/12-hash-functions/` — Integer hashing, coordinate hashing, hash-to-float
- `lessons/math/13-gradient-noise/` — Perlin noise, simplex noise, fBm, domain warping
- `lessons/math/14-blue-noise-sequences/` — Halton, R2, Sobol, Mitchell's best candidate
- `lessons/math/15-bezier-curves/` — Quadratic/cubic evaluation, tangents, splitting, flattening

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
| `mat2` | `float2x2` | 16 bytes (4 floats, column-major) |
| `vec2` | `float2` | 8 bytes (2 floats) |
| `vec3` | `float3` | 12 bytes (3 floats) |
| `vec4` | `float4` | 16 bytes (4 floats) |
| `mat3` | `float3x3` | 36 bytes (9 floats, column-major) |
| `mat4` | `float4x4` | 64 bytes (16 floats, column-major) |
| `quat` | `float4` | 16 bytes (4 floats: w, x, y, z) |

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
