/*
 * Math Lesson 14 -- Blue Noise & Low-Discrepancy Sequences
 *
 * Demonstrates:
 *   1. Why random sampling clumps — white noise vs low-discrepancy
 *   2. The Halton sequence — radical inverse in prime bases
 *   3. The R2 sequence — additive recurrence from the plastic constant
 *   4. The Sobol sequence — direction numbers and bit operations
 *   5. Blue noise — Mitchell's best candidate algorithm
 *   6. Discrepancy measurement — quantifying uniformity
 *   7. Application: dithering — replacing banding with imperceptible noise
 *   8. Application: sampling — anti-aliasing with low-discrepancy points
 *   9. Application: stippling — density-driven point placement
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

/* ── Helpers ──────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

/* Map [0, 1] to a density character for ASCII plots */
static char density_char(float value)
{
    const char *ramp = " .:-=+*#%@";
    int ramp_len = 10;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    int idx = (int)(value * (float)(ramp_len - 1) + 0.5f);
    if (idx >= ramp_len) idx = ramp_len - 1;
    return ramp[idx];
}

/* ── 1. Why Random Sampling Clumps ───────────────────────────────── */

static void demo_clumping(void)
{
    print_header("1. WHY RANDOM SAMPLING CLUMPS AND GAPS");

    printf("\n  Uniform random sampling (white noise) does NOT distribute\n");
    printf("  points evenly. By the birthday paradox, random points\n");
    printf("  inevitably cluster in some regions and leave others empty.\n\n");

    /* Generate random 2D points */
    #define GRID_SIZE 40
    #define NUM_RANDOM 80
    #define NUM_LDS    80

    char random_grid[GRID_SIZE][GRID_SIZE];
    char halton_grid[GRID_SIZE][GRID_SIZE];

    /* Clear grids */
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            random_grid[y][x] = '.';
            halton_grid[y][x] = '.';
        }
    }

    /* Place random points */
    for (int i = 0; i < NUM_RANDOM; i++) {
        uint32_t h = forge_hash_wang((uint32_t)i ^ 12345u);
        int px = (int)(forge_hash_to_float(h) * (float)(GRID_SIZE - 1));
        h = forge_hash_wang(h);
        int py = (int)(forge_hash_to_float(h) * (float)(GRID_SIZE - 1));
        if (px >= 0 && px < GRID_SIZE && py >= 0 && py < GRID_SIZE)
            random_grid[py][px] = '*';
    }

    /* Place Halton points */
    for (int i = 1; i <= NUM_LDS; i++) {
        float hx = forge_halton((uint32_t)i, 2);
        float hy = forge_halton((uint32_t)i, 3);
        int px = (int)(hx * (float)(GRID_SIZE - 1));
        int py = (int)(hy * (float)(GRID_SIZE - 1));
        if (px >= 0 && px < GRID_SIZE && py >= 0 && py < GRID_SIZE)
            halton_grid[py][px] = '*';
    }

    printf("  White noise (%d points):         Halton sequence (%d points):\n",
           NUM_RANDOM, NUM_LDS);

    for (int y = 0; y < GRID_SIZE; y++) {
        printf("  ");
        for (int x = 0; x < GRID_SIZE; x++) {
            printf("%c", random_grid[y][x]);
        }
        printf("  ");
        for (int x = 0; x < GRID_SIZE; x++) {
            printf("%c", halton_grid[y][x]);
        }
        printf("\n");
    }

    printf("\n  Notice: the random points form visible clumps and gaps.\n");
    printf("  The Halton points fill the space more evenly -- no large\n");
    printf("  empty regions, no tight clusters.\n");

    #undef GRID_SIZE
    #undef NUM_RANDOM
    #undef NUM_LDS
}

/* ── 2. Halton Sequence ──────────────────────────────────────────── */

