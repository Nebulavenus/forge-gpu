/*
 * UI vertex shader — transforms pixel-space UI vertices to clip space.
 *
 * Part of forge_scene.h — immediate-mode UI rendering pipeline.
 *
 * Vertex attributes (ForgeUiVertex layout):
 *   TEXCOORD0 -> position (float2, offset  0) -- location 0
 *   TEXCOORD1 -> uv       (float2, offset  8) -- location 1
 *   TEXCOORD2 -> color_rg (float2, offset 16) -- location 2
 *   TEXCOORD3 -> color_ba (float2, offset 24) -- location 3
 *
 * Vertex uniform:
 *   register(b0, space1) -> orthographic projection matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer UiUniforms : register(b0, space1)
{
    column_major float4x4 projection;
};

struct VSInput
{
    float2 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float2 color_rg : TEXCOORD2;
    float2 color_ba : TEXCOORD3;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    output.position = mul(projection, float4(input.position, 0.0, 1.0));
    output.uv = input.uv;
    output.color = float4(input.color_rg, input.color_ba);

    return output;
}
