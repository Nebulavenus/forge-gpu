/*
 * Shadow pass fragment shader — depth-only, no color output.
 *
 * Part of forge_scene.h — the rasterizer writes depth automatically.
 * This shader exists only because SDL_GPU requires a fragment shader
 * for every graphics pipeline, even depth-only passes.
 *
 * SPDX-License-Identifier: Zlib
 */

float4 main() : SV_Target
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
