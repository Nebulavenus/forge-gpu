/*
 * scene.vert.hlsl — Vertex shader for glTF meshes with Blinn-Phong lighting
 *
 * Transforms vertices to clip space AND world space.  The fragment shader
 * needs world-space positions and normals to compute lighting — clip space
 * alone isn't enough because lighting is defined in world coordinates
 * (where the light direction and camera position live).
 *
 * Normal transformation — adjugate transpose:
 *   Normals don't transform the same way as positions.  The correct matrix
 *   is the adjugate transpose of the upper-left 3x3 of the model matrix.
 *   This preserves perpendicularity even under non-uniform scale.
 *   See Lesson 10 for the full derivation.
 *
 * Vertex attributes (mapped from SDL GPU vertex attribute locations):
 *   TEXCOORD0 -> position (float3)  — location 0
 *   TEXCOORD1 -> normal   (float3)  — location 1
 *   TEXCOORD2 -> uv       (float2)  — location 2
 *
 * Vertex uniform:
 *   register(b0, space1) -> MVP + model matrices (128 bytes)
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
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
};

struct VSOutput
{
    float4 clip_pos   : SV_Position;
    float2 uv         : TEXCOORD0;
    float3 world_norm : TEXCOORD1;
    float3 world_pos  : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Clip-space position for the rasterizer. */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space position — needed for the view direction (V = eye - P). */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* World-space normal — transform by the adjugate transpose of the
     * model matrix's upper-left 3x3.  Unlike plain (float3x3)model, this
     * preserves perpendicularity even under non-uniform scale.
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
