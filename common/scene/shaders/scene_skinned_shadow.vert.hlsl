/*
 * scene_skinned_shadow.vert.hlsl — Skinned shadow pass vertex shader
 *
 * Part of forge_scene.h — depth-only shadow map rendering for skinned models.
 * Applies the skinning equation to position only (no normal/tangent needed
 * for depth), then projects to light clip space.
 *
 * Reads position, joints, and weights from the 72-byte skinned vertex
 * layout.  All other attributes are unused but present in the buffer.
 *
 * Per-vertex attributes (72-byte stride):
 *   TEXCOORD0 -> float3  position  (offset  0)
 *   TEXCOORD4 -> uint4   joints    (offset 48)
 *   TEXCOORD5 -> float4  weights   (offset 56)
 *
 * Uniform buffer:
 *   register(b0, space1) -> light view-projection * model matrix
 *
 * Storage buffer:
 *   register(t0, space0) -> joint matrices (MAX_JOINTS x float4x4)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer ShadowUniforms : register(b0, space1) {
    column_major float4x4 light_vp;  /* light view-projection * model */
};

/* In DXIL, vertex storage buffers use register(t[n], space0). */
StructuredBuffer<float4x4> joint_mats : register(t0, space0);

struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;   /* unused but present in vertex layout */
    float2 uv       : TEXCOORD2;   /* unused but present in vertex layout */
    float4 tangent  : TEXCOORD3;   /* unused but present in vertex layout */
    uint4  joints   : TEXCOORD4;
    float4 weights  : TEXCOORD5;
};

float4 main(VSInput input) : SV_Position {
    /* Skin the position */
    float4x4 skin_mat =
        input.weights.x * joint_mats[input.joints.x] +
        input.weights.y * joint_mats[input.joints.y] +
        input.weights.z * joint_mats[input.joints.z] +
        input.weights.w * joint_mats[input.joints.w];

    float4 skinned_pos = mul(skin_mat, float4(input.position, 1.0));

    return mul(light_vp, skinned_pos);
}