static void demo_halton(void)
{
    print_header("2. THE HALTON SEQUENCE: Radical Inverse");

    printf("\n  The Halton sequence builds coordinates by reversing the\n");
    printf("  digits of an index in different prime bases.\n\n");

    printf("  Base-2 radical inverse (Van der Corput):\n");
    printf("  %-8s %-12s %-10s\n", "Index", "Binary", "Reversed");
    printf("  %-8s %-12s %-10s\n", "--------", "------------", "----------");

    for (uint32_t i = 1; i <= 8; i++) {
        float val = forge_halton(i, 2);

        /* Print binary representation */
        char binary[16];
        uint32_t tmp = i;
        int bits = 0;
        while (tmp > 0) {
            binary[bits++] = (char)('0' + (tmp & 1));
            tmp >>= 1;
        }
        /* Reverse to get MSB-first */
        char bin_str[16];
        for (int b = 0; b < bits; b++) {
            bin_str[b] = binary[bits - 1 - b];
        }
        bin_str[bits] = '\0';

        printf("  %-8u %-12s 0.%-8s = %.6f\n", i, bin_str, binary, val);
    }

    printf("\n  2D Halton uses base 2 for x and base 3 for y:\n\n");
    printf("  %-6s  %-12s  %-12s\n", "Index", "x (base 2)", "y (base 3)");
    printf("  %-6s  %-12s  %-12s\n", "------", "------------", "------------");

    for (uint32_t i = 1; i <= 12; i++) {
        printf("  %-6u  %-12.6f  %-12.6f\n",
               i, forge_halton(i, 2), forge_halton(i, 3));
    }

    printf("\n  Each new point lands in the largest remaining gap.\n");
    printf("  This is why Halton has much lower discrepancy than random.\n");
}

/* ── 3. R2 Sequence ──────────────────────────────────────────────── */

static void demo_r2(void)
{
    print_header("3. THE R2 SEQUENCE: Plastic Constant Recurrence");

    printf("\n  R2 is the simplest high-quality 2D low-discrepancy sequence.\n");
    printf("  It uses additive recurrence with constants derived from the\n");
    printf("  plastic constant (p ~ 1.3247), the unique real root of x^3 = x + 1.\n\n");

    printf("  Formula:\n");
    printf("    x_n = frac(0.5 + n * 1/p)     where 1/p   ~ 0.7549\n");
    printf("    y_n = frac(0.5 + n * 1/p^2)   where 1/p^2 ~ 0.5698\n\n");

    printf("  %-6s  %-12s  %-12s\n", "Index", "x", "y");
    printf("  %-6s  %-12s  %-12s\n", "------", "------------", "------------");

    for (uint32_t i = 0; i < 12; i++) {
        float x, y;
        forge_r2(i, &x, &y);
        printf("  %-6u  %-12.6f  %-12.6f\n", i, x, y);
    }

    printf("\n  Why the plastic constant? In 1D, the golden ratio (phi ~ 1.618)\n");
    printf("  produces the most uniform additive recurrence. The plastic\n");
    printf("  constant is the 2D generalization -- it achieves the lowest\n");
    printf("  possible discrepancy for a 2D additive recurrence.\n");

    printf("\n  R2 vs Halton (16 points, ASCII grid):\n\n");

    #define R2_GRID 30
    char r2_grid[R2_GRID][R2_GRID];
    char hal_grid[R2_GRID][R2_GRID];

    for (int y = 0; y < R2_GRID; y++) {
        for (int x = 0; x < R2_GRID; x++) {
            r2_grid[y][x] = '.';
            hal_grid[y][x] = '.';
        }
    }

    for (uint32_t i = 0; i < 16; i++) {
        float rx, ry;
        forge_r2(i, &rx, &ry);
        int px = (int)(rx * (float)(R2_GRID - 1));
        int py = (int)(ry * (float)(R2_GRID - 1));
        if (px >= 0 && px < R2_GRID && py >= 0 && py < R2_GRID)
            r2_grid[py][px] = '*';

        float hx = forge_halton(i + 1, 2);
        float hy = forge_halton(i + 1, 3);
        px = (int)(hx * (float)(R2_GRID - 1));
        py = (int)(hy * (float)(R2_GRID - 1));
        if (px >= 0 && px < R2_GRID && py >= 0 && py < R2_GRID)
            hal_grid[py][px] = '*';
    }

    printf("  R2 (16 points):                       Halton (16 points):\n");
    for (int y = 0; y < R2_GRID; y++) {
        printf("  ");
        for (int x = 0; x < R2_GRID; x++) printf("%c", r2_grid[y][x]);
        printf("  ");
        for (int x = 0; x < R2_GRID; x++) printf("%c", hal_grid[y][x]);
        printf("\n");
    }

    #undef R2_GRID
}

