/*
 * Sky fragment shader — vertical gradient background.
 *
 * Part of forge_scene.h — renders a dark-to-light gradient from top to
 * bottom.  No uniforms; colors are hardcoded for a neutral studio backdrop.
 *
 * SPDX-License-Identifier: Zlib
 */

struct PSInput {
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    /* uv.y = 0 at the top of the screen, 1 at the bottom.
     * Interpolate from a dark upper sky to a lighter horizon. */
    float t = input.uv.y;
    float3 top     = float3(0.05, 0.05, 0.15);
    float3 horizon = float3(0.15, 0.15, 0.25);
    float3 color   = lerp(top, horizon, t);

    return float4(color, 1.0);
}
