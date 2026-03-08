/*
 * X-ray stencil mark fragment shader — dummy output for stencil-only pass.
 *
 * The pipeline has color write mask = 0, so this output is discarded.
 * SDL_GPU requires a fragment shader for every pipeline, so this minimal
 * shader satisfies that requirement.
 *
 * No TEXCOORD inputs — only SV_Position is available from the vertex shader.
 *
 * SPDX-License-Identifier: Zlib
 */

float4 main(float4 pos : SV_Position) : SV_Target0
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
