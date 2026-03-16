/*
 * Math Lesson 16 — Density Functions
 *
 * Demonstrates the concept of density: measuring how much of something
 * exists per unit of something else.  Covers intensive vs extensive
 * quantities, histograms vs density histograms, probability density
 * functions, and numerical integration of densities.
 *
 * This is a console program that prints examples step by step.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Constants ─────────────────────────────────────────────────────────────── */

#define NUM_REGIONS     5       /* Number of regions in the map example        */
#define HIST_BINS       8       /* Number of histogram bins                    */
#define PDF_SAMPLES     12      /* Samples for PDF evaluation                  */
#define INTEGRATION_N   1000    /* Subdivisions for numerical integration      */

/* ── 1. Extensive vs Intensive — Regions on a Map ─────────────────────────── */

/*
 * Extensive quantities depend on system size: population, area, energy.
 * Intensive quantities are independent of size: density, temperature, pressure.
 *
 * Density = extensive / extent  (e.g. people / km^2)
 */

typedef struct {
    const char *name;
    float population;   /* extensive: total people  */
    float area_km2;     /* extent: area in km^2     */
} Region;

static const Region regions[NUM_REGIONS] = {
    { "Village",    120.0f,   2.0f },
    { "Town",      4500.0f,  15.0f },
    { "Suburb",    8000.0f,  40.0f },
    { "City",     95000.0f,  50.0f },
    { "Metropolis", 2500000.0f, 800.0f },
};

static void demo_extensive_vs_intensive(void)
{
    SDL_Log("==========================================================");
    SDL_Log("  1. Extensive vs Intensive Quantities");
    SDL_Log("==========================================================");
    SDL_Log(" ");
    SDL_Log("  Extensive: depends on system size (population, mass, energy)");
    SDL_Log("  Intensive: independent of size  (density, temperature, pressure)");
    SDL_Log("  Density = extensive quantity / extent (unit of measurement)");
    SDL_Log(" ");
    SDL_Log("  %-12s  %10s  %8s  %12s", "Region", "Population", "Area km2",
            "Density /km2");
    SDL_Log("  %-12s  %10s  %8s  %12s", "------", "----------", "--------",
            "------------");

    for (int i = 0; i < NUM_REGIONS; i++) {
        float density = regions[i].population / regions[i].area_km2;
        SDL_Log("  %-12s  %10.0f  %8.1f  %12.1f",
                regions[i].name, regions[i].population,
                regions[i].area_km2, density);
    }

    SDL_Log(" ");
    SDL_Log("  The village has the fewest people (extensive) but the");
    SDL_Log("  city may not have the highest density (intensive).");
    SDL_Log("  Density lets us compare regions of different sizes fairly.");
    SDL_Log(" ");
}

/* ── 2. Histogram: Counts vs Density ──────────────────────────────────────── */

/*
 * A count histogram depends on bin width — wider bins collect more samples.
 * A density histogram normalizes by bin width: height = count / (N * width).
 * The area under a density histogram always sums to 1.
 */

static void demo_histogram(void)
{
    SDL_Log("==========================================================");
    SDL_Log("  2. Count Histogram vs Density Histogram");
    SDL_Log("==========================================================");
    SDL_Log(" ");

    /* Generate some sample data: 40 values in [0, 8) */
    float data[] = {
        0.2f, 0.8f, 1.1f, 1.3f, 1.5f, 1.9f, 2.0f, 2.2f,
        2.4f, 2.5f, 2.7f, 2.8f, 3.0f, 3.1f, 3.2f, 3.3f,
        3.4f, 3.5f, 3.6f, 3.7f, 3.8f, 3.9f, 4.0f, 4.1f,
        4.3f, 4.5f, 4.8f, 5.0f, 5.2f, 5.5f, 5.8f, 6.0f,
        6.1f, 6.4f, 6.8f, 7.0f, 7.2f, 7.5f, 7.7f, 7.9f,
    };
    int n = (int)(sizeof(data) / sizeof(data[0]));
    float bin_width = 1.0f;  /* Each bin covers 1 unit */

    /* Count samples in each bin */
    int counts[HIST_BINS] = {0};
    for (int i = 0; i < n; i++) {
        int bin = (int)data[i];
        if (bin >= 0 && bin < HIST_BINS) counts[bin]++;
    }

    /* Print count histogram */
    SDL_Log("  Data: %d samples in [0, 8), bin width = %.1f", n, bin_width);
    SDL_Log(" ");
    SDL_Log("  Count histogram (depends on N and bin width):");
    SDL_Log("  Bin       Count   Bar");
    for (int i = 0; i < HIST_BINS; i++) {
        char bar[32] = {0};
        int len = counts[i];
        if (len > 30) len = 30;
        for (int j = 0; j < len; j++) bar[j] = '#';
        SDL_Log("  [%d, %d)   %5d   %s", i, i + 1, counts[i], bar);
    }

    /* Print density histogram */
    SDL_Log(" ");
    SDL_Log("  Density histogram (height = count / (N * width)):");
    SDL_Log("  Bin       Density   Bar");
    float area_sum = 0.0f;
    for (int i = 0; i < HIST_BINS; i++) {
        float density = (float)counts[i] / ((float)n * bin_width);
        area_sum += density * bin_width;
        char bar[32] = {0};
        int len = (int)(density * 60.0f);  /* scale for display */
        if (len > 30) len = 30;
        for (int j = 0; j < len; j++) bar[j] = '#';
        SDL_Log("  [%d, %d)   %7.4f   %s", i, i + 1, density, bar);
    }
    SDL_Log(" ");
    SDL_Log("  Total area under density histogram: %.4f (should be 1.0)", area_sum);
    SDL_Log("  -> Density normalization keeps the area at 1.0 for any N or bin width.");
    SDL_Log(" ");
}

