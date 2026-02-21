/*
 * grid.frag.hlsl — Anti-aliased procedural grid with shadows and HDR
 *
 * Combines Lesson 12's procedural grid, Lesson 15's cascaded shadow
 * mapping, and Lesson 21's HDR light intensity.  The grid floor receives
 * shadows from scene objects while producing HDR lighting values.
 *
 * The grid algorithm (fwidth anti-aliasing, distance fade) is unchanged.
 * The lighting is Blinn-Phong with a hardcoded up-normal (the grid is flat).
 * Shadows modulate diffuse and specular terms; ambient is unaffected.
 * The light_intensity multiplier pushes lit areas past 1.0, creating
 * HDR values that the tone mapping pass will compress.
 *
 * Fragment samplers:
 *   register(t0/s0, space2) -> shadow map cascade 0 (slot 0)
 *   register(t1/s1, space2) -> shadow map cascade 1 (slot 1)
 *   register(t2/s2, space2) -> shadow map cascade 2 (slot 2)
 *
 * Uniform layout (128 bytes, 16-byte aligned):
 *   float4 line_color          (16 bytes)
 *   float4 bg_color            (16 bytes)
 *   float4 light_dir           (16 bytes)
 *   float4 eye_pos             (16 bytes)
 *   float4 cascade_splits      (16 bytes)
 *   float  grid_spacing         (4 bytes)
 *   float  line_width           (4 bytes)
 *   float  fade_distance        (4 bytes)
 *   float  ambient              (4 bytes)
 *   float  shininess            (4 bytes)
 *   float  specular_str         (4 bytes)
 *   float  light_intensity      (4 bytes)
 *   float  shadow_texel_size    (4 bytes)
 *   float  shadow_bias          (4 bytes)
 *   float3 _pad                (12 bytes)
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
    float4 line_color;         /* grid line color                            */
    float4 bg_color;           /* background surface color                   */
    float4 light_dir;          /* world-space light direction (toward light) */
    float4 eye_pos;            /* world-space camera position                */
    float4 cascade_splits;     /* cascade far distances (x=c0, y=c1, z=c2)  */
    float  grid_spacing;       /* world units between lines (e.g. 1.0)      */
    float  line_width;         /* line thickness in grid-space [0..0.5]      */
    float  fade_distance;      /* distance where grid fades to background    */
    float  ambient;            /* ambient light intensity [0..1]             */
    float  shininess;          /* specular exponent (e.g. 32, 64)           */
    float  specular_str;       /* specular intensity [0..1]                  */
    float  light_intensity;    /* light brightness multiplier (>1 = HDR)     */
    float  shadow_texel_size;  /* 1.0 / shadow_map_resolution               */
    float  shadow_bias;        /* depth comparison bias                      */
    float3 _pad;
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
    /* ── Procedural grid pattern ─────────────────────────────────────── */

    /* Scale to grid space so integer values fall on grid lines. */
    float2 grid_uv = input.world_pos.xz / grid_spacing;

    /* Distance to nearest grid line in each axis. */
    float2 dist = abs(frac(grid_uv - 0.5) - 0.5);

    /* Screen-space derivative for anti-aliasing (pixel footprint size). */
    float2 fw = fwidth(grid_uv);

    /* Anti-aliased line mask — smoothstep gives a soft edge. */
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);

    /* Combine X and Z lines — show a line if either axis is on a line. */
    float grid = max(aa_line.x, aa_line.y);

    /* Frequency-based fade (prevents moire when fwidth is too large). */
    float max_fw = max(fw.x, fw.y);
    grid *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    /* Distance fade — dissolve the grid at the far field. */
    float cam_dist = length(input.world_pos - eye_pos.xyz);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    /* Mix line and background colors. */
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

    /* ── Blinn-Phong with shadows + HDR ──────────────────────────────── */

    float3 N = float3(0.0, 1.0, 0.0);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* Diffuse (Lambert) — modulated by shadow factor */
    float NdotL = max(dot(N, L), 0.0);

    /* Specular (Blinn half-vector) — also modulated by shadow factor */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess)
                         * float3(1.0, 1.0, 1.0);

    /* Ambient is unaffected by shadow; diffuse and specular are modulated.
     * light_intensity > 1.0 pushes lit areas into HDR range. */
    float3 lit = ambient * surface
               + (shadow_factor * NdotL * surface
                  + shadow_factor * specular_term) * light_intensity;

    return float4(lit, 1.0);
}
