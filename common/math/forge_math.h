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

#include <math.h>  /* sqrtf, sinf, cosf, etc. */

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
 * See: lessons/math/02-matrices
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
 * See: lessons/math/02-matrices
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
 * See: lessons/math/02-matrices
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
 * See: lessons/math/02-matrices
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
 * See: lessons/math/02-matrices
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
 * See: lessons/math/02-matrices
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
 * See: lessons/math/02-matrices
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
 * See: lessons/math/02-matrices
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

#endif /* FORGE_MATH_H */
