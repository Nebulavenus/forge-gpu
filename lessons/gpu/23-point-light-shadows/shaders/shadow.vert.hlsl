/*
 * Shadow vertex shader — transforms vertices for cube map shadow rendering.
 *
 * Each face of the shadow cube map uses a different view-projection matrix
 * (light_mvp) to project geometry onto that face. The world-space position
 * is passed to the fragment shader for linear depth calculation.
 */

cbuffer VertUniforms : register(b0, space1)
{
    float4x4 light_mvp; /* light view-projection * model matrix */
    float4x4 model;     /* model (world) matrix                 */
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
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.clip_pos  = mul(light_mvp, float4(input.pos, 1.0));
    output.world_pos = mul(model, float4(input.pos, 1.0)).xyz;
    return output;
}
