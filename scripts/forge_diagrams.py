#!/usr/bin/env python
"""
forge_diagrams.py — Generate matplotlib diagrams for forge-gpu lesson READMEs.

Produces PNG diagrams at 200 DPI for embedding in markdown. Each lesson's
diagrams are placed in its assets/ directory (created automatically).

Usage:
    python scripts/forge_diagrams.py --lesson math/01    # one lesson
    python scripts/forge_diagrams.py --all               # all diagrams
    python scripts/forge_diagrams.py --list               # list available

Requires: pip install numpy matplotlib
"""

import argparse
import os
import sys

import matplotlib

matplotlib.use("Agg")  # noqa: E402 — must precede pyplot/patheffects imports

import matplotlib.patheffects as pe  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np
from matplotlib.colors import LinearSegmentedColormap  # noqa: E402
from matplotlib.patches import Polygon, Rectangle  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LESSONS_DIR = os.path.join(REPO_ROOT, "lessons")
DPI = 200


# ---------------------------------------------------------------------------
# Dark theme style (matching forge-gpu visual identity)
# ---------------------------------------------------------------------------

STYLE = {
    "bg": "#1a1a2e",  # Dark blue-gray background
    "grid": "#2a2a4a",  # Subtle grid lines
    "axis": "#8888aa",  # Axis lines and labels
    "text": "#e0e0f0",  # Primary text
    "text_dim": "#8888aa",  # Secondary/dim text
    "accent1": "#4fc3f7",  # Cyan — primary vectors, highlights
    "accent2": "#ff7043",  # Orange — secondary vectors, results
    "accent3": "#66bb6a",  # Green — tertiary, normals
    "accent4": "#ab47bc",  # Purple — special elements
    "warn": "#ffd54f",  # Yellow — annotations, warnings
    "surface": "#252545",  # Slightly lighter surface for fills
}

# Custom colormap for texture filtering diagrams
_FORGE_CMAP = LinearSegmentedColormap.from_list(
    "forge",
    [STYLE["bg"], STYLE["accent1"], STYLE["accent2"], STYLE["warn"]],
)


def _setup_axes(ax, xlim=None, ylim=None, grid=True, aspect="equal"):
    """Apply consistent forge-gpu dark styling to axes."""
    ax.set_facecolor(STYLE["bg"])
    if xlim:
        ax.set_xlim(xlim)
    if ylim:
        ax.set_ylim(ylim)
    if aspect:
        ax.set_aspect(aspect)
    ax.tick_params(colors=STYLE["axis"], labelsize=9)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    if grid:
        ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.5)
    ax.set_axisbelow(True)


def _draw_vector(ax, origin, vec, color, label=None, label_offset=(0.15, 0.15), lw=2.5):
    """Draw a labeled arrow vector with text stroke for readability."""
    ax.annotate(
        "",
        xy=(origin[0] + vec[0], origin[1] + vec[1]),
        xytext=origin,
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": color,
            "lw": lw,
        },
    )
    if label:
        mid = (
            origin[0] + vec[0] / 2 + label_offset[0],
            origin[1] + vec[1] / 2 + label_offset[1],
        )
        ax.text(
            mid[0],
            mid[1],
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )


def _save(fig, lesson_path, filename):
    """Save a figure to a lesson's assets/ directory."""
    assets_dir = os.path.join(LESSONS_DIR, lesson_path, "assets")
    os.makedirs(assets_dir, exist_ok=True)
    out = os.path.join(assets_dir, filename)
    fig.savefig(
        out,
        dpi=DPI,
        bbox_inches="tight",
        facecolor=STYLE["bg"],
        pad_inches=0.2,
    )
    plt.close(fig)
    rel = os.path.relpath(out, REPO_ROOT)
    print(f"  {rel}")


# ---------------------------------------------------------------------------
# math/01-vectors — vector_addition.png
# ---------------------------------------------------------------------------


