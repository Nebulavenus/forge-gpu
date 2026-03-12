/*
 * Shadow pass vertex shader — transforms vertices to light clip space.
 *
 * Part of forge_scene.h — depth-only shadow map rendering.
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
