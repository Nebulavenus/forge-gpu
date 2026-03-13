/*
 * scene_model.frag.hlsl — Model fragment shader with PBR data + Blinn-Phong
 *
 * Part of forge_scene.h — textured Blinn-Phong lighting with normal mapping,
 * PCF 3x3 shadow, and full PBR material data (base_color, metallic, roughness,
 * occlusion, emissive).  The PBR data is sampled and applied through a
 * Blinn-Phong approximation — swapping to PBR later changes only the
 * lighting math, not the data pipeline.
 *
 * Texture/sampler bindings (space2):
 *   slot 0 -> base color (sRGB)
 *   slot 1 -> normal map (linear)
 *   slot 2 -> metallic-roughness (linear, G=roughness B=metallic)
 *   slot 3 -> occlusion (linear, R channel)
 *   slot 4 -> emissive (sRGB)
 *   slot 5 -> shadow map (depth, comparison sampler)
 *
 * Uniform buffer:
 *   register(b0, space3) -> material + lighting parameters (96 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Shadow comparison bias — applied before hardware depth comparison.
 * Uses a comparison sampler (SampleCmpLevelZero) rather than the manual
 * compare in scene.frag.hlsl and grid.frag.hlsl.  Kept as a define so
 * the bias can be tuned without touching the cbuffer layout. */
#define SHADOW_BIAS 0.005

Texture2D    base_color_tex : register(t0, space2);
SamplerState base_color_smp : register(s0, space2);

Texture2D    normal_tex     : register(t1, space2);
SamplerState normal_smp     : register(s1, space2);

Texture2D    mr_tex         : register(t2, space2);
SamplerState mr_smp         : register(s2, space2);

Texture2D    occlusion_tex  : register(t3, space2);
SamplerState occlusion_smp  : register(s3, space2);

Texture2D    emissive_tex   : register(t4, space2);
SamplerState emissive_smp   : register(s4, space2);

Texture2D              shadow_map     : register(t5, space2);
SamplerComparisonState shadow_sampler : register(s5, space2);

cbuffer FragUniforms : register(b0, space3) {
    float4 light_dir;           /* xyz = direction toward light              */
    float4 eye_pos;             /* xyz = camera position                     */
    float4 base_color_factor;   /* RGBA multiplier from material             */
    float3 emissive_factor;     /* RGB emission multiplier                   */
    float  shadow_texel;        /* 1.0 / shadow_map_resolution              */
    float  metallic_factor;     /* 0 = dielectric, 1 = metal                */
    float  roughness_factor;    /* 0 = mirror, 1 = rough                    */
    float  normal_scale;        /* normal map XY intensity                   */
    float  occlusion_strength;  /* AO blend: 0 = none, 1 = full             */
    float  shininess;           /* Blinn-Phong specular exponent             */
    float  specular_str;        /* specular intensity multiplier             */
    float  alpha_cutoff;        /* MASK mode threshold                       */
    float  ambient;             /* ambient light intensity [0..1]            */
};

struct PSInput {
    float4 clip_pos      : SV_Position;
    float3 world_pos     : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float2 uv            : TEXCOORD2;
    float3 world_tangent : TEXCOORD3;
    float3 world_bitan   : TEXCOORD4;
    float4 shadow_pos    : TEXCOORD5;
};

/* PCF 3x3 shadow sampling with comparison sampler. */
float compute_shadow(float4 light_clip) {
    float3 ndc = light_clip.xyz / light_clip.w;
    float2 shadow_uv = ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;   /* Vulkan Y-flip */
    float depth = ndc.z;

    /* Fragments outside the shadow map receive full light. */
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
        depth < 0.0 || depth > 1.0)
        return 1.0;

    float shadow = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++) {
        [unroll]
        for (int x = -1; x <= 1; x++) {
            shadow += shadow_map.SampleCmpLevelZero(
                shadow_sampler, shadow_uv + float2(x, y) * shadow_texel,
                depth - SHADOW_BIAS);
        }
    }
    return shadow / 9.0;
}

float4 main(PSInput input) : SV_Target {
    /* ── Base color from texture × factor ────────────────────────── */
    float4 base = base_color_tex.Sample(base_color_smp, input.uv);
    base *= base_color_factor;

    /* ── Alpha test (MASK mode uses alpha_cutoff > 0) ───────────── */
    if (alpha_cutoff > 0.0 && base.a < alpha_cutoff)
        discard;

    /* ── Reconstruct TBN basis (re-normalize after interpolation) ─ */
    float3 N = normalize(input.world_normal);
    float3 T = normalize(input.world_tangent);
    float3 B = normalize(input.world_bitan);

    /* ── Sample and decode the normal map ────────────────────────── */
    float3 map_normal = normal_tex.Sample(normal_smp, input.uv).rgb;
    map_normal = map_normal * 2.0 - 1.0;
    map_normal.xy *= normal_scale;
    map_normal = normalize(map_normal);

    /* TBN transformation: tangent space -> world space */
    float3x3 TBN = float3x3(T, B, N);
    N = normalize(mul(map_normal, TBN));

    /* ── Sample PBR textures ────────────────────────────────────── */
    float2 mr = mr_tex.Sample(mr_smp, input.uv).bg;  /* B=metallic, G=roughness */
    float metallic  = mr.x * metallic_factor;
    float roughness = mr.y * roughness_factor;

    float ao = occlusion_tex.Sample(occlusion_smp, input.uv).r;
    ao = 1.0 + occlusion_strength * (ao - 1.0);  /* lerp(1.0, ao, strength) */

    float3 emissive = emissive_tex.Sample(emissive_smp, input.uv).rgb;
    emissive *= emissive_factor;

    /* ── Lighting vectors ────────────────────────────────────────── */
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* ── Blinn-Phong with PBR approximation ─────────────────────── */
    float NdotL = max(dot(N, L), 0.0);
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);

    /* Metallic darkens diffuse (metals absorb light, reflect specularly) */
    float3 diffuse_color = base.rgb * (1.0 - metallic * 0.7);

    /* Roughness modulates specular exponent: rougher = wider highlight */
    float spec_exp = shininess * max(1.0 - roughness, 0.05);

    float3 ambient_term  = ambient * diffuse_color * ao;
    float3 diffuse_term  = NdotL * diffuse_color;
    float3 specular_term = specular_str * pow(NdotH, spec_exp)
                         * lerp(float3(1, 1, 1), base.rgb, metallic);

    /* ── Shadow ──────────────────────────────────────────────────── */
    float shadow = compute_shadow(input.shadow_pos);

    float3 final_color = ambient_term
                       + shadow * (diffuse_term + specular_term)
                       + emissive;

    return float4(final_color, base.a);
}
