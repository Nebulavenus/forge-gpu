/*
 * tonemap.frag.hlsl — Tone mapping with bloom compositing
 *
 * Samples both the HDR render target and the bloom result (mip 0 of the
 * bloom chain), combines them, and tone maps the result to LDR.
 *
 * The bloom contribution is added BEFORE tone mapping so that bloom
 * values participate in the same HDR-to-LDR compression.  This ensures
 * bloom highlights roll off naturally rather than clipping.
 *
 * Operators implemented:
 *   0 — No tone mapping (clamp to [0,1])
 *   1 — Reinhard: x / (x + 1)
 *   2 — ACES filmic (Narkowicz approximation)
 *
 * Gamma correction:
 *   This shader outputs LINEAR values.  The sRGB swapchain (set up
 *   with SDR_LINEAR) automatically applies the sRGB gamma curve.
 *
 * Uniform layout (16 bytes):
 *   float  exposure         (4 bytes) — exposure multiplier
 *   uint   tonemap_mode     (4 bytes) — 0=clamp, 1=Reinhard, 2=ACES
 *   float  bloom_intensity  (4 bytes) — bloom contribution strength
 *   float  _pad             (4 bytes)
 *
 * Fragment samplers:
 *   register(t0/s0, space2) -> HDR render target  (slot 0)
 *   register(t1/s1, space2) -> bloom texture      (slot 1)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    hdr_texture  : register(t0, space2);
SamplerState hdr_smp      : register(s0, space2);

Texture2D    bloom_texture : register(t1, space2);
SamplerState bloom_smp     : register(s1, space2);

cbuffer TonemapUniforms : register(b0, space3)
{
    float  exposure;         /* scene exposure multiplier (default 1.0)      */
    uint   tonemap_mode;     /* 0 = clamp, 1 = Reinhard, 2 = ACES           */
    float  bloom_intensity;  /* how much bloom to add (default 0.04)         */
    float  _pad;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

/* ── Reinhard tone mapping ───────────────────────────────────────────────── */
float3 tonemap_reinhard(float3 hdr)
{
    return hdr / (hdr + 1.0);
}

/* ── ACES filmic tone mapping ────────────────────────────────────────────── */
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
    /* Sample the HDR render target. */
    float3 hdr = hdr_texture.Sample(hdr_smp, input.uv).rgb;

    /* Sample the bloom result and add it to the HDR color.
     * Bloom is added before tone mapping so the combined result is
     * compressed together — bloom highlights roll off naturally. */
    float3 bloom = bloom_texture.Sample(bloom_smp, input.uv).rgb;
    hdr += bloom * bloom_intensity;

    /* Apply exposure. */
    hdr *= exposure;

    /* Apply the selected tone mapping operator. */
    float3 ldr;
    if (tonemap_mode == 1)
    {
        ldr = tonemap_reinhard(hdr);
    }
    else if (tonemap_mode == 2)
    {
        ldr = tonemap_aces(hdr);
    }
    else
    {
        ldr = saturate(hdr);
    }

    return float4(ldr, 1.0);
}
