/*
 * Tone Map Fragment Shader
 *
 * Reads the HDR scene render target and the bloom mip chain result,
 * combines them, applies exposure scaling, then maps the high-dynamic-range
 * values to [0,1] using one of three operators:
 *   0 — Clamp (saturate): values above 1.0 are lost
 *   1 — Reinhard: smooth roll-off, maps 0→0 and ∞→1
 *   2 — ACES filmic: S-curve with lifted shadows and rich highlights
 *
 * The sRGB swapchain handles gamma automatically — no manual pow(1/2.2).
 *
 * SPDX-License-Identifier: Zlib
 */

/* HDR scene render target.
 * Fragment samplers use space2 in SDL GPU's HLSL register convention. */
Texture2D    hdr_texture   : register(t0, space2);
SamplerState hdr_sampler   : register(s0, space2);

/* Bloom result — the largest bloom mip after upsample accumulation. */
Texture2D    bloom_texture : register(t1, space2);
SamplerState bloom_sampler : register(s1, space2);

/* Tone mapping parameters. */
cbuffer TonemapUniforms : register(b0, space3)
{
    float exposure;        /* brightness multiplier applied before tone mapping */
    uint  tonemap_mode;    /* 0 = clamp, 1 = Reinhard, 2 = ACES */
    float bloom_intensity; /* bloom contribution (0 = no bloom) */
    float _pad;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

/* ── Tone mapping operators ──────────────────────────────────────────────── */

/* Reinhard: simple, well-understood. Maps 0→0, ∞→1.
 * Preserves relative brightness but can look flat in highlights. */
float3 tonemap_reinhard(float3 hdr)
{
    return hdr / (hdr + 1.0);
}

/* ACES filmic approximation (Narkowicz 2015).
 * S-shaped curve: lifts shadows, compresses highlights, saturates colors
 * slightly — the standard in real-time rendering. */
float3 tonemap_aces(float3 hdr)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e));
}

/* ── Main ────────────────────────────────────────────────────────────────── */

float4 main(PSInput input) : SV_Target
{
    /* Sample the HDR render target. */
    float3 hdr = hdr_texture.Sample(hdr_sampler, input.uv).rgb;

    /* Add bloom contribution — bloom must be combined in HDR space
     * BEFORE tone mapping so the combined result compresses naturally. */
    float3 bloom = bloom_texture.Sample(bloom_sampler, input.uv).rgb;
    hdr += bloom * bloom_intensity;

    /* Apply exposure — a multiplier that scales all HDR values before
     * tone mapping.  Higher exposure brightens the image. */
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
        /* Mode 0: no tone mapping — just clamp to [0,1]. */
        ldr = saturate(hdr);
    }

    /* Output linear LDR.  The sRGB swapchain applies gamma automatically. */
    return float4(ldr, 1.0);
}
