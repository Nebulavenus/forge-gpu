/*
 * scene.frag.hlsl — Fragment shader with normal mapping + Blinn-Phong
 *
 * Samples both a diffuse (base color) texture and a tangent-space normal
 * map.  The normal map encodes per-texel surface perturbations that add
 * geometric detail (bumps, grooves, rivets) without extra triangles.
 *
 * Normal map decoding:
 *   Normal maps store directions as RGB colors in [0, 1].  To convert
 *   back to the original [-1, 1] range:  N = texel * 2.0 - 1.0
 *
 *   glTF specifies OpenGL-convention normal maps (right-handed):
 *     R = tangent (X), G = bitangent (Y, up), B = normal (Z, outward)
 *   A flat surface encodes as (0.5, 0.5, 1.0) — which decodes to (0, 0, 1),
 *   pointing straight outward in tangent space.
 *
 * TBN matrix usage:
 *   The vertex shader passes the world-space T, B, N basis vectors.
 *   Constructing TBN = float3x3(T, B, N) and multiplying by the
 *   tangent-space normal transforms it to world space for lighting.
 *
 * Normal mode (uniform):
 *   0 = flat shading  — face normal from screen-space derivatives (ddx/ddy)
 *   1 = smooth shading — interpolated per-vertex normal
 *   2 = normal mapped  — per-texel normal from the normal map (default)
 *
 * Fragment samplers:
 *   register(t0/s0, space2) -> diffuse texture  (slot 0)
 *   register(t1/s1, space2) -> normal map        (slot 1)
 *
 * Fragment uniform:
 *   register(b0, space3) -> base color + lighting params (80 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    diffuse_tex : register(t0, space2);
SamplerState diffuse_smp : register(s0, space2);

Texture2D    normal_tex  : register(t1, space2);
SamplerState normal_smp  : register(s1, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;     /* RGBA color multiplier                       */
    float4 light_dir;      /* world-space light direction (toward light)  */
    float4 eye_pos;        /* world-space camera position                 */
    float  has_texture;    /* 1.0 = sample diffuse, 0.0 = color only     */
    float  has_normal_map; /* 1.0 = sample normal map, 0.0 = skip        */
    float  shininess;      /* specular exponent (e.g. 32, 64, 128)       */
    float  ambient;        /* ambient light intensity [0..1]              */
    float  specular_str;   /* specular intensity [0..1]                   */
    float  normal_mode;    /* 0 = flat, 1 = per-vertex, 2 = normal-mapped */
    float  _pad0;
    float  _pad1;
};

struct PSInput
{
    float4 clip_pos      : SV_Position;
    float2 uv            : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float3 world_pos     : TEXCOORD2;
    float3 world_tangent : TEXCOORD3;
    float3 world_bitan   : TEXCOORD4;
};

float4 main(PSInput input) : SV_Target
{
    /* ── Surface color ──────────────────────────────────────────────── */
    float4 texel = has_texture > 0.5
                 ? diffuse_tex.Sample(diffuse_smp, input.uv)
                 : float4(1.0, 1.0, 1.0, 1.0);
    float4 surface_color = texel * base_color;

    /* ── Determine the shading normal based on normal_mode ─────────── */
    float3 N;

    if (normal_mode > 1.5)
    {
        /* Mode 2: Normal mapping — sample the tangent-space normal map
         * and transform to world space using the TBN matrix.
         *
         * The TBN matrix columns are the world-space tangent, bitangent,
         * and normal.  Multiplying a tangent-space direction by this
         * matrix converts it to world space. */
        float3 T = normalize(input.world_tangent);
        float3 B = normalize(input.world_bitan);
        float3 Nv = normalize(input.world_normal);

        /* Construct TBN with T, B, N as rows.  In HLSL, mul(vec, mat)
         * computes vec.x * row0 + vec.y * row1 + vec.z * row2, which
         * gives us: n.x * T + n.y * B + n.z * N — exactly the tangent-
         * to-world transformation we need. */
        float3x3 TBN = float3x3(T, B, Nv);

        /* Decode the normal map: stored as [0, 1], convert to [-1, 1].
         * glTF uses OpenGL convention: +Y bitangent (green channel up). */
        float3 map_normal = normal_tex.Sample(normal_smp, input.uv).rgb;
        map_normal = map_normal * 2.0 - 1.0;

        /* Only apply normal map when the material has one; otherwise
         * fall back to the interpolated vertex normal. */
        if (has_normal_map > 0.5)
            N = normalize(mul(map_normal, TBN));
        else
            N = Nv;
    }
    else if (normal_mode > 0.5)
    {
        /* Mode 1: Per-vertex smooth shading — use the interpolated
         * vertex normal.  This is the standard approach from Lesson 10,
         * which produces smooth shading across curved surfaces. */
        N = normalize(input.world_normal);
    }
    else
    {
        /* Mode 0: Flat shading — compute the face normal from screen-
         * space partial derivatives of the world position.  The cross
         * product of ddx and ddy gives a vector perpendicular to the
         * triangle face, producing a faceted look that reveals the
         * underlying triangle mesh. */
        N = normalize(cross(ddx(input.world_pos), ddy(input.world_pos)));
    }

    /* ── Vectors for lighting ───────────────────────────────────────── */
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
    float3 specular_term = specular_str * pow(NdotH, shininess)
                         * float3(1.0, 1.0, 1.0);

    /* ── Combine ────────────────────────────────────────────────────── */
    float3 final_color = ambient_term + diffuse_term + specular_term;

    return float4(final_color, surface_color.a);
}
