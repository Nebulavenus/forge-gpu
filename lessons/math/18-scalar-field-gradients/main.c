/*
 * Math Lesson 18 — Scalar Field Gradients
 *
 * Demonstrates: partial derivatives (analytic vs numerical), the gradient
 * vector, gradient perpendicularity to isolines, height map normals, gradient
 * descent optimization, and the Laplacian as a net-curvature indicator.
 *
 * This is a console program that prints examples step by step.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Constants ─────────────────────────────────────────────────────────────── */

#define EPS             1e-4f   /* Epsilon for gradient (central differences)  */
#define FD_COMPARE_EPS  1e-3f   /* Larger eps for the FD comparison — 1e-4 is
                                 * too small for clean forward/backward results
                                 * at single precision                          */
#define LAPLACIAN_EPS   5e-3f   /* Epsilon for Laplacian — larger because the
                                 * 1/eps^2 divisor amplifies float noise       */
#define GRID_SIZE       11      /* Grid resolution for ASCII contour plot      */
#define GRID_RANGE      3.0f    /* Plot spans [-GRID_RANGE, +GRID_RANGE]      */
#define HM_SIZE         5       /* Height map grid dimensions                  */
#define HM_SPACING      1.0f   /* Height map cell spacing (world units)       */
#define HM_AMPLITUDE    4.0f   /* Gaussian bump peak height                   */
#define HM_SPREAD       3.0f   /* Gaussian spread (denominator in exponent)   */
#define GD_START_X      10.0f  /* Gradient descent starting x                 */
#define GD_START_Y      10.0f  /* Gradient descent starting y                 */
#define GD_STEP         0.1f   /* Gradient descent step size (learning rate)  */
#define GD_ITERATIONS   30     /* Number of gradient descent steps            */

/* Arrow angle sector boundaries for 8-direction ASCII gradient display.
 * Each sector spans PI/4 radians; boundaries at odd multiples of PI/8. */
#define ARROW_7PI_8     (7.0f * FORGE_PI / 8.0f)   /* ~2.749  */
#define ARROW_5PI_8     (5.0f * FORGE_PI / 8.0f)   /* ~1.963  */
#define ARROW_3PI_8     (3.0f * FORGE_PI / 8.0f)   /* ~1.178  */
#define ARROW_PI_8      (FORGE_PI / 8.0f)           /* ~0.393  */

/* Contour level thresholds for ASCII display */
#define CONTOUR_LO      4.0f   /* Below: '.'  */
#define CONTOUR_MID     8.0f   /* Below: 'o'  */
#define CONTOUR_HI      16.0f  /* Below: '+'  Above: '#' */

/* Skip gradient arrows near the origin where direction is numerically noisy */
#define ARROW_SKIP_DIST 0.1f
#define DIR_EPS         1e-3f  /* Threshold for classifying gradient direction  */

/* ── Helpers ───────────────────────────────────────────────────────────────── */

static void print_header(int section, const char *title)
{
    SDL_Log("==========================================================");
    SDL_Log("  %d. %s", section, title);
    SDL_Log("==========================================================");
    SDL_Log(" ");
}

/* ── Scalar field functions ────────────────────────────────────────────────── */

/* f(x,y) = x^2 + y^2  (paraboloid — bowl shape) */
static float field_paraboloid(float x, float y, void *ctx)
{
    (void)ctx;
    return x * x + y * y;
}

/* f(x,y) = sin(x) * cos(y)  (wavy surface) */
static float field_sincos(float x, float y, void *ctx)
{
    (void)ctx;
    return SDL_sinf(x) * SDL_cosf(y);
}

/* f(x,y) = (x - 2)^2 + (y + 1)^2  (bowl centered at (2, -1)) */
static float field_bowl(float x, float y, void *ctx)
{
    (void)ctx;
    return (x - 2.0f) * (x - 2.0f) + (y + 1.0f) * (y + 1.0f);
}

/* f(x,y) = -(x^2 + y^2)  (inverted paraboloid — maximum at origin) */
static float field_inverted(float x, float y, void *ctx)
{
    (void)ctx;
    return -(x * x + y * y);
}

/* f(x,y) = x^2 - y^2  (saddle surface) */
static float field_saddle(float x, float y, void *ctx)
{
    (void)ctx;
    return x * x - y * y;
}

/* ── 1. Partial Derivatives ────────────────────────────────────────────────── */

