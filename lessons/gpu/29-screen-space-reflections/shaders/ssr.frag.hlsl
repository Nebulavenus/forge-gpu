/*
 * SSR fragment shader — screen-space reflection via linear ray marching
 * with binary search refinement.
 *
 * For each pixel the shader:
 *   1. Reconstructs the view-space position from the depth buffer
 *   2. Reads the view-space normal from the G-buffer
 *   3. Reflects the view direction across the surface normal
 *   4. Marches along the reflected ray in view space, projecting each
 *      sample back to screen space to compare against the depth buffer
 *   5. When the ray passes behind scene geometry (coarse hit), refines
 *      the intersection with binary search for sub-pixel precision
 *   6. Rejects back-face hits where the surface normal faces away from
 *      the camera along the reflection direction
 *
 * Output: float4(reflection_color.rgb, hit_alpha)
 *   hit_alpha = 0 means no reflection (ray missed or left the screen)
 *   hit_alpha = 1 means full reflection (ray hit scene geometry)
 *   Intermediate values occur near screen edges (fade out).
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── SSR configuration ─────────────────────────────────────────────── */

/* Fallback values used when the cbuffer override is zero or absent. */
#define SSR_DEFAULT_MAX_STEPS    128
#define SSR_DEFAULT_STEP_SIZE    0.15
#define SSR_DEFAULT_THICKNESS    0.15
#define SSR_DEFAULT_MAX_DISTANCE 20.0

/* Number of binary search iterations to refine the hit point after the
 * linear march finds a coarse intersection.  Each iteration halves the
 * remaining error, so 8 steps give ~256x the precision of the linear
 * step size (0.15 / 256 ≈ 0.0006 view-space units). */
#define SSR_REFINE_STEPS 8

/* Minimum view-space travel before accepting a hit — prevents the ray
 * from immediately intersecting the surface it originated from. */
#define SSR_MIN_TRAVEL 0.3

/* Screen-edge fade margin — reflections fade when the sample point
 * is within this fraction of the screen border. */
#define SSR_EDGE_FADE_START 0.8

/* Back-face fade range — a surface can only appear in a reflection if
 * its normal faces *toward* the reflecting surface.  When
 * dot(reflect_dir, hit_normal) > 0, the hit surface faces the same way
 * as the reflected ray, so it's physically impossible to see it in the
 * reflection (e.g. a truck's roof cannot appear in a floor reflection
 * because both normals point upward).
 *
 * We fade from full to zero over the range [FADE_START, FADE_END].
 * Starting at 0.0 (the exact geometric boundary) with a small ramp
 * avoids hard cutoffs while rejecting impossible reflections. */
#define SSR_BACKFACE_FADE_START 0.0
#define SSR_BACKFACE_FADE_END   0.25

/* ── Texture bindings ──────────────────────────────────────────────── */

/* Lit scene color from the geometry pass (slot 0). */
Texture2D    color_tex   : register(t0, space2);
SamplerState color_smp   : register(s0, space2);

/* Scene depth buffer (slot 1). */
Texture2D    depth_tex   : register(t1, space2);
SamplerState depth_smp   : register(s1, space2);

/* View-space normals from the G-buffer (slot 2). */
Texture2D    normal_tex  : register(t2, space2);
SamplerState normal_smp  : register(s2, space2);

/* World-space position from the G-buffer (slot 3).
 * Alpha channel stores per-pixel reflectivity. */
Texture2D    wpos_tex    : register(t3, space2);
SamplerState wpos_smp    : register(s3, space2);

/* ── Uniforms ──────────────────────────────────────────────────────── */

cbuffer SSRParams : register(b0, space3)
{
    column_major float4x4 projection;     /* camera projection matrix          */
    column_major float4x4 inv_projection; /* inverse projection (depth->view)  */
    column_major float4x4 view_matrix;    /* camera view matrix (world->view)  */
    float2 screen_size;                   /* viewport width and height (px)    */
    float  ssr_step_size;                 /* per-step distance override        */
    float  ssr_max_distance;              /* max ray travel distance override  */
    int    ssr_max_steps;                 /* max iterations override           */
    float  ssr_thickness;                 /* depth hit tolerance override      */
    float2 _pad;                          /* align to 16 bytes                 */
};

/* ── Helper: project a view-space position to screen UV ────────────── */

