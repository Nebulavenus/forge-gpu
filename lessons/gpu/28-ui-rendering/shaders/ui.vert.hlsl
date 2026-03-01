/*
 * ui.vert.hlsl -- UI vertex shader for Lesson 28 (UI Rendering)
 *
 * Transforms screen-space pixel positions into clip space using an
 * orthographic projection matrix.  The projection maps pixel coordinates
 * (origin top-left, y-down) to the [-1, +1] normalized device coordinate
 * range, flipping y so that y=0 maps to the top of the screen.
 *
 * UV coordinates and per-vertex RGBA color are passed through unchanged
 * to the fragment shader.  The color arrives split across two float2
 * attributes (rg and ba) because ForgeUiVertex stores 8 floats and the
 * pipeline uses 4 x FLOAT2 vertex attributes for uniform alignment.
 *
 * Vertex attributes (mapped from SDL GPU vertex attribute locations):
 *   TEXCOORD0 -> position (float2, offset  0) -- location 0
 *   TEXCOORD1 -> uv       (float2, offset  8) -- location 1
 *   TEXCOORD2 -> color_rg (float2, offset 16) -- location 2
 *   TEXCOORD3 -> color_ba (float2, offset 24) -- location 3
 *
 * Vertex uniform:
 *   register(b0, space1) -> orthographic projection matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Orthographic projection: maps [0..W, 0..H] pixel coords to [-1..+1] NDC.
 * Built on the C side each frame from the current window dimensions so
 * that window resizes are handled automatically without shader changes.  */
cbuffer UiUniforms : register(b0, space1)
{
    column_major float4x4 projection;
};

/* Input from the vertex buffer -- matches ForgeUiVertex (32 bytes/vertex).
 * SDL GPU maps all vertex attributes through TEXCOORD semantics; the
 * location index corresponds to the attribute slot set in the pipeline.  */
struct VSInput
{
    float2 position : TEXCOORD0;  /* screen-space pixel coordinates (x, y)  */
    float2 uv       : TEXCOORD1;  /* atlas texture coordinates (u, v)       */
    float2 color_rg : TEXCOORD2;  /* vertex color: red and green channels   */
    float2 color_ba : TEXCOORD3;  /* vertex color: blue and alpha channels  */
};

/* Output to the rasterizer and fragment shader.  SV_Position is consumed
 * by the rasterizer for clipping and viewport mapping.  TEXCOORD0 and
 * TEXCOORD1 are interpolated across each triangle and passed to the
 * fragment shader for atlas sampling and color tinting.                    */
struct VSOutput
{
    float4 position : SV_Position; /* clip-space position for rasterizer    */
    float2 uv       : TEXCOORD0;   /* interpolated atlas UV for frag shader */
    float4 color    : TEXCOORD1;   /* reassembled RGBA color for frag shader*/
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Transform the 2D pixel position to clip space.  z is set to 0
     * because UI has no depth -- all quads live on the same plane.
     * w is set to 1 for a standard affine (non-perspective) transform.   */
    output.position = mul(projection, float4(input.position, 0.0, 1.0));

    /* Pass the atlas UV through unchanged -- the rasterizer will
     * interpolate it across the triangle for per-pixel sampling.         */
    output.uv = input.uv;

    /* Reassemble the full RGBA color from the two float2 halves.
     * The split exists because the C-side ForgeUiVertex stores r,g,b,a
     * as four separate floats, and the pipeline reads them as two
     * float2 attributes for alignment with the 32-byte vertex stride.    */
    output.color = float4(input.color_rg, input.color_ba);

    return output;
}
