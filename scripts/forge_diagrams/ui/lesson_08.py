"""Diagrams for ui/08."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 08 — Layout
# ---------------------------------------------------------------------------


def diagram_layout_cursor_model():
    """Vertical layout region with cursor advancing downward after each
    widget, showing padding inset and spacing gaps with dimension annotations."""

    fig, ax = plt.subplots(figsize=(7, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1, 12), ylim=(-0.5, 10.5), grid=False, aspect=None)
    ax.axis("off")

    # Layout rect
    lx, ly, lw, lh = 1.0, 0.5, 8.0, 9.0
    padding = 0.8
    spacing = 0.4
    widget_heights = [1.2, 1.2, 1.2, 1.4]
    labels = ["Label", "Checkbox", "Checkbox", "Slider"]
    colors = [STYLE["accent1"], STYLE["accent3"], STYLE["accent3"], STYLE["accent2"]]

    # Outer layout rect
    outer = mpatches.FancyBboxPatch(
        (lx, ly),
        lw,
        lh,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["surface"] + "60",
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(outer)
    ax.text(
        lx + lw / 2,
        ly + lh + 0.3,
        "ForgeUiLayout (vertical)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Draw widgets from top down
    inner_x = lx + padding
    inner_w = lw - 2 * padding
    cursor_y = ly + lh - padding  # start from top
    cursor_positions = [cursor_y]

    for _i, (h, label, color) in enumerate(
        zip(widget_heights, labels, colors, strict=True)
    ):
        wy = cursor_y - h
        wrect = mpatches.FancyBboxPatch(
            (inner_x, wy),
            inner_w,
            h,
            boxstyle="round,pad=0.03",
            facecolor=color + "40",
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(wrect)
        ax.text(
            inner_x + inner_w / 2,
            wy + h / 2,
            label,
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        cursor_y = wy - spacing
        cursor_positions.append(cursor_y)

    # Padding annotations (left side)
    ax.annotate(
        "",
        xy=(lx, ly + lh - 0.05),
        xytext=(inner_x, ly + lh - 0.05),
        arrowprops=dict(arrowstyle="<->", color=STYLE["warn"], lw=1.5),
    )
    ax.text(
        (lx + inner_x) / 2,
        ly + lh + 0.1,
        "padding",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Spacing annotation between widget 0 and 1
    w0_bottom = cursor_positions[0] - widget_heights[0]
    w1_top = w0_bottom - spacing
    mid_spacing = (w0_bottom + w1_top) / 2
    ax.annotate(
        "",
        xy=(inner_x + inner_w + 0.3, w0_bottom),
        xytext=(inner_x + inner_w + 0.3, w1_top),
        arrowprops=dict(arrowstyle="<->", color=STYLE["accent4"], lw=1.5),
    )
    ax.text(
        inner_x + inner_w + 0.5,
        mid_spacing,
        "spacing",
        color=STYLE["accent4"],
        fontsize=8,
        ha="left",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Cursor arrow on the left showing downward progression
    for _i, cp in enumerate(cursor_positions[:-1]):
        ax.plot(
            lx - 0.3, cp, marker=">", color=STYLE["accent1"], markersize=6, zorder=5
        )
    ax.text(
        lx - 0.7,
        cursor_positions[0],
        "cursor",
        color=STYLE["accent1"],
        fontsize=8,
        ha="right",
        va="center",
        rotation=90,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    # Vertical line showing cursor path
    ax.plot(
        [lx - 0.3, lx - 0.3],
        [cursor_positions[0], cursor_positions[-2]],
        color=STYLE["accent1"],
        linewidth=1,
        linestyle=":",
        alpha=0.6,
    )

    fig.tight_layout()
    save(fig, "ui/08-layout", "layout_cursor_model.png")


def diagram_horizontal_vs_vertical():
    """Side-by-side comparison of the same four widgets laid out horizontally
    vs vertically, showing cursor direction and consumed-space regions."""

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])

    labels = ["A", "B", "C", "D"]
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"], STYLE["accent4"]]

    for ax, title, direction in [(ax1, "VERTICAL", "v"), (ax2, "HORIZONTAL", "h")]:
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 8), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(title, color=STYLE["text"], fontsize=13, fontweight="bold", pad=12)

        lx, ly, lw, lh = 0.5, 0.5, 6.5, 6.5
        pad = 0.4
        sp = 0.3

        # Outer rect
        outer = mpatches.FancyBboxPatch(
            (lx, ly),
            lw,
            lh,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["surface"] + "40",
            edgecolor=STYLE["axis"],
            linewidth=1.0,
            linestyle="--",
        )
        ax.add_patch(outer)

        inner_x = lx + pad
        inner_y = ly + pad
        inner_w = lw - 2 * pad
        inner_h = lh - 2 * pad

        if direction == "v":
            # Vertical: full width, varying heights
            heights = [1.2, 1.0, 1.0, 1.4]
            cy = ly + lh - pad
            for _i, (h, label, color) in enumerate(
                zip(heights, labels, colors, strict=True)
            ):
                wy = cy - h
                r = mpatches.FancyBboxPatch(
                    (inner_x, wy),
                    inner_w,
                    h,
                    boxstyle="round,pad=0.03",
                    facecolor=color + "40",
                    edgecolor=color,
                    linewidth=1.5,
                )
                ax.add_patch(r)
                ax.text(
                    inner_x + inner_w / 2,
                    wy + h / 2,
                    label,
                    color=STYLE["text"],
                    fontsize=11,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )
                cy = wy - sp
            # Arrow showing cursor direction
            ax.annotate(
                "",
                xy=(lx - 0.15, ly + pad + 0.5),
                xytext=(lx - 0.15, ly + lh - pad - 0.2),
                arrowprops=dict(arrowstyle="->", color=STYLE["warn"], lw=2.0),
            )
            ax.text(
                lx - 0.3,
                ly + lh / 2,
                "cursor",
                color=STYLE["warn"],
                fontsize=8,
                ha="right",
                va="center",
                rotation=90,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )
        else:
            # Horizontal: full height, varying widths
            widths = [1.2, 1.0, 1.0, 1.4]
            cx = inner_x
            for _i, (w, label, color) in enumerate(
                zip(widths, labels, colors, strict=True)
            ):
                r = mpatches.FancyBboxPatch(
                    (cx, inner_y),
                    w,
                    inner_h,
                    boxstyle="round,pad=0.03",
                    facecolor=color + "40",
                    edgecolor=color,
                    linewidth=1.5,
                )
                ax.add_patch(r)
                ax.text(
                    cx + w / 2,
                    inner_y + inner_h / 2,
                    label,
                    color=STYLE["text"],
                    fontsize=11,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )
                cx += w + sp
            # Arrow showing cursor direction
            ax.annotate(
                "",
                xy=(lx + lw - pad - 0.5, ly - 0.15),
                xytext=(lx + pad + 0.2, ly - 0.15),
                arrowprops=dict(arrowstyle="->", color=STYLE["warn"], lw=2.0),
            )
            ax.text(
                lx + lw / 2,
                ly - 0.3,
                "cursor",
                color=STYLE["warn"],
                fontsize=8,
                ha="center",
                va="top",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

    fig.tight_layout()
    save(fig, "ui/08-layout", "horizontal_vs_vertical.png")


def diagram_nested_layout():
    """Settings panel example with vertical outer layout containing a
    horizontal inner row, color-coded to show layout ownership."""

    fig, ax = plt.subplots(figsize=(8, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1, 12), ylim=(-1, 11), grid=False, aspect=None)
    ax.axis("off")

    # Outer vertical layout
    ox, oy, ow, oh = 1, 0.5, 8, 9.5
    pad = 0.6
    sp = 0.35
    vert_color = STYLE["accent1"]
    horiz_color = STYLE["accent2"]

    outer = mpatches.FancyBboxPatch(
        (ox, oy),
        ow,
        oh,
        boxstyle="round,pad=0.08",
        facecolor=vert_color + "15",
        edgecolor=vert_color,
        linewidth=2.0,
    )
    ax.add_patch(outer)

    # Title
    ax.text(
        ox + ow / 2,
        oy + oh + 0.3,
        "push(vertical)     ...     pop()",
        color=vert_color,
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    inner_x = ox + pad
    inner_w = ow - 2 * pad

    # Widgets from top
    cy = oy + oh - pad
    widget_data = [
        ("Settings (label)", 1.0, STYLE["accent3"]),
        ("V-Sync (checkbox)", 0.9, STYLE["accent3"]),
        ("Fullscreen (checkbox)", 0.9, STYLE["accent3"]),
        ("Anti-aliasing (checkbox)", 0.9, STYLE["accent3"]),
    ]

    for label, h, color in widget_data:
        wy = cy - h
        r = mpatches.FancyBboxPatch(
            (inner_x, wy),
            inner_w,
            h,
            boxstyle="round,pad=0.03",
            facecolor=color + "30",
            edgecolor=color,
            linewidth=1.0,
        )
        ax.add_patch(r)
        ax.text(
            inner_x + inner_w / 2,
            wy + h / 2,
            label,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )
        cy = wy - sp

    # Horizontal button row
    row_h = 1.1
    row_y = cy - row_h
    row_rect = mpatches.FancyBboxPatch(
        (inner_x, row_y),
        inner_w,
        row_h,
        boxstyle="round,pad=0.05",
        facecolor=horiz_color + "20",
        edgecolor=horiz_color,
        linewidth=2.0,
        linestyle="--",
    )
    ax.add_patch(row_rect)

    # Two buttons inside the horizontal layout
    btn_sp = 0.3
    btn_w = (inner_w - btn_sp) / 2
    btn_pad = 0.0
    for i, label in enumerate(["OK", "Cancel"]):
        bx = inner_x + i * (btn_w + btn_sp)
        r = mpatches.FancyBboxPatch(
            (bx, row_y + btn_pad),
            btn_w,
            row_h - 2 * btn_pad,
            boxstyle="round,pad=0.03",
            facecolor=STYLE["accent4"] + "40",
            edgecolor=STYLE["accent4"],
            linewidth=1.0,
        )
        ax.add_patch(r)
        ax.text(
            bx + btn_w / 2,
            row_y + row_h / 2,
            label,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # push/pop annotation for horizontal row
    ax.text(
        inner_x + inner_w + 0.3,
        row_y + row_h,
        "push(horiz)",
        color=horiz_color,
        fontsize=8,
        ha="left",
        va="top",
        fontstyle="italic",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        inner_x + inner_w + 0.3,
        row_y,
        "pop()",
        color=horiz_color,
        fontsize=8,
        ha="left",
        va="bottom",
        fontstyle="italic",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    cy = row_y - sp

    # Slider
    sl_h = 1.0
    sl_y = cy - sl_h
    r = mpatches.FancyBboxPatch(
        (inner_x, sl_y),
        inner_w,
        sl_h,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["accent3"] + "30",
        edgecolor=STYLE["accent3"],
        linewidth=1.0,
    )
    ax.add_patch(r)
    ax.text(
        inner_x + inner_w / 2,
        sl_y + sl_h / 2,
        "Volume (slider)",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Legend
    legend_items = [
        (vert_color, "Vertical layout"),
        (horiz_color, "Horizontal layout"),
        (STYLE["accent3"], "Widgets"),
        (STYLE["accent4"], "Buttons"),
    ]
    for i, (color, label) in enumerate(legend_items):
        ax.plot(ox - 0.5, oy + oh - 0.5 - i * 0.5, "s", color=color, markersize=8)
        ax.text(
            ox - 0.2,
            oy + oh - 0.5 - i * 0.5,
            label,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="left",
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    fig.tight_layout()
    save(fig, "ui/08-layout", "nested_layout.png")


def diagram_layout_stack_visualization():
    """The layout stack as a vertical array showing push adding a frame
    and pop removing it, with the current top highlighted."""

    fig, axes = plt.subplots(1, 3, figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])

    states = [
        ("After push(vertical)", ["Vertical\n(panel)"], 1),
        ("After push(horizontal)", ["Vertical\n(panel)", "Horizontal\n(buttons)"], 2),
        ("After pop()", ["Vertical\n(panel)"], 1),
    ]

    max_slots = 4  # Show 4 slots of the stack

    for ax, (title, entries, depth) in zip(axes, states, strict=True):
        setup_axes(ax, xlim=(-0.5, 4), ylim=(-0.5, 5.5), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(title, color=STYLE["text"], fontsize=10, fontweight="bold", pad=12)

        slot_w = 3.0
        slot_h = 0.9
        slot_x = 0.5
        base_y = 0.5

        for i in range(max_slots):
            sy = base_y + i * (slot_h + 0.15)
            is_active = i < len(entries)
            is_top = i == len(entries) - 1

            if is_active:
                color = STYLE["accent1"] if is_top else STYLE["accent3"]
                fc = color + "40"
                ec = color
                lw = 2.0 if is_top else 1.0
            else:
                fc = STYLE["surface"] + "20"
                ec = STYLE["axis"] + "40"
                lw = 0.5

            r = mpatches.FancyBboxPatch(
                (slot_x, sy),
                slot_w,
                slot_h,
                boxstyle="round,pad=0.03",
                facecolor=fc,
                edgecolor=ec,
                linewidth=lw,
            )
            ax.add_patch(r)

            # Index label
            ax.text(
                slot_x - 0.15,
                sy + slot_h / 2,
                f"[{i}]",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="right",
                va="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

            if is_active:
                ax.text(
                    slot_x + slot_w / 2,
                    sy + slot_h / 2,
                    entries[i],
                    color=STYLE["text"],
                    fontsize=9,
                    ha="center",
                    va="center",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )
                if is_top:
                    ax.text(
                        slot_x + slot_w + 0.15,
                        sy + slot_h / 2,
                        "<- top",
                        color=STYLE["warn"],
                        fontsize=8,
                        ha="left",
                        va="center",
                        fontweight="bold",
                        path_effects=[
                            pe.withStroke(linewidth=3, foreground=STYLE["bg"])
                        ],
                    )
            else:
                ax.text(
                    slot_x + slot_w / 2,
                    sy + slot_h / 2,
                    "(empty)",
                    color=STYLE["text_dim"] + "60",
                    fontsize=8,
                    ha="center",
                    va="center",
                    fontstyle="italic",
                    path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                )

        # Depth label
        ax.text(
            slot_x + slot_w / 2,
            base_y - 0.3,
            f"depth = {depth}",
            color=STYLE["accent1"],
            fontsize=9,
            ha="center",
            va="top",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    fig.tight_layout()
    save(fig, "ui/08-layout", "layout_stack_visualization.png")


def diagram_padding_and_spacing():
    """Zoomed diagram distinguishing padding from all edges vs spacing
    between widgets, with labeled measurements."""

    fig, ax = plt.subplots(figsize=(8, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1, 12), ylim=(-0.5, 9), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Padding vs Spacing",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Layout rect
    lx, ly, lw, lh = 1, 0.5, 8, 7.5
    pad = 1.0
    sp = 0.6

    # Outer rect
    outer = mpatches.FancyBboxPatch(
        (lx, ly),
        lw,
        lh,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["surface"] + "30",
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(outer)

    # Padding region (shaded)
    # Top padding
    ax.fill_between([lx, lx + lw], ly + lh - pad, ly + lh, color=STYLE["warn"] + "18")
    # Bottom padding
    ax.fill_between([lx, lx + lw], ly, ly + pad, color=STYLE["warn"] + "18")
    # Left padding
    ax.fill_between([lx, lx + pad], ly, ly + lh, color=STYLE["warn"] + "18")
    # Right padding
    ax.fill_between([lx + lw - pad, lx + lw], ly, ly + lh, color=STYLE["warn"] + "18")

    inner_x = lx + pad
    inner_w = lw - 2 * pad
    widget_h = 1.3

    # Three widgets
    cy = ly + lh - pad
    widget_colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]
    widget_labels = ["Widget A", "Widget B", "Widget C"]

    rects = []
    for i in range(3):
        wy = cy - widget_h
        r = mpatches.FancyBboxPatch(
            (inner_x, wy),
            inner_w,
            widget_h,
            boxstyle="round,pad=0.03",
            facecolor=widget_colors[i] + "40",
            edgecolor=widget_colors[i],
            linewidth=1.5,
        )
        ax.add_patch(r)
        ax.text(
            inner_x + inner_w / 2,
            wy + widget_h / 2,
            widget_labels[i],
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )
        rects.append((inner_x, wy, inner_w, widget_h))

        if i < 2:
            # Spacing region
            gap_top = wy
            gap_bottom = wy - sp
            ax.fill_between(
                [inner_x, inner_x + inner_w],
                gap_bottom,
                gap_top,
                color=STYLE["accent4"] + "25",
            )
        cy = wy - sp

    # Padding dimension annotations
    # Top padding
    ax.annotate(
        "",
        xy=(lx + lw / 2, ly + lh),
        xytext=(lx + lw / 2, ly + lh - pad),
        arrowprops=dict(arrowstyle="<->", color=STYLE["warn"], lw=2),
    )
    ax.text(
        lx + lw / 2 + 0.3,
        ly + lh - pad / 2,
        "padding",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Left padding
    ax.annotate(
        "",
        xy=(lx, ly + 0.3),
        xytext=(lx + pad, ly + 0.3),
        arrowprops=dict(arrowstyle="<->", color=STYLE["warn"], lw=2),
    )
    ax.text(
        lx + pad / 2,
        ly + 0.05,
        "padding",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Spacing annotation
    r0 = rects[0]
    r1 = rects[1]
    gap_mid_x = inner_x + inner_w + 0.3

    # Spacing between widget A bottom and widget B top
    ax.annotate(
        "",
        xy=(gap_mid_x + 1.0, r1[1] + r1[3]),
        xytext=(gap_mid_x + 1.0, r0[1]),
        arrowprops=dict(arrowstyle="<->", color=STYLE["accent4"], lw=2),
    )
    ax.text(
        gap_mid_x + 1.2,
        (r0[1] + r1[1] + r1[3]) / 2,
        "spacing",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "ui/08-layout", "padding_and_spacing.png")


def diagram_layout_next_sequence():
    """Step-by-step four-panel sequence showing layout_next called four
    times in a vertical layout -- each panel shows the returned rect
    highlighted and the cursor position before and after."""

    fig, axes = plt.subplots(1, 4, figsize=(12, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])

    widget_heights = [1.0, 0.8, 0.8, 1.0]
    widget_labels = ["A", "B", "C", "D"]
    widget_colors = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
        STYLE["accent4"],
    ]

    lx, ly, lw, lh = 0.3, 0.3, 3.0, 5.5
    pad = 0.3
    sp = 0.25

    for step, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 4.5), ylim=(-0.3, 6.5), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(
            f"layout_next({step + 1})",
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            pad=12,
        )

        # Outer rect
        outer = mpatches.FancyBboxPatch(
            (lx, ly),
            lw,
            lh,
            boxstyle="round,pad=0.03",
            facecolor=STYLE["surface"] + "25",
            edgecolor=STYLE["axis"],
            linewidth=0.8,
            linestyle="--",
        )
        ax.add_patch(outer)

        inner_x = lx + pad
        inner_w = lw - 2 * pad
        cy = ly + lh - pad

        for i in range(4):
            h = widget_heights[i]
            wy = cy - h

            if i <= step:
                is_current = i == step
                alpha = "70" if is_current else "25"
                lw_rect = 2.0 if is_current else 0.8
                fc = widget_colors[i] + alpha
                ec = widget_colors[i]
            else:
                fc = STYLE["surface"] + "10"
                ec = STYLE["axis"] + "30"
                lw_rect = 0.5

            r = mpatches.FancyBboxPatch(
                (inner_x, wy),
                inner_w,
                h,
                boxstyle="round,pad=0.02",
                facecolor=fc,
                edgecolor=ec,
                linewidth=lw_rect,
            )
            ax.add_patch(r)

            if i <= step:
                ax.text(
                    inner_x + inner_w / 2,
                    wy + h / 2,
                    widget_labels[i],
                    color=STYLE["text"],
                    fontsize=10,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )

            cy = wy - sp

        # Cursor indicator — show position AFTER this step's widget
        # First widget has no leading spacing; subsequent widgets have sp before them
        cursor_after_y = ly + lh - pad
        for i in range(step + 1):
            if i > 0:
                cursor_after_y -= sp
            cursor_after_y -= widget_heights[i]

        ax.plot(
            [lx - 0.15, lx + 0.05],
            [cursor_after_y, cursor_after_y],
            color=STYLE["warn"],
            linewidth=2.5,
            solid_capstyle="round",
        )
        ax.text(
            lx - 0.2,
            cursor_after_y,
            "^",
            color=STYLE["warn"],
            fontsize=7,
            ha="right",
            va="center",
            fontfamily="monospace",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    fig.tight_layout()
    save(fig, "ui/08-layout", "layout_next_sequence.png")


# ---------------------------------------------------------------------------
# UI Lesson 09 — Panels and Scrolling
# ---------------------------------------------------------------------------
