/*
 * quad.vert.hlsl — Vertex shader for a textured quad
 *
 * Passes UV coordinates through to the fragment shader for texture sampling.
 * Applies the same rotation animation as Lesson 03, but now working with a
 * quad (4 vertices via an index buffer) instead of a triangle.
 *
 * Vertex attributes:
 *   TEXCOORD0 → float2 position  (location 0)
 *   TEXCOORD1 → float2 uv        (location 1)
 *
 * Uniform buffer:
 *   register(b0, space1) → vertex shader uniform slot 0
 *   Contains time + aspect ratio for animation.
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── Uniform buffer ───────────────────────────────────────────────────────
 * Data pushed from the CPU every frame via SDL_PushGPUVertexUniformData.
 * register(b0, space1) = vertex shader, uniform slot 0. */
cbuffer Uniforms : register(b0, space1)
{
    float time;     /* elapsed time in seconds   */
    float aspect;   /* window width / height     */
};

struct VSInput
{
    float2 position : TEXCOORD0;   /* vertex attribute location 0 */
    float2 uv       : TEXCOORD1;   /* vertex attribute location 1 */
};

struct VSOutput
{
    float4 position : SV_Position; /* clip-space position for rasterizer  */
    float2 uv       : TEXCOORD0;   /* interpolated to fragment shader     */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* ── Aspect ratio correction ──────────────────────────────────────
     * Same as Lesson 03: divide x by aspect BEFORE rotation so the
     * quad keeps its shape at every angle. */
    float2 corrected = float2(input.position.x / aspect, input.position.y);

    /* ── 2D rotation matrix ───────────────────────────────────────────
     * Rotate the corrected position around the origin by `time` radians.
     *
     *   | cos(t)  -sin(t) |   | x |   | x*cos(t) - y*sin(t) |
     *   | sin(t)   cos(t) | × | y | = | x*sin(t) + y*cos(t) |
     */
    float c = cos(time);
    float s = sin(time);

    float2 rotated;
    rotated.x = corrected.x * c - corrected.y * s;
    rotated.y = corrected.x * s + corrected.y * c;

    output.position = float4(rotated, 0.0, 1.0);

    /* ── UV pass-through ──────────────────────────────────────────────
     * UVs are per-vertex data that the rasterizer interpolates across
     * the surface.  The fragment shader uses them to look up texels. */
    output.uv = input.uv;

    return output;
}
