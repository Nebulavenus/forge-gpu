/*
 * transmittance_lut.comp.hlsl -- Precomputed transmittance lookup table
 *
 * Computes optical transmittance (Beer-Lambert) from any point in the
 * atmosphere toward the zenith angle, stored in a 2D texture.  Each texel
 * represents a (view_height, cos_zenith) pair, and the stored value is
 * exp(-optical_depth) -- the fraction of light surviving the path.
 *
 * The transmittance depends only on altitude and zenith angle, NOT on
 * view direction.  Pre-computing it in a LUT replaces the inner march
 * loop that previously ran at every sample point of the outer ray march,
 * reducing per-pixel cost from O(outer*inner) to O(outer).
 *
 * UV parameterization uses the Bruneton non-linear mapping (Hillaire
 * 2020, adapted from Bruneton & Neyret 2008).  This concentrates
 * precision near the horizon where transmittance changes fastest.
 *
 * Register layout (compute shader spaces):
 *   u0, space1 -- RWTexture2D output (transmittance LUT)
 *
 * LUT dimensions are compile-time constants (256x64), so no
 * uniform buffer is needed.
 *
 * SPDX-License-Identifier: Zlib
 */

/* ---- Output ------------------------------------------------------------- */

RWTexture2D<float4> output_tex : register(u0, space1);

/* ---- Atmosphere constants (Hillaire EGSR 2020, Table 1) ----------------- */

static const float R_GROUND = 6360.0;
static const float R_ATMO   = 6460.0;

static const float3 RAYLEIGH_SCATTER = float3(5.802e-3, 13.558e-3, 33.1e-3);
static const float  RAYLEIGH_H       = 8.0;

static const float MIE_SCATTER = 3.996e-3;
static const float MIE_ABSORB  = 0.444e-3;
static const float MIE_H       = 1.2;

static const float3 OZONE_ABSORB = float3(0.650e-3, 1.881e-3, 0.085e-3);
static const float  OZONE_CENTER = 25.0;
static const float  OZONE_WIDTH  = 15.0;

static const float PI = 3.14159265358979323846;

/* LUT dimensions (must match C-side constants and sky.frag.hlsl). */
static const float LUT_WIDTH  = 256.0;
static const float LUT_HEIGHT = 64.0;

/* Number of integration steps along the transmittance path.
 * Generous since this runs once at startup, not per frame. */
static const int TRANSMITTANCE_STEPS = 40;

/* ---- Ray-sphere intersection -------------------------------------------- */

bool ray_sphere_intersect(float3 ro, float3 rd, float radius,
                          out float t_near, out float t_far)
{
    float a = dot(rd, rd);
    float b = 2.0 * dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    t_near = 0.0;
    t_far  = 0.0;

    if (discriminant < 0.0)
        return false;

    float sqrt_disc = sqrt(discriminant);
    float inv_2a = 1.0 / (2.0 * a);
    t_near = (-b - sqrt_disc) * inv_2a;
    t_far  = (-b + sqrt_disc) * inv_2a;

    return true;
}

/* ---- Medium sampling ---------------------------------------------------- */

float3 sample_extinction(float3 pos)
{
    float altitude = length(pos) - R_GROUND;

    float rho_rayleigh = exp(-altitude / RAYLEIGH_H);
    float rho_mie      = exp(-altitude / MIE_H);
    float rho_ozone    = max(0.0, 1.0 - abs(altitude - OZONE_CENTER) / OZONE_WIDTH);

    float3 rayleigh_t = RAYLEIGH_SCATTER * rho_rayleigh;
    float  mie_t      = (MIE_SCATTER + MIE_ABSORB) * rho_mie;
    float3 ozone_t    = OZONE_ABSORB * rho_ozone;

    return rayleigh_t + float3(mie_t, mie_t, mie_t) + ozone_t;
}

