/*
 * forge_math.h — Math library for forge-gpu
 *
 * A learning-focused math library for graphics and game programming.
 * Every function is documented with usage examples and geometric intuition.
 *
 * Coordinate system: Right-handed, Y-up
 *   +X right, +Y up, +Z forward (toward camera)
 *
 * Matrix layout: Column-major storage, column-major math (matches HLSL)
 *   Multiplication: v' = M * v
 *   Transform order: C = A * B means "apply B first, then A"
 *
 * Winding order: Counter-clockwise (CCW) front faces
 *
 * See common/math/DESIGN.md for detailed design decisions.
 * See lessons/math/ for lessons teaching each concept.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_MATH_H
#define FORGE_MATH_H

#include <math.h>    /* sqrtf, sinf, cosf, tanf, etc. */
#include <stdint.h>  /* uint32_t for hash functions */

/* ── Scalar Helpers ──────────────────────────────────────────────────────── */

/* Linearly interpolate between two scalar values.
 *
 * When t=0, returns a.
 * When t=1, returns b.
 * When t=0.5, returns the midpoint.
 *
 * Formula: a + t * (b - a)
 *
 * This is the scalar version of vec2_lerp / vec3_lerp. It's the building
 * block for bilinear interpolation and many other blending operations.
 *
 * Usage:
 *   float mid = forge_lerpf(10.0f, 20.0f, 0.5f);  // 15.0
 *
 * See: lessons/math/01-vectors (lerp concept)
 * See: lessons/math/03-bilinear-interpolation
 */
static inline float forge_lerpf(float a, float b, float t)
{
    return a + t * (b - a);
}

/* Bilinearly interpolate between four values on a 2D grid.
 *
 * Given four corner values arranged as:
 *
 *   c01 -------- c11
 *    |            |
 *    |   (tx,ty)  |
 *    |      *     |
 *   c00 -------- c10
 *
 * The result blends all four values based on the fractional position (tx, ty)
 * within the cell, where tx and ty are each in [0, 1].
 *
 * Algorithm (two-step lerp):
 *   1. Lerp horizontally: top = lerp(c01, c11, tx)
 *                         bot = lerp(c00, c10, tx)
 *   2. Lerp vertically:   result = lerp(bot, top, ty)
 *
 * This is exactly what the GPU does when a texture sampler uses LINEAR
 * filtering — it finds the 4 nearest texels and blends them based on
 * how close the sample point is to each one.
 *
 * Parameters:
 *   c00 — bottom-left value   (x=0, y=0)
 *   c10 — bottom-right value  (x=1, y=0)
 *   c01 — top-left value      (x=0, y=1)
 *   c11 — top-right value     (x=1, y=1)
 *   tx  — horizontal blend factor [0, 1]
 *   ty  — vertical blend factor [0, 1]
 *
 * Usage:
 *   float heights[2][2] = { {1, 3}, {2, 4} };
 *   float h = forge_bilerpf(heights[0][0], heights[1][0],
 *                           heights[0][1], heights[1][1],
 *                           0.5f, 0.5f);  // 2.5 (average of all four)
 *
 * See: lessons/math/03-bilinear-interpolation
 */
static inline float forge_bilerpf(float c00, float c10,
                                   float c01, float c11,
                                   float tx, float ty)
{
    float bot = forge_lerpf(c00, c10, tx);
    float top = forge_lerpf(c01, c11, tx);
    return forge_lerpf(bot, top, ty);
}

/* Compute the base-2 logarithm of a scalar.
 *
 * Returns log₂(x) — the power you'd raise 2 to in order to get x.
 * This is the key function for computing mip levels: a texture of width W
 * has floor(log2(W)) + 1 mip levels, because each level halves the size
 * until reaching 1×1.
 *
 * Examples:
 *   forge_log2f(1.0f)   = 0.0   (2⁰ = 1)
 *   forge_log2f(2.0f)   = 1.0   (2¹ = 2)
 *   forge_log2f(4.0f)   = 2.0   (2² = 4)
 *   forge_log2f(256.0f) = 8.0   (2⁸ = 256)
 *   forge_log2f(512.0f) = 9.0   (2⁹ = 512)
 *
 * Mip level count for a 256×256 texture:
 *   int num_levels = (int)forge_log2f(256.0f) + 1;  // 9 levels
 *   // Level 0: 256×256, Level 1: 128×128, ... Level 8: 1×1
 *
 * See: lessons/math/04-mipmaps-and-lod
 */
static inline float forge_log2f(float x)
{
    return log2f(x);
}

/* Clamp a scalar to a range [lo, hi].
 *
 * Returns lo if x < lo, hi if x > hi, otherwise x.
 * Useful for clamping LOD levels, colors, blend factors, etc.
 *
 * Usage:
 *   float lod = forge_clampf(computed_lod, 0.0f, 8.0f);
 *   float alpha = forge_clampf(t, 0.0f, 1.0f);
 *
 * See: lessons/math/04-mipmaps-and-lod (LOD clamping)
 */
static inline float forge_clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* Trilinearly interpolate between eight values on a 3D grid.
 *
 * Given eight corner values of a cube:
 *
 *          c011 -------- c111        "back face" (z=1)
 *          /|            /|
 *        /  |          /  |
 *     c010 -------- c110  |
 *      |   |         |   |
 *      |  c001 ------|- c101        "front face" (z=0)
 *      |  /          |  /
 *      |/            |/
 *     c000 -------- c100
 *
 * The result blends all eight values based on the fractional position
 * (tx, ty, tz) within the cube, where each is in [0, 1].
 *
 * Algorithm:
 *   1. Bilinear interpolation on the front face (z=0):
 *      front = bilerp(c000, c100, c010, c110, tx, ty)
 *   2. Bilinear interpolation on the back face (z=1):
 *      back = bilerp(c001, c101, c011, c111, tx, ty)
 *   3. Lerp between front and back:
 *      result = lerp(front, back, tz)
 *
 * This is exactly what the GPU does for trilinear texture filtering:
 * bilinear sample from two adjacent mip levels, then lerp between them
 * based on the fractional LOD. The "z" axis is the mip level axis.
 *
 * Parameters:
 *   c000..c111 — eight corner values (binary xyz naming)
 *   tx — horizontal blend [0, 1] (x axis)
 *   ty — vertical blend   [0, 1] (y axis)
 *   tz — depth blend      [0, 1] (z axis / mip level axis)
 *
 * Usage:
 *   // Trilinear filtering between two mip levels
 *   float color = forge_trilerpf(
 *       mip0_c00, mip0_c10, mip0_c01, mip0_c11,  // level N
 *       mip1_c00, mip1_c10, mip1_c01, mip1_c11,  // level N+1
 *       frac_u, frac_v, frac_lod);
 *
 * See: lessons/math/04-mipmaps-and-lod
 * See: lessons/math/03-bilinear-interpolation (the 2D building block)
 */
static inline float forge_trilerpf(float c000, float c100,
                                    float c010, float c110,
                                    float c001, float c101,
                                    float c011, float c111,
                                    float tx, float ty, float tz)
{
    float front = forge_bilerpf(c000, c100, c010, c110, tx, ty);
    float back  = forge_bilerpf(c001, c101, c011, c111, tx, ty);
    return forge_lerpf(front, back, tz);
}

/* ── Constants ────────────────────────────────────────────────────────────── */

#define FORGE_PI      3.14159265358979323846f
#define FORGE_TAU     6.28318530717958647692f  /* 2π, full circle */
#define FORGE_DEG2RAD 0.01745329251994329576f  /* π/180 */
#define FORGE_RAD2DEG 57.2957795130823208768f  /* 180/π */

/* Machine epsilon for 32-bit float.
 *
 * The smallest float e such that (1.0f + e) != 1.0f.
 * This is the relative precision of a float at magnitude 1.0.
 * At other magnitudes, the actual precision scales proportionally:
 *   precision at value v ≈ v * FORGE_EPSILON
 *
 * Use this as a baseline when building comparison tolerances:
 *   - Absolute tolerance: a few multiples of FORGE_EPSILON
 *   - Relative tolerance: scale by the values being compared
 *
 * Value: 2^-23 ≈ 1.1920929e-7
 *
 * See: lessons/math/07-floating-point
 */
#define FORGE_EPSILON 1.1920928955078125e-7f

/* ── Floating-Point Comparison ───────────────────────────────────────────── */

/* Test if two floats are approximately equal using absolute tolerance.
 *
 * Returns true if |a - b| < tolerance.
 *
 * Best for comparing values near zero, where you know the expected
 * magnitude. For values of unknown magnitude, use forge_rel_equalf()
 * or combine both approaches.
 *
 * WARNING: An absolute tolerance that works for small values (e.g. 1e-6)
 * may be meaningless for large values (1000000 + 1e-6 is lost to rounding).
 * See the lesson for details on when to use which approach.
 *
 * Parameters:
 *   a, b      — values to compare
 *   tolerance — maximum allowed absolute difference (must be > 0)
 *
 * Usage:
 *   float result = sinf(FORGE_PI);  // should be 0, but isn't exactly
 *   if (forge_approx_equalf(result, 0.0f, 1e-6f)) { ... }
 *
 * See: lessons/math/07-floating-point
 */
static inline int forge_approx_equalf(float a, float b, float tolerance)
{
    float diff = a - b;
    return (diff < tolerance) && (diff > -tolerance);
}

/* Test if two floats are approximately equal using relative tolerance.
 *
 * Returns true if |a - b| < tolerance * max(|a|, |b|).
 *
 * The tolerance scales with the magnitude of the values, making this
 * appropriate for values of any size. A relative tolerance of 1e-5
 * means "equal to 5 decimal places."
 *
 * WARNING: Breaks down near zero because tolerance * max(|a|,|b|) → 0.
 * For comparing values near zero, use forge_approx_equalf() instead,
 * or combine both: forge_approx_equalf(a, b, abs_eps) ||
 *                   forge_rel_equalf(a, b, rel_eps)
 *
 * Parameters:
 *   a, b      — values to compare
 *   tolerance — maximum allowed relative difference (e.g. 1e-5 for 5 digits)
 *
 * Usage:
 *   float a = 1000000.0f;
 *   float b = 1000000.125f;
 *   if (forge_rel_equalf(a, b, 1e-6f)) { ... }  // true (within 6 digits)
 *
 * See: lessons/math/07-floating-point
 */
static inline int forge_rel_equalf(float a, float b, float tolerance)
{
    float diff = fabsf(a - b);
    float larger = fmaxf(fabsf(a), fabsf(b));

    /* When both values are zero, diff is also zero — they're equal. */
    if (larger == 0.0f) return diff == 0.0f;

    return diff < tolerance * larger;
}

/* ── Type Definitions ─────────────────────────────────────────────────────── */

/* 2D vector with named components.
 *
 * HLSL equivalent: float2
 *
 * Why "vec2" instead of "float2"?
 *   - Standard in C math libraries (GLM, cglm, etc.)
 *   - Portable across shader languages (HLSL, GLSL, MSL)
 *   - Clear type name in C (no confusion with C's "float" type)
 *   - Trivial mapping to HLSL: vec2 in C → float2 in shader
 */
typedef struct vec2 {
    float x, y;
} vec2;

/* 3D vector with named components.
 *
 * HLSL equivalent: float3
 */
typedef struct vec3 {
    float x, y, z;
} vec3;

/* 4D vector with named components.
 *
 * HLSL equivalent: float4
 *
 * In graphics, the w component distinguishes positions from directions:
 *   w=1: position (affected by translation)
 *   w=0: direction (not affected by translation)
 */
typedef struct vec4 {
    float x, y, z, w;
} vec4;

/* 4×4 matrix in column-major storage.
 *
 * Memory layout (16 floats):
 *   m[0..3]   = column 0
 *   m[4..7]   = column 1
 *   m[8..11]  = column 2
 *   m[12..15] = column 3
 *
 * As a mathematical matrix:
 *   | m0  m4  m8   m12 |
 *   | m1  m5  m9   m13 |
 *   | m2  m6  m10  m14 |
 *   | m3  m7  m11  m15 |
 *
 * For a transform matrix:
 *   - Columns 0–2: X, Y, Z axes (rotation + scale)
 *   - Column 3: Translation (position)
 */
typedef struct mat4 {
    float m[16];
} mat4;

/* ══════════════════════════════════════════════════════════════════════════
 * vec2 — 2D vectors
 * ══════════════════════════════════════════════════════════════════════════ */

/* Create a 2D vector from components.
 *
 * Usage:
 *   vec2 position = vec2_create(100.0f, 200.0f);
 */
static inline vec2 vec2_create(float x, float y)
{
    vec2 v = { x, y };
    return v;
}

/* Add two 2D vectors component-wise.
 *
 * Geometric meaning: Placing vector b's tail at vector a's head.
 *
 * Usage:
 *   vec2 a = vec2_create(1.0f, 2.0f);
 *   vec2 b = vec2_create(3.0f, 4.0f);
 *   vec2 c = vec2_add(a, b);  // (4.0, 6.0)
 */
static inline vec2 vec2_add(vec2 a, vec2 b)
{
    return vec2_create(a.x + b.x, a.y + b.y);
}

/* Subtract vector b from vector a component-wise.
 *
 * Geometric meaning: Vector pointing from b to a.
 *
 * Usage:
 *   vec2 a = vec2_create(5.0f, 3.0f);
 *   vec2 b = vec2_create(2.0f, 1.0f);
 *   vec2 c = vec2_sub(a, b);  // (3.0, 2.0)
 */
static inline vec2 vec2_sub(vec2 a, vec2 b)
{
    return vec2_create(a.x - b.x, a.y - b.y);
}

/* Multiply a 2D vector by a scalar.
 *
 * Geometric meaning: Scale the vector's length by s (direction unchanged).
 *
 * Usage:
 *   vec2 v = vec2_create(1.0f, 2.0f);
 *   vec2 doubled = vec2_scale(v, 2.0f);  // (2.0, 4.0)
 */
static inline vec2 vec2_scale(vec2 v, float s)
{
    return vec2_create(v.x * s, v.y * s);
}

/* Compute the dot product of two 2D vectors.
 *
 * The dot product measures how much two vectors point in the same direction.
 * Result = |a| * |b| * cos(θ), where θ is the angle between them.
 *
 * If the result is:
 *   - Positive: vectors point somewhat in the same direction
 *   - Zero:     vectors are perpendicular
 *   - Negative: vectors point in opposite directions
 *
 * Usage:
 *   vec2 a = vec2_create(1.0f, 0.0f);
 *   vec2 b = vec2_create(0.0f, 1.0f);
 *   float d = vec2_dot(a, b);  // 0.0 — perpendicular
 *
 * See: lessons/math/01-vectors for a detailed explanation.
 */
