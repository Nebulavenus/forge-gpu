/*
 * Ghost (X-ray) vertex shader — outputs position and view-space normal.
 *
 * The view-space normal is used in the fragment shader to compute a
 * Fresnel rim-lighting effect that reveals occluded object silhouettes
 * with a translucent glow.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: MVP + model-view matrices (128 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GhostVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;        /* model-view-projection matrix        */
    column_major float4x4 model_view; /* model-view matrix for eye-space nrm */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* vertex attribute location 1 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer  */
    float3 normal_eye : TEXCOORD0;   /* view-space normal for Fresnel       */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* Transform normal to view (eye) space for Fresnel computation. */
    output.normal_eye = normalize(mul((float3x3)model_view, input.normal));

    return output;
}
