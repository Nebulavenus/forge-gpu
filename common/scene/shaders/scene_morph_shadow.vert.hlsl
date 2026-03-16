/*
 * scene_morph_shadow.vert.hlsl — Morph target shadow pass vertex shader
 *
 * Part of forge_scene.h — depth-only shadow map rendering for morphed models.
 * Applies position deltas from a storage buffer, then projects to light clip
 * space.  Normal and tangent deltas are not needed for the depth-only pass.
 *
 * Per-vertex attributes (48-byte stride, same as scene_model):
 *   TEXCOORD0 -> float3 position  (offset  0)
 *
 * Uniform buffer:
 *   register(b0, space1) -> light view-projection * model matrix
 *
 * Storage buffer:
 *   register(t0, space0) -> blended position deltas (vertex_count x float4, 16-byte stride)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer ShadowUniforms : register(b0, space1) {
    column_major float4x4 light_vp;  /* light view-projection * model */
};

/* CPU-blended morph position deltas uploaded each frame.
 * float4 (not float3) for consistent 16-byte stride across SPIRV and DXIL. */
StructuredBuffer<float4> morph_pos_deltas : register(t0, space0);

struct VSInput {
    uint   vertex_id : SV_VertexID;
    float3 position  : TEXCOORD0;
};

float4 main(VSInput input) : SV_Position {
    /* Apply morph position displacement */
    float3 morphed_pos = input.position + morph_pos_deltas[input.vertex_id].xyz;

    return mul(light_vp, float4(morphed_pos, 1.0));
}
