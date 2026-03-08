"""Diagrams for math/09."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon

from .._common import STYLE, draw_vector, save, setup_axes

# ---------------------------------------------------------------------------
# math/09-view-matrix — view_transform.png
# ---------------------------------------------------------------------------


def diagram_view_transform():
    """Two-panel top-down: world space vs view space with scene objects.

    Inspired by 3D Math Primer illustrations -- shows a 2D bird's-eye view
    with a camera, colored axes, a view frustum, and scene objects.  The
    camera looks along -Z (downward on screen).  The view matrix shifts
    everything so the camera ends up at the origin.
    """
    fig = plt.figure(figsize=(11, 5.5), facecolor=STYLE["bg"])

    # --- Scene definition (all positions in world space, XZ plane) ---
    cam_pos = np.array([0.0, 4.0])  # camera at Z=4, looking down -Z

    scene_objs = [
        {
            "pos": np.array([2.0, 1.5]),
            "marker": "s",
            "color": STYLE["accent2"],
            "label": "cube",
        },
        {
            "pos": np.array([-1.5, 2.5]),
            "marker": "^",
            "color": STYLE["accent3"],
            "label": "tree",
        },
        {
            "pos": np.array([0.5, 0.0]),
            "marker": "D",
            "color": STYLE["accent4"],
            "label": "rock",
        },
    ]

    frust_half = np.radians(30)
    frust_len = 4.2

    panels = [
        ("World Space", np.array([0.0, 0.0])),
        ("View Space", -cam_pos),
    ]

    for idx, (title, shift) in enumerate(panels):
        ax = fig.add_subplot(1, 2, idx + 1)
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-5, 5)
        ax.set_ylim(-5.5, 5.5)
        ax.set_aspect("equal")

        # Floor grid
        for g in range(-5, 6):
            ax.plot([-5, 5], [g, g], color=STYLE["grid"], lw=0.3, alpha=0.3)
            ax.plot([g, g], [-5.5, 5.5], color=STYLE["grid"], lw=0.3, alpha=0.3)

        # Coordinate axes at diagram (0, 0)
        al = 4.5
        ax.annotate(
            "",
            xy=(al, 0),
            xytext=(-al, 0),
            arrowprops={
                "arrowstyle": "->,head_width=0.15",
                "color": "#bb5555",
                "lw": 1.3,
                "alpha": 0.5,
            },
        )
        ax.text(
            al + 0.15,
            -0.4,
            "+X",
            color="#bb5555",
            fontsize=8,
            fontweight="bold",
            alpha=0.6,
        )
        ax.annotate(
            "",
            xy=(0, al),
            xytext=(0, -al),
            arrowprops={
                "arrowstyle": "->,head_width=0.15",
                "color": "#5577cc",
                "lw": 1.3,
                "alpha": 0.5,
            },
        )
        ax.text(
            0.25,
            al + 0.15,
            "+Z",
            color="#5577cc",
            fontsize=8,
            fontweight="bold",
            alpha=0.6,
        )

        c = cam_pos + shift

        # View frustum cone (faint wedge showing what the camera sees)
        cos_f = np.cos(frust_half)
        sin_f = np.sin(frust_half)
        ld = np.array([-sin_f, -cos_f])  # left edge direction
        rd = np.array([sin_f, -cos_f])  # right edge direction
        frust = Polygon(
            [c, c + ld * frust_len, c + rd * frust_len],
            closed=True,
            facecolor=STYLE["warn"],
            alpha=0.05,
            edgecolor=STYLE["warn"],
            lw=0.7,
            linestyle="--",
            zorder=2,
        )
        ax.add_patch(frust)

        # Camera icon (triangle pointing down = forward along -Z)
        s_cam = 0.38
        cam_tri = Polygon(
            [
                (c[0] - s_cam * 0.8, c[1] + s_cam * 0.55),
                (c[0], c[1] - s_cam * 0.9),
                (c[0] + s_cam * 0.8, c[1] + s_cam * 0.55),
            ],
            closed=True,
            facecolor=STYLE["accent1"],
            edgecolor="white",
            linewidth=1.2,
            zorder=10,
            alpha=0.9,
        )
        ax.add_patch(cam_tri)
        cam_lbl = "camera" if idx == 0 else "camera (origin)"
        ax.text(
            c[0] + 0.5,
            c[1] + 0.1,
            cam_lbl,
            color=STYLE["accent1"],
            fontsize=10,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Scene objects
        for obj in scene_objs:
            p = obj["pos"] + shift
            ax.plot(
                p[0],
                p[1],
                obj["marker"],
                color=obj["color"],
                markersize=11,
                zorder=8,
                markeredgecolor="white",
                markeredgewidth=0.8,
            )
            ax.text(
                p[0] + 0.35,
                p[1] + 0.3,
                obj["label"],
                color=obj["color"],
                fontsize=9,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

        # Origin crosshair
        ax.plot(0, 0, "+", color=STYLE["text"], markersize=10, mew=1.5, zorder=6)
        if idx == 0:
            ax.text(
                0.3,
                -0.5,
                "origin",
                color=STYLE["text_dim"],
                fontsize=8,
                style="italic",
            )
        else:
            # Ghost marker showing where the world origin ended up
            wo = np.array([0.0, 0.0]) + shift
            ax.plot(
                wo[0],
                wo[1],
                "+",
                color=STYLE["text_dim"],
                markersize=8,
                mew=1,
                zorder=5,
                alpha=0.5,
            )
            ax.text(
                wo[0] + 0.3,
                wo[1] - 0.4,
                "world origin",
                color=STYLE["text_dim"],
                fontsize=7,
                style="italic",
                alpha=0.7,
            )
            # Annotation: objects ahead are at -Z
            ax.text(
                3.5,
                -4.8,
                "objects ahead\nhave Z < 0",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                style="italic",
                alpha=0.6,
            )

        ax.set_title(title, color=STYLE["text"], fontsize=13, fontweight="bold", pad=10)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

    # Arrow between panels
    fig.text(
        0.50,
        0.50,
        "\u2192",
        color=STYLE["warn"],
        fontsize=30,
        ha="center",
        va="center",
        fontweight="bold",
    )
    fig.text(
        0.50,
        0.40,
        "View matrix V",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
    )

    fig.suptitle(
        "The Camera as an Inverse Transform",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "math/09-view-matrix", "view_transform.png")


# ---------------------------------------------------------------------------
# math/09-view-matrix — camera_basis_vectors.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/09-view-matrix — camera_basis_vectors.png
# ---------------------------------------------------------------------------


def diagram_camera_basis_vectors():
    """Two-panel: identity vs rotated camera basis vectors in pseudo-3D."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # Simple isometric-ish projection: X axis goes right, Y goes up, Z goes
    # into the page at a diagonal.  Project 3D -> 2D:
    #   px = x - z * 0.35
    #   py = y - z * 0.35
    def proj(x, y, z):
        return (x - z * 0.35, y - z * 0.35)

    # Identity basis and rotated basis (yaw=45, pitch=30)
    yaw = np.radians(45)
    pitch = np.radians(30)

    cy, sy = np.cos(yaw), np.sin(yaw)
    cp, sp = np.cos(pitch), np.sin(pitch)

    configs = [
        {
            "title": "Identity (no rotation)",
            "forward": (0, 0, -1),
            "right": (1, 0, 0),
            "up": (0, 1, 0),
            "labels": ("fwd (0,0,\u22121)", "right (1,0,0)", "up (0,1,0)"),
        },
        {
            "title": "After yaw=45\u00b0, pitch=30\u00b0",
            "forward": (-sy * cp, sp, -cy * cp),
            "right": (cy, 0, -sy),
            "up": (sy * sp, cp, cy * sp),
            "labels": ("quat_forward(q)", "quat_right(q)", "quat_up(q)"),
        },
    ]

    for i, cfg in enumerate(configs):
        ax = fig.add_subplot(1, 2, i + 1)
        setup_axes(ax, xlim=(-1.5, 1.8), ylim=(-1.5, 1.8), grid=False)

        origin = (0, 0)

        # Light ghost axes for reference
        for endpoint, lbl in [
            ((1.3, 0, 0), "+X"),
            ((0, 1.3, 0), "+Y"),
            ((0, 0, -1.3), "-Z"),
        ]:
            px, py = proj(*endpoint)
            ax.plot(
                [0, px],
                [0, py],
                "-",
                color=STYLE["grid"],
                lw=0.5,
                alpha=0.4,
            )
            ax.text(
                px * 1.1,
                py * 1.1,
                lbl,
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="center",
            )

        # Draw the three basis vectors
        vectors = [
            (cfg["forward"], STYLE["accent2"], cfg["labels"][0]),
            (cfg["right"], STYLE["accent1"], cfg["labels"][1]),
            (cfg["up"], STYLE["accent3"], cfg["labels"][2]),
        ]

        for (vx, vy, vz), color, label in vectors:
            px, py = proj(vx, vy, vz)
            draw_vector(ax, origin, (px, py), color, label, lw=2.5)

        ax.plot(0, 0, "o", color=STYLE["text"], markersize=5, zorder=5)
        ax.set_title(cfg["title"], color=STYLE["text"], fontsize=11, fontweight="bold")
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

    # Arrow between panels
    fig.text(
        0.50,
        0.5,
        "\u2192",
        color=STYLE["warn"],
        fontsize=28,
        ha="center",
        va="center",
        fontweight="bold",
    )
    fig.text(
        0.50,
        0.42,
        "quaternion rotation",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
    )

    fig.suptitle(
        "Camera Basis Vectors from Quaternion",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "math/09-view-matrix", "camera_basis_vectors.png")


# ---------------------------------------------------------------------------
# math/10-anisotropy — pixel_footprint.png
# ---------------------------------------------------------------------------
