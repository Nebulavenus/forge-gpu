"""Diagrams for ui/13."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 13 — Theming and Color System
# ---------------------------------------------------------------------------

LESSON_13_PATH = "ui/13-theming-and-color-system"

# Shared hex palette for all Lesson 13 diagrams.  Each key is a theme slot
# name; the value is the canonical hex color for the default dark theme.


# Shared hex palette for all Lesson 13 diagrams.  Each key is a theme slot
# name; the value is the canonical hex color for the default dark theme.
THEME_SLOTS = {
    "bg": "#1a1a2e",
    "surface": "#252545",
    "surface_hot": "#3f3f55",
    "surface_active": "#181838",
    "title_bar": "#323250",
    "title_bar_text": "#e0e0f0",
    "text": "#e0e0f0",
    "text_dim": "#8888aa",
    "accent": "#4fc3f7",
    "accent_hot": "#5fd3ff",
    "border": "#2a2a4a",
    "border_focused": "#4fc3f7",
    "scrollbar_track": "#0d0d1b",
    "cursor": "#4fc3f7",
}


def diagram_theme_color_slots():
    """Palette card with 14 colored rects grouped into 4 categories:
    Backgrounds, Text, Accents, and Chrome."""

    fig, ax = plt.subplots(figsize=(14, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-0.5, 9), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.0,
        8.4,
        "ForgeUiTheme Color Slots",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Define color slots grouped by category (hex values from THEME_SLOTS)
    categories = [
        (
            "Backgrounds",
            STYLE["accent1"],
            [
                ("bg", THEME_SLOTS["bg"]),
                ("surface", THEME_SLOTS["surface"]),
                ("surface_hot", THEME_SLOTS["surface_hot"]),
                ("surface_active", THEME_SLOTS["surface_active"]),
                ("title_bar", THEME_SLOTS["title_bar"]),
            ],
        ),
        (
            "Text",
            STYLE["accent3"],
            [
                ("title_bar_text", THEME_SLOTS["title_bar_text"]),
                ("text", THEME_SLOTS["text"]),
                ("text_dim", THEME_SLOTS["text_dim"]),
            ],
        ),
        (
            "Accents",
            STYLE["accent2"],
            [
                ("accent", THEME_SLOTS["accent"]),
                ("accent_hot", THEME_SLOTS["accent_hot"]),
            ],
        ),
        (
            "Chrome",
            STYLE["accent4"],
            [
                ("border", THEME_SLOTS["border"]),
                ("border_focused", THEME_SLOTS["border_focused"]),
                ("scrollbar_track", THEME_SLOTS["scrollbar_track"]),
                ("cursor", THEME_SLOTS["cursor"]),
            ],
        ),
    ]

    swatch_w = 2.0
    swatch_h = 1.1
    gap_x = 0.4
    row_y_start = 6.8

    for cat_idx, (cat_name, cat_color, slots) in enumerate(categories):
        row_y = row_y_start - cat_idx * 1.85

        # Category label
        ax.text(
            0.0,
            row_y,
            cat_name,
            color=cat_color,
            fontsize=11,
            fontweight="bold",
            va="center",
            path_effects=stroke,
        )

        for slot_idx, (name, hexval) in enumerate(slots):
            x = 2.2 + slot_idx * (swatch_w + gap_x)
            y = row_y - swatch_h / 2

            # Color swatch
            rect = mpatches.FancyBboxPatch(
                (x, y - 0.15),
                swatch_w,
                swatch_h,
                boxstyle="round,pad=0.06",
                facecolor=hexval,
                edgecolor=cat_color,
                linewidth=1.5,
            )
            ax.add_patch(rect)

            # Decide text color based on WCAG relative luminance.
            # Linearize sRGB channels first (IEC 61966-2-1), matching the
            # math used by forge_ui_theme_relative_luminance() and the
            # contrast-ratio diagram so all luminance decisions are consistent.
            r_val = int(hexval[1:3], 16) / 255.0
            g_val = int(hexval[3:5], 16) / 255.0
            b_val = int(hexval[5:7], 16) / 255.0

            def _lin(c: float) -> float:
                return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

            lum = 0.2126 * _lin(r_val) + 0.7152 * _lin(g_val) + 0.0722 * _lin(b_val)
            txt_col = "#000000" if lum > 0.179 else "#ffffff"

            ax.text(
                x + swatch_w / 2,
                y + swatch_h / 2 + 0.05,
                name,
                color=txt_col,
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="center",
                family="monospace",
            )
            ax.text(
                x + swatch_w / 2,
                y + 0.1,
                hexval,
                color=txt_col,
                fontsize=7,
                ha="center",
                va="center",
                family="monospace",
            )

    fig.tight_layout()
    save(fig, LESSON_13_PATH, "theme_color_slots.png")


def diagram_wcag_contrast_formula():
    """Pipeline diagram: sRGB -> linearize -> luminance -> contrast ratio."""

    fig, ax = plt.subplots(figsize=(14, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14.5), ylim=(-0.5, 4), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.0,
        3.5,
        "WCAG Contrast Ratio Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    boxes = [
        ("sRGB color\n(r, g, b)", STYLE["accent1"]),
        ("Linearize\nC/12.92 or\n((C+0.055)/1.055)^2.4", STYLE["accent3"]),
        ("Luminance\n0.2126R + 0.7152G\n+ 0.0722B", STYLE["accent2"]),
        ("Contrast ratio\n(L\u2081+0.05) / (L\u2082+0.05)", STYLE["warn"]),
    ]

    box_w = 2.8
    box_h = 2.0
    gap = 0.6
    total_w = len(boxes) * box_w + (len(boxes) - 1) * gap
    x_start = (14.5 - total_w) / 2

    for i, (label, color) in enumerate(boxes):
        x = x_start + i * (box_w + gap)
        y = 0.8

        rect = mpatches.FancyBboxPatch(
            (x, y),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(rect)

        ax.text(
            x + box_w / 2,
            y + box_h / 2,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
        )

        # Arrow between boxes
        if i < len(boxes) - 1:
            ax.annotate(
                "",
                xy=(x + box_w + gap - 0.05, y + box_h / 2),
                xytext=(x + box_w + 0.05, y + box_h / 2),
                arrowprops={
                    "arrowstyle": "->,head_width=0.2,head_length=0.12",
                    "color": STYLE["text_dim"],
                    "lw": 2,
                },
            )

    # Step numbers
    for i in range(len(boxes)):
        x = x_start + i * (box_w + gap)
        ax.text(
            x + box_w / 2,
            0.45,
            f"Step {i + 1}",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            path_effects=stroke,
        )

    fig.tight_layout()
    save(fig, LESSON_13_PATH, "wcag_contrast_formula.png")


def diagram_contrast_ratio_scale():
    """Horizontal bar from 1:1 to 21:1 with annotated WCAG thresholds."""

    fig, ax = plt.subplots(figsize=(13, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(0, 22), ylim=(-1.5, 4.5), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        11.0,
        4.0,
        "WCAG Contrast Ratio Scale",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Gradient bar made of colored segments
    bar_y = 1.8
    bar_h = 0.8
    n_segments = 200
    for i in range(n_segments):
        ratio = 1.0 + (21.0 - 1.0) * i / (n_segments - 1)
        x = 1.0 + (ratio - 1.0) * (20.0 / 20.0)
        w = 20.0 / n_segments
        # Color ramp: dark red -> yellow -> green
        t = (ratio - 1.0) / 20.0
        if t < 0.175:
            # 1:1 to 4.5:1 — red to orange
            frac = t / 0.175
            r, g, b = 0.6 + 0.3 * frac, 0.15 + 0.55 * frac, 0.1
        elif t < 0.3:
            # 4.5:1 to 7:1 — orange to yellow
            frac = (t - 0.175) / 0.125
            r, g, b = 0.9, 0.7 + 0.2 * frac, 0.1 + 0.2 * frac
        else:
            # 7:1 to 21:1 — yellow to green
            frac = min(1.0, (t - 0.3) / 0.7)
            r, g, b = 0.9 - 0.5 * frac, 0.9, 0.3 + 0.3 * frac
        ax.add_patch(plt.Rectangle((x, bar_y), w + 0.02, bar_h, color=(r, g, b)))

    # Outline
    ax.add_patch(
        plt.Rectangle(
            (1.0, bar_y),
            20.0,
            bar_h,
            fill=False,
            edgecolor=STYLE["text_dim"],
            linewidth=1.5,
        )
    )

    # Threshold markers
    thresholds = [
        (3.0, "3:1", "Large text\nAA", STYLE["accent2"]),
        (4.5, "4.5:1", "Normal text\nAA", STYLE["warn"]),
        (7.0, "7:1", "AAA", STYLE["accent3"]),
    ]

    for ratio, label, desc, color in thresholds:
        x = 1.0 + (ratio - 1.0)
        # Vertical marker line
        ax.plot([x, x], [bar_y - 0.15, bar_y + bar_h + 0.15], color=color, lw=2.5)
        # Label above
        ax.text(
            x,
            bar_y + bar_h + 0.4,
            f"{label}\n{desc}",
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=stroke,
        )

    # FAIL and PASS zone labels
    ax.text(
        2.0,
        bar_y - 0.5,
        "FAIL",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        8.0,
        bar_y - 0.5,
        "PASS (AA)",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        14.0,
        bar_y - 0.5,
        "PASS (AAA)",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Scale labels at bottom
    for r in [1, 3, 5, 7, 10, 15, 21]:
        x = 1.0 + (r - 1.0)
        ax.text(
            x,
            bar_y - 0.95,
            f"{r}:1",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            path_effects=stroke,
        )

    fig.tight_layout()
    save(fig, LESSON_13_PATH, "contrast_ratio_scale.png")


def diagram_srgb_linearization_curve():
    """Plot of the sRGB transfer function (piecewise) vs pure gamma 2.2."""

    fig, ax = plt.subplots(figsize=(8, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.02, 1.05), ylim=(-0.02, 1.05), grid=True, aspect=None)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.set_xlabel("sRGB value", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("Linear value", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "sRGB Linearization Curve",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    x = np.linspace(0, 1, 500)

    # sRGB piecewise linearization
    srgb_linear = np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)

    # Pure gamma 2.2 for comparison
    gamma_22 = x**2.2

    ax.plot(
        x,
        srgb_linear,
        color=STYLE["accent1"],
        lw=2.5,
        label="sRGB (piecewise)",
    )
    ax.plot(
        x,
        gamma_22,
        color=STYLE["accent2"],
        lw=2,
        linestyle="--",
        label="Pure gamma 2.2",
    )
    ax.plot(
        x,
        x,
        color=STYLE["text_dim"],
        lw=1,
        linestyle=":",
        alpha=0.5,
        label="Linear",
    )

    # Annotate the threshold
    threshold_x = 0.04045
    threshold_y = threshold_x / 12.92
    ax.plot(threshold_x, threshold_y, "o", color=STYLE["warn"], markersize=8, zorder=5)
    ax.annotate(
        f"Threshold\n({threshold_x}, {threshold_y:.4f})",
        xy=(threshold_x, threshold_y),
        xytext=(0.25, 0.15),
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
        path_effects=stroke,
    )

    # Annotate the two segments
    ax.text(
        0.02,
        0.06,
        "C / 12.92",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        rotation=15,
        path_effects=stroke,
    )
    ax.text(
        0.55,
        0.2,
        "((C + 0.055) / 1.055)^2.4",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        path_effects=stroke,
    )

    legend = ax.legend(
        loc="upper left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    legend.get_frame().set_alpha(0.9)

    fig.tight_layout()
    save(fig, LESSON_13_PATH, "srgb_linearization_curve.png")


def diagram_theme_slot_mapping():
    """Arrows from theme slot names (left) to widget elements (right)."""

    fig, ax = plt.subplots(figsize=(12, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(-0.5, 9), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        6.0,
        8.5,
        "Theme Slot to Widget Mapping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Theme slots (left column)
    slots = [
        ("surface", STYLE["accent1"]),
        ("surface_hot", STYLE["accent1"]),
        ("surface_active", STYLE["accent1"]),
        ("accent", STYLE["accent2"]),
        ("accent_hot", STYLE["accent2"]),
        ("text", STYLE["accent3"]),
        ("text_dim", STYLE["accent3"]),
        ("border", STYLE["accent4"]),
        ("border_focused", STYLE["accent4"]),
    ]

    # Widget elements (right column)
    widgets = [
        ("Button bg", STYLE["accent1"]),
        ("Button hover", STYLE["accent1"]),
        ("Button pressed", STYLE["accent1"]),
        ("Checkbox fill", STYLE["accent2"]),
        ("Slider knob hover", STYLE["accent2"]),
        ("Label text", STYLE["accent3"]),
        ("Hint / placeholder", STYLE["accent3"]),
        ("Panel border", STYLE["accent4"]),
        ("Focused input border", STYLE["accent4"]),
    ]

    # Connections: (slot_index, widget_index)
    connections = [
        (0, 0),
        (1, 1),
        (2, 2),
        (3, 3),
        (4, 4),
        (5, 5),
        (6, 6),
        (7, 7),
        (8, 8),
    ]

    left_x = 0.2
    right_x = 8.5
    box_w = 3.0
    box_h = 0.65
    slot_y_start = 7.5

    # Draw slot boxes (left)
    for i, (name, color) in enumerate(slots):
        y = slot_y_start - i * 0.85
        rect = mpatches.FancyBboxPatch(
            (left_x, y - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(rect)
        ax.text(
            left_x + box_w / 2,
            y,
            name,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
        )

    # Draw widget boxes (right)
    for i, (name, color) in enumerate(widgets):
        y = slot_y_start - i * 0.85
        rect = mpatches.FancyBboxPatch(
            (right_x, y - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(rect)
        ax.text(
            right_x + box_w / 2,
            y,
            name,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Draw connecting arrows
    for slot_idx, widget_idx in connections:
        slot_y = slot_y_start - slot_idx * 0.85
        widget_y = slot_y_start - widget_idx * 0.85
        color = slots[slot_idx][1]

        ax.annotate(
            "",
            xy=(right_x - 0.05, widget_y),
            xytext=(left_x + box_w + 0.05, slot_y),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.1",
                "color": color,
                "lw": 1.2,
                "alpha": 0.6,
                "connectionstyle": "arc3,rad=0.0",
            },
        )

    # Column headers
    ax.text(
        left_x + box_w / 2,
        8.2,
        "Theme Slots",
        color=STYLE["text_dim"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        right_x + box_w / 2,
        8.2,
        "Widget Elements",
        color=STYLE["text_dim"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, LESSON_13_PATH, "theme_slot_mapping.png")


def diagram_adjacent_pair_matrix():
    """Grid of tested fg/bg pairs with contrast ratios and pass/fail status."""

    fig, ax = plt.subplots(figsize=(12, 10))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-1, 19), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        6.0,
        18.2,
        "Adjacent Pair Contrast Matrix",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # sRGB linearization helper
    def srgb_to_linear(c):
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    def hex_to_luminance(hexval):
        r = srgb_to_linear(int(hexval[1:3], 16) / 255.0)
        g = srgb_to_linear(int(hexval[3:5], 16) / 255.0)
        b = srgb_to_linear(int(hexval[5:7], 16) / 255.0)
        return 0.2126 * r + 0.7152 * g + 0.0722 * b

    def contrast_ratio(hex1, hex2):
        l1 = hex_to_luminance(hex1)
        l2 = hex_to_luminance(hex2)
        lighter = max(l1, l2)
        darker = min(l1, l2)
        return (lighter + 0.05) / (darker + 0.05)

    # Define the 17 tested pairs (hex values from THEME_SLOTS)
    S = THEME_SLOTS
    pairs = [
        ("text", S["text"], "bg", S["bg"]),
        ("text", S["text"], "surface", S["surface"]),
        ("text_dim", S["text_dim"], "bg", S["bg"]),
        ("text_dim", S["text_dim"], "surface", S["surface"]),
        ("accent", S["accent"], "bg", S["bg"]),
        ("accent", S["accent"], "surface", S["surface"]),
        ("accent_hot", S["accent_hot"], "bg", S["bg"]),
        ("accent_hot", S["accent_hot"], "surface", S["surface"]),
        ("title_bar_text", S["title_bar_text"], "title_bar", S["title_bar"]),
        ("text", S["text"], "surface_hot", S["surface_hot"]),
        ("text", S["text"], "surface_active", S["surface_active"]),
        ("accent", S["accent"], "surface_hot", S["surface_hot"]),
        ("accent", S["accent"], "surface_active", S["surface_active"]),
        ("text_dim", S["text_dim"], "surface_hot", S["surface_hot"]),
        ("cursor", S["cursor"], "bg", S["bg"]),
        ("text", S["text"], "scrollbar_track", S["scrollbar_track"]),
        ("border_focused", S["border_focused"], "surface", S["surface"]),
    ]

    # Non-text UI component pairs use the WCAG 3:1 threshold instead of 4.5:1.
    # These are borders, scrollbar elements, and accent indicators that are not
    # used to render readable text.
    _NON_TEXT_PAIRS = {
        ("accent_hot", "surface_hot"),
        ("border", "bg"),
        ("border_focused", "surface"),
        ("accent", "scrollbar_track"),
        ("accent_hot", "scrollbar_track"),
        ("surface_hot", "scrollbar_track"),
    }

    # Column positions
    col_x = [0.5, 3.0, 5.5, 8.5, 10.5]
    headers = ["Foreground", "Background", "Hex pair", "Ratio", "Status"]
    row_h = 0.85
    header_y = 17.2

    # Draw headers
    for j, hdr in enumerate(headers):
        ax.text(
            col_x[j],
            header_y,
            hdr,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            va="center",
            path_effects=stroke,
        )

    # Separator line
    ax.plot(
        [-0.2, 11.8],
        [header_y - 0.45, header_y - 0.45],
        color=STYLE["grid"],
        lw=1,
    )

    for i, (fg_name, fg_hex, bg_name, bg_hex) in enumerate(pairs):
        y = header_y - 1.0 - i * row_h
        ratio = contrast_ratio(fg_hex, bg_hex)
        is_non_text = (fg_name, bg_name) in _NON_TEXT_PAIRS
        threshold = 3.0 if is_non_text else 4.5
        passes = ratio >= threshold
        status_text = "PASS" if passes else "FAIL"
        status_color = STYLE["accent3"] if passes else STYLE["accent2"]

        # Row background (alternating)
        if i % 2 == 0:
            row_bg = mpatches.FancyBboxPatch(
                (-0.2, y - row_h / 2 + 0.05),
                12.0,
                row_h - 0.1,
                boxstyle="round,pad=0.03",
                facecolor=STYLE["surface"],
                edgecolor="none",
                alpha=0.3,
            )
            ax.add_patch(row_bg)

        # Foreground color name + swatch
        ax.add_patch(plt.Rectangle((col_x[0] - 0.3, y - 0.15), 0.25, 0.3, color=fg_hex))
        ax.text(
            col_x[0] + 0.1,
            y,
            fg_name,
            color=STYLE["text"],
            fontsize=8,
            va="center",
            family="monospace",
            path_effects=stroke,
        )

        # Background color name + swatch
        ax.add_patch(plt.Rectangle((col_x[1] - 0.3, y - 0.15), 0.25, 0.3, color=bg_hex))
        ax.text(
            col_x[1] + 0.1,
            y,
            bg_name,
            color=STYLE["text"],
            fontsize=8,
            va="center",
            family="monospace",
            path_effects=stroke,
        )

        # Hex pair
        ax.text(
            col_x[2],
            y,
            f"{fg_hex} / {bg_hex}",
            color=STYLE["text_dim"],
            fontsize=7,
            va="center",
            family="monospace",
            path_effects=stroke,
        )

        # Ratio
        ax.text(
            col_x[3],
            y,
            f"{ratio:.1f}:1",
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            va="center",
            family="monospace",
            path_effects=stroke,
        )

        # Status
        ax.text(
            col_x[4],
            y,
            status_text,
            color=status_color,
            fontsize=9,
            fontweight="bold",
            va="center",
            path_effects=stroke,
        )

    fig.tight_layout()
    save(fig, LESSON_13_PATH, "adjacent_pair_matrix.png")


def diagram_before_after_theming():
    """Split view: scattered #define constants (left) vs centralized
    ForgeUiTheme struct (right), connected by a 'Centralize' arrow."""

    fig, (ax_left, ax_right) = plt.subplots(1, 2, figsize=(14, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Left side: scattered defines ---
    ax_left.set_title(
        "Before (scattered #defines)",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    setup_axes(ax_left, xlim=(0, 8), ylim=(0, 10), grid=False, aspect=None)
    ax_left.axis("off")

    defines = [
        "#define BTN_NORMAL_R  0x25",
        "#define BTN_NORMAL_G  0x25",
        "#define BTN_NORMAL_B  0x45",
        "#define BTN_HOT_R     0x3f",
        "#define CB_CHECK_R    0x4f",
        "#define CB_CHECK_G    0xc3",
        "#define CB_CHECK_B    0xf7",
        "#define SLIDER_BG_R   0x1a",
        "#define LABEL_TEXT_R  0xe0",
        "#define LABEL_TEXT_G  0xe0",
        "#define PANEL_BG_R    0x1a",
        "#define PANEL_BG_G    0x1a",
        "#define BORDER_R      0x2a",
        "#define BORDER_G      0x2a",
        "#define TITLE_R       0x32",
    ]

    for i, d in enumerate(defines):
        y = 9.2 - i * 0.58
        ax_left.text(
            0.5,
            y,
            d,
            color=STYLE["accent2"] if i % 3 == 0 else STYLE["text_dim"],
            fontsize=7.5,
            va="center",
            family="monospace",
            path_effects=stroke,
        )

    # Scattered warning markers
    for y in [8.6, 6.3, 4.0, 2.2]:
        ax_left.text(
            7.2,
            y,
            "\u26a0",
            color=STYLE["warn"],
            fontsize=12,
            ha="center",
            va="center",
        )

    ax_left.text(
        4.0,
        0.3,
        "45+ separate defines",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Right side: ForgeUiTheme struct ---
    ax_right.set_title(
        "After (ForgeUiTheme)",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    setup_axes(ax_right, xlim=(0, 8), ylim=(0, 10), grid=False, aspect=None)
    ax_right.axis("off")

    # Struct background
    struct_bg = mpatches.FancyBboxPatch(
        (0.3, 0.8),
        7.4,
        8.6,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
    )
    ax_right.add_patch(struct_bg)

    struct_lines = [
        ("typedef struct {", STYLE["accent3"]),
        ("    ForgeUiColor bg;", STYLE["accent1"]),
        ("    ForgeUiColor surface;", STYLE["accent1"]),
        ("    ForgeUiColor surface_hot;", STYLE["accent1"]),
        ("    ForgeUiColor surface_active;", STYLE["accent1"]),
        ("    ForgeUiColor title_bar;", STYLE["accent1"]),
        ("    ForgeUiColor title_bar_text;", STYLE["accent3"]),
        ("    ForgeUiColor text;", STYLE["accent3"]),
        ("    ForgeUiColor text_dim;", STYLE["accent3"]),
        ("    ForgeUiColor accent;", STYLE["accent2"]),
        ("    ForgeUiColor accent_hot;", STYLE["accent2"]),
        ("    ForgeUiColor border;", STYLE["accent4"]),
        ("    ForgeUiColor border_focused;", STYLE["accent4"]),
        ("    ForgeUiColor scrollbar_track;", STYLE["accent4"]),
        ("    ForgeUiColor cursor;", STYLE["accent4"]),
        ("} ForgeUiTheme;", STYLE["accent3"]),
    ]

    for i, (line, color) in enumerate(struct_lines):
        y = 9.0 - i * 0.52
        ax_right.text(
            0.8,
            y,
            line,
            color=color,
            fontsize=7.5,
            va="center",
            family="monospace",
            path_effects=stroke,
        )

    ax_right.text(
        4.0,
        0.3,
        "1 struct, 14 named slots",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Centralize arrow between subplots (use fig.text and fig annotation)
    fig.text(
        0.5,
        0.5,
        "Centralize\n\u2192",
        color=STYLE["warn"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout(w_pad=3.0)
    save(fig, LESSON_13_PATH, "before_after_theming.png")
