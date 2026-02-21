/*
 * Math Lesson 13 -- Gradient Noise (Perlin & Simplex)
 *
 * Demonstrates:
 *   1. White noise vs gradient noise — why smooth noise matters
 *   2. The gradient noise algorithm — gradients, dot products, interpolation
 *   3. Fade curves — linear vs Perlin's quintic smoothstep
 *   4. 1D Perlin noise — ASCII waveform
 *   5. 2D Perlin noise — ASCII density map
 *   6. Simplex noise — triangular grid, comparison with Perlin
 *   7. fBm (octave stacking) — multi-scale detail
 *   8. Lacunarity and persistence — parameter effects
 *   9. Domain warping — organic distortion
 *  10. 3D Perlin noise — slicing through a volume
 *
 * This is a console program -- no window needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "math/forge_math.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

/* ASCII density ramp — 10 levels from empty to full.
 * Maps a value in [-1, 1] to a display character. */
static char density_char(float value)
{
    const char *ramp = " .:-=+*#%@";
    int ramp_len = 10;

    /* Map [-1, 1] to [0, 1] */
    float normalized = (value + 1.0f) * 0.5f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    int idx = (int)(normalized * (float)(ramp_len - 1) + 0.5f);
    if (idx >= ramp_len) idx = ramp_len - 1;
    return ramp[idx];
}

/* ── 1. White Noise vs Gradient Noise ────────────────────────────────── */

static void demo_white_vs_gradient(void)
{
    print_header("1. WHITE NOISE vs GRADIENT NOISE");

    printf("\n  White noise (hash-based) has no spatial correlation:\n");
    printf("  each sample is independent of its neighbors.\n\n");

    printf("  White noise (32 samples along a line):\n  ");
    for (int i = 0; i < 32; i++) {
        float v = forge_hash_to_sfloat(forge_hash_wang((uint32_t)i));
        printf("%c", density_char(v));
    }
    printf("\n\n");

    printf("  Gradient noise (Perlin) produces smooth, continuous values:\n");
    printf("  nearby inputs give nearby outputs.\n\n");

    printf("  Perlin noise (32 samples, frequency = 0.15):\n  ");
    for (int i = 0; i < 32; i++) {
        float v = forge_noise_perlin1d((float)i * 0.15f, 42u);
        /* Scale up for visibility (1D Perlin has small amplitude) */
        printf("%c", density_char(v * 3.0f));
    }
    printf("\n\n");

    printf("  The key difference: gradient noise is coherent.\n");
    printf("  It varies smoothly, creating natural-looking patterns\n");
    printf("  suitable for terrain, clouds, textures, and animation.\n");
}

/* ── 2. The Gradient Noise Algorithm ─────────────────────────────────── */

static void demo_gradient_algorithm(void)
{
    print_header("2. THE GRADIENT NOISE ALGORITHM (1D example)");

    float x = 2.7f;
    int ix = (int)floorf(x);
    float fx = x - (float)ix;

    printf("\n  Step 1 -- Find the grid cell:\n");
    printf("    Sample point: x = %.1f\n", x);
    printf("    Grid points:  %d (left)  and  %d (right)\n", ix, ix + 1);
    printf("    Fractional:   fx = %.1f  (distance from left grid point)\n\n", fx);

    /* Hash to get gradients */
    uint32_t h_left  = forge_hash_wang((uint32_t)ix ^ 42u);
    uint32_t h_right = forge_hash_wang((uint32_t)(ix + 1) ^ 42u);
    float g_left  = (h_left  & 1u) ? -1.0f : 1.0f;
    float g_right = (h_right & 1u) ? -1.0f : 1.0f;

    printf("  Step 2 -- Assign gradients at grid points:\n");
    printf("    hash(%d) -> gradient = %+.0f (slope %s)\n",
           ix, g_left, g_left > 0 ? "up" : "down");
    printf("    hash(%d) -> gradient = %+.0f (slope %s)\n\n",
           ix + 1, g_right, g_right > 0 ? "up" : "down");

    float d_left  = g_left  * fx;
    float d_right = g_right * (fx - 1.0f);

    printf("  Step 3 -- Dot product (gradient * distance):\n");
    printf("    left:  %+.0f * %.1f  = %+.2f\n", g_left, fx, d_left);
    printf("    right: %+.0f * %.1f = %+.2f\n\n",
           g_right, fx - 1.0f, d_right);

    float u = forge_noise_fade(fx);
    float result = d_left + u * (d_right - d_left);

    printf("  Step 4 -- Smooth interpolation:\n");
    printf("    fade(%.1f) = %.4f  (quintic smoothstep)\n", fx, u);
    printf("    result = lerp(%.2f, %.2f, %.4f) = %.4f\n\n",
           d_left, d_right, u, result);

    float direct = forge_noise_perlin1d(x, 42u);
    printf("  Verify: forge_noise_perlin1d(%.1f, 42) = %.4f\n", x, direct);
}