/* ── 3. Ratio: X per Unit Y ──────────────────────────────────────────────── */

static void demo_ratio(void)
{
    SDL_Log("==========================================================");
    SDL_Log("  3. Density as a Ratio: X per Unit Y");
    SDL_Log("==========================================================");
    SDL_Log(" ");
    SDL_Log("  Density is always a ratio: 'how much X per unit of Y'");
    SDL_Log(" ");
    SDL_Log("  %-26s  %-14s  %-10s  %-10s", "Quantity", "Extensive (X)",
            "Extent (Y)", "Density");
    SDL_Log("  %-26s  %-14s  %-10s  %-10s", "---------", "-------------",
            "----------", "--------");

    /* Mass density */
    float mass = 7.874f;      /* kg */
    float volume = 0.001f;    /* m^3 (1 liter) */
    SDL_Log("  %-26s  %-14s  %-10s  %.0f kg/m^3",
            "Mass density (iron)", "7.874 kg", "0.001 m^3", mass / volume);

    /* Energy density (irradiance) */
    float energy = 500.0f;    /* watts */
    float area = 2.0f;        /* m^2 */
    SDL_Log("  %-26s  %-14s  %-10s  %.0f W/m^2",
            "Irradiance (light on srf)", "500 W", "2.0 m^2", energy / area);

    /* Texel density */
    float texels = 1024.0f * 1024.0f;
    float surface_area = 4.0f;  /* m^2 in world space */
    SDL_Log("  %-26s  %-14s  %-10s  %.0f texels/m^2",
            "Texel density", "1048576 tx", "4.0 m^2", texels / surface_area);

    /* Frame rate as density */
    float frames = 120.0f;
    float seconds = 2.0f;
    SDL_Log("  %-26s  %-14s  %-10s  %.0f fps",
            "Frame rate", "120 frames", "2.0 s", frames / seconds);

    /* Sample density */
    float samples = 64.0f;
    float pixel_count = 1.0f;
    SDL_Log("  %-26s  %-14s  %-10s  %.0f spp",
            "Sample density (MSAA/RT)", "64 samples", "1 pixel", samples / pixel_count);

    SDL_Log(" ");
    SDL_Log("  Every density is extensive / extent.");
    SDL_Log("  Changing the extent changes the extensive amount, not the density.");
    SDL_Log(" ");
}

/* ── 4. Probability Density Functions ─────────────────────────────────────── */

/*
 * A PDF f(x) describes probability per unit x.
 * P(a <= X <= b) = integral from a to b of f(x) dx.
 * The total integral over all x equals 1.
 */

/* Gaussian (normal) PDF: f(x) = (1 / (sigma * sqrt(2*pi))) * exp(-0.5 * ((x-mu)/sigma)^2) */
static float gaussian_pdf(float x, float mu, float sigma)
{
    float z = (x - mu) / sigma;
    return (1.0f / (sigma * SDL_sqrtf(2.0f * FORGE_PI))) * SDL_expf(-0.5f * z * z);
}

/* Uniform PDF on [a, b]: f(x) = 1/(b-a) inside, 0 outside */
static float uniform_pdf(float x, float a, float b)
{
    return (x >= a && x <= b) ? 1.0f / (b - a) : 0.0f;
}

