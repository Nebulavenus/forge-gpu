/*
 * scene_morph.vert.hlsl — Morph target model vertex shader (48-byte stride)
 *
 * Part of forge_scene.h — transforms pipeline-processed vertices and applies
 * morph target displacements from storage buffers.  The CPU blends morph
 * target deltas each frame and uploads a single set of position and normal
 * offsets.  The vertex shader adds these offsets to the base mesh attributes
 * before the standard world-space transform.
 *
 * Per-vertex attributes (48-byte stride, same as scene_model):
 *   TEXCOORD0 -> float3 position   (12 bytes, offset  0)
 *   TEXCOORD1 -> float3 normal     (12 bytes, offset 12)
 *   TEXCOORD2 -> float2 uv         ( 8 bytes, offset 24)
 *   TEXCOORD3 -> float4 tangent    (16 bytes, offset 32)
 *
 * Uniform buffer:
 *   register(b0, space1) -> MVP, model, and light VP matrices (192 bytes)
 *
 * Storage buffers:
 *   register(t0, space0) -> blended position deltas (vertex_count x float4, 16-byte stride)
 *   register(t1, space0) -> blended normal deltas   (vertex_count x float4, 16-byte stride)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1) {
    column_major float4x4 mvp;
    column_major float4x4 model;
    column_major float4x4 light_vp;
};

/* CPU-blended morph deltas uploaded each frame.
 * float4 (not float3) because SPIRV ArrayStride is 16 for vec3 but DXIL
 * StructuredBuffer<float3> uses stride 12 — float4 gives 16-byte stride
 * on both backends, matching the CPU upload layout. */
StructuredBuffer<float4> morph_pos_deltas : register(t0, space0);
StructuredBuffer<float4> morph_nrm_deltas : register(t1, space0);

struct VSInput {
    uint   vertex_id : SV_VertexID;
    float3 position  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;
    float4 tangent   : TEXCOORD3;   /* xyz = tangent direction, w = handedness */
};

struct VSOutput {
    float4 clip_pos      : SV_Position;
    float3 world_pos     : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float2 uv            : TEXCOORD2;
    float3 world_tangent : TEXCOORD3;
    float3 world_bitan   : TEXCOORD4;
    float4 shadow_pos    : TEXCOORD5;
};

VSOutput main(VSInput input) {
    VSOutput output;

    /* ── Apply morph displacements ───────────────────────────────── */
    float3 morphed_pos = input.position + morph_pos_deltas[input.vertex_id].xyz;
    float3 morphed_nrm = normalize(input.normal + morph_nrm_deltas[input.vertex_id].xyz);

    /* ── Standard transform (same as scene_model.vert) ───────────── */

    /* Clip-space position for the rasterizer. */
    output.clip_pos = mul(mvp, float4(morphed_pos, 1.0));

    /* World-space position for lighting and shadow calculations. */
    float4 wp = mul(model, float4(morphed_pos, 1.0));
    output.world_pos = wp.xyz;

    /* Light-space position for shadow mapping. */
    output.shadow_pos = mul(light_vp, float4(morphed_pos, 1.0));

    /* Normal transformation via adjugate transpose — preserves
     * perpendicularity under non-uniform scale. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    float3 N = normalize(mul(adj_t, morphed_nrm));

    /* Tangent vectors follow geometry — transform by model directly.
     * Tangent deltas are not applied (morph targets rarely include them
     * and the visual difference is negligible for most use cases). */
    float3 T = normalize(mul(m, input.tangent.xyz));

    /* Gram-Schmidt re-orthogonalization: project out the N component. */
    T = normalize(T - N * dot(N, T));

    /* Bitangent from cross product with handedness sign. */
    float3 B = cross(N, T) * input.tangent.w;

    output.world_normal  = N;
    output.world_tangent = T;
    output.world_bitan   = B;
    output.uv            = input.uv;

    return output;
}
