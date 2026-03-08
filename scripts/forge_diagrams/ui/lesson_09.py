"""Diagrams for ui/09."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 09 — Panels and Scrolling
# ---------------------------------------------------------------------------


def diagram_panel_anatomy():
    """Show the anatomy of a panel: outer rect, title bar, content area,
    padding, and scrollbar track on the right edge."""

    fig, ax = plt.subplots(figsize=(8, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10), ylim=(-0.5, 8.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Panel Anatomy",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Panel outer rect
    px, py, pw, ph = 1.0, 0.5, 7.0, 7.0
    outer = mpatches.FancyBboxPatch(
        (px, py),
        pw,
        ph,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax.add_patch(outer)

    # Title bar
    title_h = 1.0
    title_rect = mpatches.FancyBboxPatch(
        (px, py + ph - title_h),
        pw,
        title_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent4"],
        edgecolor=STYLE["accent4"],
        alpha=0.6,
    )
    ax.add_patch(title_rect)
    ax.text(
        px + pw / 2,
        py + ph - title_h / 2,
        "Title Bar",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Content area (inside padding)
    pad = 0.4
    content_x = px + pad
    content_y = py + pad
    content_w = pw - 2 * pad - 0.6  # leave room for scrollbar
    content_h = ph - title_h - 2 * pad
    content_rect = mpatches.FancyBboxPatch(
        (content_x, content_y),
        content_w,
        content_h,
        boxstyle="round,pad=0.02",
        facecolor="none",
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(content_rect)
    ax.text(
        content_x + content_w / 2,
        content_y + content_h / 2,
        "Content Area\n(clip rect)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Scrollbar track
    sb_w = 0.4
    sb_x = px + pw - pad - sb_w
    sb_y = content_y
    sb_h = content_h
    sb_rect = mpatches.FancyBboxPatch(
        (sb_x, sb_y),
        sb_w,
        sb_h,
        boxstyle="round,pad=0.02",
        facecolor=STYLE["grid"],
        edgecolor=STYLE["text_dim"],
        linewidth=1,
    )
    ax.add_patch(sb_rect)

    # Scrollbar thumb
    thumb_h = sb_h * 0.35
    thumb_y = sb_y + sb_h - thumb_h - 0.2
    thumb_rect = mpatches.FancyBboxPatch(
        (sb_x + 0.05, thumb_y),
        sb_w - 0.1,
        thumb_h,
        boxstyle="round,pad=0.02",
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        alpha=0.8,
    )
    ax.add_patch(thumb_rect)

    # Labels with arrows
    annotations = [
        (
            "rect (outer bounds)",
            (px - 0.3, py + ph / 2),
            (px, py + ph / 2),
            STYLE["accent1"],
        ),
        (
            "padding",
            (px + pad / 2, py + pad / 2 + 0.3),
            (px + pad / 2, py + pad / 2),
            STYLE["warn"],
        ),
        (
            "scrollbar track",
            (sb_x + sb_w + 0.8, sb_y + sb_h / 2),
            (sb_x + sb_w, sb_y + sb_h / 2),
            STYLE["text_dim"],
        ),
        (
            "thumb",
            (sb_x + sb_w + 0.8, thumb_y + thumb_h / 2),
            (sb_x + sb_w, thumb_y + thumb_h / 2),
            STYLE["accent1"],
        ),
    ]
    for label, txt_pos, arrow_pos, color in annotations:
        ax.annotate(
            label,
            xy=arrow_pos,
            xytext=txt_pos,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            arrowprops={"arrowstyle": "->", "color": color, "lw": 1.5},
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    save(fig, "ui/09-panels-and-scrolling", "panel_anatomy.png")


def diagram_clip_rect_operation():
    """Show axis-aligned rect clipping: a widget quad partially inside the
    clip rect, with the clipped portion highlighted and the outside discarded."""

    fig, axes = plt.subplots(1, 3, figsize=(12, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Clip Rect Operation",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    titles = [
        "Fully Inside (keep)",
        "Partially Inside (clip)",
        "Fully Outside (discard)",
    ]

    for ax, title in zip(axes, titles, strict=True):
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 6.5), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(title, color=STYLE["text"], fontsize=10, fontweight="bold", pad=8)

        # Clip rect (dashed green)
        clip = mpatches.Rectangle(
            (1, 1),
            5,
            4,
            fill=False,
            edgecolor=STYLE["accent3"],
            linewidth=2,
            linestyle="--",
        )
        ax.add_patch(clip)
        ax.text(
            3.5,
            5.3,
            "clip_rect",
            color=STYLE["accent3"],
            fontsize=8,
            ha="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Case 1: fully inside
    r1 = mpatches.Rectangle(
        (2, 2),
        3,
        2,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        alpha=0.5,
        linewidth=2,
    )
    axes[0].add_patch(r1)
    axes[0].text(
        3.5,
        3,
        "widget",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Case 2: partially inside — original extends right
    r2_orig = mpatches.Rectangle(
        (3, 1.5),
        5,
        3,
        fill=False,
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
        linestyle=":",
    )
    axes[1].add_patch(r2_orig)
    r2_clip = mpatches.Rectangle(
        (3, 1.5),
        3,
        3,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        alpha=0.5,
        linewidth=2,
    )
    axes[1].add_patch(r2_clip)
    # Hatched region for discarded part
    r2_discard = mpatches.Rectangle(
        (6, 1.5),
        2,
        3,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        alpha=0.15,
        linewidth=1,
    )
    axes[1].add_patch(r2_discard)
    axes[1].text(
        4.5,
        3,
        "kept",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    axes[1].text(
        7,
        3,
        "trimmed",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Case 3: fully outside
    r3 = mpatches.Rectangle(
        (6.2, 1.5),
        1.5,
        2,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        alpha=0.3,
        linewidth=2,
    )
    axes[2].add_patch(r3)
    axes[2].text(
        7,
        2.5,
        "discarded",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout(rect=[0, 0, 1, 0.92])
    save(fig, "ui/09-panels-and-scrolling", "clip_rect_operation.png")


def diagram_scroll_offset_model():
    """Show how scroll_y offsets widget positions: widgets declared at logical
    y are rendered at (y - scroll_y), with the clip rect hiding overflow."""

    fig, axes = plt.subplots(1, 2, figsize=(10, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Scroll Offset Model",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    labels = ["scroll_y = 0  (top)", "scroll_y = 60  (scrolled down)"]
    scroll_offsets = [0, 60]
    widget_labels = [
        "Checkbox A",
        "Checkbox B",
        "Checkbox C",
        "Checkbox D",
        "Checkbox E",
    ]

    for ax, label, scroll_y in zip(axes, labels, scroll_offsets, strict=True):
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 8), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(label, color=STYLE["text"], fontsize=10, fontweight="bold", pad=8)

        # Panel content area (clip rect)
        clip_x, clip_y, clip_w, clip_h = 0.5, 1, 6, 5.5
        clip = mpatches.Rectangle(
            (clip_x, clip_y),
            clip_w,
            clip_h,
            fill=False,
            edgecolor=STYLE["accent3"],
            linewidth=2,
            linestyle="--",
        )
        ax.add_patch(clip)

        # Widgets at logical positions — offset by scroll
        widget_h = 0.8
        spacing = 0.3
        logical_y_start = 7.0  # top-down from the top

        for i, w_label in enumerate(widget_labels):
            logical_y = logical_y_start - i * (widget_h + spacing)
            rendered_y = logical_y + scroll_y * 0.03  # scale scroll for diagram

            # Check if fully inside clip rect
            inside = rendered_y >= clip_y and (rendered_y + widget_h) <= (
                clip_y + clip_h
            )
            alpha = 0.6 if inside else 0.15
            color = STYLE["accent1"] if inside else STYLE["text_dim"]

            r = mpatches.Rectangle(
                (clip_x + 0.2, rendered_y),
                clip_w - 0.4,
                widget_h,
                facecolor=color,
                edgecolor=color,
                alpha=alpha,
                linewidth=1,
            )
            ax.add_patch(r)
            if inside:
                ax.text(
                    clip_x + clip_w / 2,
                    rendered_y + widget_h / 2,
                    w_label,
                    color=STYLE["text"],
                    fontsize=8,
                    ha="center",
                    va="center",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )

        # Scroll indicator arrow
        if scroll_y > 0:
            ax.annotate(
                "",
                xy=(clip_x + clip_w + 0.5, clip_y + clip_h),
                xytext=(clip_x + clip_w + 0.5, clip_y + clip_h - 1.5),
                arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 2},
            )
            ax.text(
                clip_x + clip_w + 0.7,
                clip_y + clip_h - 0.75,
                f"scroll_y\n= {scroll_y}",
                color=STYLE["warn"],
                fontsize=8,
                ha="left",
                va="center",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

    fig.tight_layout(rect=[0, 0, 1, 0.92])
    save(fig, "ui/09-panels-and-scrolling", "scroll_offset_model.png")


def diagram_scrollbar_proportions():
    """Show the proportional relationship between content height, visible
    height, and scrollbar thumb size with labeled formulas."""

    fig, ax = plt.subplots(figsize=(9, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-0.5, 8), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Scrollbar Proportions",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Content column (tall)
    content_x, content_y = 1, 0.5
    content_w, content_h = 3, 7
    cr = mpatches.Rectangle(
        (content_x, content_y),
        content_w,
        content_h,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(cr)
    ax.text(
        content_x + content_w / 2,
        content_y + content_h + 0.2,
        "content_height",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Visible window (subset)
    visible_h = 3.5
    visible_y = content_y + content_h - visible_h - 1
    vr = mpatches.Rectangle(
        (content_x, visible_y),
        content_w,
        visible_h,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        alpha=0.2,
        linewidth=2,
    )
    ax.add_patch(vr)
    ax.text(
        content_x + content_w / 2,
        visible_y + visible_h / 2,
        "visible_h",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Scrollbar track
    track_x = 6
    track_w = 0.6
    track_h = 5
    track_y = 1
    tr = mpatches.Rectangle(
        (track_x, track_y),
        track_w,
        track_h,
        facecolor=STYLE["grid"],
        edgecolor=STYLE["text_dim"],
        linewidth=1,
    )
    ax.add_patch(tr)
    ax.text(
        track_x + track_w / 2,
        track_y + track_h + 0.2,
        "track_h",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Thumb (proportional)
    thumb_ratio = visible_h / content_h
    thumb_h = track_h * thumb_ratio
    thumb_y = track_y + track_h - thumb_h - 0.8
    thr = mpatches.FancyBboxPatch(
        (track_x + 0.05, thumb_y),
        track_w - 0.1,
        thumb_h,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        alpha=0.8,
    )
    ax.add_patch(thr)
    ax.text(
        track_x + track_w + 0.3,
        thumb_y + thumb_h / 2,
        "thumb_h",
        color=STYLE["accent1"],
        fontsize=9,
        ha="left",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Formulas
    formulas = [
        "thumb_h = track_h * visible_h / content_h",
        "thumb_y = track_y + (1 - scroll_y / max_scroll) * (track_h - thumb_h)",
        "max_scroll = content_h - visible_h",
    ]
    for i, formula in enumerate(formulas):
        ax.text(
            8.5,
            6.5 - i * 0.8,
            formula,
            color=STYLE["warn"],
            fontsize=9,
            family="monospace",
            ha="left",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Connecting arrow from visible to thumb
    ax.annotate(
        "",
        xy=(track_x, thumb_y + thumb_h / 2),
        xytext=(content_x + content_w, visible_y + visible_h / 2),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
            "linestyle": "--",
        },
    )

    save(fig, "ui/09-panels-and-scrolling", "scrollbar_proportions.png")


def diagram_uv_remap_on_clip():
    """Show UV coordinate remapping when a glyph quad is clipped: the
    proportional formula maps the clipped position to clipped UVs."""

    fig, axes = plt.subplots(1, 2, figsize=(11, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "UV Remapping on Clip",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    # Left: screen space (clipped quad)
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Screen Space", color=STYLE["text"], fontsize=10, fontweight="bold", pad=8
    )

    # Clip boundary
    clip_x = 3
    ax.axvline(x=clip_x, color=STYLE["accent3"], linewidth=2, linestyle="--")
    ax.text(
        clip_x,
        6,
        "clip edge",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Original quad (extends past clip edge)
    orig_x0, orig_x1 = 1, 5
    orig_y0, orig_y1 = 1.5, 4.5
    orig = mpatches.Rectangle(
        (orig_x0, orig_y0),
        orig_x1 - orig_x0,
        orig_y1 - orig_y0,
        fill=False,
        edgecolor=STYLE["text_dim"],
        linewidth=1,
        linestyle=":",
    )
    ax.add_patch(orig)

    # Kept portion
    kept = mpatches.Rectangle(
        (orig_x0, orig_y0),
        clip_x - orig_x0,
        orig_y1 - orig_y0,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        alpha=0.4,
        linewidth=2,
    )
    ax.add_patch(kept)

    # Labels
    ax.text(
        orig_x0,
        orig_y0 - 0.3,
        f"x0={orig_x0}",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        orig_x1,
        orig_y0 - 0.3,
        f"x1={orig_x1}",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        clip_x,
        orig_y0 - 0.3,
        f"clip={clip_x}",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Right: UV space
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "UV Space (texture)", color=STYLE["text"], fontsize=10, fontweight="bold", pad=8
    )

    # Original UV quad
    uv_x0, uv_x1 = 1, 6
    uv_y0, uv_y1 = 1.5, 4.5
    orig_uv = mpatches.Rectangle(
        (uv_x0, uv_y0),
        uv_x1 - uv_x0,
        uv_y1 - uv_y0,
        fill=False,
        edgecolor=STYLE["text_dim"],
        linewidth=1,
        linestyle=":",
    )
    ax.add_patch(orig_uv)

    # Clipped UV: proportional remap
    t = (clip_x - orig_x0) / (orig_x1 - orig_x0)
    clipped_u1 = uv_x0 + t * (uv_x1 - uv_x0)
    kept_uv = mpatches.Rectangle(
        (uv_x0, uv_y0),
        clipped_u1 - uv_x0,
        uv_y1 - uv_y0,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        alpha=0.4,
        linewidth=2,
    )
    ax.add_patch(kept_uv)

    ax.text(
        uv_x0,
        uv_y0 - 0.3,
        "u0",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        uv_x1,
        uv_y0 - 0.3,
        "u1",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        clipped_u1,
        uv_y0 - 0.3,
        f"u'={clipped_u1:.1f}",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Formula
    fig.text(
        0.5,
        0.04,
        "clipped_u = u0 + (u1 - u0) * (clipped_x - x0) / (x1 - x0)",
        color=STYLE["warn"],
        fontsize=10,
        family="monospace",
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout(rect=[0, 0.1, 1, 0.92])
    save(fig, "ui/09-panels-and-scrolling", "uv_remap_on_clip.png")


def diagram_panel_with_scroll_sequence():
    """Four-panel sequence showing a panel at different scroll positions:
    top, middle, bottom, and scrollbar thumb positions."""

    fig, axes = plt.subplots(1, 4, figsize=(14, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Panel Scroll Sequence",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    scroll_values = [0.0, 0.3, 0.7, 1.0]
    step_labels = ["scroll_y = 0", "scroll_y = 30%", "scroll_y = 70%", "scroll_y = max"]
    num_widgets = 8

    for ax, scroll_frac, label in zip(axes, scroll_values, step_labels, strict=True):
        setup_axes(ax, xlim=(-0.2, 5), ylim=(-0.2, 7), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(label, color=STYLE["text"], fontsize=9, fontweight="bold", pad=6)

        # Panel frame
        px, py, pw, ph = 0.2, 0.2, 4, 6
        panel = mpatches.Rectangle(
            (px, py),
            pw,
            ph,
            facecolor=STYLE["surface"],
            edgecolor=STYLE["accent1"],
            linewidth=1.5,
        )
        ax.add_patch(panel)

        # Title bar
        th = 0.7
        title = mpatches.Rectangle(
            (px, py + ph - th),
            pw,
            th,
            facecolor=STYLE["accent4"],
            alpha=0.5,
        )
        ax.add_patch(title)

        # Content area bounds
        content_top = py + ph - th - 0.15
        content_bottom = py + 0.15
        content_h = content_top - content_bottom

        # Widgets
        widget_h = 0.55
        widget_spacing = 0.15
        total_content = num_widgets * (widget_h + widget_spacing) - widget_spacing
        max_scroll = total_content - content_h
        if max_scroll < 0:
            max_scroll = 0
        scroll_y = scroll_frac * max_scroll

        for i in range(num_widgets):
            logical_y = content_top - i * (widget_h + widget_spacing) - widget_h
            rendered_y = logical_y + scroll_y

            # Clip check
            if rendered_y + widget_h < content_bottom or rendered_y > content_top:
                continue

            # Clip to bounds
            draw_y = max(rendered_y, content_bottom)
            draw_top = min(rendered_y + widget_h, content_top)
            draw_h = draw_top - draw_y

            alpha = 0.5 if draw_h > widget_h * 0.5 else 0.25
            wr = mpatches.Rectangle(
                (px + 0.2, draw_y),
                pw - 0.8,
                draw_h,
                facecolor=STYLE["accent1"],
                alpha=alpha,
            )
            ax.add_patch(wr)
            if draw_h > widget_h * 0.3:
                ax.text(
                    px + pw / 2 - 0.2,
                    draw_y + draw_h / 2,
                    f"W{i}",
                    color=STYLE["text"],
                    fontsize=7,
                    ha="center",
                    va="center",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )

        # Scrollbar track and thumb
        sb_x = px + pw - 0.35
        sb_w = 0.2
        track = mpatches.Rectangle(
            (sb_x, content_bottom),
            sb_w,
            content_h,
            facecolor=STYLE["grid"],
            alpha=0.5,
        )
        ax.add_patch(track)

        if max_scroll > 0:
            thumb_frac = content_h / total_content
            thumb_h = content_h * thumb_frac
            if thumb_h < 0.3:
                thumb_h = 0.3
            thumb_range = content_h - thumb_h
            thumb_y = content_top - thumb_h - scroll_frac * thumb_range
            thumb = mpatches.FancyBboxPatch(
                (sb_x + 0.02, thumb_y),
                sb_w - 0.04,
                thumb_h,
                boxstyle="round,pad=0.02",
                facecolor=STYLE["accent1"],
                alpha=0.8,
            )
            ax.add_patch(thumb)

    fig.tight_layout(rect=[0, 0, 1, 0.92])
    save(fig, "ui/09-panels-and-scrolling", "panel_with_scroll_sequence.png")


def diagram_mouse_wheel_and_drag():
    """Side-by-side comparison of the two scrolling input methods: mouse wheel
    (adds delta to scroll_y) and scrollbar thumb drag (maps position to
    scroll_y)."""

    fig, axes = plt.subplots(1, 2, figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Scrolling Input Methods",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    # Left: Mouse Wheel
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 7), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Mouse Wheel", color=STYLE["accent1"], fontsize=11, fontweight="bold", pad=8
    )

    # Panel
    px, py, pw, ph = 1, 0.5, 5, 5
    panel = mpatches.Rectangle(
        (px, py),
        pw,
        ph,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(panel)

    # Mouse cursor with wheel indicator
    mouse_x, mouse_y = 3.5, 3.5
    ax.plot(mouse_x, mouse_y, marker="v", markersize=14, color=STYLE["warn"], zorder=5)
    ax.text(
        mouse_x + 0.5,
        mouse_y,
        "scroll_delta",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Scroll direction arrow (content moves up when scrolling down)
    ax.annotate(
        "",
        xy=(mouse_x, mouse_y + 1.5),
        xytext=(mouse_x, mouse_y + 0.3),
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 2.5},
    )
    ax.text(
        mouse_x - 1,
        mouse_y + 1.0,
        "content\nmoves up",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Formula
    ax.text(
        px + pw / 2,
        py - 0.6,
        "scroll_y += delta * SCROLL_SPEED",
        color=STYLE["warn"],
        fontsize=8,
        family="monospace",
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Right: Scrollbar Drag
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 7), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Scrollbar Thumb Drag",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )

    # Panel
    panel2 = mpatches.Rectangle(
        (px, py),
        pw,
        ph,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
    )
    ax.add_patch(panel2)

    # Scrollbar track
    sb_x = px + pw - 0.5
    sb_w = 0.35
    sb_h = ph - 0.4
    sb_y = py + 0.2
    track = mpatches.Rectangle(
        (sb_x, sb_y),
        sb_w,
        sb_h,
        facecolor=STYLE["grid"],
        alpha=0.5,
    )
    ax.add_patch(track)

    # Thumb
    thumb_h = sb_h * 0.3
    thumb_y = sb_y + sb_h - thumb_h - 1
    thumb = mpatches.FancyBboxPatch(
        (sb_x + 0.03, thumb_y),
        sb_w - 0.06,
        thumb_h,
        boxstyle="round,pad=0.02",
        facecolor=STYLE["accent2"],
        alpha=0.8,
    )
    ax.add_patch(thumb)

    # Drag arrow
    ax.annotate(
        "",
        xy=(sb_x + sb_w / 2, thumb_y - 1),
        xytext=(sb_x + sb_w / 2, thumb_y + thumb_h / 2),
        arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 2.5},
    )
    ax.text(
        sb_x - 1,
        thumb_y - 0.3,
        "drag",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Active persists label
    ax.text(
        px + 1,
        py + ph + 0.3,
        "active persists outside bounds",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Formula
    ax.text(
        px + pw / 2,
        py - 0.6,
        "scroll_y = (track_y + track_h - thumb_h - thumb_y) / (track_h - thumb_h) * max_scroll",
        color=STYLE["warn"],
        fontsize=7,
        family="monospace",
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout(rect=[0, 0.08, 1, 0.92])
    save(fig, "ui/09-panels-and-scrolling", "mouse_wheel_and_drag.png")


# ---------------------------------------------------------------------------
# UI Lesson 10 — Windows
# ---------------------------------------------------------------------------
