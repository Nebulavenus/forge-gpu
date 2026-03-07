/*
 * Outline fragment shader — solid-color output for stencil-based outlines.
 *
 * Used with the outline pipeline to render a scaled-up version of an object
 * in a solid color.  The stencil test (configured in pipeline state) rejects
 * fragments where the original object was drawn, leaving only the outline
 * visible around the silhouette.
 *
 * This shader ignores all interpolated vertex data except SV_Position (which
 * the rasterizer needs).  It outputs a flat color from the uniform buffer.
 *
 * Stencil behavior is controlled entirely by pipeline state — this shader
 * simply outputs outline_color for every surviving fragment.
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: outline color (16 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer OutlineUniforms : register(b0, space3)
{
    float4 outline_color;   /* RGBA color for the outline */
};

float4 main(float4 clip_pos   : SV_Position,
             float3 world_pos  : TEXCOORD0,
             float3 world_nrm  : TEXCOORD1,
             float4 light_clip : TEXCOORD2) : SV_Target
{
    /* All lighting and interpolation data is ignored — output flat color.
     * The stencil test in the pipeline ensures only outline pixels survive. */
    return outline_color;
}
