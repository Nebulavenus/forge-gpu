/*
 * scene.vert.hlsl — Vertex shader for scene objects with shadow receiving
 *
 * Transforms vertices to clip space and computes 3 light-space positions
 * (one per cascade) for shadow map sampling in the fragment shader.
 *
 * Each cascade has its own light View-Projection matrix that maps
 * world-space positions into the cascade's shadow map UV + depth.
 * The fragment shader picks the appropriate cascade based on distance
 * from the camera.
 *
 * Vertex attributes (ForgeGltfVertex):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: MVP + model matrix (128 bytes)
 *   register(b1, space1) -> slot 1: 3 light VP matrices (192 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer SceneVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;    /* proj * view * model */
    column_major float4x4 model;  /* model-to-world transform */
};

cbuffer ShadowMatrices : register(b1, space1)
{
    column_major float4x4 light_vp[3];  /* one per cascade */
};

struct VSInput
{
    float3 position : TEXCOORD0;  /* object-space position */
    float3 normal   : TEXCOORD1;  /* object-space normal   */
    float2 uv       : TEXCOORD2;  /* texture coordinates   */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position;  /* clip-space position         */
    float2 uv         : TEXCOORD0;    /* texture coordinates         */
    float3 world_norm : TEXCOORD1;    /* world-space normal          */
    float3 world_pos  : TEXCOORD2;    /* world-space position        */
    float4 light_pos0 : TEXCOORD3;    /* light-space pos, cascade 0  */
    float4 light_pos1 : TEXCOORD4;    /* light-space pos, cascade 1  */
    float4 light_pos2 : TEXCOORD5;    /* light-space pos, cascade 2  */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* World-space position — needed for lighting and shadow lookup */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* Clip-space position for rasterization */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space normal via adjugate transpose of model's 3x3.
     * This handles non-uniform scale correctly (same method as L10/L13). */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_norm = mul(adj_t, input.normal);

    /* Pass UVs through for texture sampling */
    output.uv = input.uv;

    /* Compute light-space positions for each cascade.
     * Each light_vp[i] maps world space → cascade i's shadow map clip space.
     * The fragment shader will do perspective divide and convert to UV. */
    output.light_pos0 = mul(light_vp[0], wp);
    output.light_pos1 = mul(light_vp[1], wp);
    output.light_pos2 = mul(light_vp[2], wp);

    return output;
}
