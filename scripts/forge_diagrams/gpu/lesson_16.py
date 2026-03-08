"""Diagrams for gpu/16."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

from .._common import STYLE, save, setup_axes


def diagram_blend_modes():
    """Four blend modes compared — Opaque, Alpha Test, Alpha Blend, Additive.

    A 2x2 grid showing how each mode combines a foreground color (src) with a
    background color (dst). Each panel renders overlapping colored rectangles
    and computes the actual blended result in the overlap region.
    """
    fig, axes = plt.subplots(2, 2, figsize=(11, 8), facecolor=STYLE["bg"])

    # Source and destination colors (linear RGB, 0–1)
    src_rgb = np.array([0.2, 0.5, 1.0])  # blue-ish (accent1-like)
    src_alpha = 0.5
    dst_rgb = np.array([1.0, 0.4, 0.2])  # orange-ish (accent2-like)
    dst_alpha = 1.0

    modes = [
        {
            "name": "Opaque",
            "subtitle": "Blend disabled — depth write ON",
            "formula": "result = src",
            "overlap_rgb": src_rgb,
            "overlap_alpha": 1.0,
            "show_discard": False,
            "color": STYLE["text"],
        },
        {
            "name": "Alpha Test (MASK)",
            "subtitle": "Blend disabled — depth write ON",
            "formula": "clip(α − cutoff)",
            "overlap_rgb": src_rgb,
            "overlap_alpha": 1.0,
            "show_discard": True,
            "color": STYLE["accent3"],
        },
        {
            "name": "Alpha Blend",
            "subtitle": "SRC_ALPHA / ONE_MINUS_SRC_ALPHA — depth write OFF",
            "formula": "result = src·α + dst·(1−α)",
            "overlap_rgb": src_rgb * src_alpha + dst_rgb * (1.0 - src_alpha),
            "overlap_alpha": 1.0,
            "show_discard": False,
            "color": STYLE["accent1"],
        },
        {
            "name": "Additive",
            "subtitle": "SRC_ALPHA / ONE — depth write OFF",
            "formula": "result = src·α + dst",
            "overlap_rgb": np.clip(src_rgb * src_alpha + dst_rgb * dst_alpha, 0, 1),
            "overlap_alpha": 1.0,
            "show_discard": False,
            "color": STYLE["accent2"],
        },
    ]

    for ax, mode in zip(axes.flat, modes, strict=True):
        setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-1.5, 6.5), grid=False)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

        # --- Draw dst rectangle (background, left side) ---
        dst_rect = Rectangle(
            (0.5, 1.0),
            5.0,
            4.0,
            linewidth=1.5,
            edgecolor=(*dst_rgb, 0.8),
            facecolor=(*dst_rgb, dst_alpha),
            zorder=2,
        )
        ax.add_patch(dst_rect)
        ax.text(
            1.5,
            5.4,
            "dst",
            color=(*dst_rgb, 1.0),
            fontsize=11,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        if mode["show_discard"]:
            # Alpha test: show two cases — above cutoff (passes) and
            # below cutoff (discarded, dst shows through)

            # Top half of src: alpha > cutoff → rendered fully opaque
            pass_rect = Rectangle(
                (4.0, 3.0),
                5.5,
                2.0,
                linewidth=1.5,
                edgecolor=(*src_rgb, 0.8),
                facecolor=(*src_rgb, 1.0),
                zorder=4,
            )
            ax.add_patch(pass_rect)

            # Bottom half of src: alpha < cutoff → discarded
            # Draw with dashed border and no fill to show it's gone
            discard_rect = Rectangle(
                (4.0, 1.0),
                5.5,
                2.0,
                linewidth=1.5,
                edgecolor=(*src_rgb, 0.4),
                facecolor="none",
                linestyle="--",
                zorder=4,
            )
            ax.add_patch(discard_rect)

            # Overlap region — top half: src wins (opaque)
            overlap_pass = Rectangle(
                (4.0, 3.0),
                1.5,
                2.0,
                linewidth=0,
                facecolor=(*src_rgb, 1.0),
                zorder=5,
            )
            ax.add_patch(overlap_pass)

            # Overlap region — bottom half: dst shows through (discarded)
            overlap_discard = Rectangle(
                (4.0, 1.0),
                1.5,
                2.0,
                linewidth=0,
                facecolor=(*dst_rgb, dst_alpha),
                zorder=5,
            )
            ax.add_patch(overlap_discard)

            # Labels
            ax.text(
                6.8,
                3.8,
                "α ≥ 0.5",
                color=STYLE["accent3"],
                fontsize=9,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                6.8,
                3.3,
                "PASS",
                color=STYLE["accent3"],
                fontsize=8,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                6.8,
                1.8,
                "α < 0.5",
                color=STYLE["accent2"],
                fontsize=9,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                6.8,
                1.3,
                "DISCARD",
                color=STYLE["accent2"],
                fontsize=8,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # src label
            ax.text(
                8.5,
                5.4,
                "src",
                color=(*src_rgb, 1.0),
                fontsize=11,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
        else:
            # Non-discard modes: draw src rectangle overlapping dst
            src_rect = Rectangle(
                (4.0, 1.0),
                5.5,
                4.0,
                linewidth=1.5,
                edgecolor=(*src_rgb, 0.8),
                facecolor=(*src_rgb, src_alpha if mode["name"] != "Opaque" else 1.0),
                zorder=3,
            )
            ax.add_patch(src_rect)

            # src label
            ax.text(
                8.0,
                5.4,
                f"src (α={src_alpha})" if mode["name"] != "Opaque" else "src",
                color=(*src_rgb, 1.0),
                fontsize=11,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Overlap region with computed blend result
            overlap_rect = Rectangle(
                (4.0, 1.0),
                1.5,
                4.0,
                linewidth=0,
                facecolor=(*mode["overlap_rgb"], mode["overlap_alpha"]),
                zorder=5,
            )
            ax.add_patch(overlap_rect)

        # --- Formula label ---
        ax.text(
            5.25,
            -0.5,
            mode["formula"],
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            family="monospace",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # --- Overlap bracket label ---
        if not mode["show_discard"]:
            ax.annotate(
                "",
                xy=(4.0, 0.7),
                xytext=(5.5, 0.7),
                arrowprops={
                    "arrowstyle": "<->",
                    "color": STYLE["warn"],
                    "lw": 1.5,
                },
                zorder=10,
            )
            ax.text(
                4.75,
                0.2,
                "overlap",
                color=STYLE["warn"],
                fontsize=8,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

        # --- Title ---
        ax.set_title(
            mode["name"],
            color=mode["color"],
            fontsize=13,
            fontweight="bold",
            pad=8,
        )

        # --- Subtitle ---
        ax.text(
            5.25,
            6.2,
            mode["subtitle"],
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            style="italic",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            zorder=10,
        )

    fig.suptitle(
        "Four Blend Modes Compared",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.01,
    )

    fig.tight_layout()
    save(fig, "gpu/16-blending", "blend_modes.png")


# ---------------------------------------------------------------------------
# gpu/17-normal-maps — tangent_space.png
# ---------------------------------------------------------------------------


def diagram_aabb_sorting():
    """AABB nearest-point sorting vs center-distance sorting for transparent
    objects.

    Side-by-side top-down view showing why center-distance sorting fails when
    two objects share the same center (a flat alpha-symbol plane inside a glass
    box), and how AABB nearest-point distance produces the correct draw order.
    """
    fig, (ax_bad, ax_good) = plt.subplots(1, 2, figsize=(12, 6), facecolor=STYLE["bg"])

    for ax in (ax_bad, ax_good):
        setup_axes(ax, xlim=(-4.5, 6.5), ylim=(-3.5, 3.5), grid=False)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

    # Shared geometry (top-down 2D: X = right, Y = depth into screen)
    # Both objects are centered at the same point
    center = np.array([0.0, 0.0])

    # Glass box — large AABB
    box_half = np.array([2.0, 2.0])
    box_min = center - box_half
    box_max = center + box_half

    # Alpha-symbol plane — thin AABB (a flat vertical surface)
    plane_half = np.array([1.2, 0.15])
    plane_min = center - plane_half
    plane_max = center + plane_half

    # Camera position — to the right
    cam = np.array([5.0, 0.0])

    # Nearest point on box AABB to camera
    box_nearest = np.array(
        [
            np.clip(cam[0], box_min[0], box_max[0]),
            np.clip(cam[1], box_min[1], box_max[1]),
        ]
    )

    # Nearest point on plane AABB to camera
    plane_nearest = np.array(
        [
            np.clip(cam[0], plane_min[0], plane_max[0]),
            np.clip(cam[1], plane_min[1], plane_max[1]),
        ]
    )

    def draw_scene(ax, mode):
        """Draw the scene in either 'center' or 'aabb' mode."""
        # Glass box (semi-transparent blue rectangle)
        box_rect = Rectangle(
            (box_min[0], box_min[1]),
            box_half[0] * 2,
            box_half[1] * 2,
            linewidth=2,
            edgecolor=STYLE["accent1"],
            facecolor=STYLE["accent1"],
            alpha=0.15,
            zorder=2,
        )
        ax.add_patch(box_rect)
        ax.text(
            box_min[0] + 0.15,
            box_max[1] - 0.3,
            "glass box",
            color=STYLE["accent1"],
            fontsize=9,
            style="italic",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # Alpha-symbol plane (orange filled thin rectangle)
        plane_rect = Rectangle(
            (plane_min[0], plane_min[1]),
            plane_half[0] * 2,
            plane_half[1] * 2,
            linewidth=2,
            edgecolor=STYLE["accent2"],
            facecolor=STYLE["accent2"],
            alpha=0.5,
            zorder=3,
        )
        ax.add_patch(plane_rect)
        ax.text(
            plane_min[0] + 0.15,
            plane_min[1] - 0.4,
            "\u03b1 plane",
            color=STYLE["accent2"],
            fontsize=9,
            style="italic",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # Shared center point
        ax.plot(
            center[0],
            center[1],
            "o",
            color=STYLE["text_dim"],
            markersize=5,
            zorder=8,
        )
        ax.text(
            center[0] - 0.1,
            center[1] + 0.35,
            "center",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # Camera
        ax.plot(cam[0], cam[1], "s", color=STYLE["warn"], markersize=10, zorder=10)
        ax.text(
            cam[0],
            cam[1] - 0.55,
            "camera",
            color=STYLE["warn"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        if mode == "center":
            # Both distance lines go to the same center — ambiguous
            ax.plot(
                [cam[0], center[0]],
                [cam[1], center[1]],
                "--",
                color=STYLE["accent1"],
                lw=1.8,
                alpha=0.7,
                zorder=5,
            )
            ax.plot(
                [cam[0], center[0]],
                [cam[1], center[1] + 0.08],
                "--",
                color=STYLE["accent2"],
                lw=1.8,
                alpha=0.7,
                zorder=5,
            )

            dist = np.linalg.norm(cam - center)
            ax.text(
                cam[0] / 2 + center[0] / 2,
                0.55,
                f"d = {dist:.1f}",
                color=STYLE["accent1"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                cam[0] / 2 + center[0] / 2,
                -0.55,
                f"d = {dist:.1f}",
                color=STYLE["accent2"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Verdict
            ax.text(
                1.0,
                -2.8,
                "Same distance \u2014 arbitrary order!",
                color=STYLE["accent2"],
                fontsize=11,
                fontweight="bold",
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

        else:
            # AABB nearest-point distances
            # Line to box nearest point
            ax.plot(
                box_nearest[0],
                box_nearest[1],
                "o",
                color=STYLE["accent1"],
                markersize=7,
                zorder=8,
            )
            ax.plot(
                [cam[0], box_nearest[0]],
                [cam[1], box_nearest[1]],
                "-",
                color=STYLE["accent1"],
                lw=2.2,
                alpha=0.8,
                zorder=5,
            )
            dist_box = np.linalg.norm(cam - box_nearest)
            ax.text(
                (cam[0] + box_nearest[0]) / 2,
                0.5,
                f"d = {dist_box:.1f}",
                color=STYLE["accent1"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Line to plane nearest point
            ax.plot(
                plane_nearest[0],
                plane_nearest[1],
                "o",
                color=STYLE["accent2"],
                markersize=7,
                zorder=8,
            )
            ax.plot(
                [cam[0], plane_nearest[0]],
                [cam[1], plane_nearest[1]],
                "-",
                color=STYLE["accent2"],
                lw=2.2,
                alpha=0.8,
                zorder=5,
            )
            dist_plane = np.linalg.norm(cam - plane_nearest)
            ax.text(
                (cam[0] + plane_nearest[0]) / 2,
                -0.5,
                f"d = {dist_plane:.1f}",
                color=STYLE["accent2"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Draw order annotation
            ax.text(
                1.0,
                -2.4,
                f"\u03b1 plane: d = {dist_plane:.1f} \u2192 draw first",
                color=STYLE["accent2"],
                fontsize=10,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                1.0,
                -3.0,
                f"glass box: d = {dist_box:.1f} \u2192 draw second",
                color=STYLE["accent1"],
                fontsize=10,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

    draw_scene(ax_bad, "center")
    draw_scene(ax_good, "aabb")

    ax_bad.set_title(
        "Center-distance sorting (broken)",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax_good.set_title(
        "AABB nearest-point sorting (correct)",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.suptitle(
        "Sorting Transparent Objects \u2014 Top-Down View",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/16-blending", "aabb_sorting.png")


def diagram_arvo_method():
    """Arvo's method for transforming an AABB through a matrix.

    Two-panel diagram showing:
    - Left: The naive approach — transform all corners, find new min/max
    - Right: Arvo's decomposition — per-axis min/max from matrix columns

    Both produce the same world-space AABB, but Arvo's method avoids
    transforming every corner individually.
    """
    # --- Setup: a 2D AABB rotated 35 degrees with translation -----------
    theta = np.radians(35)
    cos_t, sin_t = np.cos(theta), np.sin(theta)
    # Rotation matrix columns
    col0 = np.array([cos_t, sin_t])
    col1 = np.array([-sin_t, cos_t])
    tx = np.array([3.0, 1.5])  # translation

    # Local AABB
    lmin = np.array([-1.5, -1.0])
    lmax = np.array([1.5, 1.0])

    # Compute the 4 corners of the local AABB
    corners_local = np.array(
        [
            [lmin[0], lmin[1]],
            [lmax[0], lmin[1]],
            [lmax[0], lmax[1]],
            [lmin[0], lmax[1]],
        ]
    )

    # Transform corners: world = M * local + t
    corners_world = np.array([col0 * c[0] + col1 * c[1] + tx for c in corners_local])

    # World AABB from corners (the correct answer both methods produce)
    wmin = corners_world.min(axis=0)
    wmax = corners_world.max(axis=0)

    # --- Figure setup ---------------------------------------------------
    fig, (ax_naive, ax_arvo) = plt.subplots(
        1, 2, figsize=(13, 6.5), facecolor=STYLE["bg"]
    )

    pad = 1.2
    xlim = (wmin[0] - 2.5, wmax[0] + pad)
    ylim = (wmin[1] - 2.0, wmax[1] + pad + 0.5)

    for ax in (ax_naive, ax_arvo):
        setup_axes(ax, xlim=xlim, ylim=ylim, grid=True)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

    # --- Helper: draw the rotated rectangle and world AABB ---------------
    def draw_common(ax, show_corners=False):
        """Draw the rotated box and its world-space AABB."""
        # Rotated rectangle (the actual transformed shape)
        rotated_poly = Polygon(
            corners_world,
            closed=True,
            linewidth=2.0,
            edgecolor=STYLE["accent1"],
            facecolor=STYLE["accent1"],
            alpha=0.18,
            zorder=3,
        )
        ax.add_patch(rotated_poly)

        # World-space AABB (dashed)
        world_rect = Rectangle(
            (wmin[0], wmin[1]),
            wmax[0] - wmin[0],
            wmax[1] - wmin[1],
            linewidth=2.0,
            edgecolor=STYLE["accent3"],
            facecolor="none",
            linestyle="--",
            zorder=4,
        )
        ax.add_patch(world_rect)

        # Label the world AABB
        ax.text(
            wmax[0] + 0.1,
            wmax[1],
            "world\nAABB",
            color=STYLE["accent3"],
            fontsize=9,
            fontweight="bold",
            va="top",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        if show_corners:
            for k, c in enumerate(corners_world):
                ax.plot(
                    c[0],
                    c[1],
                    "o",
                    color=STYLE["accent2"],
                    markersize=6,
                    zorder=8,
                )
                # Label alternating corners to avoid clutter
                if k in (0, 2):
                    ax.text(
                        c[0] - 0.15,
                        c[1] - 0.3 if k == 0 else c[1] + 0.2,
                        f"c{k}",
                        color=STYLE["accent2"],
                        fontsize=8,
                        ha="center",
                        path_effects=[
                            pe.withStroke(linewidth=3, foreground=STYLE["bg"])
                        ],
                        zorder=10,
                    )

        # Translation origin
        ax.plot(
            tx[0],
            tx[1],
            "+",
            color=STYLE["text_dim"],
            markersize=8,
            markeredgewidth=1.5,
            zorder=6,
        )

    # ---- Left panel: naive (transform all corners) ----------------------
    draw_common(ax_naive, show_corners=True)

    # Draw lines from each corner to the AABB edges to show min/max search
    for c in corners_world:
        # Vertical projection to the AABB top/bottom
        ax_naive.plot(
            [c[0], c[0]],
            [wmin[1] - 0.15, wmax[1] + 0.15],
            ":",
            color=STYLE["text_dim"],
            lw=0.7,
            alpha=0.4,
            zorder=1,
        )
        # Horizontal projection
        ax_naive.plot(
            [wmin[0] - 0.15, wmax[0] + 0.15],
            [c[1], c[1]],
            ":",
            color=STYLE["text_dim"],
            lw=0.7,
            alpha=0.4,
            zorder=1,
        )

    # Mark the extreme corners that define the AABB
    # Find which corners define xmin, xmax, ymin, ymax
    ixmin = corners_world[:, 0].argmin()
    ixmax = corners_world[:, 0].argmax()
    iymin = corners_world[:, 1].argmin()
    iymax = corners_world[:, 1].argmax()

    for idx in {ixmin, ixmax, iymin, iymax}:
        ax_naive.plot(
            corners_world[idx, 0],
            corners_world[idx, 1],
            "o",
            color=STYLE["accent3"],
            markersize=9,
            markerfacecolor="none",
            markeredgewidth=2,
            zorder=9,
        )

    # Annotation
    ax_naive.text(
        (xlim[0] + xlim[1]) / 2,
        ylim[0] + 0.3,
        "Transform each corner, then find min/max\n"
        "3D: 8 corners \u00d7 matrix multiply",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # ---- Right panel: Arvo's decomposition ------------------------------
    draw_common(ax_arvo, show_corners=False)

    # Show the decomposition: translation + column contributions
    # Start at translation
    ax_arvo.plot(
        tx[0],
        tx[1],
        "o",
        color=STYLE["warn"],
        markersize=7,
        zorder=8,
    )
    ax_arvo.text(
        tx[0] + 0.15,
        tx[1] + 0.3,
        "start: translation",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 0 contributions (scaled by lmin[0] and lmax[0])
    # These are the two possible contributions from the X extent
    contrib0_lo = col0 * lmin[0]
    contrib0_hi = col0 * lmax[0]

    # Column 1 contributions (scaled by lmin[1] and lmax[1])
    contrib1_lo = col1 * lmin[1]
    contrib1_hi = col1 * lmax[1]

    # Draw column 0 contributions from translation point
    arrow_kw = {
        "arrowstyle": "->,head_width=0.2,head_length=0.12",
        "lw": 2.2,
    }

    # Column 0 — max extent (positive X side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib0_hi[0], tx[1] + contrib0_hi[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent1"]},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib0_hi[0] / 2 + 0.05,
        tx[1] + contrib0_hi[1] / 2 + 0.25,
        "col\u2080 \u00d7 max\u2093",
        color=STYLE["accent1"],
        fontsize=8,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 0 — min extent (negative X side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib0_lo[0], tx[1] + contrib0_lo[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent1"], "linestyle": "--"},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib0_lo[0] / 2 - 0.05,
        tx[1] + contrib0_lo[1] / 2 - 0.3,
        "col\u2080 \u00d7 min\u2093",
        color=STYLE["accent1"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 1 — max extent (positive Y side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib1_hi[0], tx[1] + contrib1_hi[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent4"]},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib1_hi[0] / 2 - 0.45,
        tx[1] + contrib1_hi[1] / 2 + 0.15,
        "col\u2081 \u00d7 max\u1d67",
        color=STYLE["accent4"],
        fontsize=8,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 1 — min extent (negative Y side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib1_lo[0], tx[1] + contrib1_lo[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent4"], "linestyle": "--"},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib1_lo[0] / 2 + 0.35,
        tx[1] + contrib1_lo[1] / 2 - 0.2,
        "col\u2081 \u00d7 min\u1d67",
        color=STYLE["accent4"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Annotation explaining the per-axis logic
    ax_arvo.text(
        (xlim[0] + xlim[1]) / 2,
        ylim[0] + 0.3,
        "Per axis: new_min[i] = t[i] + \u03a3 min(M\u1d62\u2c7c\u00b7lo\u2c7c, M\u1d62\u2c7c\u00b7hi\u2c7c)\n"
        "3D: 18 multiplies + 18 comparisons (no corner transforms)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # --- Titles ----------------------------------------------------------
    ax_naive.set_title(
        "Naive: transform all corners",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax_arvo.set_title(
        "Arvo\u2019s method: per-axis decomposition",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.suptitle(
        "Transforming an AABB Through a Matrix",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/16-blending", "arvo_method.png")
