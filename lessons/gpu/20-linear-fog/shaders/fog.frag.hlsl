/*
 * fog.frag.hlsl — Fragment shader with Blinn-Phong materials + distance fog
 *
 * Extends Lesson 18's material shader with depth-based fog.  After computing
 * the Blinn-Phong lighting result, the shader calculates the distance from
 * the fragment to the camera and blends the lit color toward a fog color.
 *
 * Three fog modes are supported, selectable at runtime via the fog_mode
 * uniform:
 *
 *   Mode 0 — Linear:  fog = (end - d) / (end - start)
 *     Visibility drops linearly between a start and end distance.
 *     Simplest to understand and control.
 *
 *   Mode 1 — Exponential:  fog = e^(-density * d)
 *     Smooth exponential decay.  Denser fog overall, good for haze.
 *
 *   Mode 2 — Exp-squared:  fog = e^(-(density * d)^2)
 *     Holds clear near the camera then drops sharply.  Good for
 *     thick ground fog or atmospheric perspective.
 *
 * The fog factor ranges from 1.0 (fully visible) to 0.0 (fully fogged).
 * The final color is:  lerp(fog_color, lit_color, fog_factor).
 *
 * Uniform layout (128 bytes, 16-byte aligned):
 *   float4 mat_ambient    (16 bytes) — material ambient color (rgb)
 *   float4 mat_diffuse    (16 bytes) — material diffuse color (rgb)
 *   float4 mat_specular   (16 bytes) — specular color (rgb) + shininess (w)
 *   float4 light_dir      (16 bytes) — world-space light direction (xyz)
 *   float4 eye_pos        (16 bytes) — world-space camera position (xyz)
 *   uint   has_texture     (4 bytes) — whether to sample diffuse texture
 *   float  _pad0           (4 bytes)
 *   float  _pad1           (4 bytes)
 *   float  _pad2           (4 bytes)
 *   float4 fog_color      (16 bytes) — fog/clear color (rgb)
 *   float  fog_start       (4 bytes) — linear fog start distance
 *   float  fog_end         (4 bytes) — linear fog end distance
 *   float  fog_density     (4 bytes) — exp/exp2 fog density
 *   uint   fog_mode        (4 bytes) — 0=linear, 1=exp, 2=exp2
 *   Total: 128 bytes
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState smp         : register(s0, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 mat_ambient;    /* material ambient color (rgb, w unused)         */
    float4 mat_diffuse;    /* material diffuse color (rgb, w unused)         */
    float4 mat_specular;   /* material specular color (rgb), shininess (w)   */
    float4 light_dir;      /* world-space light direction (toward light)     */
    float4 eye_pos;        /* world-space camera position                    */
    uint   has_texture;    /* non-zero = sample diffuse texture              */
    float  _pad0;
    float  _pad1;
    float  _pad2;
    float4 fog_color;     /* fog color — should match the clear color       */
    float  fog_start;     /* linear fog: distance where fog begins          */
    float  fog_end;       /* linear fog: distance where fully fogged        */
    float  fog_density;   /* exponential modes: fog density coefficient     */
    uint   fog_mode;      /* 0 = linear, 1 = exponential, 2 = exp-squared  */
};

struct PSInput
{
    float4 clip_pos   : SV_Position; /* not used, but required by pipeline */
    float2 uv         : TEXCOORD0;   /* interpolated texture coordinates   */
    float3 world_norm : TEXCOORD1;   /* interpolated world-space normal    */
    float3 world_pos  : TEXCOORD2;   /* interpolated world-space position  */
};

float4 main(PSInput input) : SV_Target
{
    /* ── Diffuse surface color ─────────────────────────────────────── */
    float3 diffuse_color = mat_diffuse.rgb;
    if (has_texture)
    {
        diffuse_color *= diffuse_tex.Sample(smp, input.uv).rgb;
    }

    /* ── Vectors for lighting ──────────────────────────────────────── */
    float3 N = normalize(input.world_norm);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* ── Ambient ───────────────────────────────────────────────────── */
    float3 ambient_term = mat_ambient.rgb;

    /* ── Diffuse (Lambert) ─────────────────────────────────────────── */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = NdotL * diffuse_color;

    /* ── Specular (Blinn) ──────────────────────────────────────────── */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float  shininess = mat_specular.w;
    float3 specular_term = pow(NdotH, shininess) * mat_specular.rgb;

    /* ── Combine lighting ──────────────────────────────────────────── */
    float3 lit_color = ambient_term + diffuse_term + specular_term;

    /* ── Distance fog ──────────────────────────────────────────────── */
    /* Compute the distance from this fragment to the camera.
     * This is a true Euclidean distance (not just Z depth), so fog
     * looks correct when the camera rotates — fragments at the same
     * world-space distance get the same fog regardless of view angle. */
    float dist = length(eye_pos.xyz - input.world_pos);

    /* Compute the fog factor: 1.0 = fully visible, 0.0 = fully fogged.
     * The factor is clamped to [0, 1] so objects beyond the fog end
     * don't produce negative values. */
    float fog_factor = 1.0;

    if (fog_mode == 0)
    {
        /* Linear:  ramps from 1 at fog_start to 0 at fog_end. */
        fog_factor = saturate((fog_end - dist) / (fog_end - fog_start));
    }
    else if (fog_mode == 1)
    {
        /* Exponential:  smooth decay, never quite reaches zero. */
        fog_factor = saturate(exp(-fog_density * dist));
    }
    else
    {
        /* Exponential squared:  holds clear near, then drops sharply. */
        float f = fog_density * dist;
        fog_factor = saturate(exp(-(f * f)));
    }

    /* Blend between fog color and the lit surface color.
     * When fog_factor = 1 (close), we see the lit color.
     * When fog_factor = 0 (far), we see only fog. */
    float3 final_color = lerp(fog_color.rgb, lit_color, fog_factor);

    return float4(final_color, 1.0);
}
