/*
 * sky.frag.hlsl -- Physically-based atmospheric scattering (Hillaire)
 *
 * Implements Sebastien Hillaire's atmospheric model (EGSR 2020) with
 * LUT-accelerated transmittance and multi-scattering.  Each pixel casts
 * a ray from the camera through Earth's atmosphere and accumulates
 * inscattered sunlight via ray marching.
 *
 * Two precomputed LUTs (generated once at startup by compute shaders)
 * replace the per-pixel inner march and add multi-scattering:
 *   - Transmittance LUT: O(1) lookup replaces the inner ray march that
 *     previously ran at every sample point (32x8 = 256 evaluations/pixel)
 *   - Multi-scattering LUT: adds light from 2nd+ order bounces that
 *     fill in shadows and brighten the horizon
 *
 * Three scattering species:
 *   - Rayleigh: molecular scattering, wavelength-dependent (blue sky)
 *   - Mie: aerosol scattering, forward-peaked (sun halo, hazy horizon)
 *   - Ozone: absorption only, removes orange wavelengths at low sun
 *
 * All atmosphere constants are from Hillaire EGSR 2020 Table 1.
 * Units are in kilometers throughout.
 *
 * Key functions:
 *   1. ray_sphere_intersect         -- quadratic ray-sphere intersection
 *   2. rayleigh_phase               -- (3/16pi)(1 + cos^2 theta)
 *   3. mie_phase_hg                 -- Henyey-Greenstein with g=0.8
 *   4. sample_medium                -- density and extinction at altitude
 *   5. sample_transmittance         -- LUT lookup (replaces inner march)
 *   6. sample_multiscatter          -- LUT lookup for multi-scattering
 *   7. atmosphere                   -- outer ray march with LUT sampling
 *   8. sun_disc                     -- solar disc with limb darkening
 *
 * Uniform layout (48 bytes):
 *   float3 cam_pos_km       (12 bytes) -- camera position in km
 *   float  sun_intensity    ( 4 bytes) -- sun radiance multiplier
 *   float3 sun_dir          (12 bytes) -- normalized direction TO the sun
 *   int    num_steps        ( 4 bytes) -- outer ray march step count
 *   float2 resolution       ( 8 bytes) -- window size in pixels
 *   float2 _pad             ( 8 bytes) -- padding
 *
 * SPDX-License-Identifier: Zlib
 */

/* ---- LUT texture bindings (space2) -------------------------------------- */

Texture2D<float4> transmittance_lut : register(t0, space2);
SamplerState      trans_sampler     : register(s0, space2);
Texture2D<float4> multiscatter_lut  : register(t1, space2);
SamplerState      ms_sampler        : register(s1, space2);

/* ---- Uniforms (space3) -------------------------------------------------- */

