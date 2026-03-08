/*
 * X-ray stencil mark vertex shader — writes to stencil buffer only.
 *
 * Transforms vertices to clip space for stencil marking.  The associated
 * pipeline has color write mask = 0 and uses the stencil buffer to mark
 * occluded regions for the ghost (X-ray) effect.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: MVP matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer MarkVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp; /* model-view-projection matrix */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
};

float4 main(VSInput input) : SV_Position
{
    return mul(mvp, float4(input.position, 1.0));
}
