/*
 * Grid vertex shader — transform grid quad positions to clip space.
 *
 * The grid is a large flat quad on the XZ plane (Y=0). We pass through
 * the world position so the fragment shader can compute procedural lines.
 */

cbuffer VertUniforms : register(b0, space1)
{
    float4x4 vp_matrix;
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