/* ── 4. Sobol Sequence ───────────────────────────────────────────── */

static void demo_sobol(void)
{
    print_header("4. THE SOBOL SEQUENCE: Direction Numbers");

    printf("\n  Sobol sequences use bit operations (XOR with direction\n");
    printf("  numbers) to construct samples that provably achieve the\n");
    printf("  best theoretical discrepancy bounds.\n\n");

    printf("  Dimension 1 uses Van der Corput base-2 (bit reversal).\n");
    printf("  Dimension 2 uses direction numbers from a primitive\n");
    printf("  polynomial over GF(2) -- the Galois field {0, 1} where\n");
    printf("  addition = XOR and multiplication = AND.\n\n");

    printf("  %-6s  %-12s  %-12s\n", "Index", "x", "y");
    printf("  %-6s  %-12s  %-12s\n", "------", "------------", "------------");

    for (uint32_t i = 0; i < 16; i++) {
        float sx, sy;
        forge_sobol_2d(i, &sx, &sy);
        printf("  %-6u  %-12.6f  %-12.6f\n", i, sx, sy);
    }

    printf("\n  Note how Sobol systematically bisects the unit square.\n");
    printf("  Index 0 = origin, index 1 = (0.5, 0.5), then progressively\n");
    printf("  finer subdivisions.\n\n");

    printf("  Stratification property: the first N = 2^k points always\n");
    printf("  place exactly one point in each of N equal sub-squares.\n");
    printf("  Check the first 4 points: (0,0), (0.5,0.5), (0.25,0.75),\n");
    printf("  (0.75,0.25) -- one per quadrant. This guarantee is why\n");
    printf("  Sobol is the standard for quasi-Monte Carlo integration.\n");
}

/* ── 5. Blue Noise ───────────────────────────────────────────────── */

static void demo_blue_noise(void)
{
    print_header("5. BLUE NOISE: Mitchell's Best Candidate");

    printf("\n  Blue noise is a point distribution where samples maintain\n");
    printf("  a minimum distance from each other. The frequency spectrum\n");
    printf("  has little energy at low frequencies (no clumps) and energy\n");
    printf("  concentrated at high frequencies (fine-scale variation).\n\n");

    printf("  Mitchell's best candidate algorithm (1991):\n");
    printf("    For each new point:\n");
    printf("      1. Generate k random candidates\n");
    printf("      2. For each candidate, find its distance to the nearest\n");
    printf("         existing point\n");
    printf("      3. Pick the candidate with the LARGEST minimum distance\n\n");

    printf("  This maximizes the minimum separation between points,\n");
    printf("  approximating a Poisson disk distribution.\n\n");

    /* Generate and display blue noise */
    #define BN_COUNT      40
    #define BN_GRID       40
    #define BN_CANDIDATES 20  /* candidates per point (higher = better quality) */

    float bn_x[BN_COUNT], bn_y[BN_COUNT];
    forge_blue_noise_2d(bn_x, bn_y, BN_COUNT, BN_CANDIDATES, 42u);

    /* Also generate random for comparison */
    char bn_grid[BN_GRID][BN_GRID];
    char rn_grid[BN_GRID][BN_GRID];

    for (int y = 0; y < BN_GRID; y++) {
        for (int x = 0; x < BN_GRID; x++) {
            bn_grid[y][x] = '.';
            rn_grid[y][x] = '.';
        }
    }

    for (int i = 0; i < BN_COUNT; i++) {
        int px = (int)(bn_x[i] * (float)(BN_GRID - 1));
        int py = (int)(bn_y[i] * (float)(BN_GRID - 1));
        if (px >= 0 && px < BN_GRID && py >= 0 && py < BN_GRID)
            bn_grid[py][px] = '*';

        /* Random points for comparison */
        uint32_t h = forge_hash_wang((uint32_t)i ^ 99u);
        px = (int)(forge_hash_to_float(h) * (float)(BN_GRID - 1));
        h = forge_hash_wang(h);
        py = (int)(forge_hash_to_float(h) * (float)(BN_GRID - 1));
        if (px >= 0 && px < BN_GRID && py >= 0 && py < BN_GRID)
            rn_grid[py][px] = '*';
    }

    printf("  Blue noise (%d points):              Random (%d points):\n",
           BN_COUNT, BN_COUNT);

    for (int y = 0; y < BN_GRID; y++) {
        printf("  ");
        for (int x = 0; x < BN_GRID; x++) printf("%c", bn_grid[y][x]);
        printf("  ");
        for (int x = 0; x < BN_GRID; x++) printf("%c", rn_grid[y][x]);
        printf("\n");
    }

    printf("\n  Blue noise points maintain even spacing -- no two points\n");
    printf("  are too close together, and no large gaps remain. This\n");
    printf("  property makes blue noise ideal for dithering and sampling.\n");

    #undef BN_COUNT
    #undef BN_GRID
    #undef BN_CANDIDATES
}

