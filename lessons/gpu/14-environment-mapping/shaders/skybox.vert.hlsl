/*
 * skybox.vert.hlsl — Vertex shader for skybox cube
 *
 * Transforms a unit cube so it always surrounds the camera.  The view
 * matrix has its translation stripped so the skybox follows camera rotation
 * only — the camera can never "move through" the skybox.
 *
 * The output position is set to (x, y, w, w) so that after perspective
 * divide the depth is always 1.0 (the far plane).  This means every
 * other object drawn with depth < 1 will appear in front of the skybox.
 *
 * The vertex position doubles as the cube map sample direction — the
 * rasterizer interpolates it across the face and the fragment shader
 * passes it directly to TextureCube.Sample().
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 vp_no_translation;  /* View (rotation only) * Projection */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* Cube vertex position [-1..1] */
};

struct VSOutput
{
    float4 clip_pos  : SV_Position;  /* Clip-space position for rasterizer */
    float3 direction : TEXCOORD0;    /* Cube map sample direction */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Pass the raw vertex position as the cube map sample direction.
     * The rasterizer interpolates this across each face, giving each
     * fragment a direction pointing outward from the cube center. */
    output.direction = input.position;

    /* Transform through rotation-only VP */
    float4 pos = mul(vp_no_translation, float4(input.position, 1.0));

    /* Set z = w so depth is always 1.0 after perspective divide (w/w = 1).
     * This places the skybox at the far plane — everything else draws
     * in front of it. */
    output.clip_pos = pos.xyww;

    return output;
}