float2 view_to_screen_uv(float3 view_pos)
{
    float4 clip = mul(projection, float4(view_pos, 1.0));
    float2 ndc  = clip.xy / clip.w;

    /* NDC [-1,1] to UV [0,1]. Flip Y for texture coordinates. */
    float2 uv;
    uv.x = ndc.x *  0.5 + 0.5;
    uv.y = ndc.y * -0.5 + 0.5;
    return uv;
}

/* ── Helper: reconstruct view-space position from depth + UV ───────── */

float3 reconstruct_view_pos(float2 uv, float depth)
{
    /* UV [0,1] to NDC [-1,1]. Flip Y back from texture space. */
    float2 ndc;
    ndc.x = uv.x *  2.0 - 1.0;
    ndc.y = uv.y * -2.0 + 1.0;

    float4 clip_pos = float4(ndc, depth, 1.0);
    float4 view_pos = mul(inv_projection, clip_pos);
    return view_pos.xyz / view_pos.w;
}

/* ── Helper: interleaved gradient noise (Jimenez 2014) ─────────────── */
/* Returns a value in [0, 1) that varies smoothly per pixel.  Used to
 * jitter the ray march starting offset so adjacent pixels do not step
 * in lock-step, converting regular staircase artifacts into noise. */

float interleaved_gradient_noise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

/* ── Helper: fade reflections near screen edges ────────────────────── */

float screen_edge_fade(float2 uv)
{
    /* Distance from the nearest screen edge, in [0, 0.5]. */
    float2 edge_dist = min(uv, 1.0 - uv);
    float  min_edge  = min(edge_dist.x, edge_dist.y);

    /* Fade margin: SSR_EDGE_FADE_START..1.0 maps to 1..0 on each side.
     * Convert the 0.8 threshold to a distance from the edge. */
    float fade_margin = (1.0 - SSR_EDGE_FADE_START) * 0.5;
    return smoothstep(0.0, fade_margin, min_edge);
}

/* ── Main SSR ray march ────────────────────────────────────────────── */

