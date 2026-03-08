"""Diagrams for ui/04."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 04 — Text Layout
# ---------------------------------------------------------------------------


def diagram_pen_advance():
    """Show the pen/cursor model for the string "Ag" — pen positions before
    and after each character, advance width arrows, bitmap rects with bearing
    offsets, and the baseline."""

    fig, ax = plt.subplots(figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-5, 80), ylim=(-15, 35), grid=False, aspect="equal")
    ax.axis("off")

    # Simulated glyph metrics for Liberation Mono at 32px
    # 'A': advance=19.2, bearing_x=0, bearing_y=24, bitmap=19x24
    # 'g': advance=19.2, bearing_x=1, bearing_y=18, bitmap=17x25

    baseline_y = 0.0
    pen_positions = [0.0, 19.2, 38.4]  # pen x at start, after A, after g
    advance = 19.2

    # ── Draw baseline ──
    ax.axhline(
        y=baseline_y,
        color=STYLE["accent3"],
        linewidth=1.5,
        linestyle="--",
        alpha=0.7,
        xmin=0.02,
        xmax=0.98,
    )
    ax.text(
        72,
        baseline_y + 1.0,
        "baseline",
        color=STYLE["accent3"],
        fontsize=9,
        va="bottom",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # ── Draw ascender and descender lines ──
    ascender_y = 24.0
    descender_y = -7.0
    ax.axhline(
        y=ascender_y,
        color=STYLE["text_dim"],
        linewidth=0.8,
        linestyle=":",
        alpha=0.5,
        xmin=0.02,
        xmax=0.98,
    )
    ax.text(
        72,
        ascender_y + 1.0,
        "ascender",
        color=STYLE["text_dim"],
        fontsize=8,
        va="bottom",
    )
    ax.axhline(
        y=descender_y,
        color=STYLE["text_dim"],
        linewidth=0.8,
        linestyle=":",
        alpha=0.5,
        xmin=0.02,
        xmax=0.98,
    )
    ax.text(
        72,
        descender_y + 1.0,
        "descender",
        color=STYLE["text_dim"],
        fontsize=8,
        va="bottom",
    )

    # ── Glyph 'A' ──
    # Bitmap rect: pen_x + bearing_x = 0, baseline - bearing_y to top = 0
    a_x0 = 0.0  # pen_x + bearing_x (bearing_x=0)
    a_y0 = baseline_y  # bottom of bitmap at baseline
    a_w = 19.0
    a_h = 24.0
    a_rect = mpatches.FancyBboxPatch(
        (a_x0, a_y0),
        a_w,
        a_h,
        boxstyle="round,pad=0.3",
        facecolor=STYLE["accent1"],
        alpha=0.2,
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(a_rect)
    ax.text(
        a_x0 + a_w / 2,
        a_y0 + a_h / 2,
        "A",
        color=STYLE["accent1"],
        fontsize=20,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Bearing_x annotation
    ax.annotate(
        "",
        xy=(a_x0, baseline_y - 2),
        xytext=(0, baseline_y - 2),
        arrowprops={"arrowstyle": "<->", "color": STYLE["warn"], "lw": 1.2},
    )
    ax.text(
        0, baseline_y - 4.5, "bearing_x=0", color=STYLE["warn"], fontsize=7, ha="center"
    )

    # ── Glyph 'g' ──
    g_x0 = 19.2 + 1.0  # pen_x(19.2) + bearing_x(1)
    g_y0 = baseline_y - 7.0  # extends below baseline
    g_w = 17.0
    g_h = 25.0
    g_rect = mpatches.FancyBboxPatch(
        (g_x0, g_y0),
        g_w,
        g_h,
        boxstyle="round,pad=0.3",
        facecolor=STYLE["accent2"],
        alpha=0.2,
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
    )
    ax.add_patch(g_rect)
    ax.text(
        g_x0 + g_w / 2,
        g_y0 + g_h / 2,
        "g",
        color=STYLE["accent2"],
        fontsize=20,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Bearing_x annotation for 'g'
    ax.annotate(
        "",
        xy=(19.2, baseline_y - 2),
        xytext=(g_x0, baseline_y - 2),
        arrowprops={"arrowstyle": "<->", "color": STYLE["warn"], "lw": 1.2},
    )
    ax.text(
        19.7,
        baseline_y - 4.5,
        "bearing_x=1",
        color=STYLE["warn"],
        fontsize=7,
        ha="center",
    )

    # ── Pen position markers ──
    for _i, px in enumerate(pen_positions):
        marker_y = baseline_y - 9.0
        ax.plot(px, marker_y, "v", color=STYLE["accent4"], markersize=8)
        label = f"pen={px:.1f}"
        ax.text(
            px,
            marker_y - 2.5,
            label,
            color=STYLE["accent4"],
            fontsize=8,
            ha="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # ── Advance width arrows ──
    arrow_y = baseline_y + 28.0
    for i in range(2):
        x_start = pen_positions[i]
        x_end = pen_positions[i + 1]
        ax.annotate(
            "",
            xy=(x_end, arrow_y),
            xytext=(x_start, arrow_y),
            arrowprops={"arrowstyle": "->", "color": STYLE["accent3"], "lw": 2.0},
        )
        ax.text(
            (x_start + x_end) / 2,
            arrow_y + 1.5,
            f"advance = {advance:.1f} px",
            color=STYLE["accent3"],
            fontsize=8,
            ha="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Title
    ax.text(
        38.4 / 2,
        33,
        "Pen Advance Model",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )

    fig.tight_layout()
    save(fig, "ui/04-text-layout", "pen_advance.png")


def diagram_baseline_metrics():
    """Show a line of text with the baseline, ascender line, descender line,
    and lineGap clearly labeled with pixel measurements from Liberation Mono
    at 32px."""

    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-2, 42), ylim=(-10, 48), grid=False, aspect="equal")
    ax.axis("off")

    # Liberation Mono at 32px: scale = 32/2048 = 0.015625
    # ascender = 1491 -> 23.3px
    # descender = -431 -> -6.7px
    # lineGap = 307 -> 4.8px
    # line_height = 23.3 + 6.7 + 4.8 = 34.8px

    ascender_px = 23.3
    descender_px = 6.7
    line_gap_px = 4.8
    line_height = ascender_px + descender_px + line_gap_px

    # Position baseline of line 1
    baseline1 = 30.0
    ascender1 = baseline1 + ascender_px
    descender1 = baseline1 - descender_px

    # Line 2 baseline
    baseline2 = baseline1 - line_height
    ascender2 = baseline2 + ascender_px

    line_x0 = 0.0
    line_x1 = 35.0

    # ── Draw horizontal metric lines for line 1 ──
    lines_data = [
        (ascender1, "ascender", STYLE["accent1"], "-"),
        (baseline1, "baseline", STYLE["accent3"], "--"),
        (descender1, "descender", STYLE["accent2"], "-."),
    ]

    for y_val, label, color, ls in lines_data:
        ax.plot(
            [line_x0, line_x1],
            [y_val, y_val],
            color=color,
            linewidth=1.5,
            linestyle=ls,
            alpha=0.8,
        )
        ax.text(
            line_x1 + 0.5,
            y_val,
            label,
            color=color,
            fontsize=9,
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # ── Draw lineGap region ──
    gap_top = descender1
    gap_bot = ascender2
    gap_rect = mpatches.FancyBboxPatch(
        (line_x0, gap_bot),
        line_x1 - line_x0,
        gap_top - gap_bot,
        boxstyle="round,pad=0",
        facecolor=STYLE["accent4"],
        alpha=0.15,
        edgecolor=STYLE["accent4"],
        linewidth=1.0,
        linestyle=":",
    )
    ax.add_patch(gap_rect)
    ax.text(
        line_x1 + 0.5,
        (gap_top + gap_bot) / 2,
        "lineGap",
        color=STYLE["accent4"],
        fontsize=9,
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # ── Draw baseline for line 2 ──
    ax.plot(
        [line_x0, line_x1],
        [baseline2, baseline2],
        color=STYLE["accent3"],
        linewidth=1.5,
        linestyle="--",
        alpha=0.8,
    )
    ax.text(
        line_x1 + 0.5,
        baseline2,
        "baseline 2",
        color=STYLE["accent3"],
        fontsize=9,
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # ── Simulated glyphs on line 1: "Apg" ──
    # 'A' sits on the baseline, top at ascender
    ax.text(
        3,
        baseline1,
        "A",
        color=STYLE["text"],
        fontsize=22,
        fontweight="bold",
        va="baseline",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        12,
        baseline1,
        "p",
        color=STYLE["text"],
        fontsize=22,
        fontweight="bold",
        va="baseline",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        21,
        baseline1,
        "g",
        color=STYLE["text"],
        fontsize=22,
        fontweight="bold",
        va="baseline",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # ── Dimension arrows on the right side ──
    dim_x = -1.5

    # Ascender dimension
    ax.annotate(
        "",
        xy=(dim_x, ascender1),
        xytext=(dim_x, baseline1),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent1"], "lw": 1.5},
    )
    ax.text(
        dim_x - 0.5,
        (ascender1 + baseline1) / 2,
        f"{ascender_px:.1f} px",
        color=STYLE["accent1"],
        fontsize=8,
        ha="right",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Descender dimension
    ax.annotate(
        "",
        xy=(dim_x, baseline1),
        xytext=(dim_x, descender1),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent2"], "lw": 1.5},
    )
    ax.text(
        dim_x - 0.5,
        (baseline1 + descender1) / 2,
        f"{descender_px:.1f} px",
        color=STYLE["accent2"],
        fontsize=8,
        ha="right",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # LineGap dimension
    ax.annotate(
        "",
        xy=(dim_x, gap_top),
        xytext=(dim_x, gap_bot),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent4"], "lw": 1.5},
    )
    ax.text(
        dim_x - 0.5,
        (gap_top + gap_bot) / 2,
        f"{line_gap_px:.1f} px",
        color=STYLE["accent4"],
        fontsize=8,
        ha="right",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Total line height annotation
    ax.annotate(
        "",
        xy=(dim_x - 3, ascender1),
        xytext=(dim_x - 3, ascender2),
        arrowprops={"arrowstyle": "<->", "color": STYLE["warn"], "lw": 2.0},
    )
    ax.text(
        dim_x - 3.5,
        (ascender1 + ascender2) / 2,
        f"line height\n{line_height:.1f} px",
        color=STYLE["warn"],
        fontsize=9,
        ha="right",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Title
    ax.text(
        17,
        46,
        "Baseline, Ascender, Descender, and Line Gap",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        17,
        43,
        "Liberation Mono at 32 px",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
    )

    fig.tight_layout()
    save(fig, "ui/04-text-layout", "baseline_metrics.png")


def diagram_quad_vertex_layout():
    """Show one character's quad with the four vertices labeled with their
    position and UV coordinates, and the two triangles with index order."""

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])

    # ── Left panel: Screen-space quad ──
    setup_axes(ax1, xlim=(-3, 28), ylim=(-5, 22), grid=False, aspect="equal")
    ax1.axis("off")

    # Quad corners (screen-space, y-down but we draw with y-up for readability)
    # Vertex 0: top-left (5, 2)
    # Vertex 1: top-right (24, 2)
    # Vertex 2: bottom-right (24, 18)
    # Vertex 3: bottom-left (5, 18)
    verts = [(5, 18), (24, 18), (24, 2), (5, 2)]  # y-up for display
    labels = [
        "v0 (top-left)",
        "v1 (top-right)",
        "v2 (bottom-right)",
        "v3 (bottom-left)",
    ]
    pos_labels = ["(5, 2)", "(24, 2)", "(24, 18)", "(5, 18)"]

    # Draw the quad
    quad = plt.Polygon(
        verts,
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2.0,
        alpha=0.5,
    )
    ax1.add_patch(quad)

    # Draw the diagonal (triangle split)
    ax1.plot(
        [verts[0][0], verts[2][0]],
        [verts[0][1], verts[2][1]],
        color=STYLE["accent2"],
        linewidth=1.5,
        linestyle="--",
        alpha=0.7,
    )

    # Label triangles
    tri1_cx = (verts[0][0] + verts[1][0] + verts[2][0]) / 3
    tri1_cy = (verts[0][1] + verts[1][1] + verts[2][1]) / 3
    ax1.text(
        tri1_cx,
        tri1_cy,
        "T0\n(0,1,2)",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    tri2_cx = (verts[2][0] + verts[3][0] + verts[0][0]) / 3
    tri2_cy = (verts[2][1] + verts[3][1] + verts[0][1]) / 3
    ax1.text(
        tri2_cx,
        tri2_cy,
        "T1\n(2,3,0)",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Draw vertices with labels
    colors = [STYLE["accent1"], STYLE["accent1"], STYLE["accent2"], STYLE["accent2"]]
    offsets = [(-1.5, 1.5), (1.5, 1.5), (1.5, -1.5), (-1.5, -1.5)]
    ha_vals = ["right", "left", "left", "right"]

    for i, (vx, vy) in enumerate(verts):
        ax1.plot(vx, vy, "o", color=colors[i], markersize=8, zorder=5)
        ox, oy = offsets[i]
        ax1.text(
            vx + ox,
            vy + oy,
            f"{labels[i]}\npos={pos_labels[i]}",
            color=colors[i],
            fontsize=8,
            ha=ha_vals[i],
            va="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Winding direction arrow
    ax1.annotate(
        "CCW",
        xy=(verts[1][0] - 2, verts[1][1]),
        xytext=(verts[0][0] + 2, verts[0][1]),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent3"],
            "lw": 1.5,
            "connectionstyle": "arc3,rad=-0.3",
        },
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax1.set_title(
        "Screen-Space Quad", color=STYLE["text"], fontsize=12, fontweight="bold", pad=10
    )

    # ── Right panel: UV coordinates in atlas space ──
    setup_axes(ax2, xlim=(-0.1, 1.1), ylim=(-0.15, 1.15), grid=False, aspect="equal")
    ax2.axis("off")

    # Atlas boundary
    atlas_rect = mpatches.Rectangle(
        (0, 0),
        1.0,
        1.0,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["axis"],
        linewidth=1.0,
        alpha=0.3,
    )
    ax2.add_patch(atlas_rect)

    # UV quad (a small region of the atlas)
    uv0 = (0.2, 0.7)  # top-left in atlas (u0, v0) — display y-up
    uv1 = (0.45, 0.38)  # bottom-right (u1, v1)

    uv_verts = [(uv0[0], uv0[1]), (uv1[0], uv0[1]), (uv1[0], uv1[1]), (uv0[0], uv1[1])]
    uv_quad = plt.Polygon(
        uv_verts,
        closed=True,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        linewidth=2.0,
        alpha=0.3,
    )
    ax2.add_patch(uv_quad)

    # UV labels
    uv_labels = [
        f"(u0, v0)\n({uv0[0]:.2f}, {1 - uv0[1]:.2f})",
        f"(u1, v0)\n({uv1[0]:.2f}, {1 - uv0[1]:.2f})",
        f"(u1, v1)\n({uv1[0]:.2f}, {1 - uv1[1]:.2f})",
        f"(u0, v1)\n({uv0[0]:.2f}, {1 - uv1[1]:.2f})",
    ]
    uv_offsets = [(-0.08, 0.05), (0.08, 0.05), (0.08, -0.05), (-0.08, -0.05)]
    uv_ha = ["right", "left", "left", "right"]

    for i, (ux, uy) in enumerate(uv_verts):
        ax2.plot(ux, uy, "o", color=STYLE["accent1"], markersize=6, zorder=5)
        ox, oy = uv_offsets[i]
        ax2.text(
            ux + ox,
            uy + oy,
            uv_labels[i],
            color=STYLE["accent1"],
            fontsize=8,
            ha=uv_ha[i],
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Atlas label
    ax2.text(
        0.5,
        1.08,
        "Atlas Texture (1.0 x 1.0)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )

    ax2.set_title(
        "Atlas UV Coordinates",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    fig.tight_layout()
    save(fig, "ui/04-text-layout", "quad_vertex_layout.png")


def diagram_line_breaking():
    """Show a paragraph of text with a max_width boundary, illustrating where
    lines break and how pen y advances downward."""

    fig, ax = plt.subplots(figsize=(10, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-5, 55), ylim=(-5, 42), grid=False, aspect="equal")
    ax.axis("off")

    max_width = 40.0
    line_height = 8.0
    origin_x = 2.0
    top_y = 36.0

    # Simulated wrapped lines
    lines = [
        "Text layout converts",
        "a string into quads",
        "for GPU rendering.",
    ]
    line_widths = [38.0, 37.0, 34.0]  # approximate pixel widths

    # ── Draw max_width boundary ──
    boundary_x = origin_x + max_width
    ax.plot(
        [boundary_x, boundary_x],
        [-3, 40],
        color=STYLE["accent2"],
        linewidth=2.0,
        linestyle="--",
        alpha=0.8,
    )
    ax.text(
        boundary_x + 0.5,
        40,
        f"max_width = {max_width:.0f}",
        color=STYLE["accent2"],
        fontsize=9,
        va="top",
        rotation=90,
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # ── Draw left edge ──
    ax.plot(
        [origin_x, origin_x],
        [-3, 40],
        color=STYLE["text_dim"],
        linewidth=0.8,
        linestyle=":",
        alpha=0.5,
    )

    # ── Draw each line of text ──
    for i, (line_text, lw) in enumerate(zip(lines, line_widths, strict=True)):
        baseline_y = top_y - i * line_height

        # Text block rect
        rect = mpatches.FancyBboxPatch(
            (origin_x, baseline_y - 2.5),
            lw,
            6.5,
            boxstyle="round,pad=0.2",
            facecolor=STYLE["accent1"],
            alpha=0.15,
            edgecolor=STYLE["accent1"],
            linewidth=1.0,
        )
        ax.add_patch(rect)

        # Line text
        ax.text(
            origin_x + 0.5,
            baseline_y,
            line_text,
            color=STYLE["text"],
            fontsize=9,
            va="baseline",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Baseline marker
        ax.plot(
            [origin_x - 1, origin_x + lw + 1],
            [baseline_y, baseline_y],
            color=STYLE["accent3"],
            linewidth=0.7,
            linestyle=":",
            alpha=0.4,
        )

        # Line number label
        ax.text(
            origin_x - 2,
            baseline_y,
            f"L{i + 1}",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="baseline",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # ── Pen y advancement arrows ──
    arrow_x = origin_x + max_width + 5
    for i in range(len(lines) - 1):
        y_start = top_y - i * line_height
        y_end = top_y - (i + 1) * line_height
        ax.annotate(
            "",
            xy=(arrow_x, y_end),
            xytext=(arrow_x, y_start),
            arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 1.5},
        )
        ax.text(
            arrow_x + 1,
            (y_start + y_end) / 2,
            f"pen_y += {line_height:.0f}",
            color=STYLE["warn"],
            fontsize=8,
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # ── Pen x reset annotations ──
    for i in range(1, len(lines)):
        y_pos = top_y - i * line_height + 5.5
        ax.annotate(
            "",
            xy=(origin_x, y_pos),
            xytext=(origin_x + 8, y_pos),
            arrowprops={
                "arrowstyle": "->",
                "color": STYLE["accent4"],
                "lw": 1.2,
                "connectionstyle": "arc3,rad=0.3",
            },
        )
        ax.text(
            origin_x + 9,
            y_pos,
            "pen_x = origin",
            color=STYLE["accent4"],
            fontsize=7,
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Title
    ax.text(
        origin_x + max_width / 2,
        41,
        "Line Breaking with max_width",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )

    fig.tight_layout()
    save(fig, "ui/04-text-layout", "line_breaking.png")


# ---------------------------------------------------------------------------
# UI Lesson 05 — Immediate-Mode Basics
# ---------------------------------------------------------------------------
