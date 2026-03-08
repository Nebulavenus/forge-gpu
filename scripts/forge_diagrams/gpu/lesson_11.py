"""Diagrams for gpu/11."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/11-compute-shaders — fullscreen_triangle.png
# ---------------------------------------------------------------------------


def diagram_fullscreen_triangle():
    """Two-panel comparison: fullscreen quad vs fullscreen triangle.

    Left:  A quad made of two triangles with a diagonal seam and redundant
           fragment processing along the diagonal.
    Right: A single oversized triangle that covers the viewport, with the
           clipped region shown.
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 5), facecolor=STYLE["bg"])

    for ax in (ax1, ax2):
        setup_axes(ax, xlim=(-2.0, 4.0), ylim=(-2.0, 4.0), grid=False)
        ax.set_xticks([-1, 0, 1, 2, 3])
        ax.set_yticks([-1, 0, 1, 2, 3])
        ax.tick_params(colors=STYLE["axis"], labelsize=8)

    # --- Left panel: fullscreen quad (two triangles) ---
    ax1.set_title(
        "Fullscreen Quad (2 triangles)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # The viewport rectangle [-1,1] x [-1,1]
    viewport = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=2,
        edgecolor=STYLE["text"],
        facecolor="none",
        linestyle="--",
        zorder=5,
    )
    ax1.add_patch(viewport)

    # Triangle 1 (lower-left)
    tri1 = Polygon(
        [(-1, -1), (1, -1), (-1, 1)],
        closed=True,
        facecolor=STYLE["accent1"],
        alpha=0.3,
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=3,
    )
    ax1.add_patch(tri1)

    # Triangle 2 (upper-right)
    tri2 = Polygon(
        [(1, 1), (-1, 1), (1, -1)],
        closed=True,
        facecolor=STYLE["accent2"],
        alpha=0.3,
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=3,
    )
    ax1.add_patch(tri2)

    # Diagonal seam
    ax1.plot(
        [-1, 1],
        [1, -1],
        color=STYLE["warn"],
        linewidth=2.5,
        linestyle="-",
        zorder=6,
    )
    ax1.text(
        0.15,
        0.15,
        "diagonal\nseam",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=-45,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # Vertex labels — position each individually to avoid overlaps
    for (x, y), label, (ox, oy), va in [
        ((-1, -1), "(-1,-1)", (0, -0.3), "top"),
        ((1, -1), "(1,-1)", (0, -0.3), "top"),
        ((-1, 1), "(-1, 1)", (-0.35, 0), "center"),
        ((1, 1), "(1, 1)", (0.35, 0), "center"),
    ]:
        ax1.plot(x, y, "o", color=STYLE["text"], markersize=6, zorder=8)
        ax1.text(
            x + ox,
            y + oy,
            label,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va=va,
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax1.text(
        -0.6,
        -0.55,
        "Tri 1",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax1.text(
        0.55,
        0.65,
        "Tri 2",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Annotations
    ax1.text(
        0.0,
        -1.7,
        "4 vertices, 2 triangles, 6 indices\nfragments on diagonal processed twice",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    # --- Right panel: fullscreen triangle (single oversized) ---
    ax2.set_title(
        "Fullscreen Triangle (1 triangle)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # The viewport rectangle
    viewport2 = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=2,
        edgecolor=STYLE["text"],
        facecolor="none",
        linestyle="--",
        zorder=5,
    )
    ax2.add_patch(viewport2)

    # The oversized triangle: (-1,-1), (3,-1), (-1,3)
    tri_full = Polygon(
        [(-1, -1), (3, -1), (-1, 3)],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.15,
        edgecolor=STYLE["accent3"],
        linewidth=2,
        linestyle=":",
        zorder=2,
    )
    ax2.add_patch(tri_full)

    # The visible (clipped) portion
    tri_visible = Polygon(
        [(-1, -1), (1, -1), (1, 1), (-1, 1)],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.35,
        edgecolor=STYLE["accent3"],
        linewidth=2,
        zorder=3,
    )
    ax2.add_patch(tri_visible)

    # Vertex labels for the triangle — positioned to avoid edge clipping
    for (x, y), label, (ox, oy), ha in [
        ((-1, -1), "(-1,-1)\nid=0", (-0.1, -0.35), "center"),
        ((3, -1), "(3,-1)\nid=1", (-0.35, 0.35), "center"),
        ((-1, 3), "(-1, 3)\nid=2", (0.35, 0.0), "left"),
    ]:
        ax2.plot(x, y, "o", color=STYLE["accent3"], markersize=8, zorder=8)
        ax2.text(
            x + ox,
            y + oy,
            label,
            color=STYLE["accent3"],
            fontsize=9,
            fontweight="bold",
            ha=ha,
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Label the clipped region
    ax2.text(
        0.0,
        0.0,
        "visible\nregion",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=6,
    )

    # Label the clipped-away region
    ax2.text(
        1.6,
        0.5,
        "clipped\naway",
        color=STYLE["text_dim"],
        fontsize=9,
        style="italic",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # "Viewport" label
    ax2.text(
        1.0,
        1.15,
        "viewport",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax2.text(
        0.0,
        -1.7,
        "3 vertices, 1 triangle, 0 indices\nno seam, no redundant fragments",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    fig.tight_layout()
    save(fig, "gpu/11-compute-shaders", "fullscreen_triangle.png")


# ---------------------------------------------------------------------------
# GPU Lesson 12 — Shader Grid: undersampling / Nyquist aliasing
# ---------------------------------------------------------------------------
