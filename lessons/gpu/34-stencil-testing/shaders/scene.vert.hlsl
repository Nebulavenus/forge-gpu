/*
 * Scene vertex shader — transforms vertices to clip, world, and light space.
 *
 * Uses traditional vertex input (vertex attributes via the input assembler)
 * rather than vertex pulling.  The pipeline declares two attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *
 * Stencil behavior is controlled entirely by pipeline state — this shader
 * is unaware of stencil operations.
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

    /* Transform normal to world space (upper 3x3 of model matrix).
     * Correct for rigid transforms; non-uniform scale would need
     * the inverse transpose. */
    output.world_nrm = normalize(mul((float3x3)model, input.normal));

    /* Light-space position for shadow mapping.
     * light_vp already contains (lightVP * model), so multiply by the
     * model-space position — not world — to avoid applying model twice. */
    output.light_clip = mul(light_vp, float4(input.position, 1.0));

    return output;
}
