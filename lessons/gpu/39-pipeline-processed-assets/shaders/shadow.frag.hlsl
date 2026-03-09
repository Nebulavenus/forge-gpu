/*
 * shadow_frag.hlsl — Empty fragment shader for shadow depth pass
 *
 * The shadow map only needs depth values, written automatically by the
 * rasterizer.  This shader exists solely because SDL GPU requires a
 * fragment shader to be bound, even for depth-only rendering.
 *
 * SPDX-License-Identifier: Zlib
 */

void main() {
    /* Depth is written automatically by the rasterizer. */
}
