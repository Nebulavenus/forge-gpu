/*
 * Scene textured vertex shader — position + normal + UV.
 *
 * Part of forge_scene.h — extends the base scene pipeline with texture
 * coordinate support for textured meshes and atlas rendering.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: scene transforms (192 bytes, 3 mat4s)
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
    float2 uv       : TEXCOORD2;   /* vertex attribute location 2 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer  */
    float3 world_pos  : TEXCOORD0;   /* world-space position for lighting   */
    float3 world_nrm  : TEXCOORD1;   /* world-space normal for lighting     */
    float4 light_clip : TEXCOORD2;   /* light-space position for shadow map */
    float2 uv         : TEXCOORD3;   /* texture coordinates (passed through) */
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

    /* Pass UVs through for fragment shader texture sampling. */
    output.uv = input.uv;

    return output;
}
