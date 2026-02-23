/*
 * Shadow vertex shader — transform vertices into light clip space.
 *
 * Renders the scene from the directional light's point of view using an
 * orthographic projection. The resulting depth buffer is sampled later
 * in the geometry pass for shadow testing.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 light_mvp; /* light VP * model matrix */
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