static void demo_pdf(void)
{
    SDL_Log("==========================================================");
    SDL_Log("  4. Probability Density Functions (PDFs)");
    SDL_Log("==========================================================");
    SDL_Log(" ");
    SDL_Log("  A PDF f(x) gives probability *per unit x*, not probability itself.");
    SDL_Log("  P(a <= X <= b) = integral of f(x) dx from a to b.");
    SDL_Log("  The total area under any PDF equals 1.");
    SDL_Log(" ");

    /* Show Gaussian PDF values */
    float mu = 4.0f, sigma = 1.5f;
    SDL_Log("  Gaussian PDF (mu=%.1f, sigma=%.1f):", mu, sigma);
    SDL_Log("  x       f(x)      Bar");
    for (int i = 0; i <= PDF_SAMPLES; i++) {
        float x = (float)i;
        float fx = gaussian_pdf(x, mu, sigma);
        char bar[52] = {0};
        int len = (int)(fx * 150.0f);  /* scale for display */
        if (len > 50) len = 50;
        for (int j = 0; j < len; j++) bar[j] = '#';
        SDL_Log("  %-6.1f  %.5f   %s", x, fx, bar);
    }

    SDL_Log(" ");

    /* Show uniform PDF values */
    float a = 2.0f, b = 6.0f;
    SDL_Log("  Uniform PDF on [%.0f, %.0f]: f(x) = %.4f inside, 0 outside",
            a, b, 1.0f / (b - a));
    SDL_Log("  x       f(x)");
    for (int i = 0; i <= PDF_SAMPLES; i++) {
        float x = (float)i;
        SDL_Log("  %-6.1f  %.4f", x, uniform_pdf(x, a, b));
    }

    SDL_Log(" ");
    SDL_Log("  Key insight: f(x) can be > 1.  Only the integral must equal 1.");
    SDL_Log("  A narrow uniform on [0, 0.5] has f(x) = 2.0 inside the interval.");
    SDL_Log(" ");
}

/* ── 5. Integrating Density Functions ─────────────────────────────────────── */

/*
 * Integrate a density function to recover the extensive quantity.
 * Numerical integration via the trapezoidal rule:
 *   integral ~= sum of (f(x_i) + f(x_{i+1})) / 2 * dx
 */

static float integrate_gaussian(float lo, float hi, float mu, float sigma, int steps)
{
    float dx = (hi - lo) / (float)steps;
    float sum = 0.0f;
    for (int i = 0; i < steps; i++) {
        float x0 = lo + (float)i * dx;
        float x1 = x0 + dx;
        sum += (gaussian_pdf(x0, mu, sigma) + gaussian_pdf(x1, mu, sigma)) * 0.5f * dx;
    }
    return sum;
}

static void demo_integration(void)
{
    SDL_Log("==========================================================");
    SDL_Log("  5. Integrating Density Functions");
    SDL_Log("==========================================================");
    SDL_Log(" ");
    SDL_Log("  Integration recovers the total (extensive) from density (intensive).");
    SDL_Log("  Integral of density over a region = total quantity in that region.");
    SDL_Log(" ");

    float mu = 4.0f, sigma = 1.5f;

    /* Full integral should be ~1.0 */
    float full = integrate_gaussian(-10.0f, 18.0f, mu, sigma, INTEGRATION_N);
    SDL_Log("  Gaussian (mu=%.1f, sigma=%.1f):", mu, sigma);
    SDL_Log("  Integral over [-10, 18]: %.6f  (should be ~1.0)", full);
    SDL_Log(" ");

    /* Partial integrals — probability in different ranges */
    SDL_Log("  Partial integrals (probability that X falls in range):");

    float ranges[][2] = {
        { 2.5f, 5.5f },   /* mu +/- 1 sigma */
        { 1.0f, 7.0f },   /* mu +/- 2 sigma */
        { 0.0f, 4.0f },   /* left half */
        { 4.0f, 8.0f },   /* right half */
    };
    const char *labels[] = {
        "mu +/- 1 sigma [2.5, 5.5]",
        "mu +/- 2 sigma [1.0, 7.0]",
        "Left half      [0.0, 4.0]",
        "Right half     [4.0, 8.0]",
    };
    int nranges = (int)(sizeof(ranges) / sizeof(ranges[0]));

    for (int i = 0; i < nranges; i++) {
        float p = integrate_gaussian(ranges[i][0], ranges[i][1], mu, sigma,
                                     INTEGRATION_N);
        SDL_Log("    %-30s  P = %.4f  (%.1f%%)", labels[i], p, p * 100.0f);
    }

    SDL_Log(" ");
    SDL_Log("  The 68-95-99.7 rule: ~68%% within 1 sigma, ~95%% within 2 sigma.");
    SDL_Log(" ");

    /* Show that doubling region size doubles extensive, not density */
    SDL_Log("  Doubling the integration region:");
    float p_narrow = integrate_gaussian(3.0f, 5.0f, mu, sigma, INTEGRATION_N);
    float p_wide   = integrate_gaussian(2.0f, 6.0f, mu, sigma, INTEGRATION_N);
    SDL_Log("    [3, 5] (width 2): P = %.4f", p_narrow);
    SDL_Log("    [2, 6] (width 4): P = %.4f", p_wide);
    SDL_Log("    Wider region captures more probability — the density did not change,");
    SDL_Log("    we just measured more of it.");
    SDL_Log(" ");
}

