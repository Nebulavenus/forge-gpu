/*
 * grid.vert.hlsl — Grid vertex shader with shadow map support
 *
 * Transforms a large world-space quad to clip space, passes through
 * world-space position for the fragment shader's procedural grid, and
 * computes light-space positions for shadow map sampling.
 *
 * The grid quad lives on the XZ plane at Y = 0, large enough to fill
 * the visible ground.  No model matrix — vertices are already in world space.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0, only attribute)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: VP matrix (64 bytes)
 *   register(b1, space1) -> slot 1: 3 light VP matrices (192 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;   /* combined view-projection matrix */
};

cbuffer ShadowMatrices : register(b1, space1)
{
    column_major float4x4 light_vp[3];  /* one per cascade */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer */
    float3 world_pos  : TEXCOORD0;   /* world-space position for grid math */
    float4 light_pos0 : TEXCOORD1;   /* light-space pos, cascade 0        */
    float4 light_pos1 : TEXCOORD2;   /* light-space pos, cascade 1        */
    float4 light_pos2 : TEXCOORD3;   /* light-space pos, cascade 2        */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Grid vertices are already in world space, so we only need VP
     * (no model matrix).  The fragment shader uses world_pos directly. */
    float4 wp = float4(input.position, 1.0);
    output.clip_pos  = mul(vp, wp);
    output.world_pos = input.position;

    /* Compute light-space positions for shadow sampling */
    output.light_pos0 = mul(light_vp[0], wp);
    output.light_pos1 = mul(light_vp[1], wp);
    output.light_pos2 = mul(light_vp[2], wp);

    return output;
}
