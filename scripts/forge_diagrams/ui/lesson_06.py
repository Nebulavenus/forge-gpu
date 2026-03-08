"""Diagrams for ui/06."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 06 — Checkboxes and Sliders
# ---------------------------------------------------------------------------


def diagram_checkbox_anatomy():
    """Four visual states side by side (normal, hot, active, checked) with
    labeled parts: outer box, inner fill, label text."""

    fig, axes = plt.subplots(1, 4, figsize=(14, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Checkbox Anatomy: Four Visual States",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    states = [
        ("Normal", False, STYLE["text_dim"], STYLE["surface"]),
        ("Hot (hovered)", False, STYLE["accent1"], STYLE["grid"]),
        ("Active (pressed)", False, STYLE["accent2"], STYLE["bg"]),
        ("Checked", True, STYLE["text_dim"], STYLE["surface"]),
    ]

    for ax, (title, checked, border_color, box_fill) in zip(axes, states, strict=True):
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-1, 3), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(title, color=border_color, fontsize=10, fontweight="bold", pad=8)

        # Outer box
        box = mpatches.FancyBboxPatch(
            (0.5, 0.5),
            1.8,
            1.8,
            boxstyle="round,pad=0.05",
            facecolor=box_fill,
            edgecolor=border_color,
            linewidth=2.0,
        )
        ax.add_patch(box)

        # Inner fill when checked
        if checked:
            inner = mpatches.FancyBboxPatch(
                (0.8, 0.8),
                1.2,
                1.2,
                boxstyle="round,pad=0.02",
                facecolor=STYLE["accent1"],
                edgecolor=STYLE["accent1"],
                linewidth=1.0,
            )
            ax.add_patch(inner)

        # Label text
        ax.text(
            3.0,
            1.4,
            "Label",
            color=STYLE["text"],
            fontsize=11,
            ha="left",
            va="center",
            family="monospace",
        )

    # Add part labels on the first and last panels
    ax0 = axes[0]
    ax0.annotate(
        "outer box\n(white_uv)",
        xy=(1.4, 0.5),
        xytext=(1.4, -0.5),
        arrowprops=dict(arrowstyle="->", color=STYLE["text_dim"], lw=1.0),
        color=STYLE["text_dim"],
        fontsize=7.5,
        ha="center",
        va="top",
    )
    ax0.annotate(
        "label text\n(glyph UVs)",
        xy=(3.0, 1.0),
        xytext=(3.0, -0.5),
        arrowprops=dict(arrowstyle="->", color=STYLE["text_dim"], lw=1.0),
        color=STYLE["text_dim"],
        fontsize=7.5,
        ha="center",
        va="top",
    )

    ax3 = axes[3]
    ax3.annotate(
        "inner fill\n(white_uv,\naccent color)",
        xy=(1.4, 0.8),
        xytext=(1.4, -0.7),
        arrowprops=dict(arrowstyle="->", color=STYLE["accent1"], lw=1.0),
        color=STYLE["accent1"],
        fontsize=7.5,
        ha="center",
        va="top",
    )

    fig.tight_layout(rect=[0, 0, 1, 0.92])
    save(fig, "ui/06-checkboxes-and-sliders", "checkbox_anatomy.png")


def diagram_checkbox_vs_button():
    """Side-by-side comparison of button and checkbox interaction logic,
    showing that checkboxes reuse the same state machine but toggle
    external state instead of returning a one-shot click event."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Button vs Checkbox: Same State Machine, Different Outcome",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    # ---- Left: Button ----
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 6), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Button", color=STYLE["accent2"], fontsize=11, fontweight="bold", pad=12
    )

    btn_steps = [
        ("Hit test\nmouse in rect?", STYLE["text_dim"], 5.5),
        ("Activate on\npress edge", STYLE["accent2"], 4.0),
        ("Click = release\nwhile over + active", STYLE["accent3"], 2.5),
        ("return true\n(one-shot event)", STYLE["warn"], 1.0),
    ]
    for label, color, y in btn_steps:
        rect = mpatches.FancyBboxPatch(
            (0.3, y - 0.4),
            5.4,
            0.8,
            boxstyle="round,pad=0.1",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.2,
        )
        ax.add_patch(rect)
        ax.text(
            3.0, y, label, color=STYLE["text"], fontsize=9, ha="center", va="center"
        )
    for i in range(len(btn_steps) - 1):
        y_from = btn_steps[i][2] - 0.4
        y_to = btn_steps[i + 1][2] + 0.4
        ax.annotate(
            "",
            xy=(3.0, y_to),
            xytext=(3.0, y_from),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=STYLE["text_dim"],
                lw=1.2,
            ),
        )

    # ---- Right: Checkbox ----
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 6), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Checkbox", color=STYLE["accent1"], fontsize=11, fontweight="bold", pad=12
    )

    cb_steps = [
        ("Hit test\nmouse in rect?", STYLE["text_dim"], 5.5),
        ("Activate on\npress edge", STYLE["accent1"], 4.0),
        ("Click = release\nwhile over + active", STYLE["accent3"], 2.5),
        ("*value = !*value\n(toggle external state)", STYLE["warn"], 1.0),
    ]
    for label, color, y in cb_steps:
        rect = mpatches.FancyBboxPatch(
            (0.3, y - 0.4),
            5.4,
            0.8,
            boxstyle="round,pad=0.1",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.2,
        )
        ax.add_patch(rect)
        ax.text(
            3.0, y, label, color=STYLE["text"], fontsize=9, ha="center", va="center"
        )
    for i in range(len(cb_steps) - 1):
        y_from = cb_steps[i][2] - 0.4
        y_to = cb_steps[i + 1][2] + 0.4
        ax.annotate(
            "",
            xy=(3.0, y_to),
            xytext=(3.0, y_from),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=STYLE["accent1"],
                lw=1.2,
            ),
        )

    # Highlight the difference
    for _ax_idx, (ax_ref, color, text) in enumerate(
        zip(
            axes,
            [STYLE["accent2"], STYLE["accent1"]],
            ["Returns bool", "Modifies *value"],
            strict=True,
        )
    ):
        ax_ref.text(
            3.0,
            0.0,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="top",
            style="italic",
        )

    fig.tight_layout(rect=[0, 0, 1, 0.92])
    save(fig, "ui/06-checkboxes-and-sliders", "checkbox_vs_button.png")


