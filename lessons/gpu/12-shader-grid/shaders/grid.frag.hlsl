/*
 * grid.frag.hlsl — Anti-aliased procedural grid with distance fade
 *
 * Core concept of Lesson 12: generate a floor grid entirely in the shader,
 * with no texture.  The technique uses screen-space derivatives (fwidth)
 * and smoothstep to produce crisp, anti-aliased lines at any camera
 * distance.  A distance fade prevents moire patterns at the horizon.
 *
 * How the grid algorithm works:
 *
 *   1. Scale world position to grid space:  grid_uv = world_pos.xz / spacing
 *   2. Compute distance to the nearest grid line in each axis:
 *        dist = abs(frac(grid_uv - 0.5) - 0.5)
 *      This maps every point to [0, 0.5], where 0 = on a grid line.
 *   3. Compute the screen-space rate of change:  fw = fwidth(grid_uv)
 *      fwidth = abs(ddx) + abs(ddy) — how many grid cells per pixel.
 *      This is the key to anti-aliasing: it tells us the pixel's footprint.
 *      (Building on Lesson 05's discussion of ddx/ddy for mip selection —
 *      the same derivative hardware, but used here for procedural AA.)
 *   4. smoothstep produces a soft edge between "on a line" and "off":
 *        aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist)
 *      Pixels near a line boundary get a partial value instead of a hard
 *      step — that's the anti-aliasing.
 *   5. Combine X and Z lines:  grid = max(aa_line.x, aa_line.y)
 *   6. Distance fade: smoothstep fades the grid to background beyond a
 *      threshold, preventing moire at the far field where grid cells
 *      become smaller than pixels.
 *   7. Simplified Blinn-Phong: the grid surface normal is always (0,1,0)
 *      so diffuse and specular are cheap constant-direction calculations.
 *
 * Uniform layout (96 bytes, 16-byte aligned):
 *   float4 line_color     (16 bytes) — grid line color (RGBA)
 *   float4 bg_color       (16 bytes) — background/surface color (RGBA)
 *   float4 light_dir      (16 bytes) — world-space light direction (xyz)
 *   float4 eye_pos        (16 bytes) — world-space camera position (xyz)
 *   float  grid_spacing    (4 bytes) — world units between grid lines
 *   float  line_width      (4 bytes) — line thickness in grid-space units
 *   float  fade_distance   (4 bytes) — distance at which grid fades out
 *   float  ambient         (4 bytes) — ambient light intensity [0..1]
 *   float  shininess       (4 bytes) — specular exponent
 *   float  specular_str    (4 bytes) — specular intensity [0..1]
 *   float  _pad0           (4 bytes) — padding to 16-byte alignment
 *   float  _pad1           (4 bytes) — padding to 16-byte alignment
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridFragUniforms : register(b0, space3)
{
    float4 line_color;     /* grid line color                            */
    float4 bg_color;       /* background surface color                   */
    float4 light_dir;      /* world-space light direction (toward light) */
    float4 eye_pos;        /* world-space camera position                */
    float  grid_spacing;   /* world units between lines (e.g. 1.0)      */
    float  line_width;     /* line thickness in grid-space [0..0.5]      */
    float  fade_distance;  /* distance where grid fades to background    */
    float  ambient;        /* ambient light intensity [0..1]             */
    float  shininess;      /* specular exponent (e.g. 32, 64)           */
    float  specular_str;   /* specular intensity [0..1]                  */
    float  _pad0;
    float  _pad1;
};

struct PSInput
{
    float4 clip_pos  : SV_Position; /* not used directly, required by pipeline */
    float3 world_pos : TEXCOORD0;   /* interpolated world-space position       */
};

float4 main(PSInput input) : SV_Target
{
    /* ── Step 1: Scale to grid space ──────────────────────────────────── */
    /* Dividing world XZ by spacing maps coordinates so that integer
     * values fall exactly on grid lines. */
    float2 grid_uv = input.world_pos.xz / grid_spacing;

    /* ── Step 2: Distance to nearest grid line ────────────────────────── */
    /* frac(grid_uv - 0.5) - 0.5 maps each cell to [-0.5, 0.5],
     * centered on the grid line.  abs() gives distance from line center,
     * ranging from 0 (on the line) to 0.5 (midway between lines). */
    float2 dist = abs(frac(grid_uv - 0.5) - 0.5);

    /* ── Step 3: Screen-space derivative for anti-aliasing ────────────── */
    /* fwidth = abs(ddx(grid_uv)) + abs(ddy(grid_uv)) — the approximate
     * size of one pixel in grid-space units.  When pixels are large
     * relative to the grid (far away), fw is large and smoothstep
     * produces a wide fade; when pixels are small (close up), fw is
     * small and lines stay crisp.  This is the same derivative hardware
     * the GPU uses for mip level selection (Lesson 05). */
    float2 fw = fwidth(grid_uv);

    /* ── Step 4: Anti-aliased line mask ───────────────────────────────── */
    /* smoothstep creates a smooth 0-to-1 transition.  We reverse it
     * (1.0 - ...) so that points ON the line get 1.0 and points far
     * from the line get 0.0.  The transition width is one pixel (fw),
     * giving a smooth edge instead of hard aliased steps. */
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);

    /* ── Step 5: Combine X and Z lines ────────────────────────────────── */
    /* max() keeps the strongest line — if you're on either an X-aligned
     * or Z-aligned grid line, you see a line. */
    float grid = max(aa_line.x, aa_line.y);

    /* ── Step 6: Distance fade ────────────────────────────────────────── */
    /* At large distances, grid cells become sub-pixel and create moire
     * artifacts.  Fade the grid to the background color beyond
     * fade_distance.  The fade starts at half the distance to give a
     * gradual transition. */
    float cam_dist = length(input.world_pos - eye_pos.xyz);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    /* ── Mix line and background colors ───────────────────────────────── */
    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);

    /* ── Step 7: Simplified Blinn-Phong ───────────────────────────────── */
    /* The grid normal is always straight up: N = (0, 1, 0).
     * This makes the dot products trivial — no need to transform
     * normals or normalize interpolated values. */
    float3 N = float3(0.0, 1.0, 0.0);
    float3 L = normalize(light_dir.xyz);
    float3 V = normalize(eye_pos.xyz - input.world_pos);

    /* Diffuse (Lambert) — dot(N, L) simplifies to L.y */
    float NdotL = max(dot(N, L), 0.0);

    /* Specular (Blinn half-vector) */
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 specular_term = specular_str * pow(NdotH, shininess) * float3(1.0, 1.0, 1.0);

    /* Combine: ambient + diffuse + specular */
    float3 lit = ambient * surface + NdotL * surface + specular_term;

    return float4(lit, 1.0);
}
