/*
 * Scene vertex shader — transforms model vertices with MVP and passes
 * world-space position and normal to the fragment shader for lighting.
 */

cbuffer VertUniforms : register(b0, space1)
{
    float4x4 mvp;   /* model-view-projection matrix */
    float4x4 model; /* model (world) matrix         */
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
    float3 world_nrm : TEXCOORD1;
    float2 uv        : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 world = mul(model, float4(input.pos, 1.0));
    output.world_pos = world.xyz;
    output.clip_pos  = mul(mvp, float4(input.pos, 1.0));

    /* Transform normal to world space (safe for uniform scale). */
    output.world_nrm = normalize(mul((float3x3)model, input.normal));
    output.uv = input.uv;

    return output;
}
