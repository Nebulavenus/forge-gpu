/*
 * Decal vertex shader — transforms a unit cube by the decal's MVP.
 *
 * Each decal is a unit cube [-0.5, 0.5]^3 positioned in world space by
 * the decal model matrix.  The vertex shader projects this box into clip
 * space.  The fragment shader then reconstructs the world position from
 * the scene depth buffer and projects it into decal local space.
 *
 * Uses the same Vertex layout as L34 cubes (position + normal, 24 bytes).
 * Only position is used; normal is declared for stride compatibility.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1, unused)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: decal MVP matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer DecalVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp; /* decal_model * view * projection */
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

float4 main(VSInput input) : SV_Position
{
    return mul(mvp, float4(input.position, 1.0));
}
