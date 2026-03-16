/*
 * UI Theme Contrast Tests
 *
 * Automated tests for common/ui/forge_ui_theme.h — the WCAG contrast
 * ratio utilities and default theme validation.
 *
 * Tests cover:
 *   - Relative luminance computation for known sRGB values
 *   - Contrast ratio computation for known color pairs
 *   - Default theme WCAG AA validation (all 17 adjacent pairs)
 *   - Deliberately bad theme detection
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <stdio.h>
#include <math.h>
#include <SDL3/SDL.h>
#include "ui/forge_ui_theme.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST(name)                                                \
    do {                                                          \
        test_count++;                                             \
        SDL_Log("  [TEST] %s", name);                             \
    } while (0)

#define ASSERT_TRUE(expr)                                         \
    do {                                                          \
        if (!(expr)) {                                            \
            SDL_Log("    FAIL: %s (line %d)", #expr, __LINE__);   \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_EQ_INT(a, b)                                       \
    do {                                                          \
        int _a = (a), _b = (b);                                   \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %d, expected %d (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_NEAR(a, b, eps)                                    \
    do {                                                          \
        float _a = (a), _b = (b);                                 \
        if (SDL_isnan(_a) || SDL_isnan(_b)) {                             \
            SDL_Log("    FAIL: %s == %f, expected %f (NaN, "      \
                    "line %d)", #a, (double)_a, (double)_b,       \
                    __LINE__);                                    \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        if (SDL_fabsf(_a - _b) > (eps)) {                             \
            SDL_Log("    FAIL: %s == %f, expected %f (eps=%f, "   \
                    "line %d)", #a, (double)_a, (double)_b,       \
                    (double)(eps), __LINE__);                      \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

/* ── Helper: convert float [0,1] to hex byte ────────────────────────────── */

static int to_hex(float c)
{
    int v = (int)(c * 255.0f + 0.5f);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return v;
}

/* ── Test: relative luminance of black ──────────────────────────────────── */

static void test_relative_luminance_black(void)
{
    TEST("relative_luminance: black (0,0,0) gives luminance ~0");
    float lum = forge_ui_theme_relative_luminance(0.0f, 0.0f, 0.0f);
    ASSERT_NEAR(lum, 0.0f, 0.001f);
}

/* ── Test: relative luminance of white ──────────────────────────────────── */

static void test_relative_luminance_white(void)
{
    TEST("relative_luminance: white (1,1,1) gives luminance ~1");
    float lum = forge_ui_theme_relative_luminance(1.0f, 1.0f, 1.0f);
    ASSERT_NEAR(lum, 1.0f, 0.001f);
}

/* ── Test: contrast ratio of black vs white ─────────────────────────────── */

static void test_contrast_ratio_black_white(void)
{
    TEST("contrast_ratio: black vs white is ~21:1");
    float ratio = forge_ui_theme_contrast_ratio(0.0f, 0.0f, 0.0f,
                                                 1.0f, 1.0f, 1.0f);
    ASSERT_NEAR(ratio, 21.0f, 0.1f);
}

/* ── Test: contrast ratio of same color ─────────────────────────────────── */

static void test_contrast_ratio_same_color(void)
{
    TEST("contrast_ratio: same color gives 1:1");
    float ratio = forge_ui_theme_contrast_ratio(0.5f, 0.3f, 0.7f,
                                                 0.5f, 0.3f, 0.7f);
    ASSERT_NEAR(ratio, 1.0f, 0.001f);
}

/* ── Test: default theme passes all WCAG AA pairs ───────────────────────── */

