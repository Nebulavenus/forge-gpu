/*
 * debug_quad.frag.hlsl — Grayscale depth visualization for shadow map debug
 *
 * Samples the first cascade's shadow map and displays it as a grayscale
 * image.  Near-zero depth (close to the light) appears dark, depth near
 * 1.0 appears white.  This helps verify that the shadow map is being
 * rendered correctly.
 *
 * Enabled by the --show-shadow-map command-line flag.
 *
 * Fragment sampler:
 *   register(t0/s0, space2) -> shadow map cascade 0 (slot 0)
 *
 * No fragment uniforms.
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    shadow_map : register(t0, space2);
SamplerState smp        : register(s0, space2);

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    /* Sample the depth value and display as grayscale.
     * Shadow maps store depth in the red channel (D32_FLOAT). */
    float depth = shadow_map.Sample(smp, input.uv).r;

    return float4(depth, depth, depth, 1.0);
}
