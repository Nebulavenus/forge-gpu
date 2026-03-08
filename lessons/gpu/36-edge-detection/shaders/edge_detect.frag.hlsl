/*
 * Edge detection fragment shader — Sobel operator on depth and normals.
 *
 * Reads the depth buffer and view-space normal buffer from the scene pass,
 * applies a 3x3 Sobel filter to detect discontinuities, and composites
 * black outlines over the original scene color.
 *
 * Supports three edge sources: depth only, normals only, or combined
 * (max of both).  A debug mode visualizes the raw depth and normal buffers.
 *
 * Fragment samplers (space2):
 *   slot 0 -> depth buffer texture + nearest-clamp sampler
 *   slot 1 -> normal buffer texture + nearest-clamp sampler
 *   slot 2 -> scene color texture + linear-clamp sampler
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: edge detection parameters (32 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Edge source modes — matches EdgeSource enum in main.c. */
#define EDGE_SOURCE_DEPTH    0
#define EDGE_SOURCE_NORMAL   1
#define EDGE_SOURCE_COMBINED 2

/* Depth buffer (slot 0). */
Texture2D    depth_tex : register(t0, space2);
SamplerState depth_smp : register(s0, space2);

/* Normal buffer (slot 1). */
Texture2D    normal_tex : register(t1, space2);
SamplerState normal_smp : register(s1, space2);

/* Scene color buffer (slot 2). */
Texture2D    color_tex : register(t2, space2);
SamplerState color_smp : register(s2, space2);

cbuffer EdgeDetectUniforms : register(b0, space3)
{
    float2 texel_size;       /* 1.0 / render target dimensions             */
    float  depth_threshold;  /* Sobel magnitude threshold for depth edges  */
    float  normal_threshold; /* Sobel magnitude threshold for normal edges */
    int    edge_source;      /* 0 = depth, 1 = normal, 2 = combined       */
    int    show_debug;       /* 1 = show raw depth/normal buffers          */
    float2 _pad0;
};

/* ── 3x3 Sobel filter on a scalar field ────────────────────────────── */

/* Sobel horizontal kernel Gx:
 *   -1  0  +1
 *   -2  0  +2
 *   -1  0  +1
 *
 * Sobel vertical kernel Gy:
 *   -1  -2  -1
 *    0   0   0
 *   +1  +2  +1
 *
 * Returns gradient magnitude = sqrt(Gx^2 + Gy^2). */

float sobel_depth(float2 uv)
{
    /* Sample 3x3 neighborhood of raw hardware depth values.
     * Raw depth varies smoothly on flat surfaces and avoids the dark banding
     * artifacts that linearized depth produces at grazing angles. */
    float tl = depth_tex.Sample(depth_smp, uv + float2(-texel_size.x,  texel_size.y)).r;
    float tc = depth_tex.Sample(depth_smp, uv + float2( 0.0,           texel_size.y)).r;
    float tr = depth_tex.Sample(depth_smp, uv + float2( texel_size.x,  texel_size.y)).r;
    float ml = depth_tex.Sample(depth_smp, uv + float2(-texel_size.x,  0.0         )).r;
    float mr = depth_tex.Sample(depth_smp, uv + float2( texel_size.x,  0.0         )).r;
    float bl = depth_tex.Sample(depth_smp, uv + float2(-texel_size.x, -texel_size.y)).r;
    float bc = depth_tex.Sample(depth_smp, uv + float2( 0.0,          -texel_size.y)).r;
    float br = depth_tex.Sample(depth_smp, uv + float2( texel_size.x, -texel_size.y)).r;

    /* Horizontal and vertical gradients. */
    float gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    float gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;

    return sqrt(gx * gx + gy * gy);
}

/* ── 3x3 Sobel filter on a vector field (normals) ──────────────────── */

/* Applies Sobel independently to each component of the normal buffer
 * and returns the combined gradient magnitude. */

