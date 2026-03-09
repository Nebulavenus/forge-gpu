/*
 * scene_frag_pipeline.hlsl — Pipeline path fragment shader with normal mapping
 *
 * Uses the interpolated TBN basis from the vertex shader to transform
 * tangent-space normals from the normal map into world space.  Applies
 * Blinn-Phong lighting with PCF 3x3 shadow mapping.
 *
 * The pipeline path is used when the asset pipeline has generated tangent
 * vectors via MikkTSpace, enabling per-texel normal perturbation that adds
 * geometric detail without extra triangles.
 *
 * Texture/sampler bindings:
 *   register(t0, space2) -> diffuse texture
 *   register(s0, space2) -> diffuse sampler
 *   register(t1, space2) -> normal map texture
 *   register(s1, space2) -> normal map sampler
 *   register(t2, space2) -> shadow map (depth)
 *   register(s2, space2) -> shadow comparison sampler
 *
 * Uniform buffer:
 *   register(b0, space3) -> lighting and shadow parameters (32 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex    : register(t0, space2);
SamplerState diffuse_smp    : register(s0, space2);

Texture2D    normal_tex     : register(t1, space2);
SamplerState normal_smp     : register(s1, space2);

Texture2D              shadow_map     : register(t2, space2);
SamplerComparisonState shadow_sampler : register(s2, space2);

cbuffer FragUniforms : register(b0, space3) {
    float4 light_dir;       /* world-space direction toward light              */
    float4 eye_pos;         /* world-space camera position                     */
    float  shadow_texel;    /* 1.0 / shadow_map_resolution                    */
    float  shininess;       /* specular exponent (e.g. 64, 128)               */
    float  ambient;         /* ambient light intensity [0..1]                  */
    float  specular_str;    /* specular intensity multiplier [0..1]            */
};

struct PSInput {
    float4 clip_pos      : SV_Position;
    float3 world_pos     : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float2 uv            : TEXCOORD2;
    float3 world_tangent : TEXCOORD3;
    float3 world_bitan   : TEXCOORD4;
    float4 shadow_pos    : TEXCOORD5;
};

/* ── PCF 3x3 shadow sampling ────────────────────────────────────────
 * Samples the shadow map in a 3x3 grid around the projected position
 * and averages the comparison results for soft shadow edges. */
float compute_shadow(float4 light_clip) {
    float3 ndc = light_clip.xyz / light_clip.w;
    float2 shadow_uv = ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;   /* Vulkan Y-flip */
    float depth = ndc.z;

    /* Fragments outside the shadow map receive full light. */
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
        depth < 0.0 || depth > 1.0)
        return 1.0;

    float shadow = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++) {
        [unroll]
        for (int x = -1; x <= 1; x++) {
            shadow += shadow_map.SampleCmpLevelZero(
                shadow_sampler, shadow_uv + float2(x, y) * shadow_texel,
                depth - 0.002);
        }
    }
    return shadow / 9.0;
}

float4 main(PSInput input) : SV_Target {
    /* ── Surface color from diffuse texture ───────────────────────── */
    float4 surface = diffuse_tex.Sample(diffuse_smp, input.uv);

    /* ── Reconstruct TBN basis ────────────────────────────────────────
     * Re-normalize after interpolation — the rasterizer linearly
     * interpolates vertex outputs, which shortens unit vectors. */
    float3 N = normalize(input.world_normal);
    float3 T = normalize(input.world_tangent);
    float3 B = normalize(input.world_bitan);

    /* ── Sample and decode the normal map ─────────────────────────────
     * Normal maps store directions as RGB in [0, 1].  Convert back to
     * the original [-1, 1] range.  glTF uses OpenGL convention: +Y
     * bitangent (green channel up).
     *
     * NOTE: If using BC5 compressed normal maps (only RG channels),
     * reconstruct z: z = sqrt(1 - x*x - y*y).  This lesson uses
     * standard RGB (PNG) textures, so all three channels are available. */
    float3 map_normal = normal_tex.Sample(normal_smp, input.uv).rgb;
    map_normal = map_normal * 2.0 - 1.0;

    /* ── TBN transformation: tangent space -> world space ─────────────
     * The TBN matrix has T, B, N as rows.  mul(vec, mat) computes
     * vec.x * row0 + vec.y * row1 + vec.z * row2, giving us:
     * n.x * T + n.y * B + n.z * N — the tangent-to-world transform. */
    float3x3 TBN = float3x3(T, B, N);
    N = normalize(mul(map_normal, TBN));

    /* ── Lighting vectors ─────────────────────────────────────────── */
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* ── Blinn-Phong lighting ─────────────────────────────────────── */
    float NdotL = max(dot(N, L), 0.0);
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);

    float3 ambient_term  = ambient * surface.rgb;
    float3 diffuse_term  = NdotL * surface.rgb;
    float3 specular_term = specular_str * pow(NdotH, shininess)
                         * float3(1.0, 1.0, 1.0);

    /* ── Shadow ───────────────────────────────────────────────────── */
    float shadow = compute_shadow(input.shadow_pos);

    float3 final_color = ambient_term + shadow * (diffuse_term + specular_term);
    return float4(final_color, surface.a);
}
