/*
 * Grid fragment shader — anti-aliased procedural grid with directional light
 * and shadow map sampling.
 *
 * Simplified single-target version for physics lessons: outputs only lit
 * grid color to SV_Target0.  No MRT, no G-buffer normal output.
 *
 * Fragment samplers (space2):
 *   slot 0 -> shadow depth texture + nearest-clamp sampler
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: grid rendering parameters (80 bytes)
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

cbuffer GridFragUniforms : register(b0, space3)
{
    float4 line_color;         /* grid line color                            */
    float4 bg_color;           /* background surface color                   */
    float3 light_dir;          /* world-space directional light direction     */
    float  light_intensity;    /* light brightness multiplier                */
    float3 eye_pos;            /* world-space camera position                */
    float  grid_spacing;       /* world units between lines (e.g. 1.0)      */
    float  line_width;         /* line thickness in grid-space [0..0.5]      */
    float  fade_distance;      /* distance where grid fades to background    */
    float  ambient;            /* ambient light intensity [0..1]             */
    float  _pad;
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

struct PSInput
{
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float4 light_clip : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    /* ── Procedural grid pattern ─────────────────────────────────────── */

    float2 grid_uv = input.world_pos.xz / grid_spacing;

    float2 dist_to_line = abs(frac(grid_uv - 0.5) - 0.5);

    float2 fw = fwidth(grid_uv);

    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist_to_line);

    float grid = max(aa_line.x, aa_line.y);

    float max_fw = max(fw.x, fw.y);
    grid *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    float cam_dist = length(input.world_pos - eye_pos);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);

    /* ── Simple directional lighting with shadow ─────────────────────── */

    float3 N = float3(0.0, 1.0, 0.0);
    float3 L = normalize(-light_dir);
    float NdotL = max(dot(N, L), 0.0);

    float shadow = sample_shadow(input.light_clip);

    float3 lit = surface * (ambient + NdotL * light_intensity * shadow);

    return float4(lit, 1.0);
}
