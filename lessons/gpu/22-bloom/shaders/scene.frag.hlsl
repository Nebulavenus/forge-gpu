/*
 * scene.frag.hlsl — Blinn-Phong with point light and HDR output
 *
 * Simplified from Lesson 21 by replacing the directional light + cascaded
 * shadow maps with a single point light.  The lighting result is NOT clamped —
 * values above 1.0 are preserved in the R16G16B16A16_FLOAT render target.
 *
 * Point light attenuation:
 *   The light intensity falls off with distance using a quadratic model:
 *     attenuation = 1 / (1 + 0.09*d + 0.032*d^2)
 *   This creates a natural falloff: nearby surfaces are bright (HDR values)
 *   while distant surfaces fall toward ambient.
 *
 * Uniform layout (64 bytes, 16-byte aligned):
 *   float4 base_color         (16 bytes)
 *   float3 light_pos           (12 bytes) + float light_intensity (4 bytes)
 *   float3 eye_pos             (12 bytes) + float has_texture     (4 bytes)
 *   float  shininess            (4 bytes)
 *   float  ambient              (4 bytes)
 *   float  specular_str         (4 bytes)
 *   float  _pad                 (4 bytes)
 *
 * Fragment samplers:
 *   register(t0/s0, space2) -> diffuse texture (slot 0)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex  : register(t0, space2);
SamplerState diffuse_smp  : register(s0, space2);

cbuffer SceneFragUniforms : register(b0, space3)
{
    float4 base_color;         /* material base color (RGBA)                    */
    float3 light_pos;          /* world-space point light position              */
    float  light_intensity;    /* light brightness multiplier (HDR)             */
    float3 eye_pos;            /* world-space camera position                   */
    float  has_texture;        /* non-zero = sample texture, zero = solid color */
    float  shininess;          /* specular exponent (e.g. 32, 64, 128)         */
    float  ambient;            /* ambient light intensity [0..1]                */
    float  specular_str;       /* specular intensity [0..1]                     */
    float  _pad;
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
    float4 surface_color;
    if (has_texture > 0.5)
    {
        surface_color = diffuse_tex.Sample(diffuse_smp, input.uv) * base_color;
    }
    else
    {
        surface_color = base_color;
    }

    /* ── Vectors for lighting ───────────────────────────────────────── */
    float3 N = normalize(input.world_norm);
    float3 L_vec = light_pos - input.world_pos;
    float  dist = length(L_vec);
    float3 L = L_vec / dist;  /* normalize */
    float3 V = normalize(eye_pos - input.world_pos);

    /* ── Point light attenuation ────────────────────────────────────── */
    /* Quadratic attenuation model: intensity falls off naturally with
     * distance.  The constants (0.09 linear, 0.032 quadratic) give a
     * reasonable falloff for a scene-scale radius of ~10 units. */
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    /* ── Ambient ────────────────────────────────────────────────────── */
    float3 ambient_term = ambient * surface_color.rgb;

    /* ── Diffuse (Lambert) ──────────────────────────────────────────── */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = NdotL * surface_color.rgb;

    /* ── Specular (Blinn half-vector) ───────────────────────────────── */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess)
                         * float3(1.0, 1.0, 1.0);

    /* ── Combine — HDR output with point light attenuation ──────────── */
    /* Diffuse and specular are scaled by attenuation and light_intensity.
     * With high intensity and close distance, the result exceeds 1.0 —
     * the HDR render target preserves these values for the bloom pass. */
    float3 final_color = ambient_term
                       + (diffuse_term + specular_term) * attenuation * light_intensity;

    return float4(final_color, surface_color.a);
}
