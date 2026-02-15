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

#include <math.h>  /* sqrtf, sinf, cosf, tanf, etc. */

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

#endif /* FORGE_MATH_H */
