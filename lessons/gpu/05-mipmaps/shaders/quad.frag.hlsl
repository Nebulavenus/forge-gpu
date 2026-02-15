/*
 * quad.frag.hlsl — Fragment shader with texture sampling (same as Lesson 04)
 *
 * The fragment shader is identical to Lesson 04.  The mipmap behavior is
 * entirely controlled by:
 *   - The texture (which now has multiple mip levels)
 *   - The sampler (which controls mipmap_mode, min_lod, max_lod)
 *
 * The GPU automatically computes derivatives (ddx/ddy of the UVs) and
 * selects the appropriate mip level.  The shader doesn't need to know
 * anything about mipmaps — it just calls tex.Sample() and the hardware
 * handles the rest.
 *
 * SPDX-License-Identifier: Zlib
 */

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
