/*
 * multiscatter_lut.comp.hlsl -- Precomputed multi-scattering lookup table
 *
 * Computes the contribution of 2nd and higher order scattering bounces
 * at each (altitude, sun_zenith_cos) pair.  Single scattering (the main
 * ray march) only captures light that scatters once toward the camera.
 * In reality, scattered light bounces many times between molecules,
 * filling in shadows and brightening the horizon.
 *
 * Method (from Hillaire 2020, adapted from Pixel Storm):
 *   For each texel, integrate over 64 uniformly distributed directions
 *   on the unit sphere.  For each direction, march through the atmosphere
 *   and accumulate:
 *     - L_2nd: luminance from 2nd-order scattering (single scatter along
 *       each direction, weighted by isotropic phase 1/4pi)
 *     - f_ms: fraction of light that rescatters (total scattering / total
 *       extinction ratio, integrated over the sphere)
 *
 *   The infinite geometric series 1 + f_ms + f_ms^2 + ... = 1/(1-f_ms)
 *   sums all higher-order bounces.  Final result: L_2nd / (1 - f_ms).
 *
 * Higher-order scattering is nearly isotropic (direction-independent),
 * so we use an isotropic phase function (1/4pi) for all directions.
 * This is why the LUT only needs 2 parameters (altitude + sun zenith),
 * not the full 3D (altitude + sun zenith + view zenith).
 *
 * Register layout (compute shader spaces):
 *   t0, space0 -- transmittance LUT (sampled texture, read-only)
 *   s0, space0 -- sampler for transmittance LUT
 *   u0, space1 -- RWTexture2D output (multi-scattering LUT)
 *
 * LUT dimensions are compile-time constants (32x32), so no
 * uniform buffer is needed.
 *
 * SPDX-License-Identifier: Zlib
 */

/* ---- Inputs and outputs ------------------------------------------------- */

Texture2D<float4>   transmittance_lut : register(t0, space0);
SamplerState        trans_sampler     : register(s0, space0);

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
static const float LUT_WIDTH  = 32.0;
static const float LUT_HEIGHT = 32.0;

/* Direction sampling grid: 8x8 = 64 stratified directions on the sphere. */
static const int SQRT_SAMPLE_COUNT = 8;

/* Integration steps per direction.  Generous for one-time computation. */
static const int MULTISCATTER_STEPS = 20;

/* Shared atmosphere tuning — single definition for all shaders. */
#include "atmosphere_params.hlsli"

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

struct MediumSample
{
    float3 scatter;
    float3 extinction;
};

MediumSample sample_medium(float3 pos)
{
    MediumSample m;
    float altitude = length(pos) - R_GROUND;

    float rho_rayleigh = exp(-altitude / RAYLEIGH_H);
    float rho_mie      = exp(-altitude / MIE_H);
    float rho_ozone    = max(0.0, 1.0 - abs(altitude - OZONE_CENTER) / OZONE_WIDTH);

    float3 rayleigh_s = RAYLEIGH_SCATTER * rho_rayleigh;
    float  mie_s      = MIE_SCATTER * rho_mie;

    m.scatter = rayleigh_s + float3(mie_s, mie_s, mie_s);

    float3 rayleigh_t = rayleigh_s;
    float  mie_t      = mie_s + MIE_ABSORB * rho_mie;
    float3 ozone_t    = OZONE_ABSORB * rho_ozone;

    m.extinction = rayleigh_t + float3(mie_t, mie_t, mie_t) + ozone_t;

    return m;
}

/* ---- Sub-UV helpers ----------------------------------------------------- *
 *
 * When sampling a LUT with bilinear filtering, the edge texels are
 * averaged with the border (clamp) color.  Sub-UV correction maps
 * the [0,1] parameter range to the sub-range [0.5/N, 1-0.5/N] of
 * texel centers, preventing edge bleed.
 * ---------------------------------------------------------------------- */

float from_unit_to_sub_uvs(float u, float res)
{
    return (u + 0.5 / res) * (res / (res + 1.0));
}

