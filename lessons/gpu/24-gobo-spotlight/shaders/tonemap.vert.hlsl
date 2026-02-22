/*
 * Tone Map Vertex Shader — Fullscreen Quad from SV_VertexID
 *
 * Generates a fullscreen quad (2 triangles, 6 vertices) without any vertex
 * buffer.  Positions and UVs are computed purely from the vertex index.
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSOutput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Generate UV coordinates from vertex ID.
     * 6 vertices = 2 triangles forming a fullscreen quad:
     *   Triangle 0: (0,0) (1,0) (0,1)
     *   Triangle 1: (0,1) (1,0) (1,1) */
    float2 uv;
    uv.x = (vertex_id == 1 || vertex_id == 4 || vertex_id == 5) ? 1.0 : 0.0;
    uv.y = (vertex_id == 2 || vertex_id == 3 || vertex_id == 5) ? 1.0 : 0.0;

    /* Flip V so texture row 0 (top) maps to NDC Y = 1 (top). */
    output.uv = float2(uv.x, 1.0 - uv.y);

    /* Map UV [0,1] to NDC [-1,1] for fullscreen coverage. */
    output.clip_pos = float4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, 0.0, 1.0);

    return output;
}
