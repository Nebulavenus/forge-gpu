/*
 * Crosshair vertex shader — screen-space overlay for the picking cursor.
 *
 * Takes vertex positions in normalized device coordinates (NDC) directly,
 * bypassing any model/view/projection transforms.  This allows the crosshair
 * to be positioned precisely at the screen-space cursor location.
 *
 * Each vertex carries its own color, enabling the crosshair lines to have
 * different colors (e.g., white normally, green when hovering an object).
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float2 position  (location 0, NDC x/y)
 *   TEXCOORD1 -> float4 color     (location 1, per-vertex RGBA)
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSInput
{
    float2 position : TEXCOORD0;   /* screen position in NDC [-1..1] */
    float4 color    : TEXCOORD1;   /* per-vertex RGBA color          */
};

struct VSOutput
{
    float4 clip_pos : SV_Position; /* clip-space position (z=0, w=1) */
    float4 color    : TEXCOORD0;   /* interpolated color for fragment */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Position is already in NDC — place at z=0 (front) with w=1.
     * No projection needed; the crosshair is a fixed screen overlay. */
    output.clip_pos = float4(input.position, 0.0, 1.0);
    output.color = input.color;

    return output;
}
