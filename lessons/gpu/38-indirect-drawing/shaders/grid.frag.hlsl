/*
 * grid.frag.hlsl — Procedural grid floor with Blinn-Phong and shadow
 *
 * Generates anti-aliased grid lines using fwidth()/smoothstep(), then applies
 * directional lighting and shadow mapping. Same approach as L15+.
 *
 * Texture/sampler bindings:
 *   register(t0, space2) -> shadow map
 *   register(s0, space2) -> shadow comparison sampler
 *
 * Uniform buffer:
 *   register(b0, space3) -> grid fragment uniforms
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D              shadow_map     : register(t0, space2);
SamplerComparisonState shadow_sampler : register(s0, space2);

cbuffer GridFragUniforms : register(b0, space3) {
    float4 line_color;
    float4 bg_color;
    float4 light_dir;
    float4 eye_pos;
    column_major float4x4 light_vp;
    float  grid_spacing;
    float  line_width;
    float  fade_distance;
    float  ambient;
    float  shininess;
    float  specular_str;
    float  shadow_texel;
    float  _pad;
};

struct PSInput {
    float4 clip_pos  : SV_Position;
    float3 world_pos : TEXCOORD0;
};

float compute_shadow(float3 world_pos) {
    float4 lc = mul(light_vp, float4(world_pos, 1.0));
    float3 ndc = lc.xyz / lc.w;
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
    /* Grid line generation using screen-space derivatives */
    float2 grid_uv = input.world_pos.xz / grid_spacing;
    float2 grid_frac = frac(grid_uv + 0.5) - 0.5;
    float2 grid_deriv = fwidth(grid_uv);

    float2 line_mask = smoothstep(
        grid_deriv * (0.5 + line_width),
        grid_deriv * (0.5 - line_width),
        abs(grid_frac)
    );
    float grid_line = max(line_mask.x, line_mask.y);

    /* Distance fade */
    float dist = length(input.world_pos.xz - eye_pos.xz);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, dist);
    grid_line *= fade;

    /* Mix line and background colors */
    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid_line);

    /* Simple upward-facing normal for the floor */
    float3 N = float3(0.0, 1.0, 0.0);
    float3 L = normalize(light_dir.xyz);
    float NdotL = max(dot(N, L), 0.0);

    /* Shadow */
    float shadow = compute_shadow(input.world_pos);

    float3 final_color = ambient * surface + shadow * NdotL * surface;
    float alpha = max(grid_line * fade, bg_color.a * fade * 0.5);

    return float4(final_color, alpha);
}
