"""Diagrams for assets/03."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# Lesson 03 — Mesh Processing
# ---------------------------------------------------------------------------


def diagram_mesh_processing_pipeline():
    """Flowchart of the mesh optimization pipeline stages."""
    fig, ax = plt.subplots(figsize=(14, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 13), ylim=(-1, 3), grid=False, aspect=None)

    stages = [
        ("OBJ\nFile", "36 verts\n(de-indexed)", STYLE["accent1"]),
        ("Vertex\nDedup", "24 verts\n36 indices", STYLE["accent3"]),
        ("Index\nOptimize", "Cache +\nOverdraw", STYLE["accent3"]),
        ("Tangent\nGeneration", "+vec4 per\nvertex", STYLE["accent4"]),
        ("LOD\nSimplify", "3 levels\n100/50/25%", STYLE["accent2"]),
        (".fmesh\nBinary", "GPU-ready\nformat", STYLE["warn"]),
    ]

    box_width = 1.6
    box_height = 1.8
    spacing = 2.2
    y_center = 1.0

    for i, (label, metric, color) in enumerate(stages):
        x = i * spacing
        rect = mpatches.FancyBboxPatch(
            (x - box_width / 2, y_center - box_height / 2),
            box_width,
            box_height,
            boxstyle="round,pad=0.15",
            facecolor=color,
            alpha=0.25,
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(rect)

        # Stage name
        ax.text(
            x,
            y_center + 0.3,
            label,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
        )

        # Metric below the name
        ax.text(
            x,
            y_center - 0.45,
            metric,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
        )

        # Arrow to next stage
        if i < len(stages) - 1:
            ax.annotate(
                "",
                xy=((i + 1) * spacing - box_width / 2 - 0.1, y_center),
                xytext=(x + box_width / 2 + 0.1, y_center),
                arrowprops={
                    "arrowstyle": "->,head_width=0.25,head_length=0.15",
                    "color": STYLE["text_dim"],
                    "lw": 2,
                },
            )

    ax.set_title(
        "Mesh Processing Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=10,
    )

    ax.set_xticks([])
    ax.set_yticks([])
    plt.tight_layout()
    save(fig, "assets/03-mesh-processing", "mesh_pipeline.png")


def diagram_lod_simplification():
    """Visualization of LOD levels with decreasing triangle density."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Level of Detail (LOD) Simplification",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    lod_info = [
        ("LOD 0 — Full Detail", "36 triangles", "100%", 1.0, 16),
        ("LOD 1 — Medium", "18 triangles", "50%", 0.65, 10),
        ("LOD 2 — Low", "9 triangles", "25%", 0.35, 6),
    ]

    for ax, (title, tri_count, ratio, alpha, n_segments) in zip(
        axes, lod_info, strict=True
    ):
        setup_axes(ax, xlim=(-1.5, 1.5), ylim=(-1.5, 1.5), grid=False, aspect="equal")
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold")

        # Draw a sphere-like wireframe using latitude and longitude lines
        theta = np.linspace(0, 2 * np.pi, n_segments + 1)
        color = STYLE["accent1"]

        # Draw concentric latitude rings at different "heights"
        n_rings = max(n_segments // 3, 2)
        for j in range(1, n_rings + 1):
            scale = np.sin(j * np.pi / (n_rings + 1))
            offset_y = np.cos(j * np.pi / (n_rings + 1)) * 0.3
            x_ring = scale * np.cos(theta)
            y_ring = scale * np.sin(theta) * 0.4 + offset_y
            ax.plot(x_ring, y_ring, color=color, alpha=alpha, linewidth=1.2)

        # Draw longitude lines
        phi = np.linspace(0, np.pi, n_segments // 2 + 1)
        for angle in np.linspace(0, 2 * np.pi, n_segments // 2, endpoint=False):
            x_lon = np.sin(phi) * np.cos(angle)
            y_lon = np.cos(phi) * 0.4 + np.sin(phi) * np.sin(angle) * 0.4
            ax.plot(x_lon, y_lon, color=color, alpha=alpha * 0.7, linewidth=0.8)

        # Draw outer circle silhouette
        circle = Circle(
            (0, 0),
            1.0,
            fill=False,
            edgecolor=color,
            linewidth=2.0,
            alpha=alpha,
        )
        ax.add_patch(circle)

        # Label with triangle count and ratio
        ax.text(
            0,
            -1.35,
            f"{tri_count}  ({ratio})",
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
        )

        ax.set_xticks([])
        ax.set_yticks([])

    plt.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "assets/03-mesh-processing", "lod_simplification.png")


# ---------------------------------------------------------------------------
# Lesson 04 — Procedural Geometry
# ---------------------------------------------------------------------------
