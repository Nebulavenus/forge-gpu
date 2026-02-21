/*
 * shadow.frag.hlsl — Minimal fragment shader for depth-only shadow pass
 *
 * The shadow pass has no color targets — only a depth buffer.  The GPU
 * writes depth automatically from the vertex shader's SV_Position.z.
 * This fragment shader exists only because the pipeline requires one;
 * it produces no output.
 *
 * SPDX-License-Identifier: Zlib
 */

void main()
{
    /* No output — depth is written automatically by the GPU from
     * the interpolated SV_Position.z value. */
}