/* ---- Bruneton UV parameterization --------------------------------------- *
 *
 * Maps UV coordinates to (view_height, cos_zenith) using a non-linear
 * parameterization that concentrates resolution near the horizon, where
 * transmittance changes most rapidly.
 *
 * The mapping uses the distance 'd' that a ray travels through the
 * atmosphere as an intermediate variable, which naturally compresses
 * near the horizon (where d is longest).
 *
 * H = sqrt(R_ATMO^2 - R_GROUND^2) -- max geometric horizon distance
 * rho = sqrt(r^2 - R_GROUND^2)    -- distance to tangent point
 * d_min = R_ATMO - r              -- shortest path (straight up)
 * d_max = rho + H                 -- longest path (toward horizon)
 * ---------------------------------------------------------------------- */

void uv_to_transmittance_params(float2 uv,
                                out float view_height,
                                out float cos_zenith)
{
    float H = sqrt(R_ATMO * R_ATMO - R_GROUND * R_GROUND);

    /* uv.y -> view_height via rho (distance to ground tangent point). */
    float x_r = uv.y;
    float rho = H * x_r;
    view_height = sqrt(rho * rho + R_GROUND * R_GROUND);

    /* uv.x -> cos_zenith via ray distance d through the atmosphere. */
    float d_min = R_ATMO - view_height;
    float d_max = rho + H;
    float d = d_min + uv.x * (d_max - d_min);

    /* Reconstruct cos_zenith from the law of cosines:
     * d^2 = r^2 + R_ATMO^2 - 2*r*R_ATMO*cos(angle_to_atmo_top)
     * But we want the zenith angle, so:
     * cos_zenith = (H^2 - rho^2 - d^2) / (2 * view_height * d)  */
    float denom = 2.0 * view_height * d;
    if (denom < 1e-6)
    {
        cos_zenith = 1.0;
    }
    else
    {
        cos_zenith = (H * H - rho * rho - d * d) / denom;
        cos_zenith = clamp(cos_zenith, -1.0, 1.0);
    }
}

/* ---- Main --------------------------------------------------------------- */

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    /* Dispatch groups match LUT dimensions exactly (256/8, 64/8),
     * so no bounds check is needed.  We use the known constants
     * for UV computation to avoid any dependency on uniform push
     * timing. */
    float width  = LUT_WIDTH;
    float height = LUT_HEIGHT;

    /* Texel center UV with half-texel offset for correct sampling. */
    float2 uv = float2(
        ((float)id.x + 0.5) / width,
        ((float)id.y + 0.5) / height
    );

    /* Map UV to atmosphere parameters. */
    float view_height, cos_zenith;
    uv_to_transmittance_params(uv, view_height, cos_zenith);

    /* Ray origin at (0, view_height, 0), direction from cos_zenith. */
    float3 ro = float3(0.0, view_height, 0.0);
    float sin_zenith = sqrt(max(0.0, 1.0 - cos_zenith * cos_zenith));
    float3 rd = float3(sin_zenith, cos_zenith, 0.0);

    /* Find the ray's exit distance from the atmosphere. */
    float t_near, t_far;
    ray_sphere_intersect(ro, rd, R_ATMO, t_near, t_far);

    /* If we start inside, march from 0 to t_far. */
    float march_start = max(t_near, 0.0);
    float march_dist  = t_far - march_start;

    /* Check if the ray hits the ground -- stop there if so. */
    float t_gnd_near, t_gnd_far;
    if (ray_sphere_intersect(ro, rd, R_GROUND, t_gnd_near, t_gnd_far))
    {
        if (t_gnd_near > 0.0)
            march_dist = min(march_dist, t_gnd_near - march_start);
    }

    float step_size = march_dist / (float)TRANSMITTANCE_STEPS;

    /* Integrate optical depth along the path. */
    float3 optical_depth = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < TRANSMITTANCE_STEPS; i++)
    {
        float t = march_start + ((float)i + 0.5) * step_size;
        float3 pos = ro + rd * t;
        optical_depth += sample_extinction(pos) * step_size;
    }

    /* Store transmittance = exp(-optical_depth). */
    float3 transmittance = exp(-optical_depth);
    output_tex[id.xy] = float4(transmittance, 1.0);
}
