/*
 * debug_box.vert.hlsl — Vertex shader for the debug view boxes
 *
 * Draws ALL boxes (no culling) in the debug camera's viewport.
 * Uses the same per-instance object_id + storage buffer pattern as
 * indirect_box.vert.hlsl. Passes the object_id to the fragment shader
 * for visibility-based coloring (green = visible, red = culled).
 *
 * Per-vertex attributes (slot 0, VERTEX rate):
 *   TEXCOORD0 -> float3 position
 *   TEXCOORD1 -> float3 normal
 *   TEXCOORD2 -> float2 uv (unused but present in vertex buffer)
 *
 * Per-instance attributes (slot 1, INSTANCE rate):
 *   TEXCOORD3 -> uint object_id
 *
 * Read-only storage buffer:
 *   register(t0, space0) -> per-object transforms
 *
 * Uniform buffer:
 *   register(b0, space1) -> debug camera VP matrix
 *
 * SPDX-License-Identifier: Zlib
 */

struct ObjectTransform {
    column_major float4x4 model;
    float4   color;
    float4   bounding_sphere;
    uint     num_indices;
    uint     first_index;
    int      vertex_offset;
    uint     _pad;
};

StructuredBuffer<ObjectTransform> object_data : register(t0, space0);

cbuffer DebugVertUniforms : register(b0, space1) {
    column_major float4x4 debug_vp;
};

struct VSInput {
    float3 position  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;
    uint   object_id : TEXCOORD3;
};

struct VSOutput {
    float4 clip_pos  : SV_Position;
    float3 world_nrm : TEXCOORD0;
    nointerpolation uint object_id : TEXCOORD1;
};

VSOutput main(VSInput input) {
    VSOutput output;

    ObjectTransform obj = object_data[input.object_id];
    float4 world = mul(obj.model, float4(input.position, 1.0));
    output.clip_pos = mul(debug_vp, world);

    float3x3 m = (float3x3)obj.model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_nrm = mul(adj_t, input.normal);

    output.object_id = input.object_id;

    return output;
}
