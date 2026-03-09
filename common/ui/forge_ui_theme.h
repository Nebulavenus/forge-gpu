/*
 * forge_ui_theme.h -- Centralized color theme for the forge-gpu UI system
 *
 * Defines ForgeUiColor, ForgeUiTheme, and WCAG contrast ratio utilities.
 * All UI widget colors are derived from a single ForgeUiTheme struct on
 * the context, making palette changes a one-line swap.
 *
 * The default theme derives from the project's matplotlib diagram palette
 * (scripts/forge_diagrams/_common.py STYLE dict):
 *   - bg:      #1a1a2e  (deep dark navy)
 *   - surface: #252545  (widget backgrounds)
 *   - text:    #e0e0f0  (primary text, light blue-white)
 *   - accent:  #4fc3f7  (interactive highlights, cyan)
 *   - border:  #6b6b8b  (subtle outlines)
 *
 * WCAG 2.1 accessibility: forge_ui_theme_validate() checks all adjacent
 * color pairs (text on background, accent on surface, etc.) against two
 * AA contrast ratio thresholds:
 *   - 4.5:1 (SC 1.4.3) for normal text — any text the user reads
 *   - 3.0:1 (SC 1.4.11) for non-text UI components — borders, thumbs,
 *     outlines, and other graphical elements that convey meaning through
 *     shape and position rather than readable characters
 *
 * Usage:
 *   ForgeUiTheme theme = forge_ui_theme_default();
 *   if (!forge_ui_ctx_set_theme(&ctx, theme)) { handle_error(); }
 *
 *   // Or just use the default (installed by forge_ui_ctx_init):
 *   ctx.theme.accent.r  // 0.310 (#4fc3f7)
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_UI_THEME_H
#define FORGE_UI_THEME_H

#include <math.h>

/* ── Types ──────────────────────────────────────────────────────────────── */

/* RGBA color with float components in [0, 1]. */
typedef struct ForgeUiColor {
    float r;  /* red channel,   0.0 (none) to 1.0 (full), sRGB color space */
    float g;  /* green channel, 0.0 (none) to 1.0 (full) */
    float b;  /* blue channel,  0.0 (none) to 1.0 (full) */
    float a;  /* alpha channel, 0.0 (transparent) to 1.0 (opaque) */
} ForgeUiColor;

/* Centralized color palette for all UI widgets.
 *
 * Each slot maps to one or more widget states.  Widgets read from the
 * theme at draw time rather than referencing compile-time constants,
 * so changing a single ForgeUiTheme instance recolors the entire UI.
 *
 * Slot naming convention:
 *   - bg/surface/text  = structural roles (background, interactive, label)
 *   - _hot / _active   = interaction state modifiers
 *   - accent           = highlight color for focus and checked state
 *   - border           = unfocused outlines; border_focused = focused */
typedef struct ForgeUiTheme {
    ForgeUiColor bg;              /* app/panel background (darkest layer) */
    ForgeUiColor surface;         /* widget backgrounds, input fields (sits on bg) */
    ForgeUiColor surface_hot;     /* hovered widget bg (brighter than surface) */
    ForgeUiColor surface_active;  /* pressed widget bg (darker than surface) */
    ForgeUiColor title_bar;       /* panel/window title bar bg (brighter than surface) */
    ForgeUiColor title_bar_text;  /* title bar label; 4.5:1 on title_bar (SC 1.4.3) */
    ForgeUiColor text;            /* primary text; 4.5:1 on bg/surface/surface_* (SC 1.4.3) */
    ForgeUiColor text_dim;        /* secondary/disabled text; 4.5:1 on bg/surface (SC 1.4.3) */
    ForgeUiColor accent;          /* check fill, active slider, focused border, sb thumb;
                                   * 4.5:1 on bg/surface/surface_active (SC 1.4.3) */
    ForgeUiColor accent_hot;      /* hovered accent; 3:1 on surface_hot (SC 1.4.11) */
    ForgeUiColor border;          /* unfocused borders, outlines; 3:1 on bg (SC 1.4.11) */
    ForgeUiColor border_focused;  /* focused borders (= accent); 3:1 on surface (SC 1.4.11) */
    ForgeUiColor scrollbar_track; /* scrollbar track bg (darker than bg) */
    ForgeUiColor cursor;          /* text input cursor bar; 4.5:1 on surface_active (SC 1.4.3) */
} ForgeUiTheme;

/* ── Default theme ──────────────────────────────────────────────────────── */

/* Return the canonical dark theme derived from the STYLE dict in
 * scripts/forge_diagrams/_common.py.  All hex values are documented
 * in lessons/ui/13-theming-and-color-system/README.md. */
