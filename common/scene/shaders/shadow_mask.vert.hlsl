/*
 * shadow_mask.vert.hlsl — Alpha-masked shadow pass vertex shader
 *
 * Part of forge_scene.h — depth-only shadow map rendering for materials
 * with alpha_mode == MASK.  Passes through UVs so the fragment shader
 * can sample the base color texture and discard below the cutoff.
 *
 * Uses the same 48-byte model vertex layout but only reads position and UV.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (offset  0)
 *   TEXCOORD2 -> float2 uv        (offset 24)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: light view-projection * model matrix
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer ShadowUniforms : register(b0, space1)
{
    column_major float4x4 light_vp; /* light view-projection * model */
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;   /* unused but present in vertex layout */
    float2 uv       : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(light_vp, float4(input.position, 1.0));
    output.uv       = input.uv;
    return output;
}
