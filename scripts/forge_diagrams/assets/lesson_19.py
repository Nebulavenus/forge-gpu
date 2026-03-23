"""Diagrams for assets/19 — Pipeline Asset Viewer."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

# ---------------------------------------------------------------------------
# 1. .fmesh binary layout — header, LOD-submesh table, vertex + index data
# ---------------------------------------------------------------------------


def diagram_fmesh_browser_layout():
    """Binary layout of .fmesh as parsed by the TypeScript fmesh-parser."""
    fig, ax = plt.subplots(figsize=(8, 9))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False, aspect="equal")
    ax.set_xlim(-1.0, 8.5)
    ax.set_ylim(-0.5, 10.0)
    ax.axis("off")

    ct = STYLE["text"]
    c1 = STYLE["accent1"]
    c2 = STYLE["accent2"]
    c3 = STYLE["accent3"]
    c4 = STYLE["accent4"]
    cw = STYLE["warn"]

    # Blocks from top to bottom: (y_top, height, color, title, fields, byte_label)
    blocks = [
        (
            9.0,
            1.6,
            c1,
            "Header (32 bytes)",
            [
                ('magic "FMSH"', "bytes 0–3"),
                ("version (u32)", "bytes 4–7"),
                ("vertex_count (u32)", "bytes 8–11"),
                ("vertex_stride (32 | 48)", "bytes 12–15"),
                ("lod_count (u32)", "bytes 16–19"),
                ("flags · submesh_count · reserved", "bytes 20–31"),
            ],
        ),
        (
            7.0,
            1.6,
            c2,
            "LOD-Submesh Table",
            [
                ("per LOD: target_error (f32)", "4 bytes"),
                ("  per submesh: index_count (u32)", "4 bytes"),
                ("  per submesh: index_offset (u32)", "4 bytes"),
                ("  per submesh: material_index (i32)", "4 bytes"),
                ("size = lod_count × (4 + submesh_count × 12)", ""),
            ],
        ),
        (
            5.0,
            1.4,
            c3,
            "Vertex Data",
            [
                ("vertex_count × vertex_stride bytes", ""),
                ("stride 32: pos(3f) + normal(3f) + uv(2f)", ""),
                ("stride 48: + tangent(4f)", ""),
                ("interleaved — parser de-interleaves per attribute", ""),
            ],
        ),
        (
            3.2,
            1.0,
            c4,
            "Index Data",
            [
                ("all LOD × submesh indices (uint32)", ""),
                ("addressed by byte offset within index data section", ""),
            ],
        ),
    ]

    for y_top, height, color, title, fields in blocks:
        rect = mpatches.FancyBboxPatch(
            (0.5, y_top - height),
            7.0,
            height,
            boxstyle="round,pad=0.1",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.5,
            alpha=0.2,
        )
        ax.add_patch(rect)

        # Title
        ax.text(
            4.0,
            y_top - 0.15,
            title,
            ha="center",
            va="top",
            fontsize=10,
            fontweight="bold",
            color=color,
            path_effects=_STROKE,
        )

        # Fields
        for i, (field, offset_label) in enumerate(fields):
            y = y_top - 0.42 - i * 0.22
            ax.text(
                1.0,
                y,
                field,
                ha="left",
                va="top",
                fontsize=7.5,
                color=ct,
                alpha=0.9,
                family="monospace",
                path_effects=_STROKE,
            )
            if offset_label:
                ax.text(
                    7.2,
                    y,
                    offset_label,
                    ha="right",
                    va="top",
                    fontsize=7,
                    color=STYLE["text_dim"],
                    family="monospace",
                )

    # Arrows between sections
    arrow_kw = dict(arrowstyle="->", color=cw, linewidth=1.5)
    for y_from, y_to in [(7.4, 7.05), (5.4, 5.05), (3.6, 3.25)]:
        ax.annotate("", xy=(4.0, y_to), xytext=(4.0, y_from), arrowprops=arrow_kw)

    # DataView annotation
    ax.text(
        4.0,
        1.8,
        "All fields little-endian — read via DataView(..., true): getUint32/getFloat32/getInt32 by field type",
        ha="center",
        va="center",
        fontsize=8,
        color=cw,
        style="italic",
        path_effects=_STROKE,
    )

    ax.set_title(
        ".fmesh binary layout (TypeScript parser view)",
        color=ct,
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    save(fig, "assets/19-pipeline-asset-viewer", "lesson_19_fmesh_layout.png")


# ---------------------------------------------------------------------------
# 2. Loading pipeline data flow
# ---------------------------------------------------------------------------


def diagram_loading_pipeline():
    """Data flow from fetch through parsers to Three.js scene graph."""
    fig, ax = plt.subplots(figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False, aspect=None)
    ax.set_xlim(-0.5, 12.0)
    ax.set_ylim(-1.0, 5.0)
    ax.axis("off")

    ct = STYLE["text"]
    c1 = STYLE["accent1"]
    c2 = STYLE["accent2"]
    c3 = STYLE["accent3"]
    c4 = STYLE["accent4"]
    cw = STYLE["warn"]

    # Main flow boxes (left to right)
    main_boxes = [
        (0.0, 2.5, 1.6, 1.2, c1, "fetch\n.fmesh", "/api/assets/\n{id}/file"),
        (2.0, 2.5, 1.6, 1.2, c2, "parseFmesh", "validate\nheader + data"),
        (4.0, 2.5, 1.6, 1.2, c3, "buildFmesh\nGeometry", "de-interleave\naddGroup()"),
        (6.0, 2.5, 1.6, 1.2, c4, "loadFmat\nMaterials", "PBR props\nfrom .fmat"),
        (8.0, 2.5, 1.6, 1.2, c1, "resolve\ntextures", ".ftex → PNG\nfallback"),
        (10.0, 2.5, 1.6, 1.2, cw, "THREE\n.Group", "mesh +\nmaterials"),
    ]

    for x, y, w, h, color, label, sublabel in main_boxes:
        rect = mpatches.FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.08",
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
            fontsize=8,
            fontweight="bold",
            color=ct,
            path_effects=_STROKE,
        )
        ax.text(
            x + w / 2,
            y + h * 0.22,
            sublabel,
            ha="center",
            va="center",
            fontsize=6.5,
            color=ct,
            alpha=0.8,
        )

    # Main flow arrows
    arrow_kw = dict(arrowstyle="->", color=ct, linewidth=1.5)
    for i in range(5):
        x_from = main_boxes[i][0] + main_boxes[i][2]
        x_to = main_boxes[i + 1][0]
        y_mid = 3.1
        ax.annotate("", xy=(x_to, y_mid), xytext=(x_from, y_mid), arrowprops=arrow_kw)

    # Companion file fetch (below main flow)
    companion_boxes = [
        (4.0, 0.5, 1.6, 1.0, c4, "fetch\n.fmat", "derive by\next replacement"),
        (8.0, 0.5, 1.6, 1.0, c1, "fetch\n.ftex", "BC7/BC5 +\nPNG fallback"),
    ]

    for x, y, w, h, color, label, sublabel in companion_boxes:
        rect = mpatches.FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.08",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.2,
            alpha=0.5,
        )
        ax.add_patch(rect)
        ax.text(
            x + w / 2,
            y + h * 0.65,
            label,
            ha="center",
            va="center",
            fontsize=7.5,
            fontweight="bold",
            color=ct,
            alpha=0.9,
        )
        ax.text(
            x + w / 2,
            y + h * 0.22,
            sublabel,
            ha="center",
            va="center",
            fontsize=6,
            color=ct,
            alpha=0.6,
        )

    # Arrows from companion fetches up to the main flow
    companion_arrow = dict(arrowstyle="->", color=STYLE["text_dim"], linewidth=1.2)
    ax.annotate("", xy=(6.8, 2.5), xytext=(5.6, 1.5), arrowprops=companion_arrow)
    ax.annotate("", xy=(8.8, 2.5), xytext=(8.8, 1.5), arrowprops=companion_arrow)

    # Cache annotation
    ax.text(
        3.0,
        4.3,
        "FmeshData cached — LOD switch rebuilds geometry only (steps 3–6)",
        ha="center",
        va="center",
        fontsize=7.5,
        color=cw,
        style="italic",
        path_effects=_STROKE,
    )
    # Bracket line from parseFmesh to buildGeometry
    ax.plot([2.0, 4.0 + 1.6], [4.1, 4.1], color=cw, linewidth=1, alpha=0.6)
    ax.plot([2.0, 2.0], [4.1, 4.0], color=cw, linewidth=1, alpha=0.6)
    ax.plot([4.0 + 1.6, 4.0 + 1.6], [4.1, 4.0], color=cw, linewidth=1, alpha=0.6)

    ax.set_title(
        "usePipelineModel loading sequence",
        color=ct,
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    save(fig, "assets/19-pipeline-asset-viewer", "lesson_19_loading_pipeline.png")


# ---------------------------------------------------------------------------
# 3. LOD switching — cached data, geometry rebuild
# ---------------------------------------------------------------------------


def diagram_lod_switching():
    """Show how LOD selection rebuilds geometry from cached binary data."""
    fig, ax = plt.subplots(figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False, aspect=None)
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-0.5, 5.5)
    ax.axis("off")

    ct = STYLE["text"]
    c1 = STYLE["accent1"]
    c2 = STYLE["accent2"]
    c3 = STYLE["accent3"]
    cw = STYLE["warn"]

    # Cached FmeshData (left side)
    cached_rect = mpatches.FancyBboxPatch(
        (0.3, 1.0),
        3.0,
        3.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=c1,
        linewidth=2,
        linestyle="--",
    )
    ax.add_patch(cached_rect)
    ax.text(
        1.8,
        4.2,
        "Cached FmeshData",
        ha="center",
        va="center",
        fontsize=10,
        fontweight="bold",
        color=c1,
        path_effects=_STROKE,
    )
    ax.text(
        1.8,
        3.7,
        "(fetched once, reused across LOD changes)",
        ha="center",
        va="center",
        fontsize=7,
        color=STYLE["text_dim"],
        style="italic",
    )

    # Contents of cached data
    cached_items = [
        ("header", 3.2),
        ("lods[0..N]", 2.8),
        ("vertexBuffer", 2.4),
        ("indices", 2.0),
    ]
    for label, y in cached_items:
        ax.text(
            1.8,
            y,
            label,
            ha="center",
            va="center",
            fontsize=8,
            color=ct,
            family="monospace",
            path_effects=_STROKE,
        )
        inner = mpatches.FancyBboxPatch(
            (0.7, y - 0.15),
            2.2,
            0.3,
            boxstyle="round,pad=0.05",
            facecolor=c1,
            edgecolor="none",
            alpha=0.15,
        )
        ax.add_patch(inner)

    # buildFmeshGeometry box (center)
    build_rect = mpatches.FancyBboxPatch(
        (4.2, 1.8),
        2.2,
        1.8,
        boxstyle="round,pad=0.1",
        facecolor=c2,
        edgecolor=ct,
        linewidth=1.5,
        alpha=0.85,
    )
    ax.add_patch(build_rect)
    ax.text(
        5.3,
        3.1,
        "buildFmesh\nGeometry",
        ha="center",
        va="center",
        fontsize=9,
        fontweight="bold",
        color=ct,
        path_effects=_STROKE,
    )
    ax.text(
        5.3,
        2.3,
        "lodIndex param\nselects submeshes",
        ha="center",
        va="center",
        fontsize=7,
        color=ct,
        alpha=0.8,
    )

    # LOD selector input
    ax.text(
        5.3,
        4.8,
        "LOD selector dropdown",
        ha="center",
        va="center",
        fontsize=8,
        fontweight="bold",
        color=cw,
        path_effects=_STROKE,
    )
    lod_arrow = dict(arrowstyle="->", color=cw, linewidth=1.5)
    ax.annotate("", xy=(5.3, 3.7), xytext=(5.3, 4.5), arrowprops=lod_arrow)

    # Arrow from cache to builder
    arrow_kw = dict(arrowstyle="->", color=ct, linewidth=1.5)
    ax.annotate("", xy=(4.2, 2.7), xytext=(3.3, 2.7), arrowprops=arrow_kw)

    # Output geometries (right side)
    outputs = [
        (7.2, 3.8, c1, "LOD 0", "1,200 tris\nfull detail"),
        (7.2, 2.3, c3, "LOD 1", "600 tris\n50% detail"),
        (7.2, 0.8, c2, "LOD 2", "300 tris\n25% detail"),
    ]

    for x, y, color, label, stats in outputs:
        rect = mpatches.FancyBboxPatch(
            (x, y),
            2.8,
            1.0,
            boxstyle="round,pad=0.08",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.2,
            alpha=0.7,
        )
        ax.add_patch(rect)
        ax.text(
            x + 1.4,
            y + 0.65,
            label,
            ha="center",
            va="center",
            fontsize=9,
            fontweight="bold",
            color=ct,
            path_effects=_STROKE,
        )
        ax.text(
            x + 1.4,
            y + 0.25,
            stats,
            ha="center",
            va="center",
            fontsize=7,
            color=ct,
            alpha=0.8,
        )

    # Arrows from builder to outputs
    for _, y, _, _, _ in outputs:
        ax.annotate(
            "",
            xy=(7.2, y + 0.5),
            xytext=(6.4, 2.7),
            arrowprops=dict(
                arrowstyle="->",
                color=STYLE["text_dim"],
                linewidth=1,
                connectionstyle="arc3,rad=0.15",
            ),
        )

    ax.set_title(
        "LOD switching — geometry rebuilt from cached data",
        color=ct,
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    save(fig, "assets/19-pipeline-asset-viewer", "lesson_19_lod_switching.png")


# ---------------------------------------------------------------------------
# Public list
# ---------------------------------------------------------------------------

DIAGRAMS = [
    diagram_fmesh_browser_layout,
    diagram_loading_pipeline,
    diagram_lod_switching,
]
