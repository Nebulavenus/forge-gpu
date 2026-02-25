"""Diagram functions for UI lessons (lessons/ui/).

Each function generates a single PNG diagram and saves it to the lesson's
assets/ directory.  Register new diagrams in __main__.py after adding them
here.

Requires: pip install numpy matplotlib
"""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

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
