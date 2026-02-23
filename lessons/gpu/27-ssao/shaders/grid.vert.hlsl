/*
 * Grid vertex shader — transform grid quad positions to clip space.
 *
 * The grid is a large flat quad on the XZ plane (Y=0). We pass through
 * the world position so the fragment shader can compute procedural lines.
 * View-space normal is computed here for the SSAO G-buffer.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 vp_matrix;   /* view-projection matrix */
    column_major float4x4 view_matrix; /* view matrix for normals */
    column_major float4x4 light_vp;    /* light view-projection */
};

struct VSInput
{
    float3 pos : TEXCOORD0;
};

struct VSOutput
{
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 view_nrm   : TEXCOORD1;
    float4 light_clip : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.world_pos  = input.pos;
    output.clip_pos   = mul(vp_matrix, float4(input.pos, 1.0));

    /* Grid normal is straight up — transform to view space for SSAO. */
    float3 world_normal = float3(0.0, 1.0, 0.0);
    output.view_nrm = normalize(mul((float3x3)view_matrix, world_normal));

    /* Light-space position for shadow mapping. */
    output.light_clip = mul(light_vp, float4(input.pos, 1.0));

    return output;
}