float from_sub_uvs_to_unit(float u, float res)
{
    return (u - 0.5 / res) * (res / (res - 1.0));
}

/* ---- Transmittance LUT sampling ----------------------------------------- *
 *
 * Forward Bruneton mapping: (view_height, cos_zenith) -> UV.
 * This is the inverse of the mapping in transmittance_lut.comp.hlsl.
 * ---------------------------------------------------------------------- */

float3 sample_transmittance_lut(float view_height, float cos_zenith)
{
    float H = sqrt(R_ATMO * R_ATMO - R_GROUND * R_GROUND);
    float rho = sqrt(max(0.0, view_height * view_height - R_GROUND * R_GROUND));

    float d_min = R_ATMO - view_height;
    float d_max = rho + H;

    /* Compute d (distance to atmosphere boundary) using the quadratic
     * formula directly.  Must NOT clip to ground — the UV parameterization
     * always uses the atmosphere sphere distance to match the inverse
     * mapping in the transmittance compute shader. */
    float discriminant = view_height * view_height
        * (cos_zenith * cos_zenith - 1.0) + R_ATMO * R_ATMO;
    float d = max(0.0, -view_height * cos_zenith
        + sqrt(max(0.0, discriminant)));

    float x_mu = (d_max > d_min) ? (d - d_min) / (d_max - d_min) : 0.0;
    float x_r = (H > 0.0) ? rho / H : 0.0;

    float2 uv = float2(clamp(x_mu, 0.0, 1.0), clamp(x_r, 0.0, 1.0));

    return transmittance_lut.SampleLevel(trans_sampler, uv, 0).rgb;
}