static void demo_partial_derivatives(void)
{
    print_header(1, "Partial Derivatives");

    SDL_Log("  A partial derivative measures how a function changes when we");
    SDL_Log("  vary ONE variable while holding the others fixed.");
    SDL_Log(" ");
    SDL_Log("  For f(x,y) = x^2 + y^2:");
    SDL_Log("    df/dx = 2x   (hold y constant, differentiate w.r.t. x)");
    SDL_Log("    df/dy = 2y   (hold x constant, differentiate w.r.t. y)");
    SDL_Log(" ");

    /* Evaluation point */
    float px = 1.0f, py = 2.0f;
    float analytic_dfdx = 2.0f * px;   /* df/dx = 2x */
    float analytic_dfdy = 2.0f * py;   /* df/dy = 2y */

    SDL_Log("  At point (%.1f, %.1f):", px, py);
    SDL_Log("    Analytic:  df/dx = %.4f,  df/dy = %.4f",
            analytic_dfdx, analytic_dfdy);
    SDL_Log(" ");

    /* Numerical approximations — use a larger eps so the forward/backward
     * errors are clearly O(h) while central is O(h^2) in single precision. */
    SDL_Log("  Numerical finite differences (eps = %.0e):", FD_COMPARE_EPS);
    SDL_Log(" ");

    /* Forward difference:  f'(x) ~ (f(x+h) - f(x)) / h */
    float fwd_dfdx = (field_paraboloid(px + FD_COMPARE_EPS, py, NULL) -
                      field_paraboloid(px, py, NULL)) / FD_COMPARE_EPS;
    float fwd_dfdy = (field_paraboloid(px, py + FD_COMPARE_EPS, NULL) -
                      field_paraboloid(px, py, NULL)) / FD_COMPARE_EPS;
    SDL_Log("    Forward:   df/dx = %.6f,  df/dy = %.6f",
            fwd_dfdx, fwd_dfdy);
    SDL_Log("      Error:   |%.2e|,  |%.2e|",
            SDL_fabsf(fwd_dfdx - analytic_dfdx),
            SDL_fabsf(fwd_dfdy - analytic_dfdy));
    SDL_Log(" ");

    /* Backward difference:  f'(x) ~ (f(x) - f(x-h)) / h */
    float bwd_dfdx = (field_paraboloid(px, py, NULL) -
                      field_paraboloid(px - FD_COMPARE_EPS, py, NULL)) / FD_COMPARE_EPS;
    float bwd_dfdy = (field_paraboloid(px, py, NULL) -
                      field_paraboloid(px, py - FD_COMPARE_EPS, NULL)) / FD_COMPARE_EPS;
    SDL_Log("    Backward:  df/dx = %.6f,  df/dy = %.6f",
            bwd_dfdx, bwd_dfdy);
    SDL_Log("      Error:   |%.2e|,  |%.2e|",
            SDL_fabsf(bwd_dfdx - analytic_dfdx),
            SDL_fabsf(bwd_dfdy - analytic_dfdy));
    SDL_Log(" ");

    /* Central difference:  f'(x) ~ (f(x+h) - f(x-h)) / (2h) */
    float ctr_dfdx = (field_paraboloid(px + FD_COMPARE_EPS, py, NULL) -
                      field_paraboloid(px - FD_COMPARE_EPS, py, NULL)) / (2.0f * FD_COMPARE_EPS);
    float ctr_dfdy = (field_paraboloid(px, py + FD_COMPARE_EPS, NULL) -
                      field_paraboloid(px, py - FD_COMPARE_EPS, NULL)) / (2.0f * FD_COMPARE_EPS);
    SDL_Log("    Central:   df/dx = %.6f,  df/dy = %.6f",
            ctr_dfdx, ctr_dfdy);
    SDL_Log("      Error:   |%.2e|,  |%.2e|",
            SDL_fabsf(ctr_dfdx - analytic_dfdx),
            SDL_fabsf(ctr_dfdy - analytic_dfdy));
    SDL_Log(" ");
    SDL_Log("  Central differences cancel the first-order error term, giving");
    SDL_Log("  O(eps^2) accuracy compared to O(eps) for forward/backward.");
    SDL_Log(" ");
}

/* ── 2. The Gradient Vector ────────────────────────────────────────────────── */

