/*
 * Shadow pass vertex shader with vertex pulling.
 *
 * Reads vertex positions from a storage buffer (same as the scene shader)
 * and transforms them into light clip space for shadow map generation.
 * Only position is needed — no normals or UVs for the depth-only pass.
 *
 * Storage buffer binding (DXIL vertex shader):
 *   register(t0, space0) -> slot 0: vertex data (StructuredBuffer)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: light VP matrix
 *
 * SPDX-License-Identifier: Zlib
 */

struct PulledVertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

/* Same storage buffer as the scene pass — no need to duplicate data. */
StructuredBuffer<PulledVertex> vertex_buffer : register(t0, space0);

cbuffer ShadowUniforms : register(b0, space1)
{
    column_major float4x4 light_vp; /* light view-projection matrix */
};

float4 main(uint vertex_id : SV_VertexID) : SV_Position
{
    /* Pull only the position — normals and UVs are unused for shadows. */
    float3 pos = vertex_buffer[vertex_id].position;
    return mul(light_vp, float4(pos, 1.0));
}
