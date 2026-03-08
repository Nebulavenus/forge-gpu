/*
 * Fullscreen triangle vertex shader — generates a screen-filling triangle.
 *
 * Uses SV_VertexID to procedurally generate an oversize triangle that
 * covers the entire screen.  No vertex buffer is needed — draw 3 vertices
 * with no bindings.
 *
 * The oversize triangle approach uses a single triangle with vertices at
 * (-1,-1), (3,-1), (-1,3) in NDC.  The rasterizer clips it to the viewport,
 * producing a screen-filling quad with fewer vertices and no diagonal seam.
 *
 * UV coordinates are computed so (0,0) maps to the top-left and (1,1) to
 * the bottom-right of the screen.
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSOutput
{
    float4 clip_pos : SV_Position; /* clip-space position for rasterizer */
    float2 uv       : TEXCOORD0;  /* texture coordinate [0..1]          */
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Generate UV from vertex index:
     *   vertex 0 -> (0, 0)
     *   vertex 1 -> (2, 0)
     *   vertex 2 -> (0, 2)
     */
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);

    /* Map UV [0,2] to NDC [-1,1] for clip position. */
    output.clip_pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);

    /* Flip Y for texture sampling — UV origin is top-left. */
    output.uv = float2(uv.x, 1.0 - uv.y);

    return output;
}
