"""Diagrams for math/03."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# math/03-bilinear-interpolation — bilinear_interpolation.png
# ---------------------------------------------------------------------------


def diagram_bilinear_interpolation():
    """Grid cell with the 3 lerp steps highlighted."""
    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.3, 1.5), ylim=(-0.3, 1.5), grid=False)

    # Corner values with distinct colors
    corners = {
        (0, 0): ("c00 = 10", STYLE["accent1"]),
        (1, 0): ("c10 = 30", STYLE["accent2"]),
        (0, 1): ("c01 = 20", STYLE["accent4"]),
        (1, 1): ("c11 = 50", STYLE["accent3"]),
    }

    # Grid cell fill
    rect = Rectangle(
        (0, 0),
        1,
        1,
        fill=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        zorder=1,
    )
    ax.add_patch(rect)

    # Corner points and labels
    for (cx, cy), (label, color) in corners.items():
        ax.plot(cx, cy, "o", color=color, markersize=10, zorder=5)
        offset_x = -0.22 if cx == 0 else 0.08
        offset_y = -0.12 if cy == 0 else 0.08
        ax.text(
            cx + offset_x,
            cy + offset_y,
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    tx, ty = 0.35, 0.7

    # Sample point
    ax.plot(tx, ty, "*", color=STYLE["warn"], markersize=18, zorder=6)
    ax.text(
        tx + 0.06,
        ty + 0.06,
        f"({tx}, {ty})",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Step 1: bottom lerp
    bot_val = 10 + tx * (30 - 10)
    ax.plot(tx, 0, "s", color=STYLE["accent1"], markersize=8, zorder=5)
    ax.plot([0, 1], [0, 0], "-", color=STYLE["accent1"], lw=2, alpha=0.6)
    ax.text(
        tx,
        -0.18,
        f"bot = {bot_val:.0f}",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        fontweight="bold",
    )
    ax.text(
        1.15, 0.0, "1. lerp bottom", color=STYLE["accent1"], fontsize=9, va="center"
    )

    # Step 2: top lerp
    top_val = 20 + tx * (50 - 20)
    ax.plot(tx, 1, "s", color=STYLE["accent4"], markersize=8, zorder=5)
    ax.plot([0, 1], [1, 1], "-", color=STYLE["accent4"], lw=2, alpha=0.6)
    ax.text(
        tx,
        1.1,
        f"top = {top_val:.1f}",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
        fontweight="bold",
    )
    ax.text(1.15, 1.0, "2. lerp top", color=STYLE["accent4"], fontsize=9, va="center")

    # Step 3: vertical lerp
    result_val = bot_val + ty * (top_val - bot_val)
    ax.plot([tx, tx], [0, 1], "--", color=STYLE["warn"], lw=1.5, alpha=0.7)
    ax.text(
        tx - 0.27,
        0.5,
        "3. lerp\nvertical",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # Result annotation
    ax.text(
        0.5,
        -0.22,
        f"result = {result_val:.1f}",
        color=STYLE["warn"],
        fontsize=12,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_title(
        "Bilinear Interpolation: Three Lerps",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel("tx", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("ty", color=STYLE["axis"], fontsize=11)

    fig.tight_layout()
    save(fig, "math/03-bilinear-interpolation", "bilinear_interpolation.png")


# ---------------------------------------------------------------------------
# math/05-matrices — matrix_basis_vectors.png
# ---------------------------------------------------------------------------
