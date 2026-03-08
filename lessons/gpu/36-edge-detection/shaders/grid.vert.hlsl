/*
 * Grid vertex shader — transforms a large world-space quad to clip space.
 *
 * The grid quad lives on the XZ plane at Y = 0, large enough to fill
 * the visible ground.  No model matrix — vertices are already in world space.
 *
 * Also computes light-clip coordinates for shadow map sampling and
 * view-space up normal for the G-buffer normal target.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0, only attribute)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: VP + light VP + view matrices (192 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;        /* combined view-projection matrix     */
    column_major float4x4 light_vp;  /* light view-projection for shadows   */
    column_major float4x4 view;      /* view matrix for view-space normals  */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer   */
    float3 world_pos  : TEXCOORD0;   /* world-space position for grid math   */
    float4 light_clip : TEXCOORD1;   /* light-space position for shadow      */
    float3 view_up    : TEXCOORD2;   /* view-space up normal for G-buffer    */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 world = float4(input.position, 1.0);
    output.clip_pos   = mul(vp, world);
    output.world_pos  = input.position;
    output.light_clip = mul(light_vp, world);

    /* Grid floor normal is always +Y in world space; transform to view space. */
    output.view_up = normalize(mul((float3x3)view, float3(0.0, 1.0, 0.0)));

    return output;
}
