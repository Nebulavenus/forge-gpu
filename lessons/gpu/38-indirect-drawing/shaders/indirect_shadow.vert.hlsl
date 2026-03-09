/*
 * indirect_shadow.vert.hlsl — Shadow pass vertex shader for indirect-drawn boxes
 *
 * Transforms vertices to light clip space using the per-object model matrix
 * from a storage buffer. Uses the same object_id instance attribute pattern
 * as indirect_box.vert.hlsl. Only outputs SV_Position for depth-only rendering.
 *
 * Per-vertex attributes (slot 0, VERTEX rate):
 *   TEXCOORD0 -> float3 position  (location 0)
 *
 * Per-instance attributes (slot 1, INSTANCE rate):
 *   TEXCOORD3 -> uint object_id   (location 3) — note: locations 1,2 skipped but not bound
 *
 * Read-only storage buffer:
 *   register(t0, space0) -> StructuredBuffer of per-object transforms
 *
 * Uniform buffer:
 *   register(b0, space1) -> light VP matrix
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

cbuffer ShadowUniforms : register(b0, space1) {
    column_major float4x4 light_vp;
};

struct VSInput {
    float3 position  : TEXCOORD0;
    float3 normal    : TEXCOORD1;  /* present in vertex buffer but unused */
    float2 uv        : TEXCOORD2;  /* present in vertex buffer but unused */
    uint   object_id : TEXCOORD3;
};

float4 main(VSInput input) : SV_Position {
    ObjectTransform obj = object_data[input.object_id];
    float4 world = mul(obj.model, float4(input.position, 1.0));
    return mul(light_vp, world);
}
