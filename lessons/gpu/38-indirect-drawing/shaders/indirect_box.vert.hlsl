/*
 * indirect_box.vert.hlsl — Vertex shader for indirect-drawn boxes
 *
 * Uses the per-instance object_id attribute (from the instance vertex buffer)
 * to look up per-object transforms in a StructuredBuffer. This is the
 * portable alternative to SV_InstanceID, which does not consistently include
 * first_instance across all GPU backends.
 *
 * Per-vertex attributes (slot 0, VERTEX rate):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Per-instance attributes (slot 1, INSTANCE rate):
 *   TEXCOORD3 -> uint object_id   (location 3)
 *
 * Read-only storage buffer:
 *   register(t0, space0) -> StructuredBuffer of per-object transforms
 *
 * Uniform buffer:
 *   register(b0, space1) -> VP + light VP matrices
 *
 * SPDX-License-Identifier: Zlib
 */

struct ObjectTransform {
    column_major float4x4 model;   /* 64 bytes — model-to-world */
    float4   color;                /* 16 bytes — base color */
    float4   bounding_sphere;      /* 16 bytes — not used here but keeps layout */
    uint     num_indices;          /* 4 bytes */
    uint     first_index;          /* 4 bytes */
    int      vertex_offset;        /* 4 bytes */
    uint     _pad;                 /* 4 bytes */
};

StructuredBuffer<ObjectTransform> object_data : register(t0, space0);

cbuffer VertUniforms : register(b0, space1) {
    column_major float4x4 vp;
    column_major float4x4 light_vp;
};

struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    uint   object_id : TEXCOORD3;
};

struct VSOutput {
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 world_nrm  : TEXCOORD1;
    float2 uv         : TEXCOORD2;
    float4 light_clip : TEXCOORD3;
    float4 color      : TEXCOORD4;
};

VSOutput main(VSInput input) {
    VSOutput output;

    ObjectTransform obj = object_data[input.object_id];
    float4x4 model = obj.model;

    float4 world = mul(model, float4(input.position, 1.0));
    output.world_pos = world.xyz;
    output.clip_pos  = mul(vp, world);

    /* Normal transformation via adjugate transpose — handles non-uniform
     * scaling correctly without requiring the inverse matrix. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_nrm = mul(adj_t, input.normal);

    output.uv        = input.uv;
    output.light_clip = mul(light_vp, world);
    output.color      = obj.color;

    return output;
}
