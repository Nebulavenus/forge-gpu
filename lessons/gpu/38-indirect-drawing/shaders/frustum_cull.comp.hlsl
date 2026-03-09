/*
 * frustum_cull.comp.hlsl — Compute shader that fills the indirect draw buffer
 *
 * Each thread processes one object: tests its bounding sphere against the
 * view frustum's 6 planes. Visible objects get num_instances = 1 in their
 * indirect draw command; culled objects get num_instances = 0 (a hardware
 * no-op — the GPU skips them efficiently).
 *
 * Also writes a visibility flag per object for the debug view's green/red
 * coloring.
 *
 * Register layout (SDL GPU compute conventions):
 *   t0, space0 — StructuredBuffer<ObjectData> (read-only storage buffer)
 *   u0, space1 — RWStructuredBuffer<IndirectCommand> (read-write storage buffer)
 *   u1, space1 — RWStructuredBuffer<uint> (read-write storage buffer, visibility)
 *   b0, space2 — cbuffer CullUniforms (uniform buffer)
 *
 * SPDX-License-Identifier: Zlib
 */

struct ObjectData {
    float4x4 model;            /* 64 bytes — model-to-world transform */
    float4   color;            /* 16 bytes — base color multiplier */
    float4   bounding_sphere;  /* 16 bytes — xyz=center(world), w=radius */
    uint     num_indices;      /* 4 bytes */
    uint     first_index;      /* 4 bytes */
    int      vertex_offset;    /* 4 bytes */
    uint     _pad;             /* 4 bytes — align to 16 bytes */
};

struct IndirectCommand {
    uint num_indices;
    uint num_instances;
    uint first_index;
    int  vertex_offset;
    uint first_instance;
};

StructuredBuffer<ObjectData>        objects           : register(t0, space0);
RWStructuredBuffer<IndirectCommand> indirect_commands : register(u0, space1);
RWStructuredBuffer<uint>            visibility        : register(u1, space1);

cbuffer CullUniforms : register(b0, space2) {
    float4 frustum_planes[6];
    uint   num_objects;
    uint   enable_culling;
    float2 _pad;
};

/* Sphere-vs-frustum test: returns true if the sphere is inside or
 * intersecting all 6 planes (conservative — may include objects that
 * touch but don't overlap). */
bool sphere_vs_frustum(float3 center, float radius) {
    [unroll]
    for (int i = 0; i < 6; i++) {
        float dist = dot(center, frustum_planes[i].xyz) + frustum_planes[i].w;
        if (dist < -radius)
            return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint idx = tid.x;
    if (idx >= num_objects) return;

    ObjectData obj = objects[idx];
    float3 center = obj.bounding_sphere.xyz;
    float  radius = obj.bounding_sphere.w;

    bool visible = (enable_culling == 0) || sphere_vs_frustum(center, radius);

    IndirectCommand cmd;
    cmd.num_indices    = obj.num_indices;
    cmd.num_instances  = visible ? 1 : 0;
    cmd.first_index    = obj.first_index;
    cmd.vertex_offset  = obj.vertex_offset;
    cmd.first_instance = idx;

    indirect_commands[idx] = cmd;
    visibility[idx]        = visible ? 1 : 0;
}
