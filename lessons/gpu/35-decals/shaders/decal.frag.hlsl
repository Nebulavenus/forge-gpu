/*
 * Decal fragment shader — projects a decal texture onto scene geometry
 * using depth buffer reconstruction.
 *
 * Algorithm:
 *   1. Sample the scene depth at this screen pixel
 *   2. Reconstruct world-space position via inverse view-projection
 *   3. Project the world position into decal local space
 *   4. If outside the unit cube [-0.5, 0.5]^3, discard
 *   5. Map local XZ to decal texture UV
 *   6. Apply soft edge fade at box boundaries
 *
 * Fragment samplers (space2):
 *   slot 0 -> scene depth texture + nearest-clamp sampler
 *   slot 1 -> decal shape texture + linear-clamp sampler
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: decal transform and tint (160 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    depth_tex : register(t0, space2);
SamplerState depth_smp : register(s0, space2);

Texture2D    decal_tex : register(t1, space2);
SamplerState decal_smp : register(s1, space2);

cbuffer DecalFragUniforms : register(b0, space3)
{
    column_major float4x4 inv_vp;          /* inverse(proj * view)         */
    column_major float4x4 inv_decal_model; /* inverse of decal model matrix */
    float2 screen_size;                    /* viewport width, height        */
    float  near_plane;                     /* camera near plane             */
    float  far_plane;                      /* camera far plane              */
    float4 decal_tint;                     /* RGBA per-decal color          */
};

float4 main(float4 sv_pos : SV_Position) : SV_Target
{
    /* 1. Sample scene depth at this screen pixel */
    float2 screen_uv = sv_pos.xy / screen_size;
    float depth = depth_tex.Sample(depth_smp, screen_uv).r;

    /* Discard sky pixels (depth = 1.0 means nothing was rendered) */
    if (depth >= 0.9999)
        discard;

    /* 2. Reconstruct world position via inverse VP */
    float2 ndc = screen_uv * 2.0 - 1.0;
    ndc.y = -ndc.y;  /* Vulkan/SDL GPU Y-flip */

    float4 world_h = mul(inv_vp, float4(ndc, depth, 1.0));
    float3 world_pos = world_h.xyz / world_h.w;

    /* 3. Project into decal local space */
    float3 local = mul(inv_decal_model, float4(world_pos, 1.0)).xyz;

    /* 4. Box bounds check [-0.5, 0.5]^3 — discard if outside */
    if (any(abs(local) > 0.5))
        discard;

    /* 5. UV from local XZ, sample decal texture */
    float2 uv = local.xz + 0.5;
    float4 color = decal_tex.Sample(decal_smp, uv) * decal_tint;

    /* 6. Soft edge fade — smoothstep near cube boundaries */
    float fade = smoothstep(0.5, 0.4, abs(local.x))
               * smoothstep(0.5, 0.4, abs(local.y))
               * smoothstep(0.5, 0.4, abs(local.z));
    color.a *= fade;

    /* Discard nearly invisible fragments */
    if (color.a < 0.01)
        discard;

    return color;
}
