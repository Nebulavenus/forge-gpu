/*
 * mesh.frag.hlsl — Fragment shader for textured mesh rendering
 *
 * Samples the diffuse texture at the interpolated UV coordinate.
 * No lighting in this lesson — just flat texture mapping.
 * Lesson 10 (Basic Lighting) will add diffuse and specular shading.
 *
 * SDL GPU HLSL binding conventions for fragment-stage resources:
 *   Textures:  register(t0, space2)  — fragment texture slot 0
 *   Samplers:  register(s0, space2)  — fragment sampler slot 0
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState smp         : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position; /* not used, but required by pipeline */
    float2 uv       : TEXCOORD0;   /* interpolated from vertex shader    */
};

float4 main(PSInput input) : SV_Target
{
    /* Sample the diffuse texture at the interpolated UV.
     * Because our texture is R8G8B8A8_UNORM_SRGB, the GPU automatically
     * converts sRGB texel values to linear space on read.  The sRGB
     * swapchain converts back to sRGB on write — correct color pipeline. */
    return diffuse_tex.Sample(smp, input.uv);
}