/* ---- Main --------------------------------------------------------------- */

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    /* Dispatch groups match LUT dimensions exactly (32/8, 32/8),
     * so no bounds check is needed.  We use known constants for
     * UV computation to avoid dependency on uniform push timing. */
    float width  = LUT_WIDTH;
    float height = LUT_HEIGHT;

    /* UV with sub-UV correction to prevent bilinear edge bleed. */
    float2 uv = float2(
        ((float)id.x + 0.5) / width,
        ((float)id.y + 0.5) / height
    );

    /* Linear mapping: UV -> (cos_sun_zenith, altitude). */
    float cos_sun_zenith = from_sub_uvs_to_unit(uv.x, width) * 2.0 - 1.0;
    float altitude = from_sub_uvs_to_unit(uv.y, height)
                   * (R_ATMO - R_GROUND);

    /* Clamp to at least 10 m above ground to avoid the boundary case where
     * view_height == R_GROUND.  On the exact ground surface, downward rays
     * have t_gnd_near == 0 and march through the earth interior, where
     * density = exp(-negative/H) = huge, producing inf * 0 = NaN. */
    float view_height = R_GROUND + clamp(altitude, 0.01, R_ATMO - R_GROUND);

    /* Sun direction at this zenith angle. */
    float sin_sun = sqrt(max(0.0, 1.0 - cos_sun_zenith * cos_sun_zenith));
    float3 sun_dir = float3(sin_sun, cos_sun_zenith, 0.0);

    /* Position at the north pole of the planet. */
    float3 pos = float3(0.0, view_height, 0.0);

    /* Accumulate over 64 uniformly distributed directions. */
    float3 L_2nd = float3(0.0, 0.0, 0.0);  /* 2nd-order scattered luminance */
    float3 f_ms  = float3(0.0, 0.0, 0.0);  /* rescattering fraction         */

    float inv_sample_count = 1.0 / (float)(SQRT_SAMPLE_COUNT * SQRT_SAMPLE_COUNT);

    for (int i = 0; i < SQRT_SAMPLE_COUNT; i++)
    {
        for (int j = 0; j < SQRT_SAMPLE_COUNT; j++)
        {
            /* Stratified direction on the unit sphere. */
            float theta = 2.0 * PI * ((float)i + 0.5) / (float)SQRT_SAMPLE_COUNT;
            float phi = acos(1.0 - 2.0 * ((float)j + 0.5) / (float)SQRT_SAMPLE_COUNT);

            float sin_phi = sin(phi);
            float3 ray_dir = float3(
                sin_phi * cos(theta),
                cos(phi),
                sin_phi * sin(theta)
            );

            /* Find march distance along this direction. */
            float t_near, t_far;
            ray_sphere_intersect(pos, ray_dir, R_ATMO, t_near, t_far);

            float march_start = max(t_near, 0.0);
            float march_dist  = t_far - march_start;

            /* Check ground intersection. */
            float t_gnd_near, t_gnd_far;
            bool hits_ground = ray_sphere_intersect(pos, ray_dir, R_GROUND,
                                                    t_gnd_near, t_gnd_far);
            if (hits_ground && t_gnd_near >= 0.0)
                march_dist = min(march_dist, t_gnd_near - march_start);

            float step_size = march_dist / (float)MULTISCATTER_STEPS;

            /* Per-direction accumulators. */
            float3 dir_L  = float3(0.0, 0.0, 0.0);  /* scattered luminance   */
            float3 dir_f  = float3(0.0, 0.0, 0.0);  /* rescattering fraction  */
            float3 throughput = float3(1.0, 1.0, 1.0);

            for (int k = 0; k < MULTISCATTER_STEPS; k++)
            {
                float t = march_start + ((float)k + 0.5) * step_size;
                float3 sample_pos = pos + ray_dir * t;

                MediumSample med = sample_medium(sample_pos);
                float3 step_ext = exp(-med.extinction * step_size);

                /* Sun transmittance at this sample point (from the LUT). */
                float sample_height = length(sample_pos);
                float cos_sun = dot(normalize(sample_pos), sun_dir);
                float3 sun_trans = sample_transmittance_lut(sample_height, cos_sun);

                /* Earth shadow: check if the planet blocks sunlight at this
                 * sample point.  Without this, the LUT stores non-zero values
                 * for below-horizon sun angles, causing warm colors to persist
                 * after the sun sets.  Matches Pixel Storm's
                 * integrate_scattered_luminance (inl/ps_sky_lut.cu). */
                float t_shadow_near, t_shadow_far;
                bool sun_blocked = ray_sphere_intersect(
                    sample_pos, sun_dir, R_GROUND,
                    t_shadow_near, t_shadow_far);
                float earth_shadow = (sun_blocked && t_shadow_near >= 0.0)
                    ? 0.0 : 1.0;

                /* Smooth transition near the terminator to avoid a hard
                 * shadow edge.  horizon_fade goes from 0 to 1 over ~2.9°
                 * around the local horizon. */
                earth_shadow *= saturate(cos_sun * HORIZON_FADE_SCALE + HORIZON_FADE_BIAS);

                /* Analytical integration of scattering over the step. */
                float3 scatter_integral = (float3(1.0, 1.0, 1.0) - step_ext)
                    / max(med.extinction, float3(1e-6, 1e-6, 1e-6));

                /* L_2nd contribution: scattered light * sun transmittance *
                 * earth shadow.  Uses isotropic phase since higher-order
                 * scattering is direction-independent. */
                dir_L += throughput * med.scatter * sun_trans * earth_shadow
                    * scatter_integral;

                /* f_ms contribution: fraction of scattered light that could
                 * scatter again (total scattering, no sun transmittance). */
                dir_f += throughput * med.scatter * scatter_integral;

                throughput *= step_ext;
            }

            /* Solid angle weight for uniform sphere sampling:
             * Each direction subtends 4*pi / N_samples steradians.
             * Multiplied by the isotropic phase 1/(4*pi), the factors
             * cancel, leaving a weight of 1/N per direction. */
            L_2nd += dir_L * inv_sample_count;
            f_ms  += dir_f * inv_sample_count;
        }
    }

    /* Power series: sum infinite bounces via geometric series.
     * Psi = L_2nd / (1 - f_ms)
     * Clamp f_ms to prevent division by zero or divergence. */
    float3 psi = L_2nd / (1.0 - clamp(f_ms, float3(0.0, 0.0, 0.0),
                                              float3(0.99, 0.99, 0.99)));

    output_tex[id.xy] = float4(psi, 1.0);
}