/* ── 3. Fade Curves ──────────────────────────────────────────────────── */

static void demo_fade_curves(void)
{
    print_header("3. FADE CURVES: Linear vs Cubic vs Quintic");

    printf("\n  The fade curve controls how we interpolate between grid\n");
    printf("  points. Each generation provides smoother continuity:\n\n");

    printf("  Linear:   f(t) = t                     (C0 -- value matches)\n");
    printf("  Cubic:    f(t) = 3t^2 - 2t^3            (C1 -- 1st derivative = 0 at ends)\n");
    printf("  Quintic:  f(t) = 6t^5 - 15t^4 + 10t^3   (C2 -- 1st and 2nd deriv = 0)\n\n");

    printf("  %-6s  %-10s  %-10s  %-10s\n",
           "t", "Linear", "Cubic", "Quintic");
    printf("  %-6s  %-10s  %-10s  %-10s\n",
           "------", "----------", "----------", "----------");

    for (int i = 0; i <= 10; i++) {
        float t = (float)i / 10.0f;
        float linear = t;
        float cubic = t * t * (3.0f - 2.0f * t);
        float quintic = forge_noise_fade(t);
        printf("  %5.2f   %9.6f   %9.6f   %9.6f\n",
               t, linear, cubic, quintic);
    }

    printf("\n  All three agree at t=0, t=0.5, and t=1.\n");
    printf("  Cubic (Perlin 1985): zero slope at endpoints (C1).\n");
    printf("    Removes visible seams, but 2nd derivative jumps.\n");
    printf("  Quintic (Perlin 2002): zero slope AND curvature at endpoints (C2).\n");
    printf("    Both the noise and its gradient are smooth everywhere.\n");
}

/* ── 4. 1D Perlin Noise Waveform ─────────────────────────────────────── */

static void demo_1d_perlin(void)
{
    print_header("4. 1D PERLIN NOISE: ASCII Waveform");

    printf("\n  Perlin noise along a line, plotted as a waveform.\n");
    printf("  The x-axis is position, y-axis is noise value.\n\n");

    #define WAVE_WIDTH  64
    #define WAVE_HEIGHT 13
    #define WAVE_MID    (WAVE_HEIGHT / 2)

    char wave[WAVE_HEIGHT][WAVE_WIDTH + 1];

    /* Initialize grid with spaces */
    for (int r = 0; r < WAVE_HEIGHT; r++) {
        for (int c = 0; c < WAVE_WIDTH; c++) {
            wave[r][c] = ' ';
        }
        wave[r][WAVE_WIDTH] = '\0';
    }

    /* Plot noise values */
    for (int c = 0; c < WAVE_WIDTH; c++) {
        float x = (float)c * 0.12f;
        float n = forge_noise_perlin1d(x, 42u);

        /* Map noise [-0.5, 0.5] to row [0, WAVE_HEIGHT-1] */
        int row = WAVE_MID - (int)(n * (float)(WAVE_HEIGHT - 1));
        if (row < 0) row = 0;
        if (row >= WAVE_HEIGHT) row = WAVE_HEIGHT - 1;
        wave[row][c] = '*';
    }

    /* Print the waveform with left axis */
    for (int r = 0; r < WAVE_HEIGHT; r++) {
        if (r == 0)
            printf("  +0.5 |%s|\n", wave[r]);
        else if (r == WAVE_MID)
            printf("   0.0 |%s|\n", wave[r]);
        else if (r == WAVE_HEIGHT - 1)
            printf("  -0.5 |%s|\n", wave[r]);
        else
            printf("       |%s|\n", wave[r]);
    }

    printf("\n  The curve is smooth and continuous — no abrupt jumps.\n");
    printf("  Grid points (where gradients live) are spaced at integer\n");
    printf("  coordinates. The frequency parameter (0.12) controls\n");
    printf("  how quickly the pattern varies.\n");

    #undef WAVE_WIDTH
    #undef WAVE_HEIGHT
    #undef WAVE_MID
}

/* ── 5. 2D Perlin Noise ──────────────────────────────────────────────── */

static void demo_2d_perlin(void)
{
    print_header("5. 2D PERLIN NOISE: ASCII Density Map");

    printf("\n  2D Perlin noise rendered as a density map.\n");
    printf("  Each character represents a noise value at that position.\n\n");

    int width  = 60;
    int height = 20;
    float scale = 0.08f;

    for (int y = 0; y < height; y++) {
        printf("  ");
        for (int x = 0; x < width; x++) {
            float n = forge_noise_perlin2d(
                (float)x * scale, (float)y * scale, 42u);
            printf("%c", density_char(n * 2.0f));
        }
        printf("\n");
    }

    printf("\n  Light areas (@ #) represent high noise values.\n");
    printf("  Dark areas (. :) represent low noise values.\n");
    printf("  The pattern is smooth — neighboring pixels have\n");
    printf("  similar values, creating organic-looking blobs.\n");
}

