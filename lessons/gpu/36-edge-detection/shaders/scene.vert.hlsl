/*
 * Scene vertex shader — transforms vertices to clip, world, light, and view space.
 *
 * Outputs to four interpolants for Blinn-Phong lighting, shadow mapping,
 * and view-space normals written to the G-buffer normal target.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: scene transforms (256 bytes, 4 mat4s)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer SceneVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;        /* model-view-projection matrix           */
    column_major float4x4 model;      /* model (world) matrix                   */
    column_major float4x4 light_vp;   /* light VP * model for shadow coords     */
    column_major float4x4 model_view; /* model-view matrix for view-space norms */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* vertex attribute location 1 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer      */
    float3 world_pos  : TEXCOORD0;   /* world-space position for lighting       */
    float3 world_nrm  : TEXCOORD1;   /* world-space normal for lighting         */
    float4 light_clip : TEXCOORD2;   /* light-space position for shadow map     */
    float3 view_nrm   : TEXCOORD3;   /* view-space normal for G-buffer output   */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Transform to world space using the model matrix. */
    float4 world = mul(model, float4(input.position, 1.0));
    output.world_pos = world.xyz;

    /* Project to clip space. */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* Transform normal to world space (upper 3x3 of model matrix). */
    output.world_nrm = normalize(mul((float3x3)model, input.normal));

    /* Light-space position for shadow mapping. */
    output.light_clip = mul(light_vp, float4(input.position, 1.0));

    /* View-space normal for the G-buffer normal render target. */
    output.view_nrm = normalize(mul((float3x3)model_view, input.normal));

    return output;
}
