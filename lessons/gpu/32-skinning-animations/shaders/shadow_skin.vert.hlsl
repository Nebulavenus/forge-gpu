/*
 * Skinned shadow vertex shader — applies skeletal animation then light MVP.
 *
 * Renders the skinned mesh from the directional light's point of view.
 * The resulting depth buffer is sampled in the scene pass for shadow testing.
 *
 * Uses the same joint matrix uniform array as the main skin vertex shader.
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: light MVP matrix
 *   register(b1, space1) -> slot 1: joint matrices (MAX_JOINTS x float4x4)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Maximum joints must match MAX_JOINTS in main.c. */
#define MAX_JOINTS 19

cbuffer ShadowUniforms : register(b0, space1)
{
    column_major float4x4 light_vp; /* light view-projection matrix */
};

cbuffer JointUniforms : register(b1, space1)
{
    column_major float4x4 joint_mats[MAX_JOINTS];
};

struct VSInput
{
    float3 pos     : TEXCOORD0;
    float3 normal  : TEXCOORD1;
    float2 uv      : TEXCOORD2;
    uint4  joints  : TEXCOORD3;
    float4 weights : TEXCOORD4;
};

float4 main(VSInput input) : SV_Position
{
    /* Compute the skin matrix — same as the main vertex shader. */
    float4x4 skin_mat = input.weights.x * joint_mats[input.joints.x]
                       + input.weights.y * joint_mats[input.joints.y]
                       + input.weights.z * joint_mats[input.joints.z]
                       + input.weights.w * joint_mats[input.joints.w];

    /* Skin the vertex to world space, then project into light clip space. */
    float4 world = mul(skin_mat, float4(input.pos, 1.0));
    return mul(light_vp, world);
}
