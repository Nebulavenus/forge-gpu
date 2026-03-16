/*
 * shadow_mask.frag.hlsl — Alpha-masked shadow pass fragment shader
 *
 * Part of forge_scene.h — samples the base color texture and discards
 * fragments whose alpha falls below the material's alpha_cutoff.
 * Surviving fragments write depth only (no color output needed, but
 * SDL GPU requires a fragment shader for every pipeline).
 *
 * Texture/sampler bindings (space2):
 *   slot 0 -> base color texture + sampler
 *
 * Uniform buffer:
 *   register(b0, space3) -> base_color_factor (RGBA) + alpha_cutoff
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    base_color_tex : register(t0, space2);
SamplerState base_color_smp : register(s0, space2);

cbuffer MaskUniforms : register(b0, space3)
{
    float4 base_color_factor; /* RGBA multiplier from material */
    float  alpha_cutoff;      /* discard threshold */
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float alpha = base_color_tex.Sample(base_color_smp, input.uv).a;
    alpha *= base_color_factor.a;
    if (alpha < alpha_cutoff)
        discard;
    return float4(0.0, 0.0, 0.0, 0.0);
}