static inline float vec2_dot(vec2 a, vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

/* Compute the squared length of a 2D vector.
 *
 * This is faster than vec2_length because it avoids the square root.
 * Use this for comparisons (e.g., "is a longer than b?").
 *
 * Usage:
 *   vec2 v = vec2_create(3.0f, 4.0f);
 *   float len_sq = vec2_length_squared(v);  // 25.0
 */
static inline float vec2_length_squared(vec2 v)
{
    return vec2_dot(v, v);
}

/* Compute the length (magnitude) of a 2D vector.
 *
 * Geometric meaning: Distance from the origin to the point (x, y).
 *
 * Usage:
 *   vec2 v = vec2_create(3.0f, 4.0f);
 *   float len = vec2_length(v);  // 5.0
 *
 * See: lessons/math/01-vectors
 */
static inline float vec2_length(vec2 v)
{
    return sqrtf(vec2_length_squared(v));
}

/* Normalize a 2D vector (make it unit length).
 *
 * Geometric meaning: Point in the same direction, but with length 1.
 * Useful for direction vectors (e.g., "which way am I facing?").
 *
 * If the vector has zero length, returns (0, 0) to avoid division by zero.
 *
 * Usage:
 *   vec2 v = vec2_create(3.0f, 4.0f);
 *   vec2 dir = vec2_normalize(v);  // (0.6, 0.8) — unit length
 *
 * See: lessons/math/01-vectors
 */
static inline vec2 vec2_normalize(vec2 v)
{
    float len = vec2_length(v);
    if (len > 0.0f) {
        return vec2_scale(v, 1.0f / len);
    }
    return vec2_create(0.0f, 0.0f);
}

/* Linearly interpolate between two 2D vectors.
 *
 * When t=0, returns a.
 * When t=1, returns b.
 * When t=0.5, returns the midpoint.
 *
 * Formula: a + t * (b - a)
 *
 * Usage:
 *   vec2 start = vec2_create(0.0f, 0.0f);
 *   vec2 end   = vec2_create(10.0f, 10.0f);
 *   vec2 mid   = vec2_lerp(start, end, 0.5f);  // (5.0, 5.0)
 *
 * See: lessons/math/01-vectors
 */
static inline vec2 vec2_lerp(vec2 a, vec2 b, float t)
{
    return vec2_add(a, vec2_scale(vec2_sub(b, a), t));
}

/* ══════════════════════════════════════════════════════════════════════════
 * vec3 — 3D vectors
 * ══════════════════════════════════════════════════════════════════════════ */

/* Create a 3D vector from components.
 *
 * Usage:
 *   vec3 position = vec3_create(1.0f, 2.0f, 3.0f);
 *   vec3 color = vec3_create(1.0f, 0.0f, 0.0f);  // red
 */
static inline vec3 vec3_create(float x, float y, float z)
{
    vec3 v = { x, y, z };
    return v;
}

/* Add two 3D vectors component-wise. */
static inline vec3 vec3_add(vec3 a, vec3 b)
{
    return vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

/* Subtract vector b from vector a component-wise. */
static inline vec3 vec3_sub(vec3 a, vec3 b)
{
    return vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

/* Multiply a 3D vector by a scalar. */
static inline vec3 vec3_scale(vec3 v, float s)
{
    return vec3_create(v.x * s, v.y * s, v.z * s);
}

/* Compute the dot product of two 3D vectors.
 *
 * See vec2_dot for detailed explanation — same concept in 3D.
 *
 * Usage:
 *   vec3 a = vec3_create(1.0f, 0.0f, 0.0f);
 *   vec3 b = vec3_create(0.0f, 1.0f, 0.0f);
 *   float d = vec3_dot(a, b);  // 0.0 — perpendicular
 *
 * See: lessons/math/01-vectors
 */
static inline float vec3_dot(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* Compute the squared length of a 3D vector.
 *
 * Faster than vec3_length; use for comparisons.
 */
static inline float vec3_length_squared(vec3 v)
{
    return vec3_dot(v, v);
}

/* Compute the length (magnitude) of a 3D vector. */
static inline float vec3_length(vec3 v)
{
    return sqrtf(vec3_length_squared(v));
}

/* Normalize a 3D vector (make it unit length).
 *
 * Returns (0,0,0) if the input has zero length.
 *
 * See: lessons/math/01-vectors
 */
static inline vec3 vec3_normalize(vec3 v)
{
    float len = vec3_length(v);
    if (len > 0.0f) {
        return vec3_scale(v, 1.0f / len);
    }
    return vec3_create(0.0f, 0.0f, 0.0f);
}

/* Linearly interpolate between two 3D vectors.
 *
 * See vec2_lerp for detailed explanation.
 */
static inline vec3 vec3_lerp(vec3 a, vec3 b, float t)
{
    return vec3_add(a, vec3_scale(vec3_sub(b, a), t));
}

/* Bilinearly interpolate between four 3D vectors on a 2D grid.
 *
 * This is the vec3 version of forge_bilerpf — it blends four corner values
 * based on a 2D position (tx, ty). Useful for interpolating RGB colors,
 * positions, normals, or any 3-component quantity across a surface.
 *
 * Corner layout:
 *   c01 -------- c11      tx: horizontal blend [0, 1]
 *    |   (tx,ty)  |       ty: vertical blend   [0, 1]
 *   c00 -------- c10
 *
 * This is what the GPU does for LINEAR texture filtering on RGB textures.
 *
 * Usage:
 *   vec3 red   = vec3_create(1, 0, 0);
 *   vec3 green = vec3_create(0, 1, 0);
 *   vec3 blue  = vec3_create(0, 0, 1);
 *   vec3 white = vec3_create(1, 1, 1);
 *   vec3 blend = vec3_bilerp(red, green, blue, white, 0.5f, 0.5f);
 *
 * See: lessons/math/03-bilinear-interpolation
 */
static inline vec3 vec3_bilerp(vec3 c00, vec3 c10, vec3 c01, vec3 c11,
                                float tx, float ty)
{
    vec3 bot = vec3_lerp(c00, c10, tx);
    vec3 top = vec3_lerp(c01, c11, tx);
    return vec3_lerp(bot, top, ty);
}

/* Trilinearly interpolate between eight 3D vectors on a 3D grid.
 *
 * This is the vec3 version of forge_trilerpf — it blends eight corner
 * values based on a 3D position (tx, ty, tz). Useful for trilinear
 * texture filtering of RGB colors, or interpolating any 3-component
 * quantity across a volume.
 *
 * Cube layout (same as forge_trilerpf):
 *          c011 -------- c111
 *          /|            /|
 *     c010 -------- c110  |
 *      |  c001 ------|- c101
 *      |/            |/
 *     c000 -------- c100
 *
 * This is what the GPU does for trilinear filtering on RGB textures:
 * bilinear sample from two mip levels, then lerp between them.
 *
 * See: lessons/math/04-mipmaps-and-lod
 * See: lessons/math/03-bilinear-interpolation
 */
static inline vec3 vec3_trilerp(vec3 c000, vec3 c100, vec3 c010, vec3 c110,
                                 vec3 c001, vec3 c101, vec3 c011, vec3 c111,
                                 float tx, float ty, float tz)
{
    vec3 front = vec3_bilerp(c000, c100, c010, c110, tx, ty);
    vec3 back  = vec3_bilerp(c001, c101, c011, c111, tx, ty);
    return vec3_lerp(front, back, tz);
}

/* Compute the cross product of two 3D vectors.
 *
 * The cross product produces a vector perpendicular to both a and b,
 * following the right-hand rule:
 *   - Point fingers along a
 *   - Curl them toward b
 *   - Thumb points along the result
 *
 * Magnitude = |a| * |b| * sin(θ), where θ is the angle between them.
 *
 * Special cases:
 *   - If a and b are parallel (or anti-parallel), result is (0,0,0)
 *   - If a and b are perpendicular, result has maximum magnitude
 *
 * Common uses:
 *   - Computing surface normals: cross(edge1, edge2)
 *   - Building coordinate frames: cross(up, forward) = right
 *
 * Usage:
 *   vec3 x_axis = vec3_create(1.0f, 0.0f, 0.0f);
 *   vec3 y_axis = vec3_create(0.0f, 1.0f, 0.0f);
 *   vec3 z_axis = vec3_cross(x_axis, y_axis);  // (0.0, 0.0, 1.0)
 *
 * See: lessons/math/01-vectors
 */
static inline vec3 vec3_cross(vec3 a, vec3 b)
{
    return vec3_create(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

/* Negate a vector (reverse its direction).
 *
 * Returns (-v.x, -v.y, -v.z).
 *
 * Common uses:
 *   - Reversing a direction vector (e.g. flipping a view direction)
 *   - Computing the opposite of a force or velocity
 *
 * Usage:
 *   vec3 forward = vec3_create(0.0f, 0.0f, -1.0f);
 *   vec3 backward = vec3_negate(forward);  // (0.0, 0.0, 1.0)
 *
 * See: lessons/math/01-vectors
 */
static inline vec3 vec3_negate(vec3 v)
{
    return vec3_create(-v.x, -v.y, -v.z);
}

/* Reflect an incident vector about a surface normal.
 *
 * Formula: R = I - 2 * dot(I, N) * N
 *
 * The incident vector I points *toward* the surface.  The result R points
 * away from the surface, mirrored about N.  Both I and N should be unit
 * length for a geometrically correct reflection.
 *
 * Common uses:
 *   - Environment / reflection mapping: reflect the view direction about
 *     the surface normal to get the cube map sample direction
 *   - Specular highlights: reflect the light direction about the normal
 *   - Physics: reflecting a velocity off a wall
 *
 * Usage:
 *   vec3 incident = vec3_normalize(vec3_create(1.0f, -1.0f, 0.0f));
 *   vec3 normal   = vec3_create(0.0f, 1.0f, 0.0f);
 *   vec3 reflected = vec3_reflect(incident, normal);
 *   // reflected ≈ (0.707, 0.707, 0.0) — bounces upward
 *
 * See: lessons/math/01-vectors, lessons/gpu/14-environment-mapping
 */
static inline vec3 vec3_reflect(vec3 incident, vec3 normal)
{
    float d = vec3_dot(incident, normal);
    return vec3_sub(incident, vec3_scale(normal, 2.0f * d));
}

/* ══════════════════════════════════════════════════════════════════════════
 * vec4 — 4D vectors
 * ══════════════════════════════════════════════════════════════════════════ */

/* Create a 4D vector from components.
 *
 * In graphics, the w component distinguishes positions from directions:
 *   w=1: position (affected by translation)
 *   w=0: direction (not affected by translation)
 *
 * Usage:
 *   vec4 position = vec4_create(1.0f, 2.0f, 3.0f, 1.0f);
 *   vec4 direction = vec4_create(0.0f, 1.0f, 0.0f, 0.0f);
 */
static inline vec4 vec4_create(float x, float y, float z, float w)
{
    vec4 v = { x, y, z, w };
    return v;
}

/* Add two 4D vectors component-wise. */
static inline vec4 vec4_add(vec4 a, vec4 b)
{
    return vec4_create(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

/* Subtract vector b from vector a component-wise. */
static inline vec4 vec4_sub(vec4 a, vec4 b)
{
    return vec4_create(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

/* Multiply a 4D vector by a scalar. */
static inline vec4 vec4_scale(vec4 v, float s)
{
    return vec4_create(v.x * s, v.y * s, v.z * s, v.w * s);
}

/* Compute the dot product of two 4D vectors. */
static inline float vec4_dot(vec4 a, vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

/* Linearly interpolate between two 4D vectors.
 *
 * When t=0, returns a.
 * When t=1, returns b.
 * When t=0.5, returns the midpoint.
 *
 * Formula: a + t * (b - a)
 *
 * Usage:
 *   vec4 start = vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
 *   vec4 end   = vec4_create(0.0f, 0.0f, 1.0f, 1.0f);
 *   vec4 mid   = vec4_lerp(start, end, 0.5f);  // (0.5, 0, 0.5, 1)
 *
 * See: lessons/math/01-vectors
 */
static inline vec4 vec4_lerp(vec4 a, vec4 b, float t)
{
    return vec4_add(a, vec4_scale(vec4_sub(b, a), t));
}

/* Bilinearly interpolate between four 4D vectors on a 2D grid.
 *
 * This is the vec4 version of forge_bilerpf — it blends four corner values
 * based on a 2D position (tx, ty). Useful for interpolating RGBA colors
 * or any 4-component quantity across a surface.
 *
 * Corner layout:
 *   c01 -------- c11      tx: horizontal blend [0, 1]
 *    |   (tx,ty)  |       ty: vertical blend   [0, 1]
 *   c00 -------- c10
 *
 * This is what the GPU does for LINEAR texture filtering on RGBA textures.
 *
 * Usage:
 *   vec4 c00 = vec4_create(1, 0, 0, 1);  // red
 *   vec4 c10 = vec4_create(0, 1, 0, 1);  // green
 *   vec4 c01 = vec4_create(0, 0, 1, 1);  // blue
 *   vec4 c11 = vec4_create(1, 1, 1, 1);  // white
 *   vec4 blend = vec4_bilerp(c00, c10, c01, c11, 0.5f, 0.5f);
 *
 * See: lessons/math/03-bilinear-interpolation
 */
static inline vec4 vec4_bilerp(vec4 c00, vec4 c10, vec4 c01, vec4 c11,
                                float tx, float ty)
{
    vec4 bot = vec4_lerp(c00, c10, tx);
    vec4 top = vec4_lerp(c01, c11, tx);
    return vec4_lerp(bot, top, ty);
}

/* Trilinearly interpolate between eight 4D vectors on a 3D grid.
 *
 * This is the vec4 version of forge_trilerpf — it blends eight corner
 * values based on a 3D position (tx, ty, tz). Useful for trilinear
 * texture filtering of RGBA colors, or interpolating any 4-component
 * quantity across a volume.
 *
 * This is what the GPU does for trilinear filtering on RGBA textures:
 * bilinear sample from two mip levels, then lerp between them.
 *
 * See: lessons/math/04-mipmaps-and-lod
 * See: lessons/math/03-bilinear-interpolation
 */
static inline vec4 vec4_trilerp(vec4 c000, vec4 c100, vec4 c010, vec4 c110,
                                 vec4 c001, vec4 c101, vec4 c011, vec4 c111,
                                 float tx, float ty, float tz)
{
    vec4 front = vec4_bilerp(c000, c100, c010, c110, tx, ty);
    vec4 back  = vec4_bilerp(c001, c101, c011, c111, tx, ty);
    return vec4_lerp(front, back, tz);
}

/* ══════════════════════════════════════════════════════════════════════════
 * mat2 — 2×2 matrices
 * ══════════════════════════════════════════════════════════════════════════ */

/* 2×2 matrix in column-major storage.
 *
 * Memory layout (4 floats):
 *   m[0..1] = column 0
 *   m[2..3] = column 1
 *
 * As a mathematical matrix:
 *   | m[0]  m[2] |
 *   | m[1]  m[3] |
 *
 * Access element at row r, column c: m[c * 2 + r]
 *
 * 2×2 matrices are useful for:
 *   - Screen-space Jacobian (texture coordinate derivatives)
 *   - Anisotropy analysis (singular values of the Jacobian)
 *   - 2D rotation and scale without translation
 *
 * HLSL equivalent: float2x2
 *
 * See: lessons/math/10-anisotropy
 */
typedef struct mat2 {
    float m[4];
} mat2;

/* Create a 2×2 matrix from 4 values in row-major order.
 *
 * Values are given left-to-right, top-to-bottom (the way you'd write a matrix
 * on paper), but stored internally in column-major order.
 *
 * Usage:
 *   mat2 m = mat2_create(
 *       1, 2,   // row 0
 *       3, 4    // row 1
 *   );
 *   // m.m[0]=1, m.m[1]=3  (column 0)
 *   // m.m[2]=2, m.m[3]=4  (column 1)
 *
 * See: lessons/math/10-anisotropy
 */
static inline mat2 mat2_create(float m00, float m01,
                                 float m10, float m11)
{
    mat2 m;
    /* Transpose from row-major input to column-major storage */
    m.m[0] = m00;  m.m[2] = m01;
    m.m[1] = m10;  m.m[3] = m11;
    return m;
}

/* Create a 2×2 identity matrix.
 *
 *   | 1  0 |
 *   | 0  1 |
 *
 * Usage:
 *   mat2 m = mat2_identity();
 *
 * See: lessons/math/10-anisotropy
 */
static inline mat2 mat2_identity(void)
{
    mat2 m = {
        1.0f, 0.0f,  /* column 0 */
        0.0f, 1.0f   /* column 1 */
    };
    return m;
}

/* Multiply two 2×2 matrices: result = a * b
 *
 * Usage:
 *   mat2 combined = mat2_multiply(a, b);  // apply b first, then a
 *
 * See: lessons/math/10-anisotropy
 */
static inline mat2 mat2_multiply(mat2 a, mat2 b)
{
    mat2 result;
    result.m[0] = a.m[0] * b.m[0] + a.m[2] * b.m[1];
    result.m[1] = a.m[1] * b.m[0] + a.m[3] * b.m[1];
    result.m[2] = a.m[0] * b.m[2] + a.m[2] * b.m[3];
    result.m[3] = a.m[1] * b.m[2] + a.m[3] * b.m[3];
    return result;
}

/* Multiply a 2×2 matrix by a 2D vector: result = m * v
 *
 * Usage:
 *   mat2 rot = mat2_create(cosf(a), -sinf(a), sinf(a), cosf(a));
 *   vec2 v = vec2_create(1, 0);
 *   vec2 rotated = mat2_multiply_vec2(rot, v);
 *
 * See: lessons/math/10-anisotropy
 */
static inline vec2 mat2_multiply_vec2(mat2 m, vec2 v)
{
    return vec2_create(
        m.m[0] * v.x + m.m[2] * v.y,
        m.m[1] * v.x + m.m[3] * v.y
    );
}

/* Transpose a 2×2 matrix (swap rows and columns).
 *
 *   | a  b |T    | a  c |
 *   | c  d |  =  | b  d |
 *
 * See: lessons/math/10-anisotropy
 */
static inline mat2 mat2_transpose(mat2 m)
{
    return mat2_create(
        m.m[0], m.m[1],
        m.m[2], m.m[3]
    );
}

/* Compute the determinant of a 2×2 matrix.
 *
 *   det(| a  b |) = ad - bc
 *      (| c  d |)
 *
 * The determinant tells you how much the matrix scales area:
 *   det > 0: preserves orientation
 *   det < 0: flips orientation (reflection)
 *   det = 0: matrix is singular (collapses to a line or point)
 *
 * See: lessons/math/10-anisotropy
 */
static inline float mat2_determinant(mat2 m)
{
    return m.m[0] * m.m[3] - m.m[2] * m.m[1];
}

/* Compute the singular values of a 2×2 matrix.
 *
 * The singular values are the lengths of the semi-axes of the ellipse that
 * the matrix maps the unit circle to. They answer: "how much does this matrix
 * stretch space in each direction?"
 *
 * Returns a vec2 with x >= y (major axis first, minor axis second).
 *
 * Algorithm: compute eigenvalues of M^T * M, then take square roots.
 * For a 2×2 matrix, this has a closed-form solution:
 *
 *   S = M^T * M  (symmetric, positive semi-definite)
 *   eigenvalues of S: (trace +/- sqrt(trace^2 - 4*det)) / 2
 *   singular values: sqrt(eigenvalues)
 *
 * Example — identity matrix:
 *   mat2 I = mat2_identity();
 *   vec2 sv = mat2_singular_values(I);  // (1.0, 1.0) — isotropic
 *
 * Example — stretched in one direction:
 *   mat2 J = mat2_create(1, 0, 0, 0.25f);
 *   vec2 sv = mat2_singular_values(J);  // (1.0, 0.25) — 4:1 anisotropy
 *
 * In texture filtering, the Jacobian's singular values are the major and
 * minor axes of the pixel footprint in texture space. A large ratio means
 * the footprint is elongated (anisotropic), requiring anisotropic filtering
 * to avoid blurring.
 *
 * See: lessons/math/10-anisotropy
 */
static inline vec2 mat2_singular_values(mat2 m)
{
    /* Compute M^T * M */
    float a = m.m[0], b = m.m[2];  /* row 0: m00, m01 */
    float c = m.m[1], d = m.m[3];  /* row 1: m10, m11 */

    /* S = M^T * M = [[a*a+c*c, a*b+c*d], [a*b+c*d, b*b+d*d]] */
    float s00 = a * a + c * c;
    float s01 = a * b + c * d;
    float s11 = b * b + d * d;

    /* Eigenvalues of 2×2 symmetric matrix [[s00, s01], [s01, s11]] */
    float trace = s00 + s11;
    float det = s00 * s11 - s01 * s01;
    float disc = trace * trace - 4.0f * det;
    if (disc < 0.0f) disc = 0.0f;  /* numerical safety */
    float sqrt_disc = sqrtf(disc);

    float lambda1 = (trace + sqrt_disc) * 0.5f;
    float lambda2 = (trace - sqrt_disc) * 0.5f;
    if (lambda1 < 0.0f) lambda1 = 0.0f;  /* numerical safety */
    if (lambda2 < 0.0f) lambda2 = 0.0f;

    vec2 result;
    result.x = sqrtf(lambda1);  /* major (larger) */
    result.y = sqrtf(lambda2);  /* minor (smaller) */
    return result;
}

/* Compute the anisotropy ratio of a 2×2 matrix.
 *
 * The anisotropy ratio is the ratio of the largest to smallest singular value:
 *   ratio = sigma_max / sigma_min
 *
 * A ratio of 1.0 means the matrix stretches equally in all directions
 * (isotropic). Higher ratios indicate more directional stretching.
 *
 * In texture filtering:
 *   1:1  — isotropic (trilinear is fine)
 *   2:1  — mild anisotropy (2x anisotropic filtering helps)
 *   4:1  — moderate (noticeable quality improvement with AF)
 *   8:1+ — steep angle (AF essential to avoid blurring)
 *
 * GPUs typically cap this at 16:1 (maxAnisotropy setting).
 *
 * Returns 1.0 if the minor singular value is zero (degenerate matrix).
 *
 * Usage:
 *   mat2 jacobian = mat2_create(1.0f, 0.0f, 0.0f, 0.25f);
 *   float ratio = mat2_anisotropy_ratio(jacobian);  // 4.0
 *
 * See: lessons/math/10-anisotropy
 */
static inline float mat2_anisotropy_ratio(mat2 m)
{
    vec2 sv = mat2_singular_values(m);
    if (sv.y < FORGE_EPSILON) return 1.0f;  /* degenerate — avoid division by zero */
    return sv.x / sv.y;
}

/* ══════════════════════════════════════════════════════════════════════════
 * mat3 — 3×3 matrices
 * ══════════════════════════════════════════════════════════════════════════ */

/* 3×3 matrix in column-major storage.
 *
 * Memory layout (9 floats):
 *   m[0..2] = column 0
 *   m[3..5] = column 1
 *   m[6..8] = column 2
 *
 * As a mathematical matrix:
 *   | m[0]  m[3]  m[6] |
 *   | m[1]  m[4]  m[7] |
 *   | m[2]  m[5]  m[8] |
 *
 * Access element at row r, column c: m[c * 3 + r]
 *
 * 3×3 matrices are useful for:
 *   - 2D transforms (rotation, scale in the XY plane)
 *   - Normal matrix (inverse transpose of upper-left 3×3 of model matrix)
 *   - Teaching matrix math before jumping to 4×4
 *
 * HLSL equivalent: float3x3
 *
 * See: lessons/math/05-matrices
 */
typedef struct mat3 {
    float m[9];
} mat3;

/* Create a 3×3 matrix from 9 values in row-major order.
 *
 * Values are given left-to-right, top-to-bottom (the way you'd write a matrix
 * on paper), but stored internally in column-major order.
 *
 * Usage:
 *   mat3 m = mat3_create(
 *       1, 2, 3,   // row 0
 *       4, 5, 6,   // row 1
 *       7, 8, 9    // row 2
 *   );
 *   // m.m[0]=1, m.m[1]=4, m.m[2]=7  (column 0)
 *   // m.m[3]=2, m.m[4]=5, m.m[5]=8  (column 1)
 *   // m.m[6]=3, m.m[7]=6, m.m[8]=9  (column 2)
 *
 * See: lessons/math/05-matrices
 */
static inline mat3 mat3_create(float m00, float m01, float m02,
                                 float m10, float m11, float m12,
                                 float m20, float m21, float m22)
{
    mat3 m;
    /* Transpose from row-major input to column-major storage */
    m.m[0] = m00;  m.m[3] = m01;  m.m[6] = m02;
    m.m[1] = m10;  m.m[4] = m11;  m.m[7] = m12;
    m.m[2] = m20;  m.m[5] = m21;  m.m[8] = m22;
    return m;
}

/* Create a 3×3 identity matrix.
 *
 * The identity matrix leaves vectors unchanged: I * v = v
 *
 *   | 1  0  0 |
 *   | 0  1  0 |
 *   | 0  0  1 |
 *
 * Its columns are the standard basis vectors: (1,0,0), (0,1,0), (0,0,1).
 *
 * Usage:
 *   mat3 m = mat3_identity();
 *
 * See: lessons/math/05-matrices
 */
static inline mat3 mat3_identity(void)
{
    mat3 m = {
        1.0f, 0.0f, 0.0f,  /* column 0 */
        0.0f, 1.0f, 0.0f,  /* column 1 */
        0.0f, 0.0f, 1.0f   /* column 2 */
    };
    return m;
}

/* Multiply two 3×3 matrices: result = a * b
 *
 * Each column of the result is: a * (column of b).
 * Each element is the dot product of a's row with b's column.
 *
 * Transform order: C = A * B means "apply B first, then A"
 *
 * Important: Matrix multiplication is NOT commutative: A*B != B*A in general.
 * It IS associative: (A*B)*C = A*(B*C).
 *
 * Usage:
 *   mat3 rot = mat3_rotate(angle);
 *   mat3 scl = mat3_scale(vec2_create(2, 2));
 *   mat3 combined = mat3_multiply(rot, scl);  // scale first, then rotate
 *
 * See: lessons/math/05-matrices
 */
static inline mat3 mat3_multiply(mat3 a, mat3 b)
{
    mat3 result;

    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 3; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 3; k++) {
                sum += a.m[k * 3 + row] * b.m[col * 3 + k];
            }
            result.m[col * 3 + row] = sum;
        }
    }

    return result;
}

/* Multiply a 3×3 matrix by a vec3: result = m * v
 *
 * This transforms the vector by the matrix. Each component of the result
 * is the dot product of one row of m with the vector v.
 *
 * Geometric meaning: the matrix remaps the vector into a new coordinate frame.
 * If the columns of m are (c0, c1, c2), then:
 *   m * v = v.x * c0 + v.y * c1 + v.z * c2
 *
 * Usage:
 *   mat3 rot = mat3_rotate(FORGE_PI / 4.0f);  // 45° rotation
 *   vec3 v = vec3_create(1.0f, 0.0f, 0.0f);
 *   vec3 rotated = mat3_multiply_vec3(rot, v);
 *
 * See: lessons/math/05-matrices
 */
static inline vec3 mat3_multiply_vec3(mat3 m, vec3 v)
{
    return vec3_create(
        m.m[0] * v.x + m.m[3] * v.y + m.m[6] * v.z,
        m.m[1] * v.x + m.m[4] * v.y + m.m[7] * v.z,
        m.m[2] * v.x + m.m[5] * v.y + m.m[8] * v.z
    );
}

/* Transpose a 3×3 matrix: swap rows and columns.
 *
 * M^T[i][j] = M[j][i]
 *
 * Visually, this mirrors the matrix across the main diagonal.
 *
 * Properties:
 *   (A * B)^T = B^T * A^T   (transpose reverses multiplication order)
 *   (M^T)^T = M              (double transpose is identity)
 *
 * For orthogonal matrices (like rotations), transpose equals inverse:
 *   R^T = R^-1  (much faster than computing the actual inverse)
 *
 * Usage:
 *   mat3 m = mat3_create(1, 2, 3, 4, 5, 6, 7, 8, 9);
 *   mat3 t = mat3_transpose(m);
 *   // t = | 1 4 7 |
 *   //     | 2 5 8 |
 *   //     | 3 6 9 |
 *
 * See: lessons/math/05-matrices
 */
static inline mat3 mat3_transpose(mat3 m)
{
    mat3 t;
    t.m[0] = m.m[0];  t.m[3] = m.m[1];  t.m[6] = m.m[2];
    t.m[1] = m.m[3];  t.m[4] = m.m[4];  t.m[7] = m.m[5];
    t.m[2] = m.m[6];  t.m[5] = m.m[7];  t.m[8] = m.m[8];
    return t;
}

/* Compute the determinant of a 3×3 matrix.
 *
 * Geometric meaning: the determinant tells you how much the matrix scales
 * area (2D) or volume (3D).
 *   det > 0: preserves orientation, scales volume by det
 *   det < 0: flips orientation (mirror), scales volume by |det|
 *   det = 0: singular — squishes 3D down to 2D or less (not invertible)
 *   det = 1: rotation (preserves volume exactly)
 *
 * Formula (Sarrus' rule / cofactor expansion along first row):
 *   det = a(ei - fh) - b(di - fg) + c(dh - eg)
 *
 * Where:
 *   | a b c |     | m0 m3 m6 |
 *   | d e f |  =  | m1 m4 m7 |
 *   | g h i |     | m2 m5 m8 |
 *
 * Properties:
 *   det(A * B) = det(A) * det(B)
 *   det(I) = 1
 *   det(A^T) = det(A)
 *
 * Usage:
 *   mat3 rot = mat3_rotate(FORGE_PI / 4.0f);
 *   float d = mat3_determinant(rot);  // 1.0 (rotations preserve volume)
 *
 * See: lessons/math/05-matrices
 */
static inline float mat3_determinant(mat3 m)
{
    float a = m.m[0], b = m.m[3], c = m.m[6];
    float d = m.m[1], e = m.m[4], f = m.m[7];
    float g = m.m[2], h = m.m[5], i = m.m[8];

    return a * (e * i - f * h)
         - b * (d * i - f * g)
         + c * (d * h - e * g);
}

/* Compute the inverse of a 3×3 matrix.
 *
 * The inverse undoes the transformation: M * M^-1 = I
 *
 * Only exists when det(M) != 0. If the determinant is zero (singular matrix),
 * this function returns the identity matrix as a safe fallback.
 *
 * Method: adjugate (transpose of cofactor matrix) divided by determinant.
 *
 * For rotation matrices, the inverse equals the transpose (much faster):
 *   R^-1 = R^T
 * Use mat3_transpose() instead when you know the matrix is a pure rotation.
 *
 * Properties:
 *   (A * B)^-1 = B^-1 * A^-1  (inverse reverses multiplication order)
 *   (M^-1)^-1 = M
 *
 * Usage:
 *   mat3 m = mat3_create(2, 1, 0, 0, 3, 1, 0, 0, 1);
 *   mat3 inv = mat3_inverse(m);
 *   mat3 check = mat3_multiply(m, inv);  // should be ~identity
 *
 * See: lessons/math/05-matrices
 */
static inline mat3 mat3_inverse(mat3 m)
{
    float a = m.m[0], b = m.m[3], c = m.m[6];
    float d = m.m[1], e = m.m[4], f = m.m[7];
    float g = m.m[2], h = m.m[5], i = m.m[8];

    /* Cofactors */
    float c00 =  (e * i - f * h);
    float c01 = -(d * i - f * g);
    float c02 =  (d * h - e * g);
    float c10 = -(b * i - c * h);
    float c11 =  (a * i - c * g);
    float c12 = -(a * h - b * g);
    float c20 =  (b * f - c * e);
    float c21 = -(a * f - c * d);
    float c22 =  (a * e - b * d);

    float det = a * c00 + b * c01 + c * c02;

    if (det == 0.0f) {
        return mat3_identity();  /* Singular — not invertible */
    }

    float inv_det = 1.0f / det;

    /* Adjugate (transpose of cofactor matrix) / determinant */
    mat3 result;
    result.m[0] = c00 * inv_det;  result.m[3] = c10 * inv_det;  result.m[6] = c20 * inv_det;
    result.m[1] = c01 * inv_det;  result.m[4] = c11 * inv_det;  result.m[7] = c21 * inv_det;
    result.m[2] = c02 * inv_det;  result.m[5] = c12 * inv_det;  result.m[8] = c22 * inv_det;
    return result;
}

/* Create a 2D rotation matrix (rotates in the XY plane).
 *
 * Positive angle rotates counter-clockwise. The Z component is unchanged
 * (the 3rd row/column is the identity), making this useful for 2D transforms
 * embedded in a 3×3 matrix.
 *
 *   | cos(θ)  -sin(θ)  0 |
 *   | sin(θ)   cos(θ)  0 |
 *   |  0        0      1 |
 *
 * The columns of this matrix are:
 *   Column 0: (cos θ, sin θ, 0) — where the X axis goes
 *   Column 1: (-sin θ, cos θ, 0) — where the Y axis goes
 *   Column 2: (0, 0, 1) — Z axis unchanged
 *
 * These columns are orthonormal (perpendicular and unit length), which is
 * a key property of rotation matrices.
 *
 * Usage:
 *   mat3 rot = mat3_rotate(FORGE_PI / 4.0f);  // 45° CCW
 *   vec3 v = vec3_create(1.0f, 0.0f, 0.0f);
 *   vec3 rotated = mat3_multiply_vec3(rot, v);  // (0.707, 0.707, 0)
 *
 * See: lessons/math/05-matrices
 */
static inline mat3 mat3_rotate(float angle_radians)
{
    float c = cosf(angle_radians);
    float s = sinf(angle_radians);

    mat3 m = mat3_identity();
    m.m[0] =  c;  m.m[3] = -s;
    m.m[1] =  s;  m.m[4] =  c;
    return m;
}

/* Create a 2D scale matrix (scales in the XY plane).
 *
 * The Z component is unchanged (the 3rd diagonal element is 1).
 *
 *   | sx  0  0 |
 *   | 0  sy  0 |
 *   | 0   0  1 |
 *
 * Usage:
 *   mat3 scl = mat3_scale(vec2_create(2.0f, 3.0f));
 *   vec3 v = vec3_create(1.0f, 1.0f, 1.0f);
 *   vec3 scaled = mat3_multiply_vec3(scl, v);  // (2, 3, 1)
 *
 * See: lessons/math/05-matrices
 */
static inline mat3 mat3_scale(vec2 scale)
{
    mat3 m = mat3_identity();
    m.m[0] = scale.x;
    m.m[4] = scale.y;
    return m;
}

/* ══════════════════════════════════════════════════════════════════════════
 * mat4 — 4×4 matrices
 * ══════════════════════════════════════════════════════════════════════════ */

/* Create an identity matrix.
 *
 * The identity matrix leaves vectors unchanged when multiplied:
 *   M * v = v
 *
 * Diagonal elements are 1, all others are 0:
 *   | 1  0  0  0 |
 *   | 0  1  0  0 |
 *   | 0  0  1  0 |
 *   | 0  0  0  1 |
 *
 * Usage:
 *   mat4 m = mat4_identity();
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline mat4 mat4_identity(void)
{
    mat4 m = {
        1.0f, 0.0f, 0.0f, 0.0f,  /* column 0 */
        0.0f, 1.0f, 0.0f, 0.0f,  /* column 1 */
        0.0f, 0.0f, 1.0f, 0.0f,  /* column 2 */
        0.0f, 0.0f, 0.0f, 1.0f   /* column 3 */
    };
    return m;
}

/* Multiply matrix a by matrix b: result = a * b
 *
 * Transform order: C = A * B means "apply B first, then A"
 *
 * Example:
 *   mat4 translate = mat4_translate(vec3_create(1, 0, 0));
 *   mat4 rotate    = mat4_rotate_z(FORGE_PI / 4.0f);
 *   mat4 combined  = mat4_multiply(translate, rotate);
 *   // Rotates first, then translates
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline mat4 mat4_multiply(mat4 a, mat4 b)
{
    mat4 result;

    /* For each column in the result... */
    for (int col = 0; col < 4; col++) {
        /* For each row in that column... */
        for (int row = 0; row < 4; row++) {
            /* Dot product of a's row with b's column */
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            result.m[col * 4 + row] = sum;
        }
    }

    return result;
}

/* Multiply a matrix by a vector: result = m * v
 *
 * This transforms the vector by the matrix.
 *
 * Usage:
 *   mat4 rotation = mat4_rotate_z(FORGE_PI / 2.0f);  // 90° rotation
 *   vec4 v = vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
 *   vec4 rotated = mat4_multiply_vec4(rotation, v);  // (0, 1, 0, 1)
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline vec4 mat4_multiply_vec4(mat4 m, vec4 v)
{
    return vec4_create(
        m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z  + m.m[12]*v.w,
        m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z  + m.m[13]*v.w,
        m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]*v.w,
        m.m[3]*v.x + m.m[7]*v.y + m.m[11]*v.z + m.m[15]*v.w
    );
}

/* Create a translation matrix.
 *
 * This matrix moves (translates) points by the given offset.
 * The translation is stored in the 4th column (m[12], m[13], m[14]).
 *
 * Usage:
 *   vec3 offset = vec3_create(10.0f, 5.0f, 0.0f);
 *   mat4 m = mat4_translate(offset);
 *   vec4 point = vec4_create(0.0f, 0.0f, 0.0f, 1.0f);
 *   vec4 moved = mat4_multiply_vec4(m, point);  // (10, 5, 0, 1)
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline mat4 mat4_translate(vec3 translation)
{
    mat4 m = mat4_identity();
    m.m[12] = translation.x;
    m.m[13] = translation.y;
    m.m[14] = translation.z;
    return m;
}

/* Create a uniform scale matrix.
 *
 * This matrix scales all axes equally by the given factor.
 *
 * Usage:
 *   mat4 m = mat4_scale_uniform(2.0f);  // Double the size
 */
static inline mat4 mat4_scale_uniform(float s)
{
    mat4 m = mat4_identity();
    m.m[0]  = s;
    m.m[5]  = s;
    m.m[10] = s;
    return m;
}

/* Create a non-uniform scale matrix.
 *
 * This matrix scales each axis independently.
 *
 * Usage:
 *   vec3 scale = vec3_create(2.0f, 1.0f, 0.5f);
 *   mat4 m = mat4_scale(scale);
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline mat4 mat4_scale(vec3 scale)
{
    mat4 m = mat4_identity();
    m.m[0]  = scale.x;
    m.m[5]  = scale.y;
    m.m[10] = scale.z;
    return m;
}

/* Create a rotation matrix around the Z axis.
 *
 * Rotates in the XY plane (2D rotation when Z is up/forward).
 * Positive angle rotates counter-clockwise when looking down the +Z axis.
 *
 * Parameters:
 *   angle_radians — rotation angle in radians (use FORGE_DEG2RAD to convert)
 *
 * Usage:
 *   mat4 m = mat4_rotate_z(FORGE_PI / 2.0f);  // 90° CCW rotation
 *   vec4 v = vec4_create(1.0f, 0.0f, 0.0f, 0.0f);
 *   vec4 rotated = mat4_multiply_vec4(m, v);  // (0, 1, 0, 0)
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline mat4 mat4_rotate_z(float angle_radians)
{
    float c = cosf(angle_radians);
    float s = sinf(angle_radians);

    mat4 m = mat4_identity();
    m.m[0] =  c;  m.m[4] = -s;
    m.m[1] =  s;  m.m[5] =  c;
    return m;
}

/* Create a rotation matrix around the X axis.
 *
 * Rotates in the YZ plane.
 * Positive angle rotates counter-clockwise when looking down the +X axis.
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline mat4 mat4_rotate_x(float angle_radians)
{
    float c = cosf(angle_radians);
    float s = sinf(angle_radians);

    mat4 m = mat4_identity();
    m.m[5] =  c;  m.m[9]  = -s;
    m.m[6] =  s;  m.m[10] =  c;
    return m;
}

/* Create a rotation matrix around the Y axis.
 *
 * Rotates in the XZ plane.
 * Positive angle rotates counter-clockwise when looking down the +Y axis.
 *
 * See: lessons/math/02-coordinate-spaces
 */
static inline mat4 mat4_rotate_y(float angle_radians)
{
    float c = cosf(angle_radians);
    float s = sinf(angle_radians);

    mat4 m = mat4_identity();
    m.m[0] =  c;  m.m[8]  =  s;
    m.m[2] = -s;  m.m[10] =  c;
    return m;
}

/* Create a view matrix using the "look at" method.
 *
 * This creates a camera transformation that:
 *   - Positions the camera at 'eye'
 *   - Points the camera toward 'target'
 *   - Orients the camera so 'up' is roughly upward
 *
 * In the resulting view space:
 *   - Camera is at the origin
 *   - Camera looks down the -Z axis
 *   - +X is to the right, +Y is up
 *
 * This is the standard "view matrix" for 3D rendering.
 *
 * Parameters:
 *   eye    — Camera position in world space
 *   target — Point the camera is looking at
 *   up     — "Up" direction in world space (usually (0,1,0))
 *
 * Usage:
 *   vec3 eye = vec3_create(0.0f, 2.0f, 5.0f);
 *   vec3 target = vec3_create(0.0f, 0.0f, 0.0f);
 *   vec3 up = vec3_create(0.0f, 1.0f, 0.0f);
 *   mat4 view = mat4_look_at(eye, target, up);
 *
 * Math:
 *   forward = normalize(target - eye)
 *   right = normalize(cross(forward, up))
 *   up' = cross(right, forward)
 *
 *   Then build a matrix that rotates and translates world space into view space.
 *
 * See: lessons/math/02-coordinate-spaces
 * See: lessons/math/09-view-matrix
 */
static inline mat4 mat4_look_at(vec3 eye, vec3 target, vec3 up)
{
    /* Compute camera basis vectors */
    vec3 forward = vec3_normalize(vec3_sub(target, eye));
    vec3 right = vec3_normalize(vec3_cross(forward, up));
    vec3 up_prime = vec3_cross(right, forward);

    /* Build rotation part (inverse of camera orientation) */
    mat4 m = {
        right.x,     up_prime.x,    -forward.x,    0.0f,
        right.y,     up_prime.y,    -forward.y,    0.0f,
        right.z,     up_prime.z,    -forward.z,    0.0f,
        0.0f,        0.0f,           0.0f,         1.0f
    };

    /* Apply translation (move world opposite to camera position) */
    m.m[12] = -vec3_dot(right, eye);
    m.m[13] = -vec3_dot(up_prime, eye);
    m.m[14] =  vec3_dot(forward, eye);

    return m;
}

/* Perform the perspective divide: convert clip-space vec4 to NDC vec3.
 *
 * After a projection matrix transforms a point to clip space, the GPU divides
 * x, y, z by w to get Normalized Device Coordinates (NDC):
 *   NDC = (clip.x / clip.w, clip.y / clip.w, clip.z / clip.w)
 *
 * For perspective projection, w = -z_view, so dividing by w is what makes
 * distant objects smaller on screen.
 *
 * For orthographic projection, w = 1, so this is a no-op (NDC = clip.xyz).
 *
 * The GPU does this automatically between the vertex and fragment stages.
 * Having it as an explicit function is useful for:
 *   - CPU-side picking / unprojection
 *   - Understanding what the GPU does behind the scenes
 *   - Verifying projection math in math lessons
 *
 * Parameters:
 *   clip — a vec4 in clip space (output of projection matrix * view-space point)
 *
 * Usage:
 *   vec4 clip = mat4_multiply_vec4(proj, view_point);
 *   vec3 ndc = vec3_perspective_divide(clip);
 *   // ndc.x ∈ [-1, 1], ndc.y ∈ [-1, 1], ndc.z ∈ [0, 1]
 *
 * See: lessons/math/06-projections
 */
static inline vec3 vec3_perspective_divide(vec4 clip)
{
    float inv_w = 1.0f / clip.w;
    return vec3_create(clip.x * inv_w, clip.y * inv_w, clip.z * inv_w);
}

/* Create a symmetric perspective projection matrix.
 *
 * This is a convenience wrapper for the common case where the frustum is
 * symmetric about the view axis (left = -right, bottom = -top). For
 * asymmetric frustums (VR, multi-monitor, oblique clipping), use
 * mat4_perspective_from_planes() instead.
 *
 * This transforms view space into clip space, applying perspective foreshortening
 * (distant objects appear smaller).
 *
 * After applying this matrix, you must do perspective division (x/w, y/w, z/w)
 * to get normalized device coordinates (NDC).
 *
 * Parameters:
 *   fov_y_radians — Vertical field of view in radians (e.g., 60° = π/3)
 *   aspect_ratio  — Width / height (e.g., 16/9 = 1.777...)
 *   near_plane    — Distance to near clipping plane (e.g., 0.1)
 *   far_plane     — Distance to far clipping plane (e.g., 100.0)
 *
 * Coordinate ranges after projection and perspective divide:
 *   X ∈ [-1, 1] — left to right
 *   Y ∈ [-1, 1] — bottom to top
 *   Z ∈ [0, 1]  — near to far (Vulkan/Metal/D3D convention)
 *
 * Usage:
 *   float fov = 60.0f * FORGE_DEG2RAD;
 *   float aspect = 1920.0f / 1080.0f;
 *   mat4 proj = mat4_perspective(fov, aspect, 0.1f, 100.0f);
 *
 * See: lessons/math/02-coordinate-spaces
 * See: lessons/math/06-projections
 */
static inline mat4 mat4_perspective(float fov_y_radians, float aspect_ratio,
                                     float near_plane, float far_plane)
{
    float tan_half_fov = tanf(fov_y_radians * 0.5f);

    mat4 m = { 0 };  /* Zero-initialize */

    /* Perspective scaling */
    m.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
    m.m[5] = 1.0f / tan_half_fov;

    /* Depth mapping: map [near, far] to [0, 1] (Vulkan/D3D convention) */
    m.m[10] = far_plane / (near_plane - far_plane);
    m.m[11] = -1.0f;  /* w' = -z (for perspective divide) */

    /* Depth translation */
    m.m[14] = -(far_plane * near_plane) / (far_plane - near_plane);

    return m;
}

/* Create an asymmetric perspective projection matrix from frustum planes.
 *
 * The general form of perspective projection. mat4_perspective() is a symmetric
 * special case of this (where left = -right, bottom = -top, derived from FOV
 * and aspect ratio).
 *
 * Use this when you need:
 *   - VR rendering (each eye has an asymmetric frustum)
 *   - Multi-monitor setups (off-center projection)
 *   - Oblique near-plane clipping (portal rendering)
 *
 * Parameters define the frustum's near plane rectangle in view space:
 *   left       — X coordinate of the left edge on the near plane
 *   right      — X coordinate of the right edge on the near plane
 *   bottom     — Y coordinate of the bottom edge on the near plane
 *   top        — Y coordinate of the top edge on the near plane
 *   near_plane — Distance to the near clipping plane (positive)
 *   far_plane  — Distance to the far clipping plane (positive, > near)
 *
 * Coordinate ranges after projection and perspective divide:
 *   X ∈ [-1, 1] — left to right
 *   Y ∈ [-1, 1] — bottom to top
 *   Z ∈ [0, 1]  — near to far (Vulkan/Metal/D3D convention)
 *
 * Usage:
 *   // Symmetric case (equivalent to mat4_perspective with 90° FOV, 1:1 aspect):
 *   float n = 0.1f;
 *   mat4 proj = mat4_perspective_from_planes(-n, n, -n, n, n, 100.0f);
 *
 *   // Asymmetric (left eye in VR):
 *   mat4 proj = mat4_perspective_from_planes(-0.06f, 0.04f, -0.05f, 0.05f,
 *                                             0.1f, 100.0f);
 *
 * See: lessons/math/06-projections
 */
static inline mat4 mat4_perspective_from_planes(float left, float right,
                                                  float bottom, float top,
                                                  float near_plane,
                                                  float far_plane)
{
    mat4 m = { 0 };  /* Zero-initialize */

    /* X: map [left, right] on near plane to [-1, 1] in NDC */
    m.m[0]  = (2.0f * near_plane) / (right - left);
    m.m[8]  = (right + left) / (right - left);

    /* Y: map [bottom, top] on near plane to [-1, 1] in NDC */
    m.m[5]  = (2.0f * near_plane) / (top - bottom);
    m.m[9]  = (top + bottom) / (top - bottom);

    /* Z: map [near, far] to [0, 1] (Vulkan/D3D convention) */
    m.m[10] = far_plane / (near_plane - far_plane);
    m.m[14] = -(far_plane * near_plane) / (far_plane - near_plane);

    /* w' = -z (perspective divide) */
    m.m[11] = -1.0f;

    return m;
}

/* Create an orthographic projection matrix.
 *
 * This transforms view space into clip space WITHOUT perspective foreshortening
 * (distant objects appear the same size as near objects).
 *
 * The projection maps an axis-aligned box in view space to the NDC cube.
 * Everything inside the box is visible; everything outside is clipped.
 *
 * Unlike perspective projection, there is no perspective divide (w stays 1),
 * so parallel lines in the scene remain parallel on screen.
 *
 * Parameters:
 *   left   — X coordinate of the left clipping plane
 *   right  — X coordinate of the right clipping plane
 *   bottom — Y coordinate of the bottom clipping plane
 *   top    — Y coordinate of the top clipping plane
 *   near_plane — Distance to the near clipping plane (positive)
 *   far_plane  — Distance to the far clipping plane (positive, > near)
 *
 * Coordinate ranges after projection:
 *   X in [-1, 1] — left to right
 *   Y in [-1, 1] — bottom to top
 *   Z in [0, 1]  — near to far (Vulkan/Metal/D3D convention)
 *
 * Usage:
 *   mat4 proj = mat4_orthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
 *
 * Common use cases:
 *   - 2D rendering / UI: mat4_orthographic(0, width, 0, height, -1, 1)
 *   - Shadow maps: orthographic from the light's point of view
 *   - CAD / architectural visualization: no perspective distortion
 *
 * See: lessons/math/06-projections
 */
static inline mat4 mat4_orthographic(float left, float right,
                                      float bottom, float top,
                                      float near_plane, float far_plane)
{
    mat4 m = { 0 };  /* Zero-initialize */

    /* X: map [left, right] to [-1, 1] */
    m.m[0]  = 2.0f / (right - left);
    m.m[12] = -(right + left) / (right - left);

    /* Y: map [bottom, top] to [-1, 1] */
    m.m[5]  = 2.0f / (top - bottom);
    m.m[13] = -(top + bottom) / (top - bottom);

    /* Z: map [-near, -far] to [0, 1] (0-to-1 depth, right-handed) */
    m.m[10] = 1.0f / (near_plane - far_plane);
    m.m[14] = near_plane / (near_plane - far_plane);

    /* w stays 1 (no perspective divide) */
    m.m[15] = 1.0f;

    return m;
}

/* Transpose a 4×4 matrix: swap rows and columns.
 *
 * M^T[i][j] = M[j][i]
 *
 * Properties:
 *   (A * B)^T = B^T * A^T
 *   (M^T)^T = M
 *
 * For orthogonal matrices (rotations), transpose = inverse.
 *
 * Usage:
 *   mat4 rot = mat4_rotate_z(angle);
 *   mat4 inv = mat4_transpose(rot);  // inverse of a rotation
 *
 * See: lessons/math/05-matrices
 */
static inline mat4 mat4_transpose(mat4 m)
{
    mat4 t;
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            t.m[col * 4 + row] = m.m[row * 4 + col];
        }
    }
    return t;
}

/* Compute the determinant of a 4×4 matrix.
 *
 * Geometric meaning: how much the matrix scales 4D volume (hypervolume).
 * For 3D transforms, it tells you the volume scaling of the transformation.
 *
 * Method: cofactor expansion along the first row.
 *
 * Properties:
 *   det(A * B) = det(A) * det(B)
 *   det(I) = 1
 *   det = 0 means the matrix is singular (not invertible)
 *
 * Usage:
 *   mat4 rot = mat4_rotate_y(angle);
 *   float d = mat4_determinant(rot);  // 1.0
 *
 * See: lessons/math/05-matrices
 */
static inline float mat4_determinant(mat4 m)
{
    /* Use cofactor expansion along the first row.
     * det = m00*C00 - m01*C01 + m02*C02 - m03*C03
     * where Cij is the 3×3 minor determinant at (i,j).
     *
     * Matrix (row, col) with column-major indexing: M[row][col] = m[col*4+row]
     */
    float a00 = m.m[0],  a01 = m.m[4],  a02 = m.m[8],   a03 = m.m[12];
    float a10 = m.m[1],  a11 = m.m[5],  a12 = m.m[9],   a13 = m.m[13];
    float a20 = m.m[2],  a21 = m.m[6],  a22 = m.m[10],  a23 = m.m[14];
    float a30 = m.m[3],  a31 = m.m[7],  a32 = m.m[11],  a33 = m.m[15];

    /* 2×2 sub-determinants (bottom two rows) */
    float s0 = a20 * a31 - a21 * a30;
    float s1 = a20 * a32 - a22 * a30;
    float s2 = a20 * a33 - a23 * a30;
    float s3 = a21 * a32 - a22 * a31;
    float s4 = a21 * a33 - a23 * a31;
    float s5 = a22 * a33 - a23 * a32;

    /* Cofactors of first row */
    float c0 = a11 * s5 - a12 * s4 + a13 * s3;
    float c1 = a10 * s5 - a12 * s2 + a13 * s1;
    float c2 = a10 * s4 - a11 * s2 + a13 * s0;
    float c3 = a10 * s3 - a11 * s1 + a12 * s0;

    return a00 * c0 - a01 * c1 + a02 * c2 - a03 * c3;
}

/* Compute the inverse of a 4×4 matrix.
 *
 * The inverse undoes the transformation: M * M^-1 = I
 *
 * Only exists when det(M) != 0. If the matrix is singular, returns the
 * identity matrix as a safe fallback.
 *
 * Method: adjugate (transpose of cofactor matrix) divided by determinant.
 * This uses the efficient 2×2 sub-determinant approach.
 *
 * For rotation matrices, the inverse equals the transpose (much faster):
 *   R^-1 = R^T
 * Use mat4_transpose() when you know the matrix is a pure rotation.
 *
 * Properties:
 *   (A * B)^-1 = B^-1 * A^-1
 *   (M^-1)^-1 = M
 *
 * Usage:
 *   mat4 model = mat4_multiply(translation, rotation);
 *   mat4 inv = mat4_inverse(model);
 *   // model * inv ≈ identity
 *
 * See: lessons/math/05-matrices
 */
static inline mat4 mat4_inverse(mat4 m)
{
    /* Use column-major element names: m[col*4+row] */
    float m0  = m.m[0],  m1  = m.m[1],  m2  = m.m[2],  m3  = m.m[3];
    float m4  = m.m[4],  m5  = m.m[5],  m6  = m.m[6],  m7  = m.m[7];
    float m8  = m.m[8],  m9  = m.m[9],  m10 = m.m[10], m11 = m.m[11];
    float m12 = m.m[12], m13 = m.m[13], m14 = m.m[14], m15 = m.m[15];

    /* Pre-compute 2×2 sub-determinants for rows 2&3 */
    float A = m10 * m15 - m11 * m14;
    float B = m6  * m15 - m7  * m14;
    float C = m6  * m11 - m7  * m10;
    float D = m2  * m15 - m3  * m14;
    float E = m2  * m11 - m3  * m10;
    float F = m2  * m7  - m3  * m6;

    /* Cofactors for column 0 of the adjugate (used to compute determinant) */
    mat4 r;
    r.m[0]  =  m5 * A - m9 * B + m13 * C;
    r.m[1]  = -m1 * A + m9 * D - m13 * E;
    r.m[2]  =  m1 * B - m5 * D + m13 * F;
    r.m[3]  = -m1 * C + m5 * E - m9  * F;

    float det = m0 * r.m[0] + m4 * r.m[1] + m8 * r.m[2] + m12 * r.m[3];

    if (det == 0.0f) {
        return mat4_identity();  /* Singular — not invertible */
    }

    /* Cofactors for remaining columns of the adjugate */
    r.m[4]  = -m4 * A + m8 * B - m12 * C;
    r.m[5]  =  m0 * A - m8 * D + m12 * E;
    r.m[6]  = -m0 * B + m4 * D - m12 * F;
    r.m[7]  =  m0 * C - m4 * E + m8  * F;

    /* Pre-compute 2×2 sub-determinants for rows 1&3 */
    float G = m9  * m15 - m11 * m13;
    float H = m5  * m15 - m7  * m13;
    float I = m5  * m11 - m7  * m9;
    float J = m1  * m15 - m3  * m13;
    float K = m1  * m11 - m3  * m9;
    float L = m1  * m7  - m3  * m5;

    r.m[8]  =  m4 * G - m8 * H + m12 * I;
    r.m[9]  = -m0 * G + m8 * J - m12 * K;
    r.m[10] =  m0 * H - m4 * J + m12 * L;
    r.m[11] = -m0 * I + m4 * K - m8  * L;

    /* Pre-compute 2×2 sub-determinants for rows 1&2 */
    float M = m9  * m14 - m10 * m13;
    float N = m5  * m14 - m6  * m13;
    float O = m5  * m10 - m6  * m9;
    float P = m1  * m14 - m2  * m13;
    float Q = m1  * m10 - m2  * m9;
    float R = m1  * m6  - m2  * m5;

    r.m[12] = -m4 * M + m8 * N - m12 * O;
    r.m[13] =  m0 * M - m8 * P + m12 * Q;
    r.m[14] = -m0 * N + m4 * P - m12 * R;
    r.m[15] =  m0 * O - m4 * Q + m8  * R;

    /* Divide all elements by the determinant */
    float inv_det = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        r.m[i] *= inv_det;
    }

    return r;
}

/* Embed a 3×3 matrix into the upper-left corner of a 4×4 identity matrix.
 *
 * Useful for promoting a 3×3 rotation/scale to a full 4×4 transform,
 * or for demonstrating the relationship between mat3 and mat4.
 *
 *   | m3[0] m3[3] m3[6] 0 |
 *   | m3[1] m3[4] m3[7] 0 |
 *   | m3[2] m3[5] m3[8] 0 |
 *   |  0     0     0    1 |
 *
 * Usage:
 *   mat3 rot3 = mat3_rotate(angle);
 *   mat4 rot4 = mat4_from_mat3(rot3);  // 4×4 version of the same rotation
 *
 * See: lessons/math/05-matrices
 */
static inline mat4 mat4_from_mat3(mat3 m3)
{
    mat4 m = mat4_identity();
    m.m[0] = m3.m[0];  m.m[4] = m3.m[3];  m.m[8]  = m3.m[6];
    m.m[1] = m3.m[1];  m.m[5] = m3.m[4];  m.m[9]  = m3.m[7];
    m.m[2] = m3.m[2];  m.m[6] = m3.m[5];  m.m[10] = m3.m[8];
    return m;
}

/* ══════════════════════════════════════════════════════════════════════════
 * quat — Quaternions
 * ══════════════════════════════════════════════════════════════════════════ */

/* Quaternion: a 4-component number system for representing 3D rotations.
 *
 * A quaternion q = w + xi + yj + zk, where i, j, k are imaginary units with:
 *   i² = j² = k² = ijk = -1
 *   ij = k,  jk = i,  ki = j  (cyclic)
 *   ji = -k, kj = -i, ik = -j (anti-commutative)
 *
 * For rotations, we use UNIT quaternions (length = 1). A unit quaternion
 * encodes a rotation of angle θ around axis (ax, ay, az) as:
 *   q = (cos(θ/2),  sin(θ/2)*ax,  sin(θ/2)*ay,  sin(θ/2)*az)
 *
 * Storage order: (w, x, y, z) — scalar part first, matching the mathematical
 * notation q = w + xi + yj + zk.
 *
 * Why quaternions instead of matrices for rotations?
 *   - Compact: 4 floats instead of 9 (mat3) or 16 (mat4)
 *   - No gimbal lock (unlike Euler angles)
 *   - Smooth interpolation via slerp
 *   - Easy composition via multiplication
 *   - Always represent valid rotations (when normalized)
 *
 * HLSL: No built-in quaternion type; pass as float4 and multiply in shader,
 * or convert to mat4 on the CPU.
 *
 * See: lessons/math/08-orientation
 */
typedef struct quat {
    float w, x, y, z;
} quat;

/* ── Quaternion Creation ──────────────────────────────────────────────── */

/* Create a quaternion from components.
 *
 * Usage:
 *   quat q = quat_create(1.0f, 0.0f, 0.0f, 0.0f);  // identity
 */
static inline quat quat_create(float w, float x, float y, float z)
{
    quat q = { w, x, y, z };
    return q;
}

/* Create the identity quaternion (no rotation).
 *
 * The identity quaternion is (1, 0, 0, 0) — it corresponds to a rotation
 * of 0 degrees around any axis: cos(0/2) = 1, sin(0/2) = 0.
 *
 * Usage:
 *   quat q = quat_identity();
 *   // Rotating by identity leaves vectors unchanged
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_identity(void)
{
    quat q = { 1.0f, 0.0f, 0.0f, 0.0f };
    return q;
}

/* ── Quaternion Properties ────────────────────────────────────────────── */

/* Compute the dot product of two quaternions.
 *
 * Like vec4 dot product: a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z.
 *
 * For unit quaternions, the dot product tells you how "similar" two
 * rotations are:
 *   dot ≈ 1 or -1: nearly the same rotation
 *   dot ≈ 0: rotations are about 90° apart
 *
 * The sign matters: q and -q represent the SAME rotation, but the dot
 * product distinguishes them. Slerp uses this to pick the shorter path.
 *
 * See: lessons/math/08-orientation
 */
static inline float quat_dot(quat a, quat b)
{
    return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

/* Compute the squared length (norm²) of a quaternion.
 *
 * For unit quaternions, this should be 1.0. Useful for checking if
 * a quaternion needs renormalization (cheaper than computing the length).
 */
static inline float quat_length_sq(quat q)
{
    return quat_dot(q, q);
}

/* Compute the length (norm) of a quaternion.
 *
 * For unit quaternions (rotations), this should be 1.0.
 * If it drifts from 1.0 due to accumulated floating-point error,
 * call quat_normalize() to fix it.
 */
static inline float quat_length(quat q)
{
    return sqrtf(quat_length_sq(q));
}

/* ── Quaternion Operations ────────────────────────────────────────────── */

/* Normalize a quaternion to unit length.
 *
 * Unit quaternions represent rotations. After many multiplications,
 * floating-point drift can make the length deviate from 1.0. Normalizing
 * snaps it back.
 *
 * Returns (1,0,0,0) if the input has zero length.
 *
 * Usage:
 *   quat q = quat_multiply(a, b);
 *   q = quat_normalize(q);  // ensure it's still unit length
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_normalize(quat q)
{
    float len = quat_length(q);
    if (len > 0.0f) {
        float inv = 1.0f / len;
        return quat_create(q.w * inv, q.x * inv, q.y * inv, q.z * inv);
    }
    return quat_identity();
}

/* Negate a quaternion (flip all components).
 *
 * Important: q and -q represent the SAME rotation. The quaternion
 * double-cover property means every rotation has two quaternion
 * representations. Negation is used internally by slerp to ensure
 * the shortest interpolation path.
 */
static inline quat quat_negate(quat q)
{
    return quat_create(-q.w, -q.x, -q.y, -q.z);
}

/* Compute the conjugate of a quaternion: q* = (w, -x, -y, -z).
 *
 * For unit quaternions, the conjugate equals the inverse (the rotation
 * that undoes q). It's much cheaper than computing the full inverse.
 *
 * Geometric meaning: the conjugate rotates by the same angle but in
 * the opposite direction.
 *
 * Usage:
 *   quat q = quat_from_axis_angle(axis, angle);
 *   quat q_undo = quat_conjugate(q);  // rotates by -angle
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_conjugate(quat q)
{
    return quat_create(q.w, -q.x, -q.y, -q.z);
}

/* Compute the inverse of a quaternion: q⁻¹ = q* / |q|².
 *
 * The inverse satisfies: q * q⁻¹ = identity.
 *
 * For unit quaternions (|q| = 1), the inverse equals the conjugate.
 * Use quat_conjugate() instead when you know the quaternion is unit length
 * (it's cheaper — no division needed).
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_inverse(quat q)
{
    float len_sq = quat_length_sq(q);
    if (len_sq > 0.0f) {
        float inv = 1.0f / len_sq;
        return quat_create(q.w * inv, -q.x * inv, -q.y * inv, -q.z * inv);
    }
    return quat_identity();
}

/* Multiply two quaternions: result = a * b.
 *
 * Quaternion multiplication composes rotations, just like matrix
 * multiplication. The order matters (quaternion multiplication is
 * NOT commutative):
 *   q = a * b means "apply b first, then a"
 *
 * This matches our matrix convention: C = A * B means "apply B first."
 *
 * The multiplication formula comes from expanding:
 *   (a.w + a.x·i + a.y·j + a.z·k) * (b.w + b.x·i + b.y·j + b.z·k)
 * using the rules i²=j²=k²=ijk=-1.
 *
 * Usage:
 *   quat yaw   = quat_from_axis_angle(vec3_create(0, 1, 0), angle_y);
 *   quat pitch = quat_from_axis_angle(vec3_create(1, 0, 0), angle_x);
 *   quat combined = quat_multiply(yaw, pitch);  // pitch first, then yaw
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_multiply(quat a, quat b)
{
    return quat_create(
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    );
}

/* Rotate a 3D vector by a quaternion: v' = q * v * q*.
 *
 * This is the primary way to apply a quaternion rotation to a point
 * or direction vector. The vector v is treated as a pure quaternion
 * (0, v.x, v.y, v.z), then sandwiched between q and its conjugate.
 *
 * The expanded formula avoids constructing intermediate quaternions:
 *   v' = v + 2w(u × v) + 2(u × (u × v))
 * where u = (q.x, q.y, q.z) is the vector part of q.
 *
 * Usage:
 *   quat rot = quat_from_axis_angle(vec3_create(0, 1, 0), FORGE_PI / 2);
 *   vec3 v = vec3_create(1, 0, 0);
 *   vec3 rotated = quat_rotate_vec3(rot, v);  // (0, 0, -1) — 90° around Y
 *
 * See: lessons/math/08-orientation
 */
static inline vec3 quat_rotate_vec3(quat q, vec3 v)
{
    /* u = vector part of quaternion */
    vec3 u = vec3_create(q.x, q.y, q.z);

    /* t = 2 * (u × v) */
    vec3 t = vec3_scale(vec3_cross(u, v), 2.0f);

    /* v' = v + w*t + u × t */
    return vec3_add(vec3_add(v, vec3_scale(t, q.w)), vec3_cross(u, t));
}

/* ── Quaternion ↔ Axis-Angle ─────────────────────────────────────────── */

/* Create a quaternion from an axis-angle rotation.
 *
 * Axis-angle is the most intuitive rotation representation:
 *   "Rotate by θ degrees around this axis."
 *
 * The axis must be a unit vector. The angle is in radians.
 *
 * Formula:
 *   q = (cos(θ/2), sin(θ/2) * axis.x, sin(θ/2) * axis.y, sin(θ/2) * axis.z)
 *
 * Why half-angle? Because quaternions double-cover rotations. A rotation
 * of θ maps to θ/2 in quaternion space, and a full 360° rotation maps to
 * q = (-1, 0, 0, 0), while 720° brings you back to (1, 0, 0, 0).
 *
 * Usage:
 *   vec3 up = vec3_create(0, 1, 0);
 *   quat q = quat_from_axis_angle(up, 45.0f * FORGE_DEG2RAD);  // 45° yaw
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_from_axis_angle(vec3 axis, float angle_radians)
{
    float half = angle_radians * 0.5f;
    float s = sinf(half);
    return quat_create(cosf(half), s * axis.x, s * axis.y, s * axis.z);
}

/* Extract the axis and angle from a quaternion.
 *
 * Inverse of quat_from_axis_angle. Returns the rotation axis (unit vector)
 * and angle (radians, in [0, 2π]).
 *
 * Edge case: if the quaternion is the identity (no rotation), the axis is
 * undefined. We return (0, 1, 0) as a convention.
 *
 * Parameters:
 *   q         — unit quaternion
 *   out_axis  — receives the rotation axis (unit vector)
 *   out_angle — receives the rotation angle in radians
 *
 * See: lessons/math/08-orientation
 */
static inline void quat_to_axis_angle(quat q, vec3 *out_axis, float *out_angle)
{
    /* Ensure w is in [-1, 1] for acos (clamp for numerical safety) */
    float w = forge_clampf(q.w, -1.0f, 1.0f);
    *out_angle = 2.0f * acosf(w);

    /* The vector part length = sin(angle/2) */
    float s = sqrtf(1.0f - w * w);
    if (s > 1e-6f) {
        float inv_s = 1.0f / s;
        *out_axis = vec3_create(q.x * inv_s, q.y * inv_s, q.z * inv_s);
    } else {
        /* Nearly zero rotation — axis is undefined, pick Y-up */
        *out_axis = vec3_create(0.0f, 1.0f, 0.0f);
    }
}

/* ── Quaternion ↔ Euler Angles ───────────────────────────────────────── */

/* Create a quaternion from Euler angles (intrinsic Y-X-Z order).
 *
 * This is the standard game/camera convention:
 *   1. Yaw:   rotate around Y axis (look left/right)
 *   2. Pitch: rotate around X axis (look up/down)
 *   3. Roll:  rotate around Z axis (tilt head)
 *
 * Equivalent to: q = q_yaw * q_pitch * q_roll
 * (yaw applied to world, pitch in yawed frame, roll in pitched frame)
 *
 * All angles are in radians.
 *
 * WARNING: Euler angles suffer from gimbal lock when pitch = ±90°.
 * Prefer quaternions for runtime orientation and convert to/from Euler
 * only for user-facing display or input.
 *
 * Usage:
 *   float yaw = 45.0f * FORGE_DEG2RAD;
 *   float pitch = -15.0f * FORGE_DEG2RAD;
 *   quat orientation = quat_from_euler(yaw, pitch, 0.0f);
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_from_euler(float yaw, float pitch, float roll)
{
    /* Half angles */
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);

    /* Expanded quaternion product: q_y * q_x * q_z */
    return quat_create(
        cy * cp * cr + sy * sp * sr,   /* w */
        cy * sp * cr + sy * cp * sr,   /* x */
        sy * cp * cr - cy * sp * sr,   /* y */
        cy * cp * sr - sy * sp * cr    /* z */
    );
}

/* Extract Euler angles (intrinsic Y-X-Z) from a quaternion.
 *
 * Returns (yaw, pitch, roll) packed in a vec3:
 *   .x = yaw   (Y rotation, in radians)
 *   .y = pitch  (X rotation, in radians, range [-π/2, π/2])
 *   .z = roll   (Z rotation, in radians)
 *
 * At gimbal lock (pitch = ±90°), yaw and roll become coupled — we set
 * roll = 0 and absorb both into yaw (standard convention).
 *
 * WARNING: Converting to Euler and back may not give the original angles
 * because multiple Euler triplets can represent the same rotation.
 *
 * Usage:
 *   vec3 euler = quat_to_euler(orientation);
 *   float yaw_deg   = euler.x * FORGE_RAD2DEG;
 *   float pitch_deg = euler.y * FORGE_RAD2DEG;
 *   float roll_deg  = euler.z * FORGE_RAD2DEG;
 *
 * See: lessons/math/08-orientation
 */
static inline vec3 quat_to_euler(quat q)
{
    float yaw, pitch, roll;

    /* sin(pitch) from rotation matrix element R[1][2] */
    float sinp = 2.0f * (q.w * q.x - q.y * q.z);

    if (sinp >= 1.0f) {
        /* Gimbal lock: pitch = +90° */
        pitch = FORGE_PI * 0.5f;
        yaw = atan2f(2.0f * (q.w * q.y - q.x * q.z),
                     1.0f - 2.0f * (q.y * q.y + q.z * q.z));
        roll = 0.0f;
    } else if (sinp <= -1.0f) {
        /* Gimbal lock: pitch = -90° */
        pitch = -FORGE_PI * 0.5f;
        yaw = atan2f(2.0f * (q.w * q.y - q.x * q.z),
                     1.0f - 2.0f * (q.y * q.y + q.z * q.z));
        roll = 0.0f;
    } else {
        pitch = asinf(sinp);
        yaw = atan2f(2.0f * (q.x * q.z + q.w * q.y),
                     1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        roll = atan2f(2.0f * (q.x * q.y + q.w * q.z),
                      1.0f - 2.0f * (q.x * q.x + q.z * q.z));
    }

    return vec3_create(yaw, pitch, roll);
}

/* ── Quaternion ↔ Matrix ─────────────────────────────────────────────── */

/* Convert a unit quaternion to a 4×4 rotation matrix.
 *
 * The resulting matrix performs the same rotation as the quaternion.
 * Use this when you need to combine a quaternion rotation with other
 * transforms (translation, scale) in the MVP pipeline.
 *
 * The matrix is orthonormal (columns are perpendicular unit vectors),
 * has determinant 1, and its inverse equals its transpose.
 *
 * Formula (from expanding q * v * q*):
 *   | 1-2(y²+z²)  2(xy-wz)    2(xz+wy)   0 |
 *   | 2(xy+wz)    1-2(x²+z²)  2(yz-wx)   0 |
 *   | 2(xz-wy)    2(yz+wx)    1-2(x²+y²) 0 |
 *   | 0           0           0           1 |
 *
 * Usage:
 *   quat q = quat_from_axis_angle(axis, angle);
 *   mat4 rotation = quat_to_mat4(q);
 *   mat4 model = mat4_multiply(translation, rotation);  // for MVP
 *
 * See: lessons/math/08-orientation
 */
static inline mat4 quat_to_mat4(quat q)
{
    /* Pre-compute products (each used twice) */
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;

    mat4 m = mat4_identity();

    /* Column 0 (X axis) */
    m.m[0] = 1.0f - 2.0f * (yy + zz);
    m.m[1] = 2.0f * (xy + wz);
    m.m[2] = 2.0f * (xz - wy);

    /* Column 1 (Y axis) */
    m.m[4] = 2.0f * (xy - wz);
    m.m[5] = 1.0f - 2.0f * (xx + zz);
    m.m[6] = 2.0f * (yz + wx);

    /* Column 2 (Z axis) */
    m.m[8]  = 2.0f * (xz + wy);
    m.m[9]  = 2.0f * (yz - wx);
    m.m[10] = 1.0f - 2.0f * (xx + yy);

    return m;
}

/* Convert a rotation matrix to a unit quaternion.
 *
 * Extracts the quaternion from the upper-left 3×3 of a 4×4 matrix.
 * The matrix should be a pure rotation (orthonormal, determinant 1).
 * If the matrix includes scale or skew, normalize the columns first.
 *
 * Uses Shepperd's method: picks the largest diagonal element to avoid
 * division by near-zero values, ensuring numerical stability.
 *
 * Usage:
 *   mat4 rotation = mat4_rotate_y(angle);
 *   quat q = quat_from_mat4(rotation);
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_from_mat4(mat4 m)
{
    /* R[row][col] in column-major: m.m[col*4 + row] */
    float r00 = m.m[0], r11 = m.m[5], r22 = m.m[10];
    float trace = r00 + r11 + r22;
    float w, x, y, z;

    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;  /* s = 4w */
        w = s * 0.25f;
        x = (m.m[6] - m.m[9]) / s;   /* (R[2][1] - R[1][2]) / s */
        y = (m.m[8] - m.m[2]) / s;   /* (R[0][2] - R[2][0]) / s */
        z = (m.m[1] - m.m[4]) / s;   /* (R[1][0] - R[0][1]) / s */
    } else if (r00 > r11 && r00 > r22) {
        float s = sqrtf(1.0f + r00 - r11 - r22) * 2.0f;  /* s = 4x */
        w = (m.m[6] - m.m[9]) / s;
        x = s * 0.25f;
        y = (m.m[4] + m.m[1]) / s;   /* (R[0][1] + R[1][0]) / s */
        z = (m.m[8] + m.m[2]) / s;   /* (R[0][2] + R[2][0]) / s */
    } else if (r11 > r22) {
        float s = sqrtf(1.0f + r11 - r00 - r22) * 2.0f;  /* s = 4y */
        w = (m.m[8] - m.m[2]) / s;
        x = (m.m[4] + m.m[1]) / s;
        y = s * 0.25f;
        z = (m.m[9] + m.m[6]) / s;   /* (R[1][2] + R[2][1]) / s */
    } else {
        float s = sqrtf(1.0f + r22 - r00 - r11) * 2.0f;  /* s = 4z */
        w = (m.m[1] - m.m[4]) / s;
        x = (m.m[8] + m.m[2]) / s;
        y = (m.m[9] + m.m[6]) / s;
        z = s * 0.25f;
    }

    return quat_create(w, x, y, z);
}

