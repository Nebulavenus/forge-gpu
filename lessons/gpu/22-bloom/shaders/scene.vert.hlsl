/*
 * scene.vert.hlsl — Vertex shader for lit scene (point light, no shadows)
 *
 * Transforms vertices to clip space for rasterization and to world space
 * for lighting.  Simplified from Lesson 21 by removing cascade shadow
 * map outputs — this lesson uses bloom instead of shadows.
 *
 * Normal transformation uses the adjugate transpose of the model matrix's
 * upper-left 3x3, computed via cross products.  This preserves
 * perpendicularity under non-uniform scale without needing the matrix
 * inverse.  See Lesson 10 for the full derivation.
 *
 * Vertex attributes (matching ForgeGltfVertex layout):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: MVP + model matrix (128 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer SceneVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;     /* model-view-projection matrix */
    column_major float4x4 model;   /* model (world) matrix         */
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

    /* World-space position — needed for the view direction (V = eye - P)
     * and for point light distance attenuation. */
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
