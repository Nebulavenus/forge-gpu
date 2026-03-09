/*
 * indirect_box.frag.hlsl — Blinn-Phong lighting with shadow mapping
 *
 * Samples the diffuse texture and applies Blinn-Phong shading with
 * percentage-closer filtering (PCF) shadow mapping. The per-object color
 * is multiplied with the texture sample for tinting.
 *
 * Texture/sampler bindings:
 *   register(t0, space2) -> diffuse texture
 *   register(s0, space2) -> texture sampler
 *   register(t1, space2) -> shadow map (depth texture)
 *   register(s1, space2) -> shadow comparison sampler
 *
 * Uniform buffer:
 *   register(b0, space3) -> fragment uniforms (light, eye, shadow params)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState tex_sampler : register(s0, space2);

Texture2D              shadow_map     : register(t1, space2);
SamplerComparisonState shadow_sampler : register(s1, space2);

cbuffer FragUniforms : register(b0, space3) {
    float4 light_dir;       /* world-space light direction (toward light) */
    float4 eye_pos;         /* world-space camera position */
    float  shadow_texel;    /* 1.0 / shadow_map_size */
    float  shininess;       /* specular exponent */
    float  ambient;         /* ambient intensity */
    float  specular_str;    /* specular multiplier */
};

struct PSInput {
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 world_nrm  : TEXCOORD1;
    float2 uv         : TEXCOORD2;
    float4 light_clip : TEXCOORD3;
    float4 color      : TEXCOORD4;
};

/* Percentage-closer filtering: 3x3 kernel for soft shadow edges. */
float compute_shadow(float4 light_clip) {
    float3 ndc = light_clip.xyz / light_clip.w;

    /* Remap X/Y from [-1,1] to [0,1] UV space */
    float2 shadow_uv = ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;

    /* Depth already in [0,1] for Vulkan conventions */
    float depth = ndc.z;

    /* Outside shadow map bounds = fully lit */
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
        depth < 0.0 || depth > 1.0)
        return 1.0;

    /* 3x3 PCF kernel */
    float shadow = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++) {
        [unroll]
        for (int x = -1; x <= 1; x++) {
            float2 offset = float2(x, y) * shadow_texel;
            shadow += shadow_map.SampleCmpLevelZero(
                shadow_sampler, shadow_uv + offset, depth - 0.002);
        }
    }
    return shadow / 9.0;
}

float4 main(PSInput input) : SV_Target {
    /* Surface color: texture × per-object color */
    float4 tex_color = diffuse_tex.Sample(tex_sampler, input.uv);
    float4 surface = tex_color * input.color;

    /* Lighting vectors */
    float3 N = normalize(input.world_nrm);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* Blinn-Phong */
    float NdotL = max(dot(N, L), 0.0);
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);

    float3 ambient_term  = ambient * surface.rgb;
    float3 diffuse_term  = NdotL * surface.rgb;
    float3 specular_term = specular_str * pow(NdotH, shininess) * float3(1, 1, 1);

    /* Shadow */
    float shadow = compute_shadow(input.light_clip);

    float3 final_color = ambient_term + shadow * (diffuse_term + specular_term);
    return float4(final_color, surface.a);
}
