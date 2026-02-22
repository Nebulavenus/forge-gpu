/*
 * bloom_upsample.frag.hlsl — 9-tap tent filter for bloom upsample
 *
 * Implements the upsample filter from Jorge Jimenez's dual-filter bloom
 * (SIGGRAPH 2014).  The tent filter produces a smooth, artifact-free
 * upsample by weighting 9 samples in a 3x3 pattern.
 *
 * The pipeline uses additive blending (ONE + ONE) so the upsampled
 * contribution accumulates on top of the existing downsample data.
 *
 * Tent filter weights (sum = 1.0):
 *   1/16  2/16  1/16
 *   2/16  4/16  2/16
 *   1/16  2/16  1/16
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    src_tex : register(t0, space2);
SamplerState src_smp : register(s0, space2);

cbuffer BloomUpsampleUniforms : register(b0, space3)
{
    float2 texel_size;   /* 1.0 / source dimensions */
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

    /* ── 9-tap tent filter ──────────────────────────────────────────── */

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
