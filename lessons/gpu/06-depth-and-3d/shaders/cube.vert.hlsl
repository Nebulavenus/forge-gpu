/*
 * cube.vert.hlsl — Vertex shader for a 3D cube with MVP transform
 *
 * Transforms 3D vertices from object space to clip space using the
 * Model-View-Projection (MVP) matrix.  Passes per-vertex color through
 * to the fragment shader.
 *
 * This is the first shader in the series that operates in true 3D:
 *   - Input positions are float3 (not float2 like previous lessons)
 *   - The uniform is a full 4x4 MVP matrix (not time + aspect)
 *   - The GPU handles perspective divide after this shader runs
 *
 * Vertex attributes:
 *   TEXCOORD0 → float3 position  (location 0)
 *   TEXCOORD1 → float3 color     (location 1)
 *
 * Uniform buffer:
 *   register(b0, space1) → vertex shader uniform slot 0
 *   Contains the combined MVP matrix (64 bytes).
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── Uniform buffer ───────────────────────────────────────────────────────
 * The MVP matrix is composed on the CPU each frame:
 *   mvp = projection * view * model
 * and pushed via SDL_PushGPUVertexUniformData.
 *
 * forge_math.h stores matrices column-major, which matches HLSL's
 * default column_major layout — no transpose needed. */
cbuffer Uniforms : register(b0, space1)
{
    column_major float4x4 mvp;
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 color    : TEXCOORD1;   /* vertex attribute location 1 */
};

struct VSOutput
{
    float4 position : SV_Position; /* clip-space position for rasterizer  */
    float4 color    : TEXCOORD0;   /* interpolated to fragment shader     */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Transform the 3D vertex position from object space to clip space.
     *
     * mul(mvp, float4(pos, 1.0)) applies the full transform chain:
     *   object → world → camera → clip
     *
     * The w=1.0 makes this a position (affected by translation).
     * After this shader, the GPU divides by w for perspective. */
    output.position = mul(mvp, float4(input.position, 1.0));

    /* Pass vertex color through — the rasterizer will interpolate it
     * across each triangle face, giving smooth color gradients. */
    output.color = float4(input.color, 1.0);

    return output;
}
