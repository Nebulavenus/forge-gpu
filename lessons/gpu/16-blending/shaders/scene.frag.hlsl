/*
 * scene.frag.hlsl — Fragment shader with Blinn-Phong lighting
 *
 * Samples the diffuse texture and multiplies by a base color uniform,
 * then applies Blinn-Phong lighting (ambient + diffuse + specular).
 *
 * This shader is shared by opaque and standard blend pipelines.  The
 * difference between those modes is entirely in the pipeline's
 * SDL_GPUColorTargetBlendState, not in the shader code.
 *
 * Blinn-Phong components:
 *   1. Ambient  — constant minimum brightness (fakes indirect light)
 *   2. Diffuse  — Lambert's cosine law: brightness = max(dot(N, L), 0)
 *   3. Specular — Blinn half-vector: pow(max(dot(N, H), 0), shininess)
 *
 * Fragment samplers:
 *   register(t0/s0, space2) -> diffuse texture (slot 0)
 *
 * Fragment uniform:
 *   register(b0, space3) -> base color + lighting params (80 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState diffuse_smp : register(s0, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;     /* RGBA color multiplier                       */
    float4 light_dir;      /* world-space light direction (toward light)  */
    float4 eye_pos;        /* world-space camera position                 */
    float  alpha_cutoff;   /* unused in this shader (shared struct)       */
    float  has_texture;    /* 1.0 = sample texture, 0.0 = color only     */
    float  shininess;      /* specular exponent (e.g. 32, 64, 128)       */
    float  ambient;        /* ambient light intensity [0..1]              */
    float  specular_str;   /* specular intensity [0..1]                   */
    float  _pad0;
    float  _pad1;
    float  _pad2;
};

struct PSInput
{
    float4 clip_pos   : SV_Position;
    float2 uv         : TEXCOORD0;
    float3 world_norm : TEXCOORD1;
    float3 world_pos  : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target
{
    /* ── Surface color ──────────────────────────────────────────────── */
    float4 texel = has_texture > 0.5
                 ? diffuse_tex.Sample(diffuse_smp, input.uv)
                 : float4(1.0, 1.0, 1.0, 1.0);
    float4 surface_color = texel * base_color;

    /* ── Vectors for lighting ───────────────────────────────────────── */
    float3 N = normalize(input.world_norm);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* ── Ambient ────────────────────────────────────────────────────── */
    float3 ambient_term = ambient * surface_color.rgb;

    /* ── Diffuse (Lambert) ──────────────────────────────────────────── */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = NdotL * surface_color.rgb;

    /* ── Specular (Blinn) ───────────────────────────────────────────── */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess) * float3(1.0, 1.0, 1.0);

    /* ── Combine ────────────────────────────────────────────────────── */
    float3 final_color = ambient_term + diffuse_term + specular_term;

    return float4(final_color, surface_color.a);
}
