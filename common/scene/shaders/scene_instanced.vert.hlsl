/*
 * scene_instanced.vert.hlsl — Instanced mesh vertex shader
 *
 * Part of forge_scene.h — transforms instanced position + normal vertices to
 * clip, world, and light space.  The model matrix arrives as a per-instance
 * vertex attribute (4 × float4 columns) instead of a uniform, allowing one
 * draw call to place many copies of the same mesh at different transforms.
 *
 * Must produce the same VSOutput layout as scene.vert.hlsl so that
 * scene.frag.hlsl can be reused without modification.
 *
 * Per-vertex attributes (slot 0, VERTEX input rate, 24-byte stride):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *
 * Per-instance attributes (slot 1, INSTANCE input rate, 64-byte stride):
 *   TEXCOORD2 -> float4 model_c0  (location 2) — model matrix column 0
 *   TEXCOORD3 -> float4 model_c1  (location 3) — model matrix column 1
 *   TEXCOORD4 -> float4 model_c2  (location 4) — model matrix column 2
 *   TEXCOORD5 -> float4 model_c3  (location 5) — model matrix column 3
 *
 * Uniform buffer:
 *   register(b0, space1) -> VP + light VP + node world matrices (192 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer InstancedVertUniforms : register(b0, space1)
{
    column_major float4x4 vp;         /* camera view-projection matrix     */
    column_major float4x4 light_vp;   /* light view-projection matrix      */
    column_major float4x4 node_world; /* per-node local-to-model transform */
};

struct VSInput
{
    /* Per-vertex data (slot 0) */
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;

    /* Per-instance data (slot 1) — model matrix as 4 columns */
    float4 model_c0 : TEXCOORD2;
    float4 model_c1 : TEXCOORD3;
    float4 model_c2 : TEXCOORD4;
    float4 model_c3 : TEXCOORD5;
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer  */
    float3 world_pos  : TEXCOORD0;   /* world-space position for lighting   */
    float3 world_nrm  : TEXCOORD1;   /* world-space normal for lighting     */
    float4 light_clip : TEXCOORD2;   /* light-space position for shadow map */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Reconstruct model matrix from per-instance columns.
     * float4x4(c0,c1,c2,c3) treats arguments as rows in HLSL, so we
     * transpose to get column-major — matching forge_math.h mat4 layout. */
    float4x4 model = transpose(float4x4(
        input.model_c0,
        input.model_c1,
        input.model_c2,
        input.model_c3
    ));

    /* World-space position: instance_model * node_world * local_pos.
     * node_world applies the glTF node hierarchy transform (identity for
     * single-mesh models).  The instance matrix places the whole model. */
    float4 local = mul(node_world, float4(input.position, 1.0));
    float4 world = mul(model, local);
    output.world_pos = world.xyz;

    /* Clip-space position. */
    output.clip_pos = mul(vp, world);

    /* World-space normal: apply combined model * node_world via adjugate
     * transpose — preserves perpendicularity under non-uniform scale. */
    float3x3 combined = mul((float3x3)model, (float3x3)node_world);
    float3x3 adj_t;
    adj_t[0] = cross(combined[1], combined[2]);
    adj_t[1] = cross(combined[2], combined[0]);
    adj_t[2] = cross(combined[0], combined[1]);
    output.world_nrm = normalize(mul(adj_t, input.normal));

    /* Light-space position for shadow mapping.
     * light_vp is the raw light view-projection (NOT pre-multiplied with
     * model), so we transform world-space position. */
    output.light_clip = mul(light_vp, world);

    return output;
}
