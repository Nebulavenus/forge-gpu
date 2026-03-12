"""Diagrams for math/04."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — mip_chain.png
# ---------------------------------------------------------------------------


def diagram_mip_chain():
    """Mip chain showing progressively halved textures with memory cost."""
    fig = plt.figure(figsize=(10, 4), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    # Mip levels: size halves each step
    base = 128  # visual size in plot units for level 0
    levels = 9  # for a 256x256 texture
    gap = 12

    x = 0
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"], STYLE["accent4"]]

    for level in range(levels):
        size = max(base >> level, 2)  # clamp visual size
        color = colors[level % len(colors)]

        # Checkerboard fill for each mip level
        checks = max(1, min(4, size // 8))
        csize = size / checks
        for ci in range(checks):
            for cj in range(checks):
                shade = color if (ci + cj) % 2 == 0 else STYLE["surface"]
                r = Rectangle(
                    (x + ci * csize, -size / 2 + cj * csize),
                    csize,
                    csize,
                    facecolor=shade,
                    edgecolor=STYLE["grid"],
                    linewidth=0.3,
                    alpha=0.6 if shade == color else 0.3,
                    zorder=1,
                )
                ax.add_patch(r)

        # Border
        border = Rectangle(
            (x, -size / 2),
            size,
            size,
            fill=False,
            edgecolor=color,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(border)

        # Label: level number and dimensions
        tex_size = 256 >> level
        if tex_size < 1:
            tex_size = 1
        label = f"L{level}\n{tex_size}"
        ax.text(
            x + size / 2,
            -size / 2 - 8,
            label,
            color=STYLE["text"],
            fontsize=7 if level < 6 else 6,
            ha="center",
            va="top",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Arrow to next level
        if level < levels - 1:
            ax.annotate(
                "",
                xy=(x + size + gap * 0.3, 0),
                xytext=(x + size + 2, 0),
                arrowprops={
                    "arrowstyle": "->",
                    "color": STYLE["text_dim"],
                    "lw": 1,
                },
            )
            ax.text(
                x + size + gap * 0.5,
                5,
                "\u00f72",
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="bottom",
            )

        x += size + gap

    # Memory cost annotation
    ax.text(
        x / 2,
        base / 2 + 15,
        "Each level = \u00bc the texels of the previous  |  Total = ~1.33\u00d7 base  (+33% memory)",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlim(-10, x + 5)
    ax.set_ylim(-base / 2 - 30, base / 2 + 30)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Mip Chain: 256\u00d7256 Texture (9 Levels)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/04-mipmaps-and-lod", "mip_chain.png")


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — trilinear_interpolation.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — trilinear_interpolation.png
# ---------------------------------------------------------------------------


def diagram_trilinear_interpolation():
    """Trilinear interpolation: two bilinear samples blended by LOD fraction."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # --- Left panel: mip level N (bilinear) ---
    ax1 = fig.add_subplot(131)
    setup_axes(ax1, xlim=(-0.4, 1.6), ylim=(-0.4, 1.6), grid=False)

    # Grid cell
    rect1 = Rectangle(
        (0, 0),
        1,
        1,
        fill=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=1,
    )
    ax1.add_patch(rect1)

    # Corner values
    corners1 = {
        (0, 0): ("c00", STYLE["accent1"]),
        (1, 0): ("c10", STYLE["accent2"]),
        (0, 1): ("c01", STYLE["accent4"]),
        (1, 1): ("c11", STYLE["accent3"]),
    }
    for (cx, cy), (label, color) in corners1.items():
        ax1.plot(cx, cy, "o", color=color, markersize=10, zorder=5)
        ox = -0.22 if cx == 0 else 0.08
        oy = -0.15 if cy == 0 else 0.08
        ax1.text(
            cx + ox,
            cy + oy,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Sample point
    tx, ty = 0.4, 0.6
    ax1.plot(tx, ty, "*", color=STYLE["warn"], markersize=16, zorder=6)

    # Result
    ax1.text(
        0.5,
        -0.28,
        "bilerp\u2081",
        color=STYLE["accent1"],
        fontsize=11,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax1.set_title(
        "Mip Level N",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
    )

    # --- Center panel: mip level N+1 (bilinear) ---
    ax2 = fig.add_subplot(132)
    setup_axes(ax2, xlim=(-0.4, 1.6), ylim=(-0.4, 1.6), grid=False)

    rect2 = Rectangle(
        (0, 0),
        1,
        1,
        fill=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=1,
    )
    ax2.add_patch(rect2)

    corners2 = {
        (0, 0): ("c00", STYLE["accent1"]),
        (1, 0): ("c10", STYLE["accent2"]),
        (0, 1): ("c01", STYLE["accent4"]),
        (1, 1): ("c11", STYLE["accent3"]),
    }
    for (cx, cy), (label, color) in corners2.items():
        ax2.plot(cx, cy, "o", color=color, markersize=10, zorder=5)
        ox = -0.22 if cx == 0 else 0.08
        oy = -0.15 if cy == 0 else 0.08
        ax2.text(
            cx + ox,
            cy + oy,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax2.plot(tx, ty, "*", color=STYLE["warn"], markersize=16, zorder=6)

    ax2.text(
        0.5,
        -0.28,
        "bilerp\u2082",
        color=STYLE["accent2"],
        fontsize=11,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax2.set_title(
        "Mip Level N+1",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
    )

    # --- Right panel: lerp result ---
    ax3 = fig.add_subplot(133)
    ax3.set_facecolor(STYLE["bg"])
    ax3.set_xlim(0, 1)
    ax3.set_ylim(-0.5, 1.5)
    ax3.set_xticks([])
    ax3.set_yticks([])
    for spine in ax3.spines.values():
        spine.set_visible(False)

    # Vertical blend bar
    bar_x = 0.3
    bar_w = 0.4
    n_steps = 50
    for i in range(n_steps):
        t = i / n_steps
        y0 = t
        h = 1.0 / n_steps
        # Blend from accent1 to accent2
        c1 = np.array([0x4F, 0xC3, 0xF7]) / 255  # accent1 RGB
        c2 = np.array([0xFF, 0x70, 0x43]) / 255  # accent2 RGB
        blended_c = c1 * (1 - t) + c2 * t
        r = Rectangle(
            (bar_x, y0),
            bar_w,
            h,
            facecolor=blended_c,
            edgecolor="none",
            zorder=1,
        )
        ax3.add_patch(r)

    # Border
    bar_border = Rectangle(
        (bar_x, 0),
        bar_w,
        1,
        fill=False,
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        zorder=2,
    )
    ax3.add_patch(bar_border)

    # Labels
    ax3.text(
        bar_x + bar_w / 2,
        -0.08,
        "bilerp\u2081",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        fontweight="bold",
    )
    ax3.text(
        bar_x + bar_w / 2,
        1.08,
        "bilerp\u2082",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        fontweight="bold",
    )

    # LOD fraction marker
    frac = 0.3
    ax3.plot(
        [bar_x - 0.05, bar_x + bar_w + 0.05],
        [frac, frac],
        "-",
        color=STYLE["warn"],
        lw=2.5,
        zorder=3,
    )
    ax3.plot(bar_x + bar_w / 2, frac, "*", color=STYLE["warn"], markersize=14, zorder=4)
    ax3.text(
        bar_x + bar_w + 0.08,
        frac,
        f"frac = {frac}",
        color=STYLE["warn"],
        fontsize=10,
        va="center",
        fontweight="bold",
    )

    ax3.set_title(
        "Lerp by LOD\nFraction",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
    )

    # Arrows connecting panels
    fig.text(
        0.355,
        0.35,
        "\u2192",
        color=STYLE["text_dim"],
        fontsize=20,
        ha="center",
        va="center",
        fontweight="bold",
    )
    fig.text(
        0.655,
        0.35,
        "\u2192",
        color=STYLE["text_dim"],
        fontsize=20,
        ha="center",
        va="center",
        fontweight="bold",
    )

    fig.suptitle(
        "Trilinear Interpolation: Two Bilinear Samples + Lerp",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "math/04-mipmaps-and-lod", "trilinear_interpolation.png")


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — aliasing_problem.png
# ---------------------------------------------------------------------------


def diagram_aliasing_problem():
    """Show why aliasing happens when many texels map to one pixel."""
    text_fx = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 5), facecolor=STYLE["bg"])

    # --- Left panel: Near — 1 texel per pixel ---
    setup_axes(ax1, xlim=(-0.2, 4.2), ylim=(-0.8, 4.5), grid=False)
    ax1.set_aspect("equal")

    # 4x4 checkerboard (texels)
    for row in range(4):
        for col in range(4):
            shade = STYLE["accent1"] if (row + col) % 2 == 0 else STYLE["surface"]
            alpha = 0.6 if shade == STYLE["accent1"] else 0.3
            r = Rectangle(
                (col, row),
                1,
                1,
                facecolor=shade,
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                alpha=alpha,
                zorder=1,
            )
            ax1.add_patch(r)

    # 4x4 dashed pixel grid (same size)
    for row in range(4):
        for col in range(4):
            r = Rectangle(
                (col, row),
                1,
                1,
                fill=False,
                edgecolor=STYLE["warn"],
                linewidth=1.2,
                linestyle="--",
                zorder=3,
            )
            ax1.add_patch(r)

    ax1.set_title(
        "Near the Camera",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax1.text(
        2.0,
        -0.55,
        "Each pixel samples ~1 texel \u2014 accurate",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=text_fx,
    )

    # --- Right panel: Far — many texels per pixel ---
    setup_axes(ax2, xlim=(-0.2, 4.2), ylim=(-0.8, 4.5), grid=False)
    ax2.set_aspect("equal")

    # 16x16 checkerboard (texels)
    cell = 4.0 / 16.0
    for row in range(16):
        for col in range(16):
            shade = STYLE["accent1"] if (row + col) % 2 == 0 else STYLE["surface"]
            alpha = 0.6 if shade == STYLE["accent1"] else 0.3
            r = Rectangle(
                (col * cell, row * cell),
                cell,
                cell,
                facecolor=shade,
                edgecolor=STYLE["grid"],
                linewidth=0.2,
                alpha=alpha,
                zorder=1,
            )
            ax2.add_patch(r)

    # 4x4 dashed pixel grid
    pixel_size = 1.0
    for row in range(4):
        for col in range(4):
            r = Rectangle(
                (col * pixel_size, row * pixel_size),
                pixel_size,
                pixel_size,
                fill=False,
                edgecolor=STYLE["warn"],
                linewidth=1.2,
                linestyle="--",
                zorder=3,
            )
            ax2.add_patch(r)

    # Highlight one pixel cell (row 1, col 2)
    hl_col, hl_row = 2, 1
    highlight = Rectangle(
        (hl_col * pixel_size, hl_row * pixel_size),
        pixel_size,
        pixel_size,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["warn"],
        linewidth=2.5,
        alpha=0.15,
        zorder=4,
    )
    ax2.add_patch(highlight)
    highlight_border = Rectangle(
        (hl_col * pixel_size, hl_row * pixel_size),
        pixel_size,
        pixel_size,
        fill=False,
        edgecolor=STYLE["warn"],
        linewidth=2.5,
        zorder=5,
    )
    ax2.add_patch(highlight_border)

    # Annotation pointing to highlighted cell
    ax2.annotate(
        "This pixel covers 16 texels\nbut only samples a few",
        xy=(hl_col * pixel_size + pixel_size / 2, hl_row * pixel_size + pixel_size / 2),
        xytext=(3.4, 3.8),
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
        path_effects=text_fx,
        zorder=6,
    )

    ax2.set_title(
        "Far from Camera",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.suptitle(
        "The Aliasing Problem: Texels vs Pixels",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "math/04-mipmaps-and-lod", "aliasing_problem.png")


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — lod_walkthrough.png
# ---------------------------------------------------------------------------


def diagram_lod_walkthrough():
    """Trace a complete pixel through the LOD pipeline with concrete numbers."""
    text_fx = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig = plt.figure(figsize=(11, 4), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 11.0)
    ax.set_ylim(-1.5, 2.5)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.set_aspect("equal")

    # Box definitions: (center_x, label, subtitle, border_color)
    boxes = [
        (1.0, "Pixel UV\n(0.60, 0.40)", "Next pixel: (0.65, 0.40)", STYLE["accent1"]),
        (3.2, "UV Derivative\nddx = 0.05", "\u0394u per pixel", STYLE["accent2"]),
        (5.4, "Footprint\n0.05 \u00d7 256 = 12.8\ntexels/pixel", "", STYLE["accent3"]),
        (7.6, "LOD\nlog\u2082(12.8) = 3.68", "", STYLE["accent4"]),
        (
            9.8,
            "Sample\n32% mip 3 (32\u00d732)\n68% mip 4 (16\u00d716)",
            "",
            STYLE["warn"],
        ),
    ]

    box_w = 1.8
    box_h = 1.6

    for cx, label, subtitle, border_color in boxes:
        # Box
        box = FancyBboxPatch(
            (cx - box_w / 2, -box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=border_color,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)

        # Label text
        ax.text(
            cx,
            0.0 if not subtitle else 0.1,
            label,
            color=STYLE["text"],
            fontsize=8,
            ha="center",
            va="center",
            fontweight="bold",
            path_effects=text_fx,
            zorder=3,
        )

        # Subtitle below the box
        if subtitle:
            ax.text(
                cx,
                -box_h / 2 - 0.2,
                subtitle,
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="top",
                path_effects=text_fx,
                zorder=3,
            )

    # Arrows between boxes
    for i in range(len(boxes) - 1):
        x_start = boxes[i][0] + box_w / 2 + 0.02
        x_end = boxes[i + 1][0] - box_w / 2 - 0.02
        ax.annotate(
            "",
            xy=(x_end, 0),
            xytext=(x_start, 0),
            arrowprops={
                "arrowstyle": "->",
                "color": STYLE["text_dim"],
                "lw": 1.5,
            },
            zorder=1,
        )

    ax.set_title(
        "LOD Selection: Tracing One Pixel Through the Pipeline",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/04-mipmaps-and-lod", "lod_walkthrough.png")


# ---------------------------------------------------------------------------
# math/09-view-matrix — view_transform.png
# ---------------------------------------------------------------------------
