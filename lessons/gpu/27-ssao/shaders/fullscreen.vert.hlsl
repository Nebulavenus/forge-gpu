/*
 * fullscreen.vert.hlsl — Fullscreen quad via SV_VertexID
 *
 * Draws a fullscreen quad without any vertex buffer by using the built-in
 * vertex ID to generate positions. Used by the SSAO, blur, and composite
 * passes.
 *
 * Six vertices form two triangles covering the screen:
 *   0--1    Triangle 1: 0, 1, 2
 *   | /|    Triangle 2: 2, 1, 3
 *   |/ |    (mapped to vertex IDs 0-5)
 *   2--3
 *
 * UV coordinates are generated to sample the source texture.
 * The V axis is flipped because texture row 0 is the top of the image,
 * but NDC Y = -1 is the bottom of the screen.
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSOutput
{
    float4 clip_pos : SV_Position;  /* screen-space quad position     */
    float2 uv       : TEXCOORD0;    /* UV for texture sampling        */
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Generate UV coordinates from vertex ID.
     * 6 vertices = 2 triangles forming a fullscreen quad.
     * Vertex order: 0(0,0), 1(1,0), 2(0,1), 3(0,1), 4(1,0), 5(1,1) */
    float2 uv;
    uv.x = (vertex_id == 1 || vertex_id == 4 || vertex_id == 5) ? 1.0 : 0.0;
    uv.y = (vertex_id == 2 || vertex_id == 3 || vertex_id == 5) ? 1.0 : 0.0;

    /* Flip V so texture row 0 (top) maps to NDC Y = 1 (top).
     * Without this flip, the image would appear vertically inverted. */
    output.uv = float2(uv.x, 1.0 - uv.y);

    /* Map UV [0,1] to NDC [-1,1] for fullscreen coverage. */
    output.clip_pos = float4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, 0.0, 1.0);

    return output;
}
