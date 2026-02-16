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
    "math/05": [
        ("matrix_basis_vectors.png", diagram_matrix_basis_vectors),
    ],
    "math/06": [
        ("frustum.png", diagram_frustum),
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
    "math/05": "math/05-matrices",
    "math/06": "math/06-projections",
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