/* ── 6. Simplex Noise ────────────────────────────────────────────────── */

static void demo_simplex(void)
{
    print_header("6. SIMPLEX NOISE: Triangular Grid");

    printf("\n  Simplex noise uses a triangular grid instead of squares.\n");
    printf("  Advantages:\n");
    printf("    - 3 gradient evaluations per sample (vs 4 for Perlin 2D)\n");
    printf("    - Better isotropy (no axis-aligned grid bias)\n");
    printf("    - Scales better to higher dimensions (N+1 vs 2^N corners)\n\n");

    int width  = 30;
    int height = 20;
    float scale = 0.08f;

    printf("  Perlin 2D:                                                Simplex 2D:\n");

    for (int y = 0; y < height; y++) {
        printf("  ");
        /* Perlin column */
        for (int x = 0; x < width; x++) {
            float n = forge_noise_perlin2d(
                (float)x * scale, (float)y * scale, 42u);
            printf("%c", density_char(n * 2.0f));
        }

        printf("    ");

        /* Simplex column */
        for (int x = 0; x < width; x++) {
            float n = forge_noise_simplex2d(
                (float)x * scale, (float)y * scale, 42u);
            printf("%c", density_char(n * 1.5f));
        }
        printf("\n");
    }

    printf("\n  Both produce smooth noise, but simplex has rounder\n");
    printf("  features (better isotropy) and fewer directional artifacts.\n");
}

/* ── 7. fBm (Octave Stacking) ───────────────────────────────────────── */

static void demo_fbm(void)
{
    print_header("7. fBm: Fractal Brownian Motion (Octave Stacking)");

    printf("\n  fBm stacks multiple 'octaves' of noise at increasing\n");
    printf("  frequencies and decreasing amplitudes. More octaves = more\n");
    printf("  fine detail, like zooming into a coastline.\n\n");

    int width  = 30;
    int height = 10;
    float scale = 0.06f;
    int octave_counts[] = {1, 2, 4, 8};

    for (int oi = 0; oi < 4; oi++) {
        int octaves = octave_counts[oi];
        printf("  %d octave%s:\n", octaves, octaves > 1 ? "s" : "");

        for (int y = 0; y < height; y++) {
            printf("  ");
            for (int x = 0; x < width; x++) {
                float n = forge_noise_fbm2d(
                    (float)x * scale, (float)y * scale,
                    42u, octaves, 2.0f, 0.5f);
                printf("%c", density_char(n * 2.5f));
            }
            printf("\n");
        }
        printf("\n");
    }

    printf("  1 octave: smooth blobs (low frequency only).\n");
    printf("  2 octaves: adds medium-scale variation.\n");
    printf("  4 octaves: visible fine detail emerging.\n");
    printf("  8 octaves: rich, multi-scale texture.\n");
}

/* ── 8. Lacunarity and Persistence ───────────────────────────────────── */

static void demo_lacunarity_persistence(void)
{
    print_header("8. LACUNARITY & PERSISTENCE: Controlling fBm Character");

    printf("\n  Lacunarity: frequency multiplier per octave.\n");
    printf("    Higher lacunarity = more separation between scales.\n");
    printf("    Typical: 2.0 (each octave doubles frequency).\n\n");

    printf("  Persistence: amplitude multiplier per octave.\n");
    printf("    Higher persistence = more influence from fine detail.\n");
    printf("    Typical: 0.5 (each octave halves amplitude).\n\n");

    int width  = 30;
    int height = 8;
    float scale = 0.06f;
    int octaves = 6;

    /* Show different persistence values */
    float persist_values[] = {0.3f, 0.5f, 0.7f};
    const char *persist_names[] = {
        "Persistence = 0.3 (smooth, dominated by large features)",
        "Persistence = 0.5 (balanced, natural look)",
        "Persistence = 0.7 (rough, strong fine detail)"
    };

    for (int pi = 0; pi < 3; pi++) {
        printf("  %s:\n", persist_names[pi]);

        for (int y = 0; y < height; y++) {
            printf("  ");
            for (int x = 0; x < width; x++) {
                float n = forge_noise_fbm2d(
                    (float)x * scale, (float)y * scale,
                    42u, octaves, 2.0f, persist_values[pi]);
                printf("%c", density_char(n * 2.5f));
            }
            printf("\n");
        }
        printf("\n");
    }

    /* Show different lacunarity values */
    float lac_values[] = {1.5f, 2.0f, 3.0f};
    const char *lac_names[] = {
        "Lacunarity = 1.5 (scales overlap, softer detail)",
        "Lacunarity = 2.0 (standard octave doubling)",
        "Lacunarity = 3.0 (wide gaps between scales, crisper)"
    };

    for (int li = 0; li < 3; li++) {
        printf("  %s:\n", lac_names[li]);

        for (int y = 0; y < height; y++) {
            printf("  ");
            for (int x = 0; x < width; x++) {
                float n = forge_noise_fbm2d(
                    (float)x * scale, (float)y * scale,
                    42u, octaves, lac_values[li], 0.5f);
                printf("%c", density_char(n * 2.5f));
            }
            printf("\n");
        }
        printf("\n");
    }

    printf("  Lacunarity and persistence together determine the\n");
    printf("  'roughness' and character of the noise. Terrain\n");
    printf("  generation typically uses lacunarity=2.0, persistence=0.5.\n");
}

