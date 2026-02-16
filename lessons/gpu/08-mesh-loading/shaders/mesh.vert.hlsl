/*
 * mesh.vert.hlsl — Vertex shader for textured 3D mesh rendering
 *
 * Transforms mesh vertices from object space to clip space using the
 * combined Model-View-Projection matrix, and passes UV coordinates
 * through to the fragment shader for texture sampling.
 *
 * Vertex attributes (from ForgeObjVertex layout):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains the combined MVP matrix (64 bytes).
 *
 * The normal is accepted as input but not passed to the fragment shader
 * in this lesson — we only need UVs for diffuse texture mapping.
 * Lesson 10 (Basic Lighting) will make use of the normal vector.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer Uniforms : register(b0, space1)
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
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Transform from object space to clip space via the MVP matrix.
     * The model matrix positions/rotates the mesh in the world,
     * the view matrix is the camera (Lesson 07), and the projection
     * matrix creates perspective (Lesson 06). */
    output.position = mul(mvp, float4(input.position, 1.0));

    /* Pass UV coordinates through for texture sampling.  The rasterizer
     * will interpolate these across each triangle's surface. */
    output.uv = input.uv;

    return output;
}