/* ── Quaternion Interpolation ────────────────────────────────────────── */

/* Spherical linear interpolation between two quaternions.
 *
 * SLERP interpolates along the shortest arc on the 4D unit sphere,
 * producing constant angular velocity — the rotation speed is uniform.
 *
 * When t=0, returns a. When t=1, returns b. Values between give a
 * smooth rotation that moves at constant speed.
 *
 * SLERP automatically takes the shortest path: if the dot product of
 * a and b is negative (meaning they represent the same rotation but
 * are on opposite hemispheres), one is negated first.
 *
 * Formula:
 *   slerp(a, b, t) = a * sin((1-t)θ)/sin(θ) + b * sin(tθ)/sin(θ)
 *   where θ = acos(dot(a, b))
 *
 * Falls back to nlerp when the angle is very small (avoids division
 * by near-zero sin(θ)).
 *
 * Usage:
 *   quat start = quat_from_euler(0, 0, 0);
 *   quat end   = quat_from_euler(FORGE_PI, 0, 0);
 *   quat mid   = quat_slerp(start, end, 0.5f);  // halfway rotation
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_slerp(quat a, quat b, float t)
{
    float d = quat_dot(a, b);

    /* Take the shortest path — if dot < 0, negate one quaternion */
    if (d < 0.0f) {
        b = quat_negate(b);
        d = -d;
    }

    /* If quaternions are very close, fall back to linear interpolation
     * to avoid division by sin(θ) ≈ 0 */
    if (d > 0.9995f) {
        quat result = quat_create(
            a.w + t * (b.w - a.w),
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z)
        );
        return quat_normalize(result);
    }

    float theta = acosf(d);          /* angle between quaternions */
    float sin_theta = sinf(theta);
    float wa = sinf((1.0f - t) * theta) / sin_theta;
    float wb = sinf(t * theta) / sin_theta;

    return quat_create(
        wa * a.w + wb * b.w,
        wa * a.x + wb * b.x,
        wa * a.y + wb * b.y,
        wa * a.z + wb * b.z
    );
}

