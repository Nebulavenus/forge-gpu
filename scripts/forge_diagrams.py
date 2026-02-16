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
    fig.tight_layout(rect=[0, 0, 1, 0.95])
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
    fig.tight_layout(rect=[0, 0, 1, 0.95])
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
    fig.tight_layout(rect=[0, 0, 1, 0.95])
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
    fig.tight_layout(rect=[0, 0, 1, 0.95])
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
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    _save(fig, "math/09-view-matrix", "camera_basis_vectors.png")


# ---------------------------------------------------------------------------
# Diagram registry
# ---------------------------------------------------------------------------

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
    ],
    "math/09": [
        ("view_transform.png", diagram_view_transform),
        ("camera_basis_vectors.png", diagram_camera_basis_vectors),
    ],
    "gpu/04": [
        ("uv_mapping.png", diagram_uv_mapping),
        ("filtering_comparison.png", diagram_filtering_comparison),
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
    "gpu/04": "gpu/04-textures-and-samplers",
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
