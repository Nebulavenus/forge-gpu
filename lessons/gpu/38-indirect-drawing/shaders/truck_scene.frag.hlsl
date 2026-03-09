/*
 * truck_scene.frag.hlsl — Blinn-Phong + shadow for the CesiumMilkTruck
 *
 * Standard Blinn-Phong lighting with PCF shadow mapping. Uses the material
 * base_color from uniforms (set per-mesh from glTF material data).
 *
 * Texture/sampler bindings:
 *   register(t0, space2) -> diffuse texture
 *   register(s0, space2) -> texture sampler
 *   register(t1, space2) -> shadow map
 *   register(s1, space2) -> shadow comparison sampler
 *
 * Uniform buffer:
 *   register(b0, space3) -> fragment uniforms
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState tex_sampler : register(s0, space2);

Texture2D              shadow_map     : register(t1, space2);
SamplerComparisonState shadow_sampler : register(s1, space2);

cbuffer TruckFragUniforms : register(b0, space3) {
    float4 base_color;
    float4 light_dir;
    float4 eye_pos;
    float  shadow_texel;
    float  shininess;
    float  ambient;
    float  specular_str;
    uint   has_texture;
    float3 _pad;
};

struct PSInput {
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 world_nrm  : TEXCOORD1;
    float2 uv         : TEXCOORD2;
    float4 light_clip : TEXCOORD3;
};

float compute_shadow(float4 light_clip) {
    float3 ndc = light_clip.xyz / light_clip.w;
    float2 shadow_uv = ndc.xy * 0.5 + 0.5;
    shadow_uv.y = 1.0 - shadow_uv.y;
    float depth = ndc.z;

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
    float4 surface;
    if (has_texture) {
        surface = diffuse_tex.Sample(tex_sampler, input.uv) * base_color;
    } else {
        surface = base_color;
    }

    float3 N = normalize(input.world_nrm);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    float NdotL = max(dot(N, L), 0.0);
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);

    float3 ambient_term  = ambient * surface.rgb;
    float3 diffuse_term  = NdotL * surface.rgb;
    float3 specular_term = specular_str * pow(NdotH, shininess) * float3(1, 1, 1);

    float shadow = compute_shadow(input.light_clip);

    float3 final_color = ambient_term + shadow * (diffuse_term + specular_term);
    return float4(final_color, surface.a);
}
