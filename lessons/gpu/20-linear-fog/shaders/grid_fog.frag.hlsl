/*
 * grid_fog.frag.hlsl — Procedural anti-aliased grid with distance fog
 *
 * Extends Lesson 18's grid shader with the same three fog modes used by
 * the scene objects.  The grid floor and the scene objects share the same
 * fog parameters, so both fade consistently into the background — this is
 * essential for a convincing atmospheric effect.
 *
 * The grid pattern, anti-aliasing, and Blinn-Phong lighting are unchanged
 * from Lesson 12.  Fog is applied as the final step, blending the lit
 * grid color toward the fog color based on distance to the camera.
 *
 * Fragment uniform layout (128 bytes, 16-byte aligned):
 *   float4 line_color       (16 bytes) — grid line color
 *   float4 bg_color         (16 bytes) — background color between lines
 *   float4 light_dir        (16 bytes) — world-space light direction
 *   float4 eye_pos          (16 bytes) — world-space camera position
 *   float  grid_spacing      (4 bytes)
 *   float  line_width        (4 bytes)
 *   float  fade_distance     (4 bytes)
 *   float  ambient           (4 bytes)
 *   float  shininess         (4 bytes)
 *   float  specular_str      (4 bytes)
 *   float  _pad0             (4 bytes)
 *   float  _pad1             (4 bytes)
 *   float4 fog_color        (16 bytes) — fog/clear color (rgb)
 *   float  fog_start         (4 bytes) — linear fog start distance
 *   float  fog_end           (4 bytes) — linear fog end distance
 *   float  fog_density       (4 bytes) — exp/exp2 fog density
 *   uint   fog_mode          (4 bytes) — 0=linear, 1=exp, 2=exp2
 *   Total: 128 bytes
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridFragUniforms : register(b0, space3)
{
    float4 line_color;     /* grid line color (linear space)            */
    float4 bg_color;       /* background color between lines            */
    float4 light_dir;      /* world-space light direction (toward light)*/
    float4 eye_pos;        /* world-space camera position               */
    float  grid_spacing;   /* world-space distance between grid lines   */
    float  line_width;     /* grid line thickness in world units        */
    float  fade_distance;  /* distance at which grid fades out          */
    float  ambient;        /* ambient light intensity [0..1]            */
    float  shininess;      /* specular exponent (e.g. 32, 64, 128)     */
    float  specular_str;   /* specular intensity [0..1]                 */
    float  _pad0;
    float  _pad1;
    float4 fog_color;     /* fog color — should match the clear color  */
    float  fog_start;     /* linear fog: distance where fog begins     */
    float  fog_end;       /* linear fog: distance where fully fogged   */
    float  fog_density;   /* exponential modes: fog density coefficient */
    uint   fog_mode;      /* 0 = linear, 1 = exponential, 2 = exp2    */
};

struct PSInput
{
    float4 clip_pos  : SV_Position;
    float3 world_pos : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    /* Compute grid coordinates from world XZ position */
    float2 coord = input.world_pos.xz / grid_spacing;

    /* Screen-space rate of change: how many grid cells one pixel spans.
     * This is the key to both anti-aliasing and moire prevention. */
    float2 fw = fwidth(coord);

    /* Distance to the nearest grid line in each axis. */
    float2 grid_dist = abs(frac(coord - 0.5) - 0.5) / fw;
    float nearest = min(grid_dist.x, grid_dist.y);

    /* Convert distance-to-line into an alpha: 0 = on line, 1 = off line. */
    float alpha = 1.0 - saturate(nearest);

    /* Frequency-based fade (prevents moire at low angles). */
    float max_fw = max(fw.x, fw.y);
    alpha *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    /* Distance fade */
    float cam_dist = length(input.world_pos.xz - eye_pos.xz);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    alpha *= fade;

    /* Unlit surface color */
    float4 surface = lerp(bg_color, line_color, alpha);

    /* ── Blinn-Phong lighting ─────────────────────────────────────
     * The grid is a flat XZ plane, so the normal is always (0,1,0). */
    float3 N = float3(0.0, 1.0, 0.0);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* Ambient */
    float3 ambient_term = ambient * surface.rgb;

    /* Diffuse (Lambert) */
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse_term = NdotL * surface.rgb;

    /* Specular (Blinn) */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term_val = specular_str * pow(NdotH, shininess)
                         * float3(1.0, 1.0, 1.0);

    float3 lit_color = ambient_term + diffuse_term + specular_term_val;

    /* ── Distance fog ──────────────────────────────────────────────
     * Same fog calculation as the scene shader — both must match for
     * a consistent atmospheric effect across the entire scene. */
    float dist = length(eye_pos.xyz - input.world_pos);

    float fog_factor = 1.0;

    if (fog_mode == 0)
    {
        /* Linear */
        fog_factor = saturate((fog_end - dist) / (fog_end - fog_start));
    }
    else if (fog_mode == 1)
    {
        /* Exponential */
        fog_factor = saturate(exp(-fog_density * dist));
    }
    else
    {
        /* Exponential squared */
        float f = fog_density * dist;
        fog_factor = saturate(exp(-(f * f)));
    }

    float3 final_color = lerp(fog_color.rgb, lit_color, fog_factor);

    return float4(final_color, surface.a);
}
