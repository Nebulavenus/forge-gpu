/*
 * Scene vertex shader — transforms vertices to clip, world, and light space.
 *
 * Uses traditional vertex input with three attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * UV coordinates are passed through but unused by the fragment shader
 * (Suzanne models use uniform base_color, not textures).  The attribute
 * must be declared to match the ForgeGltfVertex stride (32 bytes).
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: scene transforms (MVP, model, light_vp)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer SceneVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;      /* model-view-projection matrix       */
    column_major float4x4 model;    /* model (world) matrix               */
    column_major float4x4 light_vp; /* light VP * model for shadow coords */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* vertex attribute location 1 */
    float2 uv       : TEXCOORD2;   /* vertex attribute location 2 (unused) */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer  */
    float3 world_pos  : TEXCOORD0;   /* world-space position for lighting   */
    float3 world_nrm  : TEXCOORD1;   /* world-space normal for lighting     */
    float4 light_clip : TEXCOORD2;   /* light-space position for shadow map */
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

    return output;
}
