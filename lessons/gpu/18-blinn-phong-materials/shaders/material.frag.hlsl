/*
 * material.frag.hlsl — Fragment shader with per-material Blinn-Phong
 *
 * Extends Lesson 10's lighting shader with full material properties.
 * Instead of a single surface color plus scalar ambient/specular
 * strengths, each material now defines three RGB colors:
 *
 *   - Ambient   — light reflected even without direct illumination.
 *                 Controls the object's color in shadow.
 *   - Diffuse   — surface color under direct light (Lambert's law).
 *                 This is what most people think of as "the color."
 *   - Specular  — highlight color and intensity.
 *                 Dielectrics (plastic, stone) have near-white specular.
 *                 Conductors (metals) have tinted specular matching
 *                 their reflectance color — gold has gold highlights.
 *   - Shininess — specular exponent controlling highlight tightness
 *                 (packed in mat_specular.w).
 *
 * The Blinn-Phong equation per material:
 *   ambient  = mat_ambient.rgb
 *   diffuse  = max(dot(N, L), 0) * mat_diffuse.rgb * surface_color
 *   specular = pow(max(dot(N, H), 0), shininess) * mat_specular.rgb
 *   result   = ambient + diffuse + specular
 *
 * When has_texture is set, the diffuse texture modulates mat_diffuse.
 * When unset, mat_diffuse alone defines the diffuse color.
 *
 * Uniform layout (96 bytes, 16-byte aligned):
 *   float4 mat_ambient    (16 bytes) — material ambient color (rgb)
 *   float4 mat_diffuse    (16 bytes) — material diffuse color (rgb)
 *   float4 mat_specular   (16 bytes) — specular color (rgb) + shininess (w)
 *   float4 light_dir      (16 bytes) — world-space light direction (xyz)
 *   float4 eye_pos        (16 bytes) — world-space camera position (xyz)
 *   uint   has_texture     (4 bytes) — whether to sample diffuse texture
 *   float3 _pad           (12 bytes) — padding to 96 bytes
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
    /* Start with the material's diffuse color.  When a texture is
     * bound, it modulates the material color — allowing the same
     * material to work with textured or untextured geometry. */
    float3 diffuse_color = mat_diffuse.rgb;
    if (has_texture)
    {
        diffuse_color *= diffuse_tex.Sample(smp, input.uv).rgb;
    }

    /* ── Vectors for lighting ──────────────────────────────────────── */

    /* Normalize the interpolated normal — after rasterizer interpolation
     * across a triangle, normals are no longer unit length. */
    float3 N = normalize(input.world_norm);

    /* Light direction — already normalized on CPU, normalize again
     * to be safe.  Points from surface toward the light. */
    float3 L = normalize(light_dir.xyz);

    /* View direction — from surface point toward camera. */
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* ── Ambient ───────────────────────────────────────────────────── */
    /* The ambient term uses the material's ambient reflectance directly.
     * Different materials absorb different amounts of ambient light —
     * gold has warm amber ambient, jade has cool green, chrome is neutral. */
    float3 ambient_term = mat_ambient.rgb;

    /* ── Diffuse (Lambert) ─────────────────────────────────────────── */
    /* dot(N, L) gives the cosine of the angle between normal and light.
     * max(..., 0) clamps negative values — surfaces facing away from
     * the light get zero diffuse contribution. */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = NdotL * diffuse_color;

    /* ── Specular (Blinn) ──────────────────────────────────────────── */
    /* The half-vector H sits halfway between light and view directions.
     * When H aligns with N, we see a bright highlight.
     *
     * The key difference from Lesson 10: specular is now a full RGB
     * color, not always white.  Metals have colored specular highlights
     * matching their reflectance — gold highlights are golden, copper
     * highlights are reddish.  Dielectrics (plastics, stone) have
     * near-white specular because they reflect all wavelengths equally
     * at the specular angle. */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float  shininess = mat_specular.w;
    float3 specular_term = pow(NdotH, shininess) * mat_specular.rgb;

    /* ── Combine ───────────────────────────────────────────────────── */
    float3 final_color = ambient_term + diffuse_term + specular_term;

    return float4(final_color, 1.0);
}
