/*
 * Math Lesson 03 — Bilinear Interpolation
 *
 * Demonstrates bilinear interpolation: the math behind LINEAR texture filtering.
 * Shows how two nested lerps blend the 4 nearest texels into a smooth result,
 * and compares it with NEAREST filtering.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include "math/forge_math.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

static void print_vec3(const char *label, vec3 v)
{
    printf("  %-36s (%.3f, %.3f, %.3f)\n", label, v.x, v.y, v.z);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("\n");
    printf("==============================================================\n");
    printf("  Bilinear Interpolation\n");
    printf("==============================================================\n");
    printf("\n");
    printf("Bilinear interpolation blends four values on a 2D grid based\n");
    printf("on a fractional position. It's what the GPU does when a texture\n");
    printf("sampler uses LINEAR filtering.\n");

    /* ── 1. Lerp refresher ───────────────────────────────────────────── */

    print_header("1. LINEAR INTERPOLATION (LERP) REFRESHER");
    printf("  lerp(a, b, t) = a + t * (b - a)\n");
    printf("  Blends between two values based on t in [0, 1].\n\n");

    float a = 10.0f, b = 30.0f;

    printf("  a = %.1f,  b = %.1f\n\n", a, b);
    printf("  t = 0.00  ->  lerp = %.1f   (100%% a)\n",
           forge_lerpf(a, b, 0.0f));
    printf("  t = 0.25  ->  lerp = %.1f   ( 75%% a + 25%% b)\n",
           forge_lerpf(a, b, 0.25f));
    printf("  t = 0.50  ->  lerp = %.1f   ( 50%% a + 50%% b)\n",
           forge_lerpf(a, b, 0.5f));
    printf("  t = 0.75  ->  lerp = %.1f   ( 25%% a + 75%% b)\n",
           forge_lerpf(a, b, 0.75f));
    printf("  t = 1.00  ->  lerp = %.1f   (100%% b)\n",
           forge_lerpf(a, b, 1.0f));

    printf("\n  Lerp is the 1D building block. Bilinear interpolation\n");
    printf("  extends it to 2D by doing three lerps.\n");

    /* ── 2. Bilinear interpolation step-by-step ──────────────────────── */

    print_header("2. BILINEAR INTERPOLATION -- STEP BY STEP");

    /*  Four corners of a grid cell:
     *
     *    c01=5 -------- c11=9
     *     |               |
     *     |    * (0.3,0.7) |
     *     |               |
     *    c00=1 -------- c10=3
     */
    float c00 = 1.0f, c10 = 3.0f, c01 = 5.0f, c11 = 9.0f;
    float tx = 0.3f, ty = 0.7f;

    printf("  Four corner values:\n\n");
    printf("    c01=%.0f -------- c11=%.0f\n", c01, c11);
    printf("     |                |        tx = %.1f\n", tx);
    printf("     |    * (tx,ty)   |        ty = %.1f\n", ty);
    printf("     |                |\n");
    printf("    c00=%.0f -------- c10=%.0f\n\n", c00, c10);

    /* Step 1: Lerp horizontally along bottom edge */
    float bot = forge_lerpf(c00, c10, tx);
    printf("  Step 1 -- Lerp along bottom edge (y=0):\n");
    printf("    lerp(%.0f, %.0f, %.1f) = %.2f\n\n", c00, c10, tx, bot);

    /* Step 2: Lerp horizontally along top edge */
    float top = forge_lerpf(c01, c11, tx);
    printf("  Step 2 -- Lerp along top edge (y=1):\n");
    printf("    lerp(%.0f, %.0f, %.1f) = %.2f\n\n", c01, c11, tx, top);

    /* Step 3: Lerp vertically between the two results */
    float result = forge_lerpf(bot, top, ty);
    printf("  Step 3 -- Lerp vertically between results:\n");
    printf("    lerp(%.2f, %.2f, %.1f) = %.3f\n\n", bot, top, ty, result);

    /* Verify with the library function */
    float verify = forge_bilerpf(c00, c10, c01, c11, tx, ty);
    printf("  forge_bilerpf(1, 3, 5, 9, 0.3, 0.7) = %.3f\n", verify);
    printf("  Matches step-by-step result:           %.3f\n", result);

    /* ── 3. Special cases ────────────────────────────────────────────── */

    print_header("3. SPECIAL CASES");
    printf("  When tx and ty are 0 or 1, bilerp returns a corner value.\n");
    printf("  When both are 0.5, it returns the average of all four.\n\n");

    printf("  bilerp at (0.0, 0.0) = %.1f  (bottom-left corner)\n",
           forge_bilerpf(c00, c10, c01, c11, 0.0f, 0.0f));
    printf("  bilerp at (1.0, 0.0) = %.1f  (bottom-right corner)\n",
           forge_bilerpf(c00, c10, c01, c11, 1.0f, 0.0f));
    printf("  bilerp at (0.0, 1.0) = %.1f  (top-left corner)\n",
           forge_bilerpf(c00, c10, c01, c11, 0.0f, 1.0f));
    printf("  bilerp at (1.0, 1.0) = %.1f  (top-right corner)\n",
           forge_bilerpf(c00, c10, c01, c11, 1.0f, 1.0f));
    printf("  bilerp at (0.5, 0.5) = %.1f  (center = average)\n",
           forge_bilerpf(c00, c10, c01, c11, 0.5f, 0.5f));

    float avg = (c00 + c10 + c01 + c11) / 4.0f;
    printf("  (1 + 3 + 5 + 9) / 4 = %.1f  [OK]\n", avg);

    /* ── 4. Texture sampling analogy ─────────────────────────────────── */

    print_header("4. TEXTURE SAMPLING -- HOW THE GPU USES BILERP");
    printf("  A 4x4 texture stores brightness values at integer coords.\n");
    printf("  When you sample at a fractional UV, the GPU finds the 4\n");
    printf("  nearest texels and bilinearly interpolates between them.\n\n");

    /* A small 4x4 texture (brightness values 0-255) */
    float tex[4][4] = {
        {  50, 100, 150, 200 },  /* row 0 (bottom) */
        {  75, 125, 175, 225 },  /* row 1 */
        { 100, 150, 200, 250 },  /* row 2 */
        { 125, 175, 225, 255 },  /* row 3 (top) */
    };

    printf("  4x4 texture (brightness values):\n\n");
    printf("  row 3: [ 125  175  225  255 ]   (top)\n");
    printf("  row 2: [ 100  150  200  250 ]\n");
    printf("  row 1: [  75  125  175  225 ]\n");
    printf("  row 0: [  50  100  150  200 ]   (bottom)\n\n");

    /* Sample at UV = (0.375, 0.625) on a 4x4 texture
     * Texel coordinates: u * (width-1) = 0.375 * 3 = 1.125
     *                    v * (height-1) = 0.625 * 3 = 1.875
     * Integer part: (1, 1) = bottom-left texel
     * Fractional part: (0.125, 0.875) = blend factors
     */
    float u = 0.375f, v = 0.625f;
    int tex_w = 4, tex_h = 4;

    float texel_x = u * (float)(tex_w - 1);
    float texel_y = v * (float)(tex_h - 1);
    int ix = (int)texel_x;
    int iy = (int)texel_y;
    float fx = texel_x - (float)ix;
    float fy = texel_y - (float)iy;

    printf("  Sample at UV = (%.3f, %.3f)\n\n", u, v);
    printf("  Step 1 -- Convert UV to texel coordinates:\n");
    printf("    texel_x = %.3f * %d = %.3f\n", u, tex_w - 1, texel_x);
    printf("    texel_y = %.3f * %d = %.3f\n\n", v, tex_h - 1, texel_y);

    printf("  Step 2 -- Split into integer + fraction:\n");
    printf("    integer:    (%d, %d)     -> bottom-left texel\n", ix, iy);
    printf("    fraction:   (%.3f, %.3f) -> blend factors (tx, ty)\n\n",
           fx, fy);

    printf("  Step 3 -- Gather the 4 nearest texels:\n");
    float t00 = tex[iy][ix];
    float t10 = tex[iy][ix + 1];
    float t01 = tex[iy + 1][ix];
    float t11 = tex[iy + 1][ix + 1];
    printf("    c00 = tex[%d][%d] = %.0f   (bottom-left)\n",  iy, ix, t00);
    printf("    c10 = tex[%d][%d] = %.0f   (bottom-right)\n", iy, ix+1, t10);
    printf("    c01 = tex[%d][%d] = %.0f   (top-left)\n",     iy+1, ix, t01);
    printf("    c11 = tex[%d][%d] = %.0f   (top-right)\n\n",  iy+1, ix+1, t11);

    float sampled = forge_bilerpf(t00, t10, t01, t11, fx, fy);
    printf("  Step 4 -- Bilinear interpolation:\n");
    printf("    bilerp(%.0f, %.0f, %.0f, %.0f, %.3f, %.3f) = %.1f\n\n",
           t00, t10, t01, t11, fx, fy, sampled);

    /* Nearest-neighbor for comparison */
    int nearest_x = (int)(texel_x + 0.5f);
    int nearest_y = (int)(texel_y + 0.5f);
    float nearest = tex[nearest_y][nearest_x];
    printf("  For comparison, NEAREST filtering picks the closest texel:\n");
    printf("    round(%.3f, %.3f) = (%d, %d) -> %.0f\n",
           texel_x, texel_y, nearest_x, nearest_y, nearest);
    printf("\n  LINEAR (%.1f) gives a smooth blend between texels.\n", sampled);
    printf("  NEAREST (%.0f) snaps to whichever texel center is closest.\n",
           nearest);

    /* ── 5. Color blending with vec3_bilerp ──────────────────────────── */

    print_header("5. COLOR BLENDING WITH vec3_bilerp");
    printf("  In practice, texels are colors (RGB). Bilinear interpolation\n");
    printf("  blends each channel independently, giving smooth gradients.\n\n");

    vec3 red   = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 green = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 blue  = vec3_create(0.0f, 0.0f, 1.0f);
    vec3 white = vec3_create(1.0f, 1.0f, 1.0f);

    printf("  Corner colors:\n");
    printf("    blue (0,0,1) -------- white (1,1,1)\n");
    printf("     |                     |\n");
    printf("    red  (1,0,0) -------- green (0,1,0)\n\n");

    /* Sample at several positions */
    print_vec3("bilerp at (0.0, 0.0) =",
               vec3_bilerp(red, green, blue, white, 0.0f, 0.0f));
    print_vec3("bilerp at (1.0, 0.0) =",
               vec3_bilerp(red, green, blue, white, 1.0f, 0.0f));
    print_vec3("bilerp at (0.0, 1.0) =",
               vec3_bilerp(red, green, blue, white, 0.0f, 1.0f));
    print_vec3("bilerp at (1.0, 1.0) =",
               vec3_bilerp(red, green, blue, white, 1.0f, 1.0f));
    print_vec3("bilerp at (0.5, 0.5) =",
               vec3_bilerp(red, green, blue, white, 0.5f, 0.5f));
    print_vec3("bilerp at (0.5, 0.0) =",
               vec3_bilerp(red, green, blue, white, 0.5f, 0.0f));
    print_vec3("bilerp at (0.0, 0.5) =",
               vec3_bilerp(red, green, blue, white, 0.0f, 0.5f));

    printf("\n  At center (0.5, 0.5): all four colors contribute equally,\n");
    printf("  giving (0.5, 0.5, 0.5) -- a neutral gray.\n");

    /* ── 6. Why bilinear matters ─────────────────────────────────────── */

    print_header("6. NEAREST VS LINEAR -- WHY IT MATTERS");
    printf("  Imagine a 2x2 checkerboard texture (black and white):\n\n");

    float checker[2][2] = {
        {   0, 255 },  /* bottom: black, white */
        { 255,   0 },  /* top:    white, black */
    };

    printf("    255 ------- 0\n");
    printf("     |           |\n");
    printf("     0  ------- 255\n\n");

    /* Sample along a diagonal */
    printf("  Sampling along the diagonal (tx = ty):\n\n");
    printf("  %-8s  %-12s  %-12s\n", "t", "LINEAR", "NEAREST");
    printf("  %-8s  %-12s  %-12s\n", "------", "----------", "----------");

    float steps[] = { 0.0f, 0.2f, 0.4f, 0.5f, 0.6f, 0.8f, 1.0f };
    int num_steps = (int)(sizeof(steps) / sizeof(steps[0]));

    for (int i = 0; i < num_steps; i++) {
        float t = steps[i];
        float linear_val = forge_bilerpf(
            checker[0][0], checker[0][1],
            checker[1][0], checker[1][1],
            t, t
        );

        /* Nearest: pick whichever corner is closest */
        int nx = (t < 0.5f) ? 0 : 1;
        int ny = (t < 0.5f) ? 0 : 1;
        float nearest_val = checker[ny][nx];

        printf("  %-8.1f  %-12.1f  %-12.0f\n", t, linear_val, nearest_val);
    }

    printf("\n  LINEAR produces a smooth gradient across the surface.\n");
    printf("  NEAREST produces hard jumps -- fine for pixel art,\n");
    printf("  but jarring for photographic textures.\n");

    /* ── Summary ─────────────────────────────────────────────────────── */

    printf("\n");
    printf("==============================================================\n");
    printf("  Summary\n");
    printf("==============================================================\n");
    printf("\n");
    printf("  Bilinear interpolation:\n");
    printf("    * Three lerps: two horizontal, one vertical\n");
    printf("    * Blends the 4 nearest values based on fractional position\n");
    printf("    * At corners: returns exact corner value\n");
    printf("    * At center (0.5, 0.5): returns the average of all four\n");
    printf("\n");
    printf("  In texture sampling:\n");
    printf("    * LINEAR filter = bilinear interpolation of 4 nearest texels\n");
    printf("    * NEAREST filter = pick the single closest texel\n");
    printf("    * UV coordinates map to texel coordinates\n");
    printf("    * The fractional part determines blend weights\n");
    printf("\n");
    printf("  Functions added to forge_math.h:\n");
    printf("    * forge_lerpf(a, b, t)       -- scalar lerp\n");
    printf("    * forge_bilerpf(...)          -- scalar bilinear interpolation\n");
    printf("    * vec3_bilerp(...)            -- vec3 bilinear (RGB colors)\n");
    printf("    * vec4_bilerp(...)            -- vec4 bilinear (RGBA colors)\n");
    printf("\n");
    printf("  See: lessons/math/03-bilinear-interpolation/README.md\n");
    printf("  See: lessons/gpu/04-textures-and-samplers/ (LINEAR vs NEAREST)\n");
    printf("\n");

    SDL_Quit();
    return 0;
}
