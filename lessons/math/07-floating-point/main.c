/*
 * Math Lesson 07 — Floating Point
 *
 * How computers represent real numbers, and why it matters for graphics.
 *
 * Sections:
 *   1.  Fixed-point as motivation — why integers aren't enough
 *   2.  IEEE 754 representation — sign, exponent, mantissa
 *   3.  How precision varies — more bits near zero, fewer far away
 *   4.  Epsilon and equality — absolute vs relative tolerance
 *   5.  Depth buffer precision — z-fighting and non-linear depth
 *   6.  float vs double — 32-bit vs 64-bit trade-offs
 *   7.  Summary
 *
 * New math library additions in this lesson:
 *   FORGE_EPSILON, forge_approx_equalf, forge_rel_equalf
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>  /* memcpy */
#include <stdint.h>  /* uint32_t, uint64_t */
#include <float.h>   /* FLT_EPSILON, FLT_MAX, FLT_MIN, DBL_EPSILON */
#include <SDL3/SDL_main.h>
#include "math/forge_math.h"

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Section 1: Fixed-point */
#define FIXED_SCALE        256     /* 8 fractional bits */
#define INT16_MAX_VALUE  32767     /* max signed 16-bit integer */

/* Section 3: Precision ranges and accumulation */
#define PRECISION_NUM_RANGES  8
#define ACCUM_ITERATION_COUNT 1000000
#define ACCUM_STEP            0.1f

/* Section 5: Depth buffer */
#define DEPTH_NEAR           0.1f
#define DEPTH_FAR          100.0f
#define DEPTH_NUM_SAMPLES   10
#define Z_FIGHT_SEPARATION   0.01f  /* distance between two surfaces */
#define DEPTH_EPSILON        6e-8f  /* ~1/2^24, 24-bit depth buffer precision */
#define ZFIGHT_OFFSET_COUNT  4      /* number of test distances */

/* Section 6: float vs double comparison */
#define ITERATION_COUNT  10000000

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/* Extract the bit representation of a 32-bit float.
 * We use memcpy instead of a union to be fully standards-compliant. */
