/*
 * scene.frag.hlsl — Blinn-Phong lighting with cascaded shadow maps
 *
 * Combines the lighting model from L10/L13 with shadow mapping:
 *   1. Select the appropriate cascade based on distance from camera
 *   2. Transform the fragment's light-space position to shadow map UV
 *   3. Sample the shadow map with 3x3 PCF (Percentage Closer Filtering)
 *   4. Modulate the diffuse and specular terms by the shadow factor
 *
 * PCF (Percentage Closer Filtering):
 *   Instead of a single hard shadow/lit test, we sample a 3x3 grid of
 *   neighboring texels in the shadow map and average the results.  This
 *   produces soft shadow edges that look much more natural than binary
 *   hard shadows.
 *
 * Cascade selection:
 *   The view frustum is split into 3 depth ranges (cascades).  Near
 *   objects use cascade 0 (highest resolution), mid-range objects use
 *   cascade 1, and far objects use cascade 2 (lowest resolution).
 *   Selection is based on world-space distance from the camera.
 *
 * Fragment samplers:
 *   register(t0/s0, space2) -> diffuse texture (slot 0)
 *   register(t1/s1, space2) -> shadow map cascade 0 (slot 1)
 *   register(t2/s2, space2) -> shadow map cascade 1 (slot 2)
 *   register(t3/s3, space2) -> shadow map cascade 2 (slot 3)
 *
 * Fragment uniform:
 *   register(b0, space3) -> lighting + shadow parameters (96 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex  : register(t0, space2);
SamplerState diffuse_smp  : register(s0, space2);

Texture2D    shadow_map0  : register(t1, space2);
SamplerState shadow_smp0  : register(s1, space2);

Texture2D    shadow_map1  : register(t2, space2);
SamplerState shadow_smp1  : register(s2, space2);

Texture2D    shadow_map2  : register(t3, space2);
SamplerState shadow_smp2  : register(s3, space2);

cbuffer SceneFragUniforms : register(b0, space3)
{
    float4 base_color;       /* material base color (RGBA)                   */
    float4 light_dir;        /* world-space light direction (toward light)   */
    float4 eye_pos;          /* world-space camera position                  */
    float4 cascade_splits;   /* cascade far distances (x=c0, y=c1, z=c2)    */
    uint   has_texture;      /* non-zero = sample diffuse texture            */
    float  shininess;        /* specular exponent                            */
    float  ambient;          /* ambient light intensity [0..1]               */
    float  specular_str;     /* specular intensity [0..1]                    */
    float  shadow_texel_size;/* 1.0 / shadow_map_resolution                 */
    float  shadow_bias;      /* depth comparison bias to prevent acne        */
    float  _pad0;
    float  _pad1;
};

struct PSInput
{
    float4 clip_pos   : SV_Position;
    float2 uv         : TEXCOORD0;
    float3 world_norm : TEXCOORD1;
    float3 world_pos  : TEXCOORD2;
    float4 light_pos0 : TEXCOORD3;
    float4 light_pos1 : TEXCOORD4;
    float4 light_pos2 : TEXCOORD5;
};

/* Sample a shadow map with 3x3 PCF for soft shadow edges.
 * Returns 0.0 (fully shadowed) to 1.0 (fully lit).
 *
 * The 3x3 kernel samples 9 points around the fragment's shadow map
 * position and averages whether each is lit or shadowed.  This blurs
 * the shadow boundary by roughly 1.5 texels in each direction. */
float sample_shadow_pcf(Texture2D shadow_map, SamplerState smp,
                         float2 shadow_uv, float current_depth)
{
    float shadow = 0.0;

    /* 3x3 grid: offsets from -1 to +1 texels in each axis */
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float2 offset = float2((float)x, (float)y) * shadow_texel_size;
            float map_depth = shadow_map.Sample(smp, shadow_uv + offset).r;

            /* Compare: if the shadow map depth is greater than or equal to
             * our depth (minus bias), the fragment is lit.  The bias prevents
             * shadow acne — a self-shadowing artifact caused by limited
             * depth precision. */
            shadow += (map_depth >= current_depth - shadow_bias) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;  /* Average of 9 samples */
}

float4 main(PSInput input) : SV_Target
{
    /* ── Surface color ──────────────────────────────────────────────── */
    float4 surface_color;
    if (has_texture)
    {
        surface_color = diffuse_tex.Sample(diffuse_smp, input.uv) * base_color;
    }
    else
    {
        surface_color = base_color;
    }

    /* ── Cascade selection ──────────────────────────────────────────── */
    /* Pick the tightest cascade that contains this fragment.
     * Distance from camera determines which cascade to use — closer
     * fragments get higher-resolution shadow maps. */
    float dist = length(input.world_pos - eye_pos.xyz);

    float shadow_factor = 1.0;
    if (dist < cascade_splits.x)
    {
        /* Cascade 0 — nearest, highest resolution */
        float3 proj = input.light_pos0.xyz / input.light_pos0.w;
        float2 shadow_uv = proj.xy * 0.5 + 0.5;
        shadow_uv.y = 1.0 - shadow_uv.y;  /* Flip Y: NDC to UV */
        shadow_factor = sample_shadow_pcf(shadow_map0, shadow_smp0,
                                           shadow_uv, proj.z);
    }
    else if (dist < cascade_splits.y)
    {
        /* Cascade 1 — mid-range */
        float3 proj = input.light_pos1.xyz / input.light_pos1.w;
        float2 shadow_uv = proj.xy * 0.5 + 0.5;
        shadow_uv.y = 1.0 - shadow_uv.y;
        shadow_factor = sample_shadow_pcf(shadow_map1, shadow_smp1,
                                           shadow_uv, proj.z);
    }
    else if (dist < cascade_splits.z)
    {
        /* Cascade 2 — farthest, lowest resolution */
        float3 proj = input.light_pos2.xyz / input.light_pos2.w;
        float2 shadow_uv = proj.xy * 0.5 + 0.5;
        shadow_uv.y = 1.0 - shadow_uv.y;
        shadow_factor = sample_shadow_pcf(shadow_map2, shadow_smp2,
                                           shadow_uv, proj.z);
    }
    /* Beyond all cascades: fully lit (no shadow data available) */

    /* ── Blinn-Phong lighting ───────────────────────────────────────── */
    float3 N = normalize(input.world_norm);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* Ambient — constant base brightness unaffected by shadows */
    float3 ambient_term = ambient * surface_color.rgb;

    /* Diffuse — modulated by shadow factor */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = shadow_factor * NdotL * surface_color.rgb;

    /* Specular — also modulated by shadow (no highlights in shadow) */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = shadow_factor * specular_str *
                            pow(NdotH, shininess) * float3(1.0, 1.0, 1.0);

    float3 final_color = ambient_term + diffuse_term + specular_term;
    return float4(final_color, surface_color.a);
}