static void test_default_theme_all_pairs_pass(void)
{
    TEST("validate: default theme passes all 17 adjacent pairs (WCAG AA)");
    ForgeUiTheme theme = forge_ui_theme_default();
    int failures = forge_ui_theme_validate(&theme);

    if (failures > 0) {
        /* Print detailed diagnostics for every failing pair.
         * Text pairs require 4.5:1 (SC 1.4.3); non-text graphical
         * components require 3:1 (SC 1.4.11). */
        const float AA_TEXT    = 4.5f;
        const float AA_NONTEXT = 3.0f;

        struct {
            const ForgeUiColor *fg;
            const ForgeUiColor *bg;
            const char *name;
            float min_ratio;
        } pairs[] = {
            { &theme.text,           &theme.bg,              "text / bg",             AA_TEXT },
            { &theme.text,           &theme.surface,         "text / surface",        AA_TEXT },
            { &theme.text,           &theme.surface_hot,     "text / surface_hot",    AA_TEXT },
            { &theme.text,           &theme.surface_active,  "text / surface_active", AA_TEXT },
            { &theme.title_bar_text, &theme.title_bar,       "title_bar_text / title_bar", AA_TEXT },
            { &theme.text_dim,       &theme.bg,              "text_dim / bg",         AA_TEXT },
            { &theme.text_dim,       &theme.surface,         "text_dim / surface",    AA_TEXT },
            { &theme.accent,         &theme.bg,              "accent / bg",           AA_TEXT },
            { &theme.accent,         &theme.surface,         "accent / surface",      AA_TEXT },
            { &theme.accent,         &theme.surface_active,  "accent / surface_active", AA_TEXT },
            { &theme.accent_hot,     &theme.surface_hot,     "accent_hot / surface_hot", AA_NONTEXT },
            { &theme.border,         &theme.bg,              "border / bg",           AA_NONTEXT },
            { &theme.border_focused, &theme.surface,         "border_focused / surface", AA_NONTEXT },
            { &theme.accent,         &theme.scrollbar_track,  "accent / scrollbar_track", AA_NONTEXT },
            { &theme.accent_hot,     &theme.scrollbar_track,  "accent_hot / scrollbar_track", AA_NONTEXT },
            { &theme.surface_hot,    &theme.scrollbar_track,  "surface_hot / scrollbar_track", AA_NONTEXT },
            { &theme.cursor,         &theme.surface_active,  "cursor / surface_active", AA_TEXT },
        };

        int count = (int)(sizeof(pairs) / sizeof(pairs[0]));
        for (int i = 0; i < count; i++) {
            float ratio = forge_ui_theme_contrast_ratio(
                pairs[i].fg->r, pairs[i].fg->g, pairs[i].fg->b,
                pairs[i].bg->r, pairs[i].bg->g, pairs[i].bg->b);
            if (ratio < pairs[i].min_ratio) {
                SDL_Log("    FAIL pair: %-35s  fg=#%02x%02x%02x  "
                        "bg=#%02x%02x%02x  ratio=%.2f:1  min=%.1f:1",
                        pairs[i].name,
                        to_hex(pairs[i].fg->r), to_hex(pairs[i].fg->g),
                        to_hex(pairs[i].fg->b),
                        to_hex(pairs[i].bg->r), to_hex(pairs[i].bg->g),
                        to_hex(pairs[i].bg->b),
                        (double)ratio, (double)pairs[i].min_ratio);
            }
        }
    }

    ASSERT_EQ_INT(failures, 0);
}

/* ── Test: bad theme fails validation ───────────────────────────────────── */

static void test_bad_theme_fails(void)
{
    TEST("validate: theme with low-contrast text returns failures");

    /* Start from default, then sabotage: set text color same as bg */
    ForgeUiTheme bad = forge_ui_theme_default();
    bad.text = bad.bg;           /* text identical to background */
    bad.text_dim = bad.bg;       /* dim text identical to background */
    bad.title_bar_text = bad.title_bar;  /* title text identical to bar */

    int failures = forge_ui_theme_validate(&bad);
    ASSERT_TRUE(failures > 0);

    SDL_Log("    (bad theme produced %d failures, as expected)", failures);
}

/* ── Test: luminance clamps negative inputs to zero ────────────────────── */

static void test_relative_luminance_clamps_negative(void)
{
    TEST("relative_luminance: negative inputs clamped to 0 (same as black)");
    float lum = forge_ui_theme_relative_luminance(-1.0f, -0.5f, -100.0f);
    ASSERT_NEAR(lum, 0.0f, 0.001f);
}

/* ── Test: luminance clamps inputs above 1.0 ──────────────────────────── */

static void test_relative_luminance_clamps_above_one(void)
{
    TEST("relative_luminance: inputs > 1.0 clamped to 1 (same as white)");
    float lum = forge_ui_theme_relative_luminance(2.0f, 5.0f, 1.5f);
    ASSERT_NEAR(lum, 1.0f, 0.001f);
}

/* ── Test: contrast ratio with out-of-range inputs ────────────────────── */

