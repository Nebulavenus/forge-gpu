/*
 * Crosshair fragment shader — flat color passthrough for the picking cursor.
 *
 * Outputs the interpolated per-vertex color with no lighting or texture
 * sampling.  The crosshair is a simple screen-space overlay drawn after
 * all scene rendering is complete.
 *
 * SPDX-License-Identifier: Zlib
 */

float4 main(float4 clip_pos : SV_Position,
             float4 color    : TEXCOORD0) : SV_Target
{
    return color;
}
