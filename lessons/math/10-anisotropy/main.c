/*
 * Math Lesson 10 — Anisotropy vs Isotropy
 *
 * Demonstrates direction-dependent vs direction-independent behavior:
 *   1. Core concept — isotropy (circle) vs anisotropy (ellipse)
 *   2. The screen-space Jacobian — how UV coordinates change per pixel
 *   3. Singular values — the axes of the pixel footprint ellipse
 *   4. Anisotropy ratio — how elongated the footprint is
 *   5. Isotropic vs anisotropic filtering — mip selection comparison
 *   6. Anisotropic noise — directional patterns (wood grain, brushed metal)
 *   7. Anisotropic friction — direction-dependent resistance
 *
 * This is a console program — no window needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>
#include "math/forge_math.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void print_header(const char *name)
{
    printf("\n%s\n", name);
    printf("--------------------------------------------------------------\n");
}

static void print_mat2(const char *name, mat2 m)
{
    printf("  %s = [ %7.4f  %7.4f ]\n", name, m.m[0], m.m[2]);
    printf("  %s   [ %7.4f  %7.4f ]\n", "    ", m.m[1], m.m[3]);
}

/* ── Jacobian for a tilted plane ──────────────────────────────────────── */

/* Compute the screen-space Jacobian for a textured quad viewed at a tilt.
 *
 * Imagine a flat textured plane tilted around the horizontal axis.
 * When viewed straight-on, one texel maps to roughly one pixel (isotropic).
 * When tilted, each pixel covers more texels along the tilt direction
 * (the footprint stretches) — the surface is anisotropic.
 *
 * The Jacobian J = d(uv)/d(screen) captures this mapping:
 *
 *   J = [ du/dx  du/dy ]   [ 1.0            0.0       ]
 *       [ dv/dx  dv/dy ] = [ 0.0    1/cos(tilt)       ]
 *
 * At 0 degrees:  J = identity (isotropic, 1:1 mapping)
 * At 80 degrees: J = [[1,0],[0,5.76]] (highly anisotropic)
 *
 * Why 1/cos? When the surface tilts away, perspective foreshortens it.
 * Each pixel now spans a longer strip of texture along the tilt axis.
 * The texels-per-pixel rate increases as 1/cos(tilt).
 */
