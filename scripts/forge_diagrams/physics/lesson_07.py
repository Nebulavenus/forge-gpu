"""Diagrams for physics/07 — Collision Shapes and Support Functions."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
_LESSON = "physics/07-collision-shapes"


# ---------------------------------------------------------------------------
# physics/07-collision-shapes — support_function.png
# ---------------------------------------------------------------------------


def diagram_support_function():
    """Support points traced by sweeping direction around sphere, box, capsule."""
    fig, axes = plt.subplots(1, 3, figsize=(15, 5), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    shapes = ["Sphere", "Box", "Capsule"]
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    for idx, (ax, name, color) in enumerate(zip(axes, shapes, colors, strict=True)):
        setup_axes(ax, xlim=(-2.2, 2.2), ylim=(-2.2, 2.2), aspect="equal")
        ax.axis("off")
        ax.set_title(name, color=STYLE["text"], fontsize=14, fontweight="bold", pad=10)

        # Direction angles
        theta = np.linspace(0, 2 * np.pi, 200)

        if idx == 0:
            # Sphere: shape outline = circle, support = circle
            r = 1.0
            sx = r * np.cos(theta)
            sy = r * np.sin(theta)
            shape_x = r * np.cos(theta)
            shape_y = r * np.sin(theta)
            ax.fill(shape_x, shape_y, color=color, alpha=0.15)
            ax.plot(shape_x, shape_y, color=color, lw=2.0)

        elif idx == 1:
            # Box: outline = square, support = corners
            hx, hy = 0.8, 0.8
            rect_x = [-hx, hx, hx, -hx, -hx]
            rect_y = [-hy, -hy, hy, hy, -hy]
            ax.fill(rect_x, rect_y, color=color, alpha=0.15)
            ax.plot(rect_x, rect_y, color=color, lw=2.0)
            # Support trace
            sx = []
            sy = []
            for t in theta:
                dx, dy = np.cos(t), np.sin(t)
                cx = hx if dx >= 0 else -hx
                cy = hy if dy >= 0 else -hy
                sx.append(cx)
                sy.append(cy)
            sx = np.array(sx)
            sy = np.array(sy)

        else:
            # Capsule: stadium shape
            r, h = 0.5, 0.6
            # Draw capsule as a closed stadium path (single continuous loop):
            # right wall bottom→top, top semicircle right→left,
            # left wall top→bottom, bottom semicircle left→right
            n_arc = 50
            top_arc = np.linspace(0, np.pi, n_arc)  # 0 → π
            bot_arc = np.linspace(np.pi, 2 * np.pi, n_arc)  # π → 2π
            outline_x = np.concatenate(
                [
                    r * np.cos(top_arc),  # top semicircle
                    r * np.cos(bot_arc),  # bottom semicircle
                ]
            )
            outline_y = np.concatenate(
                [
                    h + r * np.sin(top_arc),  # centered at +h
                    -h + r * np.sin(bot_arc),  # centered at -h
                ]
            )
            # Close explicitly
            outline_x = np.append(outline_x, outline_x[0])
            outline_y = np.append(outline_y, outline_y[0])
            ax.fill(outline_x, outline_y, color=color, alpha=0.15)
            ax.plot(outline_x, outline_y, color=color, lw=2.0)
            # Support trace
            sx = []
            sy = []
            for t in theta:
                dx, dy = np.cos(t), np.sin(t)
                cap_cy = h if dy >= 0 else -h
                length = np.sqrt(dx * dx + dy * dy)
                if length > 1e-6:
                    sx.append(r * dx / length)
                    sy.append(cap_cy + r * dy / length)
                else:
                    sx.append(0.0)
                    sy.append(0.0)
            sx = np.array(sx)
            sy = np.array(sy)

        # Draw support trace
        ax.plot(
            sx,
            sy,
            color=STYLE["warn"],
            lw=2.5,
            linestyle="--",
            alpha=0.9,
            label="Support trace",
        )

        # Draw a few sample direction arrows and support points
        sample_angles = [0, np.pi / 4, np.pi / 2, 3 * np.pi / 4, np.pi, 5 * np.pi / 4]
        for sa in sample_angles:
            di = int(sa / (2 * np.pi) * len(theta)) % len(theta)
            # Direction arrow from center
            dx, dy = 0.6 * np.cos(sa), 0.6 * np.sin(sa)
            ax.annotate(
                "",
                xy=(dx, dy),
                xytext=(0, 0),
                arrowprops={
                    "arrowstyle": "->",
                    "color": STYLE["text_dim"],
                    "lw": 1.0,
                    "alpha": 0.4,
                },
            )
            # Support point
            ax.plot(sx[di], sy[di], "o", color=STYLE["warn"], markersize=5, zorder=5)

        # Center dot
        ax.plot(0, 0, "o", color=STYLE["text"], markersize=4, zorder=5)

    fig.suptitle(
        "Support Function — Farthest Point in Each Direction",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.98,
    )

    # Legend
    axes[2].plot(
        [], [], color=STYLE["warn"], lw=2.5, linestyle="--", label="Support trace"
    )
    axes[2].plot([], [], "o", color=STYLE["warn"], markersize=5, label="Support point")
    axes[2].legend(
        loc="lower right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    plt.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, _LESSON, "support_function.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/07-collision-shapes — aabb_from_obb.png
# ---------------------------------------------------------------------------


def diagram_aabb_from_obb():
    """AABB computed from a rotated box (OBB), showing the enclosure."""
    fig, axes = plt.subplots(1, 3, figsize=(15, 5), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    rotations = [0, 30, 45]
    titles = ["0° rotation", "30° rotation", "45° rotation"]

    hx, hy = 1.2, 0.6

    for ax, angle_deg, title in zip(axes, rotations, titles, strict=True):
        setup_axes(ax, xlim=(-2.5, 2.5), ylim=(-2.5, 2.5), aspect="equal")
        ax.set_title(title, color=STYLE["text"], fontsize=12, fontweight="bold", pad=8)

        angle_rad = np.radians(angle_deg)
        c, s = np.cos(angle_rad), np.sin(angle_rad)

        # Rotated box corners
        corners_local = np.array(
            [[-hx, -hy], [hx, -hy], [hx, hy], [-hx, hy], [-hx, -hy]]
        )
        R = np.array([[c, -s], [s, c]])
        corners_world = (R @ corners_local.T).T

        # Draw rotated box
        ax.fill(
            corners_world[:-1, 0],
            corners_world[:-1, 1],
            color=STYLE["accent2"],
            alpha=0.2,
        )
        ax.plot(
            corners_world[:, 0], corners_world[:, 1], color=STYLE["accent2"], lw=2.5
        )

        # Compute AABB
        aabb_min_x = corners_world[:-1, 0].min()
        aabb_max_x = corners_world[:-1, 0].max()
        aabb_min_y = corners_world[:-1, 1].min()
        aabb_max_y = corners_world[:-1, 1].max()

        # Draw AABB
        aabb_rect = mpatches.Rectangle(
            (aabb_min_x, aabb_min_y),
            aabb_max_x - aabb_min_x,
            aabb_max_y - aabb_min_y,
            fill=False,
            edgecolor=STYLE["accent3"],
            lw=2.0,
            linestyle="--",
            zorder=4,
        )
        ax.add_patch(aabb_rect)

        # Draw half-extent arrows (world-space AABB half-extents)
        cx = (aabb_min_x + aabb_max_x) / 2
        cy = (aabb_min_y + aabb_max_y) / 2
        ax.plot(cx, cy, "+", color=STYLE["text"], markersize=8, mew=2)

        # Rotated local axes
        ax_x = R @ np.array([hx, 0])
        ax_y = R @ np.array([0, hy])
        ax.annotate(
            "",
            xy=ax_x,
            xytext=(0, 0),
            arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 1.5},
        )
        ax.annotate(
            "",
            xy=ax_y,
            xytext=(0, 0),
            arrowprops={"arrowstyle": "->", "color": STYLE["accent4"], "lw": 1.5},
        )

    fig.suptitle(
        "AABB from Oriented Box — Tightest Axis-Aligned Enclosure",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.98,
    )

    # Legend
    axes[0].plot([], [], color=STYLE["accent2"], lw=2.5, label="OBB")
    axes[0].plot([], [], color=STYLE["accent3"], lw=2.0, linestyle="--", label="AABB")
    axes[0].legend(
        loc="lower left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    plt.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, _LESSON, "aabb_from_obb.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Public list of all diagram functions
# ---------------------------------------------------------------------------

ALL_DIAGRAMS = [
    diagram_support_function,
    diagram_aabb_from_obb,
]
