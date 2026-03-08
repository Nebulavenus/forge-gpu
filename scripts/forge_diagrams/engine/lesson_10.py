"""Diagrams for engine/10."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

from .._common import STYLE, save, setup_axes


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — rasterization_pipeline.png
# ---------------------------------------------------------------------------
def diagram_rasterization_pipeline():
    """Rasterization pipeline flow: vertices -> triangles -> pixels."""
    fig, ax = plt.subplots(figsize=(12, 4), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.0, 14)
    ax.set_ylim(-0.5, 3.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    stages = [
        ("Vertices", STYLE["accent1"], 0.5),
        ("Triangles", STYLE["accent2"], 3.5),
        ("Bounding\nBox", STYLE["accent3"], 6.5),
        ("Edge\nTest", STYLE["accent4"], 9.5),
        ("Pixels", STYLE["warn"], 12.5),
    ]

    for label, color, x in stages:
        box = FancyBboxPatch(
            (x - 0.9, 0.6),
            1.8,
            1.8,
            boxstyle="round,pad=0.15",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)
        ax.text(
            x,
            1.5,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Arrows between stages
    for i in range(len(stages) - 1):
        x_start = stages[i][2] + 1.0
        x_end = stages[i + 1][2] - 1.0
        ax.annotate(
            "",
            xy=(x_end, 1.5),
            xytext=(x_start, 1.5),
            arrowprops={
                "arrowstyle": "->,head_width=0.2",
                "color": STYLE["text_dim"],
                "lw": 2,
            },
        )

    # Sub-labels
    sublabels = [
        (0.5, "position, color,\nUV per vertex"),
        (3.5, "3 indices per\ntriangle"),
        (6.5, "clamp AABB\nto framebuffer"),
        (9.5, "orient2d >= 0\nfor all 3 edges"),
        (12.5, "interpolate,\nblend, write"),
    ]
    for x, text in sublabels:
        ax.text(
            x,
            -0.1,
            text,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
            path_effects=stroke,
        )

    # Title
    ax.text(
        6.75,
        3.2,
        "CPU Rasterization Pipeline",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "rasterization_pipeline.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — alpha_blending.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — edge_functions.png
# ---------------------------------------------------------------------------
def diagram_edge_functions():
    """Triangle with three edge function half-planes and orient2d labels."""
    fig, ax = plt.subplots(figsize=(8, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1, 11), ylim=(-1, 11), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Triangle vertices (CCW)
    v0 = np.array([5.0, 9.0])
    v1 = np.array([1.5, 1.5])
    v2 = np.array([9.0, 2.5])

    # Fill the triangle with a subtle surface color
    tri = Polygon(
        [v0, v1, v2],
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor="none",
        alpha=0.6,
    )
    ax.add_patch(tri)

    # Draw the three edges with different accent colors
    edges = [
        (v0, v1, STYLE["accent1"], "edge 0"),
        (v1, v2, STYLE["accent2"], "edge 1"),
        (v2, v0, STYLE["accent3"], "edge 2"),
    ]

    for a, b, color, _label in edges:
        ax.plot([a[0], b[0]], [a[1], b[1]], color=color, lw=2.5, zorder=3)
        mid = (a + b) / 2
        # Normal direction (pointing inward for CCW)
        d = b - a
        n = np.array([-d[1], d[0]])
        n = n / np.linalg.norm(n) * 0.6
        ax.annotate(
            "",
            xy=mid + n,
            xytext=mid,
            arrowprops={"arrowstyle": "->,head_width=0.15", "color": color, "lw": 1.5},
            zorder=4,
        )

    # Vertex labels
    for v, name, offset in [
        (v0, "v0", (0, 0.5)),
        (v1, "v1", (-0.5, -0.5)),
        (v2, "v2", (0.5, -0.5)),
    ]:
        ax.plot(v[0], v[1], "o", color=STYLE["text"], markersize=7, zorder=5)
        ax.text(
            v[0] + offset[0],
            v[1] + offset[1],
            name,
            color=STYLE["text"],
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # orient2d formula labels along each edge
    orient_labels = [
        (v0, v1, STYLE["accent1"], "orient2d(v0, v1, p)"),
        (v1, v2, STYLE["accent2"], "orient2d(v1, v2, p)"),
        (v2, v0, STYLE["accent3"], "orient2d(v2, v0, p)"),
    ]
    for a, b, color, label in orient_labels:
        mid = (a + b) / 2
        d = b - a
        n = np.array([-d[1], d[0]])
        n = n / np.linalg.norm(n) * 1.3
        ax.text(
            mid[0] + n[0],
            mid[1] + n[1],
            label,
            color=color,
            fontsize=9,
            fontstyle="italic",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Test points: one inside, one outside
    p_in = np.array([5.0, 4.5])
    p_out = np.array([1.5, 7.0])

    ax.plot(p_in[0], p_in[1], "s", color=STYLE["accent3"], markersize=10, zorder=5)
    ax.text(
        p_in[0] + 0.5,
        p_in[1] + 0.3,
        "inside\nall >= 0",
        color=STYLE["accent3"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    ax.plot(p_out[0], p_out[1], "X", color=STYLE["accent2"], markersize=10, zorder=5)
    ax.text(
        p_out[0] + 0.4,
        p_out[1] + 0.3,
        "outside\nmixed signs",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Title
    ax.text(
        5.0,
        10.5,
        "Edge Function Inside Test",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Formula below
    ax.text(
        5.0,
        -0.3,
        "orient2d(a, b, p) = (b.x-a.x)(p.y-a.y) - (b.y-a.y)(p.x-a.x)",
        color=STYLE["text_dim"],
        fontsize=10,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "edge_functions.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — barycentric_coords.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — barycentric_coords.png
# ---------------------------------------------------------------------------
def diagram_barycentric_coords():
    """RGB triangle showing barycentric coordinate weights as smooth color."""
    fig, ax = plt.subplots(figsize=(8, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-0.5, 10.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Triangle vertices
    v0 = np.array([5.0, 9.5])  # red (top)
    v1 = np.array([0.5, 0.8])  # green (bottom-left)
    v2 = np.array([9.5, 0.8])  # blue (bottom-right)

    # Rasterize the triangle with barycentric colors using matplotlib
    # Create a fine grid and compute barycentric coordinates
    resolution = 400
    x = np.linspace(-0.5, 10.5, resolution)
    y = np.linspace(-0.5, 10.5, resolution)
    xx, yy = np.meshgrid(x, y)

    # Compute orient2d for each point
    def orient2d(ax_, ay, bx, by, px, py):
        return (bx - ax_) * (py - ay) - (by - ay) * (px - ax_)

    area = orient2d(v0[0], v0[1], v1[0], v1[1], v2[0], v2[1])
    w0 = orient2d(v1[0], v1[1], v2[0], v2[1], xx, yy)
    w1 = orient2d(v2[0], v2[1], v0[0], v0[1], xx, yy)
    w2 = orient2d(v0[0], v0[1], v1[0], v1[1], xx, yy)

    b0 = w0 / area
    b1 = w1 / area
    b2 = w2 / area

    # Inside test
    inside = (b0 >= 0) & (b1 >= 0) & (b2 >= 0)

    # Build RGB image
    rgb = np.zeros((resolution, resolution, 4))
    rgb[inside, 0] = b0[inside]  # red from v0
    rgb[inside, 1] = b1[inside]  # green from v1
    rgb[inside, 2] = b2[inside]  # blue from v2
    rgb[inside, 3] = 1.0
    rgb = np.clip(rgb, 0, 1)

    ax.imshow(
        rgb,
        extent=[-0.5, 10.5, -0.5, 10.5],
        origin="lower",
        interpolation="bilinear",
        zorder=1,
    )

    # Vertex labels
    labels = [
        (v0, "v0\nb0 = 1.0", (0, 0.4), "#ff4444"),
        (v1, "v1\nb1 = 1.0", (-0.3, -0.5), "#44ff44"),
        (v2, "v2\nb2 = 1.0", (0.3, -0.5), "#4488ff"),
    ]
    for v, name, offset, color in labels:
        ax.plot(
            v[0],
            v[1],
            "o",
            color=color,
            markersize=9,
            markeredgecolor=STYLE["text"],
            markeredgewidth=1.5,
            zorder=5,
        )
        ax.text(
            v[0] + offset[0],
            v[1] + offset[1],
            name,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Center annotation
    center = (v0 + v1 + v2) / 3
    ax.plot(
        center[0],
        center[1],
        "+",
        color=STYLE["text"],
        markersize=12,
        markeredgewidth=2,
        zorder=5,
    )
    ax.text(
        center[0] + 0.8,
        center[1] + 0.3,
        "centroid\nb0 = b1 = b2 = 1/3",
        color=STYLE["text"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Title
    ax.text(
        5.0,
        10.2,
        "Barycentric Coordinates",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    ax.text(
        5.0,
        -0.2,
        "color(p) = b0 * color(v0) + b1 * color(v1) + b2 * color(v2)",
        color=STYLE["text_dim"],
        fontsize=10,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "barycentric_coords.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — rasterization_pipeline.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — bounding_box.png
# ---------------------------------------------------------------------------
def diagram_bounding_box():
    """Bounding box optimization: only test pixels inside the AABB."""
    fig, ax = plt.subplots(figsize=(8, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(-0.5, 12.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Triangle vertices
    v0 = np.array([6.0, 11.0])
    v1 = np.array([2.0, 3.0])
    v2 = np.array([10.5, 4.5])

    # Bounding box
    bb_min = np.array([min(v0[0], v1[0], v2[0]), min(v0[1], v1[1], v2[1])])
    bb_max = np.array([max(v0[0], v1[0], v2[0]), max(v0[1], v1[1], v2[1])])

    # Draw pixel grid inside bounding box
    for gx in range(int(bb_min[0]), int(bb_max[0]) + 1):
        for gy in range(int(bb_min[1]), int(bb_max[1]) + 1):
            px, py = gx + 0.5, gy + 0.5

            # Check if inside triangle
            def orient2d(ax_, ay, bx, by, ppx, ppy):
                return (bx - ax_) * (ppy - ay) - (by - ay) * (ppx - ax_)

            w0 = orient2d(v1[0], v1[1], v2[0], v2[1], px, py)
            w1 = orient2d(v2[0], v2[1], v0[0], v0[1], px, py)
            w2 = orient2d(v0[0], v0[1], v1[0], v1[1], px, py)
            inside = w0 >= 0 and w1 >= 0 and w2 >= 0

            color = STYLE["accent3"] + "40" if inside else STYLE["grid"]
            rect = Rectangle(
                (gx, gy),
                1,
                1,
                facecolor=color,
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                zorder=1,
            )
            ax.add_patch(rect)

            if inside:
                ax.plot(px, py, ".", color=STYLE["accent3"], markersize=4, zorder=3)

    # Bounding box outline
    bb_rect = Rectangle(
        (bb_min[0], bb_min[1]),
        bb_max[0] - bb_min[0] + 1,
        bb_max[1] - bb_min[1] + 1,
        facecolor="none",
        edgecolor=STYLE["warn"],
        linewidth=2,
        linestyle="--",
        zorder=4,
    )
    ax.add_patch(bb_rect)

    # Triangle outline
    tri = Polygon(
        [v0, v1, v2],
        closed=True,
        facecolor="none",
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        zorder=5,
    )
    ax.add_patch(tri)

    # Vertex markers
    for v, name in [(v0, "v0"), (v1, "v1"), (v2, "v2")]:
        ax.plot(v[0], v[1], "o", color=STYLE["accent1"], markersize=8, zorder=6)
        ax.text(
            v[0] + 0.4,
            v[1] + 0.3,
            name,
            color=STYLE["accent1"],
            fontsize=11,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
        )

    # Legend
    ax.text(
        0.2,
        0.8,
        "AABB (bounding box)",
        color=STYLE["warn"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
    )
    ax.plot(0.0, 0.8, "s", color=STYLE["warn"], markersize=6)

    ax.text(
        0.2,
        0.0,
        "inside (fragment emitted)",
        color=STYLE["accent3"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
    )
    ax.plot(0.0, 0.0, "s", color=STYLE["accent3"] + "60", markersize=6)

    # Title
    ax.text(
        6.0,
        12.2,
        "Bounding Box Optimization",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "bounding_box.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — indexed_quad.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — alpha_blending.png
# ---------------------------------------------------------------------------
def diagram_alpha_blending():
    """Source-over alpha compositing formula with visual example."""
    fig, axes = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [1.2, 1]},
    )

    # --- Left panel: formula ---
    ax = axes[0]
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 10)
    ax.set_ylim(-0.5, 7)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        5,
        6.5,
        "Source-Over Compositing",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    formulas = [
        (5.2, "out.rgb = src.rgb * src.a + dst.rgb * (1 - src.a)"),
        (4.2, "out.a   = src.a + dst.a * (1 - src.a)"),
    ]
    for y, text in formulas:
        ax.text(
            5,
            y,
            text,
            color=STYLE["accent1"],
            fontsize=11,
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    cases = [
        (2.8, "src.a = 1.0", "fully opaque  ->  dst is replaced", STYLE["accent3"]),
        (1.8, "src.a = 0.5", "50% blend  ->  equal mix", STYLE["warn"]),
        (0.8, "src.a = 0.0", "fully transparent  ->  dst unchanged", STYLE["text_dim"]),
    ]
    for y, left, right, color in cases:
        ax.text(
            2,
            y,
            left,
            color=color,
            fontsize=10,
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            7,
            y,
            right,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- Right panel: visual demo ---
    ax2 = axes[1]
    ax2.set_facecolor(STYLE["bg"])
    ax2.set_xlim(-0.5, 6)
    ax2.set_ylim(-0.5, 7)
    ax2.axis("off")

    ax2.text(
        2.75,
        6.5,
        "Visual Example",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Destination (white background)
    dst = Rectangle(
        (0.5, 3.5),
        2,
        2,
        facecolor="#e0e0e0",
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        zorder=1,
    )
    ax2.add_patch(dst)
    ax2.text(
        1.5,
        5.8,
        "dst (white)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Source (red, semi-transparent)
    src = Rectangle(
        (1.5, 2.5),
        2,
        2,
        facecolor="#ff4444",
        alpha=0.6,
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
        zorder=2,
    )
    ax2.add_patch(src)
    ax2.text(
        3.8,
        2.8,
        "src (red, a=0.6)",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Result arrow
    ax2.annotate(
        "",
        xy=(2.75, 1.2),
        xytext=(2.75, 2.3),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["warn"],
            "lw": 2,
        },
    )

    # Blended result
    blended_r = 1.0 * 0.6 + 0.88 * 0.4
    blended_g = 0.27 * 0.6 + 0.88 * 0.4
    blended_b = 0.27 * 0.6 + 0.88 * 0.4
    result_color = (blended_r, blended_g, blended_b)
    result = Rectangle(
        (1.75, 0.0),
        2,
        1,
        facecolor=result_color,
        edgecolor=STYLE["warn"],
        linewidth=2,
        zorder=2,
    )
    ax2.add_patch(result)
    ax2.text(
        2.75,
        0.5,
        "result",
        color=STYLE["bg"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "alpha_blending.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — bounding_box.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — indexed_quad.png
# ---------------------------------------------------------------------------
def diagram_indexed_quad():
    """Indexed drawing: 4 vertices + 6 indices = 1 quad (2 triangles)."""
    fig, ax = plt.subplots(figsize=(9, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 13)
    ax.set_ylim(-1.5, 7)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Left: Vertex buffer ---
    ax.text(
        2.5,
        6.5,
        "Vertex Buffer (4 vertices)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    vert_data = ["v0 (TL)", "v1 (TR)", "v2 (BR)", "v3 (BL)"]
    for i, label in enumerate(vert_data):
        y = 5.0 - i * 1.2
        box = FancyBboxPatch(
            (0.5, y - 0.4),
            4,
            0.8,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["accent1"] + "25",
            edgecolor=STYLE["accent1"],
            linewidth=1.5,
        )
        ax.add_patch(box)
        ax.text(
            0.7,
            y,
            f"[{i}]",
            color=STYLE["accent1"],
            fontsize=10,
            fontfamily="monospace",
            ha="left",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            2.5,
            y,
            label,
            color=STYLE["text"],
            fontsize=10,
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- Right: Index buffer ---
    ax.text(
        9.5,
        6.5,
        "Index Buffer (6 indices)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Triangle 0
    tri0_indices = [
        ("0", STYLE["accent2"]),
        ("1", STYLE["accent2"]),
        ("2", STYLE["accent2"]),
    ]
    ax.text(
        9.5,
        5.5,
        "Triangle 0",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    for j, (idx, color) in enumerate(tri0_indices):
        x = 8.0 + j * 1.0
        box = FancyBboxPatch(
            (x - 0.3, 4.3),
            0.6,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(box)
        ax.text(
            x,
            4.6,
            idx,
            color=color,
            fontsize=11,
            fontfamily="monospace",
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Triangle 1
    tri1_indices = [
        ("0", STYLE["accent3"]),
        ("2", STYLE["accent3"]),
        ("3", STYLE["accent3"]),
    ]
    ax.text(
        9.5,
        3.5,
        "Triangle 1",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    for j, (idx, color) in enumerate(tri1_indices):
        x = 8.0 + j * 1.0
        box = FancyBboxPatch(
            (x - 0.3, 2.3),
            0.6,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(box)
        ax.text(
            x,
            2.6,
            idx,
            color=color,
            fontsize=11,
            fontfamily="monospace",
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- Bottom: visual quad ---
    ax.text(
        5.5,
        0.7,
        "Result: 1 Quad",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    qverts = np.array([[3.5, 0.0], [7.5, 0.0], [7.5, -1.0], [3.5, -1.0]])
    tri_a = Polygon(
        [qverts[0], qverts[1], qverts[2]],
        closed=True,
        facecolor=STYLE["accent2"] + "30",
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
    )
    tri_b = Polygon(
        [qverts[0], qverts[2], qverts[3]],
        closed=True,
        facecolor=STYLE["accent3"] + "30",
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
    )
    ax.add_patch(tri_a)
    ax.add_patch(tri_b)

    # Diagonal line
    ax.plot(
        [qverts[0][0], qverts[2][0]],
        [qverts[0][1], qverts[2][1]],
        color=STYLE["text_dim"],
        linewidth=1,
        linestyle="--",
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "indexed_quad.png")


# ---------------------------------------------------------------------------
# engine/11-git-version-control — three_areas.png
# ---------------------------------------------------------------------------
