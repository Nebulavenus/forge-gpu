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


# ---------------------------------------------------------------------------
# Lesson 04 — Procedural Geometry
# ---------------------------------------------------------------------------


def diagram_parametric_sphere():
    """Spherical coordinate system with parametric equations and wireframe."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Parametric Sphere: Spherical Coordinates",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    # --- Left panel: 3D-looking wireframe sphere ---
    ax = axes[0]
    setup_axes(ax, xlim=(-1.6, 1.6), ylim=(-1.6, 1.6), grid=False, aspect="equal")
    ax.set_title("Wireframe (Slices x Stacks)", color=STYLE["text"], fontsize=11)

    n_slices = 12
    n_stacks = 8

    # Draw latitude rings (stacks) — ellipses in 2D projection
    for i in range(1, n_stacks):
        theta = i * np.pi / n_stacks
        r = np.sin(theta)
        y_off = np.cos(theta)
        t = np.linspace(0, 2 * np.pi, 80)
        x_ring = r * np.cos(t)
        y_ring = y_off + r * np.sin(t) * 0.35  # foreshorten for 3D look
        color = STYLE["accent1"]
        alpha = 0.7
        if i == n_stacks // 2:
            color = STYLE["accent2"]
            alpha = 1.0
        ax.plot(x_ring, y_ring, color=color, alpha=alpha, linewidth=0.9)

    # Label equator
    ax.annotate(
        "Equator\n(theta = pi/2)",
        xy=(1.0, 0.0),
        xytext=(1.3, -0.7),
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent2"],
            "lw": 1.2,
        },
    )

    # Draw longitude lines (slices)
    for i in range(n_slices):
        phi = i * 2 * np.pi / n_slices
        theta_vals = np.linspace(0, np.pi, 60)
        x_lon = np.sin(theta_vals) * np.cos(phi)
        y_lon = np.cos(theta_vals) + np.sin(theta_vals) * np.sin(phi) * 0.35
        color = STYLE["accent3"]
        alpha = 0.5
        if i == 0:
            color = STYLE["warn"]
            alpha = 1.0
        ax.plot(x_lon, y_lon, color=color, alpha=alpha, linewidth=0.8)

    # Label poles
    ax.plot(0, 1.0, "o", color=STYLE["accent4"], markersize=6, zorder=5)
    ax.text(
        0.15,
        1.15,
        "North Pole\n(theta = 0)",
        color=STYLE["accent4"],
        fontsize=8,
        ha="left",
    )
    ax.plot(0, -1.0, "o", color=STYLE["accent4"], markersize=6, zorder=5)
    ax.text(
        0.15,
        -1.15,
        "South Pole\n(theta = pi)",
        color=STYLE["accent4"],
        fontsize=8,
        ha="left",
    )

    # Label seam line — arrow points to the meridian edge, not center
    ax.annotate(
        "Seam\n(U=0 / U=1)",
        xy=(1.0, 0),
        xytext=(1.5, 0.8),
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.2,
        },
    )

    # Outer silhouette
    circle = plt.Circle(
        (0, 0),
        1.0,
        fill=False,
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.4,
    )
    ax.add_patch(circle)
    ax.set_xticks([])
    ax.set_yticks([])

    # --- Right panel: parametric equations and coordinate labels ---
    ax = axes[1]
    setup_axes(ax, xlim=(0, 10), ylim=(0, 10), grid=False, aspect=None)
    ax.set_title("Parametric Equations", color=STYLE["text"], fontsize=11)

    equations = [
        (r"$x = \cos(\phi) \cdot \sin(\theta)$", STYLE["accent1"]),
        (r"$y = \cos(\theta)$", STYLE["accent3"]),
        (r"$z = \sin(\phi) \cdot \sin(\theta)$", STYLE["accent2"]),
    ]

    y_pos = 7.5
    for eq_text, eq_color in equations:
        ax.text(
            5,
            y_pos,
            eq_text,
            color=eq_color,
            fontsize=16,
            ha="center",
            va="center",
        )
        y_pos -= 1.5

    # Parameter ranges
    ax.text(
        5,
        3.2,
        r"$\theta \in [0, \pi]$  (polar angle / stacks)",
        color=STYLE["accent4"],
        fontsize=12,
        ha="center",
    )
    ax.text(
        5,
        2.0,
        r"$\phi \in [0, 2\pi]$  (longitude / slices)",
        color=STYLE["warn"],
        fontsize=12,
        ha="center",
    )

    # UV mapping note
    ax.text(
        5,
        0.7,
        "U = phi / (2 pi)    V = 1 - theta / pi",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["grid"],
            "alpha": 0.8,
        },
    )

    ax.set_xticks([])
    ax.set_yticks([])

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "assets/04-procedural-geometry", "parametric_sphere.png")


def diagram_seam_duplication():
    """UV unwrap of a sphere showing seam duplication and why slices+1 columns."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Sphere Seam: Why (slices + 1) Columns",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    # --- Left: 2D UV grid ---
    ax = axes[0]
    setup_axes(ax, xlim=(-0.15, 1.15), ylim=(-0.15, 1.15), grid=False, aspect="equal")
    ax.set_title("UV Unwrap", color=STYLE["text"], fontsize=11)

    n_cols = 9  # slices + 1
    n_rows = 7  # stacks + 1, matches n_stacks in 3D panel

    # Draw grid lines
    for i in range(n_cols):
        u = i / (n_cols - 1)
        color = STYLE["grid"]
        lw = 0.5
        if i == 0:
            color = STYLE["accent1"]
            lw = 2.5
        elif i == n_cols - 1:
            color = STYLE["accent2"]
            lw = 2.5
        ax.plot([u, u], [0, 1], color=color, linewidth=lw, alpha=0.8)

    for j in range(n_rows):
        v = j / (n_rows - 1)
        ax.plot([0, 1], [v, v], color=STYLE["grid"], linewidth=0.5, alpha=0.8)

    # Highlight U=0 column vertices
    for j in range(n_rows):
        v = j / (n_rows - 1)
        ax.plot(0, v, "o", color=STYLE["accent1"], markersize=5, zorder=5)

    # Highlight U=1 column vertices
    for j in range(n_rows):
        v = j / (n_rows - 1)
        ax.plot(1, v, "s", color=STYLE["accent2"], markersize=5, zorder=5)

    # Labels
    ax.set_xlabel("U", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("V", color=STYLE["text"], fontsize=11)
    ax.text(
        0.0,
        -0.1,
        "U = 0",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )
    ax.text(
        1.0,
        -0.1,
        "U = 1",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )

    # Annotation box
    ax.text(
        0.5,
        1.1,
        "Same 3D position, different U values",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        bbox={
            "boxstyle": "round,pad=0.3",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["warn"],
            "alpha": 0.8,
        },
    )

    # --- Right: 3D sphere showing the seam ---
    ax = axes[1]
    setup_axes(ax, xlim=(-1.6, 1.6), ylim=(-1.6, 1.6), grid=False, aspect="equal")
    ax.set_title("3D Sphere — Seam Location", color=STYLE["text"], fontsize=11)

    # Draw subtle wireframe
    n_slices = 8
    n_stacks = 6
    for i in range(1, n_stacks):
        theta = i * np.pi / n_stacks
        r = np.sin(theta)
        y_off = np.cos(theta)
        t = np.linspace(0, 2 * np.pi, 80)
        ax.plot(
            r * np.cos(t),
            y_off + r * np.sin(t) * 0.35,
            color=STYLE["grid"],
            alpha=0.5,
            linewidth=0.6,
        )

    for i in range(n_slices):
        phi = i * 2 * np.pi / n_slices
        theta_vals = np.linspace(0, np.pi, 60)
        x_lon = np.sin(theta_vals) * np.cos(phi)
        y_lon = np.cos(theta_vals) + np.sin(theta_vals) * np.sin(phi) * 0.35
        ax.plot(x_lon, y_lon, color=STYLE["grid"], alpha=0.4, linewidth=0.6)

    # Draw seam line (phi=0, front-facing longitude)
    theta_vals = np.linspace(0, np.pi, 60)
    seam_x = np.sin(theta_vals) * 1.0  # cos(0) = 1
    seam_y = np.cos(theta_vals)
    ax.plot(seam_x, seam_y, color=STYLE["warn"], linewidth=3.0, alpha=0.9, zorder=4)

    # Draw seam vertices in both colors overlapping
    # The +0.04 offset exaggerates the separation for visual clarity;
    # in the actual mesh, both vertices share the same 3D position.
    for j in range(n_stacks + 1):
        theta = j * np.pi / n_stacks
        sx = np.sin(theta)
        sy = np.cos(theta)
        ax.plot(sx, sy, "o", color=STYLE["accent1"], markersize=6, zorder=6)
        ax.plot(
            sx + 0.04,
            sy + 0.04,
            "s",
            color=STYLE["accent2"],
            markersize=5,
            zorder=6,
        )

    ax.annotate(
        "Seam: two vertices\nper 3D position\n(different U coords)",
        xy=(0.9, 0.0),
        xytext=(1.35, -0.8),
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.2,
        },
        bbox={
            "boxstyle": "round,pad=0.3",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["warn"],
            "alpha": 0.8,
        },
    )

    # Silhouette circle
    circle = plt.Circle(
        (0, 0),
        1.0,
        fill=False,
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.3,
    )
    ax.add_patch(circle)

    # Legend annotation at bottom
    ax.text(
        0,
        -1.45,
        "columns = slices + 1 (duplicate seam for correct UVs)\n"
        "U=1 markers offset +0.04 for visibility; actual positions coincide",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    ax.set_xticks([])
    ax.set_yticks([])

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "assets/04-procedural-geometry", "seam_duplication.png")


