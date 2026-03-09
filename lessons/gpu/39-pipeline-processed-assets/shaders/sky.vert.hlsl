/*
 * sky_vert.hlsl — Fullscreen triangle vertex shader for sky background
 *
 * Generates a fullscreen triangle from SV_VertexID without a vertex
 * buffer.  Three vertices (IDs 0, 1, 2) produce a triangle that covers
 * the entire screen.  The clip-space z is set to 0.9999 so the sky
 * renders behind all scene geometry.
 *
 * No vertex buffer — uses SV_VertexID to generate positions:
 *   ID 0 -> (-1, -1)  bottom-left
 *   ID 1 -> ( 3, -1)  far right
 *   ID 2 -> (-1,  3)  far top
 *
 * No uniform buffer — the sky is a simple vertical gradient.
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSOutput {
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertex_id : SV_VertexID) {
    VSOutput output;

    /* Generate fullscreen triangle UVs from vertex ID.
     * ID 0 -> (0, 0), ID 1 -> (2, 0), ID 2 -> (0, 2)
     * The triangle overshoots the [0,1] range but the rasterizer clips
     * to the viewport, covering the full screen with just 3 vertices. */
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);

    /* Map [0,1] UV to [-1,1] clip space.  Flip Y so uv.y=0 is the top
     * of the screen (matching the gradient direction we want). */
    output.clip_pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0),
                             0.9999, 1.0);
    output.uv = uv;

    return output;
}
