"""Diagrams for asset pipeline lessons."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np

from ._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# Lesson 02 — Texture Processing
# ---------------------------------------------------------------------------


def diagram_texture_block_compression():
    """Show how a 4x4 pixel block is compressed into a fixed-size block."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Block Compression: 4x4 Pixels to Fixed-Size Block",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    # Left: 4x4 pixel grid with colors
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 3.5), ylim=(-0.5, 3.5), grid=False, aspect="equal")
    ax.set_title("Source: 4x4 Pixel Block", color=STYLE["text"], fontsize=11)

    rng = np.random.RandomState(42)
    base_r, base_g, base_b = 0.3, 0.5, 0.8
    for row in range(4):
        for col in range(4):
            r = np.clip(base_r + rng.uniform(-0.15, 0.15), 0, 1)
            g = np.clip(base_g + rng.uniform(-0.15, 0.15), 0, 1)
            b = np.clip(base_b + rng.uniform(-0.15, 0.15), 0, 1)
            rect = mpatches.FancyBboxPatch(
                (col - 0.45, row - 0.45),
                0.9,
                0.9,
                boxstyle="round,pad=0.02",
                facecolor=(r, g, b),
                edgecolor=STYLE["grid"],
                linewidth=0.5,
            )
            ax.add_patch(rect)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.text(
        1.5,
        -1.1,
        "64 bytes (RGBA8)\n16 pixels x 4 bytes",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )

    # Middle: arrow
    ax = axes[1]
    setup_axes(ax, xlim=(0, 4), ylim=(0, 4), grid=False, aspect="equal")
    ax.set_title("GPU Hardware\nDecompresses", color=STYLE["text"], fontsize=11)
    ax.annotate(
        "",
        xy=(3.5, 2),
        xytext=(0.5, 2),
        arrowprops={
            "arrowstyle": "->,head_width=0.4,head_length=0.2",
            "color": STYLE["accent1"],
            "lw": 3,
        },
    )
    ax.text(
        2,
        2.6,
        "Encode once",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        fontweight="bold",
    )
    ax.text(
        2,
        1.4,
        "Decode in HW\nat read time",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
    )
    ax.set_xticks([])
    ax.set_yticks([])

    # Right: compressed block representation
    ax = axes[2]
    setup_axes(ax, xlim=(-0.5, 7.5), ylim=(-0.5, 3.5), grid=False, aspect="equal")
    ax.set_title("BC7: 16-byte Block", color=STYLE["text"], fontsize=11)

    labels = [
        "Mode",
        "Part",
        "EP0",
        "EP0",
        "EP1",
        "EP1",
        "Idx",
        "Idx",
        "Idx",
        "Idx",
        "Idx",
        "Idx",
        "Idx",
        "Idx",
        "Idx",
        "Idx",
    ]
    colors_map = {
        "Mode": STYLE["accent4"],
        "Part": STYLE["accent3"],
        "EP0": STYLE["accent1"],
        "EP1": STYLE["accent2"],
        "Idx": STYLE["warn"],
    }
    for i, label in enumerate(labels):
        col = i % 8
        row = 2 - (i // 8)
        color = colors_map[label]
        rect = mpatches.FancyBboxPatch(
            (col - 0.4, row - 0.35),
            0.8,
            0.7,
            boxstyle="round,pad=0.02",
            facecolor=color,
            alpha=0.7,
            edgecolor=STYLE["grid"],
            linewidth=0.5,
        )
        ax.add_patch(rect)
        ax.text(
            col,
            row,
            label,
            color=STYLE["bg"],
            fontsize=7,
            ha="center",
            va="center",
            fontweight="bold",
        )

    ax.set_xticks([])
    ax.set_yticks([])
    ax.text(
        3.5,
        -1.1,
        "16 bytes fixed\n4:1 compression ratio",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )

    # Legend
    legend_items = [
        ("Mode bits", STYLE["accent4"]),
        ("Partition", STYLE["accent3"]),
        ("Endpoint 0", STYLE["accent1"]),
        ("Endpoint 1", STYLE["accent2"]),
        ("Index bits", STYLE["warn"]),
    ]
    patches = [
        mpatches.Patch(facecolor=c, label=name, edgecolor=STYLE["grid"])
        for name, c in legend_items
    ]
    fig.legend(
        handles=patches,
        loc="lower center",
        ncol=5,
        fontsize=8,
        framealpha=0.3,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    plt.tight_layout(rect=[0, 0.08, 1, 0.95])
    save(fig, "assets/02-texture-processing", "block_compression.png")


def diagram_texture_format_comparison():
    """Bar chart comparing bpp and quality for common GPU texture formats."""
    fig, ax = plt.subplots(figsize=(12, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False, aspect=None)

    formats = [
        "Uncompressed\nRGBA8",
        "BC1\n(DXT1)",
        "BC3\n(DXT5)",
        "BC4\n(1-ch)",
        "BC5\n(2-ch)",
        "BC7\n(RGBA)",
        "ETC2\n(RGB)",
        "ASTC\n4x4",
        "ASTC\n6x6",
        "ASTC\n8x8",
    ]
    bpp = [32, 4, 8, 4, 8, 8, 4, 8, 3.56, 2]
    # Relative quality (0-100 scale, approximate)
    quality = [100, 60, 75, 85, 90, 95, 65, 93, 80, 65]
    colors_bpp = [STYLE["accent2"] if b > 16 else STYLE["accent1"] for b in bpp]

    x = np.arange(len(formats))
    width = 0.35

    bars1 = ax.bar(
        x - width / 2,
        bpp,
        width,
        color=colors_bpp,
        alpha=0.85,
        edgecolor=STYLE["grid"],
        linewidth=0.5,
        label="Bits per pixel",
    )

    ax2 = ax.twinx()
    ax2.bar(
        x + width / 2,
        quality,
        width,
        color=STYLE["accent3"],
        alpha=0.6,
        edgecolor=STYLE["grid"],
        linewidth=0.5,
        label="Relative quality",
    )

    ax.set_xticks(x)
    ax.set_xticklabels(formats, color=STYLE["text"], fontsize=8)
    ax.set_ylabel("Bits per Pixel", color=STYLE["accent1"], fontsize=11)
    ax2.set_ylabel("Relative Quality (%)", color=STYLE["accent3"], fontsize=11)
    ax.set_title(
        "GPU Texture Format Comparison: Size vs Quality",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=15,
    )

    ax.tick_params(colors=STYLE["axis"])
    ax2.tick_params(colors=STYLE["axis"])
    ax.set_ylim(0, 36)
    ax2.set_ylim(0, 110)

    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
    for spine in ax2.spines.values():
        spine.set_color(STYLE["grid"])

    # Add bpp value labels on bars
    for bar, val in zip(bars1, bpp, strict=True):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.5,
            f"{val}",
            color=STYLE["text"],
            fontsize=8,
            ha="center",
            va="bottom",
        )

    # Combined legend
    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax.legend(
        lines1 + lines2,
        labels1 + labels2,
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    # Memory savings annotation
    ax.annotate(
        "2048x2048 texture:\nRGBA8 = 16 MB\nBC7 = 4 MB (4x saving)",
        xy=(0, 32),
        xytext=(3, 33),
        color=STYLE["warn"],
        fontsize=9,
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["warn"],
            "alpha": 0.8,
        },
    )

    plt.tight_layout()
    save(fig, "assets/02-texture-processing", "format_comparison.png")


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
        setup_axes(
            ax, xlim=(-1.5, 1.5), ylim=(-1.5, 1.5), grid=False, aspect="equal"
        )
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
        circle = plt.Circle(
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

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "assets/03-mesh-processing", "lod_simplification.png")
