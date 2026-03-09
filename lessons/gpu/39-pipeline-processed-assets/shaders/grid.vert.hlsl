/*
 * grid_vert.hlsl — Grid floor vertex shader
 *
 * Transforms grid quad vertices and passes world position to the fragment
 * shader for procedural grid line generation.  Same pattern as L12+.
 *
 * Per-vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *
 * Uniform buffer:
 *   register(b0, space1) -> VP matrix
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridVertUniforms : register(b0, space1) {
    column_major float4x4 vp;
};

struct VSInput {
    float3 position : TEXCOORD0;
};

struct VSOutput {
    float4 clip_pos  : SV_Position;
    float3 world_pos : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.clip_pos  = mul(vp, float4(input.position, 1.0));
    output.world_pos = input.position;
    return output;
}
