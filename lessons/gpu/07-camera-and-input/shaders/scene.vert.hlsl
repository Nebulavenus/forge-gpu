/*
 * scene.vert.hlsl — Vertex shader for a 3D scene with MVP transform
 *
 * Same as Lesson 06's cube shader — transforms 3D vertices from object
 * space to clip space using the Model-View-Projection matrix.  Now the
 * view matrix changes every frame as the camera moves and rotates.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 color     (location 1)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains the combined MVP matrix (64 bytes).
 *
 * SPDX-License-Identifier: Zlib
 */

/* The MVP matrix is composed on the CPU each frame:
 *   mvp = projection * view * model
 * The VIEW matrix now comes from a first-person camera that the user
 * controls with keyboard and mouse — rebuilt every frame. */
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

    /* Transform from object space to clip space via the MVP matrix.
     * The key difference from Lesson 06: the view matrix is no longer
     * fixed — it's rebuilt each frame from the camera's position and
     * quaternion orientation (see Math Lesson 09). */
    output.position = mul(mvp, float4(input.position, 1.0));

    /* Pass vertex color through for interpolation. */
    output.color = float4(input.color, 1.0);

    return output;
}
