/*
 * Composite fragment shader — blends SSR reflections with the lit scene.
 *
 * Five display modes controlled by a uniform:
 *   mode 0: Final composite — scene color + SSR reflections blended
 *   mode 1: SSR only — reflection color on a black background
 *   mode 2: Normals — view-space normals visualized as color
 *   mode 3: Depth — linearized depth visualized as grayscale
 *   mode 4: World position — world-space coordinates visualized as color
 *
 * SPDX-License-Identifier: Zlib
 */

/* Near and far planes for depth linearization — must match main.c. */
#define NEAR_PLANE 0.1
#define FAR_PLANE  500.0

/* Scene color from the geometry pass (slot 0). */
Texture2D    color_tex  : register(t0, space2);
SamplerState color_smp  : register(s0, space2);

/* SSR result — rgb = reflection color, a = hit confidence (slot 1). */
Texture2D    ssr_tex    : register(t1, space2);
SamplerState ssr_smp    : register(s1, space2);

/* Scene depth buffer (slot 2). */
Texture2D    depth_tex  : register(t2, space2);
SamplerState depth_smp  : register(s2, space2);

/* View-space normals from the G-buffer (slot 3). */
Texture2D    normal_tex : register(t3, space2);
SamplerState normal_smp : register(s3, space2);

/* World-space position from the G-buffer (slot 4). */
Texture2D    wpos_tex   : register(t4, space2);
SamplerState wpos_smp   : register(s4, space2);

cbuffer CompositeParams : register(b0, space3)
{
    int   display_mode;    /* 0=composite, 1=SSR only, 2=normals, 3=depth, 4=world pos */
    float reflection_str;  /* global reflection strength multiplier                     */
    float2 _pad;
};

/* ── Linearize a [0,1] depth value to a view-space distance ────────── */

float linearize_depth(float d)
{
    /* Reverse the perspective projection depth formula:
     *   d = (far * near) / (far - z * (far - near))
     * solved for z gives the view-space distance. */
    return NEAR_PLANE * FAR_PLANE /
           (FAR_PLANE - d * (FAR_PLANE - NEAR_PLANE));
}

float4 main(float4 clip_pos : SV_Position,
            float2 uv       : TEXCOORD0) : SV_Target
{
    float3 scene_color = color_tex.Sample(color_smp, uv).rgb;
    float4 ssr_result  = ssr_tex.Sample(ssr_smp, uv);

    /* ── Mode 0: Final composite ───────────────────────────────────── */
    if (display_mode == 0)
    {
        /* Blend reflection over scene color using SSR alpha and global
         * reflection strength. Additive blending would over-brighten,
         * so we use a lerp to replace scene color with reflection. */
        float blend = ssr_result.a * reflection_str;
        float3 final_color = lerp(scene_color, ssr_result.rgb, blend);
        return float4(final_color, 1.0);
    }

    /* ── Mode 1: SSR only ──────────────────────────────────────────── */
    if (display_mode == 1)
    {
        /* Show reflection color modulated by its confidence alpha. */
        return float4(ssr_result.rgb * ssr_result.a, 1.0);
    }

    /* ── Mode 2: View-space normals ────────────────────────────────── */
    if (display_mode == 2)
    {
        float3 nrm = normal_tex.Sample(normal_smp, uv).xyz;
        /* Map [-1,1] normal components to [0,1] for visualization. */
        return float4(nrm * 0.5 + 0.5, 1.0);
    }

    /* ── Mode 3: Linearized depth ──────────────────────────────────── */
    if (display_mode == 3)
    {
        float d = depth_tex.Sample(depth_smp, uv).r;
        float linear_d = linearize_depth(d);
        /* Normalize to a visible range — divide by far plane. */
        float vis = saturate(linear_d / FAR_PLANE);
        return float4(vis, vis, vis, 1.0);
    }

    /* ── Mode 4: World-space position ──────────────────────────────── */
    if (display_mode == 4)
    {
        float3 wpos = wpos_tex.Sample(wpos_smp, uv).xyz;
        /* Map world coordinates to color; use frac to keep values visible
         * regardless of world scale. Divide by a moderate range first. */
        float3 vis = frac(wpos * 0.1);
        return float4(vis, 1.0);
    }

    /* Fallback — plain scene color. */
    return float4(scene_color, 1.0);
}
