"""Diagrams for ui/10."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 10 — Windows
# ---------------------------------------------------------------------------


def diagram_window_anatomy():
    """Labeled parts of a window: title bar with collapse button, drag handle
    region, content area with clip rect, scrollbar, resize grip placeholder."""

    fig, ax = plt.subplots(figsize=(10, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-0.5, 9.5), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Window outer frame
    wx, wy, ww, wh = 1.5, 0.5, 7.0, 8.0
    win = mpatches.FancyBboxPatch(
        (wx, wy),
        ww,
        wh,
        boxstyle="round,pad=0.08",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax.add_patch(win)

    # Title bar
    th = 1.0
    title_rect = mpatches.FancyBboxPatch(
        (wx + 0.05, wy + wh - th - 0.05),
        ww - 0.1,
        th,
        boxstyle="round,pad=0.04",
        facecolor=STYLE["accent1"] + "40",
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(title_rect)

    # Collapse toggle triangle (down-pointing = expanded state)
    tri_x = wx + 0.4
    tri_cy = wy + wh - th / 2
    tri = plt.Polygon(
        [
            [tri_x, tri_cy + 0.2],
            [tri_x + 0.4, tri_cy + 0.2],
            [tri_x + 0.2, tri_cy - 0.2],
        ],
        closed=True,
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
    )
    ax.add_patch(tri)

    # Down-pointing variant (shown as label)
    ax.annotate(
        "collapse\ntoggle",
        xy=(tri_x + 0.15, tri_cy),
        xytext=(tri_x - 1.5, tri_cy + 1.2),
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        arrowprops=dict(arrowstyle="->", color=STYLE["accent3"], lw=1.5),
        path_effects=stroke,
    )

    # Title text
    ax.text(
        wx + 1.2,
        wy + wh - th / 2,
        "Window Title",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        va="center",
        path_effects=stroke,
    )

    # Drag handle label
    ax.annotate(
        "drag handle\n(title bar)",
        xy=(wx + ww / 2, wy + wh - th / 2),
        xytext=(wx + ww + 1.5, wy + wh - 0.2),
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        arrowprops=dict(arrowstyle="->", color=STYLE["accent1"], lw=1.5),
        path_effects=stroke,
    )

    # Content area
    pad = 0.3
    sbw = 0.4
    cx, cy = wx + pad, wy + pad
    cw, ch = ww - 2 * pad - sbw, wh - th - 2 * pad
    content = mpatches.Rectangle(
        (cx, cy),
        cw,
        ch,
        facecolor=STYLE["bg"] + "80",
        edgecolor=STYLE["accent2"],
        linewidth=1,
        linestyle="--",
    )
    ax.add_patch(content)
    ax.text(
        cx + cw / 2,
        cy + ch / 2,
        "content area\n(clipped)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Clip rect label
    ax.annotate(
        "clip rect",
        xy=(cx, cy + ch / 2),
        xytext=(cx - 1.5, cy + ch / 2),
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        arrowprops=dict(arrowstyle="->", color=STYLE["accent2"], lw=1.5),
        path_effects=stroke,
    )

    # Scrollbar
    sx = wx + ww - pad - sbw
    scrollbar = mpatches.Rectangle(
        (sx, cy),
        sbw,
        ch,
        facecolor=STYLE["bg"] + "60",
        edgecolor=STYLE["text_dim"],
        linewidth=1,
    )
    ax.add_patch(scrollbar)

    # Scrollbar thumb
    thumb_h = ch * 0.3
    thumb = mpatches.FancyBboxPatch(
        (sx + 0.05, cy + ch - thumb_h - 0.3),
        sbw - 0.1,
        thumb_h,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["accent1"] + "80",
        edgecolor=STYLE["accent1"],
        linewidth=1,
    )
    ax.add_patch(thumb)

    ax.annotate(
        "scrollbar",
        xy=(sx + sbw / 2, cy + ch / 2),
        xytext=(sx + sbw + 1.5, cy + ch / 2 - 0.5),
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        arrowprops=dict(arrowstyle="->", color=STYLE["text_dim"], lw=1.5),
        path_effects=stroke,
    )

    # Resize grip placeholder (grayed out, future work)
    grip_sz = 0.5
    grip = mpatches.Rectangle(
        (wx + ww - grip_sz - 0.1, wy + 0.1),
        grip_sz,
        grip_sz,
        facecolor=STYLE["text_dim"] + "20",
        edgecolor=STYLE["text_dim"] + "50",
        linewidth=1,
        linestyle=":",
    )
    ax.add_patch(grip)
    ax.text(
        wx + ww + 0.3,
        wy + 0.35,
        "resize grip\n(future work)",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        path_effects=stroke,
    )

    ax.set_title(
        "Window Anatomy",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "ui/10-windows", "window_anatomy.png")


def diagram_z_order_model():
    """Three overlapping windows shown from the side in an exploded isometric
    view with z values labeled, arrow showing click-to-front promotion."""

    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1, 12), ylim=(-1, 8), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    colors = [STYLE["accent2"], STYLE["accent3"], STYLE["accent1"]]
    labels = ["Window A\nz = 0", "Window B\nz = 1", "Window C\nz = 2"]

    # Draw three stacked rectangles in pseudo-3D
    for i in range(3):
        ox = 1.0 + i * 1.2
        oy = 0.5 + i * 1.8
        w, h = 4.0, 2.5
        rect = mpatches.FancyBboxPatch(
            (ox, oy),
            w,
            h,
            boxstyle="round,pad=0.06",
            facecolor=colors[i] + "35",
            edgecolor=colors[i],
            linewidth=2,
        )
        ax.add_patch(rect)
        ax.text(
            ox + w / 2,
            oy + h / 2,
            labels[i],
            color=colors[i],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Arrow showing click-to-front promotion
    ax.annotate(
        "",
        xy=(3.0, 5.6),
        xytext=(2.0, 1.2),
        arrowprops=dict(
            arrowstyle="-|>",
            color=STYLE["warn"],
            lw=2.5,
            connectionstyle="arc3,rad=0.3",
        ),
    )
    ax.text(
        0.5,
        3.5,
        "click\nbrings\nto front",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Z-axis indicator
    ax.annotate(
        "",
        xy=(9.5, 7.0),
        xytext=(9.5, 0.5),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["text_dim"], lw=1.5),
    )
    ax.text(
        10.0,
        3.8,
        "z (draw order)\nhigher = on top",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Result label
    ax.text(
        7.0,
        6.8,
        "after click: A.z = max(z) + 1",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    ax.set_title(
        "Z-Order Model",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "ui/10-windows", "z_order_model.png")


def diagram_drag_mechanics():
    """Three-panel sequence: press on title bar stores grab_offset, drag
    updates rect position maintaining offset, release clears active."""

    fig, axes = plt.subplots(1, 3, figsize=(14, 5))
    fig.patch.set_facecolor(STYLE["bg"])

    phase_titles = ["1. Press", "2. Drag", "3. Release"]
    phase_colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    for i, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 7), ylim=(-0.5, 6), grid=False, aspect=None)
        ax.axis("off")
        stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

        # Window rectangle (different position for each phase)
        if i == 0:
            wx, wy = 1.0, 1.5
            mx, my = 2.5, 4.5
        elif i == 1:
            wx, wy = 2.5, 2.5
            mx, my = 4.0, 5.5
        else:
            wx, wy = 2.5, 2.5
            mx, my = 4.0, 5.5

        ww, wh = 4.0, 3.0
        th = 0.6

        # Window body
        win = mpatches.FancyBboxPatch(
            (wx, wy),
            ww,
            wh,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["surface"],
            edgecolor=phase_colors[i],
            linewidth=2,
        )
        ax.add_patch(win)

        # Title bar
        title = mpatches.Rectangle(
            (wx + 0.03, wy + wh - th),
            ww - 0.06,
            th,
            facecolor=phase_colors[i] + "40",
            edgecolor=phase_colors[i],
            linewidth=1,
        )
        ax.add_patch(title)

        # Mouse cursor
        ax.plot(mx, my, "o", color=STYLE["warn"], markersize=8, zorder=10)

        if i == 0:
            # Show grab_offset calculation
            ax.annotate(
                "",
                xy=(wx, wy + wh),
                xytext=(mx, my),
                arrowprops=dict(
                    arrowstyle="<->",
                    color=STYLE["warn"],
                    lw=1.5,
                    linestyle="--",
                ),
            )
            ax.text(
                (mx + wx) / 2 - 0.8,
                (my + wy + wh) / 2 + 0.3,
                "grab_offset",
                color=STYLE["warn"],
                fontsize=9,
                fontweight="bold",
                path_effects=stroke,
            )
            ax.text(
                3.5,
                0.5,
                "grab_offset = mouse - rect.origin",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                family="monospace",
                path_effects=stroke,
            )
        elif i == 1:
            # Show drag arrow
            ax.annotate(
                "",
                xy=(mx, my),
                xytext=(mx - 1.5, my - 1.0),
                arrowprops=dict(arrowstyle="-|>", color=STYLE["warn"], lw=2),
            )
            ax.text(
                3.5,
                0.5,
                "rect = mouse - grab_offset",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                family="monospace",
                path_effects=stroke,
            )
        else:
            ax.text(
                3.5,
                0.5,
                "active = ID_NONE",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                family="monospace",
                path_effects=stroke,
            )

        ax.set_title(
            phase_titles[i],
            color=phase_colors[i],
            fontsize=12,
            fontweight="bold",
            pad=8,
        )

    fig.suptitle(
        "Drag Mechanics",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "ui/10-windows", "drag_mechanics.png")


def diagram_deferred_draw_pipeline():
    """Data flow: widgets emit into per-window draw lists during declaration,
    then ctx_end sorts by z and appends to main buffer in order."""

    fig, ax = plt.subplots(figsize=(12, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-0.5, 8), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Phase 1: Declaration (left side)
    ax.text(
        2.5,
        7.5,
        "Declaration Phase",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Window draw lists
    win_colors = [STYLE["accent2"], STYLE["accent3"], STYLE["accent1"]]
    win_labels = ["Window A\n(z=0)", "Window B\n(z=1)", "Window C\n(z=2)"]

    for i in range(3):
        y = 5.5 - i * 2.2
        box = mpatches.FancyBboxPatch(
            (0.5, y),
            4.0,
            1.5,
            boxstyle="round,pad=0.06",
            facecolor=win_colors[i] + "25",
            edgecolor=win_colors[i],
            linewidth=1.5,
        )
        ax.add_patch(box)
        ax.text(
            1.2,
            y + 0.75,
            win_labels[i],
            color=win_colors[i],
            fontsize=9,
            fontweight="bold",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            3.3,
            y + 0.75,
            "V[] I[]",
            color=STYLE["text_dim"],
            fontsize=9,
            family="monospace",
            va="center",
            path_effects=stroke,
        )

    # Arrow: sort by z
    ax.annotate(
        "",
        xy=(7.0, 3.5),
        xytext=(5.0, 3.5),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["warn"], lw=2.5),
    )
    ax.text(
        6.0,
        4.2,
        "sort by z_order",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Phase 2: Final buffer (right side)
    ax.text(
        10.5,
        7.5,
        "Final Buffer (back-to-front)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Main buffer with ordered window data
    main_box = mpatches.FancyBboxPatch(
        (7.5, 0.5),
        6.0,
        6.0,
        boxstyle="round,pad=0.08",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text"],
        linewidth=2,
    )
    ax.add_patch(main_box)

    # Non-window background widgets
    bg_box = mpatches.Rectangle(
        (7.8, 5.2),
        5.4,
        0.8,
        facecolor=STYLE["text_dim"] + "20",
        edgecolor=STYLE["text_dim"],
        linewidth=1,
    )
    ax.add_patch(bg_box)
    ax.text(
        10.5,
        5.6,
        "non-window widgets (background)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Ordered window segments
    seg_labels = ["Window A (z=0)", "Window B (z=1)", "Window C (z=2)"]
    for i in range(3):
        y = 3.8 - i * 1.3
        seg = mpatches.Rectangle(
            (7.8, y),
            5.4,
            0.9,
            facecolor=win_colors[i] + "25",
            edgecolor=win_colors[i],
            linewidth=1,
        )
        ax.add_patch(seg)
        ax.text(
            10.5,
            y + 0.45,
            seg_labels[i],
            color=win_colors[i],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Draw order arrow
    ax.annotate(
        "",
        xy=(13.8, 1.0),
        xytext=(13.8, 5.8),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["text_dim"], lw=1.5),
    )
    ax.text(
        14.0,
        3.5,
        "draw\norder",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "ui/10-windows", "deferred_draw_pipeline.png")


def diagram_input_routing_overlap():
    """Top-down view of two overlapping windows with mouse in the overlap
    region, showing hovered_window_id resolving to the higher-z window."""

    fig, ax = plt.subplots(figsize=(9, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10), ylim=(-0.5, 8), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Window A (lower z, partially behind)
    wa = mpatches.FancyBboxPatch(
        (0.5, 1.0),
        5.5,
        5.5,
        boxstyle="round,pad=0.06",
        facecolor=STYLE["accent2"] + "25",
        edgecolor=STYLE["accent2"],
        linewidth=2,
        linestyle="--",
    )
    ax.add_patch(wa)
    ax.text(
        1.5,
        5.5,
        "Window A (z=0)",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=stroke,
    )
    ax.text(
        1.5,
        2.0,
        "hit tests FAIL\n(not hovered window)",
        color=STYLE["accent2"],
        fontsize=9,
        path_effects=stroke,
    )

    # Window B (higher z, in front)
    wb = mpatches.FancyBboxPatch(
        (3.0, 2.0),
        5.5,
        5.5,
        boxstyle="round,pad=0.06",
        facecolor=STYLE["accent1"] + "30",
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax.add_patch(wb)
    ax.text(
        4.0,
        6.5,
        "Window B (z=1)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        path_effects=stroke,
    )
    ax.text(
        6.5,
        4.5,
        "hit tests PASS\n(hovered window)",
        color=STYLE["accent1"],
        fontsize=9,
        path_effects=stroke,
    )

    # Overlap region highlight
    overlap = mpatches.Rectangle(
        (3.0, 2.0),
        3.0,
        4.5,
        facecolor=STYLE["warn"] + "15",
        edgecolor=STYLE["warn"],
        linewidth=1.5,
        linestyle=":",
    )
    ax.add_patch(overlap)

    # Mouse cursor in overlap
    mx, my = 4.5, 4.0
    ax.plot(mx, my, "o", color=STYLE["warn"], markersize=10, zorder=10)
    ax.annotate(
        "mouse in\noverlap region",
        xy=(mx, my),
        xytext=(mx - 2.5, my - 1.5),
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        arrowprops=dict(arrowstyle="->", color=STYLE["warn"], lw=1.5),
        path_effects=stroke,
    )

    # Resolution label
    ax.text(
        5.0,
        0.3,
        "hovered_window_id = B  (highest z containing mouse)",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        bbox=dict(
            facecolor=STYLE["surface"],
            edgecolor=STYLE["accent1"],
            boxstyle="round,pad=0.3",
        ),
    )

    ax.set_title(
        "Input Routing for Overlapping Windows",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "ui/10-windows", "input_routing_overlap.png")


def diagram_collapse_toggle():
    """Side-by-side: expanded window with down-arrow and full content vs
    collapsed window with right-arrow and only the title bar."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])

    titles = ["Expanded (collapsed = false)", "Collapsed (collapsed = true)"]
    arrows = ["down", "right"]

    for i, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 7), grid=False, aspect=None)
        ax.axis("off")
        stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

        wx, ww = 0.5, 6.5
        th = 0.8

        if i == 0:
            # Expanded: full window
            wy = 0.5
            wh = 5.5
            win = mpatches.FancyBboxPatch(
                (wx, wy),
                ww,
                wh,
                boxstyle="round,pad=0.06",
                facecolor=STYLE["surface"],
                edgecolor=STYLE["accent1"],
                linewidth=2,
            )
            ax.add_patch(win)

            # Content area
            content = mpatches.Rectangle(
                (wx + 0.3, wy + 0.3),
                ww - 0.6,
                wh - th - 0.6,
                facecolor=STYLE["bg"] + "60",
                edgecolor=STYLE["accent2"],
                linewidth=1,
                linestyle="--",
            )
            ax.add_patch(content)
            ax.text(
                wx + ww / 2,
                wy + (wh - th) / 2,
                "content area\n(widgets here)",
                color=STYLE["accent2"],
                fontsize=10,
                ha="center",
                va="center",
                path_effects=stroke,
            )

            # Height annotation
            ax.annotate(
                "",
                xy=(wx + ww + 0.3, wy),
                xytext=(wx + ww + 0.3, wy + wh),
                arrowprops=dict(arrowstyle="<->", color=STYLE["text_dim"], lw=1.5),
            )
            ax.text(
                wx + ww + 0.6,
                wy + wh / 2,
                "full\nheight",
                color=STYLE["text_dim"],
                fontsize=9,
                va="center",
                path_effects=stroke,
            )
        else:
            # Collapsed: title bar only
            wy = 4.0
            wh = th + 0.1
            win = mpatches.FancyBboxPatch(
                (wx, wy),
                ww,
                wh,
                boxstyle="round,pad=0.06",
                facecolor=STYLE["surface"],
                edgecolor=STYLE["accent1"],
                linewidth=2,
            )
            ax.add_patch(win)

            # Height annotation
            ax.annotate(
                "",
                xy=(wx + ww + 0.3, wy),
                xytext=(wx + ww + 0.3, wy + wh),
                arrowprops=dict(arrowstyle="<->", color=STYLE["text_dim"], lw=1.5),
            )
            ax.text(
                wx + ww + 0.6,
                wy + wh / 2,
                "title\nonly",
                color=STYLE["text_dim"],
                fontsize=9,
                va="center",
                path_effects=stroke,
            )

            # Content height = 0 annotation
            ax.text(
                wx + ww / 2,
                wy - 0.8,
                "content height = 0\n(no widgets emitted)",
                color=STYLE["accent2"],
                fontsize=9,
                ha="center",
                path_effects=stroke,
            )

        # Title bar
        title_bar = mpatches.Rectangle(
            (wx + 0.05, wy + wh - th),
            ww - 0.1,
            th,
            facecolor=STYLE["accent1"] + "35",
            edgecolor=STYLE["accent1"],
            linewidth=1,
        )
        ax.add_patch(title_bar)

        # Toggle triangle
        tcx = wx + 0.6
        tcy = wy + wh - th / 2
        if arrows[i] == "down":
            tri = plt.Polygon(
                [[tcx - 0.2, tcy + 0.15], [tcx + 0.2, tcy + 0.15], [tcx, tcy - 0.2]],
                closed=True,
                facecolor=STYLE["accent3"],
                edgecolor=STYLE["accent3"],
                linewidth=1.5,
            )
        else:
            tri = plt.Polygon(
                [[tcx - 0.12, tcy + 0.2], [tcx - 0.12, tcy - 0.2], [tcx + 0.2, tcy]],
                closed=True,
                facecolor=STYLE["accent3"],
                edgecolor=STYLE["accent3"],
                linewidth=1.5,
            )
        ax.add_patch(tri)

        ax.text(
            wx + 1.3,
            wy + wh - th / 2,
            "My Window",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            va="center",
            path_effects=stroke,
        )

        ax.set_title(
            titles[i],
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            pad=8,
        )

    fig.suptitle(
        "Collapse Toggle",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "ui/10-windows", "collapse_toggle.png")


