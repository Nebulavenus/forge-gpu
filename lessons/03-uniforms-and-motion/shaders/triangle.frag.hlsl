/*
 * triangle.frag.hlsl — Fragment shader (same as Lesson 02)
 *
 * Receives the interpolated color from the vertex shader and outputs it
 * directly.  The rasterizer automatically interpolates vertex colors
 * across the triangle face (smooth shading).
 *
 * This shader has no uniform buffers — the animation is handled entirely
 * in the vertex shader.
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
