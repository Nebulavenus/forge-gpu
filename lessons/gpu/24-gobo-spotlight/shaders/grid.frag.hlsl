/*
 * Grid fragment shader — procedural anti-aliased grid lines with
 * spotlight, gobo projection, and shadow mapping.
 */

/* Shadow depth map (slot 0). */
Texture2D    shadow_tex : register(t0, space2);
SamplerState shadow_smp : register(s0, space2);

/* Gobo pattern texture (slot 1). */
Texture2D    gobo_tex   : register(t1, space2);
SamplerState gobo_smp   : register(s1, space2);

/* Shadow bias. */
#define SHADOW_BIAS 0.002

/* Shadow map resolution — must match SHADOW_MAP_SIZE in main.c. */
#define SHADOW_MAP_RES 1024.0

/* Quadratic attenuation coefficients for distance falloff. */
#define ATTEN_LINEAR    0.09
#define ATTEN_QUADRATIC 0.032

cbuffer FragUniforms : register(b0, space3)
{
    float4 line_color;
    float4 bg_color;
    float3 eye_pos;
    float  grid_spacing;
    float  line_width;
    float  fade_distance;
    float  ambient;        /* ambient intensity              */
    float  fill_intensity; /* directional fill strength      */
    float4 fill_dir;       /* fill light direction (xyz)     */
    /* Spotlight data (same layout as scene shader). */
    float3 spot_pos;
    float  spot_intensity;
    float3 spot_dir;
    float  cos_inner;
    float3 spot_color;
    float  cos_outer;
    float4x4 light_vp;
};

/* ── Shadow sampling with 2x2 PCF ───────────────────────────────────── */

float sample_shadow(float3 light_ndc, float2 texel_size)
{
    float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;

    float current_depth = light_ndc.z;

    float shadow = 0.0;
    float2 offsets[4] = {
        float2(-0.5, -0.5),
        float2( 0.5, -0.5),
        float2(-0.5,  0.5),
        float2( 0.5,  0.5)
    };

    for (int i = 0; i < 4; i++)
    {
        float stored = shadow_tex.Sample(shadow_smp, shadow_uv + offsets[i] * texel_size).r;
        shadow += (current_depth - SHADOW_BIAS <= stored) ? 1.0 : 0.0;
    }

    return shadow * 0.25;
}

float4 main(float4 clip_pos : SV_Position, float3 world_pos : TEXCOORD0) : SV_Target
{
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
    float3 V = normalize(eye_pos - world_pos);

    /* ── Ambient term ────────────────────────────────────────────────── */
    float3 total_light = albedo * ambient;

    /* ── Directional fill light ──────────────────────────────────────── */
    {
        float3 L = normalize(-fill_dir.xyz);
        float NdotL = max(dot(N, L), 0.0);
        total_light += albedo * NdotL * fill_intensity;
    }

    /* ── Spotlight on the grid floor ─────────────────────────────────── */
    {
        float3 to_frag = world_pos - spot_pos;
        float  d       = length(to_frag);
        float3 L_frag  = to_frag / d;

        float cos_angle = dot(L_frag, normalize(spot_dir));
        float cone = smoothstep(cos_outer, cos_inner, cos_angle);

        if (cone > 0.0)
        {
            float4 light_clip = mul(light_vp, float4(world_pos, 1.0));
            float3 light_ndc  = light_clip.xyz / light_clip.w;

            float2 gobo_uv = light_ndc.xy * 0.5 + 0.5;
            gobo_uv.y = 1.0 - gobo_uv.y;

            float in_bounds = step(0.0, gobo_uv.x) * step(gobo_uv.x, 1.0) *
                              step(0.0, gobo_uv.y) * step(gobo_uv.y, 1.0);

            float gobo = gobo_tex.Sample(gobo_smp, gobo_uv).r;

            float2 texel_size = float2(1.0 / SHADOW_MAP_RES, 1.0 / SHADOW_MAP_RES);
            float shadow = sample_shadow(light_ndc, texel_size);

            float3 L = normalize(spot_pos - world_pos);
            float NdotL = max(dot(N, L), 0.0);
            float atten = 1.0 / (1.0 + ATTEN_LINEAR * d + ATTEN_QUADRATIC * d * d);

            total_light += albedo * NdotL * cone * gobo * shadow *
                           in_bounds * atten * spot_intensity * spot_color;
        }
    }

    return float4(total_light, 1.0);
}
