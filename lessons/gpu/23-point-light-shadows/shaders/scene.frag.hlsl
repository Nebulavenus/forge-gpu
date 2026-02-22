/*
 * Scene fragment shader — Blinn-Phong with multiple point lights and
 * omnidirectional shadow mapping via cube maps.
 *
 * Each point light has an associated shadow cube map storing linear depth
 * (distance / far_plane). To determine if a fragment is in shadow, we
 * compute the light-to-fragment vector, sample the cube map in that
 * direction, and compare the stored depth against the actual distance.
 */

#define MAX_POINT_LIGHTS 4

/* Shadow comparison bias — prevents self-shadowing (shadow acne).
 * Tuned for 512x512 cube maps; increase for lower resolution. */
#define SHADOW_BIAS 0.002

/* Quadratic attenuation coefficients for point light falloff.
 * Models inverse-square decay: 1 / (1 + kL*d + kQ*d^2). */
#define ATTEN_LINEAR    0.35
#define ATTEN_QUADRATIC 0.44

struct PointLight
{
    float3 position;     /* world-space position   */
    float  intensity;    /* HDR brightness scalar  */
    float3 color;        /* RGB light color        */
    float  _pad;         /* align to 32 bytes      */
};

/* Diffuse texture (slot 0). */
Texture2D    diffuse_tex : register(t0, space2);
SamplerState diffuse_smp : register(s0, space2);

/* Shadow cube maps (slots 1-4). */
TextureCube  shadow_cube0 : register(t1, space2);
SamplerState shadow_smp0  : register(s1, space2);
TextureCube  shadow_cube1 : register(t2, space2);
SamplerState shadow_smp1  : register(s2, space2);
TextureCube  shadow_cube2 : register(t3, space2);
SamplerState shadow_smp2  : register(s3, space2);
TextureCube  shadow_cube3 : register(t4, space2);
SamplerState shadow_smp3  : register(s4, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;                     /* material RGBA            (16) */
    float3 eye_pos;                        /* camera position          (12) */
    float  has_texture;                    /* > 0.5 = textured          (4) */
    float  shininess;                      /* specular exponent         (4) */
    float  ambient;                        /* ambient intensity         (4) */
    float  specular_str;                   /* specular strength         (4) */
    float  shadow_far_plane;               /* shadow map far plane      (4) */
    PointLight lights[MAX_POINT_LIGHTS];   /* point light array       (128) */
};

/* Sample shadow for a given light index.
 * Returns 1.0 if lit, 0.0 if in shadow. */
float sample_shadow(int light_index, float3 light_to_frag)
{
    float current_depth = length(light_to_frag) / shadow_far_plane;
    float bias = SHADOW_BIAS;

    float stored_depth;
    if (light_index == 0)
        stored_depth = shadow_cube0.Sample(shadow_smp0, light_to_frag).r;
    else if (light_index == 1)
        stored_depth = shadow_cube1.Sample(shadow_smp1, light_to_frag).r;
    else if (light_index == 2)
        stored_depth = shadow_cube2.Sample(shadow_smp2, light_to_frag).r;
    else
        stored_depth = shadow_cube3.Sample(shadow_smp3, light_to_frag).r;

    return (current_depth - bias > stored_depth) ? 0.0 : 1.0;
}

float4 main(float4 clip_pos : SV_Position,
            float3 world_pos : TEXCOORD0,
            float3 world_nrm : TEXCOORD1,
            float2 uv        : TEXCOORD2) : SV_Target
{
    /* ── Surface color ──────────────────────────────────────────────── */
    float4 albedo = base_color;
    if (has_texture > 0.5)
    {
        albedo *= diffuse_tex.Sample(diffuse_smp, uv);
    }

    /* ── Lighting ───────────────────────────────────────────────────── */
    float3 N = normalize(world_nrm);
    float3 V = normalize(eye_pos - world_pos);

    /* Ambient term */
    float3 total_light = albedo.rgb * ambient;

    for (int i = 0; i < MAX_POINT_LIGHTS; i++)
    {
        float intensity = lights[i].intensity;
        if (intensity <= 0.0) continue;

        float3 L = lights[i].position - world_pos;
        float  d = length(L);
        L /= d; /* normalize */

        /* Quadratic attenuation */
        float attenuation = 1.0 / (1.0 + ATTEN_LINEAR * d + ATTEN_QUADRATIC * d * d);

        /* Diffuse */
        float NdotL = max(dot(N, L), 0.0);
        float3 diffuse = albedo.rgb * NdotL;

        /* Specular (Blinn-Phong) */
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float3 spec = specular_str * pow(NdotH, shininess);

        /* Shadow factor — 1.0 if lit, 0.0 if occluded */
        float3 light_to_frag = world_pos - lights[i].position;
        float shadow = sample_shadow(i, light_to_frag);

        total_light += (diffuse + spec) * shadow * attenuation * intensity * lights[i].color;
    }

    return float4(total_light, albedo.a);
}