def diagram_vector_addition():
    """Vector addition with tail-to-head and parallelogram."""
    fig = plt.figure(figsize=(7, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    _setup_axes(ax, xlim=(-0.5, 5), ylim=(-0.5, 4.5))

    a = (3, 1)
    b = (1, 2.5)
    result = (a[0] + b[0], a[1] + b[1])

    # Vectors
    _draw_vector(ax, (0, 0), a, STYLE["accent1"], "a = (3, 1)")
    _draw_vector(ax, a, b, STYLE["accent2"], "b = (1, 2.5)")
    _draw_vector(ax, (0, 0), result, STYLE["accent3"], "a + b = (4, 3.5)")

    # Ghosted b from origin + parallelogram dashes
    _draw_vector(ax, (0, 0), b, STYLE["accent2"], lw=1.0)
    ax.plot(
        [b[0], result[0]],
        [b[1], result[1]],
        "--",
        color=STYLE["text_dim"],
        lw=0.8,
        alpha=0.5,
    )
    ax.plot(
        [a[0], result[0]],
        [a[1], result[1]],
        "--",
        color=STYLE["text_dim"],
        lw=0.8,
        alpha=0.5,
    )

    # Origin dot
    ax.plot(0, 0, "o", color=STYLE["text"], markersize=6, zorder=5)
    ax.text(-0.3, -0.3, "O", color=STYLE["text_dim"], fontsize=10)

    ax.set_title(
        "Vector Addition: Tail-to-Head",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)

    fig.tight_layout()
    _save(fig, "math/01-vectors", "vector_addition.png")


# ---------------------------------------------------------------------------
# math/01-vectors — dot_product.png
# ---------------------------------------------------------------------------


def diagram_dot_product():
    """Three-panel dot product: same, perpendicular, opposite."""
    fig = plt.figure(figsize=(9, 5), facecolor=STYLE["bg"])

    cases = [
        ("Same direction", (1, 0), (0.8, 0.3), "dot > 0"),
        ("Perpendicular", (1, 0), (0, 1), "dot = 0"),
        ("Opposite", (1, 0), (-0.7, -0.3), "dot < 0"),
    ]

    for i, (title, a_dir, b_dir, result) in enumerate(cases):
        ax = fig.add_subplot(1, 3, i + 1)
        _setup_axes(ax, xlim=(-1.8, 1.8), ylim=(-1.8, 1.8), grid=False)

        # Reference circle
        theta = np.linspace(0, 2 * np.pi, 64)
        ax.plot(
            1.2 * np.cos(theta),
            1.2 * np.sin(theta),
            color=STYLE["grid"],
            lw=0.5,
            alpha=0.4,
        )
        ax.axhline(0, color=STYLE["grid"], lw=0.5, alpha=0.4)
        ax.axvline(0, color=STYLE["grid"], lw=0.5, alpha=0.4)

        # Vectors (scaled up for visibility)
        scale = 1.3
        a = (a_dir[0] * scale, a_dir[1] * scale)
        b = (b_dir[0] * scale, b_dir[1] * scale)
        _draw_vector(ax, (0, 0), a, STYLE["accent1"], "a")
        _draw_vector(ax, (0, 0), b, STYLE["accent2"], "b")

        # Angle arc
        a_angle = np.arctan2(a[1], a[0])
        b_angle = np.arctan2(b[1], b[0])
        arc_t = np.linspace(a_angle, b_angle, 30)
        arc_r = 0.5
        ax.plot(
            arc_r * np.cos(arc_t),
            arc_r * np.sin(arc_t),
            color=STYLE["warn"],
            lw=1.5,
            alpha=0.8,
        )
        ax.text(
            0.6 * np.cos((a_angle + b_angle) / 2),
            0.6 * np.sin((a_angle + b_angle) / 2),
            "\u03b8",
            color=STYLE["warn"],
            fontsize=12,
            ha="center",
            va="center",
        )

        ax.plot(0, 0, "o", color=STYLE["text"], markersize=4, zorder=5)
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold")
        ax.text(
            0,
            -1.6,
            result,
            color=STYLE["accent3"],
            fontsize=12,
            ha="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    fig.suptitle(
        "Dot Product: Measuring Alignment",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    _save(fig, "math/01-vectors", "dot_product.png")


# ---------------------------------------------------------------------------
# math/03-bilinear-interpolation — bilinear_interpolation.png
# ---------------------------------------------------------------------------


def diagram_bilinear_interpolation():
    """Grid cell with the 3 lerp steps highlighted."""
    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    _setup_axes(ax, xlim=(-0.3, 1.5), ylim=(-0.3, 1.5), grid=False)

    # Corner values with distinct colors
    corners = {
        (0, 0): ("c00 = 10", STYLE["accent1"]),
        (1, 0): ("c10 = 30", STYLE["accent2"]),
        (0, 1): ("c01 = 20", STYLE["accent4"]),
        (1, 1): ("c11 = 50", STYLE["accent3"]),
    }

    # Grid cell fill
    rect = Rectangle(
        (0, 0),
        1,
        1,
        fill=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        zorder=1,
    )
    ax.add_patch(rect)

    # Corner points and labels
    for (cx, cy), (label, color) in corners.items():
        ax.plot(cx, cy, "o", color=color, markersize=10, zorder=5)
        offset_x = -0.22 if cx == 0 else 0.08
        offset_y = -0.12 if cy == 0 else 0.08
        ax.text(
            cx + offset_x,
            cy + offset_y,
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    tx, ty = 0.35, 0.7

    # Sample point
    ax.plot(tx, ty, "*", color=STYLE["warn"], markersize=18, zorder=6)
    ax.text(
        tx + 0.06,
        ty + 0.06,
        f"({tx}, {ty})",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Step 1: bottom lerp
    bot_val = 10 + tx * (30 - 10)
    ax.plot(tx, 0, "s", color=STYLE["accent1"], markersize=8, zorder=5)
    ax.plot([0, 1], [0, 0], "-", color=STYLE["accent1"], lw=2, alpha=0.6)
    ax.text(
        tx,
        -0.18,
        f"bot = {bot_val:.0f}",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        fontweight="bold",
    )
    ax.text(
        1.15, 0.0, "1. lerp bottom", color=STYLE["accent1"], fontsize=9, va="center"
    )

    # Step 2: top lerp
    top_val = 20 + tx * (50 - 20)
    ax.plot(tx, 1, "s", color=STYLE["accent4"], markersize=8, zorder=5)
    ax.plot([0, 1], [1, 1], "-", color=STYLE["accent4"], lw=2, alpha=0.6)
    ax.text(
        tx,
        1.1,
        f"top = {top_val:.1f}",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
        fontweight="bold",
    )
    ax.text(1.15, 1.0, "2. lerp top", color=STYLE["accent4"], fontsize=9, va="center")

    # Step 3: vertical lerp
    result_val = bot_val + ty * (top_val - bot_val)
    ax.plot([tx, tx], [0, 1], "--", color=STYLE["warn"], lw=1.5, alpha=0.7)
    ax.text(
        tx - 0.27,
        0.5,
        "3. lerp\nvertical",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # Result annotation
    ax.text(
        0.5,
        -0.22,
        f"result = {result_val:.1f}",
        color=STYLE["warn"],
        fontsize=12,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_title(
        "Bilinear Interpolation: Three Lerps",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel("tx", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("ty", color=STYLE["axis"], fontsize=11)

    fig.tight_layout()
    _save(fig, "math/03-bilinear-interpolation", "bilinear_interpolation.png")


# ---------------------------------------------------------------------------
# math/05-matrices — matrix_basis_vectors.png
# ---------------------------------------------------------------------------


def diagram_matrix_basis_vectors():
    """Before/after basis vectors for a 45-degree rotation."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    angle = np.radians(45)
    c, s = np.cos(angle), np.sin(angle)

    configs = [
        (
            "Before: Standard Basis (Identity)",
            (1, 0),
            (0, 1),
            "x\u0302 = (1, 0)",
            "y\u0302 = (0, 1)",
            (-0.5, 2.5),
            (-0.5, 2.5),
        ),
        (
            "After: Rotated Basis (45\u00b0 Matrix)",
            (c, s),
            (-s, c),
            f"col0 = ({c:.2f}, {s:.2f})",
            f"col1 = ({-s:.2f}, {c:.2f})",
            (-1.5, 2),
            (-0.5, 2.5),
        ),
    ]

    for i, (title, x_vec, y_vec, x_label, y_label, xlim, ylim) in enumerate(configs):
        ax = fig.add_subplot(1, 2, i + 1)
        _setup_axes(ax, xlim=xlim, ylim=ylim)

        _draw_vector(ax, (0, 0), x_vec, STYLE["accent1"], x_label, lw=3)
        _draw_vector(ax, (0, 0), y_vec, STYLE["accent2"], y_label, lw=3)
        ax.plot(0, 0, "o", color=STYLE["text"], markersize=6, zorder=5)

        # Draw unit square / rotated square
        if i == 0:
            sq_x = [0, 1, 1, 0, 0]
            sq_y = [0, 0, 1, 1, 0]
        else:
            rot_sq = np.array([[0, 0], [c, s], [c - s, s + c], [-s, c], [0, 0]])
            sq_x = rot_sq[:, 0]
            sq_y = rot_sq[:, 1]

        ax.fill(sq_x, sq_y, color=STYLE["accent1"], alpha=0.08)
        ax.plot(sq_x, sq_y, "--", color=STYLE["text_dim"], lw=0.8, alpha=0.5)

        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold")

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
        "45\u00b0 rotation",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
    )

    fig.suptitle(
        "Matrix Columns = Where Basis Vectors Go",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    _save(fig, "math/05-matrices", "matrix_basis_vectors.png")


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
    _save(fig, "math/06-projections", "frustum.png")


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

    # --- Big triangle (eye → P) ---
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

    # --- Small triangle (eye → P') ---
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

    # "n" — near distance along axis
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

    # "-z" — total depth along axis
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

    # "x_screen" — projected height at near plane
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

    # "x" — actual height at depth
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
    _save(fig, "math/06-projections", "similar_triangles.png")


# ---------------------------------------------------------------------------
# gpu/04-textures-and-samplers — uv_mapping.png
# ---------------------------------------------------------------------------


def diagram_uv_mapping():
    """Position space to UV space mapping."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # Left: Position space (quad)
    ax1 = fig.add_subplot(121)
    _setup_axes(ax1, xlim=(-1, 1), ylim=(-1, 1), grid=False)

    quad_x = [-0.6, 0.6, 0.6, -0.6, -0.6]
    quad_y = [-0.6, -0.6, 0.6, 0.6, -0.6]
    ax1.fill(quad_x, quad_y, color=STYLE["surface"], alpha=0.5)
    ax1.plot(quad_x, quad_y, "-", color=STYLE["accent1"], lw=2)

    verts = [
        (-0.6, -0.6, "v0\n(-0.6, -0.6)"),
        (0.6, -0.6, "v1\n(0.6, -0.6)"),
        (0.6, 0.6, "v2\n(0.6, 0.6)"),
        (-0.6, 0.6, "v3\n(-0.6, 0.6)"),
    ]
    vert_colors = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
        STYLE["accent4"],
    ]
    for (vx, vy, label), vc in zip(verts, vert_colors):
        ax1.plot(vx, vy, "o", color=vc, markersize=8, zorder=5)
        ox = -0.32 if vx < 0 else 0.08
        oy = -0.18 if vy < 0 else 0.06
        ax1.text(
            vx + ox,
            vy + oy,
            label,
            color=vc,
            fontsize=8,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Diagonal line (two triangles)
    ax1.plot([-0.6, 0.6], [-0.6, 0.6], "--", color=STYLE["text_dim"], lw=1)

    ax1.set_title("Position Space", color=STYLE["text"], fontsize=12, fontweight="bold")
    ax1.axhline(0, color=STYLE["grid"], lw=0.5, alpha=0.3)
    ax1.axvline(0, color=STYLE["grid"], lw=0.5, alpha=0.3)

    # Arrow between subplots
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
        "UV mapping",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
    )

    # Right: UV space (texture)
    ax2 = fig.add_subplot(122)
    _setup_axes(ax2, xlim=(-0.15, 1.15), ylim=(-0.15, 1.15), grid=False)

    # Checkerboard texture preview
    for ci in range(4):
        for cj in range(4):
            shade = STYLE["surface"] if (ci + cj) % 2 == 0 else STYLE["grid"]
            r = Rectangle(
                (ci / 4, cj / 4),
                0.25,
                0.25,
                facecolor=shade,
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                zorder=0,
            )
            ax2.add_patch(r)

    # UV quad outline
    uv_x = [0, 1, 1, 0, 0]
    uv_y = [1, 1, 0, 0, 1]
    ax2.plot(uv_x, uv_y, "-", color=STYLE["accent1"], lw=2, alpha=0.7)

    uv_verts = [
        (0, 1, "v0 (0, 1)", STYLE["accent1"]),
        (1, 1, "v1 (1, 1)", STYLE["accent2"]),
        (1, 0, "v2 (1, 0)", STYLE["accent3"]),
        (0, 0, "v3 (0, 0)", STYLE["accent4"]),
    ]
    for ux, uy, label, uc in uv_verts:
        ax2.plot(ux, uy, "o", color=uc, markersize=8, zorder=5)
        ox = -0.12 if ux == 0 else 0.04
        oy = 0.05 if uy == 0 else -0.1
        ax2.text(
            ux + ox,
            uy + oy,
            label,
            color=uc,
            fontsize=8,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax2.set_title(
        "UV / Texture Space", color=STYLE["text"], fontsize=12, fontweight="bold"
    )
    ax2.set_xlabel("U \u2192", color=STYLE["axis"], fontsize=10)
    ax2.set_ylabel("V \u2193 (shown flipped)", color=STYLE["axis"], fontsize=10)

    fig.suptitle(
        "UV Mapping: Position \u2192 Texture Coordinates",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    _save(fig, "gpu/04-textures-and-samplers", "uv_mapping.png")


# ---------------------------------------------------------------------------
# gpu/04-textures-and-samplers — filtering_comparison.png
# ---------------------------------------------------------------------------


def diagram_filtering_comparison():
    """NEAREST vs LINEAR filtering comparison."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # Create a small 4x4 "texture" with distinct values
    texture = np.array(
        [
            [0.2, 0.4, 0.6, 0.8],
            [0.3, 0.5, 0.7, 0.9],
            [0.4, 0.6, 0.8, 1.0],
            [0.1, 0.3, 0.5, 0.7],
        ]
    )

    # Left: NEAREST (blocky)
    ax1 = fig.add_subplot(121)
    ax1.set_facecolor(STYLE["bg"])
    nearest_up = np.repeat(np.repeat(texture, 8, axis=0), 8, axis=1)
    ax1.imshow(
        nearest_up,
        cmap=_FORGE_CMAP,
        interpolation="nearest",
        extent=[0, 4, 0, 4],
        origin="lower",
    )
    for i in range(5):
        ax1.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)
        ax1.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)

    # Sample point + selected texel highlight
    ax1.plot(1.3, 2.7, "x", color=STYLE["warn"], markersize=12, mew=2.5, zorder=5)
    ax1.text(1.5, 2.85, "sample", color=STYLE["warn"], fontsize=9, fontweight="bold")
    selected = Rectangle(
        (1, 2),
        1,
        1,
        fill=False,
        edgecolor=STYLE["warn"],
        linewidth=2.5,
        linestyle="--",
        zorder=4,
    )
    ax1.add_patch(selected)

    ax1.set_title(
        "NEAREST \u2014 picks closest texel",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
    )
    ax1.tick_params(colors=STYLE["axis"], labelsize=8)

    # Right: LINEAR (smooth)
    ax2 = fig.add_subplot(122)
    ax2.set_facecolor(STYLE["bg"])
    ax2.imshow(
        texture,
        cmap=_FORGE_CMAP,
        interpolation="bilinear",
        extent=[0, 4, 0, 4],
        origin="lower",
    )
    for i in range(5):
        ax2.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)
        ax2.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)

    # Sample point + contributing texels
    ax2.plot(1.3, 2.7, "x", color=STYLE["warn"], markersize=12, mew=2.5, zorder=5)
    ax2.text(1.5, 2.85, "sample", color=STYLE["warn"], fontsize=9, fontweight="bold")

    for tx, ty in [(1, 2), (2, 2), (1, 3), (2, 3)]:
        ax2.plot(
            tx + 0.5,
            ty + 0.5,
            "o",
            color=STYLE["accent3"],
            markersize=6,
            zorder=5,
            alpha=0.8,
        )
        ax2.plot(
            [1.3, tx + 0.5],
            [2.7, ty + 0.5],
            "--",
            color=STYLE["accent3"],
            lw=0.8,
            alpha=0.5,
        )

    ax2.set_title(
        "LINEAR \u2014 blends 4 nearest texels",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
    )
    ax2.tick_params(colors=STYLE["axis"], labelsize=8)

    fig.suptitle(
        "Texture Filtering: NEAREST vs LINEAR",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    _save(fig, "gpu/04-textures-and-samplers", "filtering_comparison.png")


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — mip_chain.png
# ---------------------------------------------------------------------------


def diagram_mip_chain():
    """Mip chain showing progressively halved textures with memory cost."""
    fig = plt.figure(figsize=(10, 4), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    # Mip levels: size halves each step
    base = 128  # visual size in plot units for level 0
    levels = 9  # for a 256x256 texture
    gap = 12

    x = 0
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"], STYLE["accent4"]]

    for level in range(levels):
        size = max(base >> level, 2)  # clamp visual size
        color = colors[level % len(colors)]

        # Checkerboard fill for each mip level
        checks = max(1, min(4, size // 8))
        csize = size / checks
        for ci in range(checks):
            for cj in range(checks):
                shade = color if (ci + cj) % 2 == 0 else STYLE["surface"]
                r = Rectangle(
                    (x + ci * csize, -size / 2 + cj * csize),
                    csize,
                    csize,
                    facecolor=shade,
                    edgecolor=STYLE["grid"],
                    linewidth=0.3,
                    alpha=0.6 if shade == color else 0.3,
                    zorder=1,
                )
                ax.add_patch(r)

        # Border
        border = Rectangle(
            (x, -size / 2),
            size,
            size,
            fill=False,
            edgecolor=color,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(border)

        # Label: level number and dimensions
        tex_size = 256 >> level
        if tex_size < 1:
            tex_size = 1
        label = f"L{level}\n{tex_size}"
        ax.text(
            x + size / 2,
            -size / 2 - 8,
            label,
            color=STYLE["text"],
            fontsize=7 if level < 6 else 6,
            ha="center",
            va="top",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Arrow to next level
        if level < levels - 1:
            ax.annotate(
                "",
                xy=(x + size + gap * 0.3, 0),
                xytext=(x + size + 2, 0),
                arrowprops={
                    "arrowstyle": "->",
                    "color": STYLE["text_dim"],
                    "lw": 1,
                },
            )
            ax.text(
                x + size + gap * 0.5,
                5,
                "\u00f72",
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="bottom",
            )

        x += size + gap

    # Memory cost annotation
    ax.text(
        x / 2,
        base / 2 + 15,
        "Each level = \u00bc the texels of the previous  |  Total = ~1.33\u00d7 base  (+33% memory)",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlim(-10, x + 5)
    ax.set_ylim(-base / 2 - 30, base / 2 + 30)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Mip Chain: 256\u00d7256 Texture (9 Levels)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    _save(fig, "math/04-mipmaps-and-lod", "mip_chain.png")


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — trilinear_interpolation.png
# ---------------------------------------------------------------------------


def diagram_trilinear_interpolation():
    """Trilinear interpolation: two bilinear samples blended by LOD fraction."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # --- Left panel: mip level N (bilinear) ---
    ax1 = fig.add_subplot(131)
    _setup_axes(ax1, xlim=(-0.4, 1.6), ylim=(-0.4, 1.6), grid=False)

    # Grid cell
    rect1 = Rectangle(
        (0, 0),
        1,
        1,
        fill=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=1,
    )
    ax1.add_patch(rect1)

    # Corner values
    corners1 = {
        (0, 0): ("c00", STYLE["accent1"]),
        (1, 0): ("c10", STYLE["accent2"]),
        (0, 1): ("c01", STYLE["accent4"]),
        (1, 1): ("c11", STYLE["accent3"]),
    }
    for (cx, cy), (label, color) in corners1.items():
        ax1.plot(cx, cy, "o", color=color, markersize=10, zorder=5)
        ox = -0.22 if cx == 0 else 0.08
        oy = -0.15 if cy == 0 else 0.08
        ax1.text(
            cx + ox,
            cy + oy,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Sample point
    tx, ty = 0.4, 0.6
    ax1.plot(tx, ty, "*", color=STYLE["warn"], markersize=16, zorder=6)

    # Result
    ax1.text(
        0.5,
        -0.28,
        "bilerp\u2081",
        color=STYLE["accent1"],
        fontsize=11,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax1.set_title(
        "Mip Level N",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
    )

    # --- Center panel: mip level N+1 (bilinear) ---
    ax2 = fig.add_subplot(132)
    _setup_axes(ax2, xlim=(-0.4, 1.6), ylim=(-0.4, 1.6), grid=False)

    rect2 = Rectangle(
        (0, 0),
        1,
        1,
        fill=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=1,
    )
    ax2.add_patch(rect2)

    corners2 = {
        (0, 0): ("c00", STYLE["accent1"]),
        (1, 0): ("c10", STYLE["accent2"]),
        (0, 1): ("c01", STYLE["accent4"]),
        (1, 1): ("c11", STYLE["accent3"]),
    }
    for (cx, cy), (label, color) in corners2.items():
        ax2.plot(cx, cy, "o", color=color, markersize=10, zorder=5)
        ox = -0.22 if cx == 0 else 0.08
        oy = -0.15 if cy == 0 else 0.08
        ax2.text(
            cx + ox,
            cy + oy,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax2.plot(tx, ty, "*", color=STYLE["warn"], markersize=16, zorder=6)

    ax2.text(
        0.5,
        -0.28,
        "bilerp\u2082",
        color=STYLE["accent2"],
        fontsize=11,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax2.set_title(
        "Mip Level N+1",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
    )

    # --- Right panel: lerp result ---
    ax3 = fig.add_subplot(133)
    ax3.set_facecolor(STYLE["bg"])
    ax3.set_xlim(0, 1)
    ax3.set_ylim(-0.5, 1.5)
    ax3.set_xticks([])
    ax3.set_yticks([])
    for spine in ax3.spines.values():
        spine.set_visible(False)

    # Vertical blend bar
    bar_x = 0.3
    bar_w = 0.4
    n_steps = 50
    for i in range(n_steps):
        t = i / n_steps
        y0 = t
        h = 1.0 / n_steps
        # Blend from accent1 to accent2
        c1 = np.array([0x4F, 0xC3, 0xF7]) / 255  # accent1 RGB
        c2 = np.array([0xFF, 0x70, 0x43]) / 255  # accent2 RGB
        c = c1 * (1 - t) + c2 * t
        r = Rectangle(
            (bar_x, y0),
            bar_w,
            h,
            facecolor=c,
            edgecolor="none",
            zorder=1,
        )
        ax3.add_patch(r)

    # Border
    bar_border = Rectangle(
        (bar_x, 0),
        bar_w,
        1,
        fill=False,
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        zorder=2,
    )
    ax3.add_patch(bar_border)

    # Labels
    ax3.text(
        bar_x + bar_w / 2,
        -0.08,
        "bilerp\u2081",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        fontweight="bold",
    )
    ax3.text(
        bar_x + bar_w / 2,
        1.08,
        "bilerp\u2082",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        fontweight="bold",
    )

    # LOD fraction marker
    frac = 0.3
    ax3.plot(
        [bar_x - 0.05, bar_x + bar_w + 0.05],
        [frac, frac],
        "-",
        color=STYLE["warn"],
        lw=2.5,
        zorder=3,
    )
    ax3.plot(bar_x + bar_w / 2, frac, "*", color=STYLE["warn"], markersize=14, zorder=4)
    ax3.text(
        bar_x + bar_w + 0.08,
        frac,
        f"frac = {frac}",
        color=STYLE["warn"],
        fontsize=10,
        va="center",
        fontweight="bold",
    )

    ax3.set_title(
        "Lerp by LOD\nFraction",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
    )

    # Arrows connecting panels
    fig.text(
        0.355,
        0.35,
        "\u2192",
        color=STYLE["text_dim"],
        fontsize=20,
        ha="center",
        va="center",
        fontweight="bold",
    )
    fig.text(
        0.655,
        0.35,
        "\u2192",
        color=STYLE["text_dim"],
        fontsize=20,
        ha="center",
        va="center",
        fontweight="bold",
    )

    fig.suptitle(
        "Trilinear Interpolation: Two Bilinear Samples + Lerp",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    _save(fig, "math/04-mipmaps-and-lod", "trilinear_interpolation.png")


# ---------------------------------------------------------------------------
# math/09-view-matrix — view_transform.png
# ---------------------------------------------------------------------------


def diagram_view_transform():
    """Two-panel top-down: world space vs view space with scene objects.

    Inspired by 3D Math Primer illustrations — shows a 2D bird's-eye view
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
        s = 0.38
        cam_tri = Polygon(
            [
                (c[0] - s * 0.8, c[1] + s * 0.55),
                (c[0], c[1] - s * 0.9),
                (c[0] + s * 0.8, c[1] + s * 0.55),
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
    _save(fig, "math/09-view-matrix", "view_transform.png")


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

    # Quaternion rotation: yaw around Y, then pitch around X
    # forward = quat_rotate(q, (0,0,-1))
    # right   = quat_rotate(q, (1,0,0))
    # up      = quat_rotate(q, (0,1,0))
    # For yaw*pitch: Ry(yaw) * Rx(pitch)
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
        _setup_axes(ax, xlim=(-1.5, 1.8), ylim=(-1.5, 1.8), grid=False)

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
            _draw_vector(ax, origin, (px, py), color, label, lw=2.5)

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
    _save(fig, "math/09-view-matrix", "camera_basis_vectors.png")


# ---------------------------------------------------------------------------
# math/10-anisotropy — pixel_footprint.png
# ---------------------------------------------------------------------------


def diagram_pixel_footprint():
    """Two-panel: Jacobian maps circle to ellipse, isotropic vs anisotropic sampling."""
    fig = plt.figure(figsize=(11, 5.5), facecolor=STYLE["bg"])

    # --- Left panel: pixel footprint at different tilt angles ---
    ax1 = fig.add_subplot(121)
    _setup_axes(ax1, xlim=(-2.5, 3.0), ylim=(-2.8, 2.8), grid=False)

    # Faint reference axes
    ax1.axhline(0, color=STYLE["grid"], lw=0.5, alpha=0.4)
    ax1.axvline(0, color=STYLE["grid"], lw=0.5, alpha=0.4)

    theta = np.linspace(0, 2 * np.pi, 100)

    # Draw ellipses at different tilt angles (footprints in texture space)
    tilts = [
        (0, STYLE["accent3"], "0\u00b0 (isotropic)", 1.12),
        (45, STYLE["accent1"], "45\u00b0", 1.12),
        (75, STYLE["accent2"], "75\u00b0 (anisotropic)", 1.12),
    ]

    for tilt_deg, color, label, label_x in tilts:
        tilt_rad = np.radians(tilt_deg)
        cos_t = np.cos(tilt_rad)
        sigma_u = 1.0
        sigma_v = 1.0 / cos_t if cos_t > 0.01 else 100.0
        sigma_v = min(sigma_v, 2.3)

        x = sigma_u * np.cos(theta)
        y = sigma_v * np.sin(theta)
        ax1.plot(x, y, "-", color=color, lw=2.2, alpha=0.85)

        # Label to the right of each ellipse (at the widest point)
        ax1.text(
            label_x,
            sigma_v * 0.7,
            label,
            color=color,
            fontsize=9,
            ha="left",
            va="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Label the singular value axes on the 75-degree ellipse
    # Place arrows and labels on the LEFT side to avoid covering tilt labels
    tilt75 = np.radians(75)
    sv_major = min(1.0 / np.cos(tilt75), 2.3)

    # Major axis arrow (vertical, center to edge = singular value)
    ax1.annotate(
        "",
        xy=(0, sv_major),
        xytext=(0, 0),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
    )
    ax1.text(
        -0.35,
        sv_major / 2,
        "\u03c3\u2081",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    # Minor axis arrow (horizontal, center to edge = singular value)
    ax1.annotate(
        "",
        xy=(1.0, 0),
        xytext=(0, 0),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
    )
    ax1.text(
        0.5,
        -0.35,
        "\u03c3\u2082",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax1.plot(0, 0, "o", color=STYLE["text"], markersize=4, zorder=5)
    ax1.set_title(
        "Pixel Footprint in Texture Space",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax1.set_xlabel("U (texels)", color=STYLE["axis"], fontsize=10)
    ax1.set_ylabel("V (texels)", color=STYLE["axis"], fontsize=10)

    # --- Right panel: isotropic vs anisotropic sampling ---
    ax2 = fig.add_subplot(122)
    _setup_axes(ax2, xlim=(-4.2, 4.2), ylim=(-3.2, 3.2), grid=False)

    # Draw the same 75-degree ellipse in both halves
    sigma_u = 1.0
    sigma_v = min(1.0 / np.cos(tilt75), 2.3)
    ex = sigma_u * np.cos(theta)
    ey = sigma_v * np.sin(theta)

    # Push the two examples further apart to avoid overlap with divider
    offset_l = -2.1
    offset_r = 2.1

    # Left half: isotropic (single large circle covering the ellipse)
    ax2.plot(ex + offset_l, ey, "-", color=STYLE["accent2"], lw=2, alpha=0.7)
    # Isotropic: dashed circle sized to the major axis, clipped to panel
    iso_radius = sigma_v
    ax2.plot(
        iso_radius * np.cos(theta) + offset_l,
        iso_radius * np.sin(theta),
        "--",
        color=STYLE["text_dim"],
        lw=1.2,
        alpha=0.5,
        clip_on=True,
    )
    ax2.plot(
        offset_l,
        0,
        "o",
        color=STYLE["accent2"],
        markersize=10,
        zorder=5,
        alpha=0.8,
    )
    mip_iso = np.log2(sigma_v)
    ax2.text(
        offset_l,
        -sigma_v - 0.45,
        f"Isotropic\n1 sample, mip {mip_iso:.1f}",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        offset_l,
        sigma_v + 0.35,
        "BLURRY",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        alpha=0.8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Right half: anisotropic (multiple small samples along major axis)
    ax2.plot(ex + offset_r, ey, "-", color=STYLE["accent3"], lw=2, alpha=0.7)
    n_samples = max(1, int(np.ceil(sigma_v / sigma_u)))
    for i in range(n_samples):
        t = (i + 0.5) / n_samples
        sy = -sigma_v + 2 * sigma_v * t
        ax2.plot(
            sigma_u * np.cos(theta) * 0.85 + offset_r,
            sigma_u * np.sin(theta) * 0.85 + sy,
            "-",
            color=STYLE["text_dim"],
            lw=0.7,
            alpha=0.4,
        )
        ax2.plot(
            offset_r,
            sy,
            "o",
            color=STYLE["accent3"],
            markersize=5,
            zorder=5,
            alpha=0.7,
        )
    mip_aniso = np.log2(max(sigma_u, 1e-6))
    ax2.text(
        offset_r,
        -sigma_v - 0.45,
        f"Anisotropic\n{n_samples} samples, mip {mip_aniso:.1f}",
        color=STYLE["accent3"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        offset_r,
        sigma_v + 0.35,
        "SHARP",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        alpha=0.8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Divider line
    ax2.plot([0, 0], [-3.0, 3.0], "-", color=STYLE["grid"], lw=0.8, alpha=0.5)
    ax2.text(
        0,
        3.0,
        "vs",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="bottom",
        style="italic",
    )

    ax2.set_title(
        "Isotropic vs Anisotropic Filtering",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])
    for spine in ax2.spines.values():
        spine.set_visible(False)

    fig.suptitle(
        "Anisotropy: How the Pixel Footprint Drives Texture Filtering",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    _save(fig, "math/10-anisotropy", "pixel_footprint.png")


# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — blinn_phong_vectors.png
# ---------------------------------------------------------------------------


def diagram_blinn_phong_vectors():
    """Classic lighting vector diagram: surface point with N, L, V, H, R vectors.

    This is the canonical diagram found in every graphics textbook.  A flat
    surface segment is shown from the side, with the five key vectors radiating
    from the shading point: normal (N), light direction (L), view direction (V),
    Blinn half-vector (H), and Phong reflection (R).  Angle arcs show the
    relationships that drive each shading term.
    """
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    # --- Surface ---
    surf_y = 0.0
    surf_left = -3.5
    surf_right = 3.5
    ax.fill_between(
        [surf_left, surf_right],
        [surf_y, surf_y],
        [surf_y - 0.6, surf_y - 0.6],
        color=STYLE["surface"],
        alpha=0.8,
        zorder=1,
    )
    ax.plot(
        [surf_left, surf_right],
        [surf_y, surf_y],
        "-",
        color=STYLE["axis"],
        lw=2,
        zorder=2,
    )
    ax.text(
        surf_right - 0.1,
        surf_y - 0.35,
        "surface",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="right",
        style="italic",
    )

    # --- Shading point (P) ---
    P = np.array([0.0, surf_y])
    ax.plot(P[0], P[1], "o", color=STYLE["text"], markersize=7, zorder=10)
    ax.text(
        P[0] - 0.25,
        P[1] - 0.35,
        "P",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
    )

    # --- Define vector directions (all unit length, displayed as arrows) ---
    # Angles from vertical (the normal direction)
    light_angle = np.radians(40)  # L is 40° from N
    view_angle = np.radians(55)  # V is 55° on the other side

    arrow_len = 2.8

    # Normal N — straight up from surface
    N_dir = np.array([0.0, 1.0])
    N_end = N_dir * arrow_len

    # Light direction L — upper-left (40° from N toward the left)
    L_dir = np.array([-np.sin(light_angle), np.cos(light_angle)])
    L_end = L_dir * arrow_len

    # View direction V — upper-right (55° from N toward the right)
    V_dir = np.array([np.sin(view_angle), np.cos(view_angle)])
    V_end = V_dir * arrow_len

    # Reflection R — reflect L about N:  R = 2(N·L)N - L
    NdotL = np.dot(N_dir, L_dir)
    R_dir = 2.0 * NdotL * N_dir - L_dir
    R_dir = R_dir / np.linalg.norm(R_dir)
    R_end = R_dir * arrow_len

    # Half-vector H = normalize(L + V)
    H_raw = L_dir + V_dir
    H_dir = H_raw / np.linalg.norm(H_raw)
    H_end = H_dir * (arrow_len * 0.85)  # slightly shorter to distinguish

    # --- Draw vectors ---
    def draw_arrow(start, end, color, lw=2.5):
        ax.annotate(
            "",
            xy=(start[0] + end[0], start[1] + end[1]),
            xytext=(start[0], start[1]),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.12",
                "color": color,
                "lw": lw,
            },
            zorder=5,
        )

    def label_vec(end, text, color, offset):
        pos = P + end
        ax.text(
            pos[0] + offset[0],
            pos[1] + offset[1],
            text,
            color=color,
            fontsize=14,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
            zorder=8,
        )

    # N — normal (green)
    draw_arrow(P, N_end, STYLE["accent3"], lw=3)
    label_vec(N_end, "N", STYLE["accent3"], (0.3, 0.15))

    # L — light direction (cyan)
    draw_arrow(P, L_end, STYLE["accent1"], lw=2.5)
    label_vec(L_end, "L", STYLE["accent1"], (-0.3, 0.15))

    # V — view direction (orange)
    draw_arrow(P, V_end, STYLE["accent2"], lw=2.5)
    label_vec(V_end, "V", STYLE["accent2"], (0.35, 0.1))

    # H — half-vector (yellow/warn)
    draw_arrow(P, H_end, STYLE["warn"], lw=2)
    label_vec(H_end, "H", STYLE["warn"], (0.3, 0.2))

    # R — reflection (purple, dashed)
    ax.annotate(
        "",
        xy=(P[0] + R_end[0], P[1] + R_end[1]),
        xytext=(P[0], P[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["accent4"],
            "lw": 2,
            "linestyle": "dashed",
        },
        zorder=4,
    )
    label_vec(R_end, "R", STYLE["accent4"], (0.35, 0.0))

    # --- Angle arcs ---
    def draw_arc(from_angle, to_angle, radius, color, label, label_r=None):
        """Draw an arc from from_angle to to_angle (in radians from +Y axis,
        clockwise positive) with a label."""
        # Convert from "angle from +Y" to standard polar (from +X axis)
        # +Y axis = 90° in polar.  "from +Y clockwise" = 90° - angle in polar
        a1 = np.pi / 2 - from_angle
        a2 = np.pi / 2 - to_angle
        if a1 > a2:
            a1, a2 = a2, a1
        t = np.linspace(a1, a2, 40)
        ax.plot(
            P[0] + radius * np.cos(t),
            P[1] + radius * np.sin(t),
            "-",
            color=color,
            lw=1.5,
            alpha=0.8,
            zorder=6,
        )
        # Label at midpoint
        mid = (a1 + a2) / 2
        lr = label_r if label_r else radius + 0.2
        ax.text(
            P[0] + lr * np.cos(mid),
            P[1] + lr * np.sin(mid),
            label,
            color=color,
            fontsize=11,
            ha="center",
            va="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=7,
        )

    # Angle between N and L (theta_i — angle of incidence)
    # N is at 0° from Y, L is at -light_angle from Y
    draw_arc(0, -light_angle, 1.0, STYLE["accent1"], "\u03b8\u1d62", 1.3)

    # Angle between N and H (alpha — for Blinn specular)
    # H direction angle from Y
    H_angle_from_y = np.arctan2(H_dir[0], H_dir[1])
    draw_arc(0, H_angle_from_y, 0.7, STYLE["warn"], "\u03b1", 0.95)

    # Angle between N and R (to show reflection is symmetric with L)
    R_angle_from_y = np.arctan2(R_dir[0], R_dir[1])
    draw_arc(0, R_angle_from_y, 1.5, STYLE["accent4"], "\u03b8\u1d63", 1.8)

    # --- Text annotations below surface ---
    anno_y = -1.3
    annotations = [
        (
            STYLE["accent3"],
            "N = surface normal",
        ),
        (
            STYLE["accent1"],
            "L = direction toward light",
        ),
        (
            STYLE["accent2"],
            "V = direction toward viewer",
        ),
        (
            STYLE["warn"],
            "H = normalize(L + V)  \u2190 Blinn half-vector",
        ),
        (
            STYLE["accent4"],
            "R = reflect(\u2212L, N)  \u2190 Phong reflection",
        ),
    ]
    for i, (color, text) in enumerate(annotations):
        ax.text(
            -3.3,
            anno_y - i * 0.42,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- Equation summary at bottom ---
    eq_y = anno_y - len(annotations) * 0.42 - 0.3
    eqs = [
        ("Diffuse:", "max(dot(N, L), 0) \u00d7 color", STYLE["accent1"]),
        ("Specular (Blinn):", "pow(max(dot(N, H), 0), shininess)", STYLE["warn"]),
        ("Specular (Phong):", "pow(max(dot(R, V), 0), shininess)", STYLE["accent4"]),
    ]
    for i, (label, eq, color) in enumerate(eqs):
        ax.text(
            -3.3,
            eq_y - i * 0.42,
            f"{label}  {eq}",
            color=color,
            fontsize=9,
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- Final layout ---
    ax.set_xlim(-3.8, 4.0)
    ax.set_ylim(eq_y - 0.8, 3.8)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Blinn-Phong Lighting Vectors",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    _save(fig, "gpu/10-basic-lighting", "blinn_phong_vectors.png")


# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — specular_comparison.png
# ---------------------------------------------------------------------------


def diagram_specular_comparison():
    """Three-panel showing how shininess affects the specular highlight.

    Plots the Blinn specular function pow(max(cos(alpha), 0), shininess)
    for three different exponents, showing how higher values produce
    tighter highlights.
    """
    fig = plt.figure(figsize=(10, 4.5), facecolor=STYLE["bg"])

    shininess_values = [
        (8, "shininess = 8\n(rough)", STYLE["accent2"]),
        (64, "shininess = 64\n(plastic)", STYLE["accent1"]),
        (256, "shininess = 256\n(polished)", STYLE["accent3"]),
    ]

    alpha = np.linspace(-np.pi / 2, np.pi / 2, 300)
    cos_alpha = np.maximum(np.cos(alpha), 0.0)

    for i, (s, label, color) in enumerate(shininess_values):
        ax = fig.add_subplot(1, 3, i + 1)
        _setup_axes(ax, xlim=(-90, 90), ylim=(-0.05, 1.15), grid=False, aspect="auto")

        intensity = cos_alpha**s

        # Fill under curve
        ax.fill_between(
            np.degrees(alpha),
            intensity,
            0,
            color=color,
            alpha=0.15,
            zorder=1,
        )
        # Curve
        ax.plot(np.degrees(alpha), intensity, "-", color=color, lw=2.5, zorder=2)

        # Reference lines
        ax.axhline(0, color=STYLE["grid"], lw=0.5, alpha=0.5)
        ax.axvline(0, color=STYLE["grid"], lw=0.5, alpha=0.3)

        ax.set_title(label, color=color, fontsize=11, fontweight="bold")
        ax.set_xlabel("\u03b1 (degrees from N)", color=STYLE["axis"], fontsize=9)
        if i == 0:
            ax.set_ylabel("specular intensity", color=STYLE["axis"], fontsize=9)
        ax.tick_params(colors=STYLE["axis"], labelsize=8)
        ax.set_xticks([-90, -45, 0, 45, 90])

        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Specular Highlight vs Shininess Exponent",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    _save(fig, "gpu/10-basic-lighting", "specular_comparison.png")


# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — normal_transformation.png
# ---------------------------------------------------------------------------


def diagram_normal_transformation():
    """Two-panel diagram showing why normals need special transformation.

    Left:  Object space — a circle with a tangent and a perpendicular normal.
    Right: After non-uniform scale (2x horizontal) — shows the WRONG result
           (plain model matrix) and the CORRECT result (adjugate transpose).
    The wrong normal is visibly not perpendicular to the surface; the correct
    one is.
    """
    fig = plt.figure(figsize=(12, 5.5), facecolor=STYLE["bg"])

    # --- Shared geometry ---
    theta = np.linspace(0, 2 * np.pi, 200)

    # Point on the circle at ~60 degrees (nice visual angle)
    t_param = np.radians(60)
    px, py = np.cos(t_param), np.sin(t_param)

    # Tangent at that point (derivative of (cos t, sin t) = (-sin t, cos t))
    tx, ty = -np.sin(t_param), np.cos(t_param)
    tangent_len = 0.9

    # Normal at that point (perpendicular to tangent, pointing outward)
    nx, ny = np.cos(t_param), np.sin(t_param)
    normal_len = 1.1

    # Non-uniform scale: stretch X by 2, keep Y
    sx, sy = 2.0, 1.0

    # --- Left panel: Object space ---
    ax1 = fig.add_subplot(121)
    _setup_axes(ax1, xlim=(-2.0, 2.5), ylim=(-1.8, 2.5), grid=False)
    ax1.set_title(
        "Object Space (circle)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # Draw circle
    ax1.plot(
        np.cos(theta),
        np.sin(theta),
        "-",
        color=STYLE["text_dim"],
        lw=2,
        alpha=0.8,
    )
    ax1.fill(np.cos(theta), np.sin(theta), color=STYLE["surface"], alpha=0.4)

    # Draw point
    ax1.plot(px, py, "o", color=STYLE["text"], markersize=6, zorder=5)
    ax1.text(
        px + 0.15,
        py + 0.2,
        "P",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Draw tangent
    _draw_vector(
        ax1,
        (px, py),
        (tx * tangent_len, ty * tangent_len),
        STYLE["accent1"],
        "T",
        label_offset=(0.2, 0.1),
    )

    # Draw normal
    _draw_vector(
        ax1,
        (px, py),
        (nx * normal_len, ny * normal_len),
        STYLE["accent3"],
        "N",
        label_offset=(0.15, 0.15),
    )

    # Perpendicularity indicator (small square)
    sq_size = 0.12
    sq_t = np.array([tx, ty]) * sq_size
    sq_n = np.array([nx, ny]) * sq_size
    sq_pts = np.array(
        [
            [px + sq_t[0], py + sq_t[1]],
            [px + sq_t[0] + sq_n[0], py + sq_t[1] + sq_n[1]],
            [px + sq_n[0], py + sq_n[1]],
        ]
    )
    ax1.plot(
        [sq_pts[0, 0], sq_pts[1, 0], sq_pts[2, 0]],
        [sq_pts[0, 1], sq_pts[1, 1], sq_pts[2, 1]],
        "-",
        color=STYLE["text"],
        lw=1.0,
        alpha=0.6,
    )

    ax1.text(
        0.0,
        -1.5,
        "N \u22a5 T  (perpendicular)",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Right panel: After non-uniform scale ---
    ax2 = fig.add_subplot(122)
    _setup_axes(ax2, xlim=(-3.5, 4.5), ylim=(-2.0, 3.8), grid=False)
    ax2.set_title(
        "World Space  (scale 2\u00d71)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # Draw ellipse (scaled circle)
    ax2.plot(
        sx * np.cos(theta),
        sy * np.sin(theta),
        "-",
        color=STYLE["text_dim"],
        lw=2,
        alpha=0.8,
    )
    ax2.fill(
        sx * np.cos(theta),
        sy * np.sin(theta),
        color=STYLE["surface"],
        alpha=0.4,
    )

    # Transformed point
    wpx, wpy = sx * px, sy * py
    ax2.plot(wpx, wpy, "o", color=STYLE["text"], markersize=6, zorder=5)
    ax2.text(
        wpx + 0.2,
        wpy - 0.25,
        "P'",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Transformed tangent (correct — tangents transform by M)
    wtx, wty = sx * tx, sy * ty
    wt_len = np.sqrt(wtx**2 + wty**2)
    wtx_n, wty_n = wtx / wt_len * tangent_len, wty / wt_len * tangent_len
    t_scale = 1.5
    _draw_vector(
        ax2,
        (wpx, wpy),
        (wtx_n * t_scale, wty_n * t_scale),
        STYLE["accent1"],
        label=None,
    )
    # Label at arrow tip
    t_end = (wpx + wtx_n * t_scale, wpy + wty_n * t_scale)
    ax2.text(
        t_end[0] - 0.35,
        t_end[1] + 0.2,
        "M\u00b7T",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # WRONG normal: transform by M (same as tangent) — not perpendicular
    wnx_bad, wny_bad = sx * nx, sy * ny
    wn_bad_len = np.sqrt(wnx_bad**2 + wny_bad**2)
    wnx_bad_n = wnx_bad / wn_bad_len * normal_len
    wny_bad_n = wny_bad / wn_bad_len * normal_len
    n_scale = 1.4
    _draw_vector(
        ax2,
        (wpx, wpy),
        (wnx_bad_n * n_scale, wny_bad_n * n_scale),
        STYLE["accent2"],
        label=None,
        lw=2.0,
    )
    # Label at arrow tip — offset to the right
    bad_end = (wpx + wnx_bad_n * n_scale, wpy + wny_bad_n * n_scale)
    ax2.text(
        bad_end[0] + 0.55,
        bad_end[1] + 0.1,
        "M\u00b7N  \u2717",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # CORRECT normal: adjugate transpose = (sy, sx) for diagonal 2D scale
    # For scale(sx, sy), adj^T = diag(sy, sx) (the cofactor matrix)
    cnx, cny = sy * nx, sx * ny
    cn_len = np.sqrt(cnx**2 + cny**2)
    cnx_n = cnx / cn_len * normal_len
    cny_n = cny / cn_len * normal_len
    _draw_vector(
        ax2,
        (wpx, wpy),
        (cnx_n * n_scale, cny_n * n_scale),
        STYLE["accent3"],
        label=None,
    )
    # Label at arrow tip — offset to the left
    good_end = (wpx + cnx_n * n_scale, wpy + cny_n * n_scale)
    ax2.text(
        good_end[0] - 0.55,
        good_end[1] + 0.1,
        "\u2713  adj\u1d40\u00b7N",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="right",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Perpendicularity marker between correct normal and tangent
    cdir = np.array([cnx_n, cny_n])
    cdir = cdir / np.linalg.norm(cdir)
    tdir = np.array([wtx_n, wty_n])
    tdir = tdir / np.linalg.norm(tdir)
    sq_size2 = 0.15
    sq_t2 = tdir * sq_size2
    sq_n2 = cdir * sq_size2
    sq_pts2 = np.array(
        [
            [wpx + sq_t2[0], wpy + sq_t2[1]],
            [wpx + sq_t2[0] + sq_n2[0], wpy + sq_t2[1] + sq_n2[1]],
            [wpx + sq_n2[0], wpy + sq_n2[1]],
        ]
    )
    ax2.plot(
        [sq_pts2[0, 0], sq_pts2[1, 0], sq_pts2[2, 0]],
        [sq_pts2[0, 1], sq_pts2[1, 1], sq_pts2[2, 1]],
        "-",
        color=STYLE["accent3"],
        lw=1.0,
        alpha=0.8,
    )

    # Legend — two separate colored text elements, no overlapping markers
    ax2.text(
        -1.4,
        -1.7,
        "\u2717  M\u00b7N is wrong",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        2.4,
        -1.7,
        "\u2713  adj(M)\u1d40\u00b7N is correct",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    _save(fig, "gpu/10-basic-lighting", "normal_transformation.png")


# ---------------------------------------------------------------------------
# gpu/11-compute-shaders — fullscreen_triangle.png
# ---------------------------------------------------------------------------


def diagram_fullscreen_triangle():
    """Two-panel comparison: fullscreen quad vs fullscreen triangle.

    Left:  A quad made of two triangles with a diagonal seam and redundant
           fragment processing along the diagonal.
    Right: A single oversized triangle that covers the viewport, with the
           clipped region shown.
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 5), facecolor=STYLE["bg"])

    for ax in (ax1, ax2):
        _setup_axes(ax, xlim=(-2.0, 4.0), ylim=(-2.0, 4.0), grid=False)
        ax.set_xticks([-1, 0, 1, 2, 3])
        ax.set_yticks([-1, 0, 1, 2, 3])
        ax.tick_params(colors=STYLE["axis"], labelsize=8)

    # --- Left panel: fullscreen quad (two triangles) ---
    ax1.set_title(
        "Fullscreen Quad (2 triangles)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # The viewport rectangle [-1,1] x [-1,1]
    viewport = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=2,
        edgecolor=STYLE["text"],
        facecolor="none",
        linestyle="--",
        zorder=5,
    )
    ax1.add_patch(viewport)

    # Triangle 1 (lower-left)
    tri1 = Polygon(
        [(-1, -1), (1, -1), (-1, 1)],
        closed=True,
        facecolor=STYLE["accent1"],
        alpha=0.3,
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=3,
    )
    ax1.add_patch(tri1)

    # Triangle 2 (upper-right)
    tri2 = Polygon(
        [(1, 1), (-1, 1), (1, -1)],
        closed=True,
        facecolor=STYLE["accent2"],
        alpha=0.3,
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=3,
    )
    ax1.add_patch(tri2)

    # Diagonal seam
    ax1.plot(
        [-1, 1],
        [1, -1],
        color=STYLE["warn"],
        linewidth=2.5,
        linestyle="-",
        zorder=6,
    )
    ax1.text(
        0.15,
        0.15,
        "diagonal\nseam",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=-45,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # Vertex labels — position each individually to avoid overlaps
    for (x, y), label, (ox, oy), va in [
        ((-1, -1), "(-1,-1)", (0, -0.3), "top"),
        ((1, -1), "(1,-1)", (0, -0.3), "top"),
        ((-1, 1), "(-1, 1)", (-0.35, 0), "center"),
        ((1, 1), "(1, 1)", (0.35, 0), "center"),
    ]:
        ax1.plot(x, y, "o", color=STYLE["text"], markersize=6, zorder=8)
        ax1.text(
            x + ox,
            y + oy,
            label,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va=va,
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax1.text(
        -0.6,
        -0.55,
        "Tri 1",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax1.text(
        0.55,
        0.65,
        "Tri 2",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Annotations
    ax1.text(
        0.0,
        -1.7,
        "4 vertices, 2 triangles, 6 indices\nfragments on diagonal processed twice",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    # --- Right panel: fullscreen triangle (single oversized) ---
    ax2.set_title(
        "Fullscreen Triangle (1 triangle)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # The viewport rectangle
    viewport2 = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=2,
        edgecolor=STYLE["text"],
        facecolor="none",
        linestyle="--",
        zorder=5,
    )
    ax2.add_patch(viewport2)

    # The oversized triangle: (-1,-1), (3,-1), (-1,3)
    tri_full = Polygon(
        [(-1, -1), (3, -1), (-1, 3)],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.15,
        edgecolor=STYLE["accent3"],
        linewidth=2,
        linestyle=":",
        zorder=2,
    )
    ax2.add_patch(tri_full)

    # The visible (clipped) portion
    tri_visible = Polygon(
        [(-1, -1), (1, -1), (1, 1), (-1, 1)],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.35,
        edgecolor=STYLE["accent3"],
        linewidth=2,
        zorder=3,
    )
    ax2.add_patch(tri_visible)

    # Vertex labels for the triangle — positioned to avoid edge clipping
    for (x, y), label, (ox, oy), ha in [
        ((-1, -1), "(-1,-1)\nid=0", (-0.1, -0.35), "center"),
        ((3, -1), "(3,-1)\nid=1", (-0.35, 0.35), "center"),
        ((-1, 3), "(-1, 3)\nid=2", (0.35, 0.0), "left"),
    ]:
        ax2.plot(x, y, "o", color=STYLE["accent3"], markersize=8, zorder=8)
        ax2.text(
            x + ox,
            y + oy,
            label,
            color=STYLE["accent3"],
            fontsize=9,
            fontweight="bold",
            ha=ha,
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Label the clipped region
    ax2.text(
        0.0,
        0.0,
        "visible\nregion",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=6,
    )

    # Label the clipped-away region
    ax2.text(
        1.6,
        0.5,
        "clipped\naway",
        color=STYLE["text_dim"],
        fontsize=9,
        style="italic",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # "Viewport" label
    ax2.text(
        1.0,
        1.15,
        "viewport",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax2.text(
        0.0,
        -1.7,
        "3 vertices, 1 triangle, 0 indices\nno seam, no redundant fragments",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    fig.tight_layout()
    _save(fig, "gpu/11-compute-shaders", "fullscreen_triangle.png")


# ---------------------------------------------------------------------------
# Diagram registry
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# GPU Lesson 12 — Shader Grid: undersampling / Nyquist aliasing
# ---------------------------------------------------------------------------


def diagram_undersampling():
    """Three-panel diagram showing adequate sampling vs undersampling.

    Demonstrates the Nyquist-Shannon sampling theorem visually:
    - Left:   High-frequency signal sampled above the Nyquist rate (correct)
    - Centre: Same signal sampled BELOW the Nyquist rate (aliased)
    - Right:  Grid analogy — adequate vs inadequate pixel density

    This connects the mathematical concept (sampling theorem) to the practical
    problem (moire on a procedural grid) that fwidth()/smoothstep() solves.
    """
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # -- Shared signal: a sine wave representing a grid-like periodic pattern --
    t_fine = np.linspace(0, 2, 1000)
    freq = 5.0  # 5 Hz signal
    signal = np.sin(2 * np.pi * freq * t_fine)

    # ── Left panel: adequate sampling (above Nyquist rate) ────────────────
    ax = axes[0]
    _setup_axes(ax, xlim=(-0.05, 2.05), ylim=(-1.6, 1.6), grid=False, aspect="auto")

    ax.set_title(
        "Adequate Sampling (fs > 2f)",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Continuous signal (faint)
    ax.plot(t_fine, signal, color=STYLE["accent1"], alpha=0.35, linewidth=1.5)

    # Sample points: 12 samples/sec >> 2*5 = 10 (Nyquist rate)
    fs_good = 25
    t_good = np.linspace(0, 2, fs_good * 2 + 1)
    s_good = np.sin(2 * np.pi * freq * t_good)
    ax.plot(
        t_good,
        s_good,
        "o",
        color=STYLE["accent1"],
        markersize=5,
        zorder=5,
    )

    # Reconstructed signal from samples
    ax.plot(t_good, s_good, color=STYLE["accent1"], linewidth=2.0, zorder=4)

    ax.set_xlabel(
        "Position (world space)",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.set_ylabel(
        "Grid pattern",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.text(
        1.0,
        -1.35,
        "Samples capture the signal faithfully",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # ── Centre panel: undersampling (below Nyquist rate → alias) ──────────
    ax = axes[1]
    _setup_axes(ax, xlim=(-0.05, 2.05), ylim=(-1.6, 1.6), grid=False, aspect="auto")

    ax.set_title(
        "Undersampling (fs < 2f) → Aliasing",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Original signal (faint)
    ax.plot(t_fine, signal, color=STYLE["accent1"], alpha=0.25, linewidth=1.5)

    # Too few samples: 3 samples/sec < 2*5 = 10 (below Nyquist)
    fs_bad = 3
    t_bad = np.linspace(0, 2, fs_bad * 2 + 1)
    s_bad = np.sin(2 * np.pi * freq * t_bad)
    ax.plot(
        t_bad,
        s_bad,
        "o",
        color=STYLE["accent2"],
        markersize=7,
        zorder=5,
    )

    # The alias: connecting the sparse samples shows a WRONG low-frequency wave.
    # np.interp does piecewise linear — sufficient to show the alias clearly.
    t_interp = np.linspace(0, 2, 500)
    s_interp = np.interp(t_interp, t_bad, s_bad)
    ax.plot(
        t_interp,
        s_interp,
        color=STYLE["accent2"],
        linewidth=2.5,
        linestyle="-",
        zorder=4,
        label="Perceived (alias)",
    )

    ax.set_xlabel(
        "Position (world space)",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.text(
        1.0,
        -1.35,
        "Sparse samples reconstruct a false signal (moire)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # ── Right panel: grid analogy (pixels vs grid frequency) ──────────────
    ax = axes[2]
    _setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-0.5, 10.5), grid=False, aspect="equal")

    ax.set_title(
        "Grid Cells vs Pixel Density",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Draw grid lines (the signal being sampled)
    for i in range(11):
        ax.axhline(y=i, color=STYLE["accent1"], linewidth=0.8, alpha=0.5, zorder=1)
        ax.axvline(x=i, color=STYLE["accent1"], linewidth=0.8, alpha=0.5, zorder=1)

    # Top half: dense pixel grid (well-sampled) — small dots
    for px in np.arange(0.25, 10.0, 0.5):
        for py in np.arange(5.5, 10.0, 0.5):
            ax.plot(
                px,
                py,
                "s",
                color=STYLE["accent3"],
                markersize=2,
                alpha=0.6,
                zorder=3,
            )

    # Bottom half: sparse pixel grid (undersampled) — large dots
    for px in np.arange(0.75, 10.0, 2.5):
        for py in np.arange(0.75, 5.0, 2.5):
            ax.plot(
                px,
                py,
                "s",
                color=STYLE["accent2"],
                markersize=6,
                alpha=0.8,
                zorder=3,
            )

    # Dividing line
    ax.axhline(y=5.0, color=STYLE["text"], linewidth=1.5, linestyle="--", zorder=4)

    ax.text(
        5.0,
        7.75,
        "Close: many pixels per cell\n(above Nyquist → crisp lines)",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        5.0,
        2.25,
        "Far: few pixels per cell\n(below Nyquist → moire)",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    ax.set_xlabel("World X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("World Z", color=STYLE["axis"], fontsize=9)

    fig.tight_layout(pad=1.5)
    _save(fig, "gpu/12-shader-grid", "undersampling.png")


# ---------------------------------------------------------------------------
# gpu/14-environment-mapping — reflection_mapping.png
# ---------------------------------------------------------------------------


def diagram_reflection_mapping():
    """Environment reflection mapping: incident view ray reflects off a surface
    and samples a cube map.

    Shows the key vectors for environment mapping: V (view direction from surface
    to camera), N (surface normal), I = -V (incident direction), and R (the
    reflected direction that samples the cube map).  The formula
    R = I - 2(I . N)N is annotated.  A small cube map indicator shows how R
    is used to sample the environment.
    """
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    # --- Surface (slightly curved to suggest a hull) ---
    surf_y = 0.0
    surf_x = np.linspace(-4.0, 4.0, 100)
    surf_curve = surf_y + 0.04 * (surf_x**2)  # gentle upward curve
    ax.fill_between(
        surf_x,
        surf_curve,
        surf_curve - 0.5,
        color=STYLE["surface"],
        alpha=0.8,
        zorder=1,
    )
    ax.plot(surf_x, surf_curve, "-", color=STYLE["axis"], lw=2, zorder=2)
    ax.text(
        3.6,
        surf_y - 0.25,
        "surface",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="right",
        style="italic",
    )

    # --- Shading point P ---
    P = np.array([0.0, surf_y])
    ax.plot(P[0], P[1], "o", color=STYLE["text"], markersize=7, zorder=10)
    ax.text(
        P[0] - 0.25,
        P[1] - 0.4,
        "P",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
    )

    # --- Vector definitions ---
    arrow_len = 2.8
    view_angle = np.radians(50)  # V is 50 degrees from N toward the right

    # Normal N — straight up from surface
    N_dir = np.array([0.0, 1.0])
    N_end = N_dir * arrow_len

    # View direction V — upper-right (from surface toward camera)
    V_dir = np.array([np.sin(view_angle), np.cos(view_angle)])
    V_end = V_dir * arrow_len

    # Incident direction I = -V (from camera toward surface)
    I_dir = -V_dir

    # Reflected direction R = I - 2(I . N)N
    IdotN = np.dot(I_dir, N_dir)
    R_dir = I_dir - 2.0 * IdotN * N_dir
    R_dir = R_dir / np.linalg.norm(R_dir)
    R_end = R_dir * arrow_len

    # --- Draw the incoming ray (dashed, from upper-right toward P) ---
    # Show a ray coming from the camera toward the surface
    incoming_start = P + V_dir * 3.2
    ax.annotate(
        "",
        xy=(P[0], P[1]),
        xytext=(incoming_start[0], incoming_start[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["text_dim"],
            "lw": 1.5,
            "linestyle": "dashed",
        },
        zorder=3,
    )
    # "eye" label
    ax.text(
        incoming_start[0] + 0.2,
        incoming_start[1] + 0.15,
        "eye",
        color=STYLE["text_dim"],
        fontsize=10,
        style="italic",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Draw vectors from P ---
    def draw_arrow(start, end, color, lw=2.5, ls="-"):
        ax.annotate(
            "",
            xy=(start[0] + end[0], start[1] + end[1]),
            xytext=(start[0], start[1]),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.12",
                "color": color,
                "lw": lw,
                "linestyle": ls,
            },
            zorder=5,
        )

    def label_vec(end, text, color, offset):
        pos = P + end
        ax.text(
            pos[0] + offset[0],
            pos[1] + offset[1],
            text,
            color=color,
            fontsize=14,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
            zorder=8,
        )

    # N — surface normal (green)
    draw_arrow(P, N_end, STYLE["accent3"], lw=3)
    label_vec(N_end, "N", STYLE["accent3"], (0.3, 0.15))

    # V — view direction (orange)
    draw_arrow(P, V_end, STYLE["accent2"], lw=2.5)
    label_vec(V_end, "V", STYLE["accent2"], (0.35, 0.1))

    # R — reflected direction (cyan, prominent)
    draw_arrow(P, R_end, STYLE["accent1"], lw=3)
    label_vec(R_end, "R", STYLE["accent1"], (-0.35, 0.15))

    # I = -V — incident direction (dim, dashed, shorter)
    I_end_short = I_dir * (arrow_len * 0.6)
    draw_arrow(P, I_end_short, STYLE["text_dim"], lw=1.5, ls="dashed")
    label_vec(I_end_short, "I = \u2212V", STYLE["text_dim"], (-0.1, -0.35))

    # --- Symmetry guides (purple) showing I and R are mirror images about N ---
    I_tip = P + I_dir * arrow_len * 0.6
    R_tip = P + R_dir * arrow_len * 0.6

    # Thin dotted lines from I tip and R tip through the normal axis
    ax.plot(
        [I_tip[0], I_tip[0], R_tip[0]],
        [I_tip[1], N_dir[1] * np.dot(I_tip - P, N_dir) + P[1], R_tip[1]],
        "--",
        color=STYLE["accent4"],
        lw=1,
        alpha=0.5,
        zorder=3,
    )

    # --- Angle arcs ---
    def draw_arc(angle_from, angle_to, radius, color, label, label_r=None):
        # Angles in radians from +Y axis (clockwise positive)
        a1 = np.pi / 2 - angle_from
        a2 = np.pi / 2 - angle_to
        if a1 > a2:
            a1, a2 = a2, a1
        t = np.linspace(a1, a2, 40)
        ax.plot(
            P[0] + radius * np.cos(t),
            P[1] + radius * np.sin(t),
            "-",
            color=color,
            lw=1.5,
            alpha=0.8,
            zorder=6,
        )
        mid_t = (a1 + a2) / 2
        lr = label_r if label_r else radius + 0.2
        ax.text(
            P[0] + lr * np.cos(mid_t),
            P[1] + lr * np.sin(mid_t),
            label,
            color=color,
            fontsize=11,
            ha="center",
            va="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=7,
        )

    # Angle between N and V (theta)
    V_angle_from_y = np.arctan2(V_dir[0], V_dir[1])
    draw_arc(0, V_angle_from_y, 1.0, STYLE["accent2"], "\u03b8", 1.3)

    # Angle between N and R (also theta — reflection is symmetric)
    R_angle_from_y = np.arctan2(R_dir[0], R_dir[1])
    draw_arc(0, R_angle_from_y, 1.4, STYLE["accent1"], "\u03b8", 1.7)

    # --- Cube map indicator (small box in upper-left corner) ---
    cube_cx, cube_cy = -3.0, 3.0
    cube_size = 0.5
    # Draw a small cube outline
    corners = [
        (-1, -1),
        (1, -1),
        (1, 1),
        (-1, 1),
        (-1, -1),
    ]
    for i in range(len(corners) - 1):
        x1 = cube_cx + corners[i][0] * cube_size
        y1 = cube_cy + corners[i][1] * cube_size
        x2 = cube_cx + corners[i + 1][0] * cube_size
        y2 = cube_cy + corners[i + 1][1] * cube_size
        ax.plot([x1, x2], [y1, y2], "-", color=STYLE["accent1"], lw=1.5, zorder=5)
    # 3D effect: offset back face
    off = 0.3
    for i in range(len(corners) - 1):
        x1 = cube_cx + corners[i][0] * cube_size + off
        y1 = cube_cy + corners[i][1] * cube_size + off
        x2 = cube_cx + corners[i + 1][0] * cube_size + off
        y2 = cube_cy + corners[i + 1][1] * cube_size + off
        ax.plot(
            [x1, x2], [y1, y2], "-", color=STYLE["accent1"], lw=0.8, alpha=0.4, zorder=4
        )
    # Connect front to back corners
    for cx, cy in [(1, 1), (1, -1), (-1, 1)]:
        ax.plot(
            [cube_cx + cx * cube_size, cube_cx + cx * cube_size + off],
            [cube_cy + cy * cube_size, cube_cy + cy * cube_size + off],
            "-",
            color=STYLE["accent1"],
            lw=0.8,
            alpha=0.4,
            zorder=4,
        )
    ax.text(
        cube_cx,
        cube_cy - cube_size - 0.3,
        "cube map",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    # Arrow from R toward cube map
    R_tip_full = P + R_end
    ax.annotate(
        "",
        xy=(cube_cx + 0.4, cube_cy - cube_size + 0.1),
        xytext=(R_tip_full[0], R_tip_full[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent1"],
            "lw": 1.2,
            "linestyle": "dotted",
            "connectionstyle": "arc3,rad=-0.2",
        },
        zorder=4,
    )
    ax.text(
        -1.3,
        2.8,
        "sample(R)",
        color=STYLE["accent1"],
        fontsize=9,
        style="italic",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Annotations below surface ---
    anno_y = -1.2
    annotations = [
        (STYLE["accent3"], "N = surface normal"),
        (STYLE["accent2"], "V = direction from surface toward camera"),
        (STYLE["text_dim"], "I = \u2212V = incident direction (toward surface)"),
        (STYLE["accent1"], "R = reflect(\u2212V, N) = I \u2212 2(I \u00b7 N)N"),
    ]
    for i, (color, text) in enumerate(annotations):
        ax.text(
            -3.8,
            anno_y - i * 0.42,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- HLSL code snippet ---
    code_y = anno_y - len(annotations) * 0.42 - 0.15
    code_lines = [
        "float3 V = normalize(eye_pos - world_pos);",
        "float3 R = reflect(-V, N);",
        "float3 env = env_tex.Sample(smp, R).rgb;",
        "float3 blended = lerp(diffuse, env, reflectivity);",
    ]
    for i, line in enumerate(code_lines):
        ax.text(
            -3.8,
            code_y - i * 0.38,
            line,
            color=STYLE["warn"],
            fontsize=8,
            family="monospace",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- Layout ---
    ax.set_xlim(-4.2, 4.5)
    ax.set_ylim(code_y - 0.8, 4.2)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Environment Reflection Mapping",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    _save(fig, "gpu/14-environment-mapping", "reflection_mapping.png")


DIAGRAMS = {
    "math/01": [
        ("vector_addition.png", diagram_vector_addition),
        ("dot_product.png", diagram_dot_product),
    ],
    "math/03": [
        ("bilinear_interpolation.png", diagram_bilinear_interpolation),
    ],
    "math/04": [
        ("mip_chain.png", diagram_mip_chain),
        ("trilinear_interpolation.png", diagram_trilinear_interpolation),
    ],
    "math/05": [
        ("matrix_basis_vectors.png", diagram_matrix_basis_vectors),
    ],
    "math/06": [
        ("frustum.png", diagram_frustum),
        ("similar_triangles.png", diagram_similar_triangles),
    ],
    "math/09": [
        ("view_transform.png", diagram_view_transform),
        ("camera_basis_vectors.png", diagram_camera_basis_vectors),
    ],
    "math/10": [
        ("pixel_footprint.png", diagram_pixel_footprint),
    ],
    "gpu/04": [
        ("uv_mapping.png", diagram_uv_mapping),
        ("filtering_comparison.png", diagram_filtering_comparison),
    ],
    "gpu/10": [
        ("blinn_phong_vectors.png", diagram_blinn_phong_vectors),
        ("specular_comparison.png", diagram_specular_comparison),
        ("normal_transformation.png", diagram_normal_transformation),
    ],
    "gpu/11": [
        ("fullscreen_triangle.png", diagram_fullscreen_triangle),
    ],
    "gpu/12": [
        ("undersampling.png", diagram_undersampling),
    ],
    "gpu/14": [
        ("reflection_mapping.png", diagram_reflection_mapping),
    ],
}

# Full lesson directory names for display
LESSON_NAMES = {
    "math/01": "math/01-vectors",
    "math/03": "math/03-bilinear-interpolation",
    "math/04": "math/04-mipmaps-and-lod",
    "math/05": "math/05-matrices",
    "math/06": "math/06-projections",
    "math/09": "math/09-view-matrix",
    "math/10": "math/10-anisotropy",
    "gpu/04": "gpu/04-textures-and-samplers",
    "gpu/10": "gpu/10-basic-lighting",
    "gpu/11": "gpu/11-compute-shaders",
    "gpu/12": "gpu/12-shader-grid",
    "gpu/14": "gpu/14-environment-mapping",
}


def match_lesson(query):
    """Match a query like 'math/01', 'math/01-vectors', or '01' to a registry key."""
    q = query.strip().rstrip("/")

    # Exact match
    if q in DIAGRAMS:
        return q

    # Match by full name
    for key, name in LESSON_NAMES.items():
        if q == name:
            return key

    # Match by number suffix (e.g. "01" matches "math/01")
    for key in DIAGRAMS:
        num = key.split("/")[1]
        if q == num:
            return key

    return None


def main():
    parser = argparse.ArgumentParser(
        description="Generate matplotlib diagrams for forge-gpu lessons."
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--lesson", help="Lesson to generate diagrams for (e.g. math/01)"
    )
    group.add_argument("--all", action="store_true", help="Generate all diagrams")
    group.add_argument("--list", action="store_true", help="List available diagrams")
    args = parser.parse_args()

    if args.list:
        print("Available diagrams:")
        for key in sorted(DIAGRAMS.keys()):
            name = LESSON_NAMES.get(key, key)
            diagrams = DIAGRAMS[key]
            print(f"\n  {name}/")
            for filename, _ in diagrams:
                print(f"    {filename}")
        total = sum(len(d) for d in DIAGRAMS.values())
        print(f"\n{total} diagrams total.")
        return 0

    if args.all:
        keys = sorted(DIAGRAMS.keys())
    else:
        key = match_lesson(args.lesson)
        if key is None:
            print(f"No diagrams registered for '{args.lesson}'.")
            print("Use --list to see available diagrams.")
            return 1
        keys = [key]

    total = 0
    for key in keys:
        name = LESSON_NAMES.get(key, key)
        print(f"{name}/")
        for _, func in DIAGRAMS[key]:
            func()
            total += 1

    print(f"\nGenerated {total} diagram(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
