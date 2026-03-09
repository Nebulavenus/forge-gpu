/*
 * truck_scene.vert.hlsl — Vertex shader for the CesiumMilkTruck
 *
 * Standard instanced rendering pattern from L13: the model matrix arrives
 * as 4 per-instance float4 columns (TEXCOORD3-6). Outputs world position,
 * normal, UV, and light-space clip position for shadow mapping.
 *
 * Per-vertex attributes (slot 0, VERTEX rate):
 *   TEXCOORD0 -> float3 position
 *   TEXCOORD1 -> float3 normal
 *   TEXCOORD2 -> float2 uv
 *
 * Per-instance attributes (slot 1, INSTANCE rate):
 *   TEXCOORD3-6 -> float4 model matrix columns
 *
 * Uniform buffer:
 *   register(b0, space1) -> VP + light VP
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer TruckVertUniforms : register(b0, space1) {
    column_major float4x4 vp;
    column_major float4x4 light_vp;
};

struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 model_c0 : TEXCOORD3;
    float4 model_c1 : TEXCOORD4;
    float4 model_c2 : TEXCOORD5;
    float4 model_c3 : TEXCOORD6;
};

struct VSOutput {
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 world_nrm  : TEXCOORD1;
    float2 uv         : TEXCOORD2;
    float4 light_clip : TEXCOORD3;
};

VSOutput main(VSInput input) {
    VSOutput output;

    float4x4 model = transpose(float4x4(
        input.model_c0, input.model_c1,
        input.model_c2, input.model_c3
    ));

    float4 world = mul(model, float4(input.position, 1.0));
    output.world_pos  = world.xyz;
    output.clip_pos   = mul(vp, world);
    output.light_clip = mul(light_vp, world);

    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_nrm = mul(adj_t, input.normal);

    output.uv = input.uv;

    return output;
}
