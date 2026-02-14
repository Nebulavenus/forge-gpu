/*
 * triangle.frag.hlsl — Fragment shader for a colored triangle
 *
 * Receives the interpolated color from the vertex shader and outputs it
 * directly.  The rasterizer automatically interpolates vertex colors
 * across the triangle face (this is called "varying interpolation" or
 * "smooth shading").
 *
 * SPDX-License-Identifier: Zlib
 */

struct PSInput
{
    float4 position : SV_Position; /* not used, but required by pipeline */
    float4 color    : TEXCOORD0;   /* interpolated from vertex shader    */
};

float4 main(PSInput input) : SV_Target
{
    return input.color;
}
