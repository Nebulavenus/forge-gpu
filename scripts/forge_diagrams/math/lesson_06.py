"""Diagrams for math/06."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon

from .._common import STYLE, save

# ---------------------------------------------------------------------------
# math/06-projections — frustum.png
# ---------------------------------------------------------------------------


def diagram_frustum():
    """Viewing frustum side view with near/far planes and FOV."""
    fig = plt.figure(figsize=(7, 4), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    near = 1.5
    far = 6
    fov_half = np.radians(30)

    near_h = near * np.tan(fov_half)
    far_h = far * np.tan(fov_half)

    # Frustum trapezoid fill
    frustum = Polygon(
        [(near, -near_h), (far, -far_h), (far, far_h), (near, near_h)],
        closed=True,
        alpha=0.15,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(frustum)

    # Eye point and rays
    ax.plot(0, 0, "o", color=STYLE["text"], markersize=6, zorder=5)
    ax.text(
        -0.3,
        0.0,
        "Eye",
        fontsize=10,
        fontweight="bold",
        color=STYLE["text"],
        ha="right",
        va="center",
    )
    ax.plot([0, far], [0, far_h], "-", color=STYLE["axis"], alpha=0.4, lw=1)
    ax.plot([0, far], [0, -far_h], "-", color=STYLE["axis"], alpha=0.4, lw=1)

    # Near plane
    ax.plot([near, near], [-near_h, near_h], "-", color=STYLE["accent3"], lw=2.5)
    ax.text(
        near,
        near_h + 0.25,
        "Near plane",
        fontsize=9,
        ha="center",
        color=STYLE["accent3"],
        fontweight="bold",
    )

    # Far plane
    ax.plot([far, far], [-far_h, far_h], "-", color=STYLE["accent2"], lw=2.5)
    ax.text(
        far,
        far_h + 0.25,
        "Far plane",
        fontsize=9,
        ha="center",
        color=STYLE["accent2"],
        fontweight="bold",
    )

    # FOV angle arc
    theta = np.linspace(-fov_half, fov_half, 30)
    ax.plot(
        np.cos(theta),
        np.sin(theta),
        "-",
        color=STYLE["warn"],
        lw=1.5,
    )
    ax.text(
        1.1,
        0.0,
        "FOV",
        fontsize=9,
        color=STYLE["warn"],
        fontweight="bold",
        ha="left",
        va="center",
    )

    # Depth axis arrow
    ax.annotate(
        "",
        xy=(far + 0.3, 0),
        xytext=(-0.5, 0),
        arrowprops={"arrowstyle": "->", "color": STYLE["axis"], "lw": 0.8},
    )
    ax.text(
        far + 0.5,
        0,
        "-Z (into screen)",
        fontsize=8,
        color=STYLE["axis"],
        va="center",
    )

    ax.set_xlim(-1, far + 2.5)
    ax.set_ylim(-far_h - 1, far_h + 1)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Viewing Frustum (side view)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/06-projections", "frustum.png")


def diagram_similar_triangles():
    """Similar triangles showing perspective projection derivation."""
    fig = plt.figure(figsize=(8, 4.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    # Geometry parameters
    near = 2.5  # near plane distance
    depth = 6.0  # point depth (-z)
    x_point = 2.4  # x coordinate of P
    x_screen = x_point * near / depth  # projected x on near plane

    # --- Optical axis (dashed center line) ---
    ax.plot(
        [0, depth + 0.8],
        [0, 0],
        "--",
        color=STYLE["axis"],
        alpha=0.3,
        lw=1,
    )

    # --- Big triangle (eye -> P) ---
    # Hypotenuse: eye to P
    ax.plot(
        [0, depth],
        [0, x_point],
        "-",
        color=STYLE["accent2"],
        lw=2,
        alpha=0.8,
    )
    # Vertical side: depth axis to P (at depth)
    ax.plot(
        [depth, depth],
        [0, x_point],
        "-",
        color=STYLE["accent2"],
        lw=2,
        alpha=0.8,
    )
    # Fill big triangle
    big_tri = Polygon(
        [(0, 0), (depth, x_point), (depth, 0)],
        closed=True,
        alpha=0.08,
        facecolor=STYLE["accent2"],
        edgecolor="none",
    )
    ax.add_patch(big_tri)

    # --- Small triangle (eye -> P') ---
    # Hypotenuse: eye to P' (shares line with big triangle)
    ax.plot(
        [0, near],
        [0, x_screen],
        "-",
        color=STYLE["accent1"],
        lw=2.5,
    )
    # Vertical side: axis to P' (at near plane)
    ax.plot(
        [near, near],
        [0, x_screen],
        "-",
        color=STYLE["accent1"],
        lw=2.5,
    )
    # Fill small triangle
    small_tri = Polygon(
        [(0, 0), (near, x_screen), (near, 0)],
        closed=True,
        alpha=0.15,
        facecolor=STYLE["accent1"],
        edgecolor="none",
    )
    ax.add_patch(small_tri)

    # --- Angle arc at eye ---
    arc_r = 1.2
    theta = np.linspace(0, np.arctan2(x_point, depth), 20)
    ax.plot(
        arc_r * np.cos(theta),
        arc_r * np.sin(theta),
        "-",
        color=STYLE["warn"],
        lw=1.5,
    )
    ax.text(
        arc_r * 0.65,
        0.12,
        "\u03b8",
        fontsize=12,
        color=STYLE["warn"],
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Right-angle markers ---
    sq = 0.2
    # At P (depth, 0) corner
    right_angle_big = Polygon(
        [
            (depth - sq, 0),
            (depth - sq, sq),
            (depth, sq),
        ],
        closed=False,
        fill=False,
        edgecolor=STYLE["accent2"],
        lw=1,
        alpha=0.6,
    )
    ax.add_patch(right_angle_big)
    # At P' (near, 0) corner
    right_angle_small = Polygon(
        [
            (near - sq, 0),
            (near - sq, sq),
            (near, sq),
        ],
        closed=False,
        fill=False,
        edgecolor=STYLE["accent1"],
        lw=1,
        alpha=0.6,
    )
    ax.add_patch(right_angle_small)

    # --- Near plane (full vertical line) ---
    ax.plot(
        [near, near],
        [-0.5, x_screen + 0.8],
        "-",
        color=STYLE["accent3"],
        lw=1.5,
        alpha=0.4,
    )

    # --- Points ---
    # Eye
    ax.plot(0, 0, "o", color=STYLE["text"], markersize=7, zorder=5)
    ax.text(
        -0.15,
        -0.3,
        "Eye",
        fontsize=10,
        fontweight="bold",
        color=STYLE["text"],
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # P' on near plane
    ax.plot(near, x_screen, "o", color=STYLE["accent1"], markersize=6, zorder=5)
    ax.text(
        near + 0.15,
        x_screen + 0.2,
        "P\u2032",
        fontsize=11,
        fontweight="bold",
        color=STYLE["accent1"],
        ha="left",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # P at depth
    ax.plot(depth, x_point, "o", color=STYLE["accent2"], markersize=6, zorder=5)
    ax.text(
        depth + 0.15,
        x_point + 0.15,
        "P",
        fontsize=11,
        fontweight="bold",
        color=STYLE["accent2"],
        ha="left",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Dimension labels ---
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # "n" -- near distance along axis
    ax.annotate(
        "",
        xy=(near, -0.6),
        xytext=(0, -0.6),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent3"], "lw": 1.5},
    )
    ax.text(
        near / 2,
        -0.85,
        "n",
        fontsize=11,
        fontweight="bold",
        color=STYLE["accent3"],
        ha="center",
        va="top",
        fontstyle="italic",
        path_effects=stroke,
    )

    # "-z" -- total depth along axis
    ax.annotate(
        "",
        xy=(depth, -0.6),
        xytext=(0, -0.6),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent2"], "lw": 1.5},
    )
    ax.text(
        depth / 2,
        -1.1,
        "\u2212z",
        fontsize=11,
        fontweight="bold",
        color=STYLE["accent2"],
        ha="center",
        va="top",
        fontstyle="italic",
        path_effects=stroke,
    )

    # "x_screen" -- projected height at near plane
    ax.annotate(
        "",
        xy=(near - 0.3, x_screen),
        xytext=(near - 0.3, 0),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent1"], "lw": 1.5},
    )
    ax.text(
        near - 0.55,
        x_screen / 2,
        "$x_{screen}$",
        fontsize=10,
        fontweight="bold",
        color=STYLE["accent1"],
        ha="right",
        va="center",
        path_effects=stroke,
    )

    # "x" -- actual height at depth
    ax.annotate(
        "",
        xy=(depth + 0.3, x_point),
        xytext=(depth + 0.3, 0),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent2"], "lw": 1.5},
    )
    ax.text(
        depth + 0.55,
        x_point / 2,
        "x",
        fontsize=11,
        fontweight="bold",
        color=STYLE["accent2"],
        ha="left",
        va="center",
        fontstyle="italic",
        path_effects=stroke,
    )

    # --- Triangle labels ---
    ax.text(
        near * 0.65,
        x_screen * 0.25,
        "small\ntriangle",
        fontsize=8,
        color=STYLE["accent1"],
        ha="center",
        va="center",
        alpha=0.8,
        path_effects=stroke,
    )
    ax.text(
        (near + depth) / 2,
        x_point * 0.2,
        "big triangle",
        fontsize=8,
        color=STYLE["accent2"],
        ha="center",
        va="center",
        alpha=0.8,
        path_effects=stroke,
    )

    # --- Annotation labels for planes ---
    ax.text(
        near,
        x_screen + 1.0,
        "Near plane",
        fontsize=9,
        fontweight="bold",
        color=STYLE["accent3"],
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # --- Clean up axes ---
    ax.set_xlim(-0.8, depth + 1.3)
    ax.set_ylim(-1.5, x_point + 0.8)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Similar Triangles in Perspective Projection",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/06-projections", "similar_triangles.png")


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — mip_chain.png
# ---------------------------------------------------------------------------
