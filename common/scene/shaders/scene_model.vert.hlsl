/*
 * scene_model.vert.hlsl — Model vertex shader (48-byte stride)
 *
 * Part of forge_scene.h — transforms pipeline-processed vertices that include
 * tangent data for normal mapping.  Constructs a world-space TBN basis using
 * the adjugate transpose for normals and Gram-Schmidt re-orthogonalization.
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

    /* Light-space position for shadow mapping.
     * light_vp already contains the model matrix (light_vp = lightVP * model),
     * so we use object-space position here — not world-space wp. */
    output.shadow_pos = mul(light_vp, float4(input.position, 1.0));

    /* Normal transformation via adjugate transpose — preserves
     * perpendicularity under non-uniform scale. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    float3 N = normalize(mul(adj_t, input.normal));

    /* Tangent vectors follow geometry — transform by model directly. */
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
