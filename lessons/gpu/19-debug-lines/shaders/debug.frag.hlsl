/*
 * debug.frag.hlsl — Fragment shader for debug line rendering
 *
 * Simple pass-through: outputs the interpolated vertex color as the
 * fragment color.  No lighting, no textures — debug lines are meant
 * to be flat-colored for maximum clarity.
 *
 * SPDX-License-Identifier: Zlib
 */

struct PSInput
{
    float4 clip_pos : SV_Position; /* not used, but required by pipeline */
    float4 color    : TEXCOORD0;   /* interpolated color from vertex     */
};

float4 main(PSInput input) : SV_Target
{
    return input.color;
}