cbuffer SkyFragUniforms : register(b0, space3)
{
    float3 cam_pos_km;       /* camera position in km (planet-centric)       */
    float  sun_intensity;    /* sun radiance multiplier (default 20)         */
    float3 sun_dir;          /* normalized direction toward the sun          */
    int    num_steps;        /* outer ray march steps (default 32)           */
    float2 resolution;       /* window width, height in pixels               */
    float2 _pad;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float3 view_ray : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

/* ---- Atmosphere constants (Hillaire EGSR 2020, Table 1) ----------------- */

static const float R_GROUND = 6360.0;    /* Earth radius in km             */
static const float R_ATMO   = 6460.0;    /* atmosphere top radius in km    */

static const float3 RAYLEIGH_SCATTER = float3(5.802e-3, 13.558e-3, 33.1e-3);
static const float  RAYLEIGH_H       = 8.0;

static const float MIE_SCATTER = 3.996e-3;
static const float MIE_ABSORB  = 0.444e-3;
static const float MIE_H       = 1.2;
static const float MIE_G       = 0.8;

static const float3 OZONE_ABSORB = float3(0.650e-3, 1.881e-3, 0.085e-3);
static const float  OZONE_CENTER = 25.0;
static const float  OZONE_WIDTH  = 15.0;

/* Sun angular radius -- half the ~0.53 degree diameter (0.00465 radians). */
static const float SUN_ANGULAR_RADIUS = 0.00465;

/* Sun disc rendering parameters. */
static const float SUN_DISC_MULTIPLIER = 10.0;
static const float LIMB_DARKENING_U    = 0.6;
static const float SUN_EDGE_OUTER      = 1.2;
static const float SUN_EDGE_INNER      = 0.9;

/* PI constant. */
static const float PI = 3.14159265358979323846;

/* Horizon fade parameters for earth shadow smoothing.
 * Smooths the terminator (shadow boundary) over ~2.9 degrees around
 * each sample point's local horizon: saturate(cos_zenith * SCALE + BIAS). */
static const float HORIZON_FADE_SCALE = 10.0;
static const float HORIZON_FADE_BIAS  = 0.5;

/* Transmittance LUT dimensions (must match C-side constants). */
static const float TRANSMITTANCE_LUT_W = 256.0;
static const float TRANSMITTANCE_LUT_H = 64.0;

/* Multi-scattering LUT dimensions. */
static const float MULTISCATTER_LUT_W = 32.0;
static const float MULTISCATTER_LUT_H = 32.0;

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

/* ---- Phase functions ---------------------------------------------------- */

float rayleigh_phase(float cos_theta)
{
    return (3.0 / (16.0 * PI)) * (1.0 + cos_theta * cos_theta);
}

float mie_phase_hg(float cos_theta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 / (4.0 * PI)) * (1.0 - g2) / (denom * sqrt(denom));
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
    float rho_mie = exp(-altitude / MIE_H);
    float rho_ozone = max(0.0, 1.0 - abs(altitude - OZONE_CENTER) / OZONE_WIDTH);

    float3 rayleigh_s = RAYLEIGH_SCATTER * rho_rayleigh;
    float  mie_s      = MIE_SCATTER * rho_mie;

    m.scatter = rayleigh_s + float3(mie_s, mie_s, mie_s);

    float3 rayleigh_t = rayleigh_s;
    float  mie_t      = mie_s + MIE_ABSORB * rho_mie;
    float3 ozone_t    = OZONE_ABSORB * rho_ozone;

    m.extinction = rayleigh_t + float3(mie_t, mie_t, mie_t) + ozone_t;

    return m;
}

/* ---- Transmittance LUT sampling ----------------------------------------- *
 *
 * Forward Bruneton mapping: (view_height, cos_zenith) -> UV.
 * Mirrors the inverse mapping in transmittance_lut.comp.hlsl.
 *
 * H = sqrt(R_ATMO^2 - R_GROUND^2) -- max geometric horizon distance
 * rho = sqrt(r^2 - R_GROUND^2)    -- distance to tangent point
 * d_min = R_ATMO - r              -- shortest path (straight up)
 * d_max = rho + H                 -- longest path (toward horizon)
 * x_r = rho / H                   -- maps view_height to [0,1]
 * x_mu = (d - d_min) / (d_max - d_min) -- maps cos_zenith to [0,1]
 * ---------------------------------------------------------------------- */

float2 transmittance_params_to_uv(float view_height, float cos_zenith)
{
    float H = sqrt(R_ATMO * R_ATMO - R_GROUND * R_GROUND);
    float rho = sqrt(max(0.0, view_height * view_height - R_GROUND * R_GROUND));

    /* Compute the ray distance to the atmosphere boundary using the
     * quadratic formula directly.  This must NOT clip to the ground —
     * the UV parameterization always uses the atmosphere sphere distance
     * to match the inverse mapping in the compute shader.  Earth shadow
     * is handled separately in the atmosphere() loop. */
    float discriminant = view_height * view_height
        * (cos_zenith * cos_zenith - 1.0) + R_ATMO * R_ATMO;
    float d = max(0.0, -view_height * cos_zenith
        + sqrt(max(0.0, discriminant)));

    float d_min = R_ATMO - view_height;
    float d_max = rho + H;

    float x_mu = (d_max > d_min) ? (d - d_min) / (d_max - d_min) : 0.0;
    float x_r  = (H > 0.0) ? rho / H : 0.0;

    return float2(clamp(x_mu, 0.0, 1.0), clamp(x_r, 0.0, 1.0));
}

