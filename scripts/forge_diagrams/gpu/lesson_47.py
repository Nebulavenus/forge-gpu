"""Diagrams for gpu/47."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyArrowPatch, Rectangle

from .._common import STYLE, save

# ---------------------------------------------------------------------------
# gpu/47-texture-atlas-rendering — lesson_47_atlas_uv_remap.png
# ---------------------------------------------------------------------------


def diagram_atlas_uv_remap():
    """Atlas UV remapping: original per-material UVs transformed to atlas space.

    Left panel: a 1x1 UV square representing a single material's UV space
    (coordinates 0 to 1), with a sample point marked at a specific UV.

    Right panel: a 4x4 atlas grid showing multiple material tiles, with the
    same sample point remapped to its atlas-space position via the transform
    uv_atlas = uv_original * scale + offset.

    A curved annotation arrow connects the two panels, labeled with the
    transform formula.
    """
    fig = plt.figure(figsize=(13, 6), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # -------------------------------------------------------------------------
    # Left panel — original UV space (0..1 square)
    # -------------------------------------------------------------------------
    ax1 = fig.add_axes([0.04, 0.12, 0.36, 0.76])  # type: ignore[reportCallIssue]
    ax1.set_facecolor(STYLE["bg"])
    ax1.set_xlim(-0.18, 1.22)
    ax1.set_ylim(-0.18, 1.22)
    ax1.set_aspect("equal")
    ax1.set_xticks([0.0, 0.5, 1.0])
    ax1.set_yticks([0.0, 0.5, 1.0])
    ax1.tick_params(colors=STYLE["axis"], labelsize=9)
    for spine in ax1.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)

    # UV square border
    uv_box = Rectangle(
        (0, 0),
        1,
        1,
        linewidth=2,
        edgecolor=STYLE["accent1"],
        facecolor=STYLE["surface"],
        alpha=0.35,
        zorder=2,
    )
    ax1.add_patch(uv_box)

    # Subtle internal grid lines
    for v in [0.25, 0.5, 0.75]:
        ax1.axhline(v, color=STYLE["grid"], lw=0.6, alpha=0.7, zorder=1)
        ax1.axvline(v, color=STYLE["grid"], lw=0.6, alpha=0.7, zorder=1)

    # Corner coordinate labels
    corner_kw = dict(
        color=STYLE["axis"], fontsize=8, ha="center", va="center", path_effects=stroke
    )
    ax1.text(0.0, -0.12, "(0, 0)", **corner_kw)  # type: ignore[arg-type]
    ax1.text(1.0, -0.12, "(1, 0)", **corner_kw)  # type: ignore[arg-type]
    ax1.text(-0.14, 0.0, "(0, 0)", **corner_kw)  # type: ignore[arg-type]
    ax1.text(-0.14, 1.0, "(0, 1)", **corner_kw)  # type: ignore[arg-type]

    # Axis direction labels
    ax1.text(
        0.5,
        -0.16,
        "U",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )
    ax1.text(
        -0.16,
        0.5,
        "V",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke,
    )

    # Sample point in original UV space
    # Chosen so it maps cleanly: tile (1,2) in a 4x4 grid, local UV (0.6, 0.4)
    # uv_atlas = uv_orig * 0.25 + (0.25, 0.5)
    uv_orig = np.array([0.6, 0.4])
    ax1.plot(
        *uv_orig,
        "o",
        color=STYLE["accent2"],
        markersize=11,
        zorder=6,
        markeredgecolor=STYLE["bg"],
        markeredgewidth=1.5,
    )
    ax1.text(
        uv_orig[0] + 0.07,
        uv_orig[1] + 0.07,
        f"({uv_orig[0]:.1f}, {uv_orig[1]:.1f})",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="bottom",
        path_effects=stroke,
        zorder=7,
    )

    # Dashed crosshair at sample point
    ax1.plot(
        [uv_orig[0], uv_orig[0]],
        [0, uv_orig[1]],
        "--",
        color=STYLE["accent2"],
        lw=0.9,
        alpha=0.5,
        zorder=3,
    )
    ax1.plot(
        [0, uv_orig[0]],
        [uv_orig[1], uv_orig[1]],
        "--",
        color=STYLE["accent2"],
        lw=0.9,
        alpha=0.5,
        zorder=3,
    )

    ax1.set_title(
        "Per-material UV space",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # -------------------------------------------------------------------------
    # Right panel — atlas UV space (0..1, subdivided into a 4x4 grid of tiles)
    # -------------------------------------------------------------------------
    ax2 = fig.add_axes([0.58, 0.12, 0.40, 0.76])  # type: ignore[reportCallIssue]
    ax2.set_facecolor(STYLE["bg"])
    ax2.set_xlim(-0.06, 1.10)
    ax2.set_ylim(-0.06, 1.10)
    ax2.set_aspect("equal")
    ax2.set_xticks([0.0, 0.25, 0.5, 0.75, 1.0])
    ax2.set_yticks([0.0, 0.25, 0.5, 0.75, 1.0])
    ax2.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax2.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)

    # Atlas border
    atlas_border = Rectangle(
        (0, 0),
        1,
        1,
        linewidth=2,
        edgecolor=STYLE["axis"],
        facecolor="none",
        zorder=3,
    )
    ax2.add_patch(atlas_border)

    # Draw 4x4 tile grid with alternating surface fills and material labels
    n = 4
    tile_size = 1.0 / n
    tile_colors = [STYLE["surface"], "#1e2040"]  # two subtle alternating shades

    # Highlight the target tile: column 1 (x), row 2 (y) in 0-indexed grid
    target_col, target_row = 1, 2

    label_kw = dict(
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    for row in range(n):
        for col in range(n):
            x0 = col * tile_size
            y0 = row * tile_size
            is_target = col == target_col and row == target_row

            face = STYLE["accent1"] if is_target else tile_colors[(row + col) % 2]
            alpha = 0.22 if is_target else 0.18
            edge = STYLE["accent1"] if is_target else STYLE["grid"]
            lw = 1.5 if is_target else 0.5

            tile = Rectangle(
                (x0, y0),
                tile_size,
                tile_size,
                linewidth=lw,
                edgecolor=edge,
                facecolor=face,
                alpha=alpha,
                zorder=2,
            )
            ax2.add_patch(tile)

            mat_idx = row * n + col
            ax2.text(x0 + tile_size / 2, y0 + tile_size / 2, f"M{mat_idx}", **label_kw)  # type: ignore[arg-type]

    # Remapped sample point in atlas space
    # scale = (1/4, 1/4) = 0.25, offset = (col*0.25, row*0.25) = (0.25, 0.5)
    scale = np.array([0.25, 0.25])
    offset = np.array([target_col * tile_size, target_row * tile_size])
    uv_atlas = uv_orig * scale + offset

    ax2.plot(
        *uv_atlas,
        "o",
        color=STYLE["accent2"],
        markersize=11,
        zorder=6,
        markeredgecolor=STYLE["bg"],
        markeredgewidth=1.5,
    )
    ax2.text(
        uv_atlas[0] + 0.04,
        uv_atlas[1] + 0.04,
        f"({uv_atlas[0]:.3f}, {uv_atlas[1]:.3f})",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="bottom",
        path_effects=stroke,
        zorder=7,
    )

    # Dashed crosshair
    ax2.plot(
        [uv_atlas[0], uv_atlas[0]],
        [0, uv_atlas[1]],
        "--",
        color=STYLE["accent2"],
        lw=0.9,
        alpha=0.5,
        zorder=3,
    )
    ax2.plot(
        [0, uv_atlas[0]],
        [uv_atlas[1], uv_atlas[1]],
        "--",
        color=STYLE["accent2"],
        lw=0.9,
        alpha=0.5,
        zorder=3,
    )

    # Tile boundary tick annotations (0, 0.25, 0.5, 0.75, 1.0)
    for i in range(n + 1):
        v = i * tile_size
        ax2.text(
            v,
            -0.045,
            f"{v:.2f}",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="top",
            path_effects=stroke,
        )

    ax2.set_title(
        "Atlas UV space (4×4 tiles)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # -------------------------------------------------------------------------
    # Central transform annotation
    # -------------------------------------------------------------------------
    # Draw a curved arrow in figure coordinates connecting the two panels
    arrow = FancyArrowPatch(
        posA=(0.415, 0.50),
        posB=(0.575, 0.50),
        transform=fig.transFigure,
        arrowstyle="->,head_width=0.015,head_length=0.012",
        color=STYLE["warn"],
        lw=2.5,
        connectionstyle="arc3,rad=0.0",
        zorder=10,
    )
    fig.add_artist(arrow)

    # Formula label above the arrow
    fig.text(
        0.495,
        0.62,
        "uv_atlas =",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
    )
    fig.text(
        0.495,
        0.55,
        "uv_orig × scale + offset",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
    )

    # Scale/offset values below the arrow
    fig.text(
        0.495,
        0.43,
        f"scale = ({scale[0]:.2f}, {scale[1]:.2f})\n"
        f"offset = ({offset[0]:.2f}, {offset[1]:.2f})",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        linespacing=1.6,
    )

    fig.suptitle(
        "Atlas UV Remapping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    save(fig, "gpu/47-texture-atlas-rendering", "lesson_47_atlas_uv_remap.png")