static void test_contrast_ratio_out_of_range(void)
{
    TEST("contrast_ratio: out-of-range inputs produce valid ratio");
    /* Both clamped to black → ratio should be 1:1 */
    float ratio = forge_ui_theme_contrast_ratio(-1.0f, -1.0f, -1.0f,
                                                 -2.0f, -2.0f, -2.0f);
    ASSERT_NEAR(ratio, 1.0f, 0.001f);
}

/* ── Test: validate with NULL theme returns -1 ────────────────────────── */

static void test_validate_null_theme(void)
{
    TEST("validate: NULL theme returns -1");
    int result = forge_ui_theme_validate(NULL);
    ASSERT_EQ_INT(result, -1);
}

/* ── Test: luminance returns 0 for NaN inputs ─────────────────────────── */

static void test_relative_luminance_nan(void)
{
    TEST("relative_luminance: NaN inputs return 0 (not NaN)");
    float lum_r = forge_ui_theme_relative_luminance(NAN, 0.5f, 0.5f);
    float lum_g = forge_ui_theme_relative_luminance(0.5f, NAN, 0.5f);
    float lum_b = forge_ui_theme_relative_luminance(0.5f, 0.5f, NAN);
    ASSERT_NEAR(lum_r, 0.0f, 0.001f);
    ASSERT_NEAR(lum_g, 0.0f, 0.001f);
    ASSERT_NEAR(lum_b, 0.0f, 0.001f);
}

static void test_relative_luminance_inf(void)
{
    TEST("relative_luminance: Infinity inputs return 0 (not Inf)");
    float lum_pos = forge_ui_theme_relative_luminance(INFINITY, 0.5f, 0.5f);
    float lum_neg = forge_ui_theme_relative_luminance(-INFINITY, 0.5f, 0.5f);
    ASSERT_NEAR(lum_pos, 0.0f, 0.001f);
    ASSERT_NEAR(lum_neg, 0.0f, 0.001f);
}

static void test_contrast_ratio_nan(void)
{
    TEST("contrast_ratio: NaN input treated as black (luminance 0)");
    /* NaN red channel -> luminance returns 0 (black).
     * Black vs white -> ratio ~21:1.  The key property: the result is
     * a finite, valid number — not NaN or Inf. */
    float ratio = forge_ui_theme_contrast_ratio(NAN, 0.0f, 0.0f,
                                                 1.0f, 1.0f, 1.0f);
    ASSERT_TRUE(forge_isfinite(ratio));
    ASSERT_NEAR(ratio, 21.0f, 0.1f);
}

static void test_contrast_ratio_inf(void)
{
    TEST("contrast_ratio: Infinity input treated as black (luminance 0)");
    /* Infinity red channel -> luminance returns 0 (black).
     * Black vs white -> ratio ~21:1. */
    float ratio = forge_ui_theme_contrast_ratio(INFINITY, 0.0f, 0.0f,
                                                 1.0f, 1.0f, 1.0f);
    ASSERT_TRUE(forge_isfinite(ratio));
    ASSERT_NEAR(ratio, 21.0f, 0.1f);
}

static void test_validate_nan_theme_fails(void)
{
    TEST("validate: theme with NaN color channel reports failures");
    ForgeUiTheme bad = forge_ui_theme_default();
    bad.text.r = NAN;
    int failures = forge_ui_theme_validate(&bad);
    ASSERT_TRUE(failures > 0);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== UI Theme Contrast Tests ===");

    /* NaN/Inf guards */
    test_relative_luminance_nan();
    test_relative_luminance_inf();
    test_contrast_ratio_nan();
    test_contrast_ratio_inf();
    test_validate_nan_theme_fails();

    test_relative_luminance_black();
    test_relative_luminance_white();
    test_contrast_ratio_black_white();
    test_contrast_ratio_same_color();
    test_relative_luminance_clamps_negative();
    test_relative_luminance_clamps_above_one();
    test_contrast_ratio_out_of_range();
    test_default_theme_all_pairs_pass();
    test_bad_theme_fails();
    test_validate_null_theme();

    SDL_Log("--- Results: %d tests, %d passed, %d failed ---",
            test_count, pass_count, fail_count);

    SDL_Quit();
    return fail_count > 0 ? 1 : 0;
}