def diagram_slider_anatomy():
    """Labeled slider parts with dimension annotations: track, thumb,
    effective track range, thumb width insets."""

    fig, ax = plt.subplots(figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 13.5), ylim=(-1.5, 5), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Slider Anatomy",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Widget rect outline
    rx, ry, rw, rh = 1.0, 1.0, 9.0, 2.4
    rect_outline = mpatches.FancyBboxPatch(
        (rx, ry),
        rw,
        rh,
        boxstyle="round,pad=0.05",
        facecolor="none",
        edgecolor=STYLE["text_dim"],
        linewidth=1.0,
        linestyle="--",
    )
    ax.add_patch(rect_outline)
    ax.text(
        rx + rw + 0.3,
        ry + rh / 2,
        "widget rect\n(hit test area)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="left",
        va="center",
        style="italic",
    )

    # Track
    track_h = 0.3
    track_y = ry + (rh - track_h) / 2
    track = mpatches.FancyBboxPatch(
        (rx, track_y),
        rw,
        track_h,
        boxstyle="round,pad=0.02",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
    )
    ax.add_patch(track)

    # Thumb at ~40%
    thumb_w = 0.8
    thumb_h = 1.6
    t_val = 0.4
    eff_start = rx + thumb_w / 2
    eff_w = rw - thumb_w
    thumb_cx = eff_start + t_val * eff_w
    thumb_x = thumb_cx - thumb_w / 2
    thumb_y = ry + (rh - thumb_h) / 2

    thumb = mpatches.FancyBboxPatch(
        (thumb_x, thumb_y),
        thumb_w,
        thumb_h,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["accent1"] + "60",
        edgecolor=STYLE["accent1"],
        linewidth=2.0,
    )
    ax.add_patch(thumb)

    # Labels
    # Track label — placed to the right of the track to avoid overlapping
    # the effective-track annotation below
    ax.annotate(
        "track\n(thin rect, white_uv)",
        xy=(rx + rw, track_y + track_h / 2),
        xytext=(rx + rw + 0.3, track_y - 0.9),
        arrowprops=dict(arrowstyle="->", color=STYLE["text_dim"], lw=1.0),
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="left",
        va="top",
        fontweight="bold",
    )

    # Thumb label
    ax.annotate(
        "thumb\n(white_uv, color by state)",
        xy=(thumb_cx, thumb_y + thumb_h),
        xytext=(thumb_cx, thumb_y + thumb_h + 0.8),
        arrowprops=dict(arrowstyle="->", color=STYLE["accent1"], lw=1.0),
        color=STYLE["accent1"],
        fontsize=8.5,
        ha="center",
        va="bottom",
        fontweight="bold",
    )

    # Effective track range arrows
    arr_y = track_y - 0.4
    ax.annotate(
        "",
        xy=(eff_start, arr_y),
        xytext=(rx, arr_y),
        arrowprops=dict(arrowstyle="<->", color=STYLE["warn"], lw=1.2),
    )
    ax.text(
        (rx + eff_start) / 2,
        arr_y - 0.3,
        "w/2",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="top",
        fontweight="bold",
    )

    ax.annotate(
        "",
        xy=(rx + rw, arr_y),
        xytext=(eff_start + eff_w, arr_y),
        arrowprops=dict(arrowstyle="<->", color=STYLE["warn"], lw=1.2),
    )
    ax.text(
        (rx + rw + eff_start + eff_w) / 2,
        arr_y - 0.3,
        "w/2",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="top",
        fontweight="bold",
    )

    # Effective track range
    eff_arr_y = arr_y - 0.8
    ax.annotate(
        "",
        xy=(eff_start + eff_w, eff_arr_y),
        xytext=(eff_start, eff_arr_y),
        arrowprops=dict(arrowstyle="<->", color=STYLE["accent3"], lw=1.5),
    )
    ax.text(
        eff_start + eff_w / 2,
        eff_arr_y - 0.3,
        "effective track (track_w = rect.w - thumb_w)",
        color=STYLE["accent3"],
        fontsize=8.5,
        ha="center",
        va="top",
        fontweight="bold",
    )

    fig.tight_layout()
    save(fig, "ui/06-checkboxes-and-sliders", "slider_anatomy.png")


