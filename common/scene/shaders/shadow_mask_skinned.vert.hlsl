/*
 * shadow_mask_skinned.vert.hlsl — Alpha-masked shadow pass for skinned models
 *
 * Part of forge_scene.h — applies the skinning equation to position,
 * passes through UVs for alpha-test discard in the fragment shader.
 *
 * Per-vertex attributes (72-byte stride):
 *   TEXCOORD0 -> float3  position  (offset  0)
 *   TEXCOORD2 -> float2  uv        (offset 24)
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

StructuredBuffer<float4x4> joint_mats : register(t0, space0);

struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;   /* unused but present in vertex layout */
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3;   /* unused but present in vertex layout */
    uint4  joints   : TEXCOORD4;
    float4 weights  : TEXCOORD5;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input) {
    /* Skin the position */
    float4x4 skin_mat =
        input.weights.x * joint_mats[input.joints.x] +
        input.weights.y * joint_mats[input.joints.y] +
        input.weights.z * joint_mats[input.joints.z] +
        input.weights.w * joint_mats[input.joints.w];

    float4 skinned_pos = mul(skin_mat, float4(input.position, 1.0));

    VSOutput output;
    output.position = mul(light_vp, skinned_pos);
    output.uv       = input.uv;
    return output;
}
