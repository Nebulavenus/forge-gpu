/*
 * scene_model_instanced_shadow.vert.hlsl — Instanced model shadow vertex shader
 *
 * Part of forge_scene.h — depth-only shadow map rendering for instanced
 * pipeline-processed models.  Reconstructs the per-instance model matrix
 * and transforms vertices to light clip space.
 *
 * Used for both opaque shadow pass (paired with shadow.frag.hlsl) and
 * alpha-masked shadow pass (paired with shadow_mask.frag.hlsl — needs UV).
 *
 * Per-vertex attributes (slot 0, VERTEX input rate, 48-byte stride):
 *   TEXCOORD0 -> float3 position  (location 0, offset  0)
 *   TEXCOORD1 -> float2 uv        (location 1, offset 24)
 *
 * Per-instance attributes (slot 1, INSTANCE input rate, 64-byte stride):
 *   TEXCOORD2 -> float4 model_c0  (location 2)
 *   TEXCOORD3 -> float4 model_c1  (location 3)
 *   TEXCOORD4 -> float4 model_c2  (location 4)
 *   TEXCOORD5 -> float4 model_c3  (location 5)
 *
 * Uniform buffer:
 *   register(b0, space1) -> light VP + node world matrices (128 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer ShadowUniforms : register(b0, space1)
{
    column_major float4x4 light_vp;   /* light view-projection matrix      */
    column_major float4x4 node_world; /* per-node local-to-model transform */
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;

    /* Per-instance model matrix columns */
    float4 model_c0 : TEXCOORD2;
    float4 model_c1 : TEXCOORD3;
    float4 model_c2 : TEXCOORD4;
    float4 model_c3 : TEXCOORD5;
};

struct VSOutput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;   /* needed by shadow_mask.frag.hlsl */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Reconstruct model matrix from per-instance columns. */
    float4x4 model = transpose(float4x4(
        input.model_c0,
        input.model_c1,
        input.model_c2,
        input.model_c3
    ));

    /* Transform: instance_model * node_world * local_pos → light clip. */
    float4 local = mul(node_world, float4(input.position, 1.0));
    float4 world = mul(model, local);
    output.clip_pos = mul(light_vp, world);
    output.uv       = input.uv;

    return output;
}
