/*
 * Decal fragment shader — projects a decal texture onto scene geometry
 * using depth buffer reconstruction, then applies Blinn-Phong lighting
 * and shadow sampling so decals match the scene's shading.
 *
 * Algorithm:
 *   1. Sample the scene depth at this screen pixel
 *   2. Reconstruct world-space position via inverse view-projection
 *   3. Project the world position into decal local space
 *   4. If outside the unit cube [-0.5, 0.5]^3, discard
 *   5. Map local XZ to decal texture UV
 *   6. Apply soft edge fade at box boundaries
 *   7. Reconstruct surface normal from depth buffer gradients
 *   8. Compute Blinn-Phong lighting with shadow map sampling
 *
 * Fragment samplers (space2):
 *   slot 0 -> scene depth texture + nearest-clamp sampler
 *   slot 1 -> decal shape texture + linear-clamp sampler
 *   slot 2 -> shadow depth texture + nearest-clamp sampler
 *   slot 3 -> scene normal texture + nearest-clamp sampler
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: decal transform, tint, and lighting
 *
 * SPDX-License-Identifier: Zlib
 */

/* Shadow bias — prevents self-shadowing (shadow acne). */
#define SHADOW_BIAS 0.005

/* Shadow map resolution — must match SHADOW_MAP_SIZE in main.c. */
#define SHADOW_MAP_RES 2048.0

/* Number of PCF filter samples for soft shadow edges. */
#define PCF_SAMPLES 4

Texture2D    depth_tex  : register(t0, space2);
SamplerState depth_smp  : register(s0, space2);

Texture2D    decal_tex  : register(t1, space2);
SamplerState decal_smp  : register(s1, space2);

Texture2D    shadow_tex : register(t2, space2);
SamplerState shadow_smp : register(s2, space2);

Texture2D    normal_tex : register(t3, space2);
SamplerState normal_smp : register(s3, space2);

cbuffer DecalFragUniforms : register(b0, space3)
{
    column_major float4x4 inv_vp;          /* inverse(proj * view)              */
    column_major float4x4 inv_decal_model; /* inverse of decal model matrix     */
    column_major float4x4 light_vp;        /* light view-projection for shadows */
    float2 screen_size;                    /* viewport width, height             */
    float  near_plane;                     /* camera near plane                  */
    float  far_plane;                      /* camera far plane                   */
    float4 decal_tint;                     /* RGBA per-decal color               */
    float3 eye_pos;                        /* world-space camera position        */
    float  ambient;                        /* ambient light intensity [0..1]     */
    float3 light_dir;                      /* normalized world-space light dir   */
    float  light_intensity;                /* directional light brightness       */
    float3 light_color;                    /* directional light RGB color        */
    float  shininess;                      /* specular exponent (Blinn-Phong)    */
    float  specular_str;                   /* specular strength multiplier       */
    float3 _pad0;                          /* padding to 16-byte alignment       */
};

/* ── Shadow sampling with 2x2 PCF ──────────────────────────────────── */

float sample_shadow(float3 world_pos)
{
    float4 light_clip = mul(light_vp, float4(world_pos, 1.0));
    float3 light_ndc = light_clip.xyz / light_clip.w;

    /* Map NDC [-1,1] to UV [0,1].  Y is flipped for texture coordinates. */
    float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;

    /* Fragments outside the shadow map bounds are fully lit. */
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0)
        return 1.0;

    float current_depth = light_ndc.z;
    float2 texel_size = float2(1.0 / SHADOW_MAP_RES, 1.0 / SHADOW_MAP_RES);

    /* 2x2 PCF — sample neighboring texels for soft shadow edges. */
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

float4 main(float4 sv_pos : SV_Position) : SV_Target
{
    /* 1. Sample scene depth at this screen pixel */
    float2 screen_uv = sv_pos.xy / screen_size;
    float depth = depth_tex.Sample(depth_smp, screen_uv).r;

    /* Discard sky pixels (depth = 1.0 means nothing was rendered) */
    if (depth >= 0.9999)
        discard;

    /* 2. Reconstruct world position via inverse VP */
    float2 ndc = screen_uv * 2.0 - 1.0;
    ndc.y = -ndc.y;  /* Vulkan/SDL GPU Y-flip */

    float4 world_h = mul(inv_vp, float4(ndc, depth, 1.0));
    float3 world_pos = world_h.xyz / world_h.w;

    /* 3. Project into decal local space */
    float3 local = mul(inv_decal_model, float4(world_pos, 1.0)).xyz;

    /* 4. Box bounds check [-0.5, 0.5]^3 — discard if outside */
    if (any(abs(local) > 0.5))
        discard;

    /* 5. UV from local XZ, sample decal texture */
    float2 uv = local.xz + 0.5;
    float4 decal_color = decal_tex.Sample(decal_smp, uv) * decal_tint;

    /* 6. Soft edge fade — smoothstep near cube boundaries */
    float fade = smoothstep(0.5, 0.4, abs(local.x))
               * smoothstep(0.5, 0.4, abs(local.y))
               * smoothstep(0.5, 0.4, abs(local.z));
    decal_color.a *= fade;

    /* Discard nearly invisible fragments */
    if (decal_color.a < 0.01)
        discard;

    /* 7. Sample world-space normal from the scene normal G-buffer.
     *    The scene and grid shaders encode normals as n * 0.5 + 0.5 into
     *    an RGBA8 texture.  Decode back to [-1, 1] range here. */
    float3 N = normalize(normal_tex.Sample(normal_smp, screen_uv).xyz * 2.0 - 1.0);

    /* 8. Blinn-Phong lighting with shadow */
    float3 V = normalize(eye_pos - world_pos);
    float3 L = normalize(-light_dir);

    /* Ambient */
    float3 lit = decal_color.rgb * ambient;

    /* Diffuse + specular with shadow */
    float NdotL = max(dot(N, L), 0.0);
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 spec = specular_str * pow(NdotH, shininess);

    float shadow = sample_shadow(world_pos);
    lit += (decal_color.rgb * NdotL + spec) * light_intensity *
           light_color * shadow;

    return float4(lit, decal_color.a);
}
