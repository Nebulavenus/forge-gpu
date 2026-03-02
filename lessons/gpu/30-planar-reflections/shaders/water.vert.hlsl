/*
 * Water vertex shader — transform a water quad and compute screen-space UV.
 *
 * The water surface is a large quad at a fixed Y level.  The vertex shader
 * computes the clip-space position and passes it to the fragment shader,
 * which uses it to project into screen-space UV for reflection sampling.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp; /* model-view-projection matrix */
};

struct VSInput
{
    float3 pos    : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv     : TEXCOORD2;
};

struct VSOutput
{
    float4 clip_pos  : SV_Position;
    float3 world_pos : TEXCOORD0;
    float4 proj_pos  : TEXCOORD1; /* clip-space pos for screen UV */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    output.world_pos = input.pos;
    output.clip_pos  = mul(mvp, float4(input.pos, 1.0));
    output.proj_pos  = output.clip_pos;

    return output;
}
