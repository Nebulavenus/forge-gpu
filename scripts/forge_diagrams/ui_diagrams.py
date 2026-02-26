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
