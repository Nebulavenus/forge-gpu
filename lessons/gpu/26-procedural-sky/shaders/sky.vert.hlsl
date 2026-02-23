/*
 * sky.vert.hlsl — Fullscreen quad with ray matrix reconstruction
 *
 * Draws a fullscreen quad via SV_VertexID and maps each vertex position
 * to a world-space view ray direction using a ray matrix built from
 * camera basis vectors and FOV.  The fragment shader uses these rays
 * to march through the atmosphere.
 *
 * The key idea: each screen pixel corresponds to a direction in world
 * space.  The ray matrix transforms NDC (nx, ny) into:
 *   ray_dir = nx * (aspect * tan(fov/2) * right)
 *           + ny * (tan(fov/2) * up)
 *           + forward
 *
 * This avoids the precision loss that occurs when using inverse VP
 * with planet-centric coordinates (camera at ~6360 km from origin).
 *
 * Six vertices form two triangles covering the screen:
 *   0--1    Triangle 1: 0, 1, 2
 *   | /|    Triangle 2: 2, 1, 3
 *   |/ |    (mapped to vertex IDs 0-5)
 *   2--3
 *
 * Uniform layout (64 bytes):
 *   float4x4 ray_matrix — maps NDC to world-space ray directions
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer SkyVertUniforms : register(b0, space1)
{
    float4x4 ray_matrix;  /* maps NDC to world-space ray directions */
};

struct VSOutput
{
    float4 clip_pos : SV_Position;  /* screen-space quad position      */
    float3 view_ray : TEXCOORD0;    /* world-space ray direction        */
    float2 uv       : TEXCOORD1;    /* UV for post-processing sampling  */
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Generate UV coordinates from vertex ID. */
    float2 uv;
    uv.x = (vertex_id == 1 || vertex_id == 4 || vertex_id == 5) ? 1.0 : 0.0;
    uv.y = (vertex_id == 2 || vertex_id == 3 || vertex_id == 5) ? 1.0 : 0.0;

    /* Flip V so texture row 0 (top) maps to NDC Y = 1 (top). */
    output.uv = float2(uv.x, 1.0 - uv.y);

    /* Map UV [0,1] to NDC [-1,1]. */
    float2 ndc = float2(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0);
    output.clip_pos = float4(ndc, 0.0, 1.0);

    /* Map NDC to world-space ray direction via the ray matrix.
     * The matrix encodes camera right/up/forward scaled by FOV,
     * so the result is the unnormalized world-space view direction.
     * The fragment shader normalizes it before ray marching. */
    float4 world_pos = mul(ray_matrix, float4(ndc.x, ndc.y, 1.0, 1.0));
    output.view_ray = world_pos.xyz / world_pos.w;

    return output;
}