float3 sample_transmittance(float view_height, float cos_zenith)
{
    float2 uv = transmittance_params_to_uv(view_height, cos_zenith);
    return transmittance_lut.SampleLevel(trans_sampler, uv, 0).rgb;
}

/* ---- Multi-scattering LUT sampling -------------------------------------- *
 *
 * Linear mapping with sub-UV correction to prevent bilinear edge bleed.
 * ---------------------------------------------------------------------- */

float from_unit_to_sub_uvs(float u, float res)
{
    return (u + 0.5 / res) * (res / (res + 1.0));
}

float3 sample_multiscatter(float altitude, float cos_sun_zenith)
{
    float u = from_unit_to_sub_uvs(cos_sun_zenith * 0.5 + 0.5, MULTISCATTER_LUT_W);
    float v = from_unit_to_sub_uvs(
        saturate(altitude / (R_ATMO - R_GROUND)), MULTISCATTER_LUT_H);

    return multiscatter_lut.SampleLevel(ms_sampler, float2(u, v), 0).rgb;
}

/* ---- Main atmosphere ray march ------------------------------------------ *
 *
 * The outer loop: march along the view ray through the atmosphere,
 * accumulating inscattered sunlight at each sample point.
 *
 * At each step:
 *   1. Sample the medium properties (scattering + extinction)
 *   2. Look up sun transmittance from the precomputed LUT
 *   3. Look up multi-scattering from the precomputed LUT
 *   4. Compute phase-weighted inscattered radiance
 *   5. Accumulate using Beer-Lambert transmittance
 *
 * The transmittance LUT replaces the inner march (O(1) per sample),
 * and the multi-scattering LUT adds light from higher-order bounces.
 * ---------------------------------------------------------------------- */

float3 atmosphere(float3 ray_origin, float3 ray_dir, out float3 transmittance_out)
{
    /* Intersect with the atmosphere sphere. */
    float t_near, t_far;
    if (!ray_sphere_intersect(ray_origin, ray_dir, R_ATMO, t_near, t_far))
    {
        transmittance_out = float3(1.0, 1.0, 1.0);
        return float3(0.0, 0.0, 0.0);
    }

    if (t_near < 0.0)
        t_near = 0.0;

    /* Stop at the ground if the ray hits it. */
    float t_ground_near, t_ground_far;
    if (ray_sphere_intersect(ray_origin, ray_dir, R_GROUND, t_ground_near, t_ground_far))
    {
        if (t_ground_near > 0.0)
            t_far = min(t_far, t_ground_near);
    }

    float step_size = (t_far - t_near) / (float)num_steps;

    /* Scattering angle between view ray and sun direction. */
    float cos_theta = dot(ray_dir, sun_dir);

    /* Phase functions (constant along the ray). */
    float phase_r = rayleigh_phase(cos_theta);
    float phase_m = mie_phase_hg(cos_theta, MIE_G);

    /* Accumulate inscattered radiance and track view transmittance. */
    float3 inscatter = float3(0.0, 0.0, 0.0);
    float3 transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < num_steps; i++)
    {
        float t = t_near + (float(i) + 0.5) * step_size;
        float3 pos = ray_origin + ray_dir * t;

        /* Sample scattering and extinction at this altitude. */
        MediumSample med = sample_medium(pos);

        /* Sun transmittance from the precomputed LUT. */
        float sample_height = length(pos);
        float cos_sun_zenith = dot(normalize(pos), sun_dir);
        float3 sun_trans = sample_transmittance(sample_height, cos_sun_zenith);

        /* Earth shadow: check if the sun is blocked by the planet at this
         * sample point.  Without this, points in the planet's shadow would
         * still contribute inscattered sunlight, diluting the warm Rayleigh
         * sunset colors with incorrect blue light. */
        float t_shadow_near, t_shadow_far;
        bool sun_blocked = ray_sphere_intersect(
            pos, sun_dir, R_GROUND, t_shadow_near, t_shadow_far);
        float earth_shadow = (sun_blocked && t_shadow_near >= 0.0) ? 0.0 : 1.0;

        /* Smooth transition near the terminator.  Without this, the shadow
         * boundary would be a hard edge causing visible artifacts.
         * The fade covers ~2.9 degrees around the local horizon. */
        earth_shadow *= saturate(cos_sun_zenith * HORIZON_FADE_SCALE + HORIZON_FADE_BIAS);

        /* Separate Rayleigh and Mie for phase weighting. */
        float altitude = sample_height - R_GROUND;
        float rho_r = exp(-altitude / RAYLEIGH_H);
        float rho_m = exp(-altitude / MIE_H);

        float3 scatter_r = RAYLEIGH_SCATTER * rho_r * phase_r;
        float3 scatter_m = float3(MIE_SCATTER, MIE_SCATTER, MIE_SCATTER)
                         * rho_m * phase_m;

        /* Multi-scattering contribution from the LUT.
         * Uses total scattering coefficient (no phase weighting --
         * higher-order scattering is isotropic). */
        float3 ms = sample_multiscatter(altitude, cos_sun_zenith);

        /* Combined inscattered radiance:
         * Single scatter is multiplied by earth_shadow -- no direct
         * sunlight reaches points in the planet's shadow.
         * Multi-scatter is NOT shadowed -- it's already integrated
         * over the full sphere of directions in the LUT. */
        float3 S = (scatter_r + scatter_m) * sun_trans * earth_shadow * sun_intensity
                 + ms * med.scatter * sun_intensity;

        /* Beer-Lambert: transmittance decreases exponentially. */
        float3 step_extinction = exp(-med.extinction * step_size);

        /* Analytical integration for better accuracy at large steps. */
        float3 scatter_integral = (float3(1.0, 1.0, 1.0) - step_extinction)
                                  / max(med.extinction, float3(1e-6, 1e-6, 1e-6));
        inscatter += S * scatter_integral * transmittance;

        transmittance *= step_extinction;
    }

    transmittance_out = transmittance;
    return inscatter;
}

