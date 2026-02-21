/*
 * my_vec.h — A tiny header-only 2D vector library
 *
 * This file IS the lesson.  Every line demonstrates a pattern used in real
 * header-only libraries like forge_math.h.  Read the comments carefully —
 * they explain not just what each construct does, but why it's needed.
 *
 * Key patterns demonstrated:
 *   1. Include guards (#ifndef / #define / #endif)
 *   2. static inline functions
 *   3. Type definitions in headers
 *   4. Constants with #define
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── Include Guard ────────────────────────────────────────────────────────
 *
 * These three lines (#ifndef, #define at the top, #endif at the bottom)
 * prevent this file from being processed more than once per translation
 * unit (per .c file that the compiler processes).
 *
 * How it works:
 *   1. First time the preprocessor sees #include "my_vec.h":
 *      MY_VEC_H is not yet defined, so #ifndef MY_VEC_H passes.
 *      #define MY_VEC_H sets it, and the file contents are included.
 *
 *   2. Second time (if included again, or via another header):
 *      MY_VEC_H is already defined, so #ifndef MY_VEC_H fails.
 *      Everything is skipped down to the #endif at the bottom.
 *
 * Without this guard, including the same header twice in one .c file
 * would cause "redefinition of struct" or "redefinition of macro" errors.
 *
 * Convention: the guard name matches the filename in UPPER_SNAKE_CASE.
 *   forge_math.h  ->  FORGE_MATH_H
 *   my_vec.h      ->  MY_VEC_H
 */
#ifndef MY_VEC_H
#define MY_VEC_H

#include <math.h>  /* sqrtf */

/* ── Constants ────────────────────────────────────────────────────────────
 *
 * #define constants are safe in headers.  The preprocessor replaces every
 * occurrence with the literal value before compilation even begins, so
 * they produce no symbols in object files and cannot violate the
 * one-definition rule.
 */
#define MY_VEC_PI 3.14159265f

/* ── Type Definition ──────────────────────────────────────────────────────
 *
 * typedef struct is safe in headers because it only describes the layout
 * of a type — it does not allocate any storage.  Types exist only at
 * compile time and produce no symbols in the object file.
 *
 * Multiple translation units can see the same typedef without conflict,
 * as long as the definitions are identical (which they are, since they
 * all come from this one header file).
 */
typedef struct Vec2 {
    float x, y;
} Vec2;

/* ── Functions: static inline ────────────────────────────────────────────
 *
 * Every function in a header-only library must be declared 'static inline'.
 *
 * Why 'static'?
 *   Gives each translation unit (.c file) its own private copy of the
 *   function.  When main.c and physics.c both include this header, the
 *   linker sees two separate, private functions — not two conflicting
 *   public definitions.  Without 'static', you get a "multiple definition"
 *   linker error.  This is the one-definition rule (ODR) at work.
 *
 * Why 'inline'?
 *   Tells the compiler to substitute the function body at the call site
 *   instead of emitting a function call.  For small math functions (like
 *   vector add), this eliminates call overhead entirely.  Without 'inline',
 *   the function still compiles correctly ('static' alone is sufficient
 *   for correctness), but the compiler is less likely to inline it.
 *
 * Together, 'static inline' means:
 *   "Each .c file gets its own copy, and the compiler should inline it."
 *   This is the standard pattern for header-only C libraries.
 *
 * NOTE: Function names use the Vec2_ prefix to match the type name,
 * following the same convention as forge_math.h (vec3_create, vec4_add, etc).
 * This ensures names don't conflict with other libraries.
 */

/* Create a Vec2 from x and y components. */
static inline Vec2 Vec2_create(float x, float y)
{
    Vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

/* Add two vectors component-wise: (a.x+b.x, a.y+b.y). */
static inline Vec2 Vec2_add(Vec2 a, Vec2 b)
{
    return Vec2_create(a.x + b.x, a.y + b.y);
}

/* Scale a vector by a scalar: (v.x*s, v.y*s). */
static inline Vec2 Vec2_scale(Vec2 v, float s)
{
    return Vec2_create(v.x * s, v.y * s);
}

/* Compute the length (magnitude) of a vector: sqrt(x^2 + y^2). */
static inline float Vec2_length(Vec2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

/* Return a unit-length vector pointing in the same direction.
 * Returns (0, 0) for zero-length input to avoid division by zero. */
static inline Vec2 Vec2_normalize(Vec2 v)
{
    float len = Vec2_length(v);
    if (len > 0.0001f) {
        return Vec2_scale(v, 1.0f / len);
    }
    return Vec2_create(0.0f, 0.0f);
}

#endif /* MY_VEC_H */
