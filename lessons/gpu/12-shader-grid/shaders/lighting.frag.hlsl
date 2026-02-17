/*
 * lighting.frag.hlsl — Fragment shader with Blinn-Phong lighting
 *
 * Implements the three components of the Blinn-Phong lighting model:
 *
 *   1. Ambient  — a constant minimum brightness so nothing is pure black.
 *                 Real scenes have light bouncing everywhere; ambient fakes it.
 *
 *   2. Diffuse  — the main shading.  Surfaces facing the light are bright,
 *                 surfaces facing away are dark.  Uses Lambert's cosine law:
 *                 brightness = max(dot(N, L), 0).
 *
 *   3. Specular — shiny highlights.  Uses the Blinn half-vector:
 *                 H = normalize(L + V), then pow(max(dot(N, H), 0), shininess).
 *                 Higher shininess = tighter, shinier highlights.
 *
 * Why Blinn instead of Phong?
 *   Original Phong uses reflect(L, N) and computes dot(R, V).
 *   Blinn replaces this with H = normalize(L + V) and dot(N, H).
 *   Blinn is cheaper (no reflect), matches real-world BRDF better at
 *   grazing angles, and has been the default in real-time graphics for
 *   decades (OpenGL's fixed-function pipeline used Blinn).
 *
 * Uniform layout (64 bytes, 16-byte aligned):
 *   float4 base_color   (16 bytes) — material color (RGBA)
 *   float4 light_dir    (16 bytes) — world-space light direction (xyz, w unused)
 *   float4 eye_pos      (16 bytes) — world-space camera position (xyz, w unused)
 *   uint   has_texture   (4 bytes)
 *   float  shininess     (4 bytes) — specular exponent (higher = tighter)
 *   float  ambient       (4 bytes) — ambient intensity [0..1]
 *   float  specular      (4 bytes) — specular intensity [0..1]
 *
 * We use float4 for light_dir and eye_pos even though they're vec3 because
 * HLSL cbuffer packing aligns float3 to 16 bytes anyway.  Using float4
 * makes the C struct layout explicit and avoids packing surprises.
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState smp         : register(s0, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;    /* material base color (RGBA)                    */
    float4 light_dir;     /* world-space light direction (toward light)    */
    float4 eye_pos;       /* world-space camera position                   */
    uint   has_texture;   /* non-zero = sample texture, zero = solid color */
    float  shininess;     /* specular exponent (e.g. 32, 64, 128)         */
    float  ambient;       /* ambient light intensity [0..1]                */
    float  specular_str;  /* specular intensity [0..1]                     */
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
    /* ── Surface color ──────────────────────────────────────────────── */
    float4 surface_color;
    if (has_texture)
    {
        surface_color = diffuse_tex.Sample(smp, input.uv) * base_color;
    }
    else
    {
        surface_color = base_color;
    }

    /* ── Vectors for lighting ───────────────────────────────────────── */

    /* Normalize the interpolated normal.  After rasterizer interpolation
     * across a triangle, normals are no longer unit length.  This is the
     * most common lighting bug — forgetting to normalize here. */
    float3 N = normalize(input.world_norm);

    /* Light direction — already normalized on the CPU, but normalize
     * again to be safe.  Points FROM the surface TOWARD the light. */
    float3 L = normalize(light_dir.xyz);

    /* View direction — from the surface point toward the camera. */
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* ── Ambient ────────────────────────────────────────────────────── */
    /* A flat base brightness.  Without ambient, surfaces facing away
     * from the light would be completely black. */
    float3 ambient_term = ambient * surface_color.rgb;

    /* ── Diffuse (Lambert) ──────────────────────────────────────────── */
    /* dot(N, L) gives the cosine of the angle between normal and light.
     * max(..., 0) clamps negative values — a surface facing away from
     * the light gets zero diffuse contribution, not negative light. */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = NdotL * surface_color.rgb;

    /* ── Specular (Blinn) ───────────────────────────────────────────── */
    /* The half-vector H sits halfway between the light and view vectors.
     * When H aligns with N, we see a bright highlight.  pow() controls
     * how tight the highlight is: higher shininess = smaller, sharper. */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess) * float3(1.0, 1.0, 1.0);

    /* ── Combine ────────────────────────────────────────────────────── */
    float3 final_color = ambient_term + diffuse_term + specular_term;

    return float4(final_color, surface_color.a);
}
