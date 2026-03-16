/*
 * Math Lesson 17 — Implicit 2D Curves
 *
 * Demonstrates: implicit curve representation f(x,y) = 0, signed distance
 * functions (SDFs), boolean CSG operations, smooth blending, gradient
 * computation, marching squares boundary extraction, and isolines.
 *
 * This is a console program that prints examples step by step.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Constants ─────────────────────────────────────────────────────────────── */

#define GRID_SIZE       11      /* Grid resolution for SDF visualization       */
#define GRID_RANGE      2.0f    /* Grid spans [-GRID_RANGE, +GRID_RANGE]       */
#define MARCH_GRID      8       /* Grid resolution for marching squares         */
#define MARCH_RANGE     1.5f    /* Marching squares grid extent                 */
#define GRADIENT_EPS    1e-4f   /* Epsilon for numerical gradient — balances
                                 * truncation error (wants small eps) against
                                 * floating-point cancellation (wants large eps).
                                 * For single-precision, ~sqrt(machine_eps) is
                                 * a good starting point: sqrt(1.19e-7) ~ 3.4e-4. */

/* ── Helpers ───────────────────────────────────────────────────────────────── */

static void print_header(int section, const char *title)
{
    SDL_Log("==========================================================");
    SDL_Log("  %d. %s", section, title);
    SDL_Log("==========================================================");
    SDL_Log(" ");
}

static const char *classify_str(float d)
{
    if (d < -0.01f) return "INSIDE";
    if (d >  0.01f) return "OUTSIDE";
    return "ON BOUNDARY";
}

/* Map SDF value to a character for ASCII visualization.
 * Negative = inside the shape, positive = outside. */
static char sdf_char(float d)
{
    if (d < -0.5f) return '#';    /* deep inside */
    if (d < -0.1f) return '+';    /* inside */
    if (d <  0.1f) return '0';    /* on boundary */
    if (d <  0.5f) return '.';    /* just outside */
    return ' ';                    /* far outside */
}

/* ── 1. Implicit vs Parametric ─────────────────────────────────────────────── */

static void demo_implicit_vs_parametric(void)
{
    print_header(1, "Implicit vs Parametric Representation");

    SDL_Log("  A unit circle can be defined two ways:");
    SDL_Log(" ");
    SDL_Log("  PARAMETRIC:  P(t) = (cos(t), sin(t))    t in [0, 2*pi]");
    SDL_Log("    -> Traces points ON the curve as t varies");
    SDL_Log("    -> Does not classify arbitrary points");
    SDL_Log(" ");
    SDL_Log("  IMPLICIT:    f(x,y) = x^2 + y^2 - 1 = 0");
    SDL_Log("    -> Classifies EVERY point in the plane:");
    SDL_Log("       f < 0  =>  inside");
    SDL_Log("       f = 0  =>  on the curve");
    SDL_Log("       f > 0  =>  outside");
    SDL_Log(" ");

    SDL_Log("  Evaluating f(x,y) = x^2 + y^2 - 1 at sample points:");
    SDL_Log(" ");

    typedef struct { float x, y; } Sample;
    Sample samples[] = {
        { 0.0f, 0.0f },         /* center */
        { 0.5f, 0.3f },         /* inside */
        { 1.0f, 0.0f },         /* on boundary */
        { 0.707f, 0.707f },     /* approximately on boundary */
        { 1.5f, 0.0f },         /* outside */
        { 2.0f, 1.0f },         /* far outside */
    };
    int n = (int)(sizeof(samples) / sizeof(samples[0]));

    SDL_Log("  %-14s %-10s %-10s %-14s",
            "Point", "f(x,y)", "SDF", "Classification");
    SDL_Log("  %-14s %-10s %-10s %-14s",
            "-----", "------", "---", "--------------");

    for (int i = 0; i < n; i++) {
        float x = samples[i].x, y = samples[i].y;
        float f_val = x * x + y * y - 1.0f;
        vec2 p = { x, y };
        float sdf = sdf2_circle(p, 1.0f);
        SDL_Log("  (%5.2f, %5.2f) %8.3f    %8.3f   %s",
                x, y, f_val, sdf, classify_str(sdf));
    }

    SDL_Log(" ");
    SDL_Log("  Note: f(x,y) = x^2+y^2-1 is NOT an SDF (|f| != distance).");
    SDL_Log("  The SDF sqrt(x^2+y^2)-1 gives the true Euclidean distance.");
    SDL_Log(" ");
}

