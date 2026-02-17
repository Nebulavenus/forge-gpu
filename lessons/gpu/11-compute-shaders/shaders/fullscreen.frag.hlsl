/*
 * fullscreen.frag.hlsl — Fragment shader that samples the compute-generated texture.
 *
 * This is intentionally simple: sample the plasma texture and output it.
 * The compute shader has already done all the interesting work — this shader
 * just displays the result.
 *
 * Register layout (SDL3 GPU fragment/pixel shader — DXIL):
 *   (t0, space2) — sampled texture
 *   (s0, space2) — sampler
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── Sampled texture + sampler (fragment space2) ─────────────────────────── */
/* SDL3 GPU maps fragment shader sampled textures to (t[n], space2) and
 * their corresponding samplers to (s[n], space2). */
Texture2D    tex : register(t0, space2);
SamplerState smp : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return tex.Sample(smp, input.uv);
}
