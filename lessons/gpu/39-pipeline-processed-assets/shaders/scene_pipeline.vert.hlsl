/*
 * scene_vert_pipeline.hlsl — Pipeline path vertex shader (48-byte stride)
 *
 * Transforms pipeline-processed vertices that include tangent data for
 * normal mapping.  Constructs a world-space TBN basis using the adjugate
 * transpose for normals (preserves perpendicularity under non-uniform
 * scale) and Gram-Schmidt re-orthogonalization for the tangent.
 *
 * Per-vertex attributes (48-byte stride):
 *   TEXCOORD0 -> float3 position   (12 bytes, offset  0)
 *   TEXCOORD1 -> float3 normal     (12 bytes, offset 12)
 *   TEXCOORD2 -> float2 uv         ( 8 bytes, offset 24)
 *   TEXCOORD3 -> float4 tangent    (16 bytes, offset 32)
 *
 * Uniform buffer:
 *   register(b0, space1) -> MVP, model, and light VP matrices (192 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1) {
    column_major float4x4 mvp;
    column_major float4x4 model;
    column_major float4x4 light_vp;
};

struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3;   /* xyz = tangent direction, w = handedness */
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

    /* Clip-space position for the rasterizer. */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space position for lighting and shadow calculations. */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* Light-space position for shadow mapping. */
    output.shadow_pos = mul(light_vp, wp);

    /* ── Normal transformation (adjugate transpose) ──────────────────
     * Normals are perpendicular to the surface, so they transform by
     * the adjugate transpose of the upper-left 3x3, not the model
     * matrix itself.  The cross product method computes this directly
     * without an explicit inverse.  See L10 for the derivation. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    float3 N = normalize(mul(adj_t, input.normal));

    /* ── Tangent transformation ──────────────────────────────────────
     * Tangent vectors are surface directions — they follow geometry and
     * transform by the model matrix directly, unlike normals. */
    float3 T = normalize(mul(m, input.tangent.xyz));

    /* ── Gram-Schmidt re-orthogonalization ────────────────────────────
     * After applying different transformations (adjugate for N, direct
     * for T), the vectors may no longer be exactly perpendicular.
     * Project out the N component from T to restore orthogonality. */
    T = normalize(T - N * dot(N, T));

    /* ── Bitangent from cross product ────────────────────────────────
     * The bitangent completes the tangent-space basis.  tangent.w
     * stores the handedness (+1 or -1), encoding whether the UV space
     * is mirrored.  Without this sign, mirrored UVs produce inverted
     * normal maps. */
    float3 B = cross(N, T) * input.tangent.w;

    output.world_normal  = N;
    output.world_tangent = T;
    output.world_bitan   = B;
    output.uv            = input.uv;

    return output;
}
