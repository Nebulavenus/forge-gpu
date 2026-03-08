/*
 * Shadow pass vertex shader — transforms vertices to light clip space.
 *
 * Uses traditional vertex input with a single position attribute.
 * Only position is needed — normals are unused for the depth-only pass.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: light view-projection matrix
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer ShadowUniforms : register(b0, space1)
{
    column_major float4x4 light_vp; /* light view-projection * model */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
};

float4 main(VSInput input) : SV_Position
{
    return mul(light_vp, float4(input.position, 1.0));
}