static inline ForgeUiTheme forge_ui_theme_default(void)
{
    ForgeUiTheme t;
    /* bg: #1a1a2e */
    t.bg              = (ForgeUiColor){ 0.102f, 0.102f, 0.180f, 1.0f };
    /* surface: #252545 */
    t.surface         = (ForgeUiColor){ 0.145f, 0.145f, 0.271f, 1.0f };
    /* surface_hot: #5e5e7c — hovered widgets, scrollbar thumb (3:1 on track) */
    t.surface_hot     = (ForgeUiColor){ 0.369f, 0.369f, 0.486f, 1.0f };
    /* surface_active: surface - 0.05 brightness */
    t.surface_active  = (ForgeUiColor){ 0.095f, 0.095f, 0.221f, 1.0f };
    /* title_bar: surface + 0.05 brightness */
    t.title_bar       = (ForgeUiColor){ 0.195f, 0.195f, 0.321f, 1.0f };
    /* title_bar_text: #e0e0f0 */
    t.title_bar_text  = (ForgeUiColor){ 0.878f, 0.878f, 0.941f, 1.0f };
    /* text: #e0e0f0 */
    t.text            = (ForgeUiColor){ 0.878f, 0.878f, 0.941f, 1.0f };
    /* text_dim: #9393b3 — secondary/disabled text, AA-safe on surface */
    t.text_dim        = (ForgeUiColor){ 0.576f, 0.576f, 0.702f, 1.0f };
    /* accent: #4fc3f7 */
    t.accent          = (ForgeUiColor){ 0.310f, 0.765f, 0.969f, 1.0f };
    /* accent_hot: accent + 0.08 brightness */
    t.accent_hot      = (ForgeUiColor){ 0.390f, 0.845f, 1.000f, 1.0f };
    /* border: #6b6b8b — subtle outlines, 3:1 on bg */
    t.border          = (ForgeUiColor){ 0.420f, 0.420f, 0.545f, 1.0f };
    /* border_focused: = accent #4fc3f7 */
    t.border_focused  = (ForgeUiColor){ 0.310f, 0.765f, 0.969f, 1.0f };
    /* scrollbar_track: bg - 0.05 brightness */
    t.scrollbar_track = (ForgeUiColor){ 0.052f, 0.052f, 0.130f, 1.0f };
    /* cursor: = accent #4fc3f7 */
    t.cursor          = (ForgeUiColor){ 0.310f, 0.765f, 0.969f, 1.0f };
    return t;
}

/* ── WCAG contrast ratio utilities ──────────────────────────────────────── */

/* Compute the relative luminance of an sRGB color (WCAG 2.1 definition).
 *
 * Each channel is linearized from sRGB gamma (the piecewise function
 * defined in IEC 61966-2-1), then weighted by the CIE 1931 luminosity
 * coefficients:  L = 0.2126 R + 0.7152 G + 0.0722 B.
 *
 * The returned value is in [0, 1], where 0 is black and 1 is white. */
