/*
 * scene_vert_raw.hlsl — Raw path vertex shader (32-byte stride)
 *
 * Transforms raw (unprocessed) vertices that lack tangent data.  Without
 * tangents, the fragment shader cannot perform normal mapping — it falls
 * back to the interpolated vertex normal for Blinn-Phong lighting.
 *
 * Per-vertex attributes (32-byte stride):
 *   TEXCOORD0 -> float3 position   (12 bytes, offset  0)
 *   TEXCOORD1 -> float3 normal     (12 bytes, offset 12)
 *   TEXCOORD2 -> float2 uv         ( 8 bytes, offset 24)
 *
 * Uniform buffer:
 *   register(b0, space1) -> MVP, model, and light VP matrices (192 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1) {
    column_major float4x4 mvp;
    column_major float4x4 model;
    column_major float4x4 light_vp;
};

struct VSInput {
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
};

struct VSOutput {
    float4 clip_pos     : SV_Position;
    float3 world_pos    : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float2 uv           : TEXCOORD2;
    float4 shadow_pos   : TEXCOORD3;
};

VSOutput main(VSInput input) {
    VSOutput output;

    /* Clip-space position for the rasterizer. */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space position for lighting and shadow calculations. */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* Light-space position for shadow mapping. */
    output.shadow_pos = mul(light_vp, wp);

    /* ── Normal transformation (adjugate transpose) ──────────────────
     * Use the cross product method to compute the adjugate transpose of
     * the upper-left 3x3.  This preserves perpendicularity even under
     * non-uniform scale.  See L10 for the derivation. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_normal = normalize(mul(adj_t, input.normal));

    output.uv = input.uv;

    return output;
}
