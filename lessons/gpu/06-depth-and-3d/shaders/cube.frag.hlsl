/*
 * cube.frag.hlsl — Fragment shader for per-vertex colored geometry
 *
 * The simplest possible fragment shader: output the interpolated vertex
 * color.  No textures, no lighting — just pure color to keep the focus
 * on the 3D transform and depth testing concepts.
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