def diagram_smooth_vs_flat_normals():
    """Side-by-side comparison of smooth and flat shading normals."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Smooth Normals vs Flat Normals",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    # --- Left: Smooth normals ---
    ax = axes[0]
    setup_axes(ax, xlim=(-1.8, 1.8), ylim=(-1.8, 1.8), grid=False, aspect="equal")
    ax.set_title(
        "Smooth Normals", color=STYLE["accent1"], fontsize=12, fontweight="bold"
    )

    # Draw filled circle with gradient-like shading
    n_wedges = 36
    for i in range(n_wedges):
        a0 = i * 2 * np.pi / n_wedges
        a1 = (i + 1) * 2 * np.pi / n_wedges
        # Shade based on angle (simulate light from upper-left)
        light_dot = np.cos(a0 - np.pi / 4) * 0.5 + 0.5
        brightness = 0.15 + 0.45 * light_dot
        color = (
            brightness * 0.6 + 0.1,
            brightness * 0.7 + 0.15,
            brightness * 1.0 + 0.05,
        )
        color = tuple(min(c, 1.0) for c in color)
        theta_wedge = np.linspace(a0, a1, 10)
        xs = np.concatenate([[0], np.cos(theta_wedge)])
        ys = np.concatenate([[0], np.sin(theta_wedge)])
        ax.fill(xs, ys, color=color)

    # Draw smooth per-vertex normal arrows
    n_arrows = 12
    for i in range(n_arrows):
        angle = i * 2 * np.pi / n_arrows
        bx = np.cos(angle)
        by = np.sin(angle)
        dx = np.cos(angle) * 0.5
        dy = np.sin(angle) * 0.5
        ax.annotate(
            "",
            xy=(bx + dx, by + dy),
            xytext=(bx, by),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.08",
                "color": STYLE["accent3"],
                "lw": 1.8,
            },
        )
        ax.plot(bx, by, "o", color=STYLE["accent1"], markersize=3, zorder=5)

    ax.text(
        0,
        -1.65,
        "Per-vertex normals\ninterpolated across face",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )
    ax.set_xticks([])
    ax.set_yticks([])

    # --- Right: Flat normals ---
    ax = axes[1]
    setup_axes(ax, xlim=(-1.8, 1.8), ylim=(-1.8, 1.8), grid=False, aspect="equal")
    ax.set_title("Flat Normals", color=STYLE["accent2"], fontsize=12, fontweight="bold")

    # Draw faceted polygon (low poly circle)
    n_faces = 10
    vertices = []
    for i in range(n_faces):
        angle = i * 2 * np.pi / n_faces
        vertices.append((np.cos(angle), np.sin(angle)))

    for i in range(n_faces):
        v0 = vertices[i]
        v1 = vertices[(i + 1) % n_faces]
        # Face triangle from center to edge
        xs = [0, v0[0], v1[0]]
        ys = [0, v0[1], v1[1]]

        mid_angle = (i + 0.5) * 2 * np.pi / n_faces
        light_dot = np.cos(mid_angle - np.pi / 4) * 0.5 + 0.5
        brightness = 0.15 + 0.45 * light_dot
        color = (
            brightness * 1.0 + 0.05,
            brightness * 0.5 + 0.1,
            brightness * 0.3 + 0.05,
        )
        color = tuple(min(c, 1.0) for c in color)
        ax.fill(xs, ys, color=color, edgecolor=STYLE["grid"], linewidth=1.0)

        # Per-face normal arrow from face center
        cx = (v0[0] + v1[0]) / 3.0
        cy = (v0[1] + v1[1]) / 3.0
        nx = np.cos(mid_angle) * 0.5
        ny = np.sin(mid_angle) * 0.5
        ax.annotate(
            "",
            xy=(cx + nx, cy + ny),
            xytext=(cx, cy),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.08",
                "color": STYLE["accent3"],
                "lw": 1.8,
            },
        )

    # Draw edge vertices
    for vx, vy in vertices:
        ax.plot(vx, vy, "o", color=STYLE["accent2"], markersize=3, zorder=5)

    ax.text(
        0,
        -1.65,
        "Per-face normals\nconstant across face",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )
    ax.set_xticks([])
    ax.set_yticks([])

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "assets/04-procedural-geometry", "smooth_vs_flat_normals.png")


def diagram_struct_of_arrays():
    """Memory layout comparison: Struct of Arrays vs Array of Structs."""
    fig, axes = plt.subplots(2, 1, figsize=(14, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Memory Layout: Struct of Arrays vs Array of Structs",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    bar_height = 0.6

    # --- Top: Struct of Arrays (ForgeShape) ---
    ax = axes[0]
    setup_axes(ax, xlim=(-1, 15), ylim=(-0.5, 4.5), grid=False, aspect=None)
    ax.set_title(
        "Struct of Arrays (ForgeShape)",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
    )

    x_base = 0.5
    items = [
        ("positions[]", STYLE["accent1"], 12.0, "float3 x N vertices"),
        ("normals[]", STYLE["accent3"], 12.0, "float3 x N vertices"),
        ("uvs[]", STYLE["accent4"], 8.0, "float2 x N vertices"),
        ("indices[]", STYLE["warn"], 10.0, "uint32 x M indices"),
    ]

    for i, (label, color, width, desc) in enumerate(items):
        y = 3.5 - i * 1.0
        rect = mpatches.FancyBboxPatch(
            (x_base, y - bar_height / 2),
            width,
            bar_height,
            boxstyle="round,pad=0.05",
            facecolor=color,
            alpha=0.3,
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(rect)
        ax.text(
            x_base + 0.3,
            y,
            label,
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="left",
            va="center",
        )
        ax.text(
            x_base + width - 0.3,
            y,
            desc,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="center",
        )

    # Annotation
    ax.text(
        14,
        2.0,
        "Direct GPU\nbuffer upload",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["accent1"],
            "alpha": 0.8,
        },
    )

    ax.set_xticks([])
    ax.set_yticks([])

    # --- Bottom: Array of Structs ---
    ax = axes[1]
    setup_axes(ax, xlim=(-1, 15), ylim=(-0.5, 2.5), grid=False, aspect=None)
    ax.set_title(
        "Array of Structs (Interleaved)",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
    )

    # Draw interleaved blocks
    x = 0.5
    y = 1.5
    block_w = 1.0
    gap = 0.08
    components = [
        ("pos", STYLE["accent1"]),
        ("nrm", STYLE["accent3"]),
        ("uv", STYLE["accent4"]),
    ]

    for vertex_i in range(4):
        for comp_label, comp_color in components:
            rect = mpatches.FancyBboxPatch(
                (x, y - bar_height / 2),
                block_w,
                bar_height,
                boxstyle="round,pad=0.03",
                facecolor=comp_color,
                alpha=0.35,
                edgecolor=comp_color,
                linewidth=1.5,
            )
            ax.add_patch(rect)
            ax.text(
                x + block_w / 2,
                y,
                comp_label,
                color=STYLE["text"],
                fontsize=7,
                fontweight="bold",
                ha="center",
                va="center",
            )
            x += block_w + gap

        # Vertex separator
        if vertex_i < 3:
            ax.plot(
                [x - gap / 2, x - gap / 2],
                [y - 0.5, y + 0.5],
                color=STYLE["text_dim"],
                linewidth=0.5,
                linestyle="--",
                alpha=0.5,
            )

    # Ellipsis
    ax.text(x + 0.3, y, "...", color=STYLE["text_dim"], fontsize=14, va="center")

    # Vertex labels below
    for i in range(4):
        vx = 0.5 + i * (3 * (block_w + gap)) + 1.5 * block_w
        ax.text(
            vx,
            y - 0.55,
            f"V{i}",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
        )

    # Annotation
    ax.text(
        14,
        1.0,
        "Requires stride\nor copy",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["accent2"],
            "alpha": 0.8,
        },
    )

    ax.set_xticks([])
    ax.set_yticks([])

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "assets/04-procedural-geometry", "struct_of_arrays.png")