float sobel_normal(float2 uv)
{
    /* Sample 3x3 neighborhood — RGBA16F stores normals directly in [-1,1]. */
    float3 tl = normal_tex.Sample(normal_smp, uv + float2(-texel_size.x,  texel_size.y)).rgb;
    float3 tc = normal_tex.Sample(normal_smp, uv + float2( 0.0,           texel_size.y)).rgb;
    float3 tr = normal_tex.Sample(normal_smp, uv + float2( texel_size.x,  texel_size.y)).rgb;
    float3 ml = normal_tex.Sample(normal_smp, uv + float2(-texel_size.x,  0.0         )).rgb;
    float3 mr = normal_tex.Sample(normal_smp, uv + float2( texel_size.x,  0.0         )).rgb;
    float3 bl = normal_tex.Sample(normal_smp, uv + float2(-texel_size.x, -texel_size.y)).rgb;
    float3 bc = normal_tex.Sample(normal_smp, uv + float2( 0.0,          -texel_size.y)).rgb;
    float3 br = normal_tex.Sample(normal_smp, uv + float2( texel_size.x, -texel_size.y)).rgb;

    /* Horizontal and vertical gradients per component. */
    float3 gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    float3 gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;

    /* Combined magnitude across all three components. */
    return sqrt(dot(gx, gx) + dot(gy, gy));
}

/* ── Sky mask — suppress edges at the far plane ────────────────────── */

/* Returns true if any sample in the 3x3 neighborhood is at the far plane.
 * This prevents false edges where geometry meets the clear color. */
bool is_sky_region(float2 uv)
{
    float sky_threshold = 0.999;
    float center = depth_tex.Sample(depth_smp, uv).r;
    if (center >= sky_threshold) return true;

    /* Check corners — if any neighbor is sky, suppress the edge. */
    float tl = depth_tex.Sample(depth_smp, uv + float2(-texel_size.x,  texel_size.y)).r;
    float tr = depth_tex.Sample(depth_smp, uv + float2( texel_size.x,  texel_size.y)).r;
    float bl = depth_tex.Sample(depth_smp, uv + float2(-texel_size.x, -texel_size.y)).r;
    float br = depth_tex.Sample(depth_smp, uv + float2( texel_size.x, -texel_size.y)).r;

    return (tl >= sky_threshold || tr >= sky_threshold ||
            bl >= sky_threshold || br >= sky_threshold);
}

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    float2 uv = input.uv;

    /* ── Debug mode — visualize raw buffers ────────────────────────── */

    if (show_debug != 0)
    {
        if (uv.x < 0.5)
        {
            /* Left half: raw depth as grayscale (inverted for visibility). */
            float d = depth_tex.Sample(depth_smp, uv).r;
            /* Raw depth is near 0 for close objects, near 1 for far.
             * Raise to a power to spread the visible range. */
            float vis = pow(d, 50.0);
            return float4(vis, vis, vis, 1.0);
        }
        else
        {
            /* Right half: view-space normals as RGB (remap [-1,1] to [0,1]). */
            float3 n = normal_tex.Sample(normal_smp, uv).rgb;
            return float4(n * 0.5 + 0.5, 1.0);
        }
    }

    /* ── Compute edge strength ────────────────────────────────────── */

    float edge = 0.0;

    /* Suppress edges in sky regions to avoid false outlines. */
    if (!is_sky_region(uv))
    {
        if (edge_source == EDGE_SOURCE_DEPTH || edge_source == EDGE_SOURCE_COMBINED)
        {
            float depth_edge = sobel_depth(uv);
            depth_edge = smoothstep(depth_threshold, depth_threshold * 2.0, depth_edge);
            edge = max(edge, depth_edge);
        }

        if (edge_source == EDGE_SOURCE_NORMAL || edge_source == EDGE_SOURCE_COMBINED)
        {
            float normal_edge = sobel_normal(uv);
            normal_edge = smoothstep(normal_threshold, normal_threshold * 2.0, normal_edge);
            edge = max(edge, normal_edge);
        }
    }

    /* ── Composite edges over scene color ─────────────────────────── */

    float3 scene_color = color_tex.Sample(color_smp, uv).rgb;
    float3 edge_color  = float3(0.0, 0.0, 0.0); /* black outlines */

    float3 result = lerp(scene_color, edge_color, edge);

    return float4(result, 1.0);
}
