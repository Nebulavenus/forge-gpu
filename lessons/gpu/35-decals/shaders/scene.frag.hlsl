/*
 * Scene fragment shader — Blinn-Phong lighting with shadow map.
 *
 * Computes directional lighting using the Blinn-Phong reflection model,
 * samples a shadow map with 2x2 PCF for soft edges.
 *
 * Fragment samplers (space2):
 *   slot 0 -> shadow depth texture + nearest-clamp sampler
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: material and lighting parameters
 *
 * SPDX-License-Identifier: Zlib
 */

/* Shadow bias — prevents self-shadowing (shadow acne). */
#define SHADOW_BIAS 0.005

/* Shadow map resolution — must match SHADOW_MAP_SIZE in main.c. */
#define SHADOW_MAP_RES 2048.0

/* Number of PCF filter samples for soft shadow edges. */
#define PCF_SAMPLES 4

/* Shadow depth map (slot 0). */
Texture2D    shadow_tex : register(t0, space2);
SamplerState shadow_smp : register(s0, space2);

cbuffer SceneFragUniforms : register(b0, space3)
{
    float4 base_color;      /* material RGBA                              */
    float3 eye_pos;         /* camera world-space position                */
    float  ambient;         /* ambient light intensity [0..1]             */
    float4 light_dir;       /* xyz = directional light direction          */
    float3 light_color;     /* directional light RGB color                */
    float  light_intensity; /* directional light brightness               */
    float  shininess;       /* specular exponent (Blinn-Phong)            */
    float  specular_str;    /* specular strength multiplier               */
    float2 _pad0;
};

/* ── Shadow sampling with 2x2 PCF ──────────────────────────────────── */

float sample_shadow(float4 light_clip)
{
    float3 light_ndc = light_clip.xyz / light_clip.w;

    /* Map NDC [-1,1] to UV [0,1].  Y is flipped for texture coordinates. */
    float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;

    /* Fragments outside the shadow map bounds are fully lit. */
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0)
        return 1.0;

    float current_depth = light_ndc.z;
    float2 texel_size = float2(1.0 / SHADOW_MAP_RES, 1.0 / SHADOW_MAP_RES);

    /* 2x2 PCF — sample neighboring texels for soft shadow edges. */
    float shadow = 0.0;
    float2 offsets[PCF_SAMPLES] = {
        float2(-0.5, -0.5),
        float2( 0.5, -0.5),
        float2(-0.5,  0.5),
        float2( 0.5,  0.5)
    };

    for (int i = 0; i < PCF_SAMPLES; i++)
    {
        float stored = shadow_tex.Sample(shadow_smp,
            shadow_uv + offsets[i] * texel_size).r;
        shadow += (current_depth - SHADOW_BIAS <= stored) ? 1.0 : 0.0;
    }

    return shadow / (float)PCF_SAMPLES;
}

float4 main(float4 clip_pos   : SV_Position,
             float3 world_pos  : TEXCOORD0,
             float3 world_nrm  : TEXCOORD1,
             float4 light_clip : TEXCOORD2) : SV_Target
{
    /* ── Surface color ──────────────────────────────────────────────── */
    float4 albedo = base_color;

    /* ── Common vectors ─────────────────────────────────────────────── */
    float3 N = normalize(world_nrm);
    float3 V = normalize(eye_pos - world_pos);

    /* ── Ambient term ─────────────────────────────────────────────── */
    float3 total_light = albedo.rgb * ambient;

    /* ── Directional light with shadow ─────────────────────────────── */
    {
        float3 L = normalize(-light_dir.xyz);
        float NdotL = max(dot(N, L), 0.0);
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float3 spec = specular_str * pow(NdotH, shininess);

        float shadow = sample_shadow(light_clip);
        total_light += (albedo.rgb * NdotL + spec) * light_intensity *
                       light_color * shadow;
    }

    return float4(total_light, albedo.a);
}