/* Normalized linear interpolation between two quaternions.
 *
 * NLERP is the cheaper alternative to slerp: it linearly interpolates
 * the quaternion components and then normalizes. The result follows the
 * same path as slerp but at non-constant speed (faster near the middle,
 * slower near the endpoints).
 *
 * For small rotations or when constant speed isn't needed, nlerp is
 * often preferred because it's faster and commutative.
 *
 * Usage:
 *   quat mid = quat_nlerp(start, end, 0.5f);
 *
 * See: lessons/math/08-orientation
 */
static inline quat quat_nlerp(quat a, quat b, float t)
{
    /* Take shortest path */
    if (quat_dot(a, b) < 0.0f) {
        b = quat_negate(b);
    }

    quat result = quat_create(
        a.w + t * (b.w - a.w),
        a.x + t * (b.x - a.x),
        a.y + t * (b.y - a.y),
        a.z + t * (b.z - a.z)
    );
    return quat_normalize(result);
}

/* ── View Matrix / Virtual Camera ─────────────────────────────────────── *
 *
 * Naming convention: these functions use the library's standard type_verb
 * naming (quat_forward, mat4_view_from_quat), consistent with all other
 * functions in this header (vec3_dot, mat4_look_at, quat_slerp, etc.).
 * The forge_ prefix is reserved for non-type scalar helpers at the top of
 * this file (forge_lerpf, forge_clampf, etc.).
 *
 * See: lessons/math/09-view-matrix
 */

