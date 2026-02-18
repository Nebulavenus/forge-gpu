/*
 * debug_quad.vert.hlsl — Screen-space quad via SV_VertexID
 *
 * Draws a screen-space quad without any vertex buffer by using the
 * built-in vertex ID to generate positions.  This is a common technique
 * for full-screen passes and debug overlays.
 *
 * The quad_bounds uniform specifies the screen position:
 *   x = left   (NDC, -1 to 1)
 *   y = bottom (NDC, -1 to 1)
 *   z = right  (NDC, -1 to 1)
 *   w = top    (NDC, -1 to 1)
 *
 * Vertex IDs 0-5 form two triangles covering the quad:
 *   0--1    Triangle 1: 0, 1, 2
 *   | /|    Triangle 2: 2, 1, 3
 *   |/ |
 *   2--3
 *
 * No vertex attributes — uses only SV_VertexID.
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains quad bounds as float4 (16 bytes).
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer DebugVertUniforms : register(b0, space1)
{
    float4 quad_bounds;  /* (left, bottom, right, top) in NDC */
};

struct VSOutput
{
    float4 clip_pos : SV_Position;  /* screen-space quad position */
    float2 uv       : TEXCOORD0;    /* UV for shadow map sampling */
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Generate UV coordinates from vertex ID.
     * 6 vertices = 2 triangles forming a quad.
     * Vertex order: 0(0,0), 1(1,0), 2(0,1), 3(0,1), 4(1,0), 5(1,1) */
    float2 uv;
    uv.x = (vertex_id == 1 || vertex_id == 4 || vertex_id == 5) ? 1.0 : 0.0;
    uv.y = (vertex_id == 2 || vertex_id == 3 || vertex_id == 5) ? 1.0 : 0.0;
    /* Flip V so the shadow map renders right-side up.
     * NDC Y points up (bottom = -1, top = 1) but texture V points
     * down (row 0 = top), so without this flip the image is inverted. */
    output.uv = float2(uv.x, 1.0 - uv.y);

    /* Map UV [0,1] to NDC using quad_bounds */
    float x = lerp(quad_bounds.x, quad_bounds.z, uv.x);
    float y = lerp(quad_bounds.y, quad_bounds.w, uv.y);

    output.clip_pos = float4(x, y, 0.0, 1.0);

    return output;
}