static void demo_gradient_vector(void)
{
    print_header(2, "The Gradient Vector");

    SDL_Log("  The gradient assembles all partial derivatives into a vector:");
    SDL_Log(" ");
    SDL_Log("    grad f = (df/dx, df/dy)");
    SDL_Log(" ");
    SDL_Log("  Properties:");
    SDL_Log("    - Direction: points uphill (steepest ascent)");
    SDL_Log("    - Magnitude: rate of steepest change");
    SDL_Log("    - Perpendicular to isolines (contour lines)");
    SDL_Log(" ");

    SDL_Log("  Computing gradients of f(x,y) = sin(x) * cos(y):");
    SDL_Log("    Analytic:  grad f = (cos(x)*cos(y), -sin(x)*sin(y))");
    SDL_Log(" ");

    typedef struct { float x, y; } Point;
    Point samples[] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f},
        {FORGE_PI / 4.0f, FORGE_PI / 4.0f},
        {FORGE_PI / 2.0f, FORGE_PI / 2.0f}
    };
    int n = (int)(sizeof(samples) / sizeof(samples[0]));

    SDL_Log("  %-12s  %-10s  %-18s  %-10s  %-10s",
            "Point", "f(x,y)", "grad f", "|grad f|", "Direction");
    SDL_Log("  %-12s  %-10s  %-18s  %-10s  %-10s",
            "-----", "------", "------", "--------", "---------");

    for (int i = 0; i < n; i++) {
        float x = samples[i].x, y = samples[i].y;
        float val = field_sincos(x, y, NULL);
        vec2 grad = forge_field2d_gradient(field_sincos, x, y, EPS, NULL);
        float mag = vec2_length(grad);

        const char *dir;
        if (mag < DIR_EPS) dir = "flat";
        else if (SDL_fabsf(grad.y) < DIR_EPS) dir = grad.x > 0 ? "right" : "left";
        else if (SDL_fabsf(grad.x) < DIR_EPS) dir = grad.y > 0 ? "up" : "down";
        else if (grad.x > 0 && grad.y > 0) dir = "up-right";
        else if (grad.x > 0 && grad.y < 0) dir = "down-right";
        else if (grad.x < 0 && grad.y > 0) dir = "up-left";
        else dir = "down-left";

        SDL_Log("  (%.2f,%.2f)  %10.4f  (%7.4f, %7.4f)  %10.4f  %s",
                x, y, val, grad.x, grad.y, mag, dir);
    }
    SDL_Log(" ");
    SDL_Log("  The gradient at the origin is (1, 0) — the function increases");
    SDL_Log("  fastest in the +x direction (cos(0)*cos(0) = 1, sin term = 0).");
    SDL_Log(" ");
}

/* ── 3. Gradient Perpendicularity ──────────────────────────────────────────── */