/* ── 2. SDF Primitives ─────────────────────────────────────────────────────── */

static void demo_sdf_primitives(void)
{
    print_header(2, "Signed Distance Function Primitives");

    SDL_Log("  An SDF f(p) returns the shortest distance from p to the surface.");
    SDL_Log("  Negative = inside, zero = on surface, positive = outside.");
    SDL_Log("  Key property: |gradient(f)| = 1 everywhere (unit gradient).");
    SDL_Log(" ");

    float step = 2.0f * GRID_RANGE / (GRID_SIZE - 1);
    char row[GRID_SIZE + 1];
    row[GRID_SIZE] = '\0';

    /* Circle SDF */
    SDL_Log("  --- Circle SDF: sdf2_circle(p, radius=1.0) ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            row[i] = sdf_char(sdf2_circle(p, 1.0f));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log(" ");
    SDL_Log("  Legend: # = deep inside, + = inside, 0 = boundary, . = outside");
    SDL_Log(" ");

    /* Box SDF */
    vec2 box_half = { 1.0f, 0.6f };
    SDL_Log("  --- Box SDF: sdf2_box(p, half_extents={1.0, 0.6}) ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            row[i] = sdf_char(sdf2_box(p, box_half));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log(" ");

    /* Rounded box */
    SDL_Log("  --- Rounded Box: sdf2_rounded_box(p, {1.0, 0.6}, radius=0.3) ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            row[i] = sdf_char(sdf2_rounded_box(p, box_half, 0.3f));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log(" ");

    /* Line segment */
    vec2 seg_a = { -1.0f, -0.5f };
    vec2 seg_b = {  1.0f,  0.5f };
    SDL_Log("  --- Segment SDF: sdf2_segment(p, a={-1,-0.5}, b={1,0.5}) ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            row[i] = sdf_char(sdf2_segment(p, seg_a, seg_b));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log("  (Segment SDF is unsigned -- all values >= 0)");
    SDL_Log(" ");
}

/* ── 3. The Gradient ───────────────────────────────────────────────────────── */

static void demo_gradient(void)
{
    print_header(3, "Gradient of an SDF");

    SDL_Log("  The gradient of f points in the direction of fastest increase.");
    SDL_Log("  For an SDF, |gradient(f)| = 1 everywhere (the Eikonal equation).");
    SDL_Log("  At the surface, gradient(f) IS the outward surface normal.");
    SDL_Log(" ");

    SDL_Log("  Numerical gradient of sdf2_circle(p, 1.0) at sample points:");
    SDL_Log(" ");
    SDL_Log("  %-14s %-8s %-22s %-10s",
            "Point", "SDF", "Gradient", "|grad|");
    SDL_Log("  %-14s %-8s %-22s %-10s",
            "-----", "---", "--------", "------");

    vec2 test_pts[] = {
        { 1.0f, 0.0f },
        { 0.0f, 1.0f },
        { 0.707f, 0.707f },
        { 0.5f, 0.0f },
        { 2.0f, 0.0f },
        { -1.0f, -1.0f },
    };
    int n = (int)(sizeof(test_pts) / sizeof(test_pts[0]));

    for (int i = 0; i < n; i++) {
        vec2 p = test_pts[i];
        float d = sdf2_circle(p, 1.0f);

        /* Numerical gradient using central differences */
        float eps = GRADIENT_EPS;
        vec2 px_pos = { p.x + eps, p.y };
        vec2 px_neg = { p.x - eps, p.y };
        vec2 py_pos = { p.x, p.y + eps };
        vec2 py_neg = { p.x, p.y - eps };

        float gx = (sdf2_circle(px_pos, 1.0f) - sdf2_circle(px_neg, 1.0f))
                    / (2.0f * eps);
        float gy = (sdf2_circle(py_pos, 1.0f) - sdf2_circle(py_neg, 1.0f))
                    / (2.0f * eps);
        float grad_len = SDL_sqrtf(gx * gx + gy * gy);

        SDL_Log("  (%5.2f, %5.2f) %6.3f   (%7.4f, %7.4f)     %.4f",
                p.x, p.y, d, gx, gy, grad_len);
    }

    SDL_Log(" ");
    SDL_Log("  All gradient magnitudes are ~1.0, confirming the Eikonal property.");
    SDL_Log("  At (1,0): gradient = (1,0) -- the outward normal of the circle.");
    SDL_Log(" ");
    SDL_Log("  Note: the gradient is undefined at the origin (equidistant from all");
    SDL_Log("  surface points).  The Eikonal equation holds 'almost everywhere' --");
    SDL_Log("  singularities occur at points equidistant from multiple closest");
    SDL_Log("  surface points (the 'medial axis' of the shape).");
    SDL_Log(" ");
}

/* ── 4. CSG Boolean Operations ─────────────────────────────────────────────── */

static void demo_csg_operations(void)
{
    print_header(4, "CSG Boolean Operations");

    SDL_Log("  Implicit surfaces support boolean set operations:");
    SDL_Log(" ");
    SDL_Log("    Union:        min(fA, fB)       -- combine shapes");
    SDL_Log("    Intersection: max(fA, fB)       -- keep overlap only");
    SDL_Log("    Subtraction:  max(fA, -fB)      -- cut B from A");
    SDL_Log(" ");

    float step = 2.0f * GRID_RANGE / (GRID_SIZE - 1);
    char row[GRID_SIZE + 1];
    row[GRID_SIZE] = '\0';

    vec2 center_a = { -0.5f, 0.0f };
    vec2 center_b = {  0.5f, 0.0f };
    float radius = 1.0f;

    /* Union */
    SDL_Log("  --- Union of two circles (centers at -0.5 and +0.5) ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            float da = sdf2_circle(vec2_sub(p, center_a), radius);
            float db = sdf2_circle(vec2_sub(p, center_b), radius);
            row[i] = sdf_char(sdf_union(da, db));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log(" ");

    /* Intersection */
    SDL_Log("  --- Intersection of two circles ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            float da = sdf2_circle(vec2_sub(p, center_a), radius);
            float db = sdf2_circle(vec2_sub(p, center_b), radius);
            row[i] = sdf_char(sdf_intersection(da, db));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log(" ");

    /* Subtraction */
    SDL_Log("  --- Subtraction: left circle minus right circle ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            float da = sdf2_circle(vec2_sub(p, center_a), radius);
            float db = sdf2_circle(vec2_sub(p, center_b), radius);
            row[i] = sdf_char(sdf_subtraction(da, db));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log(" ");
}

/* ── 5. Smooth Blending ────────────────────────────────────────────────────── */

static void demo_smooth_blend(void)
{
    print_header(5, "Smooth Blending (Metaballs)");

    SDL_Log("  Hard union (min) creates sharp creases where shapes meet.");
    SDL_Log("  Smooth union blends the surfaces together, controlled by k:");
    SDL_Log(" ");
    SDL_Log("    smooth_union(a, b, k) -- organic, blobby shapes");
    SDL_Log("    k = 0 -> hard union (same as min)");
    SDL_Log("    k > 0 -> smoother blending; larger k = wider blend");
    SDL_Log(" ");

    float step = 2.0f * GRID_RANGE / (GRID_SIZE - 1);
    char row[GRID_SIZE + 1];
    row[GRID_SIZE] = '\0';

    vec2 center_a = { -0.6f, 0.0f };
    vec2 center_b = {  0.6f, 0.0f };
    float radius = 0.8f;

    float k_values[] = { 0.0f, 0.3f, 0.6f, 1.0f };
    int nk = (int)(sizeof(k_values) / sizeof(k_values[0]));

    for (int ki = 0; ki < nk; ki++) {
        float k = k_values[ki];
        SDL_Log("  --- Smooth union, k = %.1f %s ---",
                k, k < 0.01f ? "(hard union)" : "");
        SDL_Log(" ");
        for (int j = 0; j < GRID_SIZE; j++) {
            float y = GRID_RANGE - j * step;
            for (int i = 0; i < GRID_SIZE; i++) {
                float x = -GRID_RANGE + i * step;
                vec2 p = { x, y };
                float da = sdf2_circle(vec2_sub(p, center_a), radius);
                float db = sdf2_circle(vec2_sub(p, center_b), radius);
                float d = (k > 0.01f) ? sdf_smooth_union(da, db, k)
                                       : sdf_union(da, db);
                row[i] = sdf_char(d);
            }
            SDL_Log("  %s", row);
        }
        SDL_Log(" ");
    }

    /* Also show smooth intersection for comparison */
    SDL_Log("  --- Smooth intersection, k = 0.5 ---");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            float da = sdf2_circle(vec2_sub(p, center_a), radius);
            float db = sdf2_circle(vec2_sub(p, center_b), radius);
            row[i] = sdf_char(sdf_smooth_intersection(da, db, 0.5f));
        }
        SDL_Log("  %s", row);
    }
    SDL_Log("  (Smooth intersection rounds the sharp edges of the overlap region)");
    SDL_Log(" ");
}

/* ── 6. Marching Squares ───────────────────────────────────────────────────── */

/* The scalar field used for the marching squares demo.  Centralized here so
 * the grid evaluation and saddle disambiguation sample the same function. */
static float march_field(vec2 p) { return sdf2_circle(p, 1.0f); }

/* Marching squares case lookup: 4 corners form a 4-bit index.
 * Each case produces 0, 1, or 2 line segments connecting edge midpoints.
 * Edges: 0=bottom, 1=right, 2=top, 3=left
 *
 * Cases 5 and 10 are ambiguous saddle points.  The table stores one default
 * connectivity for each, but the cell-processing loop below evaluates f at
 * the cell center to choose the correct topology per cell. */
#define MS_NONE -1
static const int ms_edges[16][4] = {
    /* case  0: all outside */    { MS_NONE, MS_NONE, MS_NONE, MS_NONE },
    /* case  1: BL inside */      { 0, 3, MS_NONE, MS_NONE },
    /* case  2: BR inside */      { 1, 0, MS_NONE, MS_NONE },
    /* case  3: bottom inside */  { 1, 3, MS_NONE, MS_NONE },
    /* case  4: TR inside */      { 2, 1, MS_NONE, MS_NONE },
    /* case  5: BL+TR (saddle) */ { 0, 3, 2, 1 },
    /* case  6: right inside */   { 2, 0, MS_NONE, MS_NONE },
    /* case  7: all but TL */     { 2, 3, MS_NONE, MS_NONE },
    /* case  8: TL inside */      { 3, 2, MS_NONE, MS_NONE },
    /* case  9: left inside */    { 0, 2, MS_NONE, MS_NONE },
    /* case 10: TL+BR (saddle) */ { 3, 0, 1, 2 },
    /* case 11: all but TR */     { 1, 2, MS_NONE, MS_NONE },
    /* case 12: top inside */     { 3, 1, MS_NONE, MS_NONE },
    /* case 13: all but BR */     { 0, 1, MS_NONE, MS_NONE },
    /* case 14: all but BL */     { 3, 0, MS_NONE, MS_NONE },
    /* case 15: all inside */     { MS_NONE, MS_NONE, MS_NONE, MS_NONE },
};

static void demo_marching_squares(void)
{
    print_header(6, "Marching Squares");

    SDL_Log("  Marching squares extracts the boundary (f=0 isoline) from a grid");
    SDL_Log("  of SDF samples.  Each 2x2 cell has 4 corners, each either inside");
    SDL_Log("  (f<0) or outside (f>=0).  This gives 2^4 = 16 cases.");
    SDL_Log("  For each case, the algorithm connects edge midpoints with line");
    SDL_Log("  segments to approximate where the boundary crosses the cell.");
    SDL_Log(" ");

    float step = 2.0f * MARCH_RANGE / MARCH_GRID;
    float grid_vals[MARCH_GRID + 1][MARCH_GRID + 1];

    /* Evaluate SDF at grid vertices */
    for (int j = 0; j <= MARCH_GRID; j++) {
        for (int i = 0; i <= MARCH_GRID; i++) {
            float x = -MARCH_RANGE + i * step;
            float y = -MARCH_RANGE + j * step;
            vec2 p = { x, y };
            grid_vals[j][i] = march_field(p);
        }
    }

    /* Show grid values */
    SDL_Log("  Grid values for sdf2_circle(p, 1.0) on a %dx%d grid:",
            MARCH_GRID + 1, MARCH_GRID + 1);
    SDL_Log(" ");
    for (int j = MARCH_GRID; j >= 0; j--) {
        char buf[256];
        int len = 0;
        for (int i = 0; i <= MARCH_GRID; i++) {
            size_t remaining = sizeof(buf) - (size_t)len;
            int n = SDL_snprintf(buf + len, remaining,
                                 " %5.2f", (double)grid_vals[j][i]);
            if (n > 0 && (size_t)n < remaining) len += n;
            else break;  /* buffer full — stop accumulating */
        }
        SDL_Log("  %s", buf);
    }
    SDL_Log(" ");

    /* Count line segments produced */
    int segment_count = 0;
    SDL_Log("  Boundary-crossing cells (cell -> case -> segments):");
    SDL_Log(" ");
    for (int j = 0; j < MARCH_GRID; j++) {
        for (int i = 0; i < MARCH_GRID; i++) {
            float bl = grid_vals[j][i];
            float br = grid_vals[j][i + 1];
            float tr = grid_vals[j + 1][i + 1];
            float tl = grid_vals[j + 1][i];

            int case_idx = 0;
            if (bl < 0.0f) case_idx |= 1;
            if (br < 0.0f) case_idx |= 2;
            if (tr < 0.0f) case_idx |= 4;
            if (tl < 0.0f) case_idx |= 8;

            if (case_idx == 0 || case_idx == 15) continue;

            float cx = -MARCH_RANGE + (i + 0.5f) * step;
            float cy = -MARCH_RANGE + (j + 0.5f) * step;

            /* Saddle disambiguation: evaluate f at the cell center to
             * choose the correct diagonal connectivity.  The default table
             * entries assume "separated" topology; if the center sign
             * indicates "connected", swap the edge pairs. */
            int e0 = ms_edges[case_idx][0];
            int e1 = ms_edges[case_idx][1];
            int e2 = ms_edges[case_idx][2];
            int e3 = ms_edges[case_idx][3];

            if (case_idx == 5 || case_idx == 10) {
                vec2 center = vec2_create(cx, cy);
                float fc = march_field(center);
                if (case_idx == 5) {
                    /* BL+TR inside: separated = {0,3, 2,1}, connected = {0,1, 2,3} */
                    if (fc < 0.0f) { e0 = 0; e1 = 1; e2 = 2; e3 = 3; }
                } else {
                    /* TL+BR inside: separated = {3,0, 1,2}, connected = {3,2, 1,0} */
                    if (fc < 0.0f) { e0 = 3; e1 = 2; e2 = 1; e3 = 0; }
                }
            }

            int seg = (e0 != MS_NONE) ? 1 : 0;
            if (e2 != MS_NONE) seg = 2;
            segment_count += seg;

            SDL_Log("    Cell (%d,%d) center=(%5.2f,%5.2f): case %2d -> %d segment%s",
                    i, j, cx, cy, case_idx, seg, seg > 1 ? "s" : "");
        }
    }
    SDL_Log(" ");
    SDL_Log("  Total boundary segments: %d", segment_count);
    SDL_Log("  These segments approximate the unit circle boundary.");
    SDL_Log(" ");
}

/* ── 7. Isolines / Level Sets ──────────────────────────────────────────────── */

static void demo_isolines(void)
{
    print_header(7, "Isolines (Level Sets)");

    SDL_Log("  An isoline is the set of points where f(x,y) = c for some constant.");
    SDL_Log("  The curve f(x,y) = 0 is just one isoline.  The full family of");
    SDL_Log("  isolines for a circle SDF forms concentric circles:");
    SDL_Log(" ");
    SDL_Log("    f(p) = -0.5  ->  circle of radius 0.5 (inside)");
    SDL_Log("    f(p) =  0.0  ->  circle of radius 1.0 (the surface)");
    SDL_Log("    f(p) =  0.5  ->  circle of radius 1.5 (outside)");
    SDL_Log(" ");

    float step = 2.0f * GRID_RANGE / (GRID_SIZE - 1);
    char row[GRID_SIZE + 1];
    row[GRID_SIZE] = '\0';

    SDL_Log("  Isolines of sdf2_circle at c = -0.5, 0.0, +0.5:");
    SDL_Log("  ('-' near c=-0.5, '0' near c=0, '+' near c=+0.5)");
    SDL_Log(" ");
    for (int j = 0; j < GRID_SIZE; j++) {
        float y = GRID_RANGE - j * step;
        for (int i = 0; i < GRID_SIZE; i++) {
            float x = -GRID_RANGE + i * step;
            vec2 p = { x, y };
            float d = sdf2_circle(p, 1.0f);

            if (SDL_fabsf(d) < 0.15f)              row[i] = '0';
            else if (SDL_fabsf(d + 0.5f) < 0.15f)   row[i] = '-';
            else if (SDL_fabsf(d - 0.5f) < 0.15f)   row[i] = '+';
            else                                  row[i] = ' ';
        }
        SDL_Log("  %s", row);
    }
    SDL_Log(" ");
    SDL_Log("  This is the same concept behind topographic contour maps,");
    SDL_Log("  heat map isolines, and the level-set method in fluid simulation.");
    SDL_Log(" ");
}

/* ── Main ──────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log(" ");
    SDL_Log("  Math Lesson 17 -- Implicit 2D Curves");
    SDL_Log("  =====================================");
    SDL_Log(" ");
    SDL_Log("  An implicit curve is defined by f(x,y) = 0.  Unlike parametric");
    SDL_Log("  curves that trace points along the curve, implicit functions");
    SDL_Log("  classify every point in the plane as inside, outside, or on");
    SDL_Log("  the curve.  When |f| equals the Euclidean distance to the");
    SDL_Log("  nearest surface point, f is a signed distance function (SDF).");
    SDL_Log(" ");

    demo_implicit_vs_parametric();
    demo_sdf_primitives();
    demo_gradient();
    demo_csg_operations();
    demo_smooth_blend();
    demo_marching_squares();
    demo_isolines();

    SDL_Log("==========================================================");
    SDL_Log("  Summary");
    SDL_Log("==========================================================");
    SDL_Log(" ");
    SDL_Log("  Implicit curves f(x,y) = 0 are a fundamental representation");
    SDL_Log("  in computer graphics.  SDFs are the most useful form because:");
    SDL_Log(" ");
    SDL_Log("  1. Inside/outside test:    sign of f");
    SDL_Log("  2. Distance to surface:    |f|");
    SDL_Log("  3. Surface normal:         normalize(gradient(f))");
    SDL_Log("  4. Boolean operations:     min/max");
    SDL_Log("  5. Smooth blending:        smooth min/max (metaballs)");
    SDL_Log("  6. Anti-aliased rendering: 1 - smoothstep(-aa, aa, f)");
    SDL_Log("  7. Boundary extraction:    marching squares/cubes");
    SDL_Log(" ");
    SDL_Log("  See forge_math.h for: sdf2_circle, sdf2_box, sdf2_rounded_box,");
    SDL_Log("  sdf2_segment, sdf_union, sdf_intersection, sdf_subtraction,");
    SDL_Log("  sdf_smooth_union, sdf_smooth_intersection");
    SDL_Log(" ");

    SDL_Quit();
    return 0;
}
