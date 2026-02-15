/*
 * quad.vert.hlsl — Vertex shader for mipmapped textured quad
 *
 * Nearly identical to Lesson 04's vertex shader, with one addition:
 * the uniform buffer now contains a uv_scale factor that multiplies
 * the UV coordinates, causing the texture to tile across the quad.
 * Tiling makes mipmap behavior much more visible — when a repeating
 * pattern covers many screen pixels, aliasing is obvious without mipmaps.
 *
 * Vertex attributes:
 *   TEXCOORD0 → float2 position  (location 0)
 *   TEXCOORD1 → float2 uv        (location 1)
 *
 * Uniform buffer:
 *   register(b0, space1) → vertex shader uniform slot 0
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer Uniforms : register(b0, space1)
{
    float time;       /* elapsed time in seconds   */
    float aspect;     /* window width / height     */
    float uv_scale;   /* UV multiplier for tiling  */
    float _pad;       /* padding to 16-byte alignment */
};

struct VSInput
{
    float2 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* ── Scale the quad by a time-varying factor ─────────────────────
     * The quad pulses size with a sine wave so you can see the mip
     * transitions as the texture gets smaller and larger on screen. */
    float scale = 0.5 + 0.4 * sin(time * 0.5);
    float2 scaled = input.position * scale;

    /* Aspect ratio correction (same as Lesson 04) */
    float2 corrected = float2(scaled.x / aspect, scaled.y);

    output.position = float4(corrected, 0.0, 1.0);

    /* ── UV scaling ──────────────────────────────────────────────────
     * Multiply UVs by uv_scale to tile the texture.  A scale of 8
     * means the checker pattern repeats 8 times across the quad,
     * creating many small squares that make aliasing highly visible. */
    output.uv = input.uv * uv_scale;

    return output;
}