/* ── 6. Discrepancy Comparison ───────────────────────────────────── */

static void demo_discrepancy(void)
{
    print_header("6. DISCREPANCY: Measuring Sample Quality");

    printf("\n  Star discrepancy D* measures how uniformly points fill\n");
    printf("  the unit square. Lower is better.\n\n");

    printf("  D* = max over all boxes [0,u) x [0,v) of:\n");
    printf("       |fraction of points in box  -  area of box|\n\n");

    printf("  Theory predicts:\n");
    printf("    Random:            D* ~ sqrt(log N / N)\n");
    printf("    Low-discrepancy:   D* ~ (log N)^2 / N\n\n");

    int counts[] = {16, 32, 64, 128};
    int num_counts = 4;

    printf("  %-8s  %-12s  %-12s  %-12s  %-12s\n",
           "N", "Random", "Halton", "R2", "Sobol");
    printf("  %-8s  %-12s  %-12s  %-12s  %-12s\n",
           "--------", "------------", "------------",
           "------------", "------------");

    for (int ci = 0; ci < num_counts; ci++) {
        int n = counts[ci];

        /* Random points */
        float rx[128], ry[128];
        for (int i = 0; i < n; i++) {
            uint32_t h = forge_hash_wang((uint32_t)i ^ 777u);
            rx[i] = forge_hash_to_float(h);
            h = forge_hash_wang(h);
            ry[i] = forge_hash_to_float(h);
        }

        /* Halton points */
        float hx[128], hy[128];
        for (int i = 0; i < n; i++) {
            hx[i] = forge_halton((uint32_t)(i + 1), 2);
            hy[i] = forge_halton((uint32_t)(i + 1), 3);
        }

        /* R2 points */
        float r2x[128], r2y[128];
        for (int i = 0; i < n; i++) {
            forge_r2((uint32_t)i, &r2x[i], &r2y[i]);
        }

        /* Sobol points */
        float sx[128], sy[128];
        for (int i = 0; i < n; i++) {
            forge_sobol_2d((uint32_t)i, &sx[i], &sy[i]);
        }

        float d_rand  = forge_star_discrepancy_2d(rx, ry, n);
        float d_halt  = forge_star_discrepancy_2d(hx, hy, n);
        float d_r2    = forge_star_discrepancy_2d(r2x, r2y, n);
        float d_sobol = forge_star_discrepancy_2d(sx, sy, n);

        printf("  %-8d  %-12.6f  %-12.6f  %-12.6f  %-12.6f\n",
               n, d_rand, d_halt, d_r2, d_sobol);
    }

    printf("\n  All three low-discrepancy sequences consistently beat\n");
    printf("  random sampling. The gap widens as N increases -- this\n");
    printf("  means convergence improves faster with more samples.\n");
}

