/*
 * Grid vertex shader — transforms position-only vertices for the floor grid.
 *
 * The grid is a flat quad on the XZ plane. We pass world-space position
 * to the fragment shader for procedural line generation.
 */

cbuffer VertUniforms : register(b0, space1)
{
    float4x4 vp_matrix; /* view-projection matrix */
};

struct VSInput
{
    float3 pos : TEXCOORD0;
};

struct VSOutput
{
    float4 clip_pos  : SV_Position;
    float3 world_pos : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.world_pos = input.pos;
    output.clip_pos  = mul(vp_matrix, float4(input.pos, 1.0));
    return output;
}
