/*
 * UI fragment shader — samples the font atlas and applies vertex color.
 *
 * Part of forge_scene.h — immediate-mode UI rendering pipeline.
 * The atlas is single-channel R8_UNORM; the red channel holds alpha coverage.
 *
 * Fragment samplers:
 *   register(t0, space2) -> atlas texture (R8_UNORM)
 *   register(s0, space2) -> atlas sampler (linear, clamp-to-edge)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    atlas_tex : register(t0, space2);
SamplerState atlas_smp : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

/* sRGB EOTF: convert sRGB-encoded channel to linear light. */
float srgb_to_linear(float c)
{
    return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float4 main(PSInput input) : SV_Target0
{
    float coverage = atlas_tex.Sample(atlas_smp, input.uv).r;

    /* Linearize sRGB vertex colors for correct framebuffer encoding. */
    float4 result;
    result.r = srgb_to_linear(input.color.r);
    result.g = srgb_to_linear(input.color.g);
    result.b = srgb_to_linear(input.color.b);
    result.a = input.color.a * coverage;

    return result;
}
