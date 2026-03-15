"""Diagrams for Asset Lesson 13 — Morph Targets."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# 1. Morph target data flow: glTF → parser → mesh tool → .fmesh → loader
# ---------------------------------------------------------------------------


def diagram_morph_pipeline():
    """Data flow for morph targets through the asset pipeline.

    Shows the path from glTF primitives[].targets[] through the parser,
    mesh tool serialization, and runtime loader.
    """
    fig, ax = plt.subplots(figsize=(12, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 3.5)
    ax.set_aspect("equal")
    ax.axis("off")

    ct = STYLE["text"]
    c_src = STYLE["accent2"]  # green — source data
    c_tool = STYLE["accent1"]  # cyan — processing
    c_out = STYLE["accent3"]  # orange — output format
    c_rt = STYLE["accent4"]  # purple — runtime

    # Pipeline stages: (x, y, label, sublabel, color, width)
    stages = [
        (1.0, 2.0, "glTF", "primitives[].targets[]\nmesh.weights[]", c_src, 2.0),
        (
            4.0,
            2.0,
            "forge_gltf.h",
            "ForgeGltfMorphTarget\nposition/normal/tangent\ndeltas",
            c_tool,
            2.2,
        ),
        (
            7.0,
            2.0,
            "forge-mesh-tool",
            "FLAG_MORPHS\nmorph header\ndelta arrays",
            c_tool,
            2.2,
        ),
        (
            10.0,
            2.0,
            ".fmesh v3",
            "morph section\nappended after\nindex data",
            c_out,
            2.0,
        ),
        (
            10.0,
            0.5,
            "forge_pipeline.h",
            "ForgePipelineMorphTarget\nhas_morph_data()",
            c_rt,
            2.4,
        ),
    ]

    for x, y, label, sublabel, color, w in stages:
        h = 1.2
        rect = mpatches.FancyBboxPatch(
            (x - w / 2, y - h / 2),
            w,
            h,
            boxstyle="round,pad=0.08",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.2,
            alpha=0.85,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            x,
            y + 0.25,
            label,
            color="#ffffff",
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=3,
        )
        ax.text(
            x,
            y - 0.2,
            sublabel,
            color="#cccccc",
            fontsize=7,
            ha="center",
            va="center",
            zorder=3,
        )

    # Arrows between stages
    arrow_props = dict(
        arrowstyle="->",
        color=STYLE["text_dim"],
        linewidth=1.5,
        connectionstyle="arc3,rad=0",
    )
    arrows = [
        (2.0, 2.0, 2.9, 2.0),  # glTF → parser
        (5.1, 2.0, 5.9, 2.0),  # parser → mesh tool
        (8.1, 2.0, 9.0, 2.0),  # mesh tool → .fmesh
        (10.0, 1.4, 10.0, 1.1),  # .fmesh → loader
    ]
    for x1, y1, x2, y2 in arrows:
        ax.annotate(
            "",
            xy=(x2, y2),
            xytext=(x1, y1),
            arrowprops=arrow_props,
            zorder=1,
        )

    # .fanim side path
    ax.text(
        7.0,
        0.5,
        ".fanim\nMORPH_WEIGHTS\n(path = 3)",
        color=STYLE["accent3"],
        fontsize=7,
        ha="center",
        va="center",
        style="italic",
        zorder=3,
    )
    ax.annotate(
        "",
        xy=(8.8, 0.5),
        xytext=(7.8, 0.5),
        arrowprops=arrow_props,
        zorder=1,
    )

    ax.set_title(
        "Morph Target Pipeline",
        color=ct,
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    save(fig, "assets", "lesson_13_morph_pipeline")


# ---------------------------------------------------------------------------
# 2. .fmesh morph binary layout
# ---------------------------------------------------------------------------


def diagram_morph_binary_layout():
    """.fmesh binary layout showing morph section appended after indices.

    Visualizes the header, LOD tables, vertices, indices, and the new
    morph section with its sub-components.
    """
    fig, ax = plt.subplots(figsize=(10, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-0.5, 9.5)
    ax.set_aspect("equal")
    ax.axis("off")

    ct = STYLE["text"]
    c_hdr = STYLE["accent1"]  # cyan — existing header
    c_data = STYLE["accent2"]  # green — existing data
    c_morph = STYLE["accent3"]  # orange — new morph section
    c_meta = STYLE["accent4"]  # purple — morph metadata

    bw = 6.0  # block width
    bx = 2.0  # block x position

    # Sections: (y, height, label, detail, color)
    sections = [
        (
            8.5,
            0.5,
            "Header (32 B)",
            "magic, version, vertex_count, stride, lod_count, flags, submesh_count, reserved",
            c_hdr,
        ),
        (
            7.7,
            0.5,
            "LOD-Submesh Table",
            "target_error + per-submesh (index_count, offset, material)",
            c_hdr,
        ),
        (6.9, 0.5, "Vertex Data", "vertex_count × stride bytes", c_data),
        (6.1, 0.5, "Index Data", "uint32 indices, all LODs concatenated", c_data),
        (
            5.0,
            0.5,
            "Morph Header (8 B)",
            "morph_target_count (u32) + morph_attribute_flags (u32)",
            c_morph,
        ),
        (
            4.0,
            0.7,
            "Per-Target Metadata",
            "target_count × 68 B each:\n  name (64 B) + default_weight (f32)",
            c_meta,
        ),
        (
            2.8,
            1.0,
            "Morph Delta Data",
            "Per target, contiguous:\n  position_deltas[vtx × 3]\n  normal_deltas[vtx × 3]  (if flag)\n  tangent_deltas[vtx × 3] (if flag)",
            c_morph,
        ),
    ]

    for y, h, label, detail, color in sections:
        rect = mpatches.FancyBboxPatch(
            (bx, y - h / 2),
            bw,
            h,
            boxstyle="round,pad=0.05",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.0,
            alpha=0.8,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            bx + 0.15,
            y + 0.05,
            label,
            color="#ffffff",
            fontsize=9,
            fontweight="bold",
            va="center",
            zorder=3,
        )
        ax.text(
            bx + 0.15,
            y - 0.15,
            detail,
            color="#cccccc",
            fontsize=7,
            va="center",
            zorder=3,
        )

    # Bracket marking "existing format"
    ax.annotate(
        "existing\n.fmesh v3",
        xy=(bx + bw + 0.2, 7.3),
        fontsize=8,
        color=STYLE["text_dim"],
        ha="left",
        va="center",
    )
    ax.plot(
        [bx + bw + 0.15, bx + bw + 0.15],
        [6.1, 8.5],
        color=STYLE["text_dim"],
        linewidth=1.0,
        linestyle="--",
    )

    # Bracket marking "new morph section"
    ax.annotate(
        "new morph\nsection",
        xy=(bx + bw + 0.2, 3.9),
        fontsize=8,
        color=STYLE["accent3"],
        ha="left",
        va="center",
        fontweight="bold",
    )
    ax.plot(
        [bx + bw + 0.15, bx + bw + 0.15],
        [2.3, 5.25],
        color=STYLE["accent3"],
        linewidth=1.5,
        linestyle="--",
    )

    # Flag callout
    ax.annotate(
        "flags |= FLAG_MORPHS\n(bit 2 = 0x4)",
        xy=(bx - 0.1, 8.5),
        fontsize=7,
        color=STYLE["accent3"],
        ha="right",
        va="center",
        fontweight="bold",
    )

    ax.set_title(
        ".fmesh Morph Target Binary Layout",
        color=ct,
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    save(fig, "assets", "lesson_13_morph_binary_layout")


# ---------------------------------------------------------------------------
# Public diagram list
# ---------------------------------------------------------------------------

DIAGRAMS = [
    diagram_morph_pipeline,
    diagram_morph_binary_layout,
]
