/*
 * Grid fragment shader — procedural anti-aliased grid with distance fade.
 *
 * Uses screen-space derivatives (fwidth) for pixel-perfect anti-aliasing
 * and frequency-based fade to prevent moire at low grazing angles.
 * Includes Blinn-Phong shading with multiple point lights and omnidirectional
 * shadow mapping via cube maps.
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

/* Shadow cube maps (slots 0-3). */
TextureCube  shadow_cube0 : register(t0, space2);
SamplerState shadow_smp0  : register(s0, space2);
TextureCube  shadow_cube1 : register(t1, space2);
SamplerState shadow_smp1  : register(s1, space2);
TextureCube  shadow_cube2 : register(t2, space2);
SamplerState shadow_smp2  : register(s2, space2);
TextureCube  shadow_cube3 : register(t3, space2);
SamplerState shadow_smp3  : register(s3, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 line_color;              /* grid line color (RGBA, linear)    */
    float4 bg_color;                /* grid background color (RGBA)      */
    float3 eye_pos;                 /* camera position                   */
    float  grid_spacing;            /* world units between lines         */
    float  line_width;              /* line thickness in grid space       */
    float  fade_distance;           /* distance for fade-out              */
    float  ambient;                 /* ambient intensity [0..1]           */
    float  shininess;               /* specular exponent                  */
    float  specular_str;            /* specular intensity [0..1]          */
    float  shadow_far_plane;        /* shadow map far plane               */
    float  _pad1;
    float  _pad2;
    PointLight lights[MAX_POINT_LIGHTS]; /* point light array             */
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

float4 main(float4 clip_pos : SV_Position, float3 world_pos : TEXCOORD0) : SV_Target
{
    /* ── Procedural grid pattern ────────────────────────────────────── */
    float2 grid_uv = world_pos.xz / grid_spacing;
    float2 dist    = abs(frac(grid_uv - 0.5) - 0.5);
    float2 fw      = fwidth(grid_uv);
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);
    float  grid    = max(aa_line.x, aa_line.y);

    /* Frequency-based fade: prevent moire when grid cells become sub-pixel */
    float max_fw = max(fw.x, fw.y);
    grid *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    /* Distance fade */
    float cam_dist = length(world_pos - eye_pos);
    float fade     = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    /* Mix line and background colors */
    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);

    /* ── Blinn-Phong lighting from point lights ─────────────────────── */
    float3 N = float3(0.0, 1.0, 0.0); /* grid normal is straight up */
    float3 V = normalize(eye_pos - world_pos);

    float3 total_light = surface * ambient;

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
        float3 diffuse = surface * NdotL;

        /* Specular (Blinn-Phong) */
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float3 spec = specular_str * pow(NdotH, shininess);

        /* Shadow factor — 1.0 if lit, 0.0 if occluded */
        float3 light_to_frag = world_pos - lights[i].position;
        float shadow = sample_shadow(i, light_to_frag);

        total_light += (diffuse + spec) * shadow * attenuation * intensity * lights[i].color;
    }

    return float4(total_light, 1.0);
}
