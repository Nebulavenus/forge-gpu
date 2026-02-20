"""Diagram functions for math lessons (lessons/math/)."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

from ._common import STYLE, draw_vector, save, setup_axes

# ---------------------------------------------------------------------------
# math/01-vectors — vector_addition.png
# ---------------------------------------------------------------------------


def diagram_vector_addition():
    """Vector addition with tail-to-head and parallelogram."""
    fig = plt.figure(figsize=(7, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 5), ylim=(-0.5, 4.5))

    a = (3, 1)
    b = (1, 2.5)
    result = (a[0] + b[0], a[1] + b[1])

    # Vectors
    draw_vector(ax, (0, 0), a, STYLE["accent1"], "a = (3, 1)")
    draw_vector(ax, a, b, STYLE["accent2"], "b = (1, 2.5)")
    draw_vector(ax, (0, 0), result, STYLE["accent3"], "a + b = (4, 3.5)")

    # Ghosted b from origin + parallelogram dashes
    draw_vector(ax, (0, 0), b, STYLE["accent2"], lw=1.0)
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
    save(fig, "math/01-vectors", "vector_addition.png")


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
        setup_axes(ax, xlim=(-1.8, 1.8), ylim=(-1.8, 1.8), grid=False)

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
        draw_vector(ax, (0, 0), a, STYLE["accent1"], "a")
        draw_vector(ax, (0, 0), b, STYLE["accent2"], "b")

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
    save(fig, "math/01-vectors", "dot_product.png")


# ---------------------------------------------------------------------------
# math/03-bilinear-interpolation — bilinear_interpolation.png
# ---------------------------------------------------------------------------


def diagram_bilinear_interpolation():
    """Grid cell with the 3 lerp steps highlighted."""
    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.3, 1.5), ylim=(-0.3, 1.5), grid=False)

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
    save(fig, "math/03-bilinear-interpolation", "bilinear_interpolation.png")


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
        setup_axes(ax, xlim=xlim, ylim=ylim)

        draw_vector(ax, (0, 0), x_vec, STYLE["accent1"], x_label, lw=3)
        draw_vector(ax, (0, 0), y_vec, STYLE["accent2"], y_label, lw=3)
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
    save(fig, "math/05-matrices", "matrix_basis_vectors.png")


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
    save(fig, "math/04-mipmaps-and-lod", "mip_chain.png")


# ---------------------------------------------------------------------------
# math/04-mipmaps-and-lod — trilinear_interpolation.png
# ---------------------------------------------------------------------------


def diagram_trilinear_interpolation():
    """Trilinear interpolation: two bilinear samples blended by LOD fraction."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # --- Left panel: mip level N (bilinear) ---
    ax1 = fig.add_subplot(131)
    setup_axes(ax1, xlim=(-0.4, 1.6), ylim=(-0.4, 1.6), grid=False)

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
    setup_axes(ax2, xlim=(-0.4, 1.6), ylim=(-0.4, 1.6), grid=False)

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
        blended_c = c1 * (1 - t) + c2 * t
        r = Rectangle(
            (bar_x, y0),
            bar_w,
            h,
            facecolor=blended_c,
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
    save(fig, "math/04-mipmaps-and-lod", "trilinear_interpolation.png")


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


def diagram_pixel_footprint():
    """Two-panel: Jacobian maps circle to ellipse, isotropic vs anisotropic sampling."""
    fig = plt.figure(figsize=(11, 5.5), facecolor=STYLE["bg"])

    # --- Left panel: pixel footprint at different tilt angles ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-2.5, 3.0), ylim=(-2.8, 2.8), grid=False)

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
    setup_axes(ax2, xlim=(-4.2, 4.2), ylim=(-3.2, 3.2), grid=False)

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
        sy_pos = -sigma_v + 2 * sigma_v * t
        ax2.plot(
            sigma_u * np.cos(theta) * 0.85 + offset_r,
            sigma_u * np.sin(theta) * 0.85 + sy_pos,
            "-",
            color=STYLE["text_dim"],
            lw=0.7,
            alpha=0.4,
        )
        ax2.plot(
            offset_r,
            sy_pos,
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
    save(fig, "math/10-anisotropy", "pixel_footprint.png")
