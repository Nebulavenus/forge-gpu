/*
 * grid.vert.hlsl — Vertex shader for the procedural floor grid
 *
 * Transforms a large world-space quad from object coordinates to clip space.
 * Passes world-space position through to the fragment shader, where it is
 * used to compute grid line coordinates procedurally — no texture needed.
 *
 * The grid quad is a flat rectangle on the XZ plane (Y = 0), large enough
 * to fill the visible ground around the scene.  The fragment shader does
 * all the visual work (anti-aliased lines, distance fade, lighting).
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0, only attribute)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains the combined View-Projection matrix (64 bytes).
 *   No model matrix needed — the grid is already in world space.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;   /* combined view-projection matrix */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
};

struct VSOutput
{
    float4 clip_pos  : SV_Position; /* clip-space position for rasterizer */
    float3 world_pos : TEXCOORD0;   /* world-space position for grid math */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Grid vertices are already in world space, so we only need VP
     * (no model matrix).  The fragment shader uses world_pos directly. */
    output.clip_pos  = mul(vp, float4(input.position, 1.0));
    output.world_pos = input.position;

    return output;
}