static void demo_gradient_perpendicularity(void)
{
    print_header(3, "Gradient Perpendicularity to Isolines");

    SDL_Log("  The gradient is always perpendicular to isolines (level curves).");
    SDL_Log("  This is because moving along an isoline keeps f constant,");
    SDL_Log("  so the directional derivative along it is zero — the gradient");
    SDL_Log("  has no component in that direction.");
    SDL_Log(" ");
    SDL_Log("  ASCII contour plot of f(x,y) = x^2 + y^2 with gradient arrows:");
    SDL_Log("  Contour levels shown as characters: 0-4='.', 4-8='o', 8-16='+', >16='#'");
    SDL_Log("  Gradient arrows shown at sparse grid points.");
    SDL_Log(" ");

    /* We print a grid where each cell shows the contour character,
     * and at every other grid point we overlay a gradient direction arrow. */
    float step = (2.0f * GRID_RANGE) / (float)(GRID_SIZE - 1);

    /* Print column header */
    char header[256];
    int pos = 0;
    pos += SDL_snprintf(header + pos, sizeof(header) - (size_t)pos, "       ");
    for (int col = 0; col < GRID_SIZE; col++) {
        float x = -GRID_RANGE + (float)col * step;
        pos += SDL_snprintf(header + pos, sizeof(header) - (size_t)pos,
                            "%5.1f ", x);
    }
    SDL_Log("  %s", header);

    for (int row = 0; row < GRID_SIZE; row++) {
        float y = GRID_RANGE - (float)row * step;  /* top to bottom */
        char line[256];
        int lpos = 0;
        lpos += SDL_snprintf(line + lpos, sizeof(line) - (size_t)lpos,
                             "  %5.1f  ", y);

        for (int col = 0; col < GRID_SIZE; col++) {
            float x = -GRID_RANGE + (float)col * step;
            float val = field_paraboloid(x, y, NULL);

            /* At sparse points (every 3rd), show gradient arrow */
            if (row % 3 == 1 && col % 3 == 1 &&
                SDL_fabsf(x) > ARROW_SKIP_DIST && SDL_fabsf(y) > ARROW_SKIP_DIST) {
                vec2 grad = forge_field2d_gradient(field_paraboloid, x, y, EPS, NULL);
                float angle = SDL_atan2f(grad.y, grad.x);
                char arrow;
                /* Map angle to ASCII arrow — 8 directions */
                if      (angle >  ARROW_7PI_8 || angle < -ARROW_7PI_8) arrow = '<';
                else if (angle >  ARROW_5PI_8) arrow = '\\';
                else if (angle >  ARROW_3PI_8) arrow = '^';
                else if (angle >  ARROW_PI_8)  arrow = '/';
                else if (angle > -ARROW_PI_8)  arrow = '>';
                else if (angle > -ARROW_3PI_8) arrow = '\\';
                else if (angle > -ARROW_5PI_8) arrow = 'v';
                else                           arrow = '/';
                lpos += SDL_snprintf(line + lpos, sizeof(line) - (size_t)lpos,
                                     "  %c   ", arrow);
            } else {
                char ch;
                if      (val < CONTOUR_LO)  ch = '.';
                else if (val < CONTOUR_MID) ch = 'o';
                else if (val < CONTOUR_HI)  ch = '+';
                else                        ch = '#';
                lpos += SDL_snprintf(line + lpos, sizeof(line) - (size_t)lpos,
                                     "  %c   ", ch);
            }
        }
        SDL_Log("%s", line);
    }
    SDL_Log(" ");
    SDL_Log("  Arrows point radially outward (uphill) from the origin,");
    SDL_Log("  perpendicular to the circular contour lines.");
    SDL_Log(" ");
}

/* ── 4. Height Map Normals ─────────────────────────────────────────────────── */

static void demo_heightmap_normals(void)
{
    print_header(4, "Height Map Normals");

    SDL_Log("  A height map stores elevation at each grid point.  Surface normals");
    SDL_Log("  are computed from partial derivatives of the height:");
    SDL_Log(" ");
    SDL_Log("    n = normalize(-dh/dx, 1, -dh/dz)");
    SDL_Log(" ");
    SDL_Log("  This comes from the cross product of parametric tangent vectors:");
    SDL_Log("    T_x = (1, dh/dx, 0)");
    SDL_Log("    T_z = (0, dh/dz, 1)");
    SDL_Log("    n   = normalize(T_z x T_x)");
    SDL_Log(" ");

    /* Generate a small height map: a gentle hill */
    float heights[HM_SIZE * HM_SIZE];
    SDL_Log("  Height map (%dx%d, spacing = %.1f):", HM_SIZE, HM_SIZE, HM_SPACING);
    SDL_Log(" ");

    for (int z = 0; z < HM_SIZE; z++) {
        for (int x = 0; x < HM_SIZE; x++) {
            /* Height is a Gaussian bump centered at grid center */
            float cx = (float)x - (float)(HM_SIZE - 1) / 2.0f;
            float cz = (float)z - (float)(HM_SIZE - 1) / 2.0f;
            heights[z * HM_SIZE + x] = HM_AMPLITUDE * SDL_expf(-(cx * cx + cz * cz) / HM_SPREAD);
        }
    }

    /* Print height values */
    SDL_Log("  Heights:");
    for (int z = 0; z < HM_SIZE; z++) {
        char line[256];
        int lpos = 0;
        lpos += SDL_snprintf(line + lpos, sizeof(line) - (size_t)lpos, "    ");
        for (int x = 0; x < HM_SIZE; x++) {
            lpos += SDL_snprintf(line + lpos, sizeof(line) - (size_t)lpos,
                                 "%6.2f ", heights[z * HM_SIZE + x]);
        }
        SDL_Log("%s", line);
    }
    SDL_Log(" ");

    /* Compute and print normals */
    SDL_Log("  Surface normals:");
    SDL_Log("  %-10s  %-8s  %-28s", "Grid(x,z)", "Height", "Normal (nx, ny, nz)");
    SDL_Log("  %-10s  %-8s  %-28s", "---------", "------", "-------------------");

    for (int z = 0; z < HM_SIZE; z++) {
        for (int x = 0; x < HM_SIZE; x++) {
            vec3 n = forge_heightmap_normal(heights, x, z,
                                              HM_SIZE, HM_SIZE,
                                              HM_SPACING, HM_SPACING);
            float h = heights[z * HM_SIZE + x];
            SDL_Log("  (%d, %d)      %6.2f    (%7.4f, %7.4f, %7.4f)",
                    x, z, h, n.x, n.y, n.z);
        }
    }
    SDL_Log(" ");
    SDL_Log("  The center point (2,2) has the highest elevation and an almost");
    SDL_Log("  vertical normal (0, 1, 0).  Edge points tilt outward.");
    SDL_Log(" ");
}

