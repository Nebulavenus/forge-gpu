/*
 * bloom_upsample.frag.hlsl — 9-tap tent filter for bloom upsample
 *
 * Implements Jimenez's tent-filter upsample for the bloom pipeline.
 * Adapted from Lesson 22 for the procedural sky.
 *
 * Uses additive blending (ONE + ONE) so the upsampled contribution
 * accumulates on top of the existing downsample data in each mip level.
 *
 * Tent filter weights (sum = 1.0):
 *   1/16  2/16  1/16
 *   2/16  4/16  2/16
 *   1/16  2/16  1/16
 *
 * Uniform layout (16 bytes):
 *   float2 texel_size   — 1/source_width, 1/source_height
 *   float2 _pad
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    src_tex : register(t0, space2);
SamplerState src_smp : register(s0, space2);

cbuffer BloomUpsampleUniforms : register(b0, space3)
{
    float2 texel_size;
    float2 _pad;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float2 uv = input.uv;
    float2 ts = texel_size;

    /* 9-tap tent filter. */

    /* Corners: weight 1/16 each */
    float3 color  = src_tex.Sample(src_smp, uv + float2(-1, -1) * ts).rgb * (1.0 / 16.0);
    color        += src_tex.Sample(src_smp, uv + float2( 1, -1) * ts).rgb * (1.0 / 16.0);
    color        += src_tex.Sample(src_smp, uv + float2(-1,  1) * ts).rgb * (1.0 / 16.0);
    color        += src_tex.Sample(src_smp, uv + float2( 1,  1) * ts).rgb * (1.0 / 16.0);

    /* Edges: weight 2/16 each */
    color        += src_tex.Sample(src_smp, uv + float2( 0, -1) * ts).rgb * (2.0 / 16.0);
    color        += src_tex.Sample(src_smp, uv + float2(-1,  0) * ts).rgb * (2.0 / 16.0);
    color        += src_tex.Sample(src_smp, uv + float2( 1,  0) * ts).rgb * (2.0 / 16.0);
    color        += src_tex.Sample(src_smp, uv + float2( 0,  1) * ts).rgb * (2.0 / 16.0);

    /* Center: weight 4/16 */
    color        += src_tex.Sample(src_smp, uv).rgb * (4.0 / 16.0);

    return float4(color, 1.0);
}