/* ── 9. Domain Warping ───────────────────────────────────────────────── */

static void demo_domain_warping(void)
{
    print_header("9. DOMAIN WARPING: Organic Distortion");

    printf("\n  Domain warping distorts the input coordinates before\n");
    printf("  sampling noise. The result looks like swirling, fluid\n");
    printf("  patterns — organic shapes that are difficult to achieve\n");
    printf("  with standard fBm alone.\n\n");

    printf("  Method (3 independent noise layers):\n");
    printf("    1. Sample fBm at (x, y) with seed s   -> warp offset dx\n");
    printf("    2. Sample fBm at (x, y) with seed s+1 -> warp offset dy\n");
    printf("    3. Sample fBm at (x + k*dx, y + k*dy) with seed s+2\n");
    printf("  Different seeds ensure dx and dy are uncorrelated.\n\n");

    int width  = 30;
    int height = 12;
    float scale = 0.06f;

    printf("  Plain fBm:                        Domain warped (strength=2.5):\n");

    for (int y = 0; y < height; y++) {
        printf("  ");
        /* Plain fBm */
        for (int x = 0; x < width; x++) {
            float n = forge_noise_fbm2d(
                (float)x * scale, (float)y * scale,
                42u, 4, 2.0f, 0.5f);
            printf("%c", density_char(n * 2.5f));
        }

        printf("    ");

        /* Domain-warped */
        for (int x = 0; x < width; x++) {
            float n = forge_noise_domain_warp2d(
                (float)x * scale, (float)y * scale,
                42u, 2.5f);
            printf("%c", density_char(n * 2.5f));
        }
        printf("\n");
    }

    printf("\n  The warped version has flowing, marble-like patterns.\n");
    printf("  Higher warp strength = more extreme distortion.\n");
    printf("  This method is used for marble, wood grain, lava, and\n");
    printf("  terrain with organic-looking erosion features.\n");
}

/* ── 10. 3D Perlin Noise ─────────────────────────────────────────────── */

static void demo_3d_perlin(void)
{
    print_header("10. 3D PERLIN NOISE: Slicing Through a Volume");

    printf("\n  3D noise fills a volume. By fixing z and sampling (x, y),\n");
    printf("  we see cross-sections. Different z values reveal different\n");
    printf("  slices of the same coherent 3D pattern.\n\n");

    int width  = 30;
    int height = 8;
    float scale = 0.1f;
    float z_slices[] = {0.0f, 1.5f, 3.0f};

    for (int zi = 0; zi < 3; zi++) {
        printf("  z = %.1f:\n", z_slices[zi]);

        for (int y = 0; y < height; y++) {
            printf("  ");
            for (int x = 0; x < width; x++) {
                float n = forge_noise_perlin3d(
                    (float)x * scale, (float)y * scale,
                    z_slices[zi], 42u);
                printf("%c", density_char(n * 2.5f));
            }
            printf("\n");
        }
        printf("\n");
    }

    printf("  Each slice is a smooth 2D pattern, and adjacent z-values\n");
    printf("  produce similar (but not identical) patterns. Animating z\n");
    printf("  over time creates smoothly evolving 2D noise.\n");
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("=============================================================\n");
    printf("  Math Lesson 13 -- Gradient Noise (Perlin & Simplex)\n");
    printf("=============================================================\n");

    demo_white_vs_gradient();
    demo_gradient_algorithm();
    demo_fade_curves();
    demo_1d_perlin();
    demo_2d_perlin();
    demo_simplex();
    demo_fbm();
    demo_lacunarity_persistence();
    demo_domain_warping();
    demo_3d_perlin();

    printf("\n=============================================================\n");
    printf("  See README.md for diagrams and detailed explanations.\n");
    printf("  See common/math/forge_math.h for the implementations.\n");
    printf("=============================================================\n\n");

    SDL_Quit();
    return 0;
}
