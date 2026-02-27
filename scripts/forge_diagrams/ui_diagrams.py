"""Diagram functions for UI lessons (lessons/ui/).

Each function generates a single PNG diagram and saves it to the lesson's
assets/ directory.  Register new diagrams in __main__.py after adding them
here.

Requires: pip install numpy matplotlib
"""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from ._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 01 — TTF Parsing
# ---------------------------------------------------------------------------


def diagram_ttf_file_structure():
    """Show the high-level structure of a TTF file: offset table, table
    directory, and data tables at various offsets."""

    fig, ax = plt.subplots(figsize=(8, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10), ylim=(-0.5, 7.5), grid=False, aspect=None)
    ax.axis("off")

    # File as a vertical stack of blocks
    block_x = 1.0
    block_w = 3.0
    blocks = [
        (
            "Offset Table",
            0.6,
            STYLE["accent1"],
            "12 bytes\nsfVersion, numTables,\nsearchRange, ...",
        ),
        (
            "Table Directory",
            1.0,
            STYLE["accent2"],
            "16 bytes per entry\ntag, checksum,\noffset, length",
        ),
        ("head", 0.5, STYLE["accent3"], ""),
        ("hhea", 0.5, STYLE["accent3"], ""),
        ("maxp", 0.5, STYLE["accent3"], ""),
        ("cmap", 0.5, STYLE["accent3"], ""),
        ("loca", 0.5, STYLE["accent3"], ""),
        ("glyf", 1.2, STYLE["accent4"], "(largest table)"),
    ]

    y = 7.0
    positions = {}
    for name, height, color, note in blocks:
        rect = mpatches.FancyBboxPatch(
            (block_x, y - height),
            block_w,
            height,
            boxstyle="round,pad=0.05",
            facecolor=color + "30",
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(rect)
        ax.text(
            block_x + block_w / 2,
            y - height / 2,
            name,
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
        )
        if note:
            ax.text(
                block_x + block_w + 0.3,
                y - height / 2,
                note,
                color=STYLE["text_dim"],
                fontsize=7.5,
                ha="left",
                va="center",
            )
        positions[name] = (block_x + block_w / 2, y - height / 2)
        y -= height + 0.1

    # Draw arrows from table directory to data tables
    dir_pos = positions["Table Directory"]
    for table_name in ["head", "hhea", "maxp", "cmap", "loca", "glyf"]:
        tbl_pos = positions[table_name]
        # Small arrow from directory right side pointing to table
        ax.annotate(
            "",
            xy=(block_x + 0.05, tbl_pos[1]),
            xytext=(block_x + 0.05, dir_pos[1] - 0.5),
            arrowprops=dict(
                arrowstyle="->",
                color=STYLE["text_dim"],
                lw=0.5,
                connectionstyle="arc3,rad=-0.15",
            ),
        )

    # Title
    ax.text(
        5.0,
        7.4,
        "TTF File Structure",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )

    # Byte offset annotations on the left
    ax.text(
        block_x - 0.2,
        positions["Offset Table"][1],
        "byte 0",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="right",
        va="center",
    )
    ax.text(
        block_x - 0.2,
        positions["Table Directory"][1],
        "byte 12",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="right",
        va="center",
    )

    save(fig, "ui/01-ttf-parsing", "ttf_file_structure.png")


def diagram_glyph_anatomy():
    """Show the anatomy of a simple glyph outline with on-curve and
    off-curve points, contours, and the bounding box."""

    fig, ax = plt.subplots(figsize=(6, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-100, 1400), ylim=(-200, 1500), grid=False, aspect="equal")
    ax.axis("off")

    # Simplified 'A' shape — outer contour (triangle-like)
    outer_x = [0, 614, 1228, 1034, 896, 333, 196, 0]
    outer_y = [0, 1349, 0, 0, 382, 382, 0, 0]
    on_curve_outer = [True, True, True, True, True, True, True, True]

    # Inner contour (counter/hole) — includes an off-curve control point
    # to demonstrate quadratic Bezier curves in glyph outlines
    inner_x = [400, 510, 614, 828, 400]
    inner_y = [500, 850, 1100, 500, 500]
    on_curve_inner = [True, False, True, True, True]

    # Draw bounding box
    bbox = mpatches.FancyBboxPatch(
        (0, 0),
        1228,
        1349,
        boxstyle="square,pad=0",
        facecolor="none",
        edgecolor=STYLE["text_dim"],
        linewidth=0.8,
        linestyle="--",
    )
    ax.add_patch(bbox)
    ax.text(
        1240, 1349, "yMax", color=STYLE["text_dim"], fontsize=7, va="center", ha="left"
    )
    ax.text(
        1240, 0, "yMin", color=STYLE["text_dim"], fontsize=7, va="center", ha="left"
    )

    # Draw baseline
    ax.axhline(y=0, color=STYLE["text_dim"], linewidth=0.5, linestyle=":")
    ax.text(
        -80, 0, "baseline", color=STYLE["text_dim"], fontsize=7, va="center", ha="right"
    )

    # Draw outer contour
    ax.plot(
        outer_x, outer_y, color=STYLE["accent1"], linewidth=2, solid_capstyle="round"
    )

    # Draw inner contour
    ax.plot(
        inner_x, inner_y, color=STYLE["accent2"], linewidth=2, solid_capstyle="round"
    )

    # Draw points
    for x, y, on in zip(outer_x[:-1], outer_y[:-1], on_curve_outer):
        marker = "o" if on else "s"
        color = STYLE["accent1"]
        ax.plot(
            x,
            y,
            marker=marker,
            markersize=6,
            color=color,
            markeredgecolor="white",
            markeredgewidth=0.5,
            zorder=5,
        )

    for x, y, on in zip(inner_x[:-1], inner_y[:-1], on_curve_inner):
        marker = "o" if on else "s"
        color = STYLE["accent2"]
        ax.plot(
            x,
            y,
            marker=marker,
            markersize=6,
            color=color,
            markeredgecolor="white",
            markeredgewidth=0.5,
            zorder=5,
        )

    # Labels
    ax.text(
        614,
        1400,
        "Contour 0 (outer)",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        614,
        780,
        "Contour 1\n(counter)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )

    # Legend
    ax.plot(
        [],
        [],
        "o",
        color=STYLE["accent1"],
        label="on-curve point",
        markersize=6,
        markeredgecolor="white",
        markeredgewidth=0.5,
    )
    ax.plot(
        [],
        [],
        "s",
        color=STYLE["accent2"],
        label="off-curve point",
        markersize=6,
        markeredgecolor="white",
        markeredgewidth=0.5,
    )
    leg = ax.legend(
        loc="upper right",
        fontsize=8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    leg.get_frame().set_alpha(0.9)

    # Title
    ax.text(
        614,
        -120,
        "Glyph Anatomy (simplified 'A')",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
    )

    save(fig, "ui/01-ttf-parsing", "glyph_anatomy.png")


def diagram_endianness():
    """Show big-endian vs little-endian byte layout for uint16 and uint32."""

    fig, ax = plt.subplots(figsize=(8, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 9), ylim=(-1.5, 4.5), grid=False, aspect=None)
    ax.axis("off")

    cell_w = 1.0
    cell_h = 0.7

    def draw_bytes(ax, x, y, bytes_list, label, color):
        """Draw a row of byte cells."""
        for i, b in enumerate(bytes_list):
            rect = mpatches.FancyBboxPatch(
                (x + i * cell_w, y),
                cell_w,
                cell_h,
                boxstyle="round,pad=0.03",
                facecolor=color + "30",
                edgecolor=color,
                linewidth=1.2,
            )
            ax.add_patch(rect)
            ax.text(
                x + i * cell_w + cell_w / 2,
                y + cell_h / 2,
                b,
                color=STYLE["text"],
                fontsize=10,
                fontweight="bold",
                ha="center",
                va="center",
                family="monospace",
            )
        ax.text(
            x - 0.15,
            y + cell_h / 2,
            label,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="center",
        )

    # Title
    ax.text(
        4.5,
        4.2,
        "Byte Order: Big-Endian vs Little-Endian",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
    )

    # uint16 = 0x0100 (256)
    ax.text(
        4.5,
        3.5,
        "uint16 value: 256 (0x0100)",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
    )
    draw_bytes(ax, 2.0, 2.5, ["0x01", "0x00"], "Big-endian (TTF)", STYLE["accent1"])
    draw_bytes(ax, 2.0, 1.5, ["0x00", "0x01"], "Little-endian (x86)", STYLE["accent2"])

    # Annotations — place above big-endian / below little-endian to avoid overlap
    ax.text(
        2.5,
        2.5 + cell_h + 0.05,
        "MSB first",
        color=STYLE["text_dim"],
        fontsize=6.5,
        ha="center",
        va="bottom",
    )
    ax.text(
        2.5,
        1.5 - 0.05,
        "LSB first",
        color=STYLE["text_dim"],
        fontsize=6.5,
        ha="center",
        va="top",
    )

    # uint32 = 0x00010000
    ax.text(
        4.5,
        0.8,
        "uint32 value: 65536 (0x00010000)",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
    )
    draw_bytes(
        ax,
        1.0,
        -0.2,
        ["0x00", "0x01", "0x00", "0x00"],
        "Big-endian (TTF)",
        STYLE["accent1"],
    )
    draw_bytes(
        ax,
        1.0,
        -1.2,
        ["0x00", "0x00", "0x01", "0x00"],
        "Little-endian (x86)",
        STYLE["accent2"],
    )

    save(fig, "ui/01-ttf-parsing", "endianness.png")


# ---------------------------------------------------------------------------
# UI Lesson 02 — Glyph Rasterization
# ---------------------------------------------------------------------------


def _quadratic_bezier(p0, p1, p2, n=50):
    """Evaluate a quadratic Bezier curve at *n* uniform parameter values.

    Returns arrays (xs, ys) of shape (n,).
    """
    t = np.linspace(0, 1, n)
    x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t**2 * p2[0]
    y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t**2 * p2[1]
    return x, y


def diagram_contour_reconstruction():
    """Show the three TrueType segment cases: line (on-on), quadratic Bezier
    (on-off-on), and implicit midpoint (off-off)."""

    fig, axes = plt.subplots(1, 3, figsize=(12, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Contour Reconstruction \u2014 Three Segment Cases",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    on_marker = dict(
        marker="o",
        markersize=10,
        markeredgecolor="white",
        markeredgewidth=1.0,
        zorder=5,
    )
    off_marker = dict(
        marker="s",
        markersize=10,
        markeredgecolor="white",
        markeredgewidth=1.0,
        zorder=5,
    )

    # ---- Case 1: on -> on (line segment) --------------------------------
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 4.5), ylim=(-0.5, 4.5), grid=False)
    ax.axis("off")

    p0, p1 = (0.5, 0.5), (3.5, 3.5)
    # Draw the resulting line
    ax.plot(
        [p0[0], p1[0]],
        [p0[1], p1[1]],
        color=STYLE["accent1"],
        linewidth=2.5,
        zorder=3,
    )
    # Draw on-curve points
    ax.plot(p0[0], p0[1], color=STYLE["accent1"], **on_marker)
    ax.plot(p1[0], p1[1], color=STYLE["accent1"], **on_marker)
    # Labels
    ax.text(
        p0[0] - 0.25,
        p0[1] - 0.35,
        "on",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        p1[0] + 0.25,
        p1[1] + 0.35,
        "on",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.set_title(
        "Line (on \u2192 on)",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        pad=8,
    )

    # ---- Case 2: on -> off -> on (explicit quadratic Bezier) ------------
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 4.5), ylim=(-0.5, 4.5), grid=False)
    ax.axis("off")

    p0, p1_off, p2 = (0.5, 0.5), (2.0, 4.0), (3.5, 0.5)
    # Control polygon (dashed)
    ax.plot(
        [p0[0], p1_off[0], p2[0]],
        [p0[1], p1_off[1], p2[1]],
        color=STYLE["text_dim"],
        linewidth=1.0,
        linestyle="--",
        zorder=2,
    )
    # Bezier curve
    bx, by = _quadratic_bezier(p0, p1_off, p2)
    ax.plot(bx, by, color=STYLE["accent2"], linewidth=2.5, zorder=3)
    # Points
    ax.plot(p0[0], p0[1], color=STYLE["accent1"], **on_marker)
    ax.plot(p2[0], p2[1], color=STYLE["accent1"], **on_marker)
    ax.plot(p1_off[0], p1_off[1], color=STYLE["accent2"], **off_marker)
    # Labels
    ax.text(
        p0[0] - 0.25,
        p0[1] - 0.35,
        "on",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        p2[0] + 0.25,
        p2[1] - 0.35,
        "on",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        p1_off[0] + 0.35,
        p1_off[1] + 0.15,
        "off",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.set_title(
        "Quadratic B\u00e9zier (on \u2192 off \u2192 on)",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        pad=8,
    )

    # ---- Case 3: off -> off (implicit midpoint) -------------------------
    ax = axes[2]
    setup_axes(ax, xlim=(-0.5, 5.5), ylim=(-0.5, 4.5), grid=False)
    ax.axis("off")

    # Two adjacent off-curve points with an implicit on-curve midpoint
    p_on_start = (0.3, 1.0)
    p_off1 = (1.5, 4.0)
    p_off2 = (3.5, 4.0)
    p_on_end = (4.7, 1.0)
    # Implicit midpoint between p_off1 and p_off2
    p_mid = (
        (p_off1[0] + p_off2[0]) / 2,
        (p_off1[1] + p_off2[1]) / 2,
    )

    # Control polygon (dashed)
    ax.plot(
        [p_on_start[0], p_off1[0], p_off2[0], p_on_end[0]],
        [p_on_start[1], p_off1[1], p_off2[1], p_on_end[1]],
        color=STYLE["text_dim"],
        linewidth=1.0,
        linestyle="--",
        zorder=2,
    )

    # First Bezier: on_start -> off1 -> midpoint
    bx1, by1 = _quadratic_bezier(p_on_start, p_off1, p_mid)
    ax.plot(bx1, by1, color=STYLE["accent2"], linewidth=2.5, zorder=3)
    # Second Bezier: midpoint -> off2 -> on_end
    bx2, by2 = _quadratic_bezier(p_mid, p_off2, p_on_end)
    ax.plot(bx2, by2, color=STYLE["accent2"], linewidth=2.5, zorder=3)

    # Points
    ax.plot(p_on_start[0], p_on_start[1], color=STYLE["accent1"], **on_marker)
    ax.plot(p_on_end[0], p_on_end[1], color=STYLE["accent1"], **on_marker)
    ax.plot(p_off1[0], p_off1[1], color=STYLE["accent2"], **off_marker)
    ax.plot(p_off2[0], p_off2[1], color=STYLE["accent2"], **off_marker)
    # Implicit midpoint — highlighted
    ax.plot(
        p_mid[0],
        p_mid[1],
        marker="D",
        markersize=10,
        color=STYLE["accent3"],
        markeredgecolor="white",
        markeredgewidth=1.0,
        zorder=6,
    )

    # Labels
    ax.text(
        p_on_start[0] - 0.15,
        p_on_start[1] - 0.4,
        "on",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        p_on_end[0] + 0.15,
        p_on_end[1] - 0.4,
        "on",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        p_off1[0] - 0.35,
        p_off1[1] + 0.15,
        "off",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        p_off2[0] + 0.35,
        p_off2[1] + 0.15,
        "off",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        p_mid[0],
        p_mid[1] - 0.5,
        "implicit\nmidpoint",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
    )
    ax.set_title(
        "Implicit Midpoint (off \u2192 off)",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        pad=8,
    )

    # Shared legend at the bottom of the figure
    legend_items = [
        plt.Line2D(
            [0],
            [0],
            marker="o",
            color=STYLE["bg"],
            markerfacecolor=STYLE["accent1"],
            markersize=8,
            markeredgecolor="white",
            markeredgewidth=0.8,
            label="on-curve point",
        ),
        plt.Line2D(
            [0],
            [0],
            marker="s",
            color=STYLE["bg"],
            markerfacecolor=STYLE["accent2"],
            markersize=8,
            markeredgecolor="white",
            markeredgewidth=0.8,
            label="off-curve point",
        ),
        plt.Line2D(
            [0],
            [0],
            marker="D",
            color=STYLE["bg"],
            markerfacecolor=STYLE["accent3"],
            markersize=8,
            markeredgecolor="white",
            markeredgewidth=0.8,
            label="implicit midpoint",
        ),
    ]
    fig.legend(
        handles=legend_items,
        loc="lower center",
        ncol=3,
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        framealpha=0.9,
        bbox_to_anchor=(0.5, -0.01),
    )

    fig.tight_layout(rect=[0, 0.06, 1, 0.93])
    save(fig, "ui/02-glyph-rasterization", "contour_reconstruction.png")


def diagram_scanline_crossings():
    """Show scanline rasterization of an 'O' shape with winding-rule fill."""

    fig, ax = plt.subplots(figsize=(8, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-3, 3), ylim=(-2.8, 3.0), grid=False)
    ax.axis("off")

    # Outer ellipse (clockwise)
    theta = np.linspace(0, 2 * np.pi, 200)
    outer_rx, outer_ry = 2.2, 2.2
    outer_x = outer_rx * np.cos(theta)
    outer_y = outer_ry * np.sin(theta)

    # Inner ellipse (counter-clockwise — drawn reversed)
    inner_rx, inner_ry = 1.1, 1.1
    inner_x = inner_rx * np.cos(theta)
    inner_y = inner_ry * np.sin(theta)

    # Draw the outlines
    ax.plot(outer_x, outer_y, color=STYLE["accent1"], linewidth=2.0, zorder=3)
    ax.plot(inner_x, inner_y, color=STYLE["accent2"], linewidth=2.0, zorder=3)

    # Scanline at y = 0.5
    scan_y = 0.5
    ax.plot(
        [-2.8, 2.8],
        [scan_y, scan_y],
        color=STYLE["warn"],
        linewidth=1.5,
        linestyle="--",
        zorder=4,
    )
    ax.text(
        -2.75,
        scan_y + 0.15,
        "scanline",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        va="bottom",
    )

    # Crossing points: outer ellipse at y=scan_y
    # x^2/rx^2 + y^2/ry^2 = 1  =>  x = +/- rx * sqrt(1 - y^2/ry^2)
    outer_cross_x = outer_rx * np.sqrt(1 - (scan_y / outer_ry) ** 2)
    inner_cross_x = inner_rx * np.sqrt(1 - (scan_y / inner_ry) ** 2)

    crossings = [
        (-outer_cross_x, "+1", STYLE["accent1"]),
        (-inner_cross_x, "\u22121", STYLE["accent2"]),
        (inner_cross_x, "+1", STYLE["accent2"]),
        (outer_cross_x, "\u22121", STYLE["accent1"]),
    ]

    for cx, wlabel, color in crossings:
        ax.plot(
            cx,
            scan_y,
            "o",
            markersize=10,
            color=color,
            markeredgecolor="white",
            markeredgewidth=1.0,
            zorder=6,
        )
        ax.text(
            cx,
            scan_y + 0.3,
            wlabel,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Shade the filled regions (between outer-inner crossings on each side)
    # Left filled region: from -outer to -inner
    ax.fill_betweenx(
        [scan_y - 0.08, scan_y + 0.08],
        -outer_cross_x,
        -inner_cross_x,
        color=STYLE["accent1"],
        alpha=0.3,
        zorder=2,
    )
    # Right filled region: from inner to outer
    ax.fill_betweenx(
        [scan_y - 0.08, scan_y + 0.08],
        inner_cross_x,
        outer_cross_x,
        color=STYLE["accent1"],
        alpha=0.3,
        zorder=2,
    )

    # Winding count annotations along the scanline
    winding_positions = [
        (-outer_cross_x - 0.5, "w=0", STYLE["text_dim"]),
        ((-outer_cross_x + (-inner_cross_x)) / 2, "w=1", STYLE["accent3"]),
        (0, "w=0", STYLE["text_dim"]),
        ((inner_cross_x + outer_cross_x) / 2, "w=\u22121", STYLE["accent3"]),
        (outer_cross_x + 0.5, "w=0", STYLE["text_dim"]),
    ]
    for wx, wtext, wcolor in winding_positions:
        ax.text(
            wx,
            scan_y - 0.35,
            wtext,
            color=wcolor,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Winding direction arrows on the contours
    # Outer contour: clockwise arrows
    for angle in [np.pi / 4, 3 * np.pi / 4, 5 * np.pi / 4, 7 * np.pi / 4]:
        x1 = outer_rx * np.cos(angle)
        y1 = outer_ry * np.sin(angle)
        # Tangent direction (CW: dx/dtheta = -rx*sin, dy/dtheta = ry*cos)
        # but for CW traversal we negate
        dx = outer_rx * np.sin(angle) * 0.25
        dy = -outer_ry * np.cos(angle) * 0.25
        ax.annotate(
            "",
            xy=(x1 + dx, y1 + dy),
            xytext=(x1 - dx, y1 - dy),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.12",
                color=STYLE["accent1"],
                lw=2.0,
            ),
            zorder=7,
        )

    # Inner contour: counter-clockwise arrows
    for angle in [np.pi / 4, 3 * np.pi / 4, 5 * np.pi / 4, 7 * np.pi / 4]:
        x1 = inner_rx * np.cos(angle)
        y1 = inner_ry * np.sin(angle)
        # CCW: standard tangent direction
        dx = -inner_rx * np.sin(angle) * 0.2
        dy = inner_ry * np.cos(angle) * 0.2
        ax.annotate(
            "",
            xy=(x1 + dx, y1 + dy),
            xytext=(x1 - dx, y1 - dy),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.12",
                color=STYLE["accent2"],
                lw=2.0,
            ),
            zorder=7,
        )

    # Title
    ax.text(
        0,
        2.8,
        "Scanline Rasterization \u2014 Non-Zero Winding Rule",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )

    # Annotation explaining filled vs hole
    ax.text(
        0,
        -2.55,
        "Filled where winding \u2260 0  |  Empty (hole) where winding = 0",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )

    save(fig, "ui/02-glyph-rasterization", "scanline_crossings.png")


def diagram_winding_direction():
    """Show the letter 'O' with CW outer and CCW inner contour arrows,
    illustrating how winding direction defines inside vs outside."""

    fig, ax = plt.subplots(figsize=(7, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-3.5, 3.5), ylim=(-3.5, 3.5), grid=False)
    ax.axis("off")

    theta = np.linspace(0, 2 * np.pi, 200)

    # Outer ellipse
    outer_r = 2.5
    outer_x = outer_r * np.cos(theta)
    outer_y = outer_r * np.sin(theta)

    # Inner ellipse
    inner_r = 1.3
    inner_x = inner_r * np.cos(theta)
    inner_y = inner_r * np.sin(theta)

    # Fill the ring between outer and inner
    # Outer fill
    ax.fill(outer_x, outer_y, color=STYLE["accent1"], alpha=0.15, zorder=1)
    # Punch the hole by overlaying inner with background color
    ax.fill(inner_x, inner_y, color=STYLE["bg"], zorder=2)

    # Draw contour outlines
    ax.plot(outer_x, outer_y, color=STYLE["accent1"], linewidth=2.5, zorder=3)
    ax.plot(inner_x, inner_y, color=STYLE["accent2"], linewidth=2.5, zorder=3)

    # Outer contour: clockwise arrows (8 evenly spaced)
    n_arrows = 8
    for i in range(n_arrows):
        angle = 2 * np.pi * i / n_arrows
        x1 = outer_r * np.cos(angle)
        y1 = outer_r * np.sin(angle)
        # CW tangent
        dx = outer_r * np.sin(angle) * 0.2
        dy = -outer_r * np.cos(angle) * 0.2
        ax.annotate(
            "",
            xy=(x1 + dx, y1 + dy),
            xytext=(x1 - dx, y1 - dy),
            arrowprops=dict(
                arrowstyle="->,head_width=0.25,head_length=0.15",
                color=STYLE["accent1"],
                lw=2.5,
            ),
            zorder=7,
        )

    # Inner contour: counter-clockwise arrows
    for i in range(n_arrows):
        angle = 2 * np.pi * i / n_arrows
        x1 = inner_r * np.cos(angle)
        y1 = inner_r * np.sin(angle)
        # CCW tangent
        dx = -inner_r * np.sin(angle) * 0.15
        dy = inner_r * np.cos(angle) * 0.15
        ax.annotate(
            "",
            xy=(x1 + dx, y1 + dy),
            xytext=(x1 - dx, y1 - dy),
            arrowprops=dict(
                arrowstyle="->,head_width=0.25,head_length=0.15",
                color=STYLE["accent2"],
                lw=2.5,
            ),
            zorder=7,
        )

    # Labels for contours
    ax.text(
        0,
        outer_r + 0.35,
        "Outer (CW, winding +1)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        0,
        inner_r + 0.25,
        "Inner (CCW, winding \u22121)",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Region annotations
    ax.text(
        (outer_r + inner_r) / 2 + 0.1,
        0,
        "filled\nw=1",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        0,
        0,
        "empty\nw=0",
        color=STYLE["text_dim"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Title
    ax.text(
        0,
        3.3,
        "Winding Direction \u2014 How TrueType Defines Inside/Outside",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )

    # Legend box at the bottom
    legend_items = [
        plt.Line2D(
            [0],
            [0],
            color=STYLE["accent1"],
            linewidth=2.5,
            label="Outer contour (CW)",
        ),
        plt.Line2D(
            [0],
            [0],
            color=STYLE["accent2"],
            linewidth=2.5,
            label="Inner contour (CCW)",
        ),
        mpatches.Patch(
            facecolor=STYLE["accent1"],
            alpha=0.3,
            edgecolor=STYLE["accent1"],
            label="Non-zero winding \u2192 filled",
        ),
        mpatches.Patch(
            facecolor=STYLE["bg"],
            edgecolor=STYLE["text_dim"],
            label="Zero winding \u2192 empty",
        ),
    ]
    ax.legend(
        handles=legend_items,
        loc="lower center",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        framealpha=0.9,
        ncol=2,
        bbox_to_anchor=(0.5, -0.05),
    )

    save(fig, "ui/02-glyph-rasterization", "winding_direction.png")


def diagram_antialiasing_comparison():
    """Show binary vs 4x4 supersampled rasterization of a diagonal edge on
    a pixel grid, with a magnified sub-pixel sampling inset."""

    fig, axes = plt.subplots(1, 3, figsize=(14, 6), width_ratios=[1, 1, 0.7])
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Anti-Aliasing \u2014 Binary vs 4\u00d74 Supersampling",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    grid_n = 6  # 6x6 pixel grid

    # Edge line: y = 1.1*x - 0.3.  Slope >1 ensures it cuts diagonally across
    # the grid and crosses several pixels with partial coverage.  "Inside" the
    # glyph is below the line (y < edge).
    def edge_line_y(x):
        return 1.1 * x - 0.3

    def binary_coverage(px, py):
        """Return 1 if pixel center is below the edge, else 0."""
        cx, cy = px + 0.5, py + 0.5
        return 1.0 if cy < edge_line_y(cx) else 0.0

    def supersample_coverage(px, py, n_sub=4):
        """Return fraction of sub-pixel samples below the edge."""
        count = 0
        for si in range(n_sub):
            for sj in range(n_sub):
                sx = px + (si + 0.5) / n_sub
                sy = py + (sj + 0.5) / n_sub
                if sy < edge_line_y(sx):
                    count += 1
        return count / (n_sub * n_sub)

    # Pre-parse the theme colours into [0,1] RGB tuples once
    text_rgb = (
        int(STYLE["text"][1:3], 16) / 255,
        int(STYLE["text"][3:5], 16) / 255,
        int(STYLE["text"][5:7], 16) / 255,
    )
    bg_rgb = (
        int(STYLE["bg"][1:3], 16) / 255,
        int(STYLE["bg"][3:5], 16) / 255,
        int(STYLE["bg"][5:7], 16) / 255,
    )

    # Shared edge-line x values
    ex = np.linspace(-0.1, grid_n + 0.1, 100)
    ey = edge_line_y(ex)

    # ---- Left panel: Binary rasterization --------------------------------
    ax = axes[0]
    setup_axes(ax, xlim=(-0.1, grid_n + 0.1), ylim=(-0.6, grid_n + 0.1), grid=False)
    ax.set_aspect("equal")
    ax.axis("off")

    for px in range(grid_n):
        for py in range(grid_n):
            cov = binary_coverage(px, py)
            color = STYLE["text"] if cov > 0 else STYLE["bg"]
            rect = mpatches.Rectangle(
                (px, py),
                1,
                1,
                facecolor=color,
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                zorder=2,
            )
            ax.add_patch(rect)

    ax.plot(ex, ey, color=STYLE["accent2"], linewidth=2.0, zorder=5)
    ax.text(
        grid_n / 2,
        -0.4,
        "Binary (on/off)",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
    )

    # ---- Middle panel: Supersampled rasterization ------------------------
    ax = axes[1]
    setup_axes(ax, xlim=(-0.1, grid_n + 0.1), ylim=(-0.6, grid_n + 0.1), grid=False)
    ax.set_aspect("equal")
    ax.axis("off")

    for px in range(grid_n):
        for py in range(grid_n):
            cov = supersample_coverage(px, py)
            cr = bg_rgb[0] + cov * (text_rgb[0] - bg_rgb[0])
            cg = bg_rgb[1] + cov * (text_rgb[1] - bg_rgb[1])
            cb = bg_rgb[2] + cov * (text_rgb[2] - bg_rgb[2])
            rect = mpatches.Rectangle(
                (px, py),
                1,
                1,
                facecolor=(cr, cg, cb),
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                zorder=2,
            )
            ax.add_patch(rect)

    ax.plot(ex, ey, color=STYLE["accent2"], linewidth=2.0, zorder=5)
    ax.text(
        grid_n / 2,
        -0.4,
        "4\u00d74 Supersampled",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
    )

    # Pick a pixel the edge actually crosses for the magnified view.
    # Pixel (3, 3): edge enters at y=1.1*3-0.3=3.0 and exits at y=1.1*4-0.3=4.1
    # so the edge slices through this pixel giving partial coverage.
    highlight_px, highlight_py = 3, 3
    highlight_rect = mpatches.Rectangle(
        (highlight_px, highlight_py),
        1,
        1,
        facecolor="none",
        edgecolor=STYLE["accent3"],
        linewidth=2.5,
        zorder=8,
    )
    ax.add_patch(highlight_rect)
    # Small "zoom" label
    ax.text(
        highlight_px + 1.15,
        highlight_py + 0.5,
        "\u2192",
        color=STYLE["accent3"],
        fontsize=14,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # ---- Right panel: Magnified sub-pixel view ---------------------------
    ax = axes[2]
    setup_axes(ax, xlim=(-0.3, 4.3), ylim=(-1.2, 4.5), grid=False)
    ax.set_aspect("equal")
    ax.axis("off")

    n_sub = 4
    hpx, hpy = highlight_px, highlight_py
    for si in range(n_sub):
        for sj in range(n_sub):
            sx = hpx + (si + 0.5) / n_sub
            sy = hpy + (sj + 0.5) / n_sub
            inside = sy < edge_line_y(sx)
            mx, my = si, sj
            rect = mpatches.Rectangle(
                (mx, my),
                1,
                1,
                facecolor=STYLE["accent1"] + "40" if inside else STYLE["bg"],
                edgecolor=STYLE["grid"],
                linewidth=0.8,
                zorder=2,
            )
            ax.add_patch(rect)
            ax.plot(
                mx + 0.5,
                my + 0.5,
                "o" if inside else "x",
                color=STYLE["accent1"] if inside else STYLE["text_dim"],
                markersize=7,
                markeredgewidth=1.5 if not inside else 0.5,
                zorder=5,
            )

    # Map the edge into magnified coordinates.
    # Original: x_orig = hpx + mx/4, y_orig = hpy + my/4
    # Edge condition: y_orig = 1.1*x_orig - 0.3
    #   hpy + my/4 = 1.1*(hpx + mx/4) - 0.3
    #   my = 1.1*mx + 4*(1.1*hpx - 0.3 - hpy)
    offset = 4 * (1.1 * hpx - 0.3 - hpy)  # = 4*(3.3-0.3-3) = 0.0
    mag_ex = np.linspace(-0.3, 4.3, 100)
    mag_ey = 1.1 * mag_ex + offset
    ax.plot(mag_ex, mag_ey, color=STYLE["accent2"], linewidth=2.0, zorder=6)

    # Green border around the magnified pixel
    border = mpatches.Rectangle(
        (0, 0),
        4,
        4,
        facecolor="none",
        edgecolor=STYLE["accent3"],
        linewidth=2.5,
        zorder=8,
    )
    ax.add_patch(border)

    # Coverage count
    count = sum(
        1
        for si in range(n_sub)
        for sj in range(n_sub)
        if (hpy + (sj + 0.5) / n_sub) < edge_line_y(hpx + (si + 0.5) / n_sub)
    )
    ax.text(
        2,
        -0.6,
        f"Magnified pixel\n{count}/16 samples inside\n= {count / 16:.0%} coverage",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
    )

    # Coverage legend at the bottom of the figure
    legend_items = [
        plt.Line2D(
            [0],
            [0],
            marker="o",
            color=STYLE["bg"],
            markerfacecolor=STYLE["accent1"],
            markersize=7,
            label="Sample inside glyph",
        ),
        plt.Line2D(
            [0],
            [0],
            marker="x",
            color=STYLE["bg"],
            markerfacecolor=STYLE["text_dim"],
            markeredgecolor=STYLE["text_dim"],
            markersize=7,
            label="Sample outside glyph",
        ),
        plt.Line2D(
            [0],
            [0],
            color=STYLE["accent2"],
            linewidth=2,
            label="Glyph edge",
        ),
    ]
    fig.legend(
        handles=legend_items,
        loc="lower center",
        ncol=3,
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        framealpha=0.9,
        bbox_to_anchor=(0.5, -0.01),
    )

    fig.tight_layout(rect=[0, 0.06, 1, 0.93])
    save(fig, "ui/02-glyph-rasterization", "antialiasing_comparison.png")


# ---------------------------------------------------------------------------
# UI Lesson 03 — Font Atlas Packing
# ---------------------------------------------------------------------------


def diagram_shelf_packing():
    """Show shelf packing step-by-step: glyphs placed left-to-right in rows,
    sorted by height (tallest first), with wasted space highlighted."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Shelf Packing — Height-Sorted Glyph Placement",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    atlas_w, atlas_h = 16, 12

    # Simulated glyph sizes — sorted by height descending
    glyphs_sorted = [
        (3, 5),
        (2, 5),
        (3, 5),
        (2, 5),
        (3, 5),
        (2, 5),
        (2, 4),
        (3, 4),
        (2, 4),
        (3, 4),
        (2, 4),
        (2, 4),
        (2, 3),
        (1, 3),
        (2, 3),
        (2, 3),
        (1, 3),
        (2, 3),
        (2, 3),
        (2, 2),
        (1, 2),
        (2, 2),
        (1, 2),
        (2, 2),
        (1, 2),
    ]

    # Unsorted (shuffled) version
    glyphs_unsorted = [
        (2, 3),
        (3, 5),
        (1, 2),
        (2, 4),
        (2, 5),
        (1, 3),
        (2, 2),
        (3, 4),
        (2, 3),
        (2, 5),
        (2, 4),
        (2, 3),
        (3, 5),
        (1, 2),
        (2, 4),
        (2, 2),
        (1, 3),
        (2, 5),
        (2, 3),
        (3, 4),
        (2, 2),
        (2, 4),
        (1, 3),
        (2, 3),
        (2, 5),
    ]

    colors_by_height = {
        5: STYLE["accent1"],
        4: STYLE["accent2"],
        3: STYLE["accent3"],
        2: STYLE["accent4"],
    }

    def pack_and_draw(ax, glyphs, title, padding=0):
        setup_axes(
            ax,
            xlim=(-0.5, atlas_w + 0.5),
            ylim=(-1.5, atlas_h + 0.5),
            grid=False,
            aspect="equal",
        )
        ax.axis("off")

        # Draw atlas border
        border = mpatches.Rectangle(
            (0, 0),
            atlas_w,
            atlas_h,
            facecolor=STYLE["surface"],
            edgecolor=STYLE["text_dim"],
            linewidth=1.5,
            zorder=1,
        )
        ax.add_patch(border)

        # Shelf pack
        cx, cy = 0, atlas_h
        row_h = 0
        total_used = 0
        rows = []

        for gw, gh in glyphs:
            pw = gw + padding
            ph = gh + padding
            if cx + pw > atlas_w:
                rows.append((cy, row_h))
                cy -= row_h
                cx = 0
                row_h = 0
            if cy - ph < 0:
                break

            color = colors_by_height.get(gh, STYLE["text_dim"])
            rect = mpatches.Rectangle(
                (cx, cy - gh),
                gw,
                gh,
                facecolor=color + "50",
                edgecolor=color,
                linewidth=1.0,
                zorder=3,
            )
            ax.add_patch(rect)
            total_used += gw * gh

            cx += pw
            if ph > row_h:
                row_h = ph

        if row_h > 0:
            rows.append((cy, row_h))

        # Draw shelf dividers
        for shelf_top, shelf_h in rows:
            shelf_bottom = shelf_top - shelf_h
            ax.plot(
                [0, atlas_w],
                [shelf_bottom, shelf_bottom],
                color=STYLE["warn"],
                linewidth=0.8,
                linestyle=":",
                zorder=2,
            )

        # Utilization
        total_area = atlas_w * atlas_h
        util = total_used / total_area * 100
        ax.set_title(
            f"{title}\n{util:.0f}% utilization",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            pad=8,
        )

    pack_and_draw(axes[0], glyphs_unsorted, "Unsorted (random order)")
    pack_and_draw(axes[1], glyphs_sorted, "Sorted by height (tallest first)")

    # Legend
    legend_items = [
        mpatches.Patch(
            facecolor=STYLE["accent1"] + "50",
            edgecolor=STYLE["accent1"],
            label="Tall glyphs (5px)",
        ),
        mpatches.Patch(
            facecolor=STYLE["accent2"] + "50",
            edgecolor=STYLE["accent2"],
            label="Medium glyphs (4px)",
        ),
        mpatches.Patch(
            facecolor=STYLE["accent3"] + "50",
            edgecolor=STYLE["accent3"],
            label="Short glyphs (3px)",
        ),
        mpatches.Patch(
            facecolor=STYLE["accent4"] + "50",
            edgecolor=STYLE["accent4"],
            label="Small glyphs (2px)",
        ),
    ]
    fig.legend(
        handles=legend_items,
        loc="lower center",
        ncol=4,
        fontsize=8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        framealpha=0.9,
        bbox_to_anchor=(0.5, -0.01),
    )

    fig.tight_layout(rect=[0, 0.06, 1, 0.93])
    save(fig, "ui/03-font-atlas", "shelf_packing.png")


def diagram_padding_bleed():
    """Show what happens with and without padding between adjacent glyphs
    in an atlas — bilinear filtering samples into the neighbor."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Atlas Padding — Preventing Texture Bleed",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    cell_size = 1.0

    def draw_texels(ax, grid, xlim, ylim, title):
        setup_axes(ax, xlim=xlim, ylim=ylim, grid=False, aspect="equal")
        ax.axis("off")

        rows = len(grid)
        cols = len(grid[0])
        for r in range(rows):
            for c in range(cols):
                val = grid[r][c]
                if val == 0:
                    color = STYLE["bg"]
                elif val == 1:
                    color = STYLE["accent1"]
                elif val == 2:
                    color = STYLE["accent2"]
                else:
                    color = STYLE["warn"]

                alpha = 0.7 if val > 0 else 0.2
                rect = mpatches.Rectangle(
                    (c * cell_size, (rows - 1 - r) * cell_size),
                    cell_size,
                    cell_size,
                    facecolor=color,
                    alpha=alpha,
                    edgecolor=STYLE["grid"],
                    linewidth=0.5,
                    zorder=2,
                )
                ax.add_patch(rect)

        ax.set_title(title, color=STYLE["text"], fontsize=10, fontweight="bold", pad=8)

    # Without padding: two glyph bitmaps adjacent
    # Glyph A (cyan, left) and Glyph B (orange, right) touching
    grid_no_pad = [
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 0, 0, 0, 0, 0, 0, 0],
    ]

    draw_texels(
        axes[0], grid_no_pad, (-0.5, 8.5), (-1.5, 5.5), "Without Padding — Bleed Risk"
    )

    # Draw the sampling circle at the boundary
    circle = plt.Circle(
        (3.5, 2.5),
        0.8,
        facecolor="none",
        edgecolor=STYLE["warn"],
        linewidth=2.0,
        linestyle="--",
        zorder=5,
    )
    axes[0].add_patch(circle)
    axes[0].text(
        3.5,
        0.5,
        "Bilinear filter\nsamples BOTH glyphs",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
    )

    # With padding: 1px gap between glyphs
    grid_padded = [
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0],
    ]

    draw_texels(
        axes[1],
        grid_padded,
        (-0.5, 9.5),
        (-1.5, 5.5),
        "With 1px Padding — Clean Sampling",
    )

    # Draw the sampling circle in the padded case
    circle2 = plt.Circle(
        (3.5, 2.5),
        0.8,
        facecolor="none",
        edgecolor=STYLE["accent3"],
        linewidth=2.0,
        linestyle="--",
        zorder=5,
    )
    axes[1].add_patch(circle2)
    axes[1].text(
        3.5,
        0.5,
        "Filter only samples\nglyph + empty padding",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
    )

    # Legend
    legend_items = [
        mpatches.Patch(facecolor=STYLE["accent1"], alpha=0.7, label="Glyph A texels"),
        mpatches.Patch(facecolor=STYLE["accent2"], alpha=0.7, label="Glyph B texels"),
        mpatches.Patch(
            facecolor=STYLE["bg"], edgecolor=STYLE["grid"], label="Empty (padding)"
        ),
    ]
    fig.legend(
        handles=legend_items,
        loc="lower center",
        ncol=3,
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        framealpha=0.9,
        bbox_to_anchor=(0.5, -0.01),
    )

    fig.tight_layout(rect=[0, 0.06, 1, 0.93])
    save(fig, "ui/03-font-atlas", "padding_bleed.png")


def diagram_uv_coordinates():
    """Show how pixel positions in the atlas map to normalized UV coordinates,
    with one glyph highlighted and the formula overlaid."""

    fig, ax = plt.subplots(figsize=(8, 8))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-2.5, 11), grid=False, aspect="equal")
    ax.axis("off")

    atlas_size = 8  # visual size for the atlas square

    # Draw atlas background
    atlas_rect = mpatches.Rectangle(
        (0, 0),
        atlas_size,
        atlas_size,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        zorder=1,
    )
    ax.add_patch(atlas_rect)

    # Draw some "packed glyphs" as colored rectangles
    packed = [
        (0.5, 5.5, 2.0, 2.0, "H", STYLE["accent1"]),
        (3.0, 5.5, 1.5, 2.0, "e", STYLE["accent1"]),
        (5.0, 5.5, 1.8, 2.0, "l", STYLE["accent1"]),
        (0.5, 3.0, 2.0, 2.0, "o", STYLE["accent1"]),
        (3.0, 3.0, 1.5, 2.0, "W", STYLE["accent1"]),
    ]

    for gx, gy, gw, gh, label, color in packed:
        r = mpatches.Rectangle(
            (gx, gy),
            gw,
            gh,
            facecolor=color + "30",
            edgecolor=color,
            linewidth=1.0,
            zorder=2,
        )
        ax.add_patch(r)
        ax.text(
            gx + gw / 2,
            gy + gh / 2,
            label,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
            zorder=3,
        )

    # Highlight one glyph with full annotation
    hx, hy, hw, hh = 5.0, 3.0, 2.0, 2.0
    highlight = mpatches.Rectangle(
        (hx, hy),
        hw,
        hh,
        facecolor=STYLE["accent2"] + "40",
        edgecolor=STYLE["accent2"],
        linewidth=2.5,
        zorder=4,
    )
    ax.add_patch(highlight)
    ax.text(
        hx + hw / 2,
        hy + hh / 2,
        "A",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        zorder=5,
    )

    # UV coordinate labels — top-left origin convention:
    #   u0 = x / W,  v0 = y / H,  u1 = (x+w) / W,  v1 = (y+h) / H
    # The glyph positions in the diagram use matplotlib coordinates where
    # y increases upward, but the UV formula treats y as increasing downward
    # from the top-left origin.  For the highlighted glyph at (hx, hy) the
    # atlas-space y equals hy (the diagram values are chosen so this holds).
    inv = 1.0 / atlas_size
    u0 = hx * inv
    v0 = hy * inv
    u1 = (hx + hw) * inv
    v1 = (hy + hh) * inv

    # Corner labels (display UVs directly, no double-flip)
    ax.text(
        hx - 0.1,
        hy + hh + 0.1,
        f"({u0:.2f}, {v0:.2f})",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="right",
        va="bottom",
    )
    ax.text(
        hx + hw + 0.1,
        hy - 0.1,
        f"({u1:.2f}, {v1:.2f})",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="top",
    )

    # Axis labels
    ax.text(
        atlas_size / 2,
        -0.3,
        "u (0.0 to 1.0)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="top",
    )
    ax.text(
        -0.3,
        atlas_size / 2,
        "v (0.0 to 1.0)",
        color=STYLE["text"],
        fontsize=10,
        ha="right",
        va="center",
        rotation=90,
    )

    # Origin and extent labels — top-left UV convention:
    # (0,0) at top-left, (1,0) at top-right, (0,1) at bottom-left
    ax.text(
        0,
        atlas_size + 0.15,
        "(0,0)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
    )
    ax.text(
        atlas_size,
        atlas_size + 0.15,
        "(1,0)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
    )
    ax.text(
        -0.15,
        0,
        "(0,1)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="right",
        va="center",
    )

    # Formula
    ax.text(
        atlas_size / 2,
        -1.5,
        "UV = pixel_position / atlas_dimension\n"
        "u0 = x / W    v0 = y / H    u1 = (x+w) / W    v1 = (y+h) / H",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="top",
        family="monospace",
        bbox=dict(
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            boxstyle="round,pad=0.4",
            alpha=0.9,
        ),
    )

    # Title
    ax.text(
        atlas_size / 2,
        atlas_size + 0.8,
        "UV Coordinate Mapping",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )

    save(fig, "ui/03-font-atlas", "uv_coordinates.png")


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
    for i, (line_text, lw) in enumerate(zip(lines, line_widths)):
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


def diagram_hot_active_state_machine():
    """Show the hot/active two-ID state machine from Casey Muratori's IMGUI
    talk.  Four states (none, hot, active, click) with labeled transitions
    showing mouse events and conditions."""

    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1, 11), ylim=(-1, 7), grid=False, aspect=None)
    ax.axis("off")

    # Title with vertical padding to avoid crowding content
    ax.set_title(
        "Hot / Active State Machine",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # State positions and colors
    states = {
        "NONE": (1.5, 4.0, STYLE["text_dim"]),
        "HOT": (5.0, 4.0, STYLE["accent1"]),
        "ACTIVE": (8.5, 4.0, STYLE["accent2"]),
        "CLICK!": (8.5, 1.0, STYLE["accent3"]),
    }

    # Draw state circles
    for name, (x, y, color) in states.items():
        circle = mpatches.Circle(
            (x, y),
            0.9,
            facecolor=color + "25",
            edgecolor=color,
            linewidth=2.0,
            zorder=3,
        )
        ax.add_patch(circle)
        ax.text(
            x,
            y,
            name,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=4,
        )

    # Descriptions below each state
    descs = {
        "NONE": "hot = NONE\nactive = NONE",
        "HOT": "hot = id\nactive = NONE",
        "ACTIVE": "hot = id\nactive = id",
        "CLICK!": "mouse released\nover active widget",
    }
    for name, (x, y, _color) in states.items():
        ax.text(
            x,
            y - 1.3,
            descs[name],
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="top",
            style="italic",
        )

    # Transition arrows
    arrow_style = dict(
        arrowstyle="->,head_width=0.25,head_length=0.15",
        lw=1.8,
    )

    # NONE -> HOT: mouse enters widget
    ax.annotate(
        "",
        xy=(4.1, 4.15),
        xytext=(2.4, 4.15),
        arrowprops=dict(**arrow_style, color=STYLE["accent1"]),
        zorder=5,
    )
    ax.text(
        3.25,
        4.55,
        "mouse enters\nwidget rect",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
    )

    # HOT -> NONE: mouse leaves widget
    ax.annotate(
        "",
        xy=(2.4, 3.85),
        xytext=(4.1, 3.85),
        arrowprops=dict(**arrow_style, color=STYLE["text_dim"]),
        zorder=5,
    )
    ax.text(
        3.25,
        3.35,
        "mouse leaves",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
    )

    # HOT -> ACTIVE: mouse pressed
    ax.annotate(
        "",
        xy=(7.6, 4.15),
        xytext=(5.9, 4.15),
        arrowprops=dict(**arrow_style, color=STYLE["accent2"]),
        zorder=5,
    )
    ax.text(
        6.75,
        4.55,
        "mouse button\npressed",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
    )

    # ACTIVE -> CLICK: mouse released while over widget
    ax.annotate(
        "",
        xy=(8.5, 1.9),
        xytext=(8.5, 3.1),
        arrowprops=dict(**arrow_style, color=STYLE["accent3"]),
        zorder=5,
    )
    ax.text(
        9.6,
        2.5,
        "mouse released\n(still over widget)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="left",
        va="center",
        fontweight="bold",
    )

    # ACTIVE -> NONE: mouse released outside widget (no click)
    ax.annotate(
        "",
        xy=(2.1, 3.3),
        xytext=(7.8, 3.3),
        arrowprops=dict(
            **arrow_style,
            color=STYLE["warn"],
            connectionstyle="arc3,rad=0.25",
        ),
        zorder=5,
    )
    ax.text(
        5.0,
        2.15,
        "mouse released outside\n(no click -- cancelled)",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="top",
        fontweight="bold",
    )

    # CLICK -> NONE: return to idle
    ax.annotate(
        "",
        xy=(1.5, 3.1),
        xytext=(7.65, 1.0),
        arrowprops=dict(
            **arrow_style,
            color=STYLE["text_dim"],
            connectionstyle="arc3,rad=-0.3",
        ),
        zorder=5,
    )
    ax.text(
        4.5,
        0.55,
        "next frame",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
    )

    save(fig, "ui/05-immediate-mode-basics", "hot_active_state_machine.png")


def diagram_declare_then_draw():
    """Show the immediate-mode frame loop: begin -> declare widgets ->
    end -> render.  Contrasts with retained-mode's create-once approach."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Retained Mode vs Immediate Mode",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    # ---- Left: Retained mode ----
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 6), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Retained Mode",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    # Show create-once, update approach
    retained_steps = [
        ("Create widget tree\n(once at startup)", STYLE["accent2"], 5.5),
        ("Store in\npersistent tree", STYLE["text_dim"], 4.0),
        ("Update properties\n(on change)", STYLE["accent2"], 2.5),
        ("Library manages\nrender + state", STYLE["text_dim"], 1.0),
    ]
    for label, color, y in retained_steps:
        rect = mpatches.FancyBboxPatch(
            (0.5, y - 0.4),
            5.0,
            0.8,
            boxstyle="round,pad=0.1",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.2,
        )
        ax.add_patch(rect)
        ax.text(
            3.0,
            y,
            label,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
        )

    # Down arrows between steps
    for i in range(len(retained_steps) - 1):
        y_from = retained_steps[i][2] - 0.4
        y_to = retained_steps[i + 1][2] + 0.4
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

    # ---- Right: Immediate mode ----
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 6), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Immediate Mode",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    # Show per-frame declare-then-draw loop
    imm_steps = [
        ("ctx_begin()\ninput + reset buffers", STYLE["accent1"], 5.5),
        ("Declare widgets\nlabel(), button(), ...", STYLE["accent3"], 4.0),
        ("ctx_end()\nfinalize hot/active", STYLE["accent1"], 2.5),
        ("Render\nvertices + indices", STYLE["accent4"], 1.0),
    ]
    for label, color, y in imm_steps:
        rect = mpatches.FancyBboxPatch(
            (0.5, y - 0.4),
            5.0,
            0.8,
            boxstyle="round,pad=0.1",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.2,
        )
        ax.add_patch(rect)
        ax.text(
            3.0,
            y,
            label,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
        )

    # Down arrows
    for i in range(len(imm_steps) - 1):
        y_from = imm_steps[i][2] - 0.4
        y_to = imm_steps[i + 1][2] + 0.4
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

    # Loop-back arrow from render to begin
    ax.annotate(
        "",
        xy=(5.7, 5.5),
        xytext=(5.7, 1.0),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["warn"],
            lw=1.5,
            connectionstyle="arc3,rad=-0.3",
        ),
    )
    ax.text(
        5.9,
        3.25,
        "every\nframe",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="center",
    )

    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "ui/05-immediate-mode-basics", "declare_then_draw.png")


def diagram_button_draw_data():
    """Show how a button generates draw data: a background rectangle quad
    using white_uv plus centered text quads using glyph UVs -- all in the
    same vertex/index buffer with a shared atlas texture."""

    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-1, 8), grid=False, aspect=None)
    ax.axis("off")

    # Title with vertical padding to avoid crowding content
    ax.set_title(
        "Button Draw Data: Background + Text in One Draw Call",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # ---- Button visualization (left side) ----
    # Background rect
    btn_x, btn_y, btn_w, btn_h = 0.5, 3.5, 4.5, 1.8
    btn_rect = mpatches.FancyBboxPatch(
        (btn_x, btn_y),
        btn_w,
        btn_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent4"] + "40",
        edgecolor=STYLE["accent4"],
        linewidth=2.0,
    )
    ax.add_patch(btn_rect)
    ax.text(
        btn_x + btn_w / 2,
        btn_y + btn_h / 2,
        "OK",
        color=STYLE["text"],
        fontsize=18,
        fontweight="bold",
        ha="center",
        va="center",
        family="monospace",
    )

    # Label the button parts
    ax.annotate(
        "background quad\n(white_uv region)",
        xy=(btn_x + btn_w, btn_y + btn_h - 0.1),
        xytext=(btn_x + btn_w + 0.8, btn_y + btn_h + 1.0),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent4"],
            lw=1.2,
        ),
        color=STYLE["accent4"],
        fontsize=8.5,
        fontweight="bold",
    )
    ax.annotate(
        "text quads\n(glyph UVs)",
        xy=(btn_x + btn_w / 2, btn_y + 0.3),
        xytext=(btn_x + btn_w + 0.8, btn_y - 0.8),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent1"],
            lw=1.2,
        ),
        color=STYLE["accent1"],
        fontsize=8.5,
        fontweight="bold",
    )

    # ---- Atlas texture visualization (right side) ----
    atlas_x, atlas_y = 7.0, 2.0
    atlas_w, atlas_h = 4.5, 4.5

    atlas_bg = mpatches.FancyBboxPatch(
        (atlas_x, atlas_y),
        atlas_w,
        atlas_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.0,
    )
    ax.add_patch(atlas_bg)
    ax.text(
        atlas_x + atlas_w / 2,
        atlas_y + atlas_h + 0.3,
        "Font Atlas Texture",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
    )

    # Glyph regions in the atlas
    glyphs = [
        ("A", atlas_x + 0.3, atlas_y + 3.2, 0.8, 1.0),
        ("B", atlas_x + 1.3, atlas_y + 3.2, 0.8, 1.0),
        ("O", atlas_x + 2.3, atlas_y + 3.2, 0.9, 1.0),
        ("K", atlas_x + 3.3, atlas_y + 3.2, 0.8, 1.0),
        ("...", atlas_x + 0.3, atlas_y + 1.8, 3.9, 1.0),
    ]
    for label, gx, gy, gw, gh in glyphs:
        glyph_rect = mpatches.FancyBboxPatch(
            (gx, gy),
            gw,
            gh,
            boxstyle="round,pad=0.02",
            facecolor=STYLE["accent1"] + "30",
            edgecolor=STYLE["accent1"],
            linewidth=0.8,
        )
        ax.add_patch(glyph_rect)
        ax.text(
            gx + gw / 2,
            gy + gh / 2,
            label,
            color=STYLE["text"],
            fontsize=9 if label != "..." else 12,
            ha="center",
            va="center",
            family="monospace",
        )

    # White pixel region
    wp_x = atlas_x + 0.3
    wp_y = atlas_y + 0.3
    wp_rect = mpatches.FancyBboxPatch(
        (wp_x, wp_y),
        1.0,
        1.0,
        boxstyle="round,pad=0.02",
        facecolor=STYLE["warn"] + "50",
        edgecolor=STYLE["warn"],
        linewidth=1.2,
    )
    ax.add_patch(wp_rect)
    ax.text(
        wp_x + 0.5,
        wp_y + 0.5,
        "white\npixel",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
    )

    # Arrows from button parts to atlas regions
    # Background -> white pixel
    ax.annotate(
        "",
        xy=(wp_x + 0.5, wp_y + 1.0),
        xytext=(btn_x + btn_w, btn_y + btn_h * 0.75),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["warn"],
            lw=1.5,
            connectionstyle="arc3,rad=-0.2",
            linestyle="--",
        ),
    )

    # Text -> glyph O and K
    ax.annotate(
        "",
        xy=(atlas_x + 2.75, atlas_y + 3.2),
        xytext=(btn_x + btn_w * 0.4, btn_y + btn_h * 0.25),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent1"],
            lw=1.5,
            connectionstyle="arc3,rad=-0.15",
            linestyle="--",
        ),
    )

    # ---- Vertex buffer visualization (bottom) ----
    vb_y = 0.2
    ax.text(
        6.0,
        vb_y + 0.8,
        "Vertex Buffer (shared format: pos, UV, color)",
        color=STYLE["text"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )

    buf_entries = [
        ("bg v0", STYLE["accent4"]),
        ("bg v1", STYLE["accent4"]),
        ("bg v2", STYLE["accent4"]),
        ("bg v3", STYLE["accent4"]),
        ("'O' v0", STYLE["accent1"]),
        ("'O' v1", STYLE["accent1"]),
        ("...", STYLE["text_dim"]),
        ("'K' v3", STYLE["accent1"]),
    ]
    cell_w = 1.3
    start_x = 6.0 - (len(buf_entries) * cell_w) / 2
    for i, (label, color) in enumerate(buf_entries):
        cx = start_x + i * cell_w
        rect = mpatches.FancyBboxPatch(
            (cx, vb_y - 0.3),
            cell_w - 0.1,
            0.6,
            boxstyle="round,pad=0.03",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=0.8,
        )
        ax.add_patch(rect)
        ax.text(
            cx + (cell_w - 0.1) / 2,
            vb_y,
            label,
            color=STYLE["text"],
            fontsize=6.5,
            ha="center",
            va="center",
            family="monospace",
        )

    save(fig, "ui/05-immediate-mode-basics", "button_draw_data.png")


def diagram_hit_testing():
    """Show how hit testing works: mouse position checked against widget
    bounding rectangles to determine the hot widget."""

    fig, ax = plt.subplots(figsize=(8, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10), ylim=(-0.5, 7), grid=False, aspect=None)
    ax.axis("off")

    # Title with vertical padding to avoid crowding content
    ax.set_title(
        "Hit Testing: Point-in-Rectangle",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # Draw three buttons
    buttons = [
        ("Start", 1.0, 4.5, 3.0, 1.2, STYLE["accent1"]),
        ("Options", 1.0, 2.8, 3.0, 1.2, STYLE["accent2"]),
        ("Quit", 1.0, 1.1, 3.0, 1.2, STYLE["text_dim"]),
    ]

    for label, bx, by, bw, bh, color in buttons:
        is_hit = label == "Options"  # Mouse is over Options
        rect = mpatches.FancyBboxPatch(
            (bx, by),
            bw,
            bh,
            boxstyle="round,pad=0.05",
            facecolor=color + ("40" if is_hit else "20"),
            edgecolor=color,
            linewidth=2.5 if is_hit else 1.2,
        )
        ax.add_patch(rect)
        ax.text(
            bx + bw / 2,
            by + bh / 2,
            label,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
        )
        if is_hit:
            ax.text(
                bx + bw + 0.3,
                by + bh / 2,
                "HOT",
                color=STYLE["accent1"],
                fontsize=10,
                fontweight="bold",
                ha="left",
                va="center",
            )

    # Mouse cursor
    mx, my = 2.5, 3.4
    ax.plot(
        mx,
        my,
        marker="x",
        markersize=14,
        color=STYLE["warn"],
        markeredgewidth=2.5,
        zorder=10,
    )
    ax.text(
        mx + 0.3,
        my + 0.3,
        "mouse (px, py)",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="left",
    )

    # Hit test formula
    ax.text(
        6.5,
        4.8,
        "Hit test passes when:",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="left",
    )
    conditions = [
        "px >= rect.x",
        "px <  rect.x + rect.w",
        "py >= rect.y",
        "py <  rect.y + rect.h",
    ]
    for i, cond in enumerate(conditions):
        ax.text(
            6.8,
            4.2 - i * 0.45,
            cond,
            color=STYLE["accent1"],
            fontsize=9,
            ha="left",
            family="monospace",
        )

    # Result annotation
    ax.text(
        6.5,
        2.0,
        "Last widget passing\nhit test becomes hot",
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="left",
        va="top",
        style="italic",
    )

    save(fig, "ui/05-immediate-mode-basics", "hit_testing.png")
