/*
 * material.vert.hlsl — Vertex shader for per-material Blinn-Phong
 *
 * Same as Lesson 10's lighting vertex shader: transforms vertices to
 * both clip space (for rasterization) and world space (for lighting).
 * The fragment shader needs world-space positions and normals because
 * lighting calculations are defined in world coordinates.
 *
 * Normal transformation uses the adjugate transpose of the model
 * matrix's upper-left 3x3, computed via cross products.  This method
 * preserves perpendicularity under non-uniform scale and works for
 * all matrices — including singular ones.  See Lesson 10 for the
 * full explanation.
 *
 * Vertex attributes (matching ForgeGltfVertex layout):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex uniform slot 0
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

    /* Clip-space position for the rasterizer. */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space position — needed for the view direction (V = eye - P). */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* World-space normal — transform by the adjugate transpose.
     * The cross-product formulation handles non-uniform scale correctly
     * without needing the matrix inverse. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_norm = mul(adj_t, input.normal);

    /* Pass UVs through for optional texture sampling. */
    output.uv = input.uv;

    return output;
}