/* Extract the forward direction from a quaternion orientation.
 *
 * Returns where the camera (or object) is looking — the -Z direction
 * rotated by the quaternion. In our right-handed Y-up coordinate system
 * the default forward (identity quaternion) is (0, 0, -1).
 *
 * This is equivalent to rotating (0, 0, -1) by the quaternion, but
 * uses an optimized formula that avoids the full sandwich product:
 *   forward = q * (0, 0, -1) * q*
 *
 * Why -Z? In view space the camera looks down -Z. So an unrotated
 * camera's forward direction is (0, 0, -1).
 *
 * Usage:
 *   quat orientation = quat_from_euler(yaw, pitch, 0);
 *   vec3 fwd = quat_forward(orientation);
 *   // fwd points where the camera is looking
 *
 * See: lessons/math/09-view-matrix
 */
static inline vec3 quat_forward(quat q)
{
    /* Expanded from quat_rotate_vec3(q, {0, 0, -1}) */
    return vec3_create(
        -(2.0f * (q.x * q.z + q.w * q.y)),
        -(2.0f * (q.y * q.z - q.w * q.x)),
        -(1.0f - 2.0f * (q.x * q.x + q.y * q.y))
    );
}

/* Extract the right direction from a quaternion orientation.
 *
 * Returns the +X direction rotated by the quaternion — the direction
 * pointing to the camera's right. For an identity quaternion this
 * returns (1, 0, 0).
 *
 * Usage:
 *   vec3 right = quat_right(orientation);
 *   // Use for strafing: position += right * speed * dt
 *
 * See: lessons/math/09-view-matrix
 */
