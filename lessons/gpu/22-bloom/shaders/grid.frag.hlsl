/*
 * grid.frag.hlsl — Anti-aliased procedural grid with point light and HDR
 *
 * Combines Lesson 12's procedural grid with point-light illumination.
 * Simplified from Lesson 21 by removing cascaded shadow maps.
 *
 * The grid algorithm (fwidth anti-aliasing, distance fade) is unchanged.
 * The lighting is Blinn-Phong with a hardcoded up-normal (the grid is flat).
 * The light_intensity * attenuation pushes lit areas past 1.0, creating
 * HDR values that the tone mapping pass will compress.
 *
 * Fragment samplers: none
 *
 * Uniform layout (80 bytes, 16-byte aligned):
 *   float4 line_color          (16 bytes)
 *   float4 bg_color            (16 bytes)
 *   float3 light_pos + float light_intensity (16 bytes)
 *   float3 eye_pos + float grid_spacing      (16 bytes)
 *   float  line_width           (4 bytes)
 *   float  fade_distance        (4 bytes)
 *   float  ambient              (4 bytes)
 *   float  shininess            (4 bytes)
 *   float  specular_str         (4 bytes)
 *   float3 _pad                (12 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridFragUniforms : register(b0, space3)
{
    float4 line_color;         /* grid line color                            */
    float4 bg_color;           /* background surface color                   */
    float3 light_pos;          /* world-space point light position           */
    float  light_intensity;    /* light brightness multiplier (>1 = HDR)     */
    float3 eye_pos;            /* world-space camera position                */
    float  grid_spacing;       /* world units between lines (e.g. 1.0)      */
    float  line_width;         /* line thickness in grid-space [0..0.5]      */
    float  fade_distance;      /* distance where grid fades to background    */
    float  ambient;            /* ambient light intensity [0..1]             */
    float  shininess;          /* specular exponent (e.g. 32, 64)           */
    float  specular_str;       /* specular intensity [0..1]                  */
    float3 _pad;
};

struct PSInput
{
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    /* ── Procedural grid pattern ─────────────────────────────────────── */

    /* Scale to grid space so integer values fall on grid lines. */
    float2 grid_uv = input.world_pos.xz / grid_spacing;

    /* Distance to nearest grid line in each axis. */
    float2 dist_to_line = abs(frac(grid_uv - 0.5) - 0.5);

    /* Screen-space derivative for anti-aliasing (pixel footprint size). */
    float2 fw = fwidth(grid_uv);

    /* Anti-aliased line mask — smoothstep gives a soft edge. */
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist_to_line);

    /* Combine X and Z lines — show a line if either axis is on a line. */
    float grid = max(aa_line.x, aa_line.y);

    /* Frequency-based fade (prevents moire when fwidth is too large). */
    float max_fw = max(fw.x, fw.y);
    grid *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    /* Distance fade — dissolve the grid at the far field. */
    float cam_dist = length(input.world_pos - eye_pos);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    /* Mix line and background colors. */
    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);

    /* ── Blinn-Phong with point light + HDR ──────────────────────────── */

    float3 N = float3(0.0, 1.0, 0.0);
    float3 L_vec = light_pos - input.world_pos;
    float  light_dist = length(L_vec);
    float3 L = L_vec / light_dist;
    float3 V = normalize(eye_pos - input.world_pos);

    /* Point light attenuation */
    float attenuation = 1.0 / (1.0 + 0.09 * light_dist + 0.032 * light_dist * light_dist);

    /* Diffuse (Lambert) */
    float NdotL = max(dot(N, L), 0.0);

    /* Specular (Blinn half-vector) */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess)
                         * float3(1.0, 1.0, 1.0);

    /* Ambient is unaffected by attenuation; diffuse and specular are.
     * light_intensity * attenuation pushes lit areas into HDR range. */
    float3 lit = ambient * surface
               + (NdotL * surface + specular_term) * attenuation * light_intensity;

    return float4(lit, 1.0);
}
