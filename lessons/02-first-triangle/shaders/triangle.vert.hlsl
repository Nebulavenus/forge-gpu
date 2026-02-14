/*
 * triangle.vert.hlsl — Vertex shader for a colored triangle
 *
 * Takes a 2D position and RGB color per vertex, passes the color through
 * to the fragment shader.  Position is already in normalized device
 * coordinates (NDC), so no transform is needed.
 *
 * SDL GPU convention: vertex attribute locations map to TEXCOORD{N} semantics.
 *   location 0 → TEXCOORD0 (position)
 *   location 1 → TEXCOORD1 (color)
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSInput
{
    float2 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 color    : TEXCOORD1;   /* vertex attribute location 1 */
};

struct VSOutput
{
    float4 position : SV_Position; /* clip-space position for rasterizer */
    float4 color    : TEXCOORD0;   /* interpolated to fragment shader   */
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.color    = float4(input.color, 1.0);
    return output;
}
