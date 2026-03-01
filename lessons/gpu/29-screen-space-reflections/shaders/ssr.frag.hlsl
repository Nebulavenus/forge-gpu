/*
 * SSR fragment shader — screen-space reflection via linear ray marching.
 *
 * For each pixel the shader:
 *   1. Reconstructs the view-space position from the depth buffer
 *   2. Reads the view-space normal from the G-buffer
 *   3. Reflects the view direction across the surface normal
 *   4. Marches along the reflected ray in view space, projecting each
 *      sample back to screen space to compare against the depth buffer
 *   5. When the ray passes behind scene geometry (depth buffer hit),
 *      the shader samples the scene color at that screen location
 *
 * Output: float4(reflection_color.rgb, hit_alpha)
 *   hit_alpha = 0 means no reflection (ray missed or left the screen)
 *   hit_alpha = 1 means full reflection (ray hit scene geometry)
 *   Intermediate values occur near screen edges (fade out).
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── SSR configuration ─────────────────────────────────────────────── */

/* Maximum number of ray march steps per pixel. */
#define SSR_MAX_STEPS 64

/* View-space distance per step along the reflected ray. */
#define SSR_STEP_SIZE 0.05

/* Depth tolerance for hit detection — how far behind a surface the ray
 * can be and still count as intersecting it. */
#define SSR_THICKNESS 0.5

/* Maximum view-space travel distance before the ray is abandoned. */
#define SSR_MAX_DISTANCE 50.0

/* Screen-edge fade margin — reflections fade when the sample point
 * is within this fraction of the screen border. */
#define SSR_EDGE_FADE_START 0.8

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

    /* Choose step size: use cbuffer override if provided, else default. */
    float step_size    = (ssr_step_size > 0.0) ? ssr_step_size : SSR_STEP_SIZE;
    float max_distance = (ssr_max_distance > 0.0) ? ssr_max_distance : SSR_MAX_DISTANCE;

    /* ── Linear ray march in view space ────────────────────────────── */
    float3 ray_pos = view_pos;
    float  traveled = 0.0;

    for (int i = 0; i < SSR_MAX_STEPS; i++)
    {
        /* Advance one step along the reflected ray. */
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

        /* Sample the depth buffer at the projected position. */
        float sample_depth = depth_tex.Sample(depth_smp, sample_uv).r;
        float3 sample_view = reconstruct_view_pos(sample_uv, sample_depth);

        /* Compare depths: the ray has hit geometry when its view-space Z
         * is behind (more negative than) the scene surface, within the
         * thickness tolerance. Negative Z = further from camera. */
        float depth_diff = ray_pos.z - sample_view.z;

        if (depth_diff < 0.0 && depth_diff > -SSR_THICKNESS)
        {
            /* ── Hit detected ──────────────────────────────────── */
            float3 hit_color = color_tex.Sample(color_smp, sample_uv).rgb;

            /* Fade near screen edges to hide hard cutoff artifacts. */
            float edge_alpha = screen_edge_fade(sample_uv);

            /* Fade with travel distance so distant reflections are subtle. */
            float distance_alpha = 1.0 - saturate(traveled / max_distance);

            float alpha = edge_alpha * distance_alpha * reflectivity;
            return float4(hit_color, alpha);
        }
    }

    /* No hit — return transparent black. */
    return float4(0.0, 0.0, 0.0, 0.0);
}