/* ── 7. Application: Dithering ───────────────────────────────────── */

static void demo_dithering(void)
{
    print_header("7. APPLICATION: Dithering (Replacing Banding with Noise)");

    printf("\n  When quantizing a smooth gradient to few levels, the result\n");
    printf("  shows visible bands. Adding noise before quantizing replaces\n");
    printf("  banding with a fine grain that the eye perceives as smooth.\n\n");

    printf("  Blue noise dithering is superior to white noise dithering\n");
    printf("  because it distributes the error evenly -- no clumps of\n");
    printf("  similar errors that the eye detects as patterns.\n\n");

    #define DITHER_WIDTH 60
    #define DITHER_LEVELS 4

    printf("  Smooth gradient (%.0f values per character):\n  ",
           256.0f / (float)DITHER_WIDTH);
    for (int x = 0; x < DITHER_WIDTH; x++) {
        float t = (float)x / (float)(DITHER_WIDTH - 1);
        printf("%c", density_char(t));
    }
    printf("\n\n");

    /* Quantize without dithering */
    printf("  Quantized to %d levels (banding visible):\n  ", DITHER_LEVELS);
    for (int x = 0; x < DITHER_WIDTH; x++) {
        float t = (float)x / (float)(DITHER_WIDTH - 1);
        float q = floorf(t * (float)DITHER_LEVELS) / (float)DITHER_LEVELS;
        printf("%c", density_char(q));
    }
    printf("\n\n");

    /* Dither with white noise */
    printf("  White noise dithered (noisy, some clumps):\n  ");
    for (int x = 0; x < DITHER_WIDTH; x++) {
        float t = (float)x / (float)(DITHER_WIDTH - 1);
        float noise = forge_hash_to_float(forge_hash_wang((uint32_t)x ^ 42u));
        float dithered = t + (noise - 0.5f) / (float)DITHER_LEVELS;
        float q = floorf(dithered * (float)DITHER_LEVELS) / (float)DITHER_LEVELS;
        if (q < 0.0f) q = 0.0f;
        if (q > 1.0f) q = 1.0f;
        printf("%c", density_char(q));
    }
    printf("\n\n");

    /* Dither with R1 (golden ratio) low-discrepancy sequence */
    printf("  R1 (golden ratio) dithered (even error distribution):\n  ");
    for (int x = 0; x < DITHER_WIDTH; x++) {
        float t = (float)x / (float)(DITHER_WIDTH - 1);
        float r1_val = forge_r1((uint32_t)x);
        float dithered = t + (r1_val - 0.5f) / (float)DITHER_LEVELS;
        float q = floorf(dithered * (float)DITHER_LEVELS) / (float)DITHER_LEVELS;
        if (q < 0.0f) q = 0.0f;
        if (q > 1.0f) q = 1.0f;
        printf("%c", density_char(q));
    }
    printf("\n\n");

    printf("  The R1-dithered version transitions more smoothly.\n");
    printf("  In a real renderer, this replaces 8-bit color banding\n");
    printf("  with imperceptible noise (especially with blue noise\n");
    printf("  textures that vary in 2D, not just 1D).\n");

    #undef DITHER_WIDTH
    #undef DITHER_LEVELS
}

/* ── 8. Application: Sampling ────────────────────────────────────── */

