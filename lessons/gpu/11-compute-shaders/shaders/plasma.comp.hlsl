/*
 * plasma.comp.hlsl — Compute shader that generates an animated plasma texture.
 *
 * Writes directly to a RWTexture2D using SV_DispatchThreadID to identify
 * which pixel each thread is responsible for.  The plasma effect is a sum
 * of sine waves at different frequencies and phases, animated by a time
 * uniform.  Colors are mapped to the forge diagram palette.
 *
 * Register layout (SDL3 GPU compute — DXIL):
 *   (u0, space1)  — read-write storage texture
 *   (b0, space2)  — uniform buffer (time, resolution)
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── Read-write storage texture (space1) ─────────────────────────────────── */
/* SDL3 GPU maps RW storage textures to (u[n], space1) for compute shaders.
 * This is the texture we write the plasma result into each frame. */
RWTexture2D<float4> output_tex : register(u0, space1);

/* ── Uniforms (space2) ───────────────────────────────────────────────────── */
/* Pushed via SDL_PushGPUComputeUniformData each frame.
 * Must be 16-byte aligned — pad to fill the full float4. */
cbuffer Params : register(b0, space2)
{
    float time;       /* elapsed seconds — drives the animation       */
    float width;      /* texture width in pixels                      */
    float height;     /* texture height in pixels                     */
    float _pad;       /* padding to 16-byte boundary                  */
};

/* ── Forge diagram palette (linear space) ────────────────────────────────── */
/* These are the same colors used in matplotlib diagrams throughout the
 * project, converted from sRGB hex to approximate linear RGB.
 *
 * The swapchain handles sRGB conversion, so we write linear values here. */
static const float3 CYAN   = float3(0.063f, 0.533f, 0.871f);  /* #4fc3f7 */
static const float3 ORANGE = float3(0.761f, 0.161f, 0.024f);  /* #ff7043 */
static const float3 GREEN  = float3(0.122f, 0.471f, 0.129f);  /* #66bb6a */
static const float3 PURPLE = float3(0.395f, 0.055f, 0.481f);  /* #ab47bc */

/* Dark blue background matching the project theme. */
static const float3 DARK_BG = float3(0.012f, 0.024f, 0.059f); /* ~#0b1428 */

/* ── Helper: smooth palette lookup ───────────────────────────────────────── */
/* Maps a value in [0, 1] to a color by interpolating through the four
 * palette colors.  Each color occupies a quarter of the range. */
float3 palette(float t)
{
    /* Wrap t into [0, 1) */
    t = frac(t);

    /* Four palette segments, each 0.25 wide */
    float segment = t * 4.0f;
    int   idx     = (int)segment;
    float frac_t  = frac(segment);

    /* Smoothstep for a softer transition between colors */
    float s = frac_t * frac_t * (3.0f - 2.0f * frac_t);

    if (idx == 0) return lerp(CYAN,   ORANGE, s);
    if (idx == 1) return lerp(ORANGE, GREEN,  s);
    if (idx == 2) return lerp(GREEN,  PURPLE, s);
    return              lerp(PURPLE, CYAN,   s);
}

/* ── Main compute kernel ─────────────────────────────────────────────────── */
/* Each thread writes one pixel.  Workgroup size 8x8 is a standard choice
 * for 2D image processing — 64 threads per group balances occupancy with
 * register pressure on most GPUs. */

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    /* Bounds check — the dispatch may overshoot the texture dimensions
     * if width/height aren't exact multiples of 8. */
    if (id.x >= (uint)width || id.y >= (uint)height)
        return;

    /* Normalise pixel coordinates to [0, 1] */
    float2 uv = float2((float)id.x / width, (float)id.y / height);

    /* Centre the coordinate system at (0, 0) and scale to [-1, 1] */
    float2 p = (uv - 0.5f) * 2.0f;

    /* ── Plasma function ─────────────────────────────────────────────
     * Sum several sine waves at different frequencies and orientations.
     * Each wave contributes a value in [-1, 1]; the sum is normalised
     * to [0, 1] for the palette lookup. */
    float v = 0.0f;

    /* Wave 1: horizontal ripple */
    v += sin(p.x * 3.0f + time * 1.2f);

    /* Wave 2: vertical ripple, different frequency */
    v += sin(p.y * 4.0f + time * 0.8f);

    /* Wave 3: diagonal wave */
    v += sin((p.x + p.y) * 2.5f + time * 1.5f);

    /* Wave 4: radial (circular) wave from centre */
    float r = length(p);
    v += sin(r * 5.0f - time * 2.0f);

    /* Wave 5: rotating spiral pattern */
    float angle = atan2(p.y, p.x);
    v += sin(angle * 3.0f + r * 4.0f + time * 1.0f);

    /* Normalise from [-5, 5] to [0, 1] */
    v = v / 10.0f + 0.5f;

    /* ── Map to palette ──────────────────────────────────────────────
     * Shift the palette lookup by time so the colors cycle smoothly. */
    float3 color = palette(v + time * 0.1f);

    /* Blend with the dark background at the edges for a vignette effect.
     * This gives the plasma a natural fade-out toward the window border. */
    float vignette = 1.0f - smoothstep(0.5f, 1.2f, r);
    color = lerp(DARK_BG, color, vignette);

    output_tex[id.xy] = float4(color, 1.0f);
}
