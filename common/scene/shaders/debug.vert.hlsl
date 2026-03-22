/*
 * debug.vert.hlsl — Debug line vertex shader
 *
 * Part of forge_scene.h — transforms world-space line vertices to clip space
 * and passes through the per-vertex color.  Used by both the depth-tested
 * (world) and depth-ignored (overlay) pipelines — the only difference is
 * the pipeline state, not the shader.
 *
 * Vertex attributes (28-byte stride):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float4 color     (location 1)
 *
 * Uniform buffer:
 *   register(b0, space1) -> view-projection matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer DebugUniforms : register(b0, space1)
{
    column_major float4x4 view_projection;
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

struct VSOutput
{
    float4 clip_pos : SV_Position;
    float4 color    : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.clip_pos = mul(view_projection, float4(input.position, 1.0));
    output.color    = input.color;
    return output;
}
