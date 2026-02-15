/*
 * quad.frag.hlsl — Fragment shader with texture sampling
 *
 * Samples a 2D texture using the interpolated UV coordinates from the
 * vertex shader.  This is the core of texture mapping: the UV tells us
 * where on the image to look, the sampler tells us how to filter when
 * the texel grid doesn't align exactly with the pixel grid.
 *
 * SDL GPU HLSL binding conventions for fragment-stage resources:
 *   Textures:  register(t0, space2)   — fragment texture slot 0
 *   Samplers:  register(s0, space2)   — fragment sampler slot 0
 *
 * The slot index (t0/s0, t1/s1, …) matches the array index you pass
 * to SDL_BindGPUFragmentSamplers.  space2 is mandatory for fragment-
 * stage textures and samplers in SDL's binding model.
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── Texture and sampler ──────────────────────────────────────────────
 * Texture2D holds the image data on the GPU.
 * SamplerState controls filtering (linear, nearest) and address mode
 * (repeat, clamp) when the shader reads from the texture. */
Texture2D    tex : register(t0, space2);
SamplerState smp : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position; /* not used, but required by pipeline */
    float2 uv       : TEXCOORD0;   /* interpolated from vertex shader    */
};

float4 main(PSInput input) : SV_Target
{
    /* Sample the texture at the interpolated UV coordinate.
     * The sampler controls what happens at texel boundaries (filtering)
     * and at UV values outside 0–1 (address mode).
     *
     * Because our texture is R8G8B8A8_UNORM_SRGB, the GPU automatically
     * converts sRGB color values to linear space on read.  Combined with
     * the sRGB swapchain (which converts linear back to sRGB on write),
     * this gives us a correct color pipeline end-to-end. */
    return tex.Sample(smp, input.uv);
}