static inline vec3 quat_right(quat q)
{
    /* Expanded from quat_rotate_vec3(q, {1, 0, 0}) */
    return vec3_create(
        1.0f - 2.0f * (q.y * q.y + q.z * q.z),
        2.0f * (q.x * q.y + q.w * q.z),
        2.0f * (q.x * q.z - q.w * q.y)
    );
}

/* Extract the up direction from a quaternion orientation.
 *
 * Returns the +Y direction rotated by the quaternion — the direction
 * pointing above the camera's head. For an identity quaternion this
 * returns (0, 1, 0).
 *
 * Usage:
 *   vec3 up = quat_up(orientation);
 *   // Use for flying up: position += up * speed * dt
 *
 * See: lessons/math/09-view-matrix
 */
static inline vec3 quat_up(quat q)
{
    /* Expanded from quat_rotate_vec3(q, {0, 1, 0}) */
    return vec3_create(
        2.0f * (q.x * q.y - q.w * q.z),
        1.0f - 2.0f * (q.x * q.x + q.z * q.z),
        2.0f * (q.y * q.z + q.w * q.x)
    );
}

/* Create a view matrix from a camera position and quaternion orientation.
 *
 * The view matrix transforms world-space coordinates into view space
 * (camera space). It is the INVERSE of the camera's world transform:
 *
 *   Camera world transform:  T(pos) * R(orientation)
 *   View matrix:             R^-1 * T^-1
 *                          = R^T * T(-pos)
 *
 * Because the rotation part of an orthonormal matrix has its inverse
 * equal to its transpose, and for a unit quaternion the conjugate
 * gives the inverse rotation:
 *
 *   R^-1 columns = rows of camera basis vectors (right, up, -forward)
 *   Translation  = -R^-1 * pos (dot products of basis with position)
 *
 * This function is the quaternion-based alternative to mat4_look_at.
 * Use it when you store camera orientation as a quaternion (e.g., for a
 * first-person camera driven by mouse input):
 *
 *   mat4_look_at:        needs a target point (good for orbit cameras)
 *   mat4_view_from_quat: needs an orientation  (good for FPS cameras)
 *
 * Parameters:
 *   position    — camera position in world space
 *   orientation — camera orientation as a unit quaternion
 *
 * Usage:
 *   vec3 cam_pos = vec3_create(0, 2, 5);
 *   quat cam_rot = quat_from_euler(yaw, pitch, 0);
 *   mat4 view = mat4_view_from_quat(cam_pos, cam_rot);
 *
 * See: lessons/math/09-view-matrix
 * See: lessons/math/02-coordinate-spaces (view space in the pipeline)
 */
