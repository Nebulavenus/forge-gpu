/*
 * scene_instanced_shadow.vert.hlsl — Instanced shadow pass vertex shader
 *
 * Part of forge_scene.h — depth-only shadow map rendering for instanced meshes.
 * Reconstructs the per-instance model matrix and transforms vertices to light
 * clip space.  Paired with shadow.frag.hlsl (empty fragment, depth-only).
 *
 * Per-vertex attributes (slot 0, VERTEX input rate, 24-byte stride):
 *   TEXCOORD0 -> float3 position  (location 0)
 *
 * Per-instance attributes (slot 1, INSTANCE input rate, 64-byte stride):
 *   TEXCOORD1 -> float4 model_c0  (location 1) — model matrix column 0
 *   TEXCOORD2 -> float4 model_c1  (location 2) — model matrix column 1
 *   TEXCOORD3 -> float4 model_c2  (location 3) — model matrix column 2
 *   TEXCOORD4 -> float4 model_c3  (location 4) — model matrix column 3
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

    /* Per-instance model matrix columns */
    float4 model_c0 : TEXCOORD1;
    float4 model_c1 : TEXCOORD2;
    float4 model_c2 : TEXCOORD3;
    float4 model_c3 : TEXCOORD4;
};

float4 main(VSInput input) : SV_Position
{
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
    return mul(light_vp, world);
}
