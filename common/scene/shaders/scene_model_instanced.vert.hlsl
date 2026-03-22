/*
 * scene_model_instanced.vert.hlsl — Instanced model vertex shader (48-byte + instance)
 *
 * Part of forge_scene.h — transforms pipeline-processed vertices with tangent
 * data for normal mapping, using a per-instance model matrix.  Constructs a
 * world-space TBN basis using adjugate transpose for normals and Gram-Schmidt
 * re-orthogonalization.
 *
 * Must produce the same VSOutput layout as scene_model.vert.hlsl so that
 * scene_model.frag.hlsl can be reused without modification.
 *
 * Per-vertex attributes (slot 0, VERTEX input rate, 48-byte stride):
 *   TEXCOORD0 -> float3 position  (location 0, offset  0)
 *   TEXCOORD1 -> float3 normal    (location 1, offset 12)
 *   TEXCOORD2 -> float2 uv        (location 2, offset 24)
 *   TEXCOORD3 -> float4 tangent   (location 3, offset 32)
 *
 * Per-instance attributes (slot 1, INSTANCE input rate, 64-byte stride):
 *   TEXCOORD4 -> float4 model_c0  (location 4) — model matrix column 0
 *   TEXCOORD5 -> float4 model_c1  (location 5) — model matrix column 1
 *   TEXCOORD6 -> float4 model_c2  (location 6) — model matrix column 2
 *   TEXCOORD7 -> float4 model_c3  (location 7) — model matrix column 3
 *
 * Uniform buffer:
 *   register(b0, space1) -> VP + light VP + node world matrices (192 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer InstancedVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;         /* camera view-projection matrix     */
    column_major float4x4 light_vp;   /* light view-projection matrix      */
    column_major float4x4 node_world; /* per-node local-to-model transform */
};

struct VSInput
{
    /* Per-vertex data (slot 0, 48-byte stride) */
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3;   /* xyz = tangent direction, w = handedness */

    /* Per-instance data (slot 1, 64-byte stride) */
    float4 model_c0 : TEXCOORD4;
    float4 model_c1 : TEXCOORD5;
    float4 model_c2 : TEXCOORD6;
    float4 model_c3 : TEXCOORD7;
};

struct VSOutput
{
    float4 clip_pos      : SV_Position;
    float3 world_pos     : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float2 uv            : TEXCOORD2;
    float3 world_tangent : TEXCOORD3;
    float3 world_bitan   : TEXCOORD4;
    float4 shadow_pos    : TEXCOORD5;
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

    /* World-space position: instance_model * node_world * local_pos.
     * node_world applies the glTF node hierarchy transform. */
    float4 local = mul(node_world, float4(input.position, 1.0));
    float4 wp = mul(model, local);
    output.world_pos = wp.xyz;
    output.clip_pos  = mul(vp, wp);

    /* Light-space position for shadow mapping. */
    output.shadow_pos = mul(light_vp, wp);

    /* Normal transformation via adjugate transpose of combined matrix —
     * preserves perpendicularity under non-uniform scale. */
    float3x3 combined = mul((float3x3)model, (float3x3)node_world);
    float3x3 adj_t;
    adj_t[0] = cross(combined[1], combined[2]);
    adj_t[1] = cross(combined[2], combined[0]);
    adj_t[2] = cross(combined[0], combined[1]);
    float3 N = normalize(mul(adj_t, input.normal));

    /* Tangent vectors follow geometry — transform by combined matrix. */
    float3 T = normalize(mul(combined, input.tangent.xyz));

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
