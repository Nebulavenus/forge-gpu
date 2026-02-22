/*
 * Scene fragment shader — Blinn-Phong with spotlight, gobo projection,
 * and shadow mapping.
 *
 * The spotlight is the primary light source. A dim directional fill light
 * provides just enough ambient detail so the unlit side of objects is
 * still readable.
 *
 * The spotlight's view-projection matrix serves triple duty:
 *   1. Shadow mapping — transform to light clip space, compare depth
 *   2. Gobo projection — same transform, remap NDC to UV, sample pattern
 *   3. Cone masking — fragments outside [0,1] UV get zero light
 */

/* Shadow bias — prevents self-shadowing (shadow acne). */
#define SHADOW_BIAS 0.002

/* Shadow map resolution — must match SHADOW_MAP_SIZE in main.c. */
#define SHADOW_MAP_RES 1024.0

/* Quadratic attenuation coefficients for distance falloff. */
#define ATTEN_LINEAR    0.09
#define ATTEN_QUADRATIC 0.032

/* Diffuse texture (slot 0). */
Texture2D    diffuse_tex : register(t0, space2);
SamplerState diffuse_smp : register(s0, space2);

/* Shadow depth map (slot 1). */
Texture2D    shadow_tex  : register(t1, space2);
SamplerState shadow_smp  : register(s1, space2);

/* Gobo pattern texture (slot 2). */
Texture2D    gobo_tex    : register(t2, space2);
SamplerState gobo_smp    : register(s2, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;     /* material RGBA                 (16 bytes) */
    float3 eye_pos;        /* camera position                (12 bytes) */
    float  has_texture;    /* > 0.5 = sample diffuse_tex      (4 bytes) */
    float  ambient;        /* ambient intensity                (4 bytes) */
    float  fill_intensity; /* directional fill strength        (4 bytes) */
    float  shininess;      /* specular exponent                (4 bytes) */
    float  specular_str;   /* specular strength                (4 bytes) */
    float4 fill_dir;       /* fill light direction (xyz)      (16 bytes) */
    float3 spot_pos;       /* spotlight world position        (12 bytes) */
    float  spot_intensity; /* spotlight HDR brightness          (4 bytes) */
    float3 spot_dir;       /* spotlight direction (unit vec)  (12 bytes) */
    float  cos_inner;      /* cos(inner cone half-angle)        (4 bytes) */
    float3 spot_color;     /* spotlight RGB color             (12 bytes) */
    float  cos_outer;      /* cos(outer cone half-angle)        (4 bytes) */
    float4x4 light_vp;    /* spotlight view-projection       (64 bytes) */
};

/* ── Shadow sampling with 2x2 PCF ───────────────────────────────────── */

float sample_shadow(float3 light_ndc, float2 texel_size)
{
    /* Map NDC [-1,1] → UV [0,1].  Y is flipped for texture coordinates. */
    float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;

    float current_depth = light_ndc.z;

    /* 2x2 PCF — sample 4 neighboring texels for soft shadow edges. */
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

    /* ── Common vectors ─────────────────────────────────────────────── */
    float3 N = normalize(world_nrm);
    float3 V = normalize(eye_pos - world_pos);

    /* ── Ambient term ───────────────────────────────────────────────── */
    float3 total_light = albedo.rgb * ambient;

    /* ── Directional fill light ─────────────────────────────────────── */
    {
        float3 L = normalize(-fill_dir.xyz);
        float NdotL = max(dot(N, L), 0.0);
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float3 spec = specular_str * pow(NdotH, shininess);
        total_light += (albedo.rgb * NdotL + spec) * fill_intensity;
    }

    /* ── Spotlight ──────────────────────────────────────────────────── */
    {
        /* Direction from spotlight to fragment. */
        float3 to_frag = world_pos - spot_pos;
        float  dist    = length(to_frag);
        float3 L_frag  = to_frag / dist;

        /* Cone falloff: compare angle between spotlight direction and
         * the direction to this fragment against inner/outer angles. */
        float cos_angle = dot(L_frag, normalize(spot_dir));
        float cone = smoothstep(cos_outer, cos_inner, cos_angle);

        /* Only compute spotlight contribution if inside the cone. */
        if (cone > 0.0)
        {
            /* Transform fragment into spotlight clip space. */
            float4 light_clip = mul(light_vp, float4(world_pos, 1.0));
            float3 light_ndc  = light_clip.xyz / light_clip.w;

            /* Gobo texture projection — remap NDC to UV space. */
            float2 gobo_uv = light_ndc.xy * 0.5 + 0.5;
            gobo_uv.y = 1.0 - gobo_uv.y;

            /* Clamp: fragments outside [0,1] UV range get no light. */
            float in_bounds = step(0.0, gobo_uv.x) * step(gobo_uv.x, 1.0) *
                              step(0.0, gobo_uv.y) * step(gobo_uv.y, 1.0);

            /* Sample gobo pattern (grayscale — use .r channel). */
            float gobo = gobo_tex.Sample(gobo_smp, gobo_uv).r;

            /* Shadow test. */
            float2 texel_size = float2(1.0 / SHADOW_MAP_RES, 1.0 / SHADOW_MAP_RES);
            float shadow = sample_shadow(light_ndc, texel_size);

            /* Blinn-Phong from spotlight direction. */
            float3 L = normalize(spot_pos - world_pos);
            float NdotL = max(dot(N, L), 0.0);
            float3 H = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);
            float3 diffuse = albedo.rgb * NdotL;
            float3 spec = specular_str * pow(NdotH, shininess);

            /* Quadratic attenuation. */
            float atten = 1.0 / (1.0 + ATTEN_LINEAR * dist + ATTEN_QUADRATIC * dist * dist);

            total_light += (diffuse + spec) * cone * gobo * shadow *
                           in_bounds * atten * spot_intensity * spot_color;
        }
    }

    return float4(total_light, albedo.a);
}
