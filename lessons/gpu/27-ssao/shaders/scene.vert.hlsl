/*
 * Scene vertex shader — transforms model vertices for the geometry pass.
 *
 * Outputs world-space position and normal for lighting, view-space normal
 * for the SSAO pass, UV coordinates for texturing, and the light-space
 * clip position for shadow mapping.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;      /* model-view-projection matrix */
    column_major float4x4 model;    /* model (world) matrix         */
    column_major float4x4 view;     /* camera view matrix           */
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
    float4 clip_pos      : SV_Position;
    float3 world_pos     : TEXCOORD0;
    float3 world_nrm     : TEXCOORD1;
    float2 uv            : TEXCOORD2;
    float3 view_nrm      : TEXCOORD3;
    float4 light_clip    : TEXCOORD4;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 world = mul(model, float4(input.pos, 1.0));
    output.world_pos  = world.xyz;
    output.clip_pos   = mul(mvp, float4(input.pos, 1.0));

    /* Transform normal to world space (safe for uniform scale). */
    float3 wn = normalize(mul((float3x3)model, input.normal));
    output.world_nrm = wn;

    /* Transform normal to view space for the SSAO G-buffer. */
    output.view_nrm = normalize(mul((float3x3)view, wn));

    output.uv = input.uv;

    /* Light-space position for shadow mapping. */
    output.light_clip = mul(light_vp, world);

    return output;
}
