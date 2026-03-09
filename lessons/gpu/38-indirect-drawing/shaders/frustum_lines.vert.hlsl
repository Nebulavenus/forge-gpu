/*
 * frustum_lines.vert.hlsl — Vertex shader for frustum wireframe lines
 *
 * Transforms line vertices (position + color) by the debug camera's VP matrix.
 * Used to visualize the main camera's view frustum in the debug viewport.
 *
 * Per-vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float4 color     (location 1)
 *
 * Uniform buffer:
 *   register(b0, space1) -> debug camera VP matrix
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer LineUniforms : register(b0, space1) {
    column_major float4x4 line_vp;
};

struct VSInput {
    float3 position : TEXCOORD0;
    float4 color    : TEXCOORD1;
};

struct VSOutput {
    float4 clip_pos : SV_Position;
    float4 color    : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.clip_pos = mul(line_vp, float4(input.position, 1.0));
    output.color    = input.color;
    return output;
}
