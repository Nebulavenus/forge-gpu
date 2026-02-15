/*
 * Math Lesson 05 — Mipmaps & LOD
 *
 * Demonstrates the math behind mipmapping: how textures are pre-filtered at
 * multiple resolutions to prevent aliasing, how the GPU picks the right mip
 * level, and how trilinear interpolation blends between levels for smooth
 * transitions.
 *
 * Sections:
 *   1. Why mipmaps — the aliasing problem
 *   2. Mip chain computation — halving, log2, memory cost
 *   3. Trilinear interpolation — two bilinear + lerp
 *   4. LOD selection — screen-space footprint and log2
 *   5. Practical example — textured floor at different distances
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include "math/forge_math.h"

/* ── Constants ────────────────────────────────────────────────────────── */

/* Base texture dimensions for the mip chain example */
#define BASE_TEX_SIZE  256

/* Bytes per texel (RGBA8) */
#define BYTES_PER_TEXEL 4

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

static void print_vec3(const char *label, vec3 v)
{
    printf("  %-40s (%.3f, %.3f, %.3f)\n", label, v.x, v.y, v.z);
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
    printf("  Mipmaps & LOD (Level of Detail)\n");
    printf("==============================================================\n");
    printf("\n");
    printf("Mipmaps are pre-computed, progressively smaller versions of a\n");
    printf("texture. They solve the aliasing problem that occurs when a\n");
    printf("texture is viewed at a distance, and they improve performance\n");
    printf("by letting the GPU read from smaller textures when possible.\n");

    /* ── 1. Why mipmaps ──────────────────────────────────────────────── */

    print_header("1. WHY MIPMAPS -- THE ALIASING PROBLEM");

    printf("  Imagine a 256x256 checkerboard texture applied to a floor\n");
    printf("  that stretches into the distance. Near the camera, each\n");
    printf("  texel maps to roughly one screen pixel -- looks great.\n\n");

    printf("  But far away, hundreds of texels map to a single pixel.\n");
    printf("  The GPU can only sample one (or four) texels per pixel.\n");
    printf("  It misses most of the texture detail, causing:\n\n");

    printf("    * Shimmering / flickering when the camera moves\n");
    printf("    * Moire patterns (false repeating patterns)\n");
    printf("    * Visual noise where there should be smooth color\n\n");

    printf("  The fix: pre-filter the texture at multiple resolutions.\n");
    printf("  When the surface is far away, sample from a smaller version\n");
    printf("  that has already averaged the fine detail.\n");

    /* ── 2. Mip chain computation ────────────────────────────────────── */

    print_header("2. MIP CHAIN -- HALVING AND LOG2");

    int num_levels = (int)forge_log2f((float)BASE_TEX_SIZE) + 1;

    printf("  A %dx%d texture has %d mip levels.\n\n", BASE_TEX_SIZE,
           BASE_TEX_SIZE, num_levels);
    printf("  Each level halves the dimensions of the previous level:\n\n");
    printf("  %-8s  %-12s  %-12s  %-8s\n", "Level", "Size", "Texels", "Bytes");
    printf("  %-8s  %-12s  %-12s  %-8s\n", "-----", "--------", "--------", "------");

    int total_texels = 0;
    int size = BASE_TEX_SIZE;
    for (int level = 0; level < num_levels; level++) {
        int texels = size * size;
        total_texels += texels;
        printf("  %-8d  %4dx%-7d  %-12d  %d\n",
               level, size, size, texels, texels * BYTES_PER_TEXEL);
        if (size > 1) size /= 2;
    }

    int base_texels = BASE_TEX_SIZE * BASE_TEX_SIZE;
    printf("\n  Base level texels:  %d\n", base_texels);
    printf("  Total with mipmaps: %d\n", total_texels);
    printf("  Overhead: %.0f%% extra memory (always ~33%%)\n",
           ((float)total_texels / (float)base_texels - 1.0f) * 100.0f);

    printf("\n  The formula: num_levels = floor(log2(max_dimension)) + 1\n");
    printf("    log2(%d) = %.0f, so %d + 1 = %d levels\n",
           BASE_TEX_SIZE, forge_log2f((float)BASE_TEX_SIZE),
           (int)forge_log2f((float)BASE_TEX_SIZE), num_levels);

    printf("\n  Other examples:\n");
    int sizes[] = { 512, 1024, 2048, 4096 };
    int num_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
    for (int i = 0; i < num_sizes; i++) {
        int s = sizes[i];
        int levels = (int)forge_log2f((float)s) + 1;
        printf("    %4dx%-4d -> log2(%d) + 1 = %d levels\n",
               s, s, s, levels);
    }

    /* ── 3. Trilinear interpolation ──────────────────────────────────── */

    print_header("3. TRILINEAR INTERPOLATION -- TWO BILINEAR + LERP");

    printf("  When the computed LOD falls between two mip levels, the GPU\n");
    printf("  does trilinear filtering:\n\n");
    printf("    1. Bilinear sample from mip level N   (4 texels)\n");
    printf("    2. Bilinear sample from mip level N+1 (4 texels)\n");
    printf("    3. Lerp between the two results based on fractional LOD\n\n");

    printf("  This uses 8 texels total, blended by three parameters:\n");
    printf("    tx, ty = fractional UV within each mip level\n");
    printf("    tz     = fractional part of the LOD (blend between levels)\n\n");

    /* Step-by-step numerical example */
    printf("  Example: sampling between mip levels 2 and 3\n\n");

    /* Mip level 2: a 2x2 region of brightness values */
    float mip2_c00 = 100.0f, mip2_c10 = 150.0f;
    float mip2_c01 = 120.0f, mip2_c11 = 170.0f;

    /* Mip level 3: same region, but averaged/smaller */
    float mip3_c00 = 110.0f, mip3_c10 = 140.0f;
    float mip3_c01 = 125.0f, mip3_c11 = 155.0f;

    float tx = 0.4f, ty = 0.6f, tz = 0.3f;

    printf("  Mip level 2 corners:  c00=%.0f  c10=%.0f  c01=%.0f  c11=%.0f\n",
           mip2_c00, mip2_c10, mip2_c01, mip2_c11);
    printf("  Mip level 3 corners:  c00=%.0f  c10=%.0f  c01=%.0f  c11=%.0f\n",
           mip3_c00, mip3_c10, mip3_c01, mip3_c11);
    printf("  Fractional UV: tx=%.1f, ty=%.1f\n", tx, ty);
    printf("  Fractional LOD: tz=%.1f (30%% toward level 3)\n\n", tz);

    /* Step 1: bilinear on mip 2 */
    float bilerp2 = forge_bilerpf(mip2_c00, mip2_c10, mip2_c01, mip2_c11,
                                   tx, ty);
    printf("  Step 1 -- Bilinear on mip 2:\n");
    printf("    bilerp(100, 150, 120, 170, 0.4, 0.6) = %.1f\n\n", bilerp2);

    /* Step 2: bilinear on mip 3 */
    float bilerp3 = forge_bilerpf(mip3_c00, mip3_c10, mip3_c01, mip3_c11,
                                   tx, ty);
    printf("  Step 2 -- Bilinear on mip 3:\n");
    printf("    bilerp(110, 140, 125, 155, 0.4, 0.6) = %.1f\n\n", bilerp3);

    /* Step 3: lerp between levels */
    float trilinear = forge_lerpf(bilerp2, bilerp3, tz);
    printf("  Step 3 -- Lerp between levels:\n");
    printf("    lerp(%.1f, %.1f, 0.3) = %.1f\n\n", bilerp2, bilerp3, trilinear);

    /* Verify with library function */
    float verify = forge_trilerpf(
        mip2_c00, mip2_c10, mip2_c01, mip2_c11,
        mip3_c00, mip3_c10, mip3_c01, mip3_c11,
        tx, ty, tz);
    printf("  forge_trilerpf(...) = %.1f  [matches]\n", verify);

    /* ── vec3 trilinear (RGB colors) ─────────────────────────────────── */

    printf("\n  With RGB colors (vec3_trilerp):\n\n");

    /* Two mip levels with colored corners */
    vec3 m2_00 = vec3_create(1.0f, 0.0f, 0.0f);  /* red */
    vec3 m2_10 = vec3_create(0.0f, 1.0f, 0.0f);  /* green */
    vec3 m2_01 = vec3_create(0.0f, 0.0f, 1.0f);  /* blue */
    vec3 m2_11 = vec3_create(1.0f, 1.0f, 0.0f);  /* yellow */

    vec3 m3_00 = vec3_create(0.8f, 0.2f, 0.2f);  /* muted red */
    vec3 m3_10 = vec3_create(0.2f, 0.8f, 0.2f);  /* muted green */
    vec3 m3_01 = vec3_create(0.2f, 0.2f, 0.8f);  /* muted blue */
    vec3 m3_11 = vec3_create(0.8f, 0.8f, 0.2f);  /* muted yellow */

    vec3 color = vec3_trilerp(m2_00, m2_10, m2_01, m2_11,
                               m3_00, m3_10, m3_01, m3_11,
                               0.5f, 0.5f, 0.0f);
    print_vec3("  Mip 2 only (tz=0, center):", color);

    color = vec3_trilerp(m2_00, m2_10, m2_01, m2_11,
                          m3_00, m3_10, m3_01, m3_11,
                          0.5f, 0.5f, 1.0f);
    print_vec3("  Mip 3 only (tz=1, center):", color);

    color = vec3_trilerp(m2_00, m2_10, m2_01, m2_11,
                          m3_00, m3_10, m3_01, m3_11,
                          0.5f, 0.5f, 0.5f);
    print_vec3("  Blend 50/50 (tz=0.5):", color);

    /* ── 4. LOD selection ────────────────────────────────────────────── */

    print_header("4. LOD SELECTION -- HOW THE GPU PICKS THE MIP LEVEL");

    printf("  The GPU computes LOD from screen-space derivatives:\n");
    printf("  how much the UV changes from one pixel to the next.\n\n");

    printf("  Concept:\n");
    printf("    footprint = max(|dU/dx|, |dV/dy|) * texture_size\n");
    printf("    LOD = log2(footprint)\n\n");

    printf("  If one screen pixel covers 1 texel:  LOD = log2(1)  = 0\n");
    printf("  If one screen pixel covers 2 texels: LOD = log2(2)  = 1\n");
    printf("  If one screen pixel covers 4 texels: LOD = log2(4)  = 2\n");
    printf("  If one screen pixel covers 8 texels: LOD = log2(8)  = 3\n\n");

    printf("  The GPU uses ddx() and ddy() to compute UV rate-of-change\n");
    printf("  automatically in the fragment shader. You don't need to\n");
    printf("  compute LOD yourself -- but understanding it helps you:\n\n");
    printf("    * Debug mip level issues (wrong mip selected)\n");
    printf("    * Set min_lod / max_lod on samplers\n");
    printf("    * Understand LOD bias and when to use it\n\n");

    /* Simulate LOD for a floor at different distances */
    printf("  Simulated LOD for a 256x256 textured floor:\n\n");
    printf("  %-12s  %-14s  %-6s  %-12s  %-8s\n",
           "Distance", "Texels/pixel", "LOD", "Mip level", "Mip size");
    printf("  %-12s  %-14s  %-6s  %-12s  %-8s\n",
           "--------", "------------", "---", "---------", "--------");

    float distances[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f };
    int num_distances = (int)(sizeof(distances) / sizeof(distances[0]));
    int tex_size = BASE_TEX_SIZE;

    for (int i = 0; i < num_distances; i++) {
        float dist = distances[i];
        /* Simplified: assume texels_per_pixel scales linearly with distance */
        float texels_per_pixel = dist;
        float lod = forge_log2f(texels_per_pixel);
        float clamped_lod = forge_clampf(lod, 0.0f,
                                          (float)(num_levels - 1));
        int mip = (int)clamped_lod;
        int mip_size = tex_size >> mip;
        if (mip_size < 1) mip_size = 1;

        printf("  %-12.0f  %-14.1f  %-6.1f  %-12d  %dx%d\n",
               dist, texels_per_pixel, clamped_lod, mip,
               mip_size, mip_size);
    }

    printf("\n  As distance doubles, LOD increases by 1 (one mip level up).\n");
    printf("  This is why mipmaps use power-of-two sizes -- each doubling\n");
    printf("  of distance maps exactly to one level in the mip chain.\n");

    /* ── 5. Practical example ────────────────────────────────────────── */

    print_header("5. PRACTICAL EXAMPLE -- WHAT THE GPU DOES");

    printf("  When you sample a mipmapped texture, the GPU:\n\n");
    printf("    1. Computes UV derivatives (ddx/ddy) at each pixel\n");
    printf("    2. Calculates the footprint in texel space\n");
    printf("    3. LOD = log2(footprint)\n");
    printf("    4. Clamps LOD to [min_lod, max_lod] from the sampler\n");
    printf("    5. Splits LOD into integer + fractional parts\n");
    printf("    6. Bilinear samples from mip levels floor(LOD) and ceil(LOD)\n");
    printf("    7. Lerps between them using the fractional LOD\n\n");

    printf("  Example: LOD = 2.3\n\n");

    float lod = 2.3f;
    int mip_lo = (int)lod;
    int mip_hi = mip_lo + 1;
    float frac = lod - (float)mip_lo;

    printf("    Integer part:    %d  -> sample from mip level %d (%dx%d)\n",
           mip_lo, mip_lo, tex_size >> mip_lo, tex_size >> mip_lo);
    printf("    Ceiling:         %d  -> sample from mip level %d (%dx%d)\n",
           mip_hi, mip_hi, tex_size >> mip_hi, tex_size >> mip_hi);
    printf("    Fractional part: %.1f -> blend %.0f%% level %d + %.0f%% level %d\n\n",
           frac, (1.0f - frac) * 100.0f, mip_lo, frac * 100.0f, mip_hi);

    printf("  Sampler mipmap modes:\n\n");
    printf("    NEAREST mipmap: picks the single closest mip level\n");
    printf("      -> LOD 2.3 uses mip 2 only (snaps to nearest)\n");
    printf("      -> Fast but can show visible \"pops\" between levels\n\n");
    printf("    LINEAR mipmap (trilinear): blends between two levels\n");
    printf("      -> LOD 2.3 blends 70%% mip 2 + 30%% mip 3\n");
    printf("      -> Smooth transitions, the standard for 3D games\n");

    /* ── Summary ─────────────────────────────────────────────────────── */

    printf("\n");
    printf("==============================================================\n");
    printf("  Summary\n");
    printf("==============================================================\n");
    printf("\n");
    printf("  Mipmaps:\n");
    printf("    * Pre-filtered texture at progressively smaller sizes\n");
    printf("    * Each level halves the dimensions (256 -> 128 -> 64 -> ...)\n");
    printf("    * num_levels = floor(log2(size)) + 1\n");
    printf("    * Cost: ~33%% extra memory (fixed overhead)\n");
    printf("\n");
    printf("  LOD selection:\n");
    printf("    * footprint = how many texels one screen pixel covers\n");
    printf("    * LOD = log2(footprint)\n");
    printf("    * GPU computes this automatically using ddx/ddy\n");
    printf("\n");
    printf("  Trilinear interpolation:\n");
    printf("    * Bilinear sample from two adjacent mip levels\n");
    printf("    * Lerp between them using fractional LOD\n");
    printf("    * 8 texels sampled total (4 per level)\n");
    printf("    * Eliminates visible transitions between mip levels\n");
    printf("\n");
    printf("  Functions added to forge_math.h:\n");
    printf("    * forge_log2f(x)          -- log base 2 (mip level count)\n");
    printf("    * forge_clampf(x, lo, hi) -- clamp scalar (LOD clamping)\n");
    printf("    * forge_trilerpf(...)      -- scalar trilinear interpolation\n");
    printf("    * vec3_trilerp(...)        -- vec3 trilinear (RGB colors)\n");
    printf("    * vec4_trilerp(...)        -- vec4 trilinear (RGBA colors)\n");
    printf("\n");
    printf("  See: lessons/math/05-mipmaps-and-lod/README.md\n");
    printf("  See: lessons/gpu/05-mipmaps/ (SDL_GenerateMipmapsForGPUTexture)\n");
    printf("  See: lessons/math/04-bilinear-interpolation/ (the 2D building block)\n");
    printf("\n");

    SDL_Quit();
    return 0;
}
