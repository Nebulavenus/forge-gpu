/*
 * debug.vert.hlsl — Vertex shader for debug line rendering
 *
 * Transforms world-space line vertices to clip space and passes through
 * the per-vertex color for interpolation.  Used by both the depth-tested
 * (world) and depth-ignored (overlay) pipelines — the only difference
 * is the pipeline state, not the shader.
 *
 * Vertex attributes (matching DebugVertex layout):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float4 color     (location 1)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex uniform slot 0
 *   Contains the combined view_projection matrix (64 bytes).
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer DebugUniforms : register(b0, space1)
{
    column_major float4x4 view_projection;
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float4 color    : TEXCOORD1;   /* vertex attribute location 1 */
};

struct VSOutput
{
    float4 clip_pos : SV_Position; /* clip-space position for rasterizer */
    float4 color    : TEXCOORD0;   /* interpolated color for fragment    */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Transform world-space position to clip space. */
    output.clip_pos = mul(view_projection, float4(input.position, 1.0));

    /* Pass through the vertex color for interpolation. */
    output.color = input.color;

    return output;
}
