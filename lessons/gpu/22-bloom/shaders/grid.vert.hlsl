/*
 * grid.vert.hlsl — Grid vertex shader (point light, no shadows)
 *
 * Transforms a large world-space quad to clip space, passes through
 * world-space position for the fragment shader's procedural grid.
 * Simplified from Lesson 21 by removing shadow map outputs.
 *
 * The grid quad lives on the XZ plane at Y = 0, large enough to fill
 * the visible ground.  No model matrix — vertices are already in world space.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0, only attribute)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: VP matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;   /* combined view-projection matrix */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer */
    float3 world_pos  : TEXCOORD0;   /* world-space position for grid math */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Grid vertices are already in world space, so we only need VP
     * (no model matrix).  The fragment shader uses world_pos directly. */
    output.clip_pos  = mul(vp, float4(input.position, 1.0));
    output.world_pos = input.position;

    return output;
}
