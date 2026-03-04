/*
 * Skybox fragment shader — sample a cube map along the interpolated direction.
 *
 * SPDX-License-Identifier: Zlib
 */

TextureCube  skybox_tex : register(t0, space2);
SamplerState smp        : register(s0, space2);

struct PSInput
{
    float4 clip_pos  : SV_Position;
    float3 direction : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return skybox_tex.Sample(smp, input.direction);
}
