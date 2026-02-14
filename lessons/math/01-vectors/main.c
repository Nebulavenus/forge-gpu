/*
 * Math Lesson 01 — Vectors
 *
 * Demonstrates vector operations: addition, subtraction, dot product,
 * cross product, normalization, and linear interpolation.
 *
 * This is a console program that prints examples of each operation,
 * showing the geometric meaning of vector math.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Helper: Print a vec3 ───────────────────────────────────────────────── */

static void print_vec3(const char *name, vec3 v)
{
    SDL_Log("%s = (%.3f, %.3f, %.3f)", name, v.x, v.y, v.z);
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("\n=== Vector Math Demo ===\n");

    /* ── Vector Creation ──────────────────────────────────────────────── */
    SDL_Log("--- Creating Vectors ---");
    vec3 a = vec3_create(1.0f, 0.0f, 0.0f);  /* X-axis unit vector */
    vec3 b = vec3_create(0.0f, 1.0f, 0.0f);  /* Y-axis unit vector */
    vec3 c = vec3_create(3.0f, 4.0f, 0.0f);  /* Arbitrary vector */

    print_vec3("a (X-axis)", a);
    print_vec3("b (Y-axis)", b);
    print_vec3("c", c);
    SDL_Log("");

    /* ── Vector Addition ──────────────────────────────────────────────── */
    /* Adding vectors geometrically means placing b's tail at a's head.
     * The result points from the origin to the combined displacement. */
    SDL_Log("--- Addition ---");
    vec3 sum = vec3_add(a, b);
    print_vec3("a + b", sum);
    SDL_Log("Geometric meaning: diagonal direction (northeast)");
    SDL_Log("");

    /* ── Vector Subtraction ───────────────────────────────────────────── */
    /* Subtracting b from a gives the vector pointing from b to a. */
    SDL_Log("--- Subtraction ---");
    vec3 diff = vec3_sub(a, b);
    print_vec3("a - b", diff);
    SDL_Log("Geometric meaning: vector from b to a");
    SDL_Log("");

    /* ── Scalar Multiplication (Scaling) ──────────────────────────────── */
    /* Scaling changes the length but not the direction. */
    SDL_Log("--- Scaling ---");
    vec3 scaled = vec3_scale(c, 2.0f);
    print_vec3("c * 2", scaled);
    SDL_Log("Geometric meaning: same direction, twice as long");
    SDL_Log("");

    /* ── Dot Product ──────────────────────────────────────────────────── */
    /* The dot product measures alignment between vectors.
     *   - Positive: pointing in similar directions
     *   - Zero: perpendicular
     *   - Negative: pointing in opposite directions
     * Formula: |a| * |b| * cos(θ) */
    SDL_Log("--- Dot Product ---");

    float dot_perpendicular = vec3_dot(a, b);
    SDL_Log("a · b = %.3f (perpendicular → 0)", dot_perpendicular);

    vec3 parallel = vec3_create(2.0f, 0.0f, 0.0f);
    float dot_parallel = vec3_dot(a, parallel);
    SDL_Log("a · (2,0,0) = %.3f (parallel → positive)", dot_parallel);

    vec3 opposite = vec3_create(-1.0f, 0.0f, 0.0f);
    float dot_opposite = vec3_dot(a, opposite);
    SDL_Log("a · (-1,0,0) = %.3f (opposite → negative)", dot_opposite);
    SDL_Log("");

    /* ── Length and Normalization ─────────────────────────────────────── */
    /* Length (magnitude) is the distance from the origin.
     * Normalizing makes a vector unit length (length = 1) while keeping
     * its direction. Unit vectors are useful for representing directions. */
    SDL_Log("--- Length and Normalization ---");

    float length_c = vec3_length(c);
    SDL_Log("Length of c = %.3f", length_c);

    vec3 normalized_c = vec3_normalize(c);
    print_vec3("Normalized c", normalized_c);
    SDL_Log("Length of normalized c = %.3f (should be 1.0)",
            vec3_length(normalized_c));
    SDL_Log("");

    /* ── Cross Product (3D only) ──────────────────────────────────────── */
    /* The cross product of two 3D vectors produces a third vector
     * perpendicular to both. It follows the right-hand rule:
     *   - Point fingers along the first vector
     *   - Curl them toward the second vector
     *   - Thumb points along the result
     *
     * Common uses:
     *   - Computing surface normals: cross(edge1, edge2)
     *   - Building coordinate frames: right = cross(up, forward) */
    SDL_Log("--- Cross Product ---");

    vec3 cross_ab = vec3_cross(a, b);
    print_vec3("a × b", cross_ab);
    SDL_Log("Result is perpendicular to both a and b");
    SDL_Log("In right-handed coords: X × Y = Z");

    /* Verify perpendicularity: dot product should be zero */
    float check_a = vec3_dot(cross_ab, a);
    float check_b = vec3_dot(cross_ab, b);
    SDL_Log("(a × b) · a = %.3f (should be 0)", check_a);
    SDL_Log("(a × b) · b = %.3f (should be 0)", check_b);
    SDL_Log("");

    /* ── Linear Interpolation (Lerp) ──────────────────────────────────── */
    /* Lerp smoothly blends between two vectors.
     *   t=0 → returns the first vector
     *   t=1 → returns the second vector
     *   t=0.5 → returns the midpoint
     * Useful for smooth movement and animation. */
    SDL_Log("--- Linear Interpolation ---");

    vec3 start = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 end = vec3_create(10.0f, 10.0f, 0.0f);

    vec3 lerp_0 = vec3_lerp(start, end, 0.0f);
    vec3 lerp_half = vec3_lerp(start, end, 0.5f);
    vec3 lerp_1 = vec3_lerp(start, end, 1.0f);

    print_vec3("lerp(start, end, 0.0)", lerp_0);
    print_vec3("lerp(start, end, 0.5)", lerp_half);
    print_vec3("lerp(start, end, 1.0)", lerp_1);
    SDL_Log("Geometric meaning: smooth path from start to end");
    SDL_Log("");

    /* ── Summary ──────────────────────────────────────────────────────── */
    SDL_Log("=== Summary ===");
    SDL_Log("Vectors represent position, direction, and displacement.");
    SDL_Log("Vector operations let us combine, compare, and transform them.");
    SDL_Log("These are the building blocks of 3D graphics and physics!");
    SDL_Log("");

    SDL_Quit();
    return 0;
}
