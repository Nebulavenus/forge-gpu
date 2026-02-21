/*
 * triangle.vert.hlsl — Vertex shader with a uniform buffer for animation
 *
 * Builds on Lesson 02's pass-through vertex shader by adding a uniform
 * buffer that provides the elapsed time.  The shader uses this to build
 * a 2D rotation matrix and spin the triangle.
 *
 * SDL GPU convention for uniform buffers in HLSL:
 *   Vertex shader uniforms:   register(b0, space1)
 *   Fragment shader uniforms: register(b0, space3)
 *
 * The slot index (b0, b1, …) matches the slot_index parameter you pass
 * to SDL_PushGPUVertexUniformData.  space1 is mandatory for vertex
 * stage uniforms in SDL's binding model.
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
    float3 color    : TEXCOORD1;   /* vertex attribute location 1 */
};

struct VSOutput
{
    float4 position : SV_Position; /* clip-space position for rasterizer */
    float4 color    : TEXCOORD0;   /* interpolated to fragment shader   */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* ── 2D rotation matrix ───────────────────────────────────────────
     * Rotate the input position around the origin by `time` radians.
     *
     *   | cos(t)  -sin(t) |   | x |   | x*cos(t) - y*sin(t) |
     *   | sin(t)   cos(t) | × | y | = | x*sin(t) + y*cos(t) |
     *
     * This is the standard 2D rotation formula.  Because our triangle
     * is centered at the origin, it spins in place.
     *
     * We rotate BEFORE aspect correction so the rotation happens in
     * the isotropic input space (where x and y have equal scale).
     * If we corrected first, the rotation would happen in a squished
     * coordinate space, distorting the triangle as it spins. */
    float c = cos(time);
    float s = sin(time);

    float2 rotated;
    rotated.x = input.position.x * c - input.position.y * s;
    rotated.y = input.position.x * s + input.position.y * c;

    /* ── Aspect ratio correction ──────────────────────────────────────
     * NDC goes from -1 to +1 on both axes, but the window is wider
     * than it is tall (e.g. 1280×720 → aspect = 1.778).  Without
     * correction, a shape that should look circular gets stretched
     * horizontally.  Dividing x by the aspect ratio squishes the
     * x-axis back to equal proportions.
     *
     * We apply this AFTER rotation so the triangle maintains its
     * shape while spinning.  The aspect correction then maps the
     * rotated coordinates to clip space where they display correctly
     * on the non-square window. */
    output.position = float4(rotated.x / aspect, rotated.y, 0.0, 1.0);
    output.color    = float4(input.color, 1.0);
    return output;
}
