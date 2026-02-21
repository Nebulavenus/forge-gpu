/*
 * shadow.vert.hlsl — Depth-only vertex shader for shadow map pass
 *
 * Transforms vertices into the light's clip space using a single
 * light-space MVP matrix.  No fragment outputs are needed — the GPU
 * writes depth automatically to the depth-only render target.
 *
 * This shader is used once per cascade, with a different light_mvp
 * matrix for each cascade's orthographic projection.
 *
 * Vertex attributes (ForgeGltfVertex layout — only position is used):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1, unused but must be declared)
 *   TEXCOORD2 -> float2 uv        (location 2, unused but must be declared)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains the light's View-Projection * Model matrix (64 bytes).
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer ShadowVertUniforms : register(b0, space1)
{
    column_major float4x4 light_mvp;  /* light VP * model matrix */
};

struct VSInput
{
    float3 position : TEXCOORD0;  /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;  /* location 1 — unused, must match pipeline */
    float2 uv       : TEXCOORD2;  /* location 2 — unused, must match pipeline */
};

struct VSOutput
{
    float4 clip_pos : SV_Position;  /* light-space clip position */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Transform vertex position directly to light clip space.
     * The depth value written to the shadow map comes from clip_pos.z/clip_pos.w
     * after the GPU performs the perspective divide (which is trivial for
     * orthographic projection since w = 1). */
    output.clip_pos = mul(light_mvp, float4(input.position, 1.0));

    return output;
}