def diagram_slider_value_mapping():
    """Three stacked number lines showing bidirectional mapping:
    pixel position <-> normalized t <-> user value, with formulas."""

    fig, axes = plt.subplots(3, 1, figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Slider Value Mapping: Pixel <-> t <-> Value",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    line_x0, line_x1 = 1.0, 9.0
    mark_t = 0.4  # Example position at 40%
    mark_x = line_x0 + mark_t * (line_x1 - line_x0)

    labels = [
        (
            "Pixel Position (mouse_x)",
            "track_x",
            "track_x + track_w",
            "mouse_x",
            STYLE["accent2"],
        ),
        ("Normalized t  [0, 1]", "0.0", "1.0", f"t = {mark_t:.1f}", STYLE["accent1"]),
        (
            "User Value  [min, max]",
            "min (0)",
            "max (100)",
            f"value = {mark_t * 100:.0f}",
            STYLE["accent3"],
        ),
    ]

    for ax, (title, left_lbl, right_lbl, mark_lbl, color) in zip(
        axes, labels, strict=True
    ):
        setup_axes(ax, xlim=(0, 10.5), ylim=(-0.8, 1.5), grid=False, aspect=None)
        ax.axis("off")

        # Title
        ax.text(
            0.2, 1.2, title, color=color, fontsize=10, fontweight="bold", va="bottom"
        )

        # Number line
        ax.plot(
            [line_x0, line_x1],
            [0, 0],
            color=STYLE["text_dim"],
            lw=2,
            solid_capstyle="round",
        )

        # End ticks
        for x_pos in [line_x0, line_x1]:
            ax.plot([x_pos, x_pos], [-0.15, 0.15], color=STYLE["text_dim"], lw=1.5)

        # End labels
        ax.text(
            line_x0,
            -0.4,
            left_lbl,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
        )
        ax.text(
            line_x1,
            -0.4,
            right_lbl,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
        )

        # Mark position
        ax.plot(mark_x, 0, marker="o", markersize=10, color=color, zorder=5)
        ax.text(
            mark_x,
            0.4,
            mark_lbl,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="bottom",
        )

    # Formulas between the rows
    fig.text(
        0.82,
        0.68,
        "t = (mouse_x - track_x) / track_w",
        color=STYLE["warn"],
        fontsize=8,
        family="monospace",
        transform=fig.transFigure,
    )
    fig.text(
        0.82,
        0.38,
        "value = min + t * (max - min)",
        color=STYLE["warn"],
        fontsize=8,
        family="monospace",
        transform=fig.transFigure,
    )

    # Bidirectional arrows
    for y_frac in [0.68, 0.38]:
        fig.text(
            0.78,
            y_frac,
            "<->",
            color=STYLE["warn"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            transform=fig.transFigure,
        )

    fig.tight_layout(rect=[0, 0, 0.78, 0.93])
    save(fig, "ui/06-checkboxes-and-sliders", "slider_value_mapping.png")


def diagram_drag_outside_bounds():
    """Three-panel sequence showing active persisting outside the widget
    rect during a drag: click inside, drag outside, release outside."""

    fig, axes = plt.subplots(1, 3, figsize=(14, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Drag Outside Bounds: Active Persists During Drag",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    panels = [
        ("1. Click inside track", 0.4, True, True, STYLE["accent3"]),
        ("2. Drag outside (still held)", 1.3, True, False, STYLE["accent2"]),
        ("3. Release outside", 1.3, False, False, STYLE["text_dim"]),
    ]

    for ax, (title, cursor_t, mouse_down, _cursor_inside, title_color) in zip(
        axes, panels, strict=True
    ):
        setup_axes(ax, xlim=(-1, 8), ylim=(-1.5, 4), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(title, color=title_color, fontsize=10, fontweight="bold", pad=8)

        # Widget rect (dashed)
        wrx, wry, wrw, wrh = 0.5, 1.0, 5.5, 1.8
        wr = mpatches.FancyBboxPatch(
            (wrx, wry),
            wrw,
            wrh,
            boxstyle="round,pad=0.05",
            facecolor="none",
            edgecolor=STYLE["text_dim"],
            linewidth=1.0,
            linestyle="--",
        )
        ax.add_patch(wr)

        # Track
        tk_h = 0.25
        tk_y = wry + (wrh - tk_h) / 2
        tk = mpatches.FancyBboxPatch(
            (wrx, tk_y),
            wrw,
            tk_h,
            boxstyle="round,pad=0.02",
            facecolor=STYLE["surface"],
            edgecolor=STYLE["text_dim"],
            linewidth=1.0,
        )
        ax.add_patch(tk)

        # Thumb (clamped at max if cursor_t > 1)
        t_clamped = min(max(cursor_t, 0.0), 1.0)
        tw = 0.6
        eff_start = wrx + tw / 2
        eff_w = wrw - tw
        tcx = eff_start + t_clamped * eff_w
        txx = tcx - tw / 2
        th = 1.2
        ty = wry + (wrh - th) / 2

        is_active = mouse_down
        thumb_color = STYLE["accent1"] if is_active else STYLE["text_dim"]
        thumb = mpatches.FancyBboxPatch(
            (txx, ty),
            tw,
            th,
            boxstyle="round,pad=0.03",
            facecolor=thumb_color + "50",
            edgecolor=thumb_color,
            linewidth=2.0,
        )
        ax.add_patch(thumb)

        # Cursor
        if cursor_t <= 1.0:
            cx = eff_start + cursor_t * eff_w
        else:
            cx = wrx + wrw + (cursor_t - 1.0) * eff_w
        cy = wry + wrh / 2
        marker = "v" if mouse_down else "^"
        ax.plot(
            cx, cy + 1.3, marker=marker, markersize=12, color=STYLE["warn"], zorder=10
        )
        state_label = "ACTIVE" if is_active else "released"
        ax.text(
            cx,
            cy + 1.8,
            state_label,
            color=STYLE["warn"] if is_active else STYLE["text_dim"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="bottom",
        )

        # Value indicator
        val = t_clamped * 100
        ax.text(
            wrx + wrw / 2,
            wry - 0.4,
            f"value = {val:.0f}  (t = {t_clamped:.1f})",
            color=STYLE["accent1"] if is_active else STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
        )

    fig.tight_layout(rect=[0, 0, 1, 0.91])
    save(fig, "ui/06-checkboxes-and-sliders", "drag_outside_bounds.png")


def diagram_slider_state_colors():
    """Four slider renderings showing the thumb color for each state:
    normal, hot, active, and disabled (dimmed)."""

    fig, axes = plt.subplots(4, 1, figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Slider Thumb Colors by State",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    states = [
        ("Normal", 0.3, STYLE["text_dim"], STYLE["text_dim"]),
        ("Hot (hovered)", 0.3, STYLE["accent1"], STYLE["axis"]),
        ("Active (dragging)", 0.6, STYLE["accent1"], STYLE["accent1"]),
        ("Disabled", 0.3, STYLE["text_dim"], STYLE["grid"]),
    ]

    for ax, (label, t_pos, label_color, thumb_color) in zip(axes, states, strict=True):
        setup_axes(ax, xlim=(-0.5, 12), ylim=(-0.3, 1.3), grid=False, aspect=None)
        ax.axis("off")

        # State label
        ax.text(
            0.0,
            0.5,
            label,
            color=label_color,
            fontsize=10,
            fontweight="bold",
            va="center",
        )

        # Track
        trk_x, trk_w, trk_h = 3.0, 7.0, 0.2
        trk_y = 0.5 - trk_h / 2
        trk = mpatches.FancyBboxPatch(
            (trk_x, trk_y),
            trk_w,
            trk_h,
            boxstyle="round,pad=0.02",
            facecolor=STYLE["surface"],
            edgecolor=STYLE["text_dim"],
            linewidth=1.0,
        )
        ax.add_patch(trk)

        # Thumb
        tw, th = 0.5, 0.8
        eff_s = trk_x + tw / 2
        eff_w = trk_w - tw
        tcx = eff_s + t_pos * eff_w
        txx = tcx - tw / 2
        tyy = 0.5 - th / 2
        thumb = mpatches.FancyBboxPatch(
            (txx, tyy),
            tw,
            th,
            boxstyle="round,pad=0.03",
            facecolor=thumb_color + "60",
            edgecolor=thumb_color,
            linewidth=2.0,
        )
        ax.add_patch(thumb)

        # RGBA values
        if label == "Normal":
            ax.text(
                trk_x + trk_w + 0.3,
                0.5,
                "(0.50, 0.50, 0.58)",
                color=STYLE["text_dim"],
                fontsize=7.5,
                va="center",
                family="monospace",
            )
        elif label == "Hot (hovered)":
            ax.text(
                trk_x + trk_w + 0.3,
                0.5,
                "(0.60, 0.60, 0.72)",
                color=STYLE["text_dim"],
                fontsize=7.5,
                va="center",
                family="monospace",
            )
        elif label == "Active (dragging)":
            ax.text(
                trk_x + trk_w + 0.3,
                0.5,
                "(0.31, 0.76, 0.97)",
                color=STYLE["text_dim"],
                fontsize=7.5,
                va="center",
                family="monospace",
            )
        elif label == "Disabled":
            ax.text(
                trk_x + trk_w + 0.3,
                0.5,
                "(dimmed)",
                color=STYLE["text_dim"],
                fontsize=7.5,
                va="center",
                family="monospace",
            )

    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "ui/06-checkboxes-and-sliders", "slider_state_colors.png")


def diagram_widget_interaction_comparison():
    """Three-column summary comparing button, checkbox, and slider
    interaction patterns: hit test, state machine, outcome, draw elements."""

    fig, ax = plt.subplots(figsize=(12, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-0.5, 8), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Widget Interaction Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Column headers
    cols = [
        ("Button", 2.0, STYLE["accent2"]),
        ("Checkbox", 6.0, STYLE["accent1"]),
        ("Slider", 10.0, STYLE["accent3"]),
    ]
    for label, cx, color in cols:
        ax.text(
            cx,
            7.2,
            label,
            color=color,
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
        )
        # Underline
        ax.plot([cx - 1.5, cx + 1.5], [6.9, 6.9], color=color, lw=2)

    # Row labels
    rows = [
        ("Hit test", 6.0),
        ("Activation", 5.0),
        ("Outcome", 4.0),
        ("Active\nbehavior", 3.0),
        ("Draw\nelements", 1.8),
    ]
    for label, ry in rows:
        ax.text(
            -0.3,
            ry,
            label,
            color=STYLE["text_dim"],
            fontsize=9,
            fontweight="bold",
            ha="left",
            va="center",
        )

    # Grid data
    data = {
        6.0: [  # Hit test
            "point in rect",
            "point in rect\n(box + label)",
            "point in rect\n(track area)",
        ],
        5.0: [  # Activation
            "press edge\n+ hot",
            "press edge\n+ hot",
            "press edge\n+ hot",
        ],
        4.0: [  # Outcome
            "return true\non release",
            "*value = !*value\non release",
            "*value updates\ncontinuously",
        ],
        3.0: [  # Active behavior
            "visual feedback\nonly",
            "visual feedback\nonly",
            "drag: value\ntracks mouse_x",
        ],
        1.8: [  # Draw elements
            "background rect\n+ centered text",
            "outer box\n+ inner fill\n+ label text",
            "track rect\n+ thumb rect\n(+ value label)",
        ],
    }

    for ry, entries in data.items():
        for _i, (entry, (_, cx, color)) in enumerate(zip(entries, cols, strict=True)):
            bg = mpatches.FancyBboxPatch(
                (cx - 1.5, ry - 0.55),
                3.0,
                1.1,
                boxstyle="round,pad=0.05",
                facecolor=color + "12",
                edgecolor=color + "40",
                linewidth=0.8,
            )
            ax.add_patch(bg)
            ax.text(
                cx,
                ry,
                entry,
                color=STYLE["text"],
                fontsize=7.5,
                ha="center",
                va="center",
            )

    fig.tight_layout()
    save(fig, "ui/06-checkboxes-and-sliders", "widget_interaction_comparison.png")


# ---------------------------------------------------------------------------
# UI Lesson 07 — Text Input
# ---------------------------------------------------------------------------
