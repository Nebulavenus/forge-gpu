/*
 * shadow_vert.hlsl — Shadow pass vertex shader (depth-only)
 *
 * Transforms vertex positions to light clip space for shadow map
 * generation.  Only reads position data — normal, UV, and tangent
 * attributes are present in the vertex buffer but ignored.
 *
 * This shader is used by both the pipeline (48-byte) and raw (32-byte)
 * shadow pipelines.  The shader code is identical; the vertex attribute
 * layout (and therefore the GPU pipeline object) differs per path.
 *
 * Per-vertex attributes:
 *   TEXCOORD0 -> float3 position  (only attribute read)
 *
 * Uniform buffer:
 *   register(b0, space1) -> light_vp * model combined matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer ShadowUniforms : register(b0, space1) {
    column_major float4x4 mvp;   /* light_vp * model, pre-multiplied on CPU */
};

struct VSInput {
    float3 position : TEXCOORD0;
};

float4 main(VSInput input) : SV_Position {
    return mul(mvp, float4(input.position, 1.0));
}
