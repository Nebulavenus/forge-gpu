/*
 * scene.frag.hlsl — Fragment shader for multi-material glTF rendering
 *
 * Handles both textured and solid-color materials:
 *   - If has_texture is non-zero, samples the diffuse texture and multiplies
 *     by base_color (which is usually white for textured materials).
 *   - If has_texture is zero, outputs base_color directly.
 *
 * This lets us render an entire glTF scene with a single shader and pipeline,
 * switching between textured and solid-color materials via push uniforms.
 *
 * SDL GPU HLSL binding conventions for fragment-stage resources:
 *   Textures:  register(t0, space2)  — fragment texture slot 0
 *   Samplers:  register(s0, space2)  — fragment sampler slot 0
 *   Uniforms:  register(b0, space3)  — fragment uniform slot 0
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState smp         : register(s0, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;    /* material base color (RGBA) */
    uint   has_texture;   /* non-zero = sample texture, zero = solid color */
    uint   _pad0;         /* padding to 16-byte boundary */
    uint   _pad1;
    uint   _pad2;
};

struct PSInput
{
    float4 position : SV_Position; /* not used, but required by pipeline */
    float2 uv       : TEXCOORD0;   /* interpolated from vertex shader    */
    float3 normal   : TEXCOORD1;   /* interpolated normal (unused here)  */
};

float4 main(PSInput input) : SV_Target
{
    if (has_texture)
    {
        /* Sample the diffuse texture and modulate by the material's base color.
         * For most textured glTF materials, base_color is (1,1,1,1), so this
         * just returns the texture color.  But some materials tint textures. */
        return diffuse_tex.Sample(smp, input.uv) * base_color;
    }
    else
    {
        /* No texture — use the solid base color directly.
         * The 1x1 white placeholder texture is still bound (to avoid
         * undefined behavior), but we skip sampling it. */
        return base_color;
    }
}
