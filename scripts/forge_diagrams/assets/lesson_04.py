"""Diagrams for assets/04."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
from matplotlib.patches import Circle
import numpy as np

from .._common import STYLE, save, setup_axes

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
    circle = Circle(
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

    plt.tight_layout(rect=(0, 0, 1, 0.93))
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
    circle = Circle(
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

    plt.tight_layout(rect=(0, 0, 1, 0.93))
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

    plt.tight_layout(rect=(0, 0, 1, 0.93))
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

    plt.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "assets/04-procedural-geometry", "struct_of_arrays.png")
