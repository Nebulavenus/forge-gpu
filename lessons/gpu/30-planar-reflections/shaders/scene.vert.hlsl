/*
 * Scene vertex shader — transforms vertices for the main and reflection passes.
 *
 * Outputs world-space position, normal, UV, and light-space clip position
 * for shadow mapping.  Used for boat, rocks, and the sandy floor.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;      /* model-view-projection matrix */
    column_major float4x4 model;    /* model (world) matrix         */
    column_major float4x4 light_vp; /* light view-projection matrix */
};

struct VSInput
{
    float3 pos    : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv     : TEXCOORD2;
};

struct VSOutput
{
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 world_nrm  : TEXCOORD1;
    float2 uv         : TEXCOORD2;
    float4 light_clip : TEXCOORD3;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 world = mul(model, float4(input.pos, 1.0));
    output.world_pos  = world.xyz;
    output.clip_pos   = mul(mvp, float4(input.pos, 1.0));

    /* Transform normal to world space (safe for uniform scale). */
    output.world_nrm = normalize(mul((float3x3)model, input.normal));

    output.uv = input.uv;

    /* Light-space position for shadow mapping. */
    output.light_clip = mul(light_vp, world);

    return output;
}