/* ── 5. Gradient Descent ───────────────────────────────────────────────────── */

static void demo_gradient_descent(void)
{
    print_header(5, "Gradient Descent");

    SDL_Log("  Gradient descent minimizes a function by stepping opposite to");
    SDL_Log("  the gradient (downhill).  The algorithm:");
    SDL_Log(" ");
    SDL_Log("    x_new = x_old - step_size * grad f(x_old)");
    SDL_Log(" ");
    SDL_Log("  Minimizing f(x,y) = (x - 2)^2 + (y + 1)^2");
    SDL_Log("    Known minimum at (2, -1) where f = 0");
    SDL_Log("    Starting from (%.1f, %.1f), step size = %.2f",
            GD_START_X, GD_START_Y, GD_STEP);
    SDL_Log(" ");

    SDL_Log("  %-6s  %-12s  %-12s  %-12s  %-12s",
            "Step", "x", "y", "f(x,y)", "|grad f|");
    SDL_Log("  %-6s  %-12s  %-12s  %-12s  %-12s",
            "----", "----", "----", "------", "--------");

    float x = GD_START_X, y = GD_START_Y;
    for (int i = 0; i < GD_ITERATIONS; i++) {
        float val = field_bowl(x, y, NULL);
        vec2 grad = forge_field2d_gradient(field_bowl, x, y, EPS, NULL);
        float grad_mag = vec2_length(grad);

        /* Print first 10 steps, then every 5th step */
        if (i <= 10 || i % 5 == 0 || i == GD_ITERATIONS - 1) {
            SDL_Log("  %4d    %10.4f    %10.4f    %10.4f    %10.4f",
                    i, x, y, val, grad_mag);
        }

        /* Update position: move opposite to gradient */
        x -= GD_STEP * grad.x;
        y -= GD_STEP * grad.y;
    }
    SDL_Log(" ");
    SDL_Log("  The algorithm converges toward the minimum at (2, -1).");
    SDL_Log("  Smaller step sizes converge more slowly but more reliably.");
    SDL_Log("  Larger step sizes risk overshooting and diverging.");
    SDL_Log(" ");
}

/* ── 6. The Laplacian ──────────────────────────────────────────────────────── */

