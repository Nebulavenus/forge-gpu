/*
 * Shared test framework for scene renderer tests.
 *
 * Provides assertion macros, pass/fail counters, and the gpu_available flag
 * so multiple translation units can contribute test functions to a single
 * test executable.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef TEST_SCENE_COMMON_H
#define TEST_SCENE_COMMON_H

#include <SDL3/SDL.h>
#include <math.h>
#include <stddef.h>

#include "math/forge_math.h"

/* FORGE_SCENE_MODEL_SUPPORT must be defined before including forge_scene.h
 * so the model types and functions are declared. The IMPLEMENTATION macros
 * are only defined in test_scene.c (the primary translation unit) to avoid
 * duplicate symbol definitions when linking multiple .c files. */
#define FORGE_SCENE_MODEL_SUPPORT
#include "scene/forge_scene.h"

/* ── Shared counters (defined in test_scene.c) ──────────────────────────── */

extern int test_count;
extern int pass_count;
extern int fail_count;
extern int skip_count;
extern bool gpu_available;

/* ── Test macros ────────────────────────────────────────────────────────── */

#define EPSILON 0.001f

#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_NEAR(a, b, eps) \
    do { \
        float _an = (a), _bn = (b); \
        if (!isfinite(_an) || !isfinite(_bn)) { \
            SDL_Log("    FAIL: Non-finite operand: got %.6f, expected %.6f", \
                    (double)_an, (double)_bn); \
            fail_count++; \
            return; \
        } \
        if (fabsf(_an - _bn) > (eps)) { \
            SDL_Log("    FAIL: Expected %.6f, got %.6f (eps=%.6f)", \
                    (double)_bn, (double)_an, (double)(eps)); \
            fail_count++; \
            return; \
        } \
    } while (0);

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            SDL_Log("    FAIL: Condition false: %s", #cond); \
            fail_count++; \
            return; \
        } \
    } while (0)

#define ASSERT_INT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            SDL_Log("    FAIL: Expected %d, got %d", (b), (a)); \
            fail_count++; \
            return; \
        } \
    } while (0)

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

#define SKIP_TEST() \
        SDL_Log("    SKIP"); \
        skip_count++; \
    } while (0)

/* Cleanup-aware variants for tests that allocate resources. */
#define TEST_C(name) \
    do { \
        int _test_failed = 0; \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_TRUE_C(cond) \
    if (!(cond)) { \
        SDL_Log("    FAIL: Condition false: %s", #cond); \
        fail_count++; \
        _test_failed = 1; \
        goto cleanup; \
    }

#define ASSERT_NEAR_C(a, b, eps) \
    do { \
        float _anc = (a), _bnc = (b); \
        if (!isfinite(_anc) || !isfinite(_bnc)) { \
            SDL_Log("    FAIL: Non-finite operand: got %.6f, expected %.6f", \
                    (double)_anc, (double)_bnc); \
            fail_count++; \
            _test_failed = 1; \
            goto cleanup; \
        } \
        if (fabsf(_anc - _bnc) > (eps)) { \
            SDL_Log("    FAIL: Expected %.6f, got %.6f (eps=%.6f)", \
                    (double)_bnc, (double)_anc, (double)(eps)); \
            fail_count++; \
            _test_failed = 1; \
            goto cleanup; \
        } \
    } while (0);

#define END_TEST_C() \
        if (!_test_failed) { \
            SDL_Log("    PASS"); \
            pass_count++; \
        } \
    } while (0)

#endif /* TEST_SCENE_COMMON_H */
