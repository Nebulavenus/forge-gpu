/*
 * tonemap.frag.hlsl — Tone mapping from HDR to LDR
 *
 * Samples the floating-point HDR render target and compresses its
 * values into the displayable [0, 1] range using a tone mapping operator.
 *
 * Why tone mapping?
 *   The HDR render target stores linear-light values that can be
 *   arbitrarily large (specular highlights might reach 5.0 or more).
 *   A display can only show [0, 1].  Without tone mapping, everything
 *   above 1.0 is clamped to white — all highlight detail is lost.
 *
 *   Tone mapping operators are carefully designed curves that compress
 *   the full HDR range into [0, 1] while preserving relative brightness.
 *   Dark values stay mostly unchanged; bright values are gradually
 *   compressed so highlights retain shape and variation.
 *
 * Operators implemented:
 *
 *   0 — No tone mapping (saturate/clamp).
 *       HDR values above 1.0 are clamped.  Included for comparison
 *       to show what is lost without tone mapping.
 *
 *   1 — Reinhard.
 *       x / (x + 1).  Simple and intuitive: maps [0, inf) to [0, 1).
 *       Preserves color ratios.  Tends to produce desaturated results
 *       because very bright values approach 1.0 in all channels.
 *
 *   2 — ACES filmic (Narkowicz approximation).
 *       A polynomial fit to the Academy Color Encoding System (ACES)
 *       reference rendering transform.  Produces richer contrast and
 *       slight saturation compared to Reinhard.  The most common
 *       tone mapper in modern game engines.
 *
 * Gamma correction:
 *   This shader outputs LINEAR values.  The sRGB swapchain (set up
 *   with SDR_LINEAR) automatically applies the sRGB gamma curve when
 *   writing pixels.  No manual pow(1/2.2) is needed.
 *
 * Uniform layout (16 bytes):
 *   float  exposure       (4 bytes) — exposure multiplier
 *   uint   tonemap_mode   (4 bytes) — 0=clamp, 1=Reinhard, 2=ACES
 *   float2 _pad           (8 bytes) — padding to 16 bytes
 *
 * Fragment sampler:
 *   register(t0/s0, space2) -> HDR render target (slot 0)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    hdr_texture : register(t0, space2);
SamplerState smp         : register(s0, space2);

cbuffer TonemapUniforms : register(b0, space3)
{
    float  exposure;       /* scene exposure multiplier (default 1.0)      */
    uint   tonemap_mode;   /* 0 = clamp, 1 = Reinhard, 2 = ACES           */
    float2 _pad;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

/* ── Reinhard tone mapping ───────────────────────────────────────────────── */
/* Maps [0, infinity) to [0, 1).  Simple, but can look desaturated at
 * extreme brightness because all channels converge toward 1.0. */
float3 tonemap_reinhard(float3 hdr)
{
    return hdr / (hdr + 1.0);
}

/* ── ACES filmic tone mapping ────────────────────────────────────────────── */
/* Krzysztof Narkowicz's polynomial approximation of the ACES reference
 * rendering transform.  The five coefficients (a-e) were fit to match
 * the Academy's filmic S-curve, which lifts shadows slightly, maintains
 * mid-tone contrast, and rolls off highlights smoothly. */
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
    float3 hdr = hdr_texture.Sample(smp, input.uv).rgb;

    /* Apply exposure — a multiplier that scales all HDR values before
     * tone mapping.  Increasing exposure brightens the scene (useful for
     * seeing detail in dark areas); decreasing it reveals highlight detail.
     * This is analogous to a camera's exposure setting. */
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
        /* Mode 0: no tone mapping — just clamp to [0, 1].
         * This shows what happens without tone mapping: all values
         * above 1.0 are lost, highlights blow out to flat white. */
        ldr = saturate(hdr);
    }

    /* Output linear LDR.  The sRGB swapchain applies gamma automatically.
     * If the swapchain were NOT sRGB, we would need:
     *   ldr = pow(ldr, 1.0 / 2.2);
     * But with SDR_LINEAR, the GPU handles this conversion for us. */
    return float4(ldr, 1.0);
}
