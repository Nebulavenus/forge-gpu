/*
 * particle.frag.hlsl — Textured particle fragment shader
 *
 * Samples the atlas texture at the interpolated UV and multiplies by the
 * particle's per-vertex color.  The same shader works for both additive
 * and alpha blending — the pipeline blend state determines the mode.
 *
 * Register layout (SDL GPU fragment conventions):
 *   t0, space2 — Texture2D       (atlas texture)
 *   s0, space2 — SamplerState    (linear, clamp-to-edge)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    atlas_tex : register(t0, space2);
SamplerState atlas_smp : register(s0, space2);

struct PSInput {
    float4 clip_pos : SV_Position;
    float4 color    : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target {
    float4 tex = atlas_tex.Sample(atlas_smp, input.uv);

    /* Non-premultiplied output: the pipeline blend state multiplies RGB by
     * SRC_ALPHA during compositing, so we store straight color here.
     * Alpha combines the atlas circle falloff with the particle's opacity. */
    float4 result;
    result.rgb = input.color.rgb;
    result.a   = tex.a * input.color.a;

    /* Discard fully transparent fragments to avoid wasting bandwidth */
    if (result.a < 0.004) discard;

    return result;
}
