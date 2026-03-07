/*
 * Debug overlay vertex shader — fullscreen triangle from SV_VertexID.
 *
 * Generates a fullscreen triangle (3 vertices) that covers the entire screen
 * without any vertex buffer.  Used to render debug visualizations (such as
 * the stencil buffer contents) as a screen-space overlay.
 *
 * No vertex attributes — position is computed from the vertex index.
 * No uniform buffer — the triangle always covers the full screen.
 *
 * Pipeline: debug_overlay (called with 3 vertices, no vertex buffer)
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSOutput
{
    float4 clip_pos : SV_Position; /* clip-space position for rasterizer */
    float2 uv       : TEXCOORD0;  /* texture coordinate for sampling    */
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Generate UV coordinates for a fullscreen triangle.
     * vertex 0: (0, 0)  vertex 1: (2, 0)  vertex 2: (0, 2)
     * The triangle extends beyond the screen and is clipped by the rasterizer. */
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.clip_pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv = float2(uv.x, 1.0 - uv.y);  /* flip Y for texture coordinates */

    return output;
}