static void demo_laplacian(void)
{
    print_header(6, "The Laplacian");

    SDL_Log("  The Laplacian is the sum of second partial derivatives:");
    SDL_Log(" ");
    SDL_Log("    nabla^2 f = d^2f/dx^2 + d^2f/dy^2");
    SDL_Log(" ");
    SDL_Log("  It measures how much a point's value differs from the average");
    SDL_Log("  of its neighbors — a measure of net curvature:");
    SDL_Log(" ");
    SDL_Log("    > 0  net upward curvature (value < neighbor average)");
    SDL_Log("    < 0  net downward curvature (value > neighbor average)");
    SDL_Log("    = 0  balanced curvature (e.g. saddles, flat regions)");
    SDL_Log(" ");
    SDL_Log("  Note: the Laplacian is only the trace of the Hessian, so it");
    SDL_Log("  does not uniquely classify all critical points. For example,");
    SDL_Log("  f(x,y) = x^2 - 0.5*y^2 has a positive Laplacian at the origin");
    SDL_Log("  but is still a saddle. For full classification, inspect the");
    SDL_Log("  full Hessian matrix.");
    SDL_Log(" ");

    /* Test at the origin for three different fields */
    typedef struct {
        const char *name;
        const char *formula;
        forge_field2d_fn fn;
        const char *expected;
        float analytic;
    } FieldTest;

    FieldTest tests[] = {
        {"Bowl (minimum)",    "f = x^2 + y^2",
         field_paraboloid, "positive (> 0)", 4.0f},
        {"Cap (maximum)",     "f = -(x^2 + y^2)",
         field_inverted,   "negative (< 0)", -4.0f},
        {"Saddle",            "f = x^2 - y^2",
         field_saddle,     "zero (= 0)",      0.0f},
    };
    int n = (int)(sizeof(tests) / sizeof(tests[0]));

    SDL_Log("  %-20s  %-20s  %-12s  %-12s  %-16s",
            "Surface", "Formula", "Analytic", "Numerical", "Classification");
    SDL_Log("  %-20s  %-20s  %-12s  %-12s  %-16s",
            "-------", "-------", "--------", "---------", "--------------");

    for (int i = 0; i < n; i++) {
        float numerical = forge_field2d_laplacian(tests[i].fn, 0.0f, 0.0f,
                                                    LAPLACIAN_EPS, NULL);
        SDL_Log("  %-20s  %-20s  %10.4f    %10.4f    %s",
                tests[i].name, tests[i].formula,
                tests[i].analytic, numerical, tests[i].expected);
    }
    SDL_Log(" ");

    /* Show the Laplacian at various points for x^2 + y^2 */
    SDL_Log("  For f = x^2 + y^2, the Laplacian is constant everywhere:");
    SDL_Log("    d^2f/dx^2 = 2,  d^2f/dy^2 = 2  =>  nabla^2 f = 4");
    SDL_Log(" ");

    typedef struct { float x, y; } Pt;
    Pt pts[] = {{0,0}, {1,1}, {3,4}, {-2,5}};
    int np = (int)(sizeof(pts) / sizeof(pts[0]));

    for (int i = 0; i < np; i++) {
        float lap = forge_field2d_laplacian(field_paraboloid, pts[i].x, pts[i].y,
                                            LAPLACIAN_EPS, NULL);
        SDL_Log("    nabla^2 f(%.0f, %.0f) = %.4f", pts[i].x, pts[i].y, lap);
    }
    SDL_Log(" ");
    SDL_Log("  The Laplacian is used in physics (heat diffusion, wave equation),");
    SDL_Log("  image processing (Laplacian of Gaussian edge detection), and");
    SDL_Log("  terrain analysis (curvature classification).");
    SDL_Log(" ");
}

/* ── Main ──────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log(" ");
    SDL_Log("  =====================================");
    SDL_Log("   Math Lesson 18 -- Scalar Field");
    SDL_Log("   Gradients");
    SDL_Log("  =====================================");
    SDL_Log(" ");
    SDL_Log("  A scalar field assigns a number to every point in space.");
    SDL_Log("  The gradient is a vector that tells you the direction and");
    SDL_Log("  rate of steepest increase at each point.  This lesson");
    SDL_Log("  covers partial derivatives, the gradient vector, height");
    SDL_Log("  map normals, gradient descent, and the Laplacian.");
    SDL_Log(" ");

    demo_partial_derivatives();
    demo_gradient_vector();
    demo_gradient_perpendicularity();
    demo_heightmap_normals();
    demo_gradient_descent();
    demo_laplacian();

    SDL_Log("==========================================================");
    SDL_Log("  Summary");
    SDL_Log("==========================================================");
    SDL_Log(" ");
    SDL_Log("  Scalar field gradients are a core tool in graphics and");
    SDL_Log("  simulation:");
    SDL_Log(" ");
    SDL_Log("  1. Partial derivatives:    measure change along one axis");
    SDL_Log("  2. The gradient vector:    direction of steepest ascent");
    SDL_Log("  3. Perpendicularity:       gradient is normal to isolines");
    SDL_Log("  4. Height map normals:     n = normalize(-dh/dx, 1, -dh/dz)");
    SDL_Log("  5. Gradient descent:       x_new = x - step * grad f");
    SDL_Log("  6. The Laplacian:          measures net curvature");
    SDL_Log(" ");
    SDL_Log("  See forge_math.h for: forge_field2d_gradient, forge_field2d_laplacian,");
    SDL_Log("  forge_heightmap_normal");
    SDL_Log(" ");

    SDL_Quit();
    return 0;
}