static inline mat4 mat4_view_from_quat(vec3 position, quat orientation)
{
    /* Extract camera basis vectors */
    vec3 right   = quat_right(orientation);
    vec3 up      = quat_up(orientation);
    vec3 forward = quat_forward(orientation);

    /* Build rotation part — rows are basis vectors (transpose of camera
     * orientation matrix). We negate forward because the camera looks
     * down -Z in view space. */
    mat4 m = {
        right.x,      up.x,     -forward.x,    0.0f,
        right.y,      up.y,     -forward.y,    0.0f,
        right.z,      up.z,     -forward.z,    0.0f,
        0.0f,         0.0f,      0.0f,         1.0f
    };

    /* Translation: dot each basis with -position */
    m.m[12] = -vec3_dot(right, position);
    m.m[13] = -vec3_dot(up, position);
    m.m[14] =  vec3_dot(forward, position);

    return m;
}

/* ── Rodrigues' Rotation (Axis-Angle on Vectors) ─────────────────────── */

/* Rotate a vector around an arbitrary axis by a given angle.
 *
 * This uses Rodrigues' rotation formula — a direct way to rotate a vector
 * without constructing a quaternion or matrix first. Useful for one-off
 * rotations or for understanding the geometry of rotation.
 *
 * Formula:
 *   v' = v·cos(θ) + (k × v)·sin(θ) + k·(k·v)·(1 - cos(θ))
 *
 * where k is the unit rotation axis and θ is the angle.
 *
 * Geometric intuition: decompose v into components parallel and
 * perpendicular to k. The parallel part stays fixed. The perpendicular
 * part rotates in the plane perpendicular to k.
 *
 * Parameters:
 *   v              — the vector to rotate
 *   axis           — rotation axis (must be unit length)
 *   angle_radians  — rotation angle in radians (positive = CCW when
 *                    looking down the axis toward the origin)
 *
 * Usage:
 *   vec3 v = vec3_create(1, 0, 0);
 *   vec3 axis = vec3_create(0, 1, 0);  // Y axis
 *   vec3 rotated = vec3_rotate_axis_angle(v, axis, FORGE_PI / 2);
 *   // rotated ≈ (0, 0, -1) — 90° around Y
 *
 * See: lessons/math/08-orientation
 */
static inline vec3 vec3_rotate_axis_angle(vec3 v, vec3 axis,
                                           float angle_radians)
{
    float c = cosf(angle_radians);
    float s = sinf(angle_radians);
    float k_dot_v = vec3_dot(axis, v);
    vec3 k_cross_v = vec3_cross(axis, v);

    /* v' = v*cos(θ) + (k×v)*sin(θ) + k*(k·v)*(1-cos(θ)) */
    return vec3_add(
        vec3_add(vec3_scale(v, c), vec3_scale(k_cross_v, s)),
        vec3_scale(axis, k_dot_v * (1.0f - c))
    );
}

/* ── Color Space Transforms ───────────────────────────────────────────── */
/*
 * Color science fundamentals for real-time graphics.
 *
 * Key principle: always do math (lighting, blending, interpolation) in
 * LINEAR space — apply gamma encoding only at the very end for display.
 * The sRGB transfer function is NOT a simple power curve; it has a linear
 * segment near black for numerical stability.
 *
 * Spaces covered:
 *   Linear RGB  — physically proportional light intensities (math here)
 *   sRGB        — perceptually encoded for display (gamma ~2.2)
 *   HSL / HSV   — hue-based representations for color picking / UI
 *   CIE XYZ     — device-independent reference (1931 standard observer)
 *   CIE xyY     — chromaticity (xy) + luminance (Y)
 *
 * Naming convention: color_<from>_to_<to> for conversions,
 *                    color_<property> for scalar queries.
 *
 * See: lessons/math/11-color-spaces
 */

/* ── Gamma / Linear Conversion (sRGB Transfer Function) ──────────────── */

/* Convert a single sRGB component (0-1) to linear light.
 *
 * The sRGB standard defines a piecewise transfer function — not a simple
 * pow(x, 2.2). Values near zero use a linear segment to avoid an infinite
 * slope at the origin:
 *
 *   if (s <= 0.04045)  linear = s / 12.92
 *   else               linear = ((s + 0.055) / 1.055) ^ 2.4
 *
 * Why this matters: lighting math (dot products, interpolation, blending)
 * must happen in linear space where doubling a value means doubling the
 * light intensity. sRGB values are perceptually spaced — they pack more
 * precision into dark tones where the human eye is most sensitive.
 *
 * Usage:
 *   float linear_r = color_srgb_to_linear(srgb_r);
 *
 * See: lessons/math/11-color-spaces
 */
static inline float color_srgb_to_linear(float s)
{
    if (s <= 0.04045f) {
        return s / 12.92f;
    }
    return powf((s + 0.055f) / 1.055f, 2.4f);
}

/* Convert a single linear-light component (0-1) to sRGB encoding.
 *
 * Inverse of color_srgb_to_linear. Apply this when writing final pixel
 * values for display on an sRGB monitor.
 *
 *   if (linear <= 0.0031308)  srgb = linear * 12.92
 *   else                      srgb = 1.055 * linear^(1/2.4) - 0.055
 *
 * Usage:
 *   float srgb_r = color_linear_to_srgb(linear_r);
 *
 * See: lessons/math/11-color-spaces
 */
static inline float color_linear_to_srgb(float linear)
{
    if (linear <= 0.0031308f) {
        return linear * 12.92f;
    }
    return 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f;
}

/* Convert an RGB color from sRGB encoding to linear light.
 *
 * Applies the sRGB-to-linear transfer function to each channel independently.
 * The alpha channel (if present) is NOT gamma-encoded and should not be
 * converted.
 *
 * Usage:
 *   vec3 srgb = vec3_create(0.5f, 0.5f, 0.5f);  // mid-gray in sRGB
 *   vec3 linear = color_srgb_to_linear_rgb(srgb); // ~0.214 in linear
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_srgb_to_linear_rgb(vec3 srgb)
{
    return vec3_create(
        color_srgb_to_linear(srgb.x),
        color_srgb_to_linear(srgb.y),
        color_srgb_to_linear(srgb.z)
    );
}

/* Convert an RGB color from linear light to sRGB encoding.
 *
 * Applies the linear-to-sRGB transfer function to each channel independently.
 *
 * Usage:
 *   vec3 linear = vec3_create(0.5f, 0.5f, 0.5f); // 50% light intensity
 *   vec3 srgb = color_linear_to_srgb_rgb(linear); // ~0.735 in sRGB
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_linear_to_srgb_rgb(vec3 linear)
{
    return vec3_create(
        color_linear_to_srgb(linear.x),
        color_linear_to_srgb(linear.y),
        color_linear_to_srgb(linear.z)
    );
}

/* ── Luminance ───────────────────────────────────────────────────────── */

/* Compute the relative luminance of a linear RGB color.
 *
 * Luminance is the perceptual brightness of a color as defined by the
 * CIE 1931 standard observer. The coefficients come from the sRGB/BT.709
 * color space primaries:
 *
 *   Y = 0.2126 * R + 0.7152 * G + 0.0722 * B
 *
 * Green dominates because human vision is most sensitive to green light.
 * Blue contributes very little because our S-cones are far less numerous.
 *
 * IMPORTANT: The input must be in LINEAR space. If you pass sRGB-encoded
 * values, the result will be wrong (too dark in the midtones).
 *
 * Usage:
 *   vec3 color = vec3_create(1.0f, 0.0f, 0.0f);  // pure red
 *   float lum = color_luminance(color);            // 0.2126
 *
 * See: lessons/math/11-color-spaces
 */
static inline float color_luminance(vec3 linear_rgb)
{
    return 0.2126f * linear_rgb.x
         + 0.7152f * linear_rgb.y
         + 0.0722f * linear_rgb.z;
}

/* ── RGB <-> HSL ──────────────────────────────────────────────────────── */

/* Convert a linear RGB color to HSL (Hue, Saturation, Lightness).
 *
 * HSL represents color as:
 *   H (hue):        0-360 degrees around the color wheel
 *                    0=red, 120=green, 240=blue
 *   S (saturation):  0-1, where 0 is gray and 1 is fully vivid
 *   L (lightness):   0-1, where 0 is black, 0.5 is pure color, 1 is white
 *
 * HSL is useful for color picking and artistic adjustments because hue,
 * vividness, and brightness are separated into independent axes.
 *
 * Note: the input should be in linear RGB. If you need to convert sRGB
 * values, call color_srgb_to_linear_rgb first.
 *
 * Returns: vec3 where x=H (0-360), y=S (0-1), z=L (0-1)
 *
 * Usage:
 *   vec3 red = vec3_create(1.0f, 0.0f, 0.0f);
 *   vec3 hsl = color_rgb_to_hsl(red);  // (0, 1, 0.5)
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_rgb_to_hsl(vec3 rgb)
{
    float r = rgb.x, g = rgb.y, b = rgb.z;
    float max_c = fmaxf(fmaxf(r, g), b);
    float min_c = fminf(fminf(r, g), b);
    float delta = max_c - min_c;

    /* Lightness is the average of the brightest and darkest channels */
    float l = (max_c + min_c) * 0.5f;

    if (delta < 1e-6f) {
        /* Achromatic (gray) — no hue or saturation */
        return vec3_create(0.0f, 0.0f, l);
    }

    /* Saturation depends on lightness:
     * For L <= 0.5:  S = delta / (max + min)
     * For L >  0.5:  S = delta / (2 - max - min)
     * This keeps S in [0,1] across the full lightness range. */
    float s = (l <= 0.5f)
        ? delta / (max_c + min_c)
        : delta / (2.0f - max_c - min_c);

    /* Hue: which 60-degree sextant of the color wheel */
    float h;
    if (max_c == r) {
        h = (g - b) / delta;
        if (h < 0.0f) h += 6.0f;
    } else if (max_c == g) {
        h = (b - r) / delta + 2.0f;
    } else {
        h = (r - g) / delta + 4.0f;
    }
    h *= 60.0f;

    return vec3_create(h, s, l);
}

/* Helper: convert a hue value to an RGB channel.
 * Used internally by color_hsl_to_rgb. */
static inline float color__hue_to_rgb(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 0.5f)         return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

/* Convert HSL to linear RGB.
 *
 * Input: vec3 where x=H (0-360), y=S (0-1), z=L (0-1)
 * Returns: vec3 with R, G, B in [0,1]
 *
 * Usage:
 *   vec3 hsl = vec3_create(120.0f, 1.0f, 0.5f);  // pure green
 *   vec3 rgb = color_hsl_to_rgb(hsl);             // (0, 1, 0)
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_hsl_to_rgb(vec3 hsl)
{
    float h = hsl.x / 360.0f; /* normalize to 0-1 */
    float s = hsl.y;
    float l = hsl.z;

    if (s < 1e-6f) {
        /* Achromatic */
        return vec3_create(l, l, l);
    }

    float q = (l < 0.5f) ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;

    return vec3_create(
        color__hue_to_rgb(p, q, h + 1.0f / 3.0f),
        color__hue_to_rgb(p, q, h),
        color__hue_to_rgb(p, q, h - 1.0f / 3.0f)
    );
}

/* ── RGB <-> HSV ──────────────────────────────────────────────────────── */

/* Convert a linear RGB color to HSV (Hue, Saturation, Value).
 *
 * HSV represents color as:
 *   H (hue):        0-360 degrees (same as HSL)
 *   S (saturation):  0-1, where 0 is white/gray, 1 is fully vivid
 *   V (value):       0-1, the brightness of the brightest channel
 *
 * HSV differs from HSL in how it defines "brightness":
 *   - HSV value = max(R,G,B) — the peak channel intensity
 *   - HSL lightness = (max+min)/2 — the midpoint
 *
 * HSV is common in color pickers (Photoshop, game engines) because
 * S=1, V=1 gives vivid colors at any hue, while HSL requires L=0.5.
 *
 * Returns: vec3 where x=H (0-360), y=S (0-1), z=V (0-1)
 *
 * Usage:
 *   vec3 orange = vec3_create(1.0f, 0.5f, 0.0f);
 *   vec3 hsv = color_rgb_to_hsv(orange);  // (30, 1, 1)
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_rgb_to_hsv(vec3 rgb)
{
    float r = rgb.x, g = rgb.y, b = rgb.z;
    float max_c = fmaxf(fmaxf(r, g), b);
    float min_c = fminf(fminf(r, g), b);
    float delta = max_c - min_c;

    float v = max_c;

    if (delta < 1e-6f) {
        return vec3_create(0.0f, 0.0f, v);
    }

    float s = delta / max_c;

    float h;
    if (max_c == r) {
        h = (g - b) / delta;
        if (h < 0.0f) h += 6.0f;
    } else if (max_c == g) {
        h = (b - r) / delta + 2.0f;
    } else {
        h = (r - g) / delta + 4.0f;
    }
    h *= 60.0f;

    return vec3_create(h, s, v);
}

/* Convert HSV to linear RGB.
 *
 * Input: vec3 where x=H (0-360), y=S (0-1), z=V (0-1)
 * Returns: vec3 with R, G, B in [0,1]
 *
 * Usage:
 *   vec3 hsv = vec3_create(240.0f, 1.0f, 1.0f);  // pure blue
 *   vec3 rgb = color_hsv_to_rgb(hsv);             // (0, 0, 1)
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_hsv_to_rgb(vec3 hsv)
{
    float h = hsv.x / 60.0f; /* 0-6 sextant index */
    float s = hsv.y;
    float v = hsv.z;

    if (s < 1e-6f) {
        return vec3_create(v, v, v);
    }

    int i = (int)floorf(h);
    float f = h - (float)i; /* fractional part within sextant */
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (i % 6) {
    case 0:  return vec3_create(v, t, p);
    case 1:  return vec3_create(q, v, p);
    case 2:  return vec3_create(p, v, t);
    case 3:  return vec3_create(p, q, v);
    case 4:  return vec3_create(t, p, v);
    default: return vec3_create(v, p, q);
    }
}

/* ── RGB <-> CIE XYZ (sRGB Primaries, D65 Illuminant) ────────────────── */

