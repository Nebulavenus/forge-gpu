/*
 * bloom_downsample.frag.hlsl — Jimenez 13-tap weighted downsample
 *
 * Implements the downsample filter from Jorge Jimenez's SIGGRAPH 2014
 * presentation.  Adapted from Lesson 22 for the procedural sky's
 * bloom pipeline.
 *
 * 13 samples form 5 overlapping 2x2 boxes:
 *   a . b . c        box_tl = (a+b+f+g)/4
 *   . d . e .        box_tr = (b+c+g+h)/4
 *   f . g . h        box_bl = (f+g+k+l)/4
 *   . i . j .        box_br = (g+h+l+m)/4
 *   k . l . m        box_ct = (d+e+i+j)/4
 *
 * First pass uses Karis averaging (1/(1+luma) weighting) to suppress
 * firefly artifacts from the bright sun disc.
 *
 * Uniform layout (16 bytes):
 *   float2 texel_size   — 1/source_width, 1/source_height
 *   float  threshold    — brightness cutoff (first pass only)
 *   float  use_karis    — 1.0 for first pass, 0.0 otherwise
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    src_tex : register(t0, space2);
SamplerState src_smp : register(s0, space2);

cbuffer BloomDownsampleUniforms : register(b0, space3)
{
    float2 texel_size;
    float  threshold;
    float  use_karis;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

/* BT.709 luminance. */
float luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

/* Subtract threshold from luminance, scale to preserve hue. */
float3 apply_threshold(float3 c, float thresh)
{
    float luma = luminance(c);
    float contribution = max(luma - thresh, 0.0);
    return c * (contribution / max(luma, 0.0001));
}

/* Karis weight: 1/(1+luma) — suppresses firefly artifacts. */
float karis_weight(float3 c)
{
    return 1.0 / (1.0 + luminance(c));
}

float4 main(PSInput input) : SV_Target
{
    float2 uv = input.uv;
    float2 ts = texel_size;

    /* 13-tap sample positions. */
    float3 a = src_tex.Sample(src_smp, uv + float2(-2, -2) * ts).rgb;
    float3 b = src_tex.Sample(src_smp, uv + float2( 0, -2) * ts).rgb;
    float3 c = src_tex.Sample(src_smp, uv + float2( 2, -2) * ts).rgb;
    float3 d = src_tex.Sample(src_smp, uv + float2(-1, -1) * ts).rgb;
    float3 e = src_tex.Sample(src_smp, uv + float2( 1, -1) * ts).rgb;
    float3 f = src_tex.Sample(src_smp, uv + float2(-2,  0) * ts).rgb;
    float3 g = src_tex.Sample(src_smp, uv                       ).rgb;
    float3 h = src_tex.Sample(src_smp, uv + float2( 2,  0) * ts).rgb;
    float3 i = src_tex.Sample(src_smp, uv + float2(-1,  1) * ts).rgb;
    float3 j = src_tex.Sample(src_smp, uv + float2( 1,  1) * ts).rgb;
    float3 k = src_tex.Sample(src_smp, uv + float2(-2,  2) * ts).rgb;
    float3 l = src_tex.Sample(src_smp, uv + float2( 0,  2) * ts).rgb;
    float3 m = src_tex.Sample(src_smp, uv + float2( 2,  2) * ts).rgb;

    float3 result;

    if (use_karis > 0.5)
    {
        /* First pass: threshold + Karis averaging. */
        a = apply_threshold(a, threshold);
        b = apply_threshold(b, threshold);
        c = apply_threshold(c, threshold);
        d = apply_threshold(d, threshold);
        e = apply_threshold(e, threshold);
        f = apply_threshold(f, threshold);
        g = apply_threshold(g, threshold);
        h = apply_threshold(h, threshold);
        i = apply_threshold(i, threshold);
        j = apply_threshold(j, threshold);
        k = apply_threshold(k, threshold);
        l = apply_threshold(l, threshold);
        m = apply_threshold(m, threshold);

        float3 box_tl = (a + b + f + g) * 0.25;
        float3 box_tr = (b + c + g + h) * 0.25;
        float3 box_bl = (f + g + k + l) * 0.25;
        float3 box_br = (g + h + l + m) * 0.25;
        float3 box_ct = (d + e + i + j) * 0.25;

        float w_tl = karis_weight(box_tl);
        float w_tr = karis_weight(box_tr);
        float w_bl = karis_weight(box_bl);
        float w_br = karis_weight(box_br);
        float w_ct = karis_weight(box_ct);

        float corner_w = 0.125;
        float center_w = 0.5;

        result = box_tl * (w_tl * corner_w)
               + box_tr * (w_tr * corner_w)
               + box_bl * (w_bl * corner_w)
               + box_br * (w_br * corner_w)
               + box_ct * (w_ct * center_w);

        float total_w = (w_tl + w_tr + w_bl + w_br) * corner_w
                      + w_ct * center_w;
        result /= max(total_w, 0.0001);
    }
    else
    {
        /* Subsequent passes: standard uniform weighting. */
        float3 box_tl = (a + b + f + g) * 0.25;
        float3 box_tr = (b + c + g + h) * 0.25;
        float3 box_bl = (f + g + k + l) * 0.25;
        float3 box_br = (g + h + l + m) * 0.25;
        float3 box_ct = (d + e + i + j) * 0.25;

        result = 0.125 * (box_tl + box_tr + box_bl + box_br)
               + 0.5   * box_ct;
    }

    return float4(result, 1.0);
}
