/*
 * grid.frag.hlsl — Procedural grid with shadow receiving
 *
 * Extends L12/L13's procedural grid with shadow map sampling.  The grid
 * floor receives shadows from scene objects, making the shadow effect
 * clearly visible on a flat surface.
 *
 * The grid algorithm is identical to L12 (fwidth anti-aliasing, distance
 * fade, Blinn-Phong).  The new addition is cascade selection and PCF
 * shadow sampling, matching the scene fragment shader's approach.
 *
 * Fragment samplers:
 *   register(t0/s0, space2) -> shadow map cascade 0 (slot 0)
 *   register(t1/s1, space2) -> shadow map cascade 1 (slot 1)
 *   register(t2/s2, space2) -> shadow map cascade 2 (slot 2)
 *
 * Fragment uniform:
 *   register(b0, space3) -> grid appearance + shadow params (112 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    shadow_map0 : register(t0, space2);
SamplerState shadow_smp0 : register(s0, space2);

Texture2D    shadow_map1 : register(t1, space2);
SamplerState shadow_smp1 : register(s1, space2);

Texture2D    shadow_map2 : register(t2, space2);
SamplerState shadow_smp2 : register(s2, space2);

cbuffer GridFragUniforms : register(b0, space3)
{
    float4 line_color;        /* grid line color                              */
    float4 bg_color;          /* background surface color                     */
    float4 light_dir;         /* world-space light direction (toward light)   */
    float4 eye_pos;           /* world-space camera position                  */
    float4 cascade_splits;    /* cascade far distances (x=c0, y=c1, z=c2)    */
    float  grid_spacing;      /* world units between lines                    */
    float  line_width;        /* line thickness in grid-space [0..0.5]        */
    float  fade_distance;     /* distance where grid fades to background      */
    float  ambient;           /* ambient light intensity [0..1]               */
    float  shininess;         /* specular exponent                            */
    float  specular_str;      /* specular intensity [0..1]                    */
    float  shadow_texel_size; /* 1.0 / shadow_map_resolution                 */
    float  shadow_bias;       /* depth comparison bias                        */
};

struct PSInput
{
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float4 light_pos0 : TEXCOORD1;
    float4 light_pos1 : TEXCOORD2;
    float4 light_pos2 : TEXCOORD3;
};

/* 3x3 PCF shadow sampling — same algorithm as scene.frag.hlsl */
float sample_shadow_pcf(Texture2D shadow_map, SamplerState smp,
                         float2 shadow_uv, float current_depth)
{
    float shadow = 0.0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float2 offset = float2((float)x, (float)y) * shadow_texel_size;
            float map_depth = shadow_map.Sample(smp, shadow_uv + offset).r;
            shadow += (map_depth >= current_depth - shadow_bias) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

float4 main(PSInput input) : SV_Target
{
    /* ── Grid computation (identical to L12/L13) ────────────────────── */
    float2 grid_uv = input.world_pos.xz / grid_spacing;
    float2 dist = abs(frac(grid_uv - 0.5) - 0.5);
    float2 fw = fwidth(grid_uv);
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);
    float grid = max(aa_line.x, aa_line.y);

    /* Distance fade to prevent moire at the horizon */
    float cam_dist = length(input.world_pos - eye_pos.xyz);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);

    /* ── Shadow computation ──────────────────────────────────────────── */
    float shadow_factor = 1.0;
    if (cam_dist < cascade_splits.x)
    {
        float3 proj = input.light_pos0.xyz / input.light_pos0.w;
        float2 shadow_uv = proj.xy * 0.5 + 0.5;
        shadow_uv.y = 1.0 - shadow_uv.y;
        shadow_factor = sample_shadow_pcf(shadow_map0, shadow_smp0,
                                           shadow_uv, proj.z);
    }
    else if (cam_dist < cascade_splits.y)
    {
        float3 proj = input.light_pos1.xyz / input.light_pos1.w;
        float2 shadow_uv = proj.xy * 0.5 + 0.5;
        shadow_uv.y = 1.0 - shadow_uv.y;
        shadow_factor = sample_shadow_pcf(shadow_map1, shadow_smp1,
                                           shadow_uv, proj.z);
    }
    else if (cam_dist < cascade_splits.z)
    {
        float3 proj = input.light_pos2.xyz / input.light_pos2.w;
        float2 shadow_uv = proj.xy * 0.5 + 0.5;
        shadow_uv.y = 1.0 - shadow_uv.y;
        shadow_factor = sample_shadow_pcf(shadow_map2, shadow_smp2,
                                           shadow_uv, proj.z);
    }

    /* ── Blinn-Phong with shadows ───────────────────────────────────── */
    float3 N = float3(0.0, 1.0, 0.0);  /* Grid normal is always up */
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    float NdotL = max(dot(N, L), 0.0);

    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess) *
                            float3(1.0, 1.0, 1.0);

    /* Ambient is unaffected by shadow; diffuse and specular are modulated */
    float3 lit = ambient * surface +
                 shadow_factor * NdotL * surface +
                 shadow_factor * specular_term;

    return float4(lit, 1.0);
}
