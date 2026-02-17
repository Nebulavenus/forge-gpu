/*
 * shuttle.vert.hlsl — Vertex shader for lit shuttle with environment mapping
 *
 * Same structure as Lesson 10's lighting vertex shader: transforms vertices
 * to both clip space (for rasterization) and world space (for lighting and
 * environment reflection in the fragment shader).
 *
 * Normals use the adjugate transpose method for correct transformation
 * under non-uniform scale.  See Lesson 10's README for the derivation.
 *
 * Vertex attributes (matching ForgeObjVertex layout):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffer (128 bytes):
 *   mvp   (64 bytes) — Model-View-Projection matrix
 *   model (64 bytes) — Model (world) matrix
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;      /* Model-View-Projection (64 bytes) */
    column_major float4x4 model;    /* Model (world) matrix (64 bytes)   */
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
     * and for computing the reflection vector. */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* World-space normal via adjugate transpose of the 3x3 model matrix.
     * Preserves perpendicularity under non-uniform scale without needing
     * a matrix inverse.  See Lesson 10 for the full explanation. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_norm = mul(adj_t, input.normal);

    /* Pass UVs through for diffuse texture sampling. */
    output.uv = input.uv;

    return output;
}
