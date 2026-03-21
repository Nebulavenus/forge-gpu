/*
 * Physics Test Framework — shared macros and counters
 *
 * Included by test_physics.c, test_physics_rbc.c, test_physics_shapes.c,
 * test_physics_sap.c, test_physics_gjk.c, test_physics_epa.c,
 * test_physics_production.c, and test_physics_manifold.c
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef TEST_PHYSICS_COMMON_H
#define TEST_PHYSICS_COMMON_H

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* NaN and infinity constants — C99 defines NAN and INFINITY in <math.h>, which
 * SDL3/SDL.h transitively includes on most platforms.  These fallbacks use
 * compiler builtins where available, and volatile runtime division elsewhere
 * to avoid MSVC error C2124 (compile-time division by zero). */
#ifndef NAN
#if defined(__GNUC__) || defined(__clang__)
#define NAN __builtin_nanf("")
#else
static float forge_test__make_nan(void) { volatile float z = 0.0f; return z / z; }
#define NAN forge_test__make_nan()
#endif
#endif
#ifndef INFINITY
#if defined(__GNUC__) || defined(__clang__)
#define INFINITY __builtin_inff()
#else
static float forge_test__make_inf(void) { volatile float z = 0.0f; return 1.0f / z; }
#define INFINITY forge_test__make_inf()
#endif
#endif
#include "containers/forge_containers.h"
#include "physics/forge_physics.h"

/* ── Test counters (defined in test_physics.c) ──────────────────────────── */

extern int test_count;
extern int pass_count;
extern int fail_count;

/* ── Test runners (defined in separate translation units) ───────────── */

void run_rbc_tests(void);
void run_collision_shape_tests(void);
void run_sap_tests(void);
void run_gjk_tests(void);
void run_production_tests(void);
void run_epa_tests(void);
void run_manifold_tests(void);

/* ── Shared test constants ─────────────────────────────────────────────── */

#define EPSILON           0.001f
#define PHYSICS_DT        (1.0f / 60.0f)

/* ── Test macros ────────────────────────────────────────────────────────── */

#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_NEAR(a, b, eps) \
    do { \
        double actual_ = (double)(a); \
        double expected_ = (double)(b); \
        double eps_ = (double)(eps); \
        if (!forge_isfinite(actual_) || !forge_isfinite(expected_) || \
            SDL_fabs(actual_ - expected_) > eps_) { \
            SDL_Log("    FAIL: Expected %.6f, got %.6f (eps=%.6f)", \
                    expected_, actual_, eps_); \
            fail_count++; \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            SDL_Log("    FAIL: Condition false: %s", #cond); \
            fail_count++; \
            return; \
        } \
    } while (0)

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

#endif /* TEST_PHYSICS_COMMON_H */