/* ── 6. Graphics and Game Dev Applications ────────────────────────────────── */

static void demo_graphics_applications(void)
{
    SDL_Log("==========================================================");
    SDL_Log("  6. Density Functions in Graphics and Game Dev");
    SDL_Log("==========================================================");
    SDL_Log(" ");
    SDL_Log("  Radiometry (lighting):");
    SDL_Log("    Radiant flux (W)       -- total power (extensive)");
    SDL_Log("    Irradiance (W/m^2)     -- power per unit area (density)");
    SDL_Log("    Radiance (W/m^2/sr)    -- power per area per solid angle (double density)");
    SDL_Log("    To compute total light on a surface: integrate irradiance over area.");
    SDL_Log(" ");
    SDL_Log("  Monte Carlo rendering:");
    SDL_Log("    Importance sampling uses a PDF to concentrate samples where light");
    SDL_Log("    contribution is highest.  The estimator divides by the PDF value");
    SDL_Log("    to remove the sampling bias: result = f(x) / pdf(x).");
    SDL_Log(" ");

    /* Quick importance sampling example */
    SDL_Log("  Example — uniform vs importance sampling of cos(theta):");

    /* Uniform sampling: average of cos(theta) over [0, pi/2] */
    float uniform_sum = 0.0f;
    int ns = 8;
    for (int i = 0; i < ns; i++) {
        float theta = ((float)i + 0.5f) / (float)ns * (FORGE_PI * 0.5f);
        float pdf = 1.0f / (FORGE_PI * 0.5f);  /* uniform on [0, pi/2] */
        uniform_sum += SDL_cosf(theta) / pdf;
    }
    float uniform_est = uniform_sum / (float)ns;

    /* Cosine-weighted: pdf(theta) = cos(theta) on [0, pi/2] (integrates to 1) */
    float cosine_sum = 0.0f;
    for (int i = 0; i < ns; i++) {
        /* Inverse CDF: CDF = sin(theta), so theta = arcsin(u) */
        float u = ((float)i + 0.5f) / (float)ns;
        float theta = SDL_asinf(u);
        float pdf = SDL_cosf(theta);
        cosine_sum += SDL_cosf(theta) / pdf;
    }
    float cosine_est = cosine_sum / (float)ns;

    float exact = 1.0f;  /* integral of cos(theta) d(theta) from 0 to pi/2 */
    SDL_Log("    Exact integral of cos(theta) over [0, pi/2] = %.4f", exact);
    SDL_Log("    Uniform sampling   (%d samples): %.4f  (error: %.4f)",
            ns, uniform_est, SDL_fabsf(uniform_est - exact));
    SDL_Log("    Cosine-weighted    (%d samples): %.4f  (error: %.4f)",
            ns, cosine_est, SDL_fabsf(cosine_est - exact));
    SDL_Log("    -> Importance sampling reduces error by matching the PDF to the integrand.");

    SDL_Log(" ");
    SDL_Log("  Other graphics densities:");
    SDL_Log("    Texel density    (texels/m^2)    -- texture resolution on surfaces");
    SDL_Log("    Vertex density   (vertices/m^2)  -- mesh tessellation level");
    SDL_Log("    Particle density (particles/m^3) -- fog, smoke, fluid simulation");
    SDL_Log("    Sample density   (samples/pixel) -- MSAA, ray tracing quality");
    SDL_Log(" ");
    SDL_Log("  Game design densities:");
    SDL_Log("    NPC density      (NPCs/km^2)     -- open world population");
    SDL_Log("    Loot density     (items/room)     -- reward pacing");
    SDL_Log("    Event density    (events/minute)  -- gameplay pacing");
    SDL_Log(" ");
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log(" ");
    SDL_Log("  Math Lesson 16 -- Density Functions");
    SDL_Log("  ====================================");
    SDL_Log(" ");

    demo_extensive_vs_intensive();
    demo_histogram();
    demo_ratio();
    demo_pdf();
    demo_integration();
    demo_graphics_applications();

    SDL_Log("  ====================================");
    SDL_Log("  End of Lesson 16 -- Density Functions");
    SDL_Log(" ");

    SDL_Quit();
    return 0;
}
