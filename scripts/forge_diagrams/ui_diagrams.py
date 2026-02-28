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

    for ax, (title, checked, border_color, box_fill) in zip(axes, states):
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

    for ax, (title, left_lbl, right_lbl, mark_lbl, color) in zip(axes, labels):
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
        axes, panels
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

    for ax, (label, t_pos, label_color, thumb_color) in zip(axes, states):
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
        for _i, (entry, (_, cx, color)) in enumerate(zip(entries, cols)):
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


def diagram_focus_state_machine():
    """Two-state focus machine (Unfocused / Focused) with a transient key-event
    processing phase shown as a dashed node, reflecting the single focused-ID
    model in forge_ui_ctx.h."""

    fig, ax = plt.subplots(figsize=(10, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-0.5, 6), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Focus State Machine",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Persistent state positions and colors
    states = {
        "UNFOCUSED": (1.5, 3.0, STYLE["text_dim"]),
        "FOCUSED": (5.5, 3.0, STYLE["accent1"]),
    }

    # Draw persistent state circles (solid border)
    for name, (x, y, color) in states.items():
        circle = mpatches.Circle(
            (x, y),
            1.0,
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
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Transient key-event node (dashed border to signal non-persistent)
    ke_x, ke_y, ke_color = 9.5, 3.0, STYLE["accent2"]
    ke_circle = mpatches.Circle(
        (ke_x, ke_y),
        1.0,
        facecolor=ke_color + "15",
        edgecolor=ke_color,
        linewidth=1.5,
        linestyle="--",
        zorder=3,
    )
    ax.add_patch(ke_circle)
    ax.text(
        ke_x,
        ke_y + 0.15,
        "KEY EVENT",
        color=STYLE["text"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        zorder=4,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        ke_x,
        ke_y - 0.25,
        "(transient)",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        zorder=4,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Descriptions below each node
    descs = {
        "UNFOCUSED": "focused = NONE\nno keyboard input",
        "FOCUSED": "focused = id\ncursor visible",
    }
    for name, (x, y, _) in states.items():
        ax.text(
            x,
            y - 1.5,
            descs[name],
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="top",
            style="italic",
        )
    ax.text(
        ke_x,
        ke_y - 1.5,
        "momentary key processing\nwhile focused (same frame)",
        color=STYLE["text_dim"],
        fontsize=7.5,
        ha="center",
        va="top",
        style="italic",
    )

    arrow_style = dict(
        arrowstyle="->,head_width=0.25,head_length=0.15",
        lw=1.8,
    )

    # UNFOCUSED -> FOCUSED: click on text input
    ax.annotate(
        "",
        xy=(4.5, 3.2),
        xytext=(2.5, 3.2),
        arrowprops=dict(**arrow_style, color=STYLE["accent1"]),
        zorder=5,
    )
    ax.text(
        3.5,
        3.7,
        "click on\ntext input",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # FOCUSED -> UNFOCUSED: click outside / Escape
    ax.annotate(
        "",
        xy=(2.5, 2.8),
        xytext=(4.5, 2.8),
        arrowprops=dict(**arrow_style, color=STYLE["text_dim"]),
        zorder=5,
    )
    ax.text(
        3.5,
        2.25,
        "click outside\nor Escape",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # FOCUSED -> KEY EVENT: key arrives while focused
    ax.annotate(
        "",
        xy=(8.5, 3.2),
        xytext=(6.5, 3.2),
        arrowprops=dict(**arrow_style, color=STYLE["accent2"]),
        zorder=5,
    )
    ax.text(
        7.5,
        3.7,
        "key event\n(text/arrow/del)",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # KEY EVENT -> FOCUSED: processed, returns to focused
    ax.annotate(
        "",
        xy=(6.5, 2.8),
        xytext=(8.5, 2.8),
        arrowprops=dict(**arrow_style, color=STYLE["accent3"]),
        zorder=5,
    )
    ax.text(
        7.5,
        2.25,
        "input processed\n(returns immediately)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # KEY EVENT -> UNFOCUSED: Escape key
    ax.annotate(
        "",
        xy=(2.2, 4.0),
        xytext=(9.2, 4.0),
        arrowprops=dict(
            **arrow_style,
            color=STYLE["warn"],
            connectionstyle="arc3,rad=0.35",
        ),
        zorder=5,
    )
    ax.text(
        5.5,
        5.3,
        "Escape",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "focus_state_machine.png")


def diagram_text_input_anatomy():
    """Labeled parts of a text input widget: background rect, text baseline,
    cursor bar, left padding, with annotations for focused vs unfocused."""

    fig, axes = plt.subplots(1, 2, figsize=(11, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Text Input Anatomy",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    titles = ["Unfocused", "Focused"]
    border_colors = [STYLE["text_dim"] + "40", STYLE["accent1"]]
    bg_colors = [STYLE["surface"], STYLE["surface"]]
    show_cursor = [False, True]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 12), ylim=(-1, 5), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(
            titles[idx],
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            pad=8,
        )

        # Background rect
        bg = mpatches.FancyBboxPatch(
            (0, 1.0),
            11,
            2.5,
            boxstyle="round,pad=0.08",
            facecolor=bg_colors[idx],
            edgecolor=border_colors[idx],
            linewidth=2.0 if idx == 1 else 1.0,
            zorder=2,
        )
        ax.add_patch(bg)

        # Text "Hello"
        text_x = 0.8
        baseline_y = 2.0
        letters = list("Hello")
        widths = [1.2, 1.0, 0.6, 0.6, 1.2]
        pen_x = text_x
        for li, letter in enumerate(letters):
            ax.text(
                pen_x,
                baseline_y,
                letter,
                color=STYLE["text"],
                fontsize=18,
                fontweight="bold",
                ha="left",
                va="baseline",
                zorder=3,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )
            pen_x += widths[li]

        # Annotations
        # Left padding
        ax.annotate(
            "",
            xy=(0.8, 0.6),
            xytext=(0, 0.6),
            arrowprops=dict(
                arrowstyle="<->",
                color=STYLE["warn"],
                lw=1.2,
            ),
        )
        ax.text(
            0.4,
            0.15,
            "padding",
            color=STYLE["warn"],
            fontsize=7,
            ha="center",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Baseline indicator
        ax.plot(
            [0.5, 10.5],
            [baseline_y, baseline_y],
            color=STYLE["accent3"] + "60",
            linewidth=0.8,
            linestyle="--",
            zorder=2,
        )
        ax.text(
            10.7,
            baseline_y,
            "baseline",
            color=STYLE["accent3"],
            fontsize=7,
            ha="left",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Cursor bar (focused only)
        if show_cursor[idx]:
            cursor_x = pen_x + 0.1
            cursor_rect = mpatches.Rectangle(
                (cursor_x, 1.3),
                0.15,
                2.0,
                facecolor=STYLE["accent1"],
                zorder=4,
            )
            ax.add_patch(cursor_rect)
            ax.annotate(
                "cursor bar\n(2px wide)",
                xy=(cursor_x, 3.5),
                xytext=(cursor_x + 2.0, 4.3),
                color=STYLE["accent1"],
                fontsize=7.5,
                fontweight="bold",
                ha="center",
                arrowprops=dict(
                    arrowstyle="->",
                    color=STYLE["accent1"],
                    lw=1.2,
                ),
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

        # Border annotation (focused only)
        if idx == 1:
            ax.annotate(
                "accent border",
                xy=(0, 3.5),
                xytext=(-0.3, 4.5),
                color=STYLE["accent1"],
                fontsize=7.5,
                fontweight="bold",
                ha="center",
                arrowprops=dict(
                    arrowstyle="->",
                    color=STYLE["accent1"],
                    lw=1.0,
                ),
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )
        else:
            ax.text(
                5.5,
                4.3,
                "dark background\nsubtle border",
                color=STYLE["text_dim"],
                fontsize=7.5,
                ha="center",
                va="bottom",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "text_input_anatomy.png")


def diagram_cursor_positioning():
    """The word 'Hello' with cursor bar drawn after the first 'l'
    (between the two l's), showing pen_x = sum(widths[:3]) for 'Hel'."""

    fig, ax = plt.subplots(figsize=(9, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 13), ylim=(-1.5, 5), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Cursor Positioning via Substring Measurement",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # Draw text input background
    bg = mpatches.FancyBboxPatch(
        (0, 0.5),
        12,
        2.8,
        boxstyle="round,pad=0.08",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
        zorder=2,
    )
    ax.add_patch(bg)

    # Draw letters with individual bounding boxes
    padding_x = 0.6
    baseline_y = 1.5
    letters = list("Hello")
    widths = [1.6, 1.3, 0.8, 0.8, 1.5]
    pen_x = padding_x

    for i, (letter, w) in enumerate(zip(letters, widths)):
        color = STYLE["accent1"] if i < 3 else STYLE["text_dim"]
        ax.text(
            pen_x + 0.1,
            baseline_y,
            letter,
            color=color,
            fontsize=22,
            fontweight="bold",
            ha="left",
            va="baseline",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Glyph advance box
        box = mpatches.Rectangle(
            (pen_x, 0.8),
            w,
            2.2,
            facecolor="none",
            edgecolor=color + "40",
            linewidth=0.8,
            linestyle="--",
            zorder=3,
        )
        ax.add_patch(box)
        pen_x += w

    # Cursor position (after first 'l', between the two l's)
    cursor_x = padding_x + sum(widths[:3])
    cursor_rect = mpatches.Rectangle(
        (cursor_x, 0.7),
        0.12,
        2.4,
        facecolor=STYLE["accent1"],
        zorder=5,
    )
    ax.add_patch(cursor_rect)

    # Measurement bracket under "Hel"
    meas_start = padding_x
    meas_end = cursor_x
    bracket_y = 0.2
    ax.plot(
        [meas_start, meas_end],
        [bracket_y, bracket_y],
        color=STYLE["accent2"],
        linewidth=2.0,
    )
    ax.plot(
        [meas_start, meas_start],
        [bracket_y - 0.2, bracket_y + 0.2],
        color=STYLE["accent2"],
        linewidth=2.0,
    )
    ax.plot(
        [meas_end, meas_end],
        [bracket_y - 0.2, bracket_y + 0.2],
        color=STYLE["accent2"],
        linewidth=2.0,
    )

    ax.text(
        (meas_start + meas_end) / 2,
        bracket_y - 0.4,
        'text_measure("Hel").width',
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Formula annotation
    ax.text(
        6.5,
        4.2,
        "cursor_x  =  padding  +  text_measure( buffer[0..cursor] ).width",
        color=STYLE["text"],
        fontsize=8.5,
        ha="center",
        va="bottom",
        fontfamily="monospace",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Cursor index annotation
    ax.annotate(
        "cursor = 3",
        xy=(cursor_x, 3.3),
        xytext=(cursor_x + 2.5, 4.0),
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent1"],
            lw=1.2,
        ),
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "cursor_positioning.png")


def diagram_character_insertion():
    """Before/after byte-array diagram showing 'Helo' becoming 'Hello' with
    splice at index 3 — trailing bytes shift right then 'l' written at cursor."""

    fig, axes = plt.subplots(2, 1, figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Character Insertion (splice at cursor)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.98,
    )

    labels = ["Before:  cursor = 3,  insert 'l'", "After:   cursor = 4"]
    before_chars = list("Helo") + ["\\0"]
    after_chars = list("Hello") + ["\\0"]
    char_sets = [before_chars, after_chars]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(-1.5, 12), ylim=(-1, 3), grid=False, aspect=None)
        ax.axis("off")

        chars = char_sets[idx]
        cell_w = 1.5
        start_x = 0.5
        y = 0.5

        ax.text(
            -1.3,
            1.3,
            labels[idx],
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="left",
            va="center",
        )

        for i, ch in enumerate(chars):
            x = start_x + i * cell_w

            # Highlight the inserted/affected cells
            if idx == 0 and i == 3:
                # Cursor position (before insertion)
                fc = STYLE["accent2"] + "30"
                ec = STYLE["accent2"]
            elif idx == 1 and i == 3:
                # Newly inserted character
                fc = STYLE["accent1"] + "30"
                ec = STYLE["accent1"]
            elif idx == 1 and i == 4:
                # Shifted character
                fc = STYLE["accent2"] + "20"
                ec = STYLE["accent2"] + "80"
            else:
                fc = STYLE["surface"]
                ec = STYLE["text_dim"] + "60"

            cell = mpatches.FancyBboxPatch(
                (x, y),
                cell_w - 0.15,
                1.2,
                boxstyle="round,pad=0.05",
                facecolor=fc,
                edgecolor=ec,
                linewidth=1.5,
                zorder=3,
            )
            ax.add_patch(cell)

            # Character label
            ax.text(
                x + (cell_w - 0.15) / 2,
                y + 0.6,
                ch,
                color=STYLE["text"],
                fontsize=14,
                fontweight="bold",
                ha="center",
                va="center",
                fontfamily="monospace",
                zorder=4,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

            # Index label below
            ax.text(
                x + (cell_w - 0.15) / 2,
                y - 0.3,
                str(i),
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                va="top",
            )

        # Cursor indicator
        if idx == 0:
            cx = start_x + 3 * cell_w - 0.1
            ax.plot(
                [cx, cx],
                [y - 0.1, y + 1.3],
                color=STYLE["accent1"],
                linewidth=2.5,
                zorder=5,
            )
            ax.text(
                cx,
                y + 1.6,
                "cursor",
                color=STYLE["accent1"],
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="bottom",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )
        else:
            cx = start_x + 4 * cell_w - 0.1
            ax.plot(
                [cx, cx],
                [y - 0.1, y + 1.3],
                color=STYLE["accent1"],
                linewidth=2.5,
                zorder=5,
            )

    # Arrow between panels showing the operation
    axes[0].annotate(
        "1. shift 'o','\\0' right\n2. write 'l' at [3]",
        xy=(8.5, -0.5),
        xytext=(10, 0.5),
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "character_insertion.png")


def diagram_character_deletion():
    """Side-by-side Backspace vs Delete on 'Hello' with cursor at index 3,
    showing which byte is removed and how trailing bytes shift left."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Character Deletion: Backspace vs Delete",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.98,
    )

    titles = ["Backspace (cursor = 3)", "Delete (cursor = 3)"]
    # Backspace removes char BEFORE cursor (index 2 = 'l'), result: "Helo"
    # Delete removes char AT cursor (index 3 = 'l'), result: "Helo"
    removed_indices = [2, 3]
    result_texts = ["Helo", "Helo"]
    result_cursors = [2, 3]
    descriptions = [
        "removes byte BEFORE cursor\ncursor moves left by 1",
        "removes byte AT cursor\ncursor stays at same index",
    ]

    before_chars = list("Hello") + ["\\0"]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 10), ylim=(-3.5, 4), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(
            titles[idx],
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            pad=8,
        )

        cell_w = 1.3
        start_x = 0.5

        # Before row
        y_before = 2.0
        ax.text(
            -0.3,
            y_before + 0.5,
            "Before:",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="center",
        )

        for i, ch in enumerate(before_chars):
            x = start_x + i * cell_w
            removed = i == removed_indices[idx]
            fc = STYLE["accent2"] + "35" if removed else STYLE["surface"]
            ec = STYLE["accent2"] if removed else STYLE["text_dim"] + "60"

            cell = mpatches.FancyBboxPatch(
                (x, y_before),
                cell_w - 0.1,
                1.0,
                boxstyle="round,pad=0.04",
                facecolor=fc,
                edgecolor=ec,
                linewidth=1.5 if removed else 1.0,
                zorder=3,
            )
            ax.add_patch(cell)
            ax.text(
                x + (cell_w - 0.1) / 2,
                y_before + 0.5,
                ch,
                color=STYLE["text"] if not removed else STYLE["accent2"],
                fontsize=12,
                fontweight="bold",
                ha="center",
                va="center",
                fontfamily="monospace",
                zorder=4,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

        # Cursor line (before)
        cx_before = start_x + 3 * cell_w - 0.08
        ax.plot(
            [cx_before, cx_before],
            [y_before - 0.1, y_before + 1.1],
            color=STYLE["accent1"],
            linewidth=2.5,
            zorder=5,
        )

        # After row
        y_after = -0.5
        ax.text(
            -0.3,
            y_after + 0.5,
            "After:",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="center",
        )

        after_chars = list(result_texts[idx]) + ["\\0"]
        for i, ch in enumerate(after_chars):
            x = start_x + i * cell_w
            fc = STYLE["surface"]
            ec = STYLE["text_dim"] + "60"
            cell = mpatches.FancyBboxPatch(
                (x, y_after),
                cell_w - 0.1,
                1.0,
                boxstyle="round,pad=0.04",
                facecolor=fc,
                edgecolor=ec,
                linewidth=1.0,
                zorder=3,
            )
            ax.add_patch(cell)
            ax.text(
                x + (cell_w - 0.1) / 2,
                y_after + 0.5,
                ch,
                color=STYLE["text"],
                fontsize=12,
                fontweight="bold",
                ha="center",
                va="center",
                fontfamily="monospace",
                zorder=4,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

        # Cursor line (after)
        cx_after = start_x + result_cursors[idx] * cell_w - 0.08
        ax.plot(
            [cx_after, cx_after],
            [y_after - 0.1, y_after + 1.1],
            color=STYLE["accent1"],
            linewidth=2.5,
            zorder=5,
        )

        # Arrow from removed cell
        rx = start_x + removed_indices[idx] * cell_w + (cell_w - 0.1) / 2
        ax.annotate(
            "",
            xy=(rx, y_before - 0.15),
            xytext=(rx, y_before - 0.05),
            arrowprops=dict(
                arrowstyle="-",
                color=STYLE["accent2"],
                lw=2.0,
            ),
        )
        ax.text(
            rx,
            y_before - 0.35,
            "X",
            color=STYLE["accent2"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Description
        ax.text(
            start_x + 2.5 * cell_w,
            y_after - 0.8,
            descriptions[idx],
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
            style="italic",
        )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "character_deletion.png")


def diagram_keyboard_input_flow():
    """Vertical data-flow diagram showing simulated key events feeding into
    ForgeUiContext begin, then flowing to the focused ForgeUiTextInputState
    through the text input widget function."""

    fig, ax = plt.subplots(figsize=(7, 8))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-2, 9), ylim=(-1, 11), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Keyboard Input Flow",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Flow boxes from top to bottom
    boxes = [
        (
            3.5,
            9.5,
            "Simulated Key Events",
            STYLE["warn"],
            "text_input, key_backspace,\nkey_left, key_right, ...",
        ),
        (
            3.5,
            7.5,
            "forge_ui_ctx_set_keyboard()",
            STYLE["accent1"],
            "stores keyboard state\nin ForgeUiContext",
        ),
        (
            3.5,
            5.5,
            "forge_ui_ctx_text_input()",
            STYLE["accent2"],
            "if focused == id:\n  process keyboard input",
        ),
        (
            3.5,
            3.5,
            "ForgeUiTextInputState",
            STYLE["accent3"],
            "buffer modified, cursor\nupdated, length changed",
        ),
        (
            3.5,
            1.5,
            "Draw Data (vertices/indices)",
            STYLE["accent4"],
            "background + text quads\n+ cursor bar",
        ),
    ]

    box_w = 5.5
    box_h = 1.3

    for cx, cy, title, color, desc in boxes:
        rect = mpatches.FancyBboxPatch(
            (cx - box_w / 2, cy - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.12",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.5,
            zorder=3,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy + 0.15,
            title,
            color=STYLE["text"],
            fontsize=9.5,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )
        ax.text(
            cx,
            cy - 0.35,
            desc,
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="center",
            style="italic",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Arrows between boxes
    arrow_style = dict(
        arrowstyle="->,head_width=0.2,head_length=0.12",
        lw=2.0,
    )

    arrow_pairs = [
        (9.5, 7.5, STYLE["accent1"]),
        (7.5, 5.5, STYLE["accent2"]),
        (5.5, 3.5, STYLE["accent3"]),
        (3.5, 1.5, STYLE["accent4"]),
    ]

    for from_y, to_y, color in arrow_pairs:
        ax.annotate(
            "",
            xy=(3.5, to_y + box_h / 2 + 0.05),
            xytext=(3.5, from_y - box_h / 2 - 0.05),
            arrowprops=dict(**arrow_style, color=color),
            zorder=5,
        )

    # Side annotation: "only if focused == id"
    ax.annotate(
        "only runs if\nfocused == id",
        xy=(6.25, 5.5),
        xytext=(8.0, 5.5),
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent2"],
            lw=1.0,
        ),
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "keyboard_input_flow.png")


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

    for _i, (h, label, color) in enumerate(zip(widget_heights, labels, colors)):
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
            for _i, (h, label, color) in enumerate(zip(heights, labels, colors)):
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
            for _i, (w, label, color) in enumerate(zip(widths, labels, colors)):
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

    for ax, (title, entries, depth) in zip(axes, states):
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