static inline float forge_ui_theme_relative_luminance(float r, float g, float b)
{
    /* Reject non-finite inputs: NaN comparisons always return false,
     * so NaN would bypass the clamp logic and propagate through the
     * sRGB linearization, producing NaN luminance that silently passes
     * contrast validation (NaN < threshold is false). */
    if (!isfinite(r) || !isfinite(g) || !isfinite(b)) return 0.0f;

    /* Clamp inputs to valid sRGB range [0, 1] to prevent undefined
     * behavior from powf with negative bases or out-of-range values. */
    if (r < 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;

    /* sRGB → linear: if C <= 0.04045, Clinear = C / 12.92
     *                 else Clinear = ((C + 0.055) / 1.055) ^ 2.4 */
    float rl = (r <= 0.04045f) ? r / 12.92f : powf((r + 0.055f) / 1.055f, 2.4f);
    float gl = (g <= 0.04045f) ? g / 12.92f : powf((g + 0.055f) / 1.055f, 2.4f);
    float bl = (b <= 0.04045f) ? b / 12.92f : powf((b + 0.055f) / 1.055f, 2.4f);
    return 0.2126f * rl + 0.7152f * gl + 0.0722f * bl;
}

/* Compute the WCAG 2.1 contrast ratio between two sRGB colors.
 *
 * Contrast ratio = (L_lighter + 0.05) / (L_darker + 0.05)
 *
 * The 0.05 offset prevents division by zero for pure black and models
 * ambient light reflected off the display surface.
 *
 * Returns a value in [1, 21].  WCAG AA requires >= 4.5 for normal text. */
static inline float forge_ui_theme_contrast_ratio(float r1, float g1, float b1,
                                                   float r2, float g2, float b2)
{
    float l1 = forge_ui_theme_relative_luminance(r1, g1, b1);
    float l2 = forge_ui_theme_relative_luminance(r2, g2, b2);

    /* If either luminance is non-finite (should not happen after the
     * guard in relative_luminance, but defensive), return 0 so the
     * caller treats the pair as failing contrast validation. */
    if (!isfinite(l1) || !isfinite(l2)) return 0.0f;

    /* Ensure l1 >= l2 (lighter on top) */
    if (l2 > l1) {
        float tmp = l1;
        l1 = l2;
        l2 = tmp;
    }

    return (l1 + 0.05f) / (l2 + 0.05f);
}

/* Validate all adjacent color pairs in a theme against WCAG 2.1 AA.
 *
 * "Adjacent" means the two colors appear next to each other in the
 * rendered UI — text on its background, accent on surface, etc.
 *
 * Text-on-background pairs are checked against the AA normal-text
 * threshold (4.5:1, SC 1.4.3).  Non-text graphical UI components
 * (borders, scrollbar tracks) use the lower SC 1.4.11 threshold
 * (3:1) appropriate for user-interface components.
 *
 * Returns the number of pairs that fail.  A return of 0 means every
 * pair meets its applicable threshold. */
static inline int forge_ui_theme_validate(const ForgeUiTheme *theme)
{
    if (!theme) return -1;

    /* WCAG AA thresholds */
    const float AA_TEXT    = 4.5f;  /* SC 1.4.3  — normal text */
    const float AA_NONTEXT = 3.0f;  /* SC 1.4.11 — UI components */

    int failures = 0;

    /* Each entry: foreground, background, pair name, threshold.
     * Text pairs use 4.5:1; non-text graphical pairs use 3:1. */
    struct {
        const ForgeUiColor *fg;
        const ForgeUiColor *bg;
        const char *name;
        float min_ratio;
    } pairs[] = {
        /* text on backgrounds (4.5:1) */
        { &theme->text,           &theme->bg,             "text / bg",             AA_TEXT },
        { &theme->text,           &theme->surface,        "text / surface",        AA_TEXT },
        { &theme->text,           &theme->surface_hot,    "text / surface_hot",    AA_TEXT },
        { &theme->text,           &theme->surface_active, "text / surface_active", AA_TEXT },
        { &theme->title_bar_text, &theme->title_bar,      "title_bar_text / title_bar", AA_TEXT },
        /* dim text on backgrounds (4.5:1) */
        { &theme->text_dim,       &theme->bg,             "text_dim / bg",         AA_TEXT },
        { &theme->text_dim,       &theme->surface,        "text_dim / surface",    AA_TEXT },
        /* accent text on backgrounds (4.5:1) */
        { &theme->accent,         &theme->bg,             "accent / bg",           AA_TEXT },
        { &theme->accent,         &theme->surface,        "accent / surface",      AA_TEXT },
        { &theme->accent,         &theme->surface_active, "accent / surface_active", AA_TEXT },
        /* accent hover — non-text graphical component (3:1, SC 1.4.11) */
        { &theme->accent_hot,     &theme->surface_hot,    "accent_hot / surface_hot", AA_NONTEXT },
        /* border visibility — non-text UI components (3:1, SC 1.4.11) */
        { &theme->border,         &theme->bg,             "border / bg",           AA_NONTEXT },
        { &theme->border_focused, &theme->surface,        "border_focused / surface", AA_NONTEXT },
        /* scrollbar — non-text UI components (3:1) */
        { &theme->accent,         &theme->scrollbar_track, "accent / scrollbar_track", AA_NONTEXT },
        { &theme->accent_hot,     &theme->scrollbar_track, "accent_hot / scrollbar_track", AA_NONTEXT },
        { &theme->surface_hot,    &theme->scrollbar_track, "surface_hot / scrollbar_track", AA_NONTEXT },
        /* cursor — thin bar, requires text-level contrast (4.5:1, SC 1.4.3) */
        { &theme->cursor,         &theme->surface_active, "cursor / surface_active", AA_TEXT },
    };

    int count = (int)(sizeof(pairs) / sizeof(pairs[0]));
    for (int i = 0; i < count; i++) {
        float ratio = forge_ui_theme_contrast_ratio(
            pairs[i].fg->r, pairs[i].fg->g, pairs[i].fg->b,
            pairs[i].bg->r, pairs[i].bg->g, pairs[i].bg->b);
        if (ratio < pairs[i].min_ratio) {
            failures++;
        }
    }

    return failures;
}

#endif /* FORGE_UI_THEME_H */
