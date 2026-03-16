/*
 * Physics Test Framework — shared macros and counters
 *
 * Included by test_physics.c, test_physics_rbc.c, and test_physics_shapes.c
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef TEST_PHYSICS_COMMON_H
#define TEST_PHYSICS_COMMON_H

#include <SDL3/SDL.h>
#include <math.h>
#include "math/forge_math.h"
#include "physics/forge_physics.h"

/* ── Test counters (defined in test_physics.c) ──────────────────────────── */

extern int test_count;
extern int pass_count;
extern int fail_count;

/* ── Test runners (defined in separate translation units) ───────────── */

void run_rbc_tests(void);
void run_collision_shape_tests(void);

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
