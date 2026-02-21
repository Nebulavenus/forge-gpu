/*
 * emissive.frag.hlsl — Constant HDR emission color
 *
 * Outputs a constant bright color (the emission_color uniform) regardless
 * of lighting or surface properties.  Used for the emissive sphere that
 * serves as the visible point light source.
 *
 * The emission values are far above 1.0 (e.g. (50, 45, 40)) so the sphere
 * appears blindingly bright in HDR.  This is what creates the bloom glow —
 * the bloom downsample pass detects these bright pixels and spreads them.
 *
 * Reuses scene.vert.hlsl for vertex transformation — only the fragment
 * shader differs.
 *
 * Uniform layout (16 bytes):
 *   float3 emission_color     (12 bytes) — HDR emission RGB
 *   float  _pad                (4 bytes)
 *
 * No fragment samplers — purely uniform-driven.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer EmissiveUniforms : register(b0, space3)
{
    float3 emission_color;   /* HDR emission (values >> 1.0) */
    float  _pad;
};

struct PSInput
{
    float4 clip_pos   : SV_Position;
    float2 uv         : TEXCOORD0;
    float3 world_norm : TEXCOORD1;
    float3 world_pos  : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target
{
    /* Output the emission color directly — no lighting computation.
     * The high HDR values (50+) will trigger the bloom threshold and
     * produce a visible glow around the sphere. */
    return float4(emission_color, 1.0);
}
