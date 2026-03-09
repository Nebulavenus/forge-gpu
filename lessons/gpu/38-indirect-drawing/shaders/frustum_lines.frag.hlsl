/*
 * frustum_lines.frag.hlsl — Fragment shader for frustum wireframe
 *
 * Outputs the interpolated vertex color. Used for the frustum wireframe
 * lines (bright yellow) and the viewport divider line (white).
 *
 * SPDX-License-Identifier: Zlib
 */

struct PSInput {
    float4 clip_pos : SV_Position;
    float4 color    : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    return input.color;
}