static void demo_sampling(void)
{
    print_header("8. APPLICATION: Sampling (Anti-Aliasing & AO Kernels)");

    printf("\n  Monte Carlo rendering estimates integrals by averaging\n");
    printf("  samples. Low-discrepancy sequences converge faster because\n");
    printf("  they sample space more uniformly.\n\n");

    printf("  Example: estimate the area of a quarter circle (true = pi/4).\n");
    printf("  Drop N points in [0,1)^2, count how many satisfy x^2+y^2 < 1.\n\n");

    int sample_counts[] = {16, 64, 256, 1024};
    float true_value = (float)FORGE_PI / 4.0f;  /* pi/4 = area of quarter circle */

    printf("  %-8s  %-14s  %-14s  %-14s\n",
           "N", "Random error", "Halton error", "R2 error");
    printf("  %-8s  %-14s  %-14s  %-14s\n",
           "--------", "--------------", "--------------", "--------------");

    for (int ci = 0; ci < 4; ci++) {
        int n = sample_counts[ci];
        int count_rand = 0, count_halt = 0, count_r2 = 0;

        for (int i = 0; i < n; i++) {
            /* Random */
            uint32_t h = forge_hash_wang((uint32_t)i ^ 0xDEAD);
            float rx = forge_hash_to_float(h);
            h = forge_hash_wang(h);
            float ry = forge_hash_to_float(h);
            if (rx * rx + ry * ry < 1.0f) count_rand++;

            /* Halton */
            float hx = forge_halton((uint32_t)(i + 1), 2);
            float hy = forge_halton((uint32_t)(i + 1), 3);
            if (hx * hx + hy * hy < 1.0f) count_halt++;

            /* R2 */
            float r2x, r2y;
            forge_r2((uint32_t)i, &r2x, &r2y);
            if (r2x * r2x + r2y * r2y < 1.0f) count_r2++;
        }

        float est_rand = (float)count_rand / (float)n;
        float est_halt = (float)count_halt / (float)n;
        float est_r2   = (float)count_r2   / (float)n;

        printf("  %-8d  %-14.6f  %-14.6f  %-14.6f\n",
               n,
               fabsf(est_rand - true_value),
               fabsf(est_halt - true_value),
               fabsf(est_r2   - true_value));
    }

    printf("\n  Low-discrepancy sequences consistently produce smaller\n");
    printf("  errors at any given sample count. In rendering, this means:\n");
    printf("  - Anti-aliasing: fewer samples for smooth edges\n");
    printf("  - AO kernels: more uniform hemisphere coverage\n");
    printf("  - Soft shadows: less noise with fewer shadow rays\n");
}

/* ── 9. Application: Stippling ───────────────────────────────────── */

static void demo_stippling(void)
{
    print_header("9. APPLICATION: Stippling (Density-Driven Point Placement)");

    printf("\n  Stippling represents a grayscale image using dots.\n");
    printf("  Darker areas get more dots; lighter areas get fewer.\n");
    printf("  Blue noise point placement ensures even dot spacing.\n\n");

    printf("  Method: generate blue noise candidates, keep a point\n");
    printf("  with probability proportional to the darkness at that\n");
    printf("  location. The blue noise base ensures no clumping.\n\n");

    /* Create a simple radial gradient as our "image" */
    #define STIP_W 60
    #define STIP_H 25
    #define STIP_CANDIDATES 4000
    #define STIP_ASPECT_RATIO 2.0f   /* Compensate for non-square characters */
    #define STIP_DENSITY      0.8f   /* Max acceptance probability (0-1) */

    char stipple[STIP_H][STIP_W];
    for (int y = 0; y < STIP_H; y++) {
        for (int x = 0; x < STIP_W; x++) {
            stipple[y][x] = ' ';
        }
    }

    /* Radial gradient: dark in center, light at edges */
    float cx = (float)STIP_W * 0.5f;
    float cy = (float)STIP_H * 0.5f;
    float max_r = sqrtf(cx * cx + cy * cy);

    for (int i = 0; i < STIP_CANDIDATES; i++) {
        /* Use R2 for candidate positions */
        float px, py;
        forge_r2((uint32_t)i, &px, &py);

        int ix = (int)(px * (float)STIP_W);
        int iy = (int)(py * (float)STIP_H);
        if (ix >= STIP_W) ix = STIP_W - 1;
        if (iy >= STIP_H) iy = STIP_H - 1;

        /* Compute darkness at this position (radial gradient) */
        float dx = (float)ix - cx;
        float dy = ((float)iy - cy) * STIP_ASPECT_RATIO;
        float r = sqrtf(dx * dx + dy * dy);
        float darkness = 1.0f - (r / max_r);
        if (darkness < 0.0f) darkness = 0.0f;

        /* Accept point with probability proportional to darkness */
        float threshold = forge_hash_to_float(
            forge_hash_wang((uint32_t)i ^ 0xBEEF));
        if (threshold < darkness * STIP_DENSITY) {
            stipple[iy][ix] = '.';
        }
    }

    printf("  Stippled radial gradient (dark center, light edges):\n\n");
    for (int y = 0; y < STIP_H; y++) {
        printf("  ");
        for (int x = 0; x < STIP_W; x++) {
            printf("%c", stipple[y][x]);
        }
        printf("\n");
    }

    printf("\n  The dots are denser in the center (darker) and sparse\n");
    printf("  at the edges (lighter), with even spacing throughout.\n");
    printf("  This technique is used in scientific visualization,\n");
    printf("  artistic rendering (non-photorealistic), and print media.\n");

    #undef STIP_W
    #undef STIP_H
    #undef STIP_CANDIDATES
    #undef STIP_ASPECT_RATIO
    #undef STIP_DENSITY
}

