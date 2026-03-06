/*
 * Vertex-pulled vertex shader — reads vertex data from a storage buffer
 * instead of using the fixed-function vertex input assembler.
 *
 * Traditional rendering binds vertex buffers and declares vertex attributes
 * in the pipeline.  The GPU's input assembler reads and decodes each
 * attribute before the vertex shader sees it.
 *
 * Vertex pulling replaces that with a manual read: the shader receives only
 * SV_VertexID and fetches position, normal, and UV from a StructuredBuffer.
 * This decouples the vertex format from the pipeline state, enabling:
 *   - Flexible vertex layouts (add/remove attributes without rebuilding pipelines)
 *   - Mesh compression (quantized normals, half-float positions)
 *   - Compute-to-vertex data flow (write into the same buffer from compute)
 *
 * Storage buffer binding (DXIL vertex shader):
 *   register(t0, space0) -> slot 0: vertex data (StructuredBuffer)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: scene transforms (MVP, model, light_vp)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Vertex data stored in the storage buffer — matches PulledVertex in main.c.
 * Each element is 32 bytes: position(12) + normal(12) + uv(8). */
struct PulledVertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

/* Read-only structured buffer containing all vertex data for the mesh.
 * Bound to vertex storage buffer slot 0 via SDL_BindGPUVertexStorageBuffers.
 * In DXIL, vertex storage buffers use register(t[n], space0). */
StructuredBuffer<PulledVertex> vertex_buffer : register(t0, space0);

cbuffer SceneUniforms : register(b0, space1)
{
    column_major float4x4 mvp;      /* model-view-projection matrix        */
    column_major float4x4 model;    /* model (world) matrix — for normals  */
    column_major float4x4 light_vp; /* light VP for shadow map projection  */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 world_nrm  : TEXCOORD1;
    float2 uv         : TEXCOORD2;
    float4 light_clip : TEXCOORD3;
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Fetch vertex data from the storage buffer using the vertex index.
     * This replaces the fixed-function input assembler — the GPU reads
     * structured data directly instead of decoding interleaved attributes. */
    PulledVertex v = vertex_buffer[vertex_id];

    /* Transform to world space using the model matrix. */
    float4 world = mul(model, float4(v.position, 1.0));
    output.world_pos = world.xyz;

    /* Project to clip space. */
    output.clip_pos = mul(mvp, float4(v.position, 1.0));

    /* Transform normal to world space (upper 3x3 of model matrix).
     * Correct for rigid transforms; non-uniform scale would need
     * the inverse transpose. */
    output.world_nrm = normalize(mul((float3x3)model, v.normal));

    /* Pass through UV coordinates for texture sampling. */
    output.uv = v.uv;

    /* Light-space position for shadow mapping.
     * light_vp already contains (lightVP * model), so multiply by the
     * model-space position — not world — to avoid applying model twice. */
    output.light_clip = mul(light_vp, float4(v.position, 1.0));

    return output;
}