float4 main(float4 clip_pos : SV_Position,
            float2 uv       : TEXCOORD0) : SV_Target
{
    /* Read per-pixel reflectivity from the world-position alpha channel. */
    float reflectivity = wpos_tex.Sample(wpos_smp, uv).a;

    /* Skip non-reflective pixels early. */
    if (reflectivity < 0.01)
        return float4(0.0, 0.0, 0.0, 0.0);

    /* ── Reconstruct view-space position from depth ────────────────── */
    float depth    = depth_tex.Sample(depth_smp, uv).r;
    float3 view_pos = reconstruct_view_pos(uv, depth);

    /* ── Read view-space normal ────────────────────────────────────── */
    float3 view_nrm = normalize(normal_tex.Sample(normal_smp, uv).xyz);

    /* ── Compute reflected ray direction in view space ─────────────── */
    float3 view_dir    = normalize(view_pos);
    float3 reflect_dir = reflect(view_dir, view_nrm);

    /* Choose parameters: use cbuffer overrides if provided, else defaults. */
    float step_size    = (ssr_step_size > 0.0)    ? ssr_step_size    : SSR_DEFAULT_STEP_SIZE;
    float max_distance = (ssr_max_distance > 0.0)  ? ssr_max_distance : SSR_DEFAULT_MAX_DISTANCE;
    int   max_steps    = (ssr_max_steps > 0)       ? ssr_max_steps    : SSR_DEFAULT_MAX_STEPS;
    float thickness    = (ssr_thickness > 0.0)     ? ssr_thickness    : SSR_DEFAULT_THICKNESS;

    /* ── Jitter the ray start ────────────────────────────────────────── */
    /* Without jitter, every pixel's ray steps fall on the same regular
     * grid (0.15, 0.30, 0.45 …).  Adjacent floor pixels that reflect
     * an angled surface (like a truck hood) snap to different grid
     * positions, producing visible staircase artifacts.
     *
     * Adding a per-pixel random offset in [0, step_size) randomises the
     * grid so neighbouring pixels no longer step in lock-step.  The
     * staircase becomes noise, which is far less noticeable.  Interleaved
     * gradient noise (Jimenez 2014) gives a well-distributed value per
     * pixel without needing a noise texture. */
    float jitter = interleaved_gradient_noise(clip_pos.xy) * step_size;

    /* ── Phase 1: Linear ray march in view space ────────────────────── */
    /* March along the reflected ray in fixed steps.  When the ray passes
     * behind a scene surface (depth buffer hit), record the interval
     * between the last "in front" position and the first "behind"
     * position for binary search refinement. */
    float3 ray_pos  = view_pos + reflect_dir * jitter;
    float3 prev_pos = view_pos;
    float  traveled = jitter;
    bool   found_hit = false;

    for (int i = 0; i < max_steps; i++)
    {
        prev_pos  = ray_pos;
        ray_pos  += reflect_dir * step_size;
        traveled += step_size;

        /* Abandon the ray if it has traveled too far. */
        if (traveled > max_distance)
            break;

        /* Project the current ray position back to screen UV. */
        float2 sample_uv = view_to_screen_uv(ray_pos);

        /* Abandon the ray if it leaves the screen. */
        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 ||
            sample_uv.y < 0.0 || sample_uv.y > 1.0)
            break;

        /* Skip early steps to avoid self-intersection — the ray can
         * false-hit the surface it started from at shallow angles. */
        if (traveled < SSR_MIN_TRAVEL)
            continue;

        /* Sample the depth buffer at the projected position. */
        float sample_depth = depth_tex.Sample(depth_smp, sample_uv).r;
        float3 sample_view = reconstruct_view_pos(sample_uv, sample_depth);

        /* Compare depths: the ray has hit geometry when its view-space Z
         * is behind (more negative than) the scene surface, within the
         * thickness tolerance. Negative Z = further from camera. */
        float depth_diff = ray_pos.z - sample_view.z;

        if (depth_diff < 0.0 && depth_diff > -thickness)
        {
            found_hit = true;
            break;
        }
    }

    if (!found_hit)
        return float4(0.0, 0.0, 0.0, 0.0);

    /* ── Phase 2: Binary search refinement ───────────────────────── */
    /* The linear march found a coarse hit between prev_pos (in front
     * of the surface) and ray_pos (behind it).  Binary search narrows
     * this interval to find a much more precise intersection point.
     * Each iteration halves the error, giving sub-pixel accuracy. */
    float3 refine_lo = prev_pos;   /* last position in front of surface */
    float3 refine_hi = ray_pos;    /* first position behind surface     */

    for (int j = 0; j < SSR_REFINE_STEPS; j++)
    {
        float3 mid = (refine_lo + refine_hi) * 0.5;
        float2 mid_uv = view_to_screen_uv(mid);

        /* Abandon refinement if the midpoint leaves the screen. */
        if (mid_uv.x < 0.0 || mid_uv.x > 1.0 ||
            mid_uv.y < 0.0 || mid_uv.y > 1.0)
            break;

        float  mid_depth = depth_tex.Sample(depth_smp, mid_uv).r;
        float3 mid_view  = reconstruct_view_pos(mid_uv, mid_depth);
        float  mid_diff  = mid.z - mid_view.z;

        if (mid_diff < 0.0)
            refine_hi = mid;   /* still behind surface — narrow upper */
        else
            refine_lo = mid;   /* in front of surface — narrow lower  */
    }

    /* Use the refined "behind" position as the final hit point. */
    float2 hit_uv = view_to_screen_uv(refine_hi);

    /* Clamp to valid screen space (in case refinement drifted). */
    if (hit_uv.x < 0.0 || hit_uv.x > 1.0 ||
        hit_uv.y < 0.0 || hit_uv.y > 1.0)
        return float4(0.0, 0.0, 0.0, 0.0);

    /* ── Phase 3: Compute final color and confidence ────────────── */
    float3 hit_color = color_tex.Sample(color_smp, hit_uv).rgb;

    /* Fade near screen edges to hide hard cutoff artifacts. */
    float edge_alpha = screen_edge_fade(hit_uv);

    /* Fade with travel distance so distant reflections are subtle. */
    float distance_alpha = 1.0 - saturate(traveled / max_distance);

    /* Back-face fade — when the hit surface normal faces the same
     * direction as the reflected ray, the screen-visible color may
     * not match what the reflection should show.  Fade these hits
     * smoothly instead of rejecting them outright (hard cutoffs
     * create visible holes at certain viewing angles). */
    float3 hit_normal = normalize(normal_tex.Sample(normal_smp, hit_uv).xyz);
    float facing = dot(reflect_dir, hit_normal);
    float backface_fade = 1.0 - smoothstep(SSR_BACKFACE_FADE_START,
                                            SSR_BACKFACE_FADE_END, facing);

    float alpha = edge_alpha * distance_alpha * backface_fade * reflectivity;
    return float4(hit_color, alpha);
}
