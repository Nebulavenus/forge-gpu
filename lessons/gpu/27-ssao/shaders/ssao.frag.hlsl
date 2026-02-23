/*
 * SSAO fragment shader — hemisphere kernel sampling in view space.
 *
 * Screen-Space Ambient Occlusion estimates how much ambient light reaches
 * each pixel by sampling the depth buffer in a hemisphere around the
 * surface normal. Occluded samples (behind nearby geometry) darken the
 * pixel, simulating contact shadows in crevices and corners.
 *
 * Algorithm (John Chapman / LearnOpenGL approach):
 *   1. Reconstruct view-space position from depth via inverse projection
 *   2. Read view-space normal from the G-buffer
 *   3. Build a TBN matrix using the normal and a random rotation vector
 *   4. For each of 64 hemisphere samples:
 *      - Transform to view space, offset from fragment position
 *      - Project to screen UV, sample the depth buffer
 *      - Compare: closer surface → occluded
 *      - Range check to prevent distant geometry from contributing
 *   5. Output: 1.0 - (occluded_count / 64)
 *
 * The random rotation vector comes from a 4x4 tiled noise texture,
 * optionally jittered with Interleaved Gradient Noise (Jimenez 2014)
 * to break the repeating pattern.
 *
 * SPDX-License-Identifier: Zlib
 */

#define KERNEL_SIZE 64
#define TWO_PI      6.28318530718

/* View-space normals from the G-buffer (slot 0). */
Texture2D    normal_tex : register(t0, space2);
SamplerState normal_smp : register(s0, space2);

/* Scene depth buffer (slot 1). */
Texture2D    depth_tex  : register(t1, space2);
SamplerState depth_smp  : register(s1, space2);

/* 4x4 random rotation noise texture (slot 2). */
Texture2D    noise_tex  : register(t2, space2);
SamplerState noise_smp  : register(s2, space2);

cbuffer SSAOParams : register(b0, space3)
{
    float4   samples[KERNEL_SIZE]; /* hemisphere kernel (xyz + pad) */
    column_major float4x4 projection;     /* camera projection matrix      */
    column_major float4x4 inv_projection; /* inverse projection            */
    float2   noise_scale;         /* screen_size / noise_size      */
    float    radius;              /* sample hemisphere radius      */
    float    bias;                /* self-occlusion bias           */
    int      use_ign_jitter;      /* 1 = add IGN rotation jitter   */
    float3   _pad;
};

/* ── Interleaved Gradient Noise (Jimenez 2014) ──────────────────────── */

float ign(float2 screen_pos)
{
    float3 ign_coeffs = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(ign_coeffs.z * frac(dot(screen_pos, ign_coeffs.xy)));
}

/* ── Reconstruct view-space position from depth ─────────────────────── */

float3 view_pos_from_depth(float2 uv, float depth)
{
    /* Map UV [0,1] to NDC [-1,1]. Flip Y because texture V=0 is top. */
    float2 ndc_xy = uv * 2.0 - 1.0;
    ndc_xy.y = -ndc_xy.y;

    /* Build clip-space position and unproject via inverse projection. */
    float4 clip = float4(ndc_xy, depth, 1.0);
    float4 view = mul(inv_projection, clip);
    return view.xyz / view.w;
}

float4 main(float4 clip_pos : SV_Position,
            float2 uv       : TEXCOORD0) : SV_Target
{
    /* ── Read G-buffer ──────────────────────────────────────────────── */
    float  depth      = depth_tex.Sample(depth_smp, uv).r;
    float3 view_nrm   = normalize(normal_tex.Sample(normal_smp, uv).xyz);
    float3 frag_pos   = view_pos_from_depth(uv, depth);

    /* Skip sky pixels (depth = 1.0 means no geometry). */
    if (depth >= 1.0)
        return float4(1.0, 1.0, 1.0, 1.0);

    /* ── Noise vector for random TBN rotation ───────────────────────── */
    float3 random_vec = noise_tex.Sample(noise_smp, uv * noise_scale).xyz;

    /* Optionally jitter the rotation with IGN to break the 4x4 tiling. */
    if (use_ign_jitter)
    {
        float angle = ign(clip_pos.xy) * TWO_PI;
        float s, c;
        sincos(angle, s, c);
        /* Rotate the noise vector in the XY plane. */
        float2 rotated = float2(
            random_vec.x * c - random_vec.y * s,
            random_vec.x * s + random_vec.y * c);
        random_vec = float3(rotated, 0.0);
    }

    /* ── Build TBN matrix (Gram-Schmidt orthonormalization) ─────────── */
    float3 tangent   = normalize(random_vec - view_nrm * dot(random_vec, view_nrm));
    float3 bitangent = cross(view_nrm, tangent);
    float3x3 TBN     = float3x3(tangent, bitangent, view_nrm);

    /* ── Sample hemisphere kernel ───────────────────────────────────── */
    float occlusion = 0.0;

    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        /* Transform kernel sample from tangent space to view space. */
        float3 sample_dir = mul(samples[i].xyz, TBN);
        float3 sample_pos = frag_pos + sample_dir * radius;

        /* Project sample position to screen UV. */
        float4 offset = mul(projection, float4(sample_pos, 1.0));
        offset.xy /= offset.w;
        /* NDC [-1,1] to UV [0,1], flip Y. */
        float2 sample_uv = offset.xy * 0.5 + 0.5;
        sample_uv.y = 1.0 - sample_uv.y;

        /* Sample the depth buffer at the projected UV. */
        float sample_depth = depth_tex.Sample(depth_smp, sample_uv).r;
        float3 stored_pos  = view_pos_from_depth(sample_uv, sample_depth);

        /* Occlusion test: is the stored surface closer than our sample? */
        float range_check = smoothstep(0.0, 1.0,
            radius / abs(frag_pos.z - stored_pos.z));
        occlusion += (stored_pos.z >= sample_pos.z + bias ? 1.0 : 0.0)
                     * range_check;
    }

    float ao = 1.0 - (occlusion / (float)KERNEL_SIZE);
    return float4(ao, ao, ao, 1.0);
}
