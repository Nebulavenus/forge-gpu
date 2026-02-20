/*
 * scene.vert.hlsl — Vertex shader with TBN matrix for normal mapping
 *
 * Transforms vertices to clip space and world space, then constructs
 * the tangent-bitangent-normal (TBN) basis in world space.  The TBN
 * matrix lets the fragment shader transform normal map samples from
 * tangent space (the coordinate system baked into the texture) into
 * world space (where lighting is computed).
 *
 * TBN construction (Eric Lengyel's method):
 *   1. Normal (N): transform by the adjugate transpose of the model
 *      matrix's upper-left 3x3.  This preserves perpendicularity even
 *      under non-uniform scale (see Lesson 10).
 *   2. Tangent (T): transform by the model matrix (tangent vectors are
 *      surface directions, not perpendiculars — they follow geometry).
 *      Then re-orthogonalize against N using Gram-Schmidt.
 *   3. Bitangent (B): cross(N, T) * handedness.  The tangent.w component
 *      stores +1 or -1, encoding whether the UV space is mirrored.  This
 *      is critical for models with mirrored UVs (like NormalTangentMirrorTest).
 *
 * Vertex attributes (mapped from SDL GPU vertex attribute locations):
 *   TEXCOORD0 -> position (float3)  — location 0
 *   TEXCOORD1 -> normal   (float3)  — location 1
 *   TEXCOORD2 -> uv       (float2)  — location 2
 *   TEXCOORD3 -> tangent  (float4)  — location 3
 *
 * Vertex uniform:
 *   register(b0, space1) -> MVP + model matrices (128 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;
    column_major float4x4 model;
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3;   /* xyz = tangent direction, w = handedness */
};

struct VSOutput
{
    float4 clip_pos      : SV_Position;
    float2 uv            : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float3 world_pos     : TEXCOORD2;
    float3 world_tangent : TEXCOORD3;
    float3 world_bitan   : TEXCOORD4;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Clip-space position for the rasterizer. */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space position — needed for the view direction (V = eye - P). */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* ── Normal transformation (adjugate transpose) ────────────────────
     * Normals are perpendicular to the surface, so they don't transform
     * the same way as positions.  The adjugate transpose of the upper-left
     * 3×3 preserves perpendicularity even under non-uniform scale.
     * See Lesson 10 for the full derivation. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    float3 N = normalize(mul(adj_t, input.normal));

    /* ── Tangent transformation ────────────────────────────────────────
     * Tangent vectors are directions in the surface plane.  Unlike normals,
     * they transform by the model matrix directly (they follow the surface
     * geometry, not its perpendicular). */
    float3 T = normalize(mul(m, input.tangent.xyz));

    /* ── Gram-Schmidt re-orthogonalization ──────────────────────────────
     * After applying different transformations (adjugate for N, direct
     * for T), the vectors may no longer be exactly perpendicular.
     * Gram-Schmidt projects out the N component from T, ensuring the
     * TBN matrix is orthonormal. */
    T = normalize(T - N * dot(N, T));

    /* ── Bitangent from cross product ──────────────────────────────────
     * The bitangent completes the tangent-space basis.  The handedness
     * (tangent.w) encodes whether the texture coordinate space is mirrored.
     * Without this sign, mirrored UVs would produce inverted normal maps.
     * The NormalTangentMirrorTest model specifically tests this. */
    float3 B = cross(N, T) * input.tangent.w;

    output.world_normal  = N;
    output.world_tangent = T;
    output.world_bitan   = B;

    /* Pass UVs through for texture sampling. */
    output.uv = input.uv;

    return output;
}
