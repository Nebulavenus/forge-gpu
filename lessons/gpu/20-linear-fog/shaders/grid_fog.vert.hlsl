/*
 * grid_fog.vert.hlsl — Vertex shader for the procedural grid floor with fog
 *
 * Identical to Lesson 18's grid.vert.hlsl: transforms grid vertices to
 * clip space and passes world position to the fragment shader.  The
 * fragment shader uses the world position for both the procedural grid
 * pattern and for computing the fog distance to the camera.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position (location 0)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: VP matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;  /* proj * view (no model — grid is at origin) */
};

struct VSInput
{
    float3 position : TEXCOORD0;
};

struct VSOutput
{
    float4 clip_pos  : SV_Position;
    float3 world_pos : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.clip_pos = mul(vp, float4(input.position, 1.0));
    output.world_pos = input.position;
    return output;
}
