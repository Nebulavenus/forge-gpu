/*
 * Shadow vertex shader — transform vertices into the spotlight's clip space.
 *
 * Unlike the point light shadow pass (Lesson 23) which uses a cube map with
 * 6 faces, a spotlight only needs a single 2D depth map rendered from its
 * perspective frustum. We just need standard MVP transformation here.
 */

cbuffer VertUniforms : register(b0, space1)
{
    float4x4 light_mvp; /* light VP * model matrix */
};

struct VSInput
{
    float3 pos    : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv     : TEXCOORD2;
};

float4 main(VSInput input) : SV_Position
{
    return mul(light_mvp, float4(input.pos, 1.0));
}