/* ── 10. Sequence Comparison Summary ─────────────────────────────── */

static void demo_comparison(void)
{
    print_header("10. SEQUENCE COMPARISON SUMMARY");

    printf("\n  %-18s  %-8s  %-14s  %-20s\n",
           "Sequence", "Speed", "Quality (D*)", "Best for");
    printf("  %-18s  %-8s  %-14s  %-20s\n",
           "------------------", "--------", "--------------",
           "--------------------");
    printf("  %-18s  %-8s  %-14s  %-20s\n",
           "White noise", "Fast", "Poor", "Stochastic effects");
    printf("  %-18s  %-8s  %-14s  %-20s\n",
           "Halton", "Fast", "Good", "General sampling");
    printf("  %-18s  %-8s  %-14s  %-20s\n",
           "R2", "Fastest", "Very good", "2D sampling, dither");
    printf("  %-18s  %-8s  %-14s  %-20s\n",
           "Sobol", "Fast", "Excellent", "Monte Carlo integ.");
    printf("  %-18s  %-8s  %-14s  %-20s\n",
           "Blue noise", "Slow*", "N/A (spatial)", "Dithering, stippling");

    printf("\n  * Blue noise generation is O(n*m*k) -- slow to generate,\n");
    printf("    but the result is pre-computed and reused. In practice,\n");
    printf("    blue noise textures are loaded from disk, not generated\n");
    printf("    per-frame.\n\n");

    printf("  Key insight: low-discrepancy sequences and blue noise solve\n");
    printf("  different problems.\n");
    printf("  - LDS (Halton, R2, Sobol) minimize discrepancy: they fill\n");
    printf("    space uniformly for integration/estimation.\n");
    printf("  - Blue noise minimizes visual artifacts: it distributes error\n");
    printf("    at frequencies the human eye is least sensitive to.\n\n");

    printf("  In rendering, both are used together:\n");
    printf("  - LDS for sample positions (AO, anti-aliasing, soft shadows)\n");
    printf("  - Blue noise for dithering (banding removal, temporal noise)\n");
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("=============================================================\n");
    printf("  Math Lesson 14 -- Blue Noise & Low-Discrepancy Sequences\n");
    printf("=============================================================\n");

    demo_clumping();
    demo_halton();
    demo_r2();
    demo_sobol();
    demo_blue_noise();
    demo_discrepancy();
    demo_dithering();
    demo_sampling();
    demo_stippling();
    demo_comparison();

    printf("\n=============================================================\n");
    printf("  See README.md for diagrams and detailed explanations.\n");
    printf("  See common/math/forge_math.h for the implementations.\n");
    printf("=============================================================\n\n");

    SDL_Quit();
    return 0;
}