static mat2 jacobian_tilted_plane(float tilt_deg)
{
    float tilt_rad = tilt_deg * FORGE_DEG2RAD;
    float cos_tilt = cosf(tilt_rad);
    /* Clamp to avoid division by zero at 90 degrees */
    if (cos_tilt < 0.001f) cos_tilt = 0.001f;
    return mat2_create(
        1.0f,          0.0f,
        0.0f, 1.0f / cos_tilt
    );
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    printf("\n");
    printf("==============================================================\n");
    printf("  Anisotropy vs Isotropy\n");
    printf("==============================================================\n");
    printf("\n");
    printf("  Isotropic  = same in all directions     (iso = equal)\n");
    printf("  Anisotropic = different in some directions (an = not)\n");
    printf("\n");
    printf("  A circle is isotropic -- same radius everywhere.\n");
    printf("  An ellipse is anisotropic -- radius depends on direction.\n");

    /* ── Part 1: Circle vs ellipse ────────────────────────────────── */
    print_header("Part 1: Isotropic circle vs anisotropic ellipse");

    printf("\n  Sampling the radius at 8 directions (45-degree steps):\n\n");
    printf("  Direction   Circle (r=1)   Ellipse (a=1, b=0.5)\n");
    printf("  ---------   ------------   ---------------------\n");

    for (int i = 0; i < 8; i++) {
        float angle_deg = (float)(i * 45);
        float angle_rad = angle_deg * FORGE_DEG2RAD;
        float cos_a = cosf(angle_rad);
        float sin_a = sinf(angle_rad);

        /* Circle: r = 1 in all directions */
        float circle_r = 1.0f;

        /* Ellipse: x = cos(t), y = sin(t) scaled by (a=1, b=0.5)
         * Point on ellipse: (cos(t), 0.5*sin(t))
         * Distance from origin: sqrt(cos^2 + 0.25*sin^2) */
        float ellipse_r = sqrtf(cos_a * cos_a + 0.25f * sin_a * sin_a);

        printf("  %5.0f deg     %.4f          %.4f\n",
               angle_deg, circle_r, ellipse_r);
    }

    printf("\n  The circle has the same radius in every direction (isotropic).\n");
    printf("  The ellipse stretches -- its radius depends on direction\n");
    printf("  (anisotropic, with ratio a/b = 1/0.5 = 2:1).\n");

    /* ── Part 2: The mat2 type ────────────────────────────────────── */
    print_header("Part 2: 2x2 matrices transform circles into ellipses");

    printf("\n  A 2x2 matrix maps the unit circle to an ellipse.\n");
    printf("  The singular values are the ellipse's semi-axis lengths.\n\n");

    /* Identity: maps circle to itself */
    mat2 identity = mat2_identity();
    vec2 sv_id = mat2_singular_values(identity);
    printf("  Identity matrix:\n");
    print_mat2("M", identity);
    printf("  Singular values: (%.4f, %.4f) -> ratio %.2f:1 (isotropic)\n\n",
           sv_id.x, sv_id.y, mat2_anisotropy_ratio(identity));

    /* Scale: stretches one axis */
    mat2 stretch = mat2_create(1.0f, 0.0f, 0.0f, 0.25f);
    vec2 sv_st = mat2_singular_values(stretch);
    printf("  Vertical compression:\n");
    print_mat2("M", stretch);
    printf("  Singular values: (%.4f, %.4f) -> ratio %.2f:1 (anisotropic)\n\n",
           sv_st.x, sv_st.y, mat2_anisotropy_ratio(stretch));

    /* Rotation: preserves shape */
    float angle = 45.0f * FORGE_DEG2RAD;
    mat2 rotate = mat2_create(cosf(angle), -sinf(angle),
                               sinf(angle),  cosf(angle));
    vec2 sv_rot = mat2_singular_values(rotate);
    printf("  45-degree rotation:\n");
    print_mat2("M", rotate);
    printf("  Singular values: (%.4f, %.4f) -> ratio %.2f:1 (isotropic)\n",
           sv_rot.x, sv_rot.y, mat2_anisotropy_ratio(rotate));
    printf("  (rotations are isotropic -- they don't change shape)\n");

    /* ── Part 3: The Jacobian ─────────────────────────────────────── */
    print_header("Part 3: The screen-space Jacobian");

    printf("\n  When a textured surface is projected onto the screen,\n");
    printf("  the Jacobian matrix J describes how UV coordinates\n");
    printf("  change per pixel:\n\n");
    printf("       J = [ du/dx  du/dy ]\n");
    printf("           [ dv/dx  dv/dy ]\n\n");
    printf("  Each column is a partial derivative: how much does (u,v)\n");
    printf("  change when we move one pixel right (x) or down (y)?\n\n");
    printf("  For a plane tilted around the horizontal axis:\n");
    printf("  - du/dx = 1 (horizontal UV rate unchanged)\n");
    printf("  - dv/dy = 1/cos(tilt) (more texels per pixel as tilt grows)\n\n");

    float tilt_angles[] = { 0.0f, 30.0f, 60.0f, 80.0f, 85.0f };
    int num_angles = 5;

    for (int i = 0; i < num_angles; i++) {
        mat2 J = jacobian_tilted_plane(tilt_angles[i]);
        printf("  Plane tilted %.0f degrees:\n", tilt_angles[i]);
        print_mat2("J", J);
        printf("\n");
    }

    /* ── Part 4: Singular values of the Jacobian ──────────────────── */
    print_header("Part 4: Singular values = pixel footprint axes");

    printf("\n  The singular values of J are the lengths of the major and\n");
    printf("  minor axes of the pixel footprint ellipse in texture space.\n\n");
    printf("  sigma_1 (major) = longest stretch\n");
    printf("  sigma_2 (minor) = shortest stretch\n");
    printf("  Anisotropy ratio = sigma_1 / sigma_2\n\n");

    printf("  Tilt     sigma_1  sigma_2  Ratio   Description\n");
    printf("  ------   -------  -------  ------  ----------------------\n");

    for (int i = 0; i < num_angles; i++) {
        mat2 J = jacobian_tilted_plane(tilt_angles[i]);
        vec2 sv = mat2_singular_values(J);
        float ratio = mat2_anisotropy_ratio(J);
        const char *desc =
            ratio < 1.1f ? "isotropic" :
            ratio < 2.0f ? "mild anisotropy" :
            ratio < 4.0f ? "moderate" :
            "highly anisotropic";
        printf("  %5.0f    %7.4f  %7.4f  %6.2f  %s\n",
               tilt_angles[i], sv.x, sv.y, ratio, desc);
    }

    printf("\n  As the tilt increases, sigma_1 grows (the footprint stretches\n");
    printf("  along the tilt direction) while sigma_2 stays the same.\n");
    printf("  The ratio grows -- the footprint becomes more elongated.\n");

    /* ── Part 5: Isotropic vs anisotropic filtering ───────────────── */
    print_header("Part 5: Isotropic vs anisotropic texture filtering");

    printf("\n  Isotropic filtering (trilinear):\n");
    printf("    Uses the LARGER singular value to pick the mip level.\n");
    printf("    This prevents aliasing along the compressed axis, but\n");
    printf("    over-blurs the other axis (wastes detail).\n\n");
    printf("  Anisotropic filtering:\n");
    printf("    Uses the SMALLER singular value to pick the mip level\n");
    printf("    (preserving detail), then takes multiple samples along\n");
    printf("    the major axis to cover the elongated footprint.\n\n");

    printf("  Tilt   | Isotropic              | Anisotropic\n");
    printf("  -------|------------------------|------------------------------\n");

    for (int i = 0; i < num_angles; i++) {
        mat2 J = jacobian_tilted_plane(tilt_angles[i]);
        vec2 sv = mat2_singular_values(J);
        float ratio = mat2_anisotropy_ratio(J);

        /* Isotropic: mip from max singular value (larger = more blur) */
        float iso_mip = forge_log2f(sv.x > 0.001f ? sv.x : 0.001f);

        /* Anisotropic: mip from min singular value, multi-sample */
        float aniso_mip = forge_log2f(sv.y > 0.001f ? sv.y : 0.001f);
        int aniso_samples = (int)ceilf(ratio);
        if (aniso_samples < 1) aniso_samples = 1;

        printf("  %5.0f  | mip %5.2f (1 sample)   | mip %5.2f (%d samples)\n",
               tilt_angles[i], iso_mip, aniso_mip, aniso_samples);
    }

    printf("\n  At 80 degrees tilt:\n");
    printf("  - Isotropic uses the large singular value -> high mip level.\n");
    printf("    This avoids aliasing along the stretched axis, but blurs\n");
    printf("    the unstretched axis too (wastes detail you could keep).\n");
    printf("  - Anisotropic uses the small singular value -> low mip level\n");
    printf("    (stays sharp), then takes ~6 samples along the stretched\n");
    printf("    axis to cover the elongated footprint -- sharp AND alias-free.\n");

    /* ── Part 6: Eigenvalues of J^T * J ──────────────────────────── */
    print_header("Part 6: How GPUs compute this (eigenvalues of J^T * J)");

    printf("\n  GPUs compute screen-space derivatives using finite differences:\n");
    printf("    ddx = value(x+1, y) - value(x, y)   (per-pixel)\n");
    printf("    ddy = value(x, y+1) - value(x, y)\n\n");
    printf("  These give the Jacobian columns. The GPU then computes\n");
    printf("  J^T * J (a symmetric 2x2 matrix) and finds its eigenvalues.\n\n");
    printf("  The eigenvalues of J^T * J are the SQUARES of the singular\n");
    printf("  values of J. This avoids the need for a full SVD.\n\n");

    /* Show the J^T * J computation for a 75-degree tilt */
    float demo_tilt = 75.0f;
    mat2 J = jacobian_tilted_plane(demo_tilt);
    mat2 Jt = mat2_transpose(J);
    mat2 JtJ = mat2_multiply(Jt, J);
    vec2 sv = mat2_singular_values(J);

    printf("  Example: plane tilted %.0f degrees\n\n", demo_tilt);
    print_mat2("J   ", J);
    printf("\n");
    print_mat2("J^T ", Jt);
    printf("\n");
    print_mat2("J^TJ", JtJ);
    printf("\n");
    printf("  Eigenvalues of J^T*J: %.4f, %.4f\n",
           sv.x * sv.x, sv.y * sv.y);
    printf("  Singular values of J: %.4f, %.4f  (square roots)\n",
           sv.x, sv.y);
    printf("  Anisotropy ratio:     %.2f:1\n", mat2_anisotropy_ratio(J));

    /* ── Part 7: Anisotropic noise ────────────────────────────────── */
    print_header("Part 7: Anisotropic noise");

    printf("\n  Isotropic noise (like basic Perlin) looks the same in all\n");
    printf("  directions. To create directional patterns, scale the input\n");
    printf("  coordinates differently along each axis:\n\n");
    printf("    isotropic:  noise(x, y)         -- uniform\n");
    printf("    anisotropic: noise(x*sx, y*sy)  -- stretched\n\n");

    struct {
        const char *material;
        float sx, sy;
    } noise_examples[] = {
        { "Uniform (isotropic)",  1.0f,  1.0f },
        { "Wood grain",           1.0f,  8.0f },
        { "Brushed metal",       12.0f,  1.0f },
        { "Marble veins",         1.0f,  3.0f },
    };

    printf("  Material                 Scale (x, y)   Ratio\n");
    printf("  -----------------------  ------------   -----\n");
    for (int i = 0; i < 4; i++) {
        float sx = noise_examples[i].sx;
        float sy = noise_examples[i].sy;
        float ratio = sx > sy ? sx / sy : sy / sx;
        printf("  %-23s  (%5.1f, %4.1f)   %4.0f:1\n",
               noise_examples[i].material, sx, sy, ratio);
    }

    printf("\n  The stretch direction determines the pattern direction.\n");
    printf("  Wood grain stretches along the trunk (vertical).\n");
    printf("  Brushed metal stretches along the brush stroke (horizontal).\n");

    /* ── Part 8: Anisotropic friction ─────────────────────────────── */
    print_header("Part 8: Anisotropic friction");

    printf("\n  Isotropic friction: same resistance in all directions.\n");
    printf("  Anisotropic friction: resistance depends on direction.\n\n");

    struct {
        const char *surface;
        float along;   /* friction coefficient along the grain/groove */
        float across;  /* friction coefficient across the grain/groove */
    } friction_examples[] = {
        { "Rubber on concrete",  0.80f, 0.80f },
        { "Ice skate blade",     0.01f, 0.50f },
        { "Grooved metal",       0.20f, 0.60f },
        { "Tire (rolling dir)",  0.02f, 0.90f },
    };

    printf("  Surface                Along  Across  Ratio\n");
    printf("  ---------------------  -----  ------  -----\n");
    for (int i = 0; i < 4; i++) {
        float a = friction_examples[i].along;
        float c = friction_examples[i].across;
        float ratio = c > a ? c / a : a / c;
        printf("  %-21s  %5.2f   %5.2f  %5.1f:1\n",
               friction_examples[i].surface, a, c, ratio);
    }

    printf("\n  Isotropic surfaces (ratio ~1:1) resist equally in all\n");
    printf("  directions. Anisotropic surfaces have a 'preferred'\n");
    printf("  direction of motion -- the ice skate slides easily along\n");
    printf("  the blade but resists sideways motion (50:1 ratio).\n");

    /* ── Summary ──────────────────────────────────────────────────── */
    print_header("Summary");

    printf("\n  Anisotropy = direction matters.\n\n");
    printf("  The Jacobian captures how a mapping stretches space.\n");
    printf("  Its singular values measure the stretch in each direction.\n");
    printf("  The ratio of singular values is the anisotropy ratio.\n\n");
    printf("  GPU texture filtering uses this:\n");
    printf("  - Isotropic (trilinear): picks mip from largest axis (blurry)\n");
    printf("  - Anisotropic: picks mip from smallest axis, multi-samples\n");
    printf("    along the largest axis (sharp AND alias-free)\n\n");
    printf("  Beyond textures, anisotropy appears in noise generation\n");
    printf("  (wood grain, brushed metal) and physics (friction on ice,\n");
    printf("  grooved surfaces, tire grip).\n\n");

    SDL_Quit();
    return 0;
}
