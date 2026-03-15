/*
 * scene_skinned.vert.hlsl — Skinned model vertex shader (72-byte stride)
 *
 * Part of forge_scene.h — transforms skinned pipeline-processed vertices
 * that include joint indices, blend weights, and tangent data for normal
 * mapping.  Joint matrices are read from a structured storage buffer.
 *
 * The skinning equation blends up to 4 joint transforms per vertex:
 *   skin_mat = w0*J[j0] + w1*J[j1] + w2*J[j2] + w3*J[j3]
 *
 * After skinning, the vertex follows the same world-space transform,
 * TBN construction, and shadow projection as scene_model.vert.hlsl.
 *
 * Per-vertex attributes (72-byte stride):
 *   TEXCOORD0 -> float3  position  (12 bytes, offset  0)
 *   TEXCOORD1 -> float3  normal    (12 bytes, offset 12)
 *   TEXCOORD2 -> float2  uv        ( 8 bytes, offset 24)
 *   TEXCOORD3 -> float4  tangent   (16 bytes, offset 32)
 *   TEXCOORD4 -> uint4   joints    ( 8 bytes, offset 48, USHORT4)
 *   TEXCOORD5 -> float4  weights   (16 bytes, offset 56)
 *
 * Uniform buffer:
 *   register(b0, space1) -> MVP, model, and light VP matrices (192 bytes)
 *
 * Storage buffer:
 *   register(t0, space0) -> joint matrices (MAX_JOINTS x float4x4)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1) {
    column_major float4x4 mvp;
    column_major float4x4 model;
    column_major float4x4 light_vp;
};

/* In DXIL, vertex storage buffers use register(t[n], space0). */
StructuredBuffer<float4x4> joint_mats : register(t0, space0);

struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3;   /* xyz = tangent direction, w = handedness */
    uint4  joints   : TEXCOORD4;   /* 4 joint indices */
    float4 weights  : TEXCOORD5;   /* 4 blend weights */
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

    /* ── Skinning: blend 4 joint transforms ────────────────────────── */
    float4x4 skin_mat =
        input.weights.x * joint_mats[input.joints.x] +
        input.weights.y * joint_mats[input.joints.y] +
        input.weights.z * joint_mats[input.joints.z] +
        input.weights.w * joint_mats[input.joints.w];

    float4 skinned_pos = mul(skin_mat, float4(input.position, 1.0));
    float3 skinned_nrm = normalize(mul((float3x3)skin_mat, input.normal));
    float3 skinned_tan = normalize(mul((float3x3)skin_mat, input.tangent.xyz));

    /* ── Standard transform (same as scene_model.vert) ─────────────── */

    /* Clip-space position */
    output.clip_pos = mul(mvp, skinned_pos);

    /* World-space position for lighting */
    float4 wp = mul(model, skinned_pos);
    output.world_pos = wp.xyz;

    /* Light-space position for shadow mapping */
    output.shadow_pos = mul(light_vp, skinned_pos);

    /* Normal transformation via adjugate transpose */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    float3 N = normalize(mul(adj_t, skinned_nrm));

    /* Tangent follows geometry — transform by model */
    float3 T = normalize(mul(m, skinned_tan));

    /* Gram-Schmidt re-orthogonalization */
    T = normalize(T - N * dot(N, T));

    /* Bitangent from cross product with handedness sign */
    float3 B = cross(N, T) * input.tangent.w;

    output.world_normal  = N;
    output.world_tangent = T;
    output.world_bitan   = B;
    output.uv            = input.uv;

    return output;
}
