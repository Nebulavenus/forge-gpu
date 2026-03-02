/*
 * Skybox vertex shader — render an environment cube map behind everything.
 *
 * Uses rotation-only view-projection so the skybox follows camera rotation
 * but not translation.  Output depth is forced to 1.0 via the pos.xyww
 * technique — every other object draws in front.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 vp_no_translation; /* View (rotation only) * Projection */
};

struct VSInput
{
    float3 position : TEXCOORD0; /* Cube vertex position [-1..1] */
};

struct VSOutput
{
    float4 clip_pos  : SV_Position;  /* Clip-space position for rasterizer */
    float3 direction : TEXCOORD0;    /* Cube map sample direction */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Pass raw vertex position as cube map sample direction. */
    output.direction = input.position;

    /* Transform through rotation-only VP. */
    float4 pos = mul(vp_no_translation, float4(input.position, 1.0));

    /* Set z = w so depth is always 1.0 after perspective divide.
     * This places the skybox at the far plane. */
    output.clip_pos = pos.xyww;

    return output;
}
