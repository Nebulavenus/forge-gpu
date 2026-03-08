"""Diagrams for gpu/36."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, FancyBboxPatch, Polygon, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/36-edge-detection — sobel_kernels.png
# ---------------------------------------------------------------------------


def diagram_sobel_kernels():
    """Sobel Gx and Gy 3x3 kernels with gradient magnitude formula."""
    fig = plt.figure(figsize=(10, 5.5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Gx kernel (left) ---
    ax1 = fig.add_axes([0.04, 0.22, 0.38, 0.62])
    setup_axes(ax1, xlim=(-0.5, 2.5), ylim=(-0.5, 2.5), grid=False, aspect="equal")
    ax1.set_xticks([])
    ax1.set_yticks([])
    ax1.set_title(
        "Gx (horizontal gradient)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    gx = [[-1, 0, 1], [-2, 0, 2], [-1, 0, 1]]
    for r in range(3):
        for c in range(3):
            val = gx[r][c]
            if val < 0:
                fc = STYLE["accent1"]
                tc = STYLE["bg"]
            elif val > 0:
                fc = STYLE["accent2"]
                tc = STYLE["bg"]
            else:
                fc = STYLE["surface"]
                tc = STYLE["text_dim"]
            rect = Rectangle(
                (c - 0.45, 2 - r - 0.45),
                0.9,
                0.9,
                facecolor=fc,
                edgecolor=STYLE["grid"],
                linewidth=1.5,
                zorder=2,
            )
            ax1.add_patch(rect)
            ax1.text(
                c,
                2 - r,
                str(val),
                color=tc,
                fontsize=16,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    # --- Gy kernel (right) ---
    ax2 = fig.add_axes([0.46, 0.22, 0.38, 0.62])
    setup_axes(ax2, xlim=(-0.5, 2.5), ylim=(-0.5, 2.5), grid=False, aspect="equal")
    ax2.set_xticks([])
    ax2.set_yticks([])
    ax2.set_title(
        "Gy (vertical gradient)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    gy = [[-1, -2, -1], [0, 0, 0], [1, 2, 1]]
    for r in range(3):
        for c in range(3):
            val = gy[r][c]
            if val < 0:
                fc = STYLE["accent1"]
                tc = STYLE["bg"]
            elif val > 0:
                fc = STYLE["accent2"]
                tc = STYLE["bg"]
            else:
                fc = STYLE["surface"]
                tc = STYLE["text_dim"]
            rect = Rectangle(
                (c - 0.45, 2 - r - 0.45),
                0.9,
                0.9,
                facecolor=fc,
                edgecolor=STYLE["grid"],
                linewidth=1.5,
                zorder=2,
            )
            ax2.add_patch(rect)
            ax2.text(
                c,
                2 - r,
                str(val),
                color=tc,
                fontsize=16,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    # --- Color legend ---
    ax_leg = fig.add_axes([0.86, 0.40, 0.12, 0.30])
    ax_leg.set_facecolor(STYLE["bg"])
    ax_leg.axis("off")
    ax_leg.text(
        0.0,
        0.85,
        "Negative",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        va="center",
        path_effects=stroke,
    )
    ax_leg.text(
        0.0,
        0.50,
        "Zero",
        color=STYLE["text_dim"],
        fontsize=9,
        fontweight="bold",
        va="center",
        path_effects=stroke,
    )
    ax_leg.text(
        0.0,
        0.15,
        "Positive",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        va="center",
        path_effects=stroke,
    )

    # --- Formula at bottom ---
    fig.text(
        0.44,
        0.06,
        r"$G = \sqrt{G_x^2 + G_y^2}$          edge = smoothstep(threshold, threshold $\times$ 2, G)",
        color=STYLE["warn"],
        fontsize=12,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    save(fig, "gpu/36-edge-detection", "sobel_kernels.png")


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — depth_vs_normal_edges.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — depth_vs_normal_edges.png
# ---------------------------------------------------------------------------


def diagram_depth_vs_normal_edges():
    """3-panel comparison: depth edges, normal edges, combined edges."""
    fig, axes = plt.subplots(1, 3, figsize=(15, 5.5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    titles = ["Depth Edges", "Normal Edges", "Combined"]
    colors = [STYLE["accent1"], STYLE["accent3"], STYLE["accent2"]]
    descs = [
        "Detects silhouettes where\ndepth changes abruptly",
        "Detects creases where\nnormals differ on same surface",
        "Best of both: union of\ndepth + normal edges",
    ]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(0, 10), ylim=(0, 8), grid=False, aspect="equal")
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_title(
            titles[idx], color=colors[idx], fontsize=12, fontweight="bold", pad=12
        )

        # Draw ground plane
        ax.fill_between([0, 10], [1.5, 1.5], [0, 0], color=STYLE["surface"], zorder=1)
        ax.plot([0, 10], [1.5, 1.5], color=STYLE["grid"], lw=1.5, zorder=2)

        # Draw a cube
        cube_pts = np.array([[2.5, 1.5], [2.5, 4.5], [5.5, 4.5], [5.5, 1.5]])
        cube_top = np.array([[2.5, 4.5], [4.0, 5.5], [7.0, 5.5], [5.5, 4.5]])
        cube_side = np.array([[5.5, 1.5], [5.5, 4.5], [7.0, 5.5], [7.0, 2.5]])

        ax.add_patch(
            Polygon(
                cube_pts,
                closed=True,
                facecolor=STYLE["surface"],
                edgecolor=STYLE["grid"],
                linewidth=1,
                zorder=3,
            )
        )
        ax.add_patch(
            Polygon(
                cube_top,
                closed=True,
                facecolor="#303060",
                edgecolor=STYLE["grid"],
                linewidth=1,
                zorder=3,
            )
        )
        ax.add_patch(
            Polygon(
                cube_side,
                closed=True,
                facecolor="#202040",
                edgecolor=STYLE["grid"],
                linewidth=1,
                zorder=3,
            )
        )

        edge_lw = 3.0

        if idx == 0:  # Depth edges — silhouette + depth discontinuities
            # Outer silhouette edges plus the front-to-side boundary where
            # depth changes across the edge (Sobel detects this gradient)
            for pts in [
                [(2.5, 1.5), (2.5, 4.5)],
                [(2.5, 4.5), (4.0, 5.5)],
                [(4.0, 5.5), (7.0, 5.5)],
                [(7.0, 5.5), (7.0, 2.5)],
                [(7.0, 2.5), (5.5, 1.5)],
                [(2.5, 1.5), (5.5, 1.5)],
                [(5.5, 1.5), (5.5, 4.5)],  # front/side depth boundary
            ]:
                ax.plot(
                    *zip(*pts, strict=True), color=colors[idx], lw=edge_lw, zorder=6
                )
            # Depth catches silhouettes and depth-discontinuity creases
            ax.text(
                5.0,
                0.5,
                "Misses same-depth creases\n(e.g. top/front boundary)",
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="center",
                fontstyle="italic",
                path_effects=stroke,
                zorder=7,
            )

        elif idx == 1:  # Normal edges — creases
            # Internal crease edges (where normals differ)
            for pts in [
                [(2.5, 4.5), (5.5, 4.5)],
                [(5.5, 4.5), (5.5, 1.5)],
                [(5.5, 4.5), (7.0, 5.5)],
            ]:
                ax.plot(
                    *zip(*pts, strict=True), color=colors[idx], lw=edge_lw, zorder=6
                )
            # Floor-cube boundary
            ax.plot([2.5, 5.5], [1.5, 1.5], color=colors[idx], lw=edge_lw, zorder=6)
            ax.text(
                5.0,
                0.5,
                "Catches face boundaries",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                va="center",
                fontstyle="italic",
                path_effects=stroke,
                zorder=7,
            )

        else:  # Combined
            # All edges
            for pts in [
                [(2.5, 1.5), (2.5, 4.5)],
                [(2.5, 4.5), (4.0, 5.5)],
                [(4.0, 5.5), (7.0, 5.5)],
                [(7.0, 5.5), (7.0, 2.5)],
                [(7.0, 2.5), (5.5, 1.5)],
                [(2.5, 1.5), (5.5, 1.5)],
                [(2.5, 4.5), (5.5, 4.5)],
                [(5.5, 4.5), (5.5, 1.5)],
                [(5.5, 4.5), (7.0, 5.5)],
            ]:
                ax.plot(
                    *zip(*pts, strict=True), color=colors[idx], lw=edge_lw, zorder=6
                )
            ax.text(
                5.0,
                0.5,
                "Complete edge coverage",
                color=STYLE["warn"],
                fontsize=8,
                ha="center",
                va="center",
                fontweight="bold",
                path_effects=stroke,
                zorder=7,
            )

    # Description text below each panel
    for idx, ax in enumerate(axes):
        ax.text(
            5.0,
            -0.3,
            descs[idx],
            color=STYLE["text"],
            fontsize=8,
            ha="center",
            va="top",
            path_effects=stroke,
            transform=ax.transData,
            zorder=7,
        )

    fig.tight_layout()
    save(fig, "gpu/36-edge-detection", "depth_vs_normal_edges.png")


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — gbuffer_layout.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — gbuffer_layout.png
# ---------------------------------------------------------------------------


def diagram_edge_detection_gbuffer_layout():
    """MRT G-buffer layout: Color0, Color1 (normals), Depth-Stencil."""
    fig = plt.figure(figsize=(13, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 14), ylim=(-1, 6.5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Scene pass source box at top
    src_box = FancyBboxPatch(
        (4.5, 4.8),
        4.5,
        1.2,
        boxstyle="round,pad=0.12",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        linewidth=2.5,
        zorder=3,
    )
    ax.add_patch(src_box)
    ax.text(
        6.75,
        5.4,
        "Scene Pass (MRT)",
        color=STYLE["accent4"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Three G-buffer targets
    targets = [
        (
            0.0,
            "Color0\n(SV_Target0)",
            "R8G8B8A8_UNORM",
            "Lit scene color\nBlinn-Phong + shadows",
            STYLE["accent1"],
        ),
        (
            4.5,
            "Color1\n(SV_Target1)",
            "R16G16B16A16_FLOAT",
            "View-space normals\nRaw XYZ, no encoding",
            STYLE["accent3"],
        ),
        (
            9.0,
            "Depth-Stencil\n(attachment)",
            "D24_UNORM_S8_UINT",
            "24-bit depth + 8-bit stencil\nSamplable for post-process",
            STYLE["accent2"],
        ),
    ]

    for x, name, fmt, desc, color in targets:
        box = FancyBboxPatch(
            (x, 0.5),
            4.0,
            3.2,
            boxstyle="round,pad=0.10",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.0,
            zorder=3,
        )
        ax.add_patch(box)
        ax.text(
            x + 2.0,
            3.2,
            name,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            x + 2.0,
            2.2,
            fmt,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke_thin,
            zorder=5,
        )
        ax.text(
            x + 2.0,
            1.3,
            desc,
            color=STYLE["text"],
            fontsize=8,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Arrow from source to target
        arrow_x = x + 2.0
        ax.annotate(
            "",
            xy=(arrow_x, 3.72),
            xytext=(6.75, 4.8),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.12",
                "color": color,
                "lw": 2.0,
                "connectionstyle": "arc3,rad=0",
            },
            zorder=4,
        )

    ax.set_title(
        "G-Buffer Layout: Multiple Render Targets",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/36-edge-detection", "gbuffer_layout.png")


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — xray_depth_fail.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — xray_depth_fail.png
# ---------------------------------------------------------------------------


def diagram_xray_depth_fail():
    """4-step X-ray technique: scene, mark (depth_fail), ghost, result."""
    fig, axes = plt.subplots(1, 4, figsize=(18, 5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    steps = [
        ("1. Normal Scene", "Wall occludes target"),
        (
            "2. Mark: depth_fail=INCREMENT_AND_WRAP",
            "Stencil++ where target\nfails depth vs wall",
        ),
        ("3. Ghost: stencil != 0", "Fresnel silhouette where\nstencil was marked"),
        ("4. Final Result", "Ghost visible through wall"),
    ]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(0, 10), ylim=(0, 8), grid=False, aspect="equal")
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_title(
            steps[idx][0], color=STYLE["text"], fontsize=10, fontweight="bold", pad=10
        )

        # Camera icon (left side)
        ax.plot([0.5, 1.5], [4.0, 4.0], color=STYLE["text_dim"], lw=2, zorder=2)
        ax.plot([0.5, 0.5], [3.5, 4.5], color=STYLE["text_dim"], lw=2, zorder=2)
        ax.text(
            1.0,
            3.2,
            "cam",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Wall (in the middle)
        wall = Rectangle(
            (4.0, 1.0),
            0.6,
            5.5,
            facecolor=STYLE["grid"],
            edgecolor=STYLE["text_dim"],
            linewidth=1.5,
            zorder=3,
        )
        ax.add_patch(wall)
        ax.text(
            4.3,
            7.0,
            "wall",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Target object (behind wall)
        target_alpha = 0.3 if idx == 0 else 0.5
        target_color = STYLE["accent1"] if idx >= 2 else STYLE["text_dim"]

        target = Circle(
            (7.5, 4.0),
            1.2,
            facecolor=target_color if idx >= 2 else STYLE["surface"],
            edgecolor=target_color,
            linewidth=2,
            alpha=target_alpha,
            zorder=2,
        )
        ax.add_patch(target)
        ax.text(
            7.5,
            4.0,
            "target",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # View rays
        ax.plot(
            [1.5, 4.0],
            [4.5, 5.0],
            "--",
            color=STYLE["text_dim"],
            lw=0.8,
            alpha=0.5,
            zorder=1,
        )
        ax.plot(
            [1.5, 4.0],
            [3.5, 3.0],
            "--",
            color=STYLE["text_dim"],
            lw=0.8,
            alpha=0.5,
            zorder=1,
        )

        if idx == 1:  # Mark pass — show stencil increment
            # Rays penetrating to target
            ax.plot(
                [4.6, 7.5],
                [5.0, 5.2],
                "-",
                color=STYLE["warn"],
                lw=1.5,
                alpha=0.7,
                zorder=4,
            )
            ax.plot(
                [4.6, 7.5],
                [3.0, 2.8],
                "-",
                color=STYLE["warn"],
                lw=1.5,
                alpha=0.7,
                zorder=4,
            )
            ax.text(
                6.0,
                6.5,
                "depth test FAILS\nstencil++",
                color=STYLE["warn"],
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

        elif idx == 2:  # Ghost pass — Fresnel rim
            ghost = Circle(
                (7.5, 4.0),
                1.25,
                facecolor="none",
                edgecolor=STYLE["accent1"],
                linewidth=3,
                linestyle="-",
                alpha=0.8,
                zorder=4,
            )
            ax.add_patch(ghost)
            ax.text(
                7.5,
                6.5,
                "stencil != 0:\ndraw ghost",
                color=STYLE["accent1"],
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

        elif idx == 3:  # Final result — ghost visible through wall
            ghost = Circle(
                (7.5, 4.0),
                1.25,
                facecolor=STYLE["accent1"],
                edgecolor=STYLE["accent1"],
                linewidth=3,
                alpha=0.4,
                zorder=4,
            )
            ax.add_patch(ghost)
            # Ghost rim on the wall surface
            rim_pts = np.array([[4.0, 2.5], [4.0, 5.5]])
            ax.plot(
                rim_pts[:, 0],
                rim_pts[:, 1],
                color=STYLE["accent1"],
                lw=4,
                alpha=0.6,
                zorder=5,
            )

        # Step description
        ax.text(
            5.0,
            0.2,
            steps[idx][1],
            color=STYLE["text"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    fig.suptitle(
        "X-Ray Vision: Stencil depth_fail_op Technique",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "gpu/36-edge-detection", "xray_depth_fail.png")


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — outline_method_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — outline_method_comparison.png
# ---------------------------------------------------------------------------


def diagram_outline_method_comparison():
    """Compare 3 outline methods: Stencil Expansion, Sobel, Jump Flood."""
    fig = plt.figure(figsize=(14, 7), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    methods = [
        {
            "title": "Stencil Expansion\n(Lesson 34)",
            "color": STYLE["accent1"],
            "space": "Object-space",
            "width": "Approximate (world units)",
            "cost": "2 draw calls per object",
            "pros": [
                "Roughly uniform width",
                "Simple to implement",
                "Works per-object",
            ],
            "cons": [
                "Requires scaled redraw",
                "Varies at silhouette edges",
                "No interior edges",
            ],
        },
        {
            "title": "Sobel Post-Process\n(Lesson 36)",
            "color": STYLE["accent2"],
            "space": "Screen-space",
            "width": "Variable (pixel-based)",
            "cost": "1 fullscreen pass",
            "pros": [
                "Catches all edges",
                "Screen-space: cheap",
                "Depth + normal combo",
            ],
            "cons": [
                "Variable width outlines",
                "Threshold tuning needed",
                "Requires G-buffer",
            ],
        },
        {
            "title": "Jump Flood\n(Future)",
            "color": STYLE["accent3"],
            "space": "Screen-space",
            "width": "Uniform (pixels)",
            "cost": "ceil(log2(max(w,h))) passes",
            "pros": [
                "Uniform pixel width",
                "Distance field for free",
                "Arbitrary shapes",
            ],
            "cons": [
                "Multiple passes needed",
                "Higher GPU bandwidth",
                "Complex implementation",
            ],
        },
    ]

    for i, m in enumerate(methods):
        ax = fig.add_axes([0.02 + i * 0.33, 0.05, 0.31, 0.82])
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(0, 10)
        ax.set_ylim(0, 14)
        ax.axis("off")

        # Title box
        title_box = FancyBboxPatch(
            (0.3, 11.5),
            9.4,
            2.0,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=m["color"],
            linewidth=2.5,
            zorder=3,
        )
        ax.add_patch(title_box)
        ax.text(
            5.0,
            12.5,
            m["title"],
            color=m["color"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Properties
        props = [
            ("Space:", m["space"]),
            ("Width:", m["width"]),
            ("Cost:", m["cost"]),
        ]
        y_pos = 10.5
        for label, val in props:
            ax.text(
                0.5,
                y_pos,
                label,
                color=STYLE["text_dim"],
                fontsize=9,
                fontweight="bold",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )
            ax.text(
                3.0,
                y_pos,
                val,
                color=STYLE["text"],
                fontsize=9,
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )
            y_pos -= 0.8

        # Pros
        y_pos -= 0.4
        ax.text(
            0.5,
            y_pos,
            "Strengths:",
            color=STYLE["accent3"],
            fontsize=9,
            fontweight="bold",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        y_pos -= 0.6
        for pro in m["pros"]:
            ax.text(
                1.0,
                y_pos,
                f"+ {pro}",
                color=STYLE["accent3"],
                fontsize=8,
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )
            y_pos -= 0.6

        # Cons
        y_pos -= 0.3
        ax.text(
            0.5,
            y_pos,
            "Limitations:",
            color=STYLE["accent2"],
            fontsize=9,
            fontweight="bold",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        y_pos -= 0.6
        for con in m["cons"]:
            ax.text(
                1.0,
                y_pos,
                f"- {con}",
                color=STYLE["accent2"],
                fontsize=8,
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )
            y_pos -= 0.6

    fig.suptitle(
        "Outline Methods Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )
    save(fig, "gpu/36-edge-detection", "outline_method_comparison.png")


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — edge_detection_pipeline.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — edge_detection_pipeline.png
# ---------------------------------------------------------------------------


def diagram_edge_detection_pipeline():
    """Full render pipeline: Shadow -> G-Buffer -> Edge -> Mark -> Ghost."""
    fig, ax = plt.subplots(1, 1, figsize=(16, 7.5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 17), ylim=(-0.5, 8.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    def draw_pass_box(cx, cy, w, h, title, color, lines):
        rect = FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.5,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy + h / 2 - 0.35,
            title,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke,
            zorder=5,
        )
        for i, (label, lcolor) in enumerate(lines):
            ax.text(
                cx,
                cy + 0.05 - i * 0.5,
                label,
                color=lcolor,
                fontsize=7.5,
                fontfamily="monospace",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    def draw_arrow(x1, y1, x2, y2, color, label=None):
        ax.annotate(
            "",
            xy=(x2, y2),
            xytext=(x1, y1),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.12",
                "color": color,
                "lw": 2.0,
            },
            zorder=3,
        )
        if label:
            mx, my = (x1 + x2) / 2, (y1 + y2) / 2
            ax.text(
                mx,
                my + 0.3,
                label,
                color=color,
                fontsize=7,
                ha="center",
                va="bottom",
                path_effects=stroke_thin,
                zorder=5,
            )

    # Pass 1: Shadow
    draw_pass_box(
        2.0,
        6.5,
        3.2,
        2.0,
        "Pass 1: Shadow",
        STYLE["text_dim"],
        [
            ("depth-only", STYLE["text_dim"]),
            ("D32_FLOAT 2048x2048", STYLE["text_dim"]),
        ],
    )

    # Pass 2: G-Buffer
    draw_pass_box(
        6.5,
        6.5,
        3.8,
        2.0,
        "Pass 2: G-Buffer (MRT)",
        STYLE["accent1"],
        [
            ("Color0: lit scene", STYLE["accent1"]),
            ("Color1: normals (R16F)", STYLE["accent3"]),
            ("DS: D24S8", STYLE["accent2"]),
        ],
    )

    # Pass 3: Edge Composite
    draw_pass_box(
        11.5,
        6.5,
        3.5,
        2.0,
        "Pass 3: Edge Detect",
        STYLE["warn"],
        [
            ("fullscreen triangle", STYLE["warn"]),
            ("Sobel on depth+normals", STYLE["text"]),
            ("output: swapchain", STYLE["text"]),
        ],
    )

    # Pass 4: Mark
    draw_pass_box(
        11.5,
        2.5,
        3.5,
        2.0,
        "Pass 4: X-Ray Mark",
        STYLE["accent4"],
        [
            ("depth_fail = INCREMENT_AND_WRAP", STYLE["accent4"]),
            ("color mask = 0", STYLE["text_dim"]),
            ("stencil write only", STYLE["text"]),
        ],
    )

    # Pass 5: Ghost
    draw_pass_box(
        15.5,
        2.5,
        2.5,
        2.0,
        "Pass 5: Ghost",
        STYLE["accent1"],
        [
            ("stencil != 0", STYLE["accent1"]),
            ("Fresnel rim", STYLE["text"]),
            ("additive blend", STYLE["text"]),
        ],
    )

    # Arrows: flow
    draw_arrow(3.6, 6.5, 4.6, 6.5, STYLE["text_dim"], "shadow_depth")
    draw_arrow(8.4, 6.5, 9.75, 6.5, STYLE["accent1"], "color + depth + normals")
    draw_arrow(13.25, 6.5, 15.5, 6.5, STYLE["warn"])

    # Output box
    out_box = FancyBboxPatch(
        (15.0, 5.8),
        1.8,
        1.4,
        boxstyle="round,pad=0.10",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["warn"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(out_box)
    ax.text(
        15.9,
        6.5,
        "Final\nOutput",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Conditional branch for X-ray mode
    ax.text(
        8.5,
        4.5,
        "X-ray mode only",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        fontstyle="italic",
        path_effects=stroke,
        zorder=5,
    )

    # D24S8 feeds into Mark pass
    draw_arrow(6.5, 5.5, 6.5, 4.2, STYLE["accent2"])
    ax.text(
        5.5,
        4.7,
        "D24S8",
        color=STYLE["accent2"],
        fontsize=7,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Arrow from depth to mark
    draw_arrow(6.5, 4.2, 9.75, 3.0, STYLE["accent2"], "depth (LOAD)")

    # Arrow from mark to ghost
    draw_arrow(13.25, 2.5, 14.25, 2.5, STYLE["accent4"], "stencil")

    # Ghost to output
    draw_arrow(15.5, 3.5, 15.9, 5.8, STYLE["accent1"])

    # Dashed box around passes 4-5 (conditional)
    cond_rect = Rectangle(
        (9.3, 1.0),
        9.2,
        4.2,
        facecolor="none",
        edgecolor=STYLE["accent4"],
        linewidth=1.5,
        linestyle="--",
        zorder=1,
    )
    ax.add_patch(cond_rect)

    ax.set_title(
        "Edge Detection Render Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/36-edge-detection", "edge_detection_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — color_id_picking.png
# ---------------------------------------------------------------------------