/* Convert linear sRGB to CIE 1931 XYZ.
 *
 * CIE XYZ is the device-independent reference color space defined by the
 * International Commission on Illumination (CIE) in 1931. It was designed
 * so that:
 *   X, Y, Z >= 0 for all visible colors
 *   Y = luminance (perceptual brightness)
 *   The space encompasses all colors a human can see
 *
 * The 3x3 matrix below converts from sRGB's primaries (red, green, blue
 * phosphor/LED colors on a standard monitor) to XYZ. The matrix is derived
 * from the chromaticity coordinates of the sRGB primaries and the D65
 * white point (daylight illuminant, 6504K).
 *
 * sRGB primary chromaticities (CIE xy):
 *   Red:   (0.6400, 0.3300)
 *   Green: (0.3000, 0.6000)
 *   Blue:  (0.1500, 0.0600)
 *   White: D65 = (0.3127, 0.3290)
 *
 * IMPORTANT: Input must be in LINEAR sRGB, not gamma-encoded sRGB.
 *
 * Usage:
 *   vec3 linear_rgb = vec3_create(1.0f, 0.0f, 0.0f);  // linear red
 *   vec3 xyz = color_linear_rgb_to_xyz(linear_rgb);
 *   // xyz ~ (0.4124, 0.2126, 0.0193) — red's position in XYZ
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_linear_rgb_to_xyz(vec3 rgb)
{
    /* sRGB to XYZ matrix (D65, row-by-row for readability) */
    return vec3_create(
        0.4124564f * rgb.x + 0.3575761f * rgb.y + 0.1804375f * rgb.z,
        0.2126729f * rgb.x + 0.7151522f * rgb.y + 0.0721750f * rgb.z,
        0.0193339f * rgb.x + 0.1191920f * rgb.y + 0.9503041f * rgb.z
    );
}

/* Convert CIE 1931 XYZ to linear sRGB.
 *
 * Inverse of color_linear_rgb_to_xyz. Note that XYZ values outside the
 * sRGB gamut will produce negative or >1 RGB components. Clamp after
 * conversion if needed for display.
 *
 * Usage:
 *   vec3 xyz = vec3_create(0.4124f, 0.2126f, 0.0193f);
 *   vec3 rgb = color_xyz_to_linear_rgb(xyz);  // ~ (1, 0, 0)
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_xyz_to_linear_rgb(vec3 xyz)
{
    /* XYZ to sRGB matrix (D65, inverse of the above) */
    return vec3_create(
         3.2404542f * xyz.x - 1.5371385f * xyz.y - 0.4985314f * xyz.z,
        -0.9692660f * xyz.x + 1.8760108f * xyz.y + 0.0415560f * xyz.z,
         0.0556434f * xyz.x - 0.2040259f * xyz.y + 1.0572252f * xyz.z
    );
}

/* ── CIE xyY (Chromaticity + Luminance) ──────────────────────────────── */

/* Convert CIE XYZ to CIE xyY (chromaticity coordinates + luminance).
 *
 * The CIE xy chromaticity diagram separates color (hue + saturation) from
 * brightness. Every color can be plotted as a point (x, y) on the
 * chromaticity diagram, regardless of how bright it is:
 *
 *   x = X / (X + Y + Z)     — red-green axis
 *   y = Y / (X + Y + Z)     — roughly a green axis
 *   Y = luminance            — carried through unchanged
 *
 * The third coordinate z = 1 - x - y is implicit and not stored.
 *
 * This is how color gamuts are visualized: the sRGB gamut is a triangle
 * on the xy diagram connecting the red, green, and blue primaries.
 *
 * Returns: vec3 where x=x, y=y, z=Y (luminance)
 *
 * Usage:
 *   vec3 xyz = color_linear_rgb_to_xyz(vec3_create(1, 0, 0));
 *   vec3 xyY = color_xyz_to_xyY(xyz);
 *   // xyY ~ (0.6400, 0.3300, 0.2126) — red primary chromaticity
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_xyz_to_xyY(vec3 xyz)
{
    float sum = xyz.x + xyz.y + xyz.z;
    if (sum < 1e-10f) {
        /* Black — use D65 white point chromaticity to avoid 0/0 */
        return vec3_create(0.3127f, 0.3290f, 0.0f);
    }
    return vec3_create(xyz.x / sum, xyz.y / sum, xyz.y);
}

/* Convert CIE xyY back to CIE XYZ.
 *
 * Reconstructs full XYZ from chromaticity (x, y) and luminance (Y):
 *   X = Y * x / y
 *   Z = Y * (1 - x - y) / y
 *
 * Input: vec3 where x=x, y=y, z=Y (luminance)
 *
 * Usage:
 *   vec3 xyY = vec3_create(0.3127f, 0.3290f, 1.0f);  // D65 white, Y=1
 *   vec3 xyz = color_xyY_to_xyz(xyY);
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_xyY_to_xyz(vec3 xyY)
{
    float cx = xyY.x;
    float cy = xyY.y;
    float Y  = xyY.z;
    if (cy < 1e-10f) {
        return vec3_create(0.0f, 0.0f, 0.0f);
    }
    return vec3_create(
        Y * cx / cy,
        Y,
        Y * (1.0f - cx - cy) / cy
    );
}

/* ── Tone Mapping (HDR -> LDR) ───────────────────────────────────────── */

/* Apply Reinhard tone mapping to a linear HDR color.
 *
 * The simplest global tone mapping operator. Maps the infinite range
 * [0, inf) to [0, 1):
 *
 *   mapped = color / (color + 1)
 *
 * Applied per-channel. This preserves hue but can desaturate bright
 * colors. For more control, use the luminance-based variant or
 * a filmic curve (ACES, AgX).
 *
 * Usage:
 *   vec3 hdr = vec3_create(4.0f, 2.0f, 1.0f);  // bright HDR color
 *   vec3 ldr = color_tonemap_reinhard(hdr);      // (0.80, 0.67, 0.50)
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_tonemap_reinhard(vec3 hdr)
{
    return vec3_create(
        hdr.x / (hdr.x + 1.0f),
        hdr.y / (hdr.y + 1.0f),
        hdr.z / (hdr.z + 1.0f)
    );
}

/* Apply exposure adjustment to an HDR color.
 *
 * Simulates a camera's exposure control. Multiplies the color by
 * 2^exposure, matching photographic stops:
 *
 *   +1 EV = double the light (one stop brighter)
 *   -1 EV = half the light (one stop darker)
 *    0 EV = no change
 *
 * Apply this BEFORE tone mapping.
 *
 * Usage:
 *   vec3 color = vec3_create(1.0f, 1.0f, 1.0f);
 *   vec3 bright = color_apply_exposure(color, 2.0f);   // 4x brighter
 *   vec3 dark   = color_apply_exposure(color, -1.0f);  // half brightness
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_apply_exposure(vec3 hdr, float exposure_ev)
{
    float scale = powf(2.0f, exposure_ev);
    return vec3_scale(hdr, scale);
}

/* Apply the ACES filmic tone mapping curve (Narkowicz 2015 fit).
 *
 * A widely-used filmic curve that produces a natural, film-like response
 * with a gentle highlight rolloff and slightly lifted blacks. This is the
 * simplified "Krzysztof Narkowicz" fit to the ACES Reference Rendering
 * Transform (RRT) + Output Device Transform (ODT):
 *
 *   f(x) = (x * (2.51x + 0.03)) / (x * (2.43x + 0.59) + 0.14)
 *
 * Input should be in linear sRGB (or a working space with similar
 * primaries). The output is in [0, 1] and should be gamma-encoded
 * for display.
 *
 * For production-quality ACES, a full ACES pipeline (AP0 -> RRT -> ODT)
 * is more accurate, but this fit is excellent for real-time use.
 *
 * Usage:
 *   vec3 hdr = vec3_create(4.0f, 2.0f, 1.0f);
 *   vec3 ldr = color_tonemap_aces(hdr);
 *   vec3 display = color_linear_to_srgb_rgb(ldr);
 *
 * See: lessons/math/11-color-spaces
 */
static inline vec3 color_tonemap_aces(vec3 hdr)
{
    /* Narkowicz 2015 ACES fit constants */
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;

    vec3 result;
    result.x = (hdr.x * (a * hdr.x + b)) / (hdr.x * (c * hdr.x + d) + e);
    result.y = (hdr.y * (a * hdr.y + b)) / (hdr.y * (c * hdr.y + d) + e);
    result.z = (hdr.z * (a * hdr.z + b)) / (hdr.z * (c * hdr.z + d) + e);

    /* Clamp to [0, 1] */
    result.x = forge_clampf(result.x, 0.0f, 1.0f);
    result.y = forge_clampf(result.y, 0.0f, 1.0f);
    result.z = forge_clampf(result.z, 0.0f, 1.0f);

    return result;
}

/* ── Hash Functions (Integer Hashing for Noise) ──────────────────────── */
/*
 * Deterministic integer hash functions for procedural noise, dithering,
 * and any situation requiring reproducible pseudorandom values without
 * mutable state.
 *
 * GPUs execute thousands of shader invocations in parallel. There is no
 * shared random number generator — each fragment must compute its own
 * "random" value from its coordinates. Hash functions fill this role:
 * given an integer seed (pixel position, frame index, etc.), they produce
 * a uniformly-distributed 32-bit output that looks random but is fully
 * deterministic and reproducible.
 *
 * Three hash functions are provided, each with different trade-offs:
 *   forge_hash_wang     — Thomas Wang (2007), fast, simple, well-known
 *   forge_hash_pcg      — PCG output permutation (O'Neill 2014), high quality
 *   forge_hash_xxhash32 — xxHash32 finalizer (Collet 2012), excellent avalanche
 *
 * Naming convention: forge_hash_ prefix (scalar helpers, not tied to a type).
 *
 * See: lessons/math/12-hash-functions
 */

/* Thomas Wang's 32-bit integer hash (2007).
 *
 * A fast multiply-xor-shift hash with good avalanche properties.
 * Each step serves a specific purpose:
 *   - XOR with shifted self: mixes upper bits into lower bits
 *   - Multiply by odd constant: spreads bit influence across all positions
 *
 * The constant 0x27d4eb2d (668,265,261) is a large odd number chosen to
 * maximize the avalanche effect — the probability that flipping one input
 * bit will flip any given output bit (ideally 50%).
 *
 * This is one of the most widely-used hash functions in shader code due to
 * its simplicity and speed.
 *
 * Usage:
 *   uint32_t h = forge_hash_wang(pixel_x + pixel_y * width);
 *   float noise = forge_hash_to_float(h);
 *
 * See: lessons/math/12-hash-functions
 */
static inline uint32_t forge_hash_wang(uint32_t key)
{
    key = (key ^ 61u) ^ (key >> 16);
    key *= 9u;
    key ^= key >> 4;
    key *= 0x27d4eb2du;
    key ^= key >> 15;
    return key;
}

/* PCG output permutation hash (based on O'Neill, 2014).
 *
 * Derived from Melissa O'Neill's Permuted Congruential Generator. This
 * version uses the PCG output permutation as a standalone hash function,
 * as described by Jarzynski & Olano (JCGT, 2020).
 *
 * The algorithm has two stages:
 *   1. Linear congruential step: state = input * 747796405 + 2891336453
 *      This spreads the input across the state using a carefully-chosen
 *      multiplier (found by O'Neill through statistical testing).
 *
 *   2. Output permutation: a data-dependent right-shift controlled by
 *      the top 4 bits of the state, followed by a multiply and final
 *      XOR. The data-dependent shift is the key insight — it makes the
 *      output depend on the input in a non-linear way.
 *
 * Higher quality than Wang hash, slightly more expensive.
 *
 * Usage:
 *   uint32_t h = forge_hash_pcg(seed);
 *   float r = forge_hash_to_float(h);
 *
 * See: lessons/math/12-hash-functions
 */
static inline uint32_t forge_hash_pcg(uint32_t input)
{
    uint32_t state = input * 747796405u + 2891336453u;
    uint32_t word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

/* xxHash32 avalanche finalizer (Collet, 2012).
 *
 * The finalization step from Yann Collet's xxHash, a fast non-cryptographic
 * hash used in compression (LZ4, Zstandard) and databases. This finalizer
 * ensures full avalanche — every input bit affects every output bit.
 *
 * The pattern — xor-shift, then multiply by a prime, repeated twice —
 * is the same structure used in MurmurHash3's finalizer. The specific
 * constants are xxHash's PRIME32_2 (0x85ebca77 = 2,246,822,519) and
 * PRIME32_3 (0xc2b2ae3d = 3,266,489,917), selected by Collet through
 * automated search to minimize statistical bias.
 *
 * Excellent avalanche properties. Useful as a general-purpose bit mixer.
 *
 * Usage:
 *   uint32_t h = forge_hash_xxhash32(pixel_index);
 *   float noise = forge_hash_to_float(h);
 *
 * See: lessons/math/12-hash-functions
 */
static inline uint32_t forge_hash_xxhash32(uint32_t h)
{
    h ^= h >> 15;
    h *= 0x85ebca77u;
    h ^= h >> 13;
    h *= 0xc2b2ae3du;
    h ^= h >> 16;
    return h;
}

/* ── Hash Seed Combination ───────────────────────────────────────────── */

/* Combine a hash seed with an additional value.
 *
 * Based on the widely-used boost::hash_combine pattern. Mixes a new
 * value into an existing seed using the golden ratio constant, addition,
 * and bidirectional shifts. This is how you build multi-dimensional
 * seeds from individual coordinates.
 *
 * The constant 0x9e3779b9 is floor(2^32 / phi), where phi is the golden
 * ratio (1 + sqrt(5)) / 2. The golden ratio has the slowest-converging
 * continued fraction of any irrational number, making it distribute
 * additive sequences as evenly as possible around the integer ring.
 *
 * Usage:
 *   uint32_t seed = 0;
 *   seed = forge_hash_combine(seed, pixel_x);
 *   seed = forge_hash_combine(seed, pixel_y);
 *   seed = forge_hash_combine(seed, frame);
 *   uint32_t h = forge_hash_wang(seed);
 *
 * See: lessons/math/12-hash-functions
 */
static inline uint32_t forge_hash_combine(uint32_t seed, uint32_t value)
{
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

/* Hash a 2D integer coordinate pair to a single uint32.
 *
 * Cascaded hashing: hash y first, XOR with x, then hash again. This
 * ensures that (1, 2) and (2, 1) produce different outputs, unlike
 * simple XOR (which is commutative).
 *
 * This is the standard approach for position-based shader noise:
 * convert pixel coordinates to integers and hash them.
 *
 * Usage:
 *   uint32_t h = forge_hash2d(pixel_x, pixel_y);
 *   float noise = forge_hash_to_float(h);
 *
 * See: lessons/math/12-hash-functions
 */
static inline uint32_t forge_hash2d(uint32_t x, uint32_t y)
{
    return forge_hash_wang(x ^ forge_hash_wang(y));
}

/* Hash a 3D integer coordinate triple to a single uint32.
 *
 * Extends the cascaded approach to three dimensions. Useful for
 * 3D noise or 2D noise with a time/frame seed:
 *   forge_hash3d(pixel_x, pixel_y, frame_index)
 *
 * Usage:
 *   uint32_t h = forge_hash3d(x, y, frame);
 *   float noise = forge_hash_to_float(h);
 *
 * See: lessons/math/12-hash-functions
 */
static inline uint32_t forge_hash3d(uint32_t x, uint32_t y, uint32_t z)
{
    return forge_hash_wang(x ^ forge_hash_wang(y ^ forge_hash_wang(z)));
}

/* ── Hash to Float Conversion ────────────────────────────────────────── */

/* Convert a 32-bit hash to a uniform float in [0, 1).
 *
 * Uses the top 24 bits of the hash (>> 8) divided by 2^24. Why 24 bits?
 * A 32-bit IEEE 754 float has 23 explicit mantissa bits plus 1 implicit
 * leading bit, giving 24 bits of integer precision. Every integer from
 * 0 to 2^24 (16,777,216) maps to a unique float value. Beyond 2^24,
 * consecutive integers map to the same float (rounding occurs).
 *
 * By restricting to 24 bits, we get exactly 16,777,216 uniformly-spaced
 * values in [0, 1) with no rounding gaps or duplicates.
 *
 * Usage:
 *   uint32_t h = forge_hash_wang(seed);
 *   float noise = forge_hash_to_float(h);  // [0.0, 1.0)
 *
 * See: lessons/math/12-hash-functions
 */
static inline float forge_hash_to_float(uint32_t h)
{
    return (float)(h >> 8) * (1.0f / 16777216.0f);
}

/* Convert a 32-bit hash to a uniform float in [-1, 1).
 *
 * Maps the hash to [0, 1) and then rescales to [-1, 1). Useful for
 * gradient noise where random directions can point in both positive
 * and negative directions.
 *
 * Usage:
 *   uint32_t h = forge_hash_wang(seed);
 *   float offset = forge_hash_to_sfloat(h);  // [-1.0, 1.0)
 *
 * See: lessons/math/12-hash-functions
 */
static inline float forge_hash_to_sfloat(uint32_t h)
{
    return forge_hash_to_float(h) * 2.0f - 1.0f;
}

#endif /* FORGE_MATH_H */