def diagram_window_state_persistence():
    """ForgeUiWindowState as application-owned data surviving across frames.
    Arrows show which interactions mutate which fields."""

    fig, ax = plt.subplots(figsize=(10, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-0.5, 9), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Central state box
    bx, by, bw, bh = 3.5, 2.5, 5.0, 5.0
    state_box = mpatches.FancyBboxPatch(
        (bx, by),
        bw,
        bh,
        boxstyle="round,pad=0.12",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax.add_patch(state_box)

    ax.text(
        bx + bw / 2,
        by + bh - 0.4,
        "ForgeUiWindowState",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Fields
    fields = [
        ("rect", "position & size", STYLE["accent2"]),
        ("scroll_y", "content offset", STYLE["accent3"]),
        ("collapsed", "bool toggle", STYLE["accent4"]),
        ("z_order", "draw priority", STYLE["warn"]),
    ]

    for j, (name, desc, color) in enumerate(fields):
        fy = by + bh - 1.2 - j * 1.0
        ax.text(
            bx + 0.5,
            fy,
            name,
            color=color,
            fontsize=11,
            fontweight="bold",
            family="monospace",
            path_effects=stroke,
        )
        ax.text(
            bx + 2.8,
            fy,
            desc,
            color=STYLE["text_dim"],
            fontsize=9,
            path_effects=stroke,
        )

    # Interactions (outside the box, with arrows pointing to fields)
    interactions = [
        ("drag title bar", 0, STYLE["accent2"], (-0.5, 6.5)),
        ("scrollbar / wheel", 1, STYLE["accent3"], (-0.5, 4.8)),
        ("click toggle", 2, STYLE["accent4"], (-0.5, 3.2)),
        ("click on window", 3, STYLE["warn"], (10.0, 5.5)),
    ]

    for label, field_idx, color, (tx, ty) in interactions:
        fy = by + bh - 1.2 - field_idx * 1.0
        target_x = bx if tx < bx else bx + bw
        ax.annotate(
            "",
            xy=(target_x, fy),
            xytext=(tx + (1.5 if tx < bx else -1.5), ty),
            arrowprops=dict(
                arrowstyle="-|>",
                color=color,
                lw=2,
                connectionstyle="arc3,rad=0.15",
            ),
        )
        ax.text(
            tx,
            ty,
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            bbox=dict(
                facecolor=STYLE["bg"],
                edgecolor=color,
                boxstyle="round,pad=0.3",
            ),
        )

    # Frame label
    ax.text(
        6.0,
        0.8,
        "application-owned: survives across frames",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        fontstyle="italic",
        path_effects=stroke,
    )

    ax.set_title(
        "Window State Persistence",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "ui/10-windows", "window_state_persistence.png")


def diagram_window_vs_panel_comparison():
    """Two-column comparison: panel (fixed position, no z-order, no title
    interaction) vs window (dragging, z-ordering, collapse)."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 6))
    fig.patch.set_facecolor(STYLE["bg"])

    for i, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 8), grid=False, aspect=None)
        ax.axis("off")
        stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

        if i == 0:
            # Panel
            color = STYLE["accent2"]
            header = "Panel (Lesson 09)"
            features = [
                ("Fixed position", True),
                ("Title bar", True),
                ("Content clipping", True),
                ("Scrollbar", True),
                ("Dragging", False),
                ("Z-ordering", False),
                ("Collapse/expand", False),
                ("Deferred draw", False),
                ("Input routing", False),
            ]
        else:
            # Window
            color = STYLE["accent1"]
            header = "Window (Lesson 10)"
            features = [
                ("Fixed position", False),
                ("Title bar + drag", True),
                ("Content clipping", True),
                ("Scrollbar", True),
                ("Dragging", True),
                ("Z-ordering", True),
                ("Collapse/expand", True),
                ("Deferred draw", True),
                ("Input routing", True),
            ]

        # Header box
        hbox = mpatches.FancyBboxPatch(
            (0.3, 6.5),
            7.0,
            1.0,
            boxstyle="round,pad=0.08",
            facecolor=color + "30",
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(hbox)
        ax.text(
            3.8,
            7.0,
            header,
            color=color,
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

        # Feature list
        for j, (feat, has_it) in enumerate(features):
            fy = 5.8 - j * 0.7
            marker = "[+]" if has_it else "[ ]"
            marker_color = STYLE["accent3"] if has_it else STYLE["text_dim"]
            feat_color = STYLE["text"] if has_it else STYLE["text_dim"]

            ax.text(
                1.0,
                fy,
                marker,
                color=marker_color,
                fontsize=10,
                family="monospace",
                fontweight="bold",
                path_effects=stroke,
            )
            ax.text(
                2.0,
                fy,
                feat,
                color=feat_color,
                fontsize=10,
                path_effects=stroke,
            )

    fig.suptitle(
        "Window vs Panel Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "ui/10-windows", "window_vs_panel_comparison.png")


# ---------------------------------------------------------------------------
# UI Lesson 11 — Widget ID System
# ---------------------------------------------------------------------------