static uint32_t float_to_bits(float f)
{
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

/* Print the IEEE 754 bit layout of a float:
 *   S EEEEEEEE MMMMMMMMMMMMMMMMMMMMMMM
 *   (1 sign bit, 8 exponent bits, 23 mantissa bits) */
static void print_float_bits(const char *label, float f)
{
    uint32_t bits = float_to_bits(f);

    uint32_t sign     = (bits >> 31) & 1;
    uint32_t exponent = (bits >> 23) & 0xFF;
    uint32_t mantissa = bits & 0x7FFFFF;

    /* Build binary string: S EEEEEEEE MMMMMMMMMMMMMMMMMMMMMMM */
    char bin[36];  /* 1 + 1 + 8 + 1 + 23 + 1 = 35 chars + null */
    int pos = 0;

    bin[pos++] = sign ? '1' : '0';
    bin[pos++] = ' ';

    for (int i = 7; i >= 0; i--) {
        bin[pos++] = (exponent >> i) & 1 ? '1' : '0';
    }
    bin[pos++] = ' ';

    for (int i = 22; i >= 0; i--) {
        bin[pos++] = (mantissa >> i) & 1 ? '1' : '0';
    }
    bin[pos] = '\0';

    int biased_exp = (int)exponent;
    int actual_exp = biased_exp - 127;

    SDL_Log("  %s = %g", label, f);
    SDL_Log("    bits: %s", bin);
    SDL_Log("    sign=%u  exponent=%d (biased %d)  mantissa=0x%06X",
            sign, actual_exp, biased_exp, mantissa);
}

/* Compute the spacing between consecutive floats at value f.
 * This is the smallest change that can be represented at this magnitude. */
static float float_spacing_at(float f)
{
    /* Use the standard technique: increment the bit pattern by 1 */
    uint32_t bits = float_to_bits(f);
    uint32_t next_bits = bits + 1;
    float next;
    memcpy(&next, &next_bits, sizeof(next));
    return next - f;
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("=============================================================\n");
    printf("  Math Lesson 07 — Floating Point\n");
    printf("  How computers represent real numbers (and where they fail)\n");
    printf("=============================================================\n\n");

    /* ── Section 1: Fixed-point as motivation ────────────────────────────
     *
     * Before floating point, computers used FIXED-POINT arithmetic:
     * an integer where some bits represent the fractional part.
     *
     * Example: 8.8 fixed-point uses 8 bits for the integer part and
     * 8 bits for the fractional part (multiply by 256 to store, divide
     * by 256 to read back).
     *
     * Problem: fixed range AND fixed precision. You can't represent both
     * very large and very small numbers with the same format.
     */
    printf("-- 1. Fixed-point as motivation --------------------------------\n\n");
    printf("  Before floating point, computers used fixed-point:\n");
    printf("  An integer where some bits are the fractional part.\n\n");

    {
        /* 8.8 fixed-point: multiply by 256 to store */
        int fp_pi      = (int)(3.14159f * FIXED_SCALE);  /* 804 */
        int fp_small   = (int)(0.01f * FIXED_SCALE);     /* 2 (rounded!) */
        int fp_big     = (int)(100.5f * FIXED_SCALE);    /* 25728 */

        float back_pi    = (float)fp_pi / FIXED_SCALE;
        float back_small = (float)fp_small / FIXED_SCALE;
        float back_big   = (float)fp_big / FIXED_SCALE;

        SDL_Log("  8.8 fixed-point (scale = %d):", FIXED_SCALE);
        SDL_Log("    pi    = 3.14159 -> stored as %5d -> back = %.5f (error = %.5f)",
                fp_pi, back_pi, 3.14159f - back_pi);
        SDL_Log("    small = 0.01    -> stored as %5d -> back = %.5f (error = %.5f)",
                fp_small, back_small, 0.01f - back_small);
        SDL_Log("    big   = 100.5   -> stored as %5d -> back = %.5f (error = %.5f)",
                fp_big, back_big, 100.5f - back_big);
        printf("\n");

        /* Show the precision problem */
        int fp_tiny = (int)(0.001f * FIXED_SCALE);   /* 0! */
        float back_tiny = (float)fp_tiny / FIXED_SCALE;
        SDL_Log("  The problem: 0.001 -> stored as %d -> back = %.5f",
                fp_tiny, back_tiny);
        SDL_Log("  Smallest representable value: 1/%d = %.6f",
                FIXED_SCALE, 1.0f / FIXED_SCALE);
        SDL_Log("  Largest (16-bit signed):      %d / %d = %.1f",
                INT16_MAX_VALUE, FIXED_SCALE, (float)INT16_MAX_VALUE / FIXED_SCALE);
        printf("\n");
        printf("  Fixed-point has constant precision everywhere.\n");
        printf("  Floating-point lets precision FLOAT to where you need it.\n\n");
    }

    /* ── Section 2: IEEE 754 representation ──────────────────────────────
     *
     * A 32-bit float has three fields:
     *
     *   [S] [EEEEEEEE] [MMMMMMMMMMMMMMMMMMMMMMM]
     *    1      8               23 bits
     *   sign  exponent        mantissa (fraction)
     *
     * Value = (-1)^S  *  2^(E-127)  *  (1 + M/2^23)
     *
     * The "1 +" is implicit (the "hidden bit") — you get 24 bits of
     * precision from only 23 stored mantissa bits.
     *
     * The exponent "floats" the decimal point to the right magnitude,
     * and the mantissa provides relative precision at that scale.
     */
    printf("-- 2. IEEE 754 representation ----------------------------------\n\n");
    printf("  A 32-bit float:\n");
    printf("    [S] [EEEEEEEE] [MMMMMMMMMMMMMMMMMMMMMMM]\n");
    printf("     1      8               23 bits\n");
    printf("    sign  exponent        mantissa (fraction)\n\n");
    printf("  Value = (-1)^S  *  2^(E-127)  *  (1 + M/2^23)\n\n");

    {
        print_float_bits("1.0", 1.0f);
        printf("\n");
        print_float_bits("-1.0", -1.0f);
        printf("\n");
        print_float_bits("0.5", 0.5f);
        printf("\n");
        print_float_bits("2.0", 2.0f);
        printf("\n");
        print_float_bits("0.1", 0.1f);
        printf("\n");

        /* Show that 0.1 can't be represented exactly */
        SDL_Log("  Note: 0.1 in binary is 0.0001100110011... (repeating)");
        SDL_Log("  It cannot be stored exactly in any finite binary format.");
        SDL_Log("  Stored value: %.20f", 0.1f);
        SDL_Log("  Error:        %.20f", (double)0.1f - 0.1);
        printf("\n");

        /* Special values */
        printf("  Special values:\n");
        print_float_bits("+0.0", 0.0f);
        printf("\n");
        print_float_bits("-0.0", -0.0f);
        printf("\n");
        /* Compute infinity at runtime to avoid compile-time divide by zero */
        float pos_one = 1.0f;
        float pos_zero = 0.0f;
        print_float_bits("+Inf", pos_one / pos_zero);
        printf("\n");
        /* Compute NaN at runtime to avoid compile-time divide by zero */
        float nan_val = pos_zero / pos_zero;
        print_float_bits(" NaN", nan_val);
        printf("\n");
    }

    /* ── Section 3: How precision varies ─────────────────────────────────
     *
     * This is the KEY insight about floating point:
     *   - Near zero, the spacing between consecutive floats is TINY
     *   - Far from zero, the spacing is LARGE
     *
     * Specifically, the spacing at value f is approximately:
     *   spacing ~= f * FLT_EPSILON  (where FLT_EPSILON ~= 1.19e-7)
     *
     * This means you get about 7 decimal digits of precision everywhere,
     * but the absolute precision depends on the magnitude of the number.
     */
    printf("-- 3. How precision varies across the number line --------------\n\n");
    printf("  Floats have ~7 decimal digits of precision at ANY magnitude.\n");
    printf("  But the absolute spacing between consecutive values changes:\n\n");

    {
        float values[] = {
            1.0f, 10.0f, 100.0f, 1000.0f,
            10000.0f, 100000.0f, 1000000.0f, 16777216.0f
        };

        SDL_Log("       value      |   spacing (eps at value)   | digits of precision");
        SDL_Log("  ----------------|---------------------------|--------------------");

        for (int i = 0; i < PRECISION_NUM_RANGES; i++) {
            float v = values[i];
            float eps = float_spacing_at(v);
            /* Count digits: -log10(eps / v) */
            float rel = eps / v;
            float digits = -log10f(rel);
            SDL_Log("  %14.1f  |  %25.15f  |  ~%.1f", v, eps, digits);
        }
        printf("\n");

        /* The critical consequence for graphics */
        SDL_Log("  Key insight: 16,777,216 + 1 = ?");
        float big = 16777216.0f;  /* 2^24 — exactly the mantissa capacity */
        float big_plus_one = big + 1.0f;
        SDL_Log("    16777216.0f + 1.0f = %.1f", big_plus_one);
        SDL_Log("    They're equal! At this magnitude, 1.0 is below the");
        SDL_Log("    spacing between consecutive floats.");
        printf("\n");

        /* Demonstrate accumulation error */
        float sum = 0.0f;
        float expected_sum = ACCUM_STEP * ACCUM_ITERATION_COUNT;
        for (int i = 0; i < ACCUM_ITERATION_COUNT; i++) {
            sum += ACCUM_STEP;
        }
        SDL_Log("  Accumulation error: %.1f added %d times", ACCUM_STEP, ACCUM_ITERATION_COUNT);
        SDL_Log("    Expected: %.1f", expected_sum);
        SDL_Log("    Got:      %.6f", sum);
        SDL_Log("    Error:    %.6f", sum - expected_sum);
        printf("\n");
    }

    /* ── Section 4: Epsilon and equality testing ─────────────────────────
     *
     * Because of rounding, you should NEVER compare floats with ==.
     * Instead, check if they're "close enough."
     *
     * Two approaches:
     *   1. Absolute tolerance:  |a - b| < epsilon
     *      - Good for values near zero
     *      - Breaks for large values (epsilon too small to matter)
     *
     *   2. Relative tolerance:  |a - b| < epsilon * max(|a|, |b|)
     *      - Good for values of any magnitude
     *      - Breaks near zero (epsilon * 0 = 0)
     *
     * Best practice: combine both (absolute OR relative).
     * The math library now provides forge_approx_equalf (absolute)
     * and forge_rel_equalf (relative).
     */
    printf("-- 4. Epsilon and equality testing ------------------------------\n\n");
    printf("  NEVER compare floats with ==. Check \"close enough\" instead.\n\n");

    {
        /* Show why == fails.
         * sqrt(2) * sqrt(2) should be exactly 2, but rounding in
         * sqrtf introduces a tiny error. This reliably demonstrates
         * the problem on all platforms. */
        float a = sqrtf(2.0f) * sqrtf(2.0f);
        float b = 2.0f;
        SDL_Log("  sqrt(2) * sqrt(2) == 2?");
        SDL_Log("    sqrtf(2) * sqrtf(2) = %.20f", a);
        SDL_Log("    2.0f                = %.20f", b);
        SDL_Log("    Equal (==): %s", (a == b) ? "YES" : "NO");
        SDL_Log("    Difference: %.20f", a - b);
        printf("\n");

        /* Absolute tolerance */
        SDL_Log("  Approach 1: Absolute tolerance (|a - b| < epsilon)");
        SDL_Log("    forge_approx_equalf(sqrt2*sqrt2, 2.0, 1e-6) = %s",
                forge_approx_equalf(a, b, 1e-6f) ? "true" : "false");
        printf("\n");

        /* Show where absolute tolerance fails */
        float big_a = 1000000.0f;
        float big_b = 1000000.0625f;  /* smallest representable step above big_a is ~0.0625 */
        SDL_Log("  But absolute tolerance fails for large numbers:");
        SDL_Log("    a = %.4f, b = %.4f", big_a, big_b);
        SDL_Log("    |a - b| = %.4f", big_b - big_a);
        SDL_Log("    forge_approx_equalf(a, b, 1e-6) = %s  (too strict!)",
                forge_approx_equalf(big_a, big_b, 1e-6f) ? "true" : "false");
        printf("\n");

        /* Relative tolerance */
        SDL_Log("  Approach 2: Relative tolerance (|a - b| < eps * max(|a|,|b|))");
        SDL_Log("    forge_rel_equalf(1000000.0, 1000000.0625, 1e-6) = %s",
                forge_rel_equalf(1000000.0f, 1000000.0625f, 1e-6f) ? "true" : "false");
        printf("\n");

        /* Show where relative tolerance fails (near zero) */
        float tiny_a = 1e-10f;
        float tiny_b = 2e-10f;
        SDL_Log("  But relative tolerance fails near zero:");
        SDL_Log("    a = %.1e, b = %.1e", tiny_a, tiny_b);
        SDL_Log("    forge_rel_equalf(a, b, 1e-6) = %s  (b is 2x a!)",
                forge_rel_equalf(tiny_a, tiny_b, 1e-6f) ? "true" : "false");
        printf("\n");

        /* Combined approach */
        SDL_Log("  Best practice: combine both (absolute OR relative):");
        SDL_Log("    |a - b| < abs_eps  OR  |a - b| < rel_eps * max(|a|,|b|)");
        printf("\n");

        /* Practical example with FORGE_EPSILON */
        SDL_Log("  FORGE_EPSILON = %e (= FLT_EPSILON = %e)", FORGE_EPSILON, FLT_EPSILON);
        SDL_Log("  Use it as a baseline for building tolerances.");
        printf("\n");
    }

    /* ── Section 5: Depth buffer precision ───────────────────────────────
     *
     * The depth buffer is where floating-point precision matters MOST
     * in graphics. Here's why:
     *
     * The perspective matrix maps view-space z to NDC z with:
     *   ndc_z = (far / (near - far)) + (far * near / (near - far)) / (-z_view)
     *
     * This is a HYPERBOLIC mapping: lots of precision near the near plane,
     * almost none near the far plane. At far distances, many different
     * z values map to the same depth buffer value → Z-FIGHTING.
     *
     * Z-fighting: two surfaces at slightly different depths flicker
     * because the depth buffer can't tell them apart.
     */
    printf("-- 5. Depth buffer precision (z-fighting) ----------------------\n\n");
    printf("  The perspective depth mapping is non-linear (hyperbolic).\n");
    printf("  Most precision is near the near plane; far plane has almost none.\n\n");

    {
        float near = DEPTH_NEAR;
        float far = DEPTH_FAR;
        mat4 proj = mat4_perspective(60.0f * FORGE_DEG2RAD, 1.0f, near, far);

        SDL_Log("  Perspective: near=%.1f, far=%.1f", near, far);
        SDL_Log("  How view-space depth maps to NDC z [0, 1]:\n");

        SDL_Log("   view-space z  |  NDC z     |  depth buffer bits used");
        SDL_Log("  --------------|------------|------------------------");

        /* Sample at various depths */
        float depths[] = {
            -0.1f, -0.2f, -0.5f, -1.0f, -2.0f,
            -5.0f, -10.0f, -25.0f, -50.0f, -100.0f
        };

        float prev_ndc = 0.0f;
        for (int i = 0; i < DEPTH_NUM_SAMPLES; i++) {
            vec4 view_pt = vec4_create(0.0f, 0.0f, depths[i], 1.0f);
            vec4 clip = mat4_multiply_vec4(proj, view_pt);
            vec3 ndc = vec3_perspective_divide(clip);

            float ndc_range = ndc.z - prev_ndc;
            /* Express as percentage of the full [0,1] depth range */
            SDL_Log("    z = %7.1f  |  %.6f  |  %.1f%% of depth range%s",
                    depths[i], ndc.z, ndc_range * 100.0f,
                    i == 0 ? " (near plane)" : "");
            prev_ndc = ndc.z;
        }
        printf("\n");

        /* Show the consequence: precision at different distances */
        SDL_Log("  Depth precision (spacing between consecutive values):");
        SDL_Log("    At z=-0.1 (near):  ndc_z=0.0, precision is excellent");
        SDL_Log("    At z=-1.0:         first 90%% of depth range already used!");
        SDL_Log("    At z=-50 to -100:  only ~0.1%% of range for half the scene");
        printf("\n");

        /* Z-fighting demonstration */
        printf("  Z-fighting example:\n");
        printf("  Two surfaces %.2f units apart at different distances:\n\n",
               Z_FIGHT_SEPARATION);

        float offsets[ZFIGHT_OFFSET_COUNT] = { 1.0f, 10.0f, 50.0f, 90.0f };

        SDL_Log("    distance  | surface A ndc_z | surface B ndc_z | difference");
        SDL_Log("   -----------|----------------|----------------|----------");
        for (int i = 0; i < ZFIGHT_OFFSET_COUNT; i++) {
            float z_a = -offsets[i];
            float z_b = -(offsets[i] + Z_FIGHT_SEPARATION);

            vec4 clip_a = mat4_multiply_vec4(proj, vec4_create(0, 0, z_a, 1));
            vec4 clip_b = mat4_multiply_vec4(proj, vec4_create(0, 0, z_b, 1));
            vec3 ndc_a = vec3_perspective_divide(clip_a);
            vec3 ndc_b = vec3_perspective_divide(clip_b);

            float diff = ndc_b.z - ndc_a.z;

            /* A 24-bit depth buffer has precision of about 1/2^24 */
            int resolvable = (diff > DEPTH_EPSILON) ? 1 : 0;

            SDL_Log("    z=%5.1f   |    %.8f    |    %.8f    |  %.2e  %s",
                    z_a, ndc_a.z, ndc_b.z, diff,
                    resolvable ? "[OK]" : "[Z-FIGHT!]");
        }
        printf("\n");

        /* Mitigation strategies */
        printf("  How to reduce z-fighting:\n");
        printf("    1. Push the near plane as far as possible (0.1 not 0.001)\n");
        printf("    2. Reduce far/near ratio (1000:1 is better than 100000:1)\n");
        printf("    3. Use reversed-Z (maps near->1, far->0) for better distribution\n");
        printf("    4. Use a 32-bit depth buffer (D32_FLOAT) instead of 24-bit\n\n");
    }

    /* ── Section 6: float vs double ──────────────────────────────────────
     *
     * 64-bit double has:
     *   - 1 sign bit, 11 exponent bits, 52 mantissa bits
     *   - ~15-16 decimal digits of precision (vs ~7 for float)
     *   - Range: ~1e-308 to ~1e+308 (vs ~1e-38 to ~1e+38)
     *
     * Why do GPUs use 32-bit float?
     *   - Twice the throughput (process two 32-bit values per 64-bit lane)
     *   - Half the memory bandwidth
     *   - Half the register usage
     *   - 7 digits is enough for most pixel-level computations
     *   - GPU memory bandwidth is the bottleneck, not precision
     *
     * When DO you need double?
     *   - Large-world coordinates (open-world games)
     *   - Scientific computation
     *   - Accumulating many small values (physics simulation)
     *   - Anything where errors compound over millions of operations
     */
    printf("-- 6. float vs double (32-bit vs 64-bit) -----------------------\n\n");

    {
        SDL_Log("  Comparison:");
        SDL_Log("                |  float (32-bit)  |  double (64-bit)");
        SDL_Log("  --------------|-----------------|------------------");
        SDL_Log("  Sign bits     |        1        |        1");
        SDL_Log("  Exponent bits |        8        |       11");
        SDL_Log("  Mantissa bits |       23        |       52");
        SDL_Log("  Total bits    |       32        |       64");
        SDL_Log("  Precision     |   ~7 digits     |  ~15 digits");
        SDL_Log("  Epsilon       |  %.2e     |  %.2e", FLT_EPSILON, DBL_EPSILON);
        SDL_Log("  Max value     |  %.2e     |  %.2e", (double)FLT_MAX, DBL_MAX);
        SDL_Log("  Min positive  |  %.2e     |  %.2e", (double)FLT_MIN, DBL_MIN);
        printf("\n");

        /* Precision comparison */
        float  f_third = 1.0f / 3.0f;
        double d_third = 1.0 / 3.0;
        SDL_Log("  1/3 as float:  %.20f", f_third);
        SDL_Log("  1/3 as double: %.20f", d_third);
        printf("\n");

        /* Accumulation comparison */
        float  f_sum = 0.0f;
        double d_sum = 0.0;
        double expected = (double)ACCUM_STEP * ITERATION_COUNT;
        for (int i = 0; i < ITERATION_COUNT; i++) {
            f_sum += ACCUM_STEP;
            d_sum += (double)ACCUM_STEP;
        }
        SDL_Log("  Adding %.1f %d times:", ACCUM_STEP, ITERATION_COUNT);
        SDL_Log("    float  result: %15.6f  (error: %.6f)", f_sum, f_sum - (float)expected);
        SDL_Log("    double result: %15.6f  (error: %.6f)", d_sum, d_sum - expected);
        printf("\n");

        /* Why GPUs use float */
        printf("  Why GPUs favor 32-bit float:\n");
        printf("    * 2x throughput (two floats per 64-bit ALU lane)\n");
        printf("    * 2x memory bandwidth savings\n");
        printf("    * 7 digits is enough for screen-space pixel computation\n");
        printf("    * A 4K display is ~4000 pixels wide: needs only 4 digits\n");
        printf("    * Colors are 8-bit per channel: 3 digits is plenty\n\n");

        printf("  When you need double on the CPU:\n");
        printf("    * Large-world coordinates (> 10 km precision to mm)\n");
        printf("    * Physics accumulation over many frames\n");
        printf("    * Intermediate calculations that get cast back to float\n");
        printf("    * Example: compute world position in double, then subtract\n");
        printf("      camera position -> small float for GPU rendering\n\n");
    }

    /* ── Section 7: Summary ──────────────────────────────────────────────── */
    printf("-- 7. Summary --------------------------------------------------\n\n");
    printf("  IEEE 754 floating-point:\n");
    printf("    * sign + exponent + mantissa = (-1)^S * 2^(E-127) * (1 + M)\n");
    printf("    * ~7 decimal digits of precision (32-bit float)\n");
    printf("    * Precision is RELATIVE: more near zero, less far away\n\n");

    printf("  Equality testing:\n");
    printf("    * NEVER use == for floats\n");
    printf("    * Absolute tolerance: |a-b| < eps (good near zero)\n");
    printf("    * Relative tolerance: |a-b| < eps * max(|a|,|b|) (good everywhere else)\n");
    printf("    * Use forge_approx_equalf() and forge_rel_equalf()\n\n");

    printf("  Depth buffer precision:\n");
    printf("    * Perspective maps z hyperbolically -> non-linear precision\n");
    printf("    * Most precision at near plane, almost none at far\n");
    printf("    * Z-fighting happens when surfaces are too close at far distances\n");
    printf("    * Mitigations: push near plane out, reversed-Z, 32-bit depth\n\n");

    printf("  float vs double:\n");
    printf("    * GPUs use float: 2x speed, enough for pixel-level math\n");
    printf("    * Use double on CPU for world coordinates, physics accumulation\n\n");

    printf("  New math library functions:\n");
    printf("    * FORGE_EPSILON               — machine epsilon for float\n");
    printf("    * forge_approx_equalf(a,b,e)  — absolute tolerance comparison\n");
    printf("    * forge_rel_equalf(a,b,e)     — relative tolerance comparison\n\n");

    printf("  See: lessons/math/07-floating-point/README.md\n");
    printf("  See: lessons/math/06-projections/ (depth mapping in practice)\n");
    printf("  See: lessons/gpu/06-depth-and-3d/ (depth buffer in action)\n\n");

    SDL_Quit();
    return 0;
}
