/*
 * lighting.vert.hlsl — Vertex shader for Blinn-Phong lighting
 *
 * Transforms vertices from object space to clip space AND to world space.
 * The fragment shader needs world-space positions and normals to compute
 * lighting — clip space alone isn't enough because lighting is defined
 * in world coordinates (where the light direction and camera position live).
 *
 * Two matrices:
 *   mvp   — combined Model-View-Projection for clip-space output
 *   model — the model (world) matrix for world-space position and normal
 *
 * Normal transformation — adjugate transpose:
 *   Normals are directions perpendicular to a surface, and they don't
 *   transform the same way as positions.  The correct matrix for normals
 *   is the ADJUGATE TRANSPOSE of the upper-left 3x3 of the model matrix.
 *   This equals (M^-1)^T * det(M) for invertible matrices, but unlike the
 *   commonly-taught inverse-transpose, it works for ALL matrices (including
 *   singular ones) and avoids dividing by the determinant — which the
 *   fragment shader's normalize() cancels out anyway.
 *
 *   The adjugate transpose has a beautiful cross-product formulation:
 *     row 0 = cross(model_row1, model_row2)
 *     row 1 = cross(model_row2, model_row0)
 *     row 2 = cross(model_row0, model_row1)
 *
 *   See the lesson README for the full derivation.
 *
 * Vertex attributes (matching ForgeGltfVertex layout):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains mvp (64 bytes) + model (64 bytes) = 128 bytes.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;
    column_major float4x4 model;
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* vertex attribute location 1 */
    float2 uv       : TEXCOORD2;   /* vertex attribute location 2 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer */
    float2 uv         : TEXCOORD0;   /* texture coordinates               */
    float3 world_norm : TEXCOORD1;   /* world-space normal (not normalized)*/
    float3 world_pos  : TEXCOORD2;   /* world-space position               */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Clip-space position for the rasterizer (same as previous lessons). */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space position — needed for the view direction (V = eye - P). */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* World-space normal — transform by the ADJUGATE TRANSPOSE of the
     * model matrix's upper-left 3x3.  Unlike plain (float3x3)model, this
     * preserves perpendicularity even under non-uniform scale.
     *
     * The adjugate transpose rows are cross products of the matrix rows:
     *   adj_t row 0 = cross(row 1, row 2)
     *   adj_t row 1 = cross(row 2, row 0)
     *   adj_t row 2 = cross(row 0, row 1)
     *
     * NOT normalized here — the rasterizer will interpolate across the
     * triangle, producing non-unit vectors anyway.  The fragment shader
     * normalizes per-pixel. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_norm = mul(adj_t, input.normal);

    /* Pass UVs through for texture sampling. */
    output.uv = input.uv;

    return output;
}
