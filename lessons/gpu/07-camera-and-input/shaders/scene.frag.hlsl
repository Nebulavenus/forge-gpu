/*
 * scene.frag.hlsl — Fragment shader for per-vertex colored geometry
 *
 * Output the interpolated vertex color.  No textures or lighting —
 * the focus of this lesson is camera movement and input handling.
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
