/*
 * Skinned vertex shader — applies skeletal animation before MVP transform.
 *
 * Each vertex has 4 joint indices and 4 blend weights.  The skin matrix
 * is a weighted sum of the joint matrices, which transforms the vertex
 * from bind-pose model space to animated world space.
 *
 * The joint matrix for joint i is:
 *   jointMatrix[i] = worldTransform[joints[i]] * inverseBindMatrix[i]
 *
 * This is computed on the CPU and passed as a uniform array.
 *
 * Outputs world-space position, normal, UV, and light-space clip position
 * for shadow mapping.
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: scene uniforms (MVP, model, light_vp)
 *   register(b1, space1) -> slot 1: joint matrices (MAX_JOINTS x float4x4)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Maximum joints must match MAX_JOINTS in main.c. */
#define MAX_JOINTS 19

cbuffer SceneUniforms : register(b0, space1)
{
    column_major float4x4 mvp;      /* model-view-projection matrix */
    column_major float4x4 model;    /* model (world) matrix — used for normals */
    column_major float4x4 light_vp; /* light view-projection for shadows */
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
    uint4  joints  : TEXCOORD3;   /* USHORT4 auto-expanded to uint4 by GPU */
    float4 weights : TEXCOORD4;
};

struct VSOutput
{
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 world_nrm  : TEXCOORD1;
    float2 uv         : TEXCOORD2;
    float4 light_clip : TEXCOORD3;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Compute the skin matrix — weighted sum of up to 4 joint transforms.
     * Each joint matrix transforms from bind-pose model space to current
     * world space (jointMatrix = worldTransform * inverseBindMatrix). */
    float4x4 skin_mat = input.weights.x * joint_mats[input.joints.x]
                       + input.weights.y * joint_mats[input.joints.y]
                       + input.weights.z * joint_mats[input.joints.z]
                       + input.weights.w * joint_mats[input.joints.w];

    /* Apply skin matrix to get mesh-local position, then model matrix
     * to get true world-space position for lighting. */
    float4 skinned = mul(skin_mat, float4(input.pos, 1.0));
    float4 world   = mul(model, skinned);
    output.world_pos = world.xyz;

    /* Transform skinned position to clip space. */
    output.clip_pos = mul(mvp, skinned);

    /* Skin the normal (bind-pose → mesh-local), then transform to world
     * space with the model matrix.  Without the model multiply the normal
     * stays in mesh-local space, so lighting rotates with the character. */
    float3 local_nrm = mul((float3x3)skin_mat, input.normal);
    output.world_nrm = normalize(mul((float3x3)model, local_nrm));

    output.uv = input.uv;

    /* Light-space position for shadow mapping.
     * light_vp already includes mesh_world, so use mesh-local skinned pos. */
    output.light_clip = mul(light_vp, skinned);

    return output;
}
