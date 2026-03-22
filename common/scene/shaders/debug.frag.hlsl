/*
 * debug.frag.hlsl — Debug line fragment shader
 *
 * Part of forge_scene.h — outputs the interpolated vertex color as the
 * fragment color.  No lighting, no textures — debug lines are flat-colored
 * for maximum clarity.
 *
 * SPDX-License-Identifier: Zlib
 */

struct PSInput
{
    float4 clip_pos : SV_Position;
    float4 color    : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return input.color;
}
