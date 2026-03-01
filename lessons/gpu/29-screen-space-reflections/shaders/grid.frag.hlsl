/*
 * Grid fragment shader — procedural anti-aliased grid lines with
 * directional light and shadow mapping. Outputs to 3 MRT targets for SSR.
 *
 * The grid floor acts as the primary reflective surface in this lesson.
 * Its reflectivity is stored in the alpha channel of the world-position
 * target so the SSR pass knows which pixels should receive reflections.
 *
 * MRT output:
 *   SV_Target0 — lit grid color      (R8G8B8A8_UNORM)
 *   SV_Target1 — view-space normal   (R16G16B16A16_FLOAT)
 *   SV_Target2 — world-space position (R16G16B16A16_FLOAT), alpha = reflectivity
 *
 * SPDX-License-Identifier: Zlib
 */

/* Shadow depth map (slot 0). */
Texture2D    shadow_tex : register(t0, space2);
SamplerState shadow_smp : register(s0, space2);

/* Shadow bias. */
#define SHADOW_BIAS 0.005

/* Shadow map resolution — must match SHADOW_MAP_SIZE in main.c. */
#define SHADOW_MAP_RES 2048.0

/* Number of PCF filter samples for soft shadow edges. */
#define PCF_SAMPLES 4

cbuffer FragUniforms : register(b0, space3)
{
    float4 line_color;
    float4 bg_color;
    float3 eye_pos;
    float  grid_spacing;
    float  line_width;
    float  fade_distance;
    float  ambient;
    float  light_intensity;
    float4 light_dir;
    float3 light_color;
    float  reflectivity;   /* how much SSR reflection the grid receives */
};

/* MRT output structure — three render targets for SSR. */
struct PSOutput
{
    float4 color       : SV_Target0;  /* lit grid color          */
    float4 view_normal : SV_Target1;  /* view-space normal xyz   */
    float4 world_pos   : SV_Target2;  /* world-space pos, a=reflectivity */
};

/* ── Shadow sampling with 2x2 PCF ──────────────────────────────────── */

float sample_shadow(float4 light_clip)
{
    float3 light_ndc = light_clip.xyz / light_clip.w;

    float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;

    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0)
        return 1.0;

    float current_depth = light_ndc.z;
    float2 texel_size = float2(1.0 / SHADOW_MAP_RES, 1.0 / SHADOW_MAP_RES);

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

PSOutput main(float4 clip_pos   : SV_Position,
              float3 world_pos  : TEXCOORD0,
              float3 view_nrm   : TEXCOORD1,
              float4 light_clip : TEXCOORD2)
{
    PSOutput output;

    /* ── Procedural grid ────────────────────────────────────────────── */
    float2 grid_uv = world_pos.xz / grid_spacing;
    float2 dist    = abs(frac(grid_uv - 0.5) - 0.5);
    float2 fw      = fwidth(grid_uv);
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);
    float  grid    = max(aa_line.x, aa_line.y);

    /* Frequency-based fade: prevent moire when grid cells become sub-pixel. */
    float max_fw = max(fw.x, fw.y);
    grid *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    /* Distance fade. */
    float cam_dist = length(world_pos - eye_pos);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    float3 albedo = lerp(bg_color.rgb, line_color.rgb, grid);

    /* Grid normal is straight up. */
    float3 N = float3(0.0, 1.0, 0.0);

    /* ── Ambient term ────────────────────────────────────────────────── */
    float3 total_light = albedo * ambient;

    /* ── Directional light with shadow ───────────────────────────────── */
    {
        float3 L = normalize(-light_dir.xyz);
        float NdotL = max(dot(N, L), 0.0);
        float shadow = sample_shadow(light_clip);
        total_light += albedo * NdotL * light_intensity * light_color * shadow;
    }

    output.color = float4(total_light, 1.0);

    /* ── View-space normal for SSR G-buffer ─────────────────────────── */
    output.view_normal = float4(normalize(view_nrm), 1.0);

    /* ── World-space position for SSR; alpha stores reflectivity ────── */
    output.world_pos = float4(world_pos, reflectivity);

    return output;
}