/* ---- Sun disc with limb darkening --------------------------------------- */

float3 sun_disc(float3 ray_dir, float3 sun_direction, float3 transmittance)
{
    float cos_angle = dot(ray_dir, sun_direction);
    float angle = acos(clamp(cos_angle, -1.0, 1.0));

    float edge = smoothstep(SUN_ANGULAR_RADIUS * SUN_EDGE_OUTER,
                            SUN_ANGULAR_RADIUS * SUN_EDGE_INNER, angle);

    if (edge <= 0.0)
        return float3(0.0, 0.0, 0.0);

    float r = angle / SUN_ANGULAR_RADIUS;
    float cos_limb = sqrt(max(0.0, 1.0 - r * r));
    float limb_darkening = 1.0 - LIMB_DARKENING_U * (1.0 - cos_limb);

    float3 sun_color = float3(1.0, 1.0, 1.0) * sun_intensity * SUN_DISC_MULTIPLIER
                     * limb_darkening * edge * transmittance;

    return sun_color;
}

/* ---- Fragment shader entry point ---------------------------------------- */

float4 main(PSInput input) : SV_Target
{
    /* The vertex shader outputs world-space ray directions via the ray
     * matrix (camera basis vectors scaled by FOV/aspect).  Normalize
     * to get the per-pixel view ray direction. */
    float3 ray_dir = normalize(input.view_ray);

    /* March through the atmosphere, accumulating inscattered light. */
    float3 transmittance;
    float3 color = atmosphere(cam_pos_km, ray_dir, transmittance);

    /* Add the sun disc only if the sun is not occluded by the planet.
     * Check if a ray from the camera toward the sun hits the ground
     * sphere -- if it does, the planet blocks the sun disc. */
    float t_gnd_near, t_gnd_far;
    bool sun_hits_ground = ray_sphere_intersect(
        cam_pos_km, sun_dir, R_GROUND, t_gnd_near, t_gnd_far);
    if (!sun_hits_ground || t_gnd_near < 0.0)
        color += sun_disc(ray_dir, sun_dir, transmittance);

    return float4(color, 1.0);
}
