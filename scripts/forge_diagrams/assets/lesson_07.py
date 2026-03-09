"""Diagrams for Asset Lesson 07 — Materials."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# 1. Material data flow: glTF → parser → mesh tool → outputs → loader → GPU
# ---------------------------------------------------------------------------


def diagram_material_data_flow():
    """Data flow showing how material data moves through the pipeline."""
    fig, ax = plt.subplots(figsize=(10, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-0.5, 4.5)
    ax.set_aspect("equal")
    ax.axis("off")

    c1 = STYLE["accent1"]
    c2 = STYLE["accent2"]
    c3 = STYLE["accent3"]
    ct = STYLE["text"]

    # Boxes: (x, y, width, height, color, label, sublabel)
    boxes = [
        (0.0, 2.0, 1.8, 1.5, c1, "glTF", "primitives\nmaterials\ntextures"),
        (2.5, 2.0, 1.8, 1.5, c2, "Parser", "forge_gltf.h\nPBR fields"),
        (5.0, 2.8, 1.8, 0.7, c3, ".fmesh v2", "submesh table"),
        (5.0, 1.5, 1.8, 0.7, c3, ".fmat", "PBR materials"),
        (7.5, 2.0, 1.8, 1.5, c1, "Loader", "forge_pipeline.h\nsubmeshes"),
        (0.0, 0.0, 1.8, 1.0, c2, "Mesh Tool", "multi-primitive\nLOD + optimize"),
    ]

    for x, y, w, h, color, label, sublabel in boxes:
        rect = mpatches.FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.5,
            alpha=0.85,
        )
        ax.add_patch(rect)
        ax.text(
            x + w / 2,
            y + h * 0.65,
            label,
            ha="center",
            va="center",
            fontsize=9,
            fontweight="bold",
            color=ct,
        )
        ax.text(
            x + w / 2,
            y + h * 0.25,
            sublabel,
            ha="center",
            va="center",
            fontsize=7,
            color=ct,
            alpha=0.8,
        )

    # Arrows
    arrow_kw = dict(
        arrowstyle="->",
        color=ct,
        linewidth=1.5,
        connectionstyle="arc3,rad=0.0",
    )

    arrows = [
        # glTF → Parser
        ((1.8, 2.75), (2.5, 2.75)),
        # Parser → Mesh Tool
        ((3.4, 2.0), (1.8, 0.75)),
        # Mesh Tool → .fmesh v2
        ((1.8, 0.5), (5.0, 3.1)),
        # Mesh Tool → .fmat
        ((1.8, 0.25), (5.0, 1.85)),
        # .fmesh → Loader
        ((6.8, 3.1), (7.5, 2.95)),
        # .fmat → Loader
        ((6.8, 1.85), (7.5, 2.55)),
    ]

    for start, end in arrows:
        ax.annotate(
            "",
            xy=end,
            xytext=start,
            arrowprops=arrow_kw,
        )

    ax.set_title(
        "Material data flow through the asset pipeline",
        color=ct,
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    save(fig, "assets", "lesson_07_material_data_flow")


# ---------------------------------------------------------------------------
# 2. .fmesh v2 binary layout with LOD-submesh table
# ---------------------------------------------------------------------------


def diagram_fmesh_v2_layout():
    """Binary layout of the .fmesh v2 format with submesh table."""
    fig, ax = plt.subplots(figsize=(7, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 6.5)
    ax.set_ylim(-0.5, 9.5)
    ax.set_aspect("equal")
    ax.axis("off")

    c1 = STYLE["accent1"]
    c2 = STYLE["accent2"]
    c3 = STYLE["accent3"]
    ct = STYLE["text"]

    # Draw blocks from top to bottom
    blocks = [
        (
            8.0,
            1.3,
            c1,
            "Header (32 bytes)",
            [
                'magic: "FMSH"  version: 2',
                "vertex_count  vertex_stride",
                "lod_count  flags",
                "submesh_count  reserved",
            ],
        ),
        (
            5.5,
            2.0,
            c2,
            "LOD-Submesh Table",
            [
                "LOD 0: target_error",
                "  submesh 0: idx_count, idx_offset, mat_idx",
                "  submesh 1: idx_count, idx_offset, mat_idx",
                "LOD 1: target_error",
                "  submesh 0: idx_count, idx_offset, mat_idx",
                "  submesh 1: idx_count, idx_offset, mat_idx",
            ],
        ),
        (
            3.0,
            1.5,
            c3,
            "Vertex Data",
            [
                "vertex_count \u00d7 vertex_stride bytes",
                "pos(12) + norm(12) + uv(8) [+ tan(16)]",
            ],
        ),
        (
            1.0,
            1.0,
            c1,
            "Index Data",
            [
                "All LOD \u00d7 submesh indices (uint32)",
                "contiguous, byte-offset addressed",
            ],
        ),
    ]

    for y_top, height, color, title, lines in blocks:
        rect = mpatches.FancyBboxPatch(
            (0.5, y_top - height),
            5.0,
            height,
            boxstyle="round,pad=0.1",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.5,
            alpha=0.85,
        )
        ax.add_patch(rect)
        ax.text(
            3.0,
            y_top - 0.15,
            title,
            ha="center",
            va="top",
            fontsize=9,
            fontweight="bold",
            color=ct,
        )
        for i, line in enumerate(lines):
            ax.text(
                3.0,
                y_top - 0.4 - i * 0.25,
                line,
                ha="center",
                va="top",
                fontsize=7,
                color=ct,
                alpha=0.8,
                family="monospace",
            )

    ax.set_title(
        ".fmesh v2 binary layout",
        color=ct,
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    save(fig, "assets", "lesson_07_fmesh_v2_layout")


# ---------------------------------------------------------------------------
# Public list of all diagram functions in this module
# ---------------------------------------------------------------------------

DIAGRAMS = [
    diagram_material_data_flow,
    diagram_fmesh_v2_layout,
]
