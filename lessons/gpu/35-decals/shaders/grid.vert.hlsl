/*
 * Grid vertex shader — transforms a large world-space quad to clip space.
 *
 * The grid quad lives on the XZ plane at Y = 0, large enough to fill
 * the visible ground.  No model matrix — vertices are already in world space.
 *
 * Also computes light-clip coordinates for shadow map sampling so that
 * scene objects cast shadows onto the grid floor.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0, only attribute)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: VP + light VP matrices (128 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;        /* combined view-projection matrix */
    column_major float4x4 light_vp;  /* light view-projection for shadows */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer */
    float3 world_pos  : TEXCOORD0;   /* world-space position for grid math */
    float4 light_clip : TEXCOORD1;   /* light-space position for shadow    */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 world = float4(input.position, 1.0);
    output.clip_pos  = mul(vp, world);
    output.world_pos = input.position;
    output.light_clip = mul(light_vp, world);

    return output;
}
