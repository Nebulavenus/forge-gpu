/*
 * scene.vert.hlsl — Vertex shader for glTF scene rendering
 *
 * Transforms mesh vertices from object space to clip space using the
 * combined Model-View-Projection matrix, and passes UV coordinates
 * and world-space normals through to the fragment shader.
 *
 * Vertex attributes (matching SceneVertex layout in main.c):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains the combined MVP matrix (64 bytes).
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* vertex attribute location 1 */
    float2 uv       : TEXCOORD2;   /* vertex attribute location 2 */
};

struct VSOutput
{
    float4 position : SV_Position; /* clip-space position for rasterizer  */
    float2 uv       : TEXCOORD0;   /* interpolated to fragment shader     */
    float3 normal   : TEXCOORD1;   /* world-space normal (for future lighting) */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Transform from object space to clip space via the MVP matrix.
     * Each primitive's model matrix is pre-multiplied with view and projection
     * on the CPU, so the shader only needs one matrix multiply. */
    output.position = mul(mvp, float4(input.position, 1.0));

    /* Pass UV coordinates through for texture sampling.  The rasterizer
     * will interpolate these across each triangle's surface. */
    output.uv = input.uv;

    /* Pass the normal through — not used for shading in this lesson,
     * but available for future lighting (Lesson 10). */
    output.normal = input.normal;

    return output;
}
