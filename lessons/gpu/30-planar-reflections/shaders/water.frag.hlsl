/*
 * Water fragment shader — Fresnel-blended reflective water surface.
 *
 * Projects the fragment position to screen-space UV and samples the
 * reflection texture.  Uses Schlick's Fresnel approximation to blend
 * between a water tint color and the reflection — at grazing angles the
 * surface is almost fully reflective; looking straight down reveals the
 * sandy floor beneath.
 *
 * SPDX-License-Identifier: Zlib
 */

/* Reflection texture (slot 0). */
Texture2D    reflection_tex : register(t0, space2);
SamplerState reflection_smp : register(s0, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float3 eye_pos;        /* camera world position      */
    float  water_level;    /* Y position of water plane  */
    float4 water_tint;     /* RGBA tint for water color  */
    float  fresnel_f0;     /* Fresnel reflectance at normal incidence */
    float  _pad[3];
};

float4 main(float4 clip_pos  : SV_Position,
             float3 world_pos : TEXCOORD0,
             float4 proj_pos  : TEXCOORD1) : SV_Target
{
    /* ── Screen-space UV from clip position ────────────────────────── */
    float2 screen_uv = proj_pos.xy / proj_pos.w;
    screen_uv = screen_uv * 0.5 + 0.5;
    screen_uv.y = 1.0 - screen_uv.y;

    /* ── Sample reflection texture ────────────────────────────────── */
    float3 reflection = reflection_tex.Sample(reflection_smp, screen_uv).rgb;

    /* ── Schlick Fresnel ──────────────────────────────────────────── */
    float3 N = float3(0.0, 1.0, 0.0); /* water surface normal (flat) */
    float3 V = normalize(eye_pos - world_pos);
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = fresnel_f0 + (1.0 - fresnel_f0) * pow(1.0 - NdotV, 5.0);

    /* ── Blend reflection with water tint ─────────────────────────── */
    float3 color = lerp(water_tint.rgb, reflection, fresnel);

    /* Alpha: steep angles (low fresnel) see through to the floor;
     * grazing angles (high fresnel) are fully opaque reflection. */
    float alpha = lerp(0.6, 1.0, fresnel);

    return float4(color, alpha);
}
