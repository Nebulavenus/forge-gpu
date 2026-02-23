/*
 * fullscreen.vert.hlsl — Fullscreen quad via SV_VertexID
 *
 * Standard fullscreen quad for post-processing passes (bloom and tonemap).
 * No vertex buffer needed — positions and UVs are computed from the
 * built-in vertex ID.
 *
 * Shared by: bloom_downsample, bloom_upsample, and tonemap passes.
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
     * 6 vertices = 2 triangles forming a fullscreen quad. */
    float2 uv;
    uv.x = (vertex_id == 1 || vertex_id == 4 || vertex_id == 5) ? 1.0 : 0.0;
    uv.y = (vertex_id == 2 || vertex_id == 3 || vertex_id == 5) ? 1.0 : 0.0;

    /* Flip V so texture row 0 (top) maps to NDC Y = 1 (top). */
    output.uv = float2(uv.x, 1.0 - uv.y);

    /* Map UV [0,1] to NDC [-1,1] for fullscreen coverage. */
    output.clip_pos = float4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, 0.0, 1.0);

    return output;
}
