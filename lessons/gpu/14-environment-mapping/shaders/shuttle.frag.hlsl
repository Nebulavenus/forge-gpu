/*
 * shuttle.frag.hlsl — Fragment shader with Blinn-Phong + environment mapping
 *
 * Extends Lesson 10's Blinn-Phong fragment shader with reflection mapping:
 *
 *   1. Sample the diffuse texture for the surface's own color.
 *   2. Compute the reflection vector R = reflect(-V, N).  This gives the
 *      direction that a mirror at this surface point would show.
 *   3. Sample the environment cube map along R to get the reflected color.
 *   4. Blend diffuse and reflected color: lerp(diffuse, env, reflectivity).
 *   5. Apply Blinn-Phong lighting (ambient + diffuse + specular) as before.
 *
 * The reflectivity parameter controls how mirror-like the surface is:
 *   0.0 = fully diffuse (no reflections)
 *   1.0 = perfect mirror (only reflections)
 *   0.6 = prominent reflections blended with the surface texture
 *
 * Uniform layout (80 bytes, 16-byte aligned):
 *   float4 base_color      (16 bytes)
 *   float4 light_dir       (16 bytes)
 *   float4 eye_pos         (16 bytes)
 *   uint   has_texture      (4 bytes)
 *   float  shininess        (4 bytes)
 *   float  ambient          (4 bytes)
 *   float  specular_str     (4 bytes)
 *   float  reflectivity     (4 bytes) — environment map blend factor
 *   float  padding[3]       (12 bytes) — pad to 16-byte boundary
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);   /* Diffuse texture (slot 0) */
TextureCube  env_tex     : register(t1, space2);   /* Environment cube map (slot 1) */
SamplerState diffuse_smp : register(s0, space2);   /* Sampler for diffuse */
SamplerState env_smp     : register(s1, space2);   /* Sampler for environment */

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;    /* material base color (RGBA)                    */
    float4 light_dir;     /* world-space light direction (toward light)    */
    float4 eye_pos;       /* world-space camera position                   */
    uint   has_texture;   /* non-zero = sample texture, zero = solid color */
    float  shininess;     /* specular exponent (e.g. 32, 64, 128)         */
    float  ambient;       /* ambient light intensity [0..1]                */
    float  specular_str;  /* specular intensity [0..1]                     */
    float  reflectivity;  /* environment map blend [0..1]                  */
    float3 _padding;      /* align to 16-byte boundary                     */
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
        surface_color = diffuse_tex.Sample(diffuse_smp, input.uv) * base_color;
    }
    else
    {
        surface_color = base_color;
    }

    /* ── Vectors for lighting ───────────────────────────────────────── */

    /* Normalize the interpolated normal.  After rasterizer interpolation
     * across a triangle, normals are no longer unit length. */
    float3 N = normalize(input.world_norm);

    /* Light direction — points from surface toward light. */
    float3 L = normalize(light_dir.xyz);

    /* View direction — from surface toward camera. */
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* ── Environment reflection ─────────────────────────────────────── */
    /* Compute the reflection of the view direction about the normal.
     * reflect() expects the incident vector pointing TOWARD the surface,
     * so we negate V.  The result R points outward — exactly the direction
     * to sample the environment cube map. */
    float3 R = reflect(-V, N);
    float3 env_color = env_tex.Sample(env_smp, R).rgb;

    /* Blend surface color with environment reflection. */
    float3 blended = lerp(surface_color.rgb, env_color, reflectivity);

    /* ── Ambient ────────────────────────────────────────────────────── */
    float3 ambient_term = ambient * blended;

    /* ── Diffuse (Lambert) ──────────────────────────────────────────── */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = NdotL * blended;

    /* ── Specular (Blinn) ───────────────────────────────────────────── */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess) * float3(1.0, 1.0, 1.0);

    /* ── Combine ────────────────────────────────────────────────────── */
    float3 final_color = ambient_term + diffuse_term + specular_term;

    return float4(final_color, surface_color.a);
}
