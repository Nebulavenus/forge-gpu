/*
 * tonemap.frag.hlsl — ACES tone mapping with bloom compositing
 *
 * Samples both the HDR render target and the bloom result, combines
 * them, and tone maps to LDR using the ACES filmic curve.
 *
 * Bloom is added BEFORE tone mapping so that bloom values participate
 * in the same HDR-to-LDR compression — highlights roll off naturally.
 *
 * Gamma: outputs LINEAR values. The sRGB swapchain applies gamma.
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    hdr_texture   : register(t0, space2);
SamplerState hdr_smp       : register(s0, space2);

Texture2D    bloom_texture : register(t1, space2);
SamplerState bloom_smp     : register(s1, space2);

cbuffer TonemapUniforms : register(b0, space3)
{
    float exposure;         /* scene exposure multiplier */
    float bloom_intensity;  /* bloom contribution strength */
    float _pad0;
    float _pad1;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

/* ACES filmic tone mapping (Narkowicz approximation). */
float3 tonemap_aces(float3 hdr)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e));
}

float4 main(PSInput input) : SV_Target
{
    float3 hdr = hdr_texture.Sample(hdr_smp, input.uv).rgb;

    /* Add bloom before tone mapping */
    float3 bloom = bloom_texture.Sample(bloom_smp, input.uv).rgb;
    hdr += bloom * bloom_intensity;

    /* Apply exposure and tone map */
    hdr *= exposure;
    float3 ldr = tonemap_aces(hdr);

    return float4(ldr, 1.0);
}
