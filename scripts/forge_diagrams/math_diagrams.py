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


# ---------------------------------------------------------------------------
# math/11-color-spaces — cie_chromaticity.png
# ---------------------------------------------------------------------------


def diagram_cie_chromaticity():
    """CIE 1931 xy chromaticity diagram with sRGB, DCI-P3, Rec.2020 gamuts."""
    fig = plt.figure(figsize=(8, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.02, 0.82), ylim=(-0.02, 0.92), grid=False)

    # CIE 1931 spectral locus (2-degree standard observer, 380-700 nm)
    # These are the (x, y) chromaticity coordinates of monochromatic light
    # at each wavelength. Data from CIE tables.
    spectral_xy = np.array(
        [
            [0.1741, 0.0050],
            [0.1740, 0.0050],
            [0.1738, 0.0049],
            [0.1736, 0.0049],
            [0.1733, 0.0048],
            [0.1730, 0.0048],
            [0.1726, 0.0048],
            [0.1721, 0.0048],
            [0.1714, 0.0051],
            [0.1703, 0.0058],
            [0.1689, 0.0069],
            [0.1669, 0.0086],
            [0.1644, 0.0109],
            [0.1611, 0.0138],
            [0.1566, 0.0177],
            [0.1510, 0.0227],
            [0.1440, 0.0297],
            [0.1355, 0.0399],
            [0.1241, 0.0578],
            [0.1096, 0.0868],
            [0.0913, 0.1327],
            [0.0687, 0.2007],
            [0.0454, 0.2950],
            [0.0235, 0.4127],
            [0.0082, 0.5384],
            [0.0039, 0.6548],
            [0.0139, 0.7502],
            [0.0389, 0.8120],
            [0.0743, 0.8338],
            [0.1142, 0.8262],
            [0.1547, 0.8059],
            [0.1929, 0.7816],
            [0.2296, 0.7543],
            [0.2658, 0.7243],
            [0.3016, 0.6923],
            [0.3373, 0.6589],
            [0.3731, 0.6245],
            [0.4087, 0.5896],
            [0.4441, 0.5547],
            [0.4788, 0.5202],
            [0.5125, 0.4866],
            [0.5448, 0.4544],
            [0.5752, 0.4242],
            [0.6029, 0.3965],
            [0.6270, 0.3725],
            [0.6482, 0.3514],
            [0.6658, 0.3340],
            [0.6801, 0.3197],
            [0.6915, 0.3083],
            [0.7006, 0.2993],
            [0.7079, 0.2920],
            [0.7140, 0.2859],
            [0.7190, 0.2809],
            [0.7230, 0.2770],
            [0.7260, 0.2740],
            [0.7283, 0.2717],
            [0.7300, 0.2700],
            [0.7311, 0.2689],
            [0.7320, 0.2680],
            [0.7327, 0.2673],
            [0.7334, 0.2666],
            [0.7340, 0.2660],
            [0.7344, 0.2656],
            [0.7346, 0.2654],
            [0.7347, 0.2653],
        ]
    )

    # Fill the spectral locus with a faint tinted region
    locus_closed = np.vstack([spectral_xy, spectral_xy[0]])
    locus_poly = Polygon(
        locus_closed,
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor="none",
        alpha=0.4,
    )
    ax.add_patch(locus_poly)

    # Draw the spectral locus outline
    ax.plot(
        spectral_xy[:, 0],
        spectral_xy[:, 1],
        color=STYLE["text"],
        lw=1.5,
        alpha=0.8,
    )
    # Purple line closing the locus
    ax.plot(
        [spectral_xy[0, 0], spectral_xy[-1, 0]],
        [spectral_xy[0, 1], spectral_xy[-1, 1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.0,
        alpha=0.5,
    )

    # Label select wavelengths
    label_nms = [460, 480, 500, 520, 540, 560, 580, 600, 620, 700]
    for nm in label_nms:
        idx = (nm - 380) // 5
        if 0 <= idx < len(spectral_xy):
            x, y = spectral_xy[idx]
            # Offset labels outward
            cx, cy = 0.33, 0.33  # center of diagram
            dx, dy = x - cx, y - cy
            dist = np.sqrt(dx * dx + dy * dy)
            if dist > 0:
                ox = dx / dist * 0.04
                oy = dy / dist * 0.04
            else:
                ox, oy = 0.04, 0.04
            ax.annotate(
                f"{nm}",
                xy=(x, y),
                xytext=(x + ox, y + oy),
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="center",
                arrowprops={"arrowstyle": "-", "color": STYLE["grid"], "lw": 0.5},
            )

    # Gamut triangles
    gamuts = [
        (
            "sRGB",
            [(0.6400, 0.3300), (0.3000, 0.6000), (0.1500, 0.0600)],
            STYLE["accent1"],
            2.0,
            "-",
        ),
        (
            "DCI-P3",
            [(0.6800, 0.3200), (0.2650, 0.6900), (0.1500, 0.0600)],
            STYLE["accent2"],
            1.5,
            "--",
        ),
        (
            "Rec.2020",
            [(0.7080, 0.2920), (0.1700, 0.7970), (0.1310, 0.0460)],
            STYLE["accent3"],
            1.5,
            ":",
        ),
    ]

    for name, pts, color, lw, ls in gamuts:
        tri = Polygon(
            pts,
            closed=True,
            facecolor="none",
            edgecolor=color,
            lw=lw,
            ls=ls,
        )
        ax.add_patch(tri)

        # Label near the centroid
        cx = sum(p[0] for p in pts) / 3
        cy = sum(p[1] for p in pts) / 3
        ax.text(
            cx,
            cy - 0.03,
            name,
            color=color,
            fontsize=10,
            ha="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # D65 white point
    ax.plot(0.3127, 0.3290, "o", color=STYLE["warn"], markersize=8, zorder=5)
    ax.text(
        0.3127 + 0.02,
        0.3290 - 0.03,
        "D65",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "CIE 1931 Chromaticity Diagram",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/11-color-spaces", "cie_chromaticity.png")


# ---------------------------------------------------------------------------
# math/11-color-spaces — gamma_perception.png
# ---------------------------------------------------------------------------


def diagram_gamma_perception():
    """Show how sRGB gamma allocates more precision to dark values."""
    fig, (ax1, ax2) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
    )

    x = np.linspace(0, 1, 500)

    # --- Left panel: transfer functions ---
    setup_axes(ax1, xlim=(-0.02, 1.02), ylim=(-0.02, 1.02), grid=True, aspect=None)

    # sRGB transfer function (linear -> encoded)
    srgb = np.where(
        x <= 0.0031308,
        x * 12.92,
        1.055 * np.power(x, 1 / 2.4) - 0.055,
    )
    ax1.plot(x, srgb, color=STYLE["accent1"], lw=2.5, label="sRGB (piecewise)")
    ax1.plot(
        x,
        np.power(x, 1 / 2.2),
        "--",
        color=STYLE["accent2"],
        lw=1.5,
        alpha=0.7,
        label="Simple pow(x, 1/2.2)",
    )
    ax1.plot(x, x, ":", color=STYLE["text_dim"], lw=1.0, label="Linear (no correction)")

    ax1.set_xlabel("Linear light intensity", color=STYLE["axis"], fontsize=10)
    ax1.set_ylabel(
        "Encoded value (for storage/display)", color=STYLE["axis"], fontsize=10
    )
    ax1.set_title(
        "Gamma Encoding: Linear to sRGB",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax1.legend(
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    # Annotate the midpoint
    mid_srgb = 1.055 * (0.5 ** (1 / 2.4)) - 0.055
    ax1.plot(0.5, mid_srgb, "o", color=STYLE["warn"], markersize=8, zorder=5)
    ax1.annotate(
        f"50% light -> sRGB {mid_srgb:.2f}",
        xy=(0.5, mid_srgb),
        xytext=(0.15, 0.9),
        color=STYLE["warn"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 1.5},
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Right panel: perceptual spacing ---
    setup_axes(ax2, grid=False, aspect=None)

    # Show gradient bars: linear vs sRGB
    n_steps = 32
    linear_levels = np.linspace(0, 1, n_steps)

    # Decode sRGB to show what equally-spaced sRGB values look like in linear
    srgb_levels = np.linspace(0, 1, n_steps)
    srgb_decoded = np.where(
        srgb_levels <= 0.04045,
        srgb_levels / 12.92,
        np.power((srgb_levels + 0.055) / 1.055, 2.4),
    )

    bar_height = 0.3
    for i in range(n_steps):
        # Linear encoding (top bar) — equally-spaced light levels
        ax2.add_patch(
            Rectangle(
                (i / n_steps, 0.6),
                1 / n_steps,
                bar_height,
                facecolor=(linear_levels[i],) * 3,
                edgecolor="none",
            )
        )
        # sRGB encoding (bottom bar) — perceptually-spaced
        ax2.add_patch(
            Rectangle(
                (i / n_steps, 0.1),
                1 / n_steps,
                bar_height,
                facecolor=(srgb_decoded[i],) * 3,
                edgecolor="none",
            )
        )

    ax2.set_xlim(0, 1)
    ax2.set_ylim(0, 1.1)
    ax2.text(
        0.5,
        0.97,
        "Linear encoding (wastes bits on brights)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        0.5,
        0.47,
        "sRGB encoding (more steps in darks)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.set_title(
        "Perceptual Spacing: 32 Equal Steps",
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
        "Gamma Correction: Matching Human Perception",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    save(fig, "math/11-color-spaces", "gamma_perception.png")


# ---------------------------------------------------------------------------
# math/11-color-spaces — tone_mapping_curves.png
# ---------------------------------------------------------------------------


def diagram_tone_mapping_curves():
    """Compare Reinhard, ACES, and linear clamp tone mapping curves."""
    fig = plt.figure(figsize=(8, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.2, 10.2), ylim=(-0.02, 1.15), grid=True, aspect=None)

    x = np.linspace(0, 10, 500)

    # Linear clamp
    linear = np.clip(x, 0, 1)
    ax.plot(x, linear, ":", color=STYLE["text_dim"], lw=1.5, label="Linear clamp")

    # Reinhard: x / (x + 1)
    reinhard = x / (x + 1)
    ax.plot(x, reinhard, color=STYLE["accent2"], lw=2.5, label="Reinhard")

    # ACES (Narkowicz 2015 fit)
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    aces = np.clip((x * (a * x + b)) / (x * (c * x + d) + e), 0, 1)
    ax.plot(x, aces, color=STYLE["accent1"], lw=2.5, label="ACES (Narkowicz)")

    # Mark the 1.0 input line
    ax.axvline(x=1.0, color=STYLE["grid"], lw=0.8, ls="--", alpha=0.5)
    ax.text(
        1.05,
        1.08,
        "SDR range boundary",
        color=STYLE["text_dim"],
        fontsize=8,
        rotation=0,
    )

    # Highlight area
    ax.fill_between(
        x,
        0,
        1,
        where=(x <= 1),  # type: ignore[arg-type]
        alpha=0.06,
        color=STYLE["accent1"],
    )

    ax.set_xlabel("HDR input intensity", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Display output (0-1)", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Tone Mapping: Compressing HDR to Display Range",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.legend(
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        loc="lower right",
    )

    fig.tight_layout()
    save(fig, "math/11-color-spaces", "tone_mapping_curves.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — white_noise_comparison.png
# ---------------------------------------------------------------------------

# Python implementations of the three hash functions from forge_math.h.
# These use np.uint32 arrays for correct 32-bit unsigned overflow.


def _hash_wang(key):
    """Wang hash (Thomas Wang 2007) — vectorised uint32."""
    k = np.asarray(key, dtype=np.uint32)
    k = (k ^ np.uint32(61)) ^ (k >> np.uint32(16))
    k = k * np.uint32(9)
    k = k ^ (k >> np.uint32(4))
    k = k * np.uint32(0x27D4EB2D)
    k = k ^ (k >> np.uint32(15))
    return k


def _hash_pcg(inp):
    """PCG output permutation hash — vectorised uint32."""
    s = np.asarray(inp, dtype=np.uint32)
    s = s * np.uint32(747796405) + np.uint32(2891336453)
    shift = (s >> np.uint32(28)) + np.uint32(4)
    # Element-wise variable shift using NumPy's right_shift ufunc
    word = np.right_shift(s, shift) ^ s
    word = word * np.uint32(277803737)
    return (word >> np.uint32(22)) ^ word


def _hash_xxhash32(h):
    """xxHash32 avalanche finaliser — vectorised uint32."""
    h = np.asarray(h, dtype=np.uint32)
    h = h ^ (h >> np.uint32(15))
    h = h * np.uint32(0x85EBCA77)
    h = h ^ (h >> np.uint32(13))
    h = h * np.uint32(0xC2B2AE3D)
    h = h ^ (h >> np.uint32(16))
    return h


def _hash2d(x, y, hash_fn=_hash_wang):
    """Cascaded 2D hash: hash(x ^ hash(y))."""
    return hash_fn(np.asarray(x, dtype=np.uint32) ^ hash_fn(y))


def _hash_to_float(h):
    """Convert uint32 hash to float in [0, 1)."""
    return (h >> np.uint32(8)).astype(np.float64) / 16777216.0


def diagram_white_noise_comparison():
    """Side-by-side white noise grids for Wang, PCG, and xxHash32."""
    w, h = 256, 256

    # Create 2D coordinate grids
    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.uint32), np.arange(w, dtype=np.uint32), indexing="ij"
    )

    noise_wang = _hash_to_float(_hash2d(xx, yy, _hash_wang))
    noise_pcg = _hash_to_float(_hash2d(xx, yy, _hash_pcg))
    noise_xx = _hash_to_float(_hash2d(xx, yy, _hash_xxhash32))

    fig, axes = plt.subplots(1, 3, figsize=(12, 4.5), facecolor=STYLE["bg"])
    titles = ["Wang Hash", "PCG Hash", "xxHash32"]
    noises = [noise_wang, noise_pcg, noise_xx]

    for ax, title, noise in zip(axes, titles, noises):
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(noise, cmap="gray", vmin=0, vmax=1, interpolation="nearest")
        ax.set_title(title, color=STYLE["text"], fontsize=12, fontweight="bold", pad=8)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "White Noise: hash2d(x, y) for Each Hash Function",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/12-hash-functions", "white_noise_comparison.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — avalanche_matrix.png
# ---------------------------------------------------------------------------


def diagram_avalanche_matrix():
    """32x32 avalanche matrix heatmap: P(output bit j flips | input bit i flips)."""
    n_samples = 4096
    inputs = np.arange(n_samples, dtype=np.uint32)

    # Build 32x32 matrix: for each input bit i, measure flip probability of each
    # output bit j over many sample inputs
    matrix = np.zeros((32, 32), dtype=np.float64)

    base_hashes = _hash_wang(inputs)
    for i in range(32):
        flipped_inputs = inputs ^ np.uint32(1 << i)
        flipped_hashes = _hash_wang(flipped_inputs)
        diff = base_hashes ^ flipped_hashes
        for j in range(32):
            bit_changed = (diff >> np.uint32(j)) & np.uint32(1)
            matrix[i, j] = np.mean(bit_changed.astype(np.float64))

    fig = plt.figure(figsize=(7, 6.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    # Use a diverging colormap centered on 0.5 (the ideal value)
    im = ax.imshow(
        matrix,
        cmap="RdYlGn",
        vmin=0.35,
        vmax=0.65,
        interpolation="nearest",
        aspect="equal",
    )

    ax.set_xlabel("Output bit", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Input bit flipped", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Avalanche Matrix: Wang Hash\nP(output bit j flips | input bit i flips)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax.tick_params(colors=STYLE["axis"], labelsize=7)

    # Tick every 4 bits
    ticks = list(range(0, 32, 4))
    ax.set_xticks(ticks)
    ax.set_yticks(ticks)

    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_label("Flip probability (ideal = 0.50)", color=STYLE["axis"], fontsize=10)
    cbar.ax.tick_params(colors=STYLE["axis"], labelsize=8)
    cbar.outline.set_edgecolor(STYLE["grid"])  # type: ignore[reportCallIssue]

    # Annotate the ideal line
    avg = np.mean(matrix)
    ax.text(
        0.02,
        0.02,
        f"Mean flip probability: {avg:.3f}  (ideal: 0.500)",
        transform=ax.transAxes,
        color=STYLE["text"],
        fontsize=9,
        ha="left",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "math/12-hash-functions", "avalanche_matrix.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — distribution_histogram.png
# ---------------------------------------------------------------------------


def diagram_distribution_histogram():
    """Histogram comparing output distribution of three hash functions."""
    n = 100000
    inputs = np.arange(n, dtype=np.uint32)
    n_bins = 50

    values_wang = _hash_to_float(_hash_wang(inputs))
    values_pcg = _hash_to_float(_hash_pcg(inputs))
    values_xx = _hash_to_float(_hash_xxhash32(inputs))

    fig, axes = plt.subplots(1, 3, figsize=(13, 4), facecolor=STYLE["bg"])
    datasets = [
        ("Wang Hash", values_wang, STYLE["accent1"]),
        ("PCG Hash", values_pcg, STYLE["accent2"]),
        ("xxHash32", values_xx, STYLE["accent3"]),
    ]

    expected = n / n_bins

    for ax, (title, data, color) in zip(axes, datasets):
        ax.set_facecolor(STYLE["bg"])
        counts, edges, _ = ax.hist(
            data, bins=n_bins, range=(0, 1), color=color, alpha=0.75, edgecolor="none"
        )
        ax.axhline(
            y=expected,
            color=STYLE["warn"],
            lw=1.5,
            ls="--",
            label=f"Expected ({int(expected)})",
        )
        ax.set_title(title, color=STYLE["text"], fontsize=12, fontweight="bold", pad=8)
        ax.set_xlabel("Hash output [0, 1)", color=STYLE["axis"], fontsize=9)
        ax.set_ylabel("Count", color=STYLE["axis"], fontsize=9)
        ax.tick_params(colors=STYLE["axis"], labelsize=8)
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

        # Stats
        deviation = np.std(counts - expected)
        ax.text(
            0.98,
            0.95,
            f"Std dev: {deviation:.1f}",
            transform=ax.transAxes,
            color=STYLE["text"],
            fontsize=8,
            ha="right",
            va="top",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )
        ax.legend(
            fontsize=8,
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            labelcolor=STYLE["text"],
            loc="upper left",
        )

    fig.suptitle(
        f"Distribution Uniformity: {n:,} Sequential Inputs",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/12-hash-functions", "distribution_histogram.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — hash_pipeline.png
# ---------------------------------------------------------------------------


def _hash3d(x, y, z):
    """Cascaded 3D hash: hash(x ^ hash(y ^ hash(z)))."""
    return _hash_wang(
        np.asarray(x, dtype=np.uint32)
        ^ _hash_wang(np.asarray(y, dtype=np.uint32) ^ _hash_wang(z))
    )


def _noise_fade(t):
    """Perlin's quintic fade curve."""
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def _noise_grad2d(hash_val, dx, dy):
    """2D gradient dot product (4-gradient set)."""
    h = np.asarray(hash_val, dtype=np.uint32) & np.uint32(3)
    # gx: +1 for cases 0,2; -1 for cases 1,3 (bit 0 controls sign)
    gx = np.where(h & np.uint32(1), -1.0, 1.0)
    # gy: +1 for cases 0,1; -1 for cases 2,3 (bit 1 controls sign)
    gy = np.where(h & np.uint32(2), -1.0, 1.0)
    return gx * dx + gy * dy


def _perlin2d(x, y, seed):
    """2D Perlin gradient noise (vectorised NumPy)."""
    x = np.asarray(x, dtype=np.float64)
    y = np.asarray(y, dtype=np.float64)

    ix = np.floor(x).astype(np.int32)
    iy = np.floor(y).astype(np.int32)
    fx = x - ix.astype(np.float64)
    fy = y - iy.astype(np.float64)

    u = _noise_fade(fx)
    v = _noise_fade(fy)

    uix = ix.astype(np.uint32)
    uiy = iy.astype(np.uint32)
    s = np.uint32(seed)

    h00 = _hash3d(uix, uiy, s)
    h10 = _hash3d(uix + np.uint32(1), uiy, s)
    h01 = _hash3d(uix, uiy + np.uint32(1), s)
    h11 = _hash3d(uix + np.uint32(1), uiy + np.uint32(1), s)

    g00 = _noise_grad2d(h00, fx, fy)
    g10 = _noise_grad2d(h10, fx - 1.0, fy)
    g01 = _noise_grad2d(h01, fx, fy - 1.0)
    g11 = _noise_grad2d(h11, fx - 1.0, fy - 1.0)

    x0 = g00 + u * (g10 - g00)
    x1 = g01 + u * (g11 - g01)
    return x0 + v * (x1 - x0)


def _simplex2d(x, y, seed):
    """2D simplex noise (vectorised NumPy)."""
    x = np.asarray(x, dtype=np.float64)
    y = np.asarray(y, dtype=np.float64)

    F2 = 0.36602540378
    G2 = 0.21132486540

    s = (x + y) * F2
    i = np.floor(x + s).astype(np.int32)
    j = np.floor(y + s).astype(np.int32)

    t = (i + j).astype(np.float64) * G2
    x0 = x - (i.astype(np.float64) - t)
    y0 = y - (j.astype(np.float64) - t)

    i1 = np.where(x0 > y0, 1, 0).astype(np.int32)
    j1 = np.where(x0 > y0, 0, 1).astype(np.int32)

    x1 = x0 - i1.astype(np.float64) + G2
    y1 = y0 - j1.astype(np.float64) + G2
    x2 = x0 - 1.0 + 2.0 * G2
    y2 = y0 - 1.0 + 2.0 * G2

    ui = i.astype(np.uint32)
    uj = j.astype(np.uint32)
    ss = np.uint32(seed)

    h0 = _hash3d(ui, uj, ss)
    h1 = _hash3d(ui + i1.astype(np.uint32), uj + j1.astype(np.uint32), ss)
    h2 = _hash3d(ui + np.uint32(1), uj + np.uint32(1), ss)

    t0 = np.maximum(0.5 - x0 * x0 - y0 * y0, 0.0)
    t1 = np.maximum(0.5 - x1 * x1 - y1 * y1, 0.0)
    t2 = np.maximum(0.5 - x2 * x2 - y2 * y2, 0.0)

    n0 = t0 * t0 * t0 * t0 * _noise_grad2d(h0, x0, y0)
    n1 = t1 * t1 * t1 * t1 * _noise_grad2d(h1, x1, y1)
    n2 = t2 * t2 * t2 * t2 * _noise_grad2d(h2, x2, y2)

    return 70.0 * (n0 + n1 + n2)


def _fbm2d(x, y, seed, octaves, lacunarity=2.0, persistence=0.5):
    """2D fractal Brownian motion (vectorised)."""
    if octaves <= 0:
        return np.zeros_like(x, dtype=np.float64)
    total = np.zeros_like(x, dtype=np.float64)
    amplitude = 1.0
    frequency = 1.0
    max_amp = 0.0
    for i in range(octaves):
        total += amplitude * _perlin2d(x * frequency, y * frequency, seed + i)
        max_amp += amplitude
        frequency *= lacunarity
        amplitude *= persistence
    return total / max_amp


def _domain_warp2d(x, y, seed, strength):
    """2D domain warping via fBm (vectorised)."""
    wx = _fbm2d(x, y, seed, 4)
    wy = _fbm2d(x, y, seed + 1, 4)
    return _fbm2d(x + strength * wx, y + strength * wy, seed + 2, 4)


def diagram_hash_pipeline():
    """Visual flow of the Wang hash pipeline showing each mixing step."""
    fig = plt.figure(figsize=(10, 4), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-1.5, 2.5)
    ax.set_aspect("equal")
    ax.axis("off")

    steps = [
        ("Input\nkey", STYLE["text"]),
        ("XOR\nkey^61\n^(key>>16)", STYLE["accent1"]),
        ("MUL\nkey*9", STYLE["accent2"]),
        ("XOR\nkey^=\nkey>>4", STYLE["accent1"]),
        ("MUL\nkey*=\n0x27d4eb2d", STYLE["accent2"]),
        ("XOR\nkey^=\nkey>>15", STYLE["accent1"]),
        ("Output\nhash", STYLE["accent3"]),
    ]

    box_w = 1.2
    spacing = 1.5
    y = 0.5

    for i, (label, color) in enumerate(steps):
        x = i * spacing
        rect = Rectangle(
            (x - box_w / 2, y - 0.6),
            box_w,
            1.2,
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            x,
            y,
            label,
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=3,
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Arrow to next step
        if i < len(steps) - 1:
            ax.annotate(
                "",
                xy=((i + 1) * spacing - box_w / 2 - 0.05, y),
                xytext=(x + box_w / 2 + 0.05, y),
                arrowprops={
                    "arrowstyle": "->,head_width=0.2,head_length=0.1",
                    "color": STYLE["text_dim"],
                    "lw": 1.5,
                },
                zorder=1,
            )

    # Purpose labels below
    purposes = [
        "",
        "fold upper\ninto lower",
        "spread via\ncarry chain",
        "fold multiply\nresult",
        "full-width\nbit spread",
        "final\navalanche",
        "",
    ]
    for i, purpose in enumerate(purposes):
        if purpose:
            ax.text(
                i * spacing,
                y - 1.1,
                purpose,
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="top",
            )

    ax.set_title(
        "Wang Hash Pipeline: How Each Step Mixes Bits",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=15,
    )

    fig.tight_layout()
    save(fig, "math/12-hash-functions", "hash_pipeline.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — gradient_noise_concept.png
# ---------------------------------------------------------------------------


def diagram_gradient_noise_concept():
    """Core concept: grid with gradient arrows, sample point, dot products."""
    fig = plt.figure(figsize=(9, 9), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.8, 3.8), ylim=(-0.8, 3.8))

    # Draw the integer grid
    for i in range(4):
        ax.axhline(i, color=STYLE["grid"], lw=0.6, alpha=0.5)
        ax.axvline(i, color=STYLE["grid"], lw=0.6, alpha=0.5)

    # Define gradient directions at each grid point (seeded deterministically)
    rng = np.random.default_rng(42)
    gradients_4 = [(1, 1), (-1, 1), (1, -1), (-1, -1)]
    grid_grads = {}
    for gx in range(4):
        for gy in range(4):
            idx = rng.integers(0, 4)
            grid_grads[(gx, gy)] = gradients_4[idx]

    # Draw gradient arrows at all grid points (dimmed)
    arrow_len = 0.3
    for (gx, gy), (dx, dy) in grid_grads.items():
        mag = np.sqrt(dx * dx + dy * dy)
        ndx, ndy = dx / mag * arrow_len, dy / mag * arrow_len
        ax.annotate(
            "",
            xy=(gx + ndx, gy + ndy),
            xytext=(gx, gy),
            arrowprops={
                "arrowstyle": "->,head_width=0.12,head_length=0.06",
                "color": STYLE["text_dim"],
                "lw": 1.5,
            },
        )
        ax.plot(gx, gy, "o", color=STYLE["text_dim"], markersize=5, zorder=4)

    # Highlight the cell containing the sample point
    sample_x, sample_y = 1.65, 1.35
    cell_x, cell_y = 1, 1

    # Highlight cell
    cell_rect = Rectangle(
        (cell_x, cell_y),
        1,
        1,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        alpha=0.6,
        zorder=1,
    )
    ax.add_patch(cell_rect)

    # Draw highlighted gradient arrows at the 4 corners
    corners = [(0, 0), (1, 0), (0, 1), (1, 1)]
    corner_colors = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent4"],
        STYLE["accent3"],
    ]
    corner_labels = ["g00", "g10", "g01", "g11"]

    for (cx, cy), color, label in zip(corners, corner_colors, corner_labels):
        gx_abs = cell_x + cx
        gy_abs = cell_y + cy
        dx, dy = grid_grads[(gx_abs, gy_abs)]
        mag = np.sqrt(dx * dx + dy * dy)
        ndx, ndy = dx / mag * 0.4, dy / mag * 0.4

        ax.annotate(
            "",
            xy=(gx_abs + ndx, gy_abs + ndy),
            xytext=(gx_abs, gy_abs),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.08",
                "color": color,
                "lw": 2.5,
            },
            zorder=6,
        )
        ax.plot(gx_abs, gy_abs, "o", color=color, markersize=8, zorder=7)

        # Label
        ox = -0.25 if cx == 0 else 0.12
        oy = -0.18 if cy == 0 else 0.12
        ax.text(
            gx_abs + ox,
            gy_abs + oy,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Sample point
    ax.plot(
        sample_x,
        sample_y,
        "*",
        color=STYLE["warn"],
        markersize=18,
        zorder=8,
        markeredgecolor="white",
        markeredgewidth=0.5,
    )
    ax.text(
        sample_x + 0.12,
        sample_y + 0.15,
        f"P ({sample_x:.2f}, {sample_y:.2f})",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Distance vectors from each corner to sample point (dashed)
    for (cx, cy), color in zip(corners, corner_colors):
        gx_abs = cell_x + cx
        gy_abs = cell_y + cy
        ax.plot(
            [gx_abs, sample_x],
            [gy_abs, sample_y],
            "--",
            color=color,
            lw=1.0,
            alpha=0.6,
            zorder=3,
        )

    # Annotation: explain the process
    ax.text(
        3.7,
        -0.5,
        "1. Hash each corner to get gradient\n"
        "2. Dot(gradient, distance-to-P)\n"
        "3. Interpolate with fade curve",
        color=STYLE["text"],
        fontsize=9,
        ha="right",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Gradient Noise: Lattice Gradients + Dot Products + Interpolation",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "gradient_noise_concept.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — fade_curves.png
# ---------------------------------------------------------------------------


def diagram_fade_curves():
    """Compare linear, smoothstep, and Perlin's quintic fade curves."""
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.05, 1.05), ylim=(-0.05, 1.15), grid=True, aspect=None)

    t = np.linspace(0, 1, 500)

    # Linear
    ax.plot(t, t, ":", color=STYLE["text_dim"], lw=1.5, label="Linear: t")

    # Smoothstep (Hermite): 3t^2 - 2t^3
    smoothstep = 3 * t**2 - 2 * t**3
    ax.plot(
        t,
        smoothstep,
        "--",
        color=STYLE["accent2"],
        lw=2.0,
        label="Smoothstep: $3t^2 - 2t^3$ (C1)",
    )

    # Quintic (Perlin improved): 6t^5 - 15t^4 + 10t^3
    quintic = 6 * t**5 - 15 * t**4 + 10 * t**3
    ax.plot(
        t,
        quintic,
        "-",
        color=STYLE["accent1"],
        lw=2.5,
        label="Quintic: $6t^5 - 15t^4 + 10t^3$ (C2)",
    )

    # Mark the endpoints
    for color in [STYLE["text_dim"], STYLE["accent2"], STYLE["accent1"]]:
        ax.plot(0, 0, "o", color=color, markersize=5, zorder=5)
        ax.plot(1, 1, "o", color=color, markersize=5, zorder=5)

    # Annotate derivatives
    ax.annotate(
        "Zero 1st & 2nd derivative\nat both endpoints",
        xy=(0.08, quintic[40]),
        xytext=(0.25, 0.15),
        color=STYLE["accent1"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 1.2},
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel(
        "t (fractional position within grid cell)", color=STYLE["axis"], fontsize=10
    )
    ax.set_ylabel("fade(t) (interpolation weight)", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Fade Curves: Why Perlin's Quintic Eliminates Grid Artifacts",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax.legend(
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        loc="upper left",
    )

    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "fade_curves.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — perlin_vs_simplex_grid.png
# ---------------------------------------------------------------------------


def diagram_perlin_vs_simplex_grid():
    """Side-by-side: square grid (Perlin) vs triangular grid (simplex)."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5.5), facecolor=STYLE["bg"])

    # --- Left panel: Square grid (Perlin) ---
    setup_axes(ax1, xlim=(-0.5, 4.5), ylim=(-0.5, 4.5), grid=False)

    # Draw square grid
    for i in range(5):
        ax1.plot([-0.3, 4.3], [i, i], color=STYLE["grid"], lw=0.8, alpha=0.6)
        ax1.plot([i, i], [-0.3, 4.3], color=STYLE["grid"], lw=0.8, alpha=0.6)

    # Grid points
    for x in range(5):
        for y in range(5):
            ax1.plot(x, y, "o", color=STYLE["text_dim"], markersize=5, zorder=4)

    # Highlight one cell and its 4 corners
    cell_rect = Rectangle(
        (1, 1),
        1,
        1,
        facecolor=STYLE["accent1"],
        alpha=0.15,
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        zorder=2,
    )
    ax1.add_patch(cell_rect)

    sample = (1.6, 1.4)
    ax1.plot(*sample, "*", color=STYLE["warn"], markersize=14, zorder=8)

    for cx, cy in [(1, 1), (2, 1), (1, 2), (2, 2)]:
        ax1.plot(cx, cy, "o", color=STYLE["accent1"], markersize=8, zorder=6)
        ax1.plot(
            [cx, sample[0]],
            [cy, sample[1]],
            "--",
            color=STYLE["accent1"],
            lw=1,
            alpha=0.5,
            zorder=3,
        )

    ax1.text(
        2.0,
        -0.3,
        "4 corners per sample\n(bilinear interpolation)",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax1.set_title(
        "Square Grid (Perlin)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )
    ax1.set_xticks([])
    ax1.set_yticks([])

    # --- Right panel: Simplex (triangular) grid ---
    setup_axes(ax2, xlim=(-0.5, 4.5), ylim=(-0.5, 4.5), grid=False)

    # Draw triangular grid (equilateral triangles via skewing)
    G2 = (3 - np.sqrt(3)) / 6

    # Generate grid points in simplex space and unskew to display
    simplex_pts = []
    for si in range(-1, 7):
        for sj in range(-1, 7):
            # Unskew from simplex to Cartesian
            t = (si + sj) * G2
            px = si - t
            py = sj - t
            if -0.3 <= px <= 4.3 and -0.3 <= py <= 4.3:
                simplex_pts.append((px, py, si, sj))

    # Draw edges
    for px, py, si, sj in simplex_pts:
        for dsi, dsj in [(1, 0), (0, 1), (1, -1)]:
            ni, nj = si + dsi, sj + dsj
            nt = (ni + nj) * G2
            nx, ny = ni - nt, nj - nt
            if -0.3 <= nx <= 4.3 and -0.3 <= ny <= 4.3:
                ax2.plot(
                    [px, nx],
                    [py, ny],
                    color=STYLE["grid"],
                    lw=0.8,
                    alpha=0.6,
                )

    # Grid points
    for px, py, _, _ in simplex_pts:
        ax2.plot(px, py, "o", color=STYLE["text_dim"], markersize=5, zorder=4)

    # Highlight a triangle and its 3 corners
    # Pick a specific triangle
    tri_i, tri_j = 2, 2
    t_val = (tri_i + tri_j) * G2
    p0 = (tri_i - t_val, tri_j - t_val)

    t_val1 = (tri_i + 1 + tri_j) * G2
    p1 = (tri_i + 1 - t_val1, tri_j - t_val1)

    t_val2 = (tri_i + 1 + tri_j + 1) * G2
    p2 = (tri_i + 1 - t_val2, tri_j + 1 - t_val2)

    tri = Polygon(
        [p0, p1, p2],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.15,
        edgecolor=STYLE["accent3"],
        linewidth=2.5,
        zorder=2,
    )
    ax2.add_patch(tri)

    sample_s = ((p0[0] + p1[0] + p2[0]) / 3, (p0[1] + p1[1] + p2[1]) / 3)
    ax2.plot(*sample_s, "*", color=STYLE["warn"], markersize=14, zorder=8)

    for px, py in [p0, p1, p2]:
        ax2.plot(px, py, "o", color=STYLE["accent3"], markersize=8, zorder=6)
        ax2.plot(
            [px, sample_s[0]],
            [py, sample_s[1]],
            "--",
            color=STYLE["accent3"],
            lw=1,
            alpha=0.5,
            zorder=3,
        )

    ax2.text(
        2.0,
        -0.3,
        "3 corners per sample\n(better scaling to higher dims)",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.set_title(
        "Simplex Grid (Triangular)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])

    fig.suptitle(
        "Perlin vs Simplex: Grid Structure",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "math/13-gradient-noise", "perlin_vs_simplex_grid.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — noise_comparison.png
# ---------------------------------------------------------------------------


def diagram_noise_comparison():
    """Side-by-side 2D noise renders: white, Perlin, simplex."""
    w, h = 256, 256
    scale = 0.03

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    # White noise
    white = (
        _hash_to_float(_hash2d(xx.astype(np.uint32), yy.astype(np.uint32))) * 2.0 - 1.0
    )

    # Perlin noise
    perlin = _perlin2d(xx * scale, yy * scale, 42)

    # Simplex noise
    simplex = _simplex2d(xx * scale, yy * scale, 42)

    fig, axes = plt.subplots(1, 3, figsize=(14, 5), facecolor=STYLE["bg"])
    titles = [
        "White Noise (uncorrelated)",
        "Perlin Noise (square grid)",
        "Simplex Noise (triangular grid)",
    ]
    noises = [white, perlin * 2.5, simplex * 1.8]
    cmaps = ["gray", "gray", "gray"]

    for ax, title, noise, cmap in zip(axes, titles, noises, cmaps):
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            noise,
            cmap=cmap,
            vmin=-1,
            vmax=1,
            interpolation="nearest",
            aspect="equal",
        )
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold", pad=8)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Noise Types: From Random Static to Smooth Coherent Patterns",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "noise_comparison.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — fbm_octaves.png
# ---------------------------------------------------------------------------


def diagram_fbm_octaves():
    """fBm with 1, 2, 4, and 8 octaves showing progressive detail."""
    w, h = 256, 256
    scale = 0.02

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    octave_counts = [1, 2, 4, 8]

    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5), facecolor=STYLE["bg"])

    for ax, octaves in zip(axes, octave_counts):
        noise = _fbm2d(xx * scale, yy * scale, 42, octaves)
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            noise,
            cmap="gray",
            vmin=-0.8,
            vmax=0.8,
            interpolation="bilinear",
            aspect="equal",
        )
        ax.set_title(
            f"{octaves} octave{'s' if octaves > 1 else ''}",
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=8,
        )
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "fBm (Fractal Brownian Motion): Adding Octaves Builds Detail",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "fbm_octaves.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — lacunarity_persistence.png
# ---------------------------------------------------------------------------


def diagram_lacunarity_persistence():
    """Grid showing fBm with varying lacunarity and persistence."""
    w, h = 192, 192
    scale = 0.02
    octaves = 6

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    lacunarities = [1.5, 2.0, 3.0]
    persistences = [0.3, 0.5, 0.7]

    fig, axes = plt.subplots(
        3,
        3,
        figsize=(12, 12),
        facecolor=STYLE["bg"],
    )

    for row, persistence in enumerate(persistences):
        for col, lacunarity in enumerate(lacunarities):
            ax = axes[row][col]
            noise = _fbm2d(xx * scale, yy * scale, 42, octaves, lacunarity, persistence)
            ax.set_facecolor(STYLE["bg"])
            ax.imshow(
                noise,
                cmap="gray",
                vmin=-0.8,
                vmax=0.8,
                interpolation="bilinear",
                aspect="equal",
            )
            ax.set_xticks([])
            ax.set_yticks([])
            for spine in ax.spines.values():
                spine.set_color(STYLE["grid"])
                spine.set_linewidth(0.5)

            if row == 0:
                ax.set_title(
                    f"lacunarity = {lacunarity}",
                    color=STYLE["accent1"],
                    fontsize=11,
                    fontweight="bold",
                    pad=8,
                )
            if col == 0:
                ax.set_ylabel(
                    f"persistence = {persistence}",
                    color=STYLE["accent2"],
                    fontsize=11,
                    fontweight="bold",
                    labelpad=8,
                )

    fig.suptitle(
        "fBm Parameters: Lacunarity (columns) vs Persistence (rows)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.01,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "lacunarity_persistence.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — domain_warping.png
# ---------------------------------------------------------------------------


def diagram_domain_warping():
    """Side-by-side: plain fBm vs domain-warped noise at various strengths."""
    w, h = 256, 256
    scale = 0.02

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    strengths = [0.0, 1.5, 3.0, 5.0]
    labels = [
        "No warping (strength=0)",
        "Mild (strength=1.5)",
        "Moderate (strength=3.0)",
        "Strong (strength=5.0)",
    ]

    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5), facecolor=STYLE["bg"])

    for ax, strength, label in zip(axes, strengths, labels):
        if strength == 0.0:
            noise = _fbm2d(xx * scale, yy * scale, 42, 4)
        else:
            noise = _domain_warp2d(xx * scale, yy * scale, 42, strength)

        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            noise,
            cmap="gray",
            vmin=-0.8,
            vmax=0.8,
            interpolation="bilinear",
            aspect="equal",
        )
        ax.set_title(label, color=STYLE["text"], fontsize=10, fontweight="bold", pad=8)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Domain Warping: Distorting Coordinates for Organic Patterns",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "domain_warping.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — sampling_comparison.png
# ---------------------------------------------------------------------------


def _halton(index, base):
    """Radical inverse in the given base."""
    result = 0.0
    fraction = 1.0 / base
    i = index
    while i > 0:
        result += (i % base) * fraction
        i //= base
        fraction /= base
    return result


def _r2(index):
    """R2 quasi-random sequence point."""
    alpha1 = 0.7548776662466927
    alpha2 = 0.5698402909980532
    x = (0.5 + index * alpha1) % 1.0
    y = (0.5 + index * alpha2) % 1.0
    return x, y


def _wang_hash(key):
    """Thomas Wang integer hash (Python version for diagram generation)."""
    key = key & 0xFFFFFFFF
    key = (~key + (key << 21)) & 0xFFFFFFFF
    key = key ^ (key >> 24)
    key = ((key + (key << 3)) + (key << 8)) & 0xFFFFFFFF
    key = key ^ (key >> 14)
    key = ((key + (key << 2)) + (key << 4)) & 0xFFFFFFFF
    key = key ^ (key >> 28)
    key = (key + (key << 31)) & 0xFFFFFFFF
    return key


def _hash_to_float_low24(h):
    """Map low 24 bits of a scalar hash to [0, 1)."""
    return (h & 0x00FFFFFF) / 16777216.0


def _blue_noise_2d(count, candidates, seed):
    """Mitchell's best candidate blue noise."""
    xs = [_hash_to_float_low24(_wang_hash(seed))]
    ys = [_hash_to_float_low24(_wang_hash(seed ^ 0x9E3779B9))]

    for i in range(1, count):
        best_x, best_y, best_dist = 0, 0, -1
        for c in range(candidates):
            h1 = _wang_hash((seed + (i * candidates + c) * 2654435761) & 0xFFFFFFFF)
            h2 = _wang_hash(h1)
            cx, cy = _hash_to_float_low24(h1), _hash_to_float_low24(h2)

            min_dist = 1e30
            for j in range(len(xs)):
                dx = cx - xs[j]
                dy = cy - ys[j]
                if dx > 0.5:
                    dx -= 1.0
                if dx < -0.5:
                    dx += 1.0
                if dy > 0.5:
                    dy -= 1.0
                if dy < -0.5:
                    dy += 1.0
                d2 = dx * dx + dy * dy
                if d2 < min_dist:
                    min_dist = d2

            if min_dist > best_dist:
                best_dist = min_dist
                best_x, best_y = cx, cy

        xs.append(best_x)
        ys.append(best_y)

    return np.array(xs), np.array(ys)


def diagram_sampling_comparison():
    """Four-panel scatter plot: white noise, Halton, R2, blue noise."""
    n = 256

    # White noise
    rng = np.random.default_rng(42)
    wx = rng.random(n)
    wy = rng.random(n)

    # Halton (base 2, 3)
    hx = np.array([_halton(i + 1, 2) for i in range(n)])
    hy = np.array([_halton(i + 1, 3) for i in range(n)])

    # R2
    r2x = np.array([_r2(i)[0] for i in range(n)])
    r2y = np.array([_r2(i)[1] for i in range(n)])

    # Blue noise (fewer points for speed)
    bn_count = 256
    bnx, bny = _blue_noise_2d(bn_count, 20, 42)

    datasets = [
        ("White Noise", wx, wy, STYLE["accent2"]),
        ("Halton (2, 3)", hx, hy, STYLE["accent1"]),
        ("R2 Sequence", r2x, r2y, STYLE["accent3"]),
        ("Blue Noise", bnx, bny, STYLE["accent4"]),
    ]

    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5), facecolor=STYLE["bg"])

    for ax, (title, xs, ys, color) in zip(axes, datasets):
        ax.set_facecolor(STYLE["bg"])
        ax.scatter(xs, ys, s=6, c=color, alpha=0.8, edgecolors="none")
        ax.set_xlim(0, 1)
        ax.set_ylim(0, 1)
        ax.set_aspect("equal")
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold", pad=8)
        ax.tick_params(colors=STYLE["axis"], labelsize=7)
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)
        ax.grid(True, color=STYLE["grid"], linewidth=0.3, alpha=0.4)

    fig.suptitle(
        "Sampling Distributions: 256 Points in [0, 1)²",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "sampling_comparison.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — dithering_comparison.png
# ---------------------------------------------------------------------------


def diagram_dithering_comparison():
    """Gradient banding vs white noise vs R1 vs blue noise dithering."""
    width = 512
    levels = 8  # Quantize to this many levels for visible banding

    # Smooth gradient [0, 1]
    gradient = np.linspace(0, 1, width).reshape(1, -1)
    gradient = np.repeat(gradient, 64, axis=0)  # Make it 64 pixels tall

    # Quantize without dithering
    banded = np.floor(gradient * levels) / levels

    # White noise dithering
    rng = np.random.default_rng(42)
    white = rng.random(gradient.shape)
    dithered_white = np.floor((gradient + (white - 0.5) / levels) * levels) / levels
    dithered_white = np.clip(dithered_white, 0, 1)

    # R1 (golden ratio) dithering — use a different offset per row too
    inv_phi = 0.6180339887498949
    r1_vals = np.zeros_like(gradient)
    for row in range(r1_vals.shape[0]):
        for col in range(r1_vals.shape[1]):
            idx = row * r1_vals.shape[1] + col
            r1_vals[row, col] = (0.5 + idx * inv_phi) % 1.0
    dithered_r1 = np.floor((gradient + (r1_vals - 0.5) / levels) * levels) / levels
    dithered_r1 = np.clip(dithered_r1, 0, 1)

    # Blue noise dithering — build a 64x64 tile via 2D Mitchell's best candidate,
    # then tile across the gradient dimensions.
    # Use _blue_noise_2d to generate 2D sample positions with toroidal distance,
    # then build a rank map: each cell gets a threshold from insertion order.
    tile_size = 64
    tile_count = tile_size * tile_size
    bnx, bny = _blue_noise_2d(tile_count, 10, 7)
    # Build rank map: map each 2D position to a grid cell, assign rank by
    # insertion order (earlier samples get lower ranks → lower thresholds)
    bn_tile = np.zeros((tile_size, tile_size))
    occupied = np.full((tile_size, tile_size), -1, dtype=int)
    for rank in range(tile_count):
        col = int(bnx[rank] * tile_size) % tile_size
        row = int(bny[rank] * tile_size) % tile_size
        # Handle collisions: find nearest unoccupied cell
        if occupied[row, col] >= 0:
            found = False
            for radius in range(1, tile_size):
                for dr in range(-radius, radius + 1):
                    for dc in range(-radius, radius + 1):
                        if abs(dr) != radius and abs(dc) != radius:
                            continue
                        nr = (row + dr) % tile_size
                        nc = (col + dc) % tile_size
                        if occupied[nr, nc] < 0:
                            row, col = nr, nc
                            found = True
                            break
                    if found:
                        break
                if found:
                    break
        occupied[row, col] = rank
        bn_tile[row, col] = rank / tile_count  # Normalize to [0, 1)
    rows, cols = gradient.shape
    blue_noise = np.tile(bn_tile, (rows // tile_size + 1, cols // tile_size + 1))[
        :rows, :cols
    ]
    dithered_blue = np.floor((gradient + (blue_noise - 0.5) / levels) * levels) / levels
    dithered_blue = np.clip(dithered_blue, 0, 1)

    panels = [
        ("Original gradient", gradient),
        ("Quantized (banding)", banded),
        ("White noise dithered", dithered_white),
        ("R1 (golden ratio) dithered", dithered_r1),
        ("Blue noise dithered", dithered_blue),
    ]

    fig, axes = plt.subplots(5, 1, figsize=(12, 6.25), facecolor=STYLE["bg"])

    for ax, (title, data) in zip(axes, panels):
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            data, cmap="gray", vmin=0, vmax=1, aspect="auto", interpolation="nearest"
        )
        ax.set_ylabel(
            title,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            rotation=0,
            ha="right",
            va="center",
            labelpad=10,
        )
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Dithering: Replacing Banding with Imperceptible Noise",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "dithering_comparison.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — power_spectrum.png
# ---------------------------------------------------------------------------


def diagram_power_spectrum():
    """2D power spectrum comparison: white noise vs blue noise."""
    size = 128

    # White noise
    rng = np.random.default_rng(42)
    white = rng.random((size, size))

    # Blue noise approximation via scattered points on a grid
    blue = np.zeros((size, size))
    n_points = size * size // 4
    bnx, bny = _blue_noise_2d(min(n_points, 400), 25, 42)
    for px, py in zip(bnx, bny):
        ix = int(px * (size - 1))
        iy = int(py * (size - 1))
        if 0 <= ix < size and 0 <= iy < size:
            blue[iy, ix] = 1.0

    # Compute 2D FFT magnitude (power spectrum)
    white_fft = np.abs(np.fft.fftshift(np.fft.fft2(white - white.mean()))) ** 2
    blue_fft = np.abs(np.fft.fftshift(np.fft.fft2(blue - blue.mean()))) ** 2

    # Normalize for display
    white_fft = np.log1p(white_fft)
    blue_fft = np.log1p(blue_fft)
    white_fft /= white_fft.max() if white_fft.max() > 0 else 1
    blue_fft /= blue_fft.max() if blue_fft.max() > 0 else 1

    # Radial average for 1D profile
    center = size // 2
    freqs_w = np.zeros(center)
    freqs_b = np.zeros(center)
    counts = np.zeros(center)

    for y in range(size):
        for x in range(size):
            r = int(np.sqrt((x - center) ** 2 + (y - center) ** 2))
            if r < center:
                freqs_w[r] += white_fft[y, x]
                freqs_b[r] += blue_fft[y, x]
                counts[r] += 1

    mask = counts > 0
    freqs_w[mask] /= counts[mask]
    freqs_b[mask] /= counts[mask]

    fig = plt.figure(figsize=(14, 5), facecolor=STYLE["bg"])

    # White noise spectrum
    ax1 = fig.add_subplot(131)
    ax1.set_facecolor(STYLE["bg"])
    ax1.imshow(white_fft, cmap="inferno", interpolation="bilinear")
    ax1.set_title(
        "White Noise Spectrum",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    ax1.set_xticks([])
    ax1.set_yticks([])
    for spine in ax1.spines.values():
        spine.set_color(STYLE["grid"])

    # Blue noise spectrum
    ax2 = fig.add_subplot(132)
    ax2.set_facecolor(STYLE["bg"])
    ax2.imshow(blue_fft, cmap="inferno", interpolation="bilinear")
    ax2.set_title(
        "Blue Noise Spectrum",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])
    for spine in ax2.spines.values():
        spine.set_color(STYLE["grid"])

    # Radial profile
    ax3 = fig.add_subplot(133)
    ax3.set_facecolor(STYLE["bg"])
    ax3.plot(
        freqs_w, color=STYLE["accent2"], linewidth=2, label="White noise", alpha=0.8
    )
    ax3.plot(
        freqs_b, color=STYLE["accent1"], linewidth=2, label="Blue noise", alpha=0.8
    )
    ax3.set_xlabel("Frequency", color=STYLE["axis"], fontsize=10)
    ax3.set_ylabel("Power", color=STYLE["axis"], fontsize=10)
    ax3.set_title(
        "Radial Power Profile",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    ax3.tick_params(colors=STYLE["axis"], labelsize=8)
    ax3.legend(
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=9,
    )
    for spine in ax3.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax3.grid(True, color=STYLE["grid"], linewidth=0.3, alpha=0.4)

    fig.suptitle(
        "Power Spectrum: White Noise vs Blue Noise",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "power_spectrum.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — discrepancy_convergence.png
# ---------------------------------------------------------------------------


def diagram_discrepancy_convergence():
    """Log-log plot of star discrepancy vs sample count."""
    sample_counts = [8, 16, 32, 64, 128, 256]

    d_random = []
    d_halton = []
    d_r2 = []

    rng = np.random.default_rng(42)

    for n in sample_counts:
        # Random
        rx = rng.random(n)
        ry = rng.random(n)
        d_random.append(_star_discrepancy(rx, ry))

        # Halton
        hx = np.array([_halton(i + 1, 2) for i in range(n)])
        hy = np.array([_halton(i + 1, 3) for i in range(n)])
        d_halton.append(_star_discrepancy(hx, hy))

        # R2
        pts = [_r2(i) for i in range(n)]
        r2x = np.array([p[0] for p in pts])
        r2y = np.array([p[1] for p in pts])
        d_r2.append(_star_discrepancy(r2x, r2y))

    fig, ax = plt.subplots(figsize=(8, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])

    ax.loglog(
        sample_counts,
        d_random,
        "o-",
        color=STYLE["accent2"],
        linewidth=2,
        markersize=6,
        label="White noise",
    )
    ax.loglog(
        sample_counts,
        d_halton,
        "s-",
        color=STYLE["accent1"],
        linewidth=2,
        markersize=6,
        label="Halton (2, 3)",
    )
    ax.loglog(
        sample_counts,
        d_r2,
        "D-",
        color=STYLE["accent3"],
        linewidth=2,
        markersize=6,
        label="R2",
    )

    # Reference lines
    ns = np.array(sample_counts, dtype=float)
    ax.loglog(
        ns,
        0.8 / np.sqrt(ns),
        "--",
        color=STYLE["text_dim"],
        linewidth=1,
        alpha=0.5,
        label=r"$O(1/\sqrt{N})$ (random)",
    )
    ax.loglog(
        ns,
        2.0 / ns,
        "--",
        color=STYLE["warn"],
        linewidth=1,
        alpha=0.5,
        label=r"$O(1/N)$ (optimal)",
    )

    ax.set_xlabel("Number of samples (N)", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Star discrepancy D*", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Discrepancy vs Sample Count",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.tick_params(colors=STYLE["axis"], labelsize=9)
    ax.legend(
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=9,
    )
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.3, alpha=0.4, which="both")

    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "discrepancy_convergence.png")


def _star_discrepancy(xs, ys):
    """Brute-force star discrepancy (Python version for diagrams)."""
    n = len(xs)
    max_disc = 0.0
    # Test rectangles [0,u]×[0,v] at each sample point (inclusive comparisons)
    for i in range(n):
        u, v = xs[i], ys[i]
        inside = np.sum((xs <= u) & (ys <= v))
        disc = abs(inside / n - u * v)
        if disc > max_disc:
            max_disc = disc
    # Test boundary rectangles with u=1 (full width)
    for i in range(n):
        inside = np.sum(ys <= ys[i])
        disc = abs(inside / n - ys[i])
        if disc > max_disc:
            max_disc = disc
    # Test boundary rectangles with v=1 (full height)
    for i in range(n):
        inside = np.sum(xs <= xs[i])
        disc = abs(inside / n - xs[i])
        if disc > max_disc:
            max_disc = disc
    # Test corner rectangle [0,1]×[0,1] — should be 0 but include for completeness
    disc = abs(1.0 - 1.0)  # all n points inside, area = 1
    if disc > max_disc:
        max_disc = disc
    return max_disc


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — radical_inverse.png
# ---------------------------------------------------------------------------


def diagram_radical_inverse():
    """Visualization of the radical inverse filling [0,1) progressively."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 4), facecolor=STYLE["bg"])

    counts = [4, 8, 16]
    for ax, n in zip(axes, counts):
        ax.set_facecolor(STYLE["bg"])

        # Plot the Halton base-2 points on a number line
        vals = [_halton(i + 1, 2) for i in range(n)]

        # Show number line
        ax.axhline(0, color=STYLE["grid"], linewidth=1, alpha=0.6)

        # Plot points with labels for small n
        for i, v in enumerate(vals):
            ax.plot(v, 0, "o", color=STYLE["accent1"], markersize=8, zorder=5)
            if n <= 8:
                ax.annotate(
                    f"{i + 1}",
                    xy=(v, 0),
                    xytext=(0, 12),
                    textcoords="offset points",
                    ha="center",
                    color=STYLE["text"],
                    fontsize=8,
                    fontweight="bold",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )

        ax.set_xlim(-0.05, 1.05)
        ax.set_ylim(-0.3, 0.5)
        ax.set_title(
            f"n = {n} points",
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            pad=8,
        )
        ax.set_xlabel("[0, 1)", color=STYLE["axis"], fontsize=10)
        ax.tick_params(colors=STYLE["axis"], labelsize=8, left=False, labelleft=False)
        for spine in ["left", "top", "right"]:
            ax.spines[spine].set_visible(False)
        ax.spines["bottom"].set_color(STYLE["grid"])
        ax.spines["bottom"].set_linewidth(0.5)

    fig.suptitle(
        "Radical Inverse (Base 2): Progressive Gap-Filling",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "radical_inverse.png")


# ---------------------------------------------------------------------------
# math/02-coordinate-spaces — 3-D coordinate space visualizations
# ---------------------------------------------------------------------------
#
# A simple 3-D house (box body + triangular-prism roof) is transformed
# through the six-stage rendering pipeline.  Each diagram renders the
# house with flat-shaded faces so learners can follow it from local
# space all the way to screen pixels.  World-space diagrams include a
# yard (ground plane) and road for spatial context.

_COORD_LESSON = "math/02-coordinate-spaces"

# ── 3-D house geometry (local space) ──────────────────────────────────────
#
# Y is up.  The house body is a 1×0.7×0.6 box sitting on the XZ ground
# plane (y = 0).  The roof is a triangular prism peaking at y = 1.0.
#
#   Vertices (x, y, z):
#
#        8─────9          roof peak   (y = 1.0)
#       /|\   /|\
#      / | \ / | \
#     2──+──3  |  |       wall top    (y = 0.7)
#     |  4──|──5  |       (back wall top, same y)
#     | /   | /  /
#     |/    |/  /
#     0─────1  /          ground      (y = 0)
#      \       /
#       6─────7           back ground (y = 0)
#
# Faces reference these indices.

_HOUSE_VERTS = np.array(
    [
        [-0.5, 0.0, -0.3],  # 0  front-left-bottom
        [0.5, 0.0, -0.3],  # 1  front-right-bottom
        [-0.5, 0.7, -0.3],  # 2  front-left-top
        [0.5, 0.7, -0.3],  # 3  front-right-top
        [-0.5, 0.7, 0.3],  # 4  back-left-top
        [0.5, 0.7, 0.3],  # 5  back-right-top
        [-0.5, 0.0, 0.3],  # 6  back-left-bottom
        [0.5, 0.0, 0.3],  # 7  back-right-bottom
        [0.0, 1.0, -0.3],  # 8  front-roof-peak
        [0.0, 1.0, 0.3],  # 9  back-roof-peak
    ],
    dtype=float,
)

# Each face is a list of vertex indices forming a polygon.
# Winding is outward-facing for shading orientation.
_HOUSE_FACES = [
    # Body walls
    [0, 1, 3, 2],  # front
    [6, 4, 5, 7],  # back
    [0, 6, 7, 1],  # bottom
    [0, 2, 4, 6],  # left
    [1, 7, 5, 3],  # right
    # Roof
    [2, 3, 8],  # front gable
    [4, 9, 5],  # back gable
    [2, 8, 9, 4],  # left slope
    [3, 5, 9, 8],  # right slope
]

# Flat-shade colours — index matches _HOUSE_FACES.
# Front wall is accent1, sides darker, roof accent3.
_HOUSE_COLORS = [
    "#5dc8f0",  # front wall  (bright cyan)
    "#3a8daa",  # back wall   (darker)
    "#2a6a80",  # bottom      (darkest, rarely seen)
    "#3596b8",  # left wall
    "#3596b8",  # right wall
    "#78d080",  # front gable (green)
    "#4ea85a",  # back gable
    "#5cb868",  # left slope
    "#5cb868",  # right slope
]


def _house_polys(verts):
    """Return a list of Nx3 arrays — one polygon per house face."""
    return [verts[idx] for idx in _HOUSE_FACES]


# ── Transformation helpers ────────────────────────────────────────────────


def _mat4_rot_y(deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    m = np.eye(4)
    m[0, 0], m[0, 2] = c, s
    m[2, 0], m[2, 2] = -s, c
    return m


def _mat4_translate(tx, ty, tz):
    m = np.eye(4)
    m[0, 3], m[1, 3], m[2, 3] = tx, ty, tz
    return m


def _mat4_scale(s):
    return np.diag([s, s, s, 1.0])


def _mat4_look_at(eye, target, up):
    f = target - eye
    f = f / np.linalg.norm(f)
    r = np.cross(f, up)
    r = r / np.linalg.norm(r)
    u = np.cross(r, f)
    m = np.eye(4)
    m[0, :3], m[1, :3], m[2, :3] = r, u, -f
    m[0, 3] = -r.dot(eye)
    m[1, 3] = -u.dot(eye)
    m[2, 3] = f.dot(eye)
    return m


def _mat4_perspective(fov_deg, aspect, near, far):
    fv = 1.0 / np.tan(np.radians(fov_deg) / 2.0)
    m = np.zeros((4, 4))
    m[0, 0] = fv / aspect
    m[1, 1] = fv
    m[2, 2] = far / (near - far)
    m[2, 3] = (near * far) / (near - far)
    m[3, 2] = -1.0
    return m


def _xf3(verts3, mat4):
    """Transform Nx3 vertices by a 4x4 matrix, return Nx3."""
    n = verts3.shape[0]
    h = np.hstack([verts3, np.ones((n, 1))])
    out = (mat4 @ h.T).T
    return out[:, :3]


def _xf4(verts3, mat4):
    """Transform Nx3 vertices by a 4x4 matrix, return Nx4 (keep w)."""
    n = verts3.shape[0]
    h = np.hstack([verts3, np.ones((n, 1))])
    return (mat4 @ h.T).T


# ── Shared pipeline parameters ────────────────────────────────────────────

_MODEL_MAT = _mat4_translate(3.0, 0.0, -2.0) @ _mat4_rot_y(35.0) @ _mat4_scale(2.0)
_CAM_EYE = np.array([8.0, 5.0, 10.0])
_CAM_TARGET = np.array([3.0, 1.0, -2.0])
_CAM_UP = np.array([0.0, 1.0, 0.0])
_VIEW_MAT = _mat4_look_at(_CAM_EYE, _CAM_TARGET, _CAM_UP)
_PROJ_MAT = _mat4_perspective(60.0, 16.0 / 9.0, 0.1, 100.0)
_SCR_W, _SCR_H = 1920.0, 1080.0


def _coord_pipeline():
    """Push house vertices through the full transform pipeline.

    Returns (world_verts, view_verts, clip4, ndc_verts, screen_x, screen_y).
    All arrays use GPU Y-up convention (apply _gpu_to_mpl before plotting).
    """
    world_verts = _xf3(_HOUSE_VERTS, _MODEL_MAT)
    view_verts = _xf3(world_verts, _VIEW_MAT)
    clip4 = _xf4(view_verts, _PROJ_MAT)
    ndc_verts = clip4[:, :3] / clip4[:, 3:4]
    screen_x = (ndc_verts[:, 0] + 1.0) * 0.5 * _SCR_W
    screen_y = (1.0 - ndc_verts[:, 1]) * 0.5 * _SCR_H
    return world_verts, view_verts, clip4, ndc_verts, screen_x, screen_y


def _scenery_world_verts():
    """Return (ground, road) quad arrays in GPU world-space coordinates.

    Matches the yard and road drawn in the world-space / view-space diagrams
    so the same scenery appears consistently across all pipeline stages.
    """
    world_verts, _, _, _, _, _ = _coord_pipeline()
    hc = world_verts.mean(axis=0)

    gnd_size = 7.0
    ghs = gnd_size / 2.0
    ground = np.array(
        [
            [hc[0] - ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] + ghs],
            [hc[0] - ghs, 0.0, hc[2] + ghs],
        ]
    )

    road_x0, road_x1 = -2.0, 10.0
    road_z = hc[2] + 5.0
    road_hw = 1.5 / 2.0
    road = np.array(
        [
            [road_x0, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z + road_hw],
            [road_x0, 0.0, road_z + road_hw],
        ]
    )
    return ground, road


def _scenery_through_pipeline():
    """Push ground/road through view → clip → NDC → screen.

    Returns a dict with keys 'ground' and 'road', each containing
    sub-dicts with 'clip4', 'ndc', 'screen_x', 'screen_y' arrays.
    """
    ground_w, road_w = _scenery_world_verts()

    result = {}
    for name, world_quad in [("ground", ground_w), ("road", road_w)]:
        view = _xf3(world_quad, _VIEW_MAT)
        clip4 = _xf4(view, _PROJ_MAT)
        ndc = clip4[:, :3] / clip4[:, 3:4]
        sx = (ndc[:, 0] + 1.0) * 0.5 * _SCR_W
        sy = (1.0 - ndc[:, 1]) * 0.5 * _SCR_H
        result[name] = {"clip4": clip4, "ndc": ndc, "screen_x": sx, "screen_y": sy}
    return result


def _add_scenery_2d(ax, ground_xy, road_xy):
    """Draw ground and road as 2-D polygons on *ax* (for clip / NDC / screen).

    *ground_xy* and *road_xy* are Nx2 arrays of (x, y) for each quad.
    """
    ax.add_patch(
        Polygon(
            ground_xy,
            closed=True,
            facecolor=STYLE["accent3"],
            edgecolor=STYLE["grid"],
            linewidth=0.5,
            alpha=0.30,
            zorder=2,
        )
    )
    ax.add_patch(
        Polygon(
            road_xy,
            closed=True,
            facecolor=STYLE["grid"],
            edgecolor=STYLE["text_dim"],
            linewidth=0.5,
            alpha=0.35,
            zorder=2,
        )
    )


# ── Coordinate-system adapter ─────────────────────────────────────────────
#
# GPU convention: Y-up (x right, y up, z toward viewer).
# Matplotlib Axes3D: Z-up (x right, y depth, z up).
# _gpu_to_mpl swaps the Y and Z columns so our Y-up data renders upright.


def _gpu_to_mpl(verts):
    """Swap Y↔Z so GPU Y-up data plots correctly in matplotlib Z-up axes."""
    return verts[:, [0, 2, 1]]


# ── 3-D axis styling helper ───────────────────────────────────────────────


def _style_3d(ax, title, xlabel="x", ylabel="z", zlabel="y (up)"):
    """Apply the forge dark theme to a 3-D axes."""
    ax.set_facecolor(STYLE["bg"])
    ax.xaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax.yaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax.zaxis.pane.set_facecolor(STYLE["surface"])
    ax.xaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.yaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.zaxis.pane.set_edgecolor(STYLE["grid"])
    ax.tick_params(colors=STYLE["axis"], labelsize=7)
    ax.set_xlabel(xlabel, color=STYLE["axis"], fontsize=9, labelpad=2)
    ax.set_ylabel(ylabel, color=STYLE["axis"], fontsize=9, labelpad=2)
    ax.set_zlabel(zlabel, color=STYLE["axis"], fontsize=9, labelpad=2)
    ax.set_title(
        title,
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )


def _add_house_3d(ax, verts):
    """Add the shaded 3-D house to *ax* using Poly3DCollection."""
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    polys = _house_polys(verts)
    col = Poly3DCollection(
        polys,
        facecolors=_HOUSE_COLORS,
        edgecolors=STYLE["text_dim"],
        linewidths=0.5,
        alpha=0.85,
    )
    ax.add_collection3d(col)


def _add_ground_plane(ax, center_xz, size, color=STYLE["accent3"]):
    """Draw a flat ground-plane quad at mpl-z = 0 (GPU y = 0)."""
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    cx, cz = center_xz
    hs = size / 2.0
    # mpl axes: (x, y=depth, z=up).  Ground is flat at z = 0.
    quad = [
        [cx - hs, cz - hs, 0],
        [cx + hs, cz - hs, 0],
        [cx + hs, cz + hs, 0],
        [cx - hs, cz + hs, 0],
    ]
    col = Poly3DCollection(
        [quad],
        facecolors=[color],
        edgecolors=[STYLE["grid"]],
        alpha=0.40,
    )
    ax.add_collection3d(col)


def _add_road(ax, x_range, z_center, width):
    """Draw a road strip at mpl-z = 0.001 (sits just above the ground)."""
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    x0, x1 = x_range
    hw = width / 2.0
    mpl_z = 0.001  # tiny offset above ground
    # mpl axes: (x, y=depth, z=up)
    quad = [
        [x0, z_center - hw, mpl_z],
        [x1, z_center - hw, mpl_z],
        [x1, z_center + hw, mpl_z],
        [x0, z_center + hw, mpl_z],
    ]
    col = Poly3DCollection(
        [quad],
        facecolors=[STYLE["grid"]],
        edgecolors=[STYLE["text_dim"]],
        linewidths=0.5,
        alpha=0.45,
    )
    ax.add_collection3d(col)
    # Centre dashes
    n_dash = 6
    xs = np.linspace(x0 + 0.3, x1 - 0.3, n_dash)
    for xd in xs:
        ax.plot(
            [xd, xd + 0.25],
            [z_center] * 2,
            [mpl_z + 0.002] * 2,
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.7,
        )


def _set_equal_3d(ax, verts, pad=0.5):
    """Force equal aspect on a 3-D axes based on vertex extents."""
    mins = verts.min(axis=0)
    maxs = verts.max(axis=0)
    center = (mins + maxs) / 2.0
    half = (maxs - mins).max() / 2.0 + pad
    ax.set_xlim(center[0] - half, center[0] + half)
    ax.set_ylim(center[1] - half, center[1] + half)
    ax.set_zlim(center[2] - half, center[2] + half)


# ── Diagram 1: Local Space ────────────────────────────────────────────────


def diagram_coord_local_space():
    """3-D house centred at the origin in its own local coordinate system."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    fig = plt.figure(figsize=(7, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_subplot(111, projection="3d")  # type: ignore[assignment]

    mpl_verts = _gpu_to_mpl(_HOUSE_VERTS)
    _add_house_3d(ax, mpl_verts)

    # Origin marker (mpl coords: x, z, y)
    ax.scatter(
        [0],
        [0],
        [0],  # type: ignore[arg-type]
        color=STYLE["warn"],
        s=60,
        marker="+",
        linewidths=2,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        0.15,
        -0.15,
        -0.15,
        "origin",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    _set_equal_3d(ax, mpl_verts, pad=0.6)
    ax.view_init(elev=25, azim=-45)
    _style_3d(ax, "1. Local Space")

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_local_space.png")


# ── Diagram 2: World Space ────────────────────────────────────────────────


def _add_camera_icon(ax, eye_mpl, target_mpl, size=0.6):
    """Draw a small wireframe camera pyramid in mpl coordinates."""
    fwd = target_mpl - eye_mpl
    fwd = fwd / np.linalg.norm(fwd)
    # Pick a temporary up that isn't parallel to fwd
    tmp_up = np.array([0.0, 0.0, 1.0])
    if abs(np.dot(fwd, tmp_up)) > 0.99:
        tmp_up = np.array([0.0, 1.0, 0.0])
    right = np.cross(fwd, tmp_up)
    right = right / np.linalg.norm(right) * size * 0.5
    up = np.cross(right, fwd)
    up = up / np.linalg.norm(up) * size * 0.5
    tip = eye_mpl
    base_center = eye_mpl + fwd * size
    corners = [
        base_center + right + up,
        base_center - right + up,
        base_center - right - up,
        base_center + right - up,
    ]
    for c in corners:
        ax.plot(
            [tip[0], c[0]],
            [tip[1], c[1]],
            [tip[2], c[2]],
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.9,
        )
    # Base rectangle
    for i in range(4):
        j = (i + 1) % 4
        ax.plot(
            [corners[i][0], corners[j][0]],
            [corners[i][1], corners[j][1]],
            [corners[i][2], corners[j][2]],
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.9,
        )


def diagram_coord_world_space():
    """House placed in the world scene with yard and road for context."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_subplot(111, projection="3d")  # type: ignore[assignment]

    world_verts, _, _, _, _, _ = _coord_pipeline()
    mpl_verts = _gpu_to_mpl(world_verts)
    _add_house_3d(ax, mpl_verts)

    # Yard (green ground around the house)
    hc = world_verts.mean(axis=0)  # GPU coords for centre
    _add_ground_plane(ax, (hc[0], hc[2]), 7.0)

    # Road running along X in front of the house
    _add_road(ax, (-2, 10), hc[2] + 5.0, 1.5)

    # World origin (mpl: x, z, y)
    ax.scatter(
        [0],
        [0],
        [0],  # type: ignore[arg-type]
        color=STYLE["warn"],
        s=60,
        marker="+",
        linewidths=2,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        0.3,
        0.6,
        0.0,
        "world origin",
        color=STYLE["warn"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Camera icon at _CAM_EYE, pointing at _CAM_TARGET
    eye_mpl = _gpu_to_mpl(_CAM_EYE.reshape(1, 3)).flatten()
    tgt_mpl = _gpu_to_mpl(_CAM_TARGET.reshape(1, 3)).flatten()
    _add_camera_icon(ax, eye_mpl, tgt_mpl, size=1.0)
    ax.text(
        eye_mpl[0] + 0.4,
        eye_mpl[1] + 0.4,
        eye_mpl[2] + 0.4,
        "camera",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Include camera position in the bounds calculation
    all_pts = np.vstack([mpl_verts, eye_mpl.reshape(1, 3)])
    _set_equal_3d(ax, all_pts, pad=3.0)
    ax.view_init(elev=45, azim=-50)
    _style_3d(ax, "2. World Space")

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_world_space.png")


# ── Diagram 3: View / Camera Space ───────────────────────────────────────


def diagram_coord_view_space():
    """Whole scene re-expressed relative to the camera at the origin."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_subplot(111, projection="3d")  # type: ignore[assignment]

    # --- Transform house into view space ---
    world_verts, view_verts, _, _, _, _ = _coord_pipeline()
    mpl_house = _gpu_to_mpl(view_verts)
    _add_house_3d(ax, mpl_house)

    # --- Reconstruct the same ground plane used in the world-space diagram,
    #     then transform it into view space ---
    hc = world_verts.mean(axis=0)  # house centre in GPU world coords
    gnd_size = 7.0
    ghs = gnd_size / 2.0
    # Ground quad corners in GPU world space (y = 0 is the ground)
    ground_world = np.array(
        [
            [hc[0] - ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] + ghs],
            [hc[0] - ghs, 0.0, hc[2] + ghs],
        ]
    )
    ground_view = _xf3(ground_world, _VIEW_MAT)
    ground_mpl = _gpu_to_mpl(ground_view)
    col_gnd = Poly3DCollection(
        [ground_mpl.tolist()],
        facecolors=[STYLE["accent3"]],
        edgecolors=[STYLE["grid"]],
        alpha=0.40,
    )
    ax.add_collection3d(col_gnd)

    # --- Road (same as world-space diagram, transformed to view space) ---
    road_x0, road_x1 = -2.0, 10.0
    road_z = hc[2] + 5.0  # GPU z centre of road
    road_hw = 1.5 / 2.0
    road_world = np.array(
        [
            [road_x0, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z + road_hw],
            [road_x0, 0.0, road_z + road_hw],
        ]
    )
    road_view = _xf3(road_world, _VIEW_MAT)
    road_mpl = _gpu_to_mpl(road_view)
    col_road = Poly3DCollection(
        [road_mpl.tolist()],
        facecolors=[STYLE["grid"]],
        edgecolors=[STYLE["text_dim"]],
        linewidths=0.5,
        alpha=0.45,
    )
    ax.add_collection3d(col_road)

    # Road centre dashes
    n_dash = 6
    dash_xs_world = np.linspace(road_x0 + 0.3, road_x1 - 0.3, n_dash)
    for xd in dash_xs_world:
        seg_world = np.array([[xd, 0.001, road_z], [xd + 0.25, 0.001, road_z]])
        seg_view = _xf3(seg_world, _VIEW_MAT)
        seg_mpl = _gpu_to_mpl(seg_view)
        ax.plot(
            seg_mpl[:, 0],
            seg_mpl[:, 1],
            seg_mpl[:, 2],
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.7,
        )

    # --- World origin transformed into view space ---
    origin_view = _xf3(np.array([[0.0, 0.0, 0.0]]), _VIEW_MAT)
    origin_mpl = _gpu_to_mpl(origin_view).flatten()
    ax.scatter(
        [origin_mpl[0]],
        [origin_mpl[1]],
        [origin_mpl[2]],  # type: ignore[arg-type]
        color=STYLE["accent2"],
        s=60,
        marker="+",
        linewidths=2,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        origin_mpl[0] + 0.3,
        origin_mpl[1] + 0.6,
        origin_mpl[2],
        "world origin",
        color=STYLE["accent2"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Camera at origin (this IS view space, so camera sits at [0,0,0]) ---
    ax.scatter(
        [0],
        [0],
        [0],  # type: ignore[arg-type]
        color=STYLE["warn"],
        s=80,
        marker="^",
        linewidths=1.5,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        0.3,
        0.0,
        0.3,
        "camera\n(origin)",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Frame the whole scene (house + ground + road + camera + origin) ---
    all_pts = np.vstack(
        [mpl_house, ground_mpl, road_mpl, origin_mpl.reshape(1, 3), [[0, 0, 0]]]
    )
    _set_equal_3d(ax, all_pts, pad=3.0)
    # Position the viewpoint roughly behind and above the camera origin,
    # looking toward the scene (camera looks down -Z → mpl -Y).
    ax.view_init(elev=25, azim=80)
    _style_3d(
        ax,
        "3. View / Camera Space",
        xlabel="x (right)",
        ylabel="z (depth)",
        zlabel="y (up)",
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_view_space.png")


# ── Diagram 4: Clip Space ────────────────────────────────────────────────


def diagram_coord_clip_space():
    """House shown from the camera's perspective in clip-space coordinates.

    Uses a 2D view (clip x vs clip y) with scenery.  The w annotation
    highlights that the homogeneous w component is no longer 1 after
    projection — the key property that distinguishes clip space from
    the earlier spaces.
    """
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    _, _, clip4, _, _, _ = _coord_pipeline()
    clip_w = clip4[:, 3]

    # --- Scenery (ground + road) projected into clip space ---
    scenery = _scenery_through_pipeline()
    ground_clip4 = scenery["ground"]["clip4"]
    road_clip4 = scenery["road"]["clip4"]
    _add_scenery_2d(
        ax,
        np.column_stack([ground_clip4[:, 0], ground_clip4[:, 1]]),
        np.column_stack([road_clip4[:, 0], road_clip4[:, 1]]),
    )

    # --- House faces (painter's algorithm using clip z / w for depth) ---
    clip_depth = clip4[:, 2] / clip_w
    face_depths = []
    for face_idx, idx_list in enumerate(_HOUSE_FACES):
        avg_z = np.mean(clip_depth[idx_list])
        face_depths.append((avg_z, face_idx, idx_list))
    face_depths.sort(key=lambda t: t[0], reverse=True)

    for draw_order, (_, face_idx, idx_list) in enumerate(face_depths):
        fx = clip4[idx_list, 0]
        fy = clip4[idx_list, 1]
        poly = np.column_stack([fx, fy])
        ax.add_patch(
            Polygon(
                poly,
                closed=True,
                facecolor=_HOUSE_COLORS[face_idx],
                edgecolor=STYLE["text_dim"],
                linewidth=0.6,
                alpha=0.85,
                zorder=3 + draw_order,
            )
        )

    # Annotate w at the roof peak (highest clip y)
    peak_idx = int(np.argmax(clip4[:, 1]))
    w_val = clip_w[peak_idx]
    ax.annotate(
        f"w = {w_val:.1f}",
        xy=(clip4[peak_idx, 0], clip4[peak_idx, 1]),
        xytext=(clip4[peak_idx, 0] + 1.2, clip4[peak_idx, 1] + 1.0),
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 1.2},
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Note about clip-space boundaries
    ax.text(
        0.02,
        0.02,
        "visible when  |x|, |y| \u2264 w",
        color=STYLE["text_dim"],
        fontsize=8,
        transform=ax.transAxes,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Auto-scale with some padding around the house
    all_x = np.concatenate([clip4[:, 0], ground_clip4[:, 0]])
    all_y = np.concatenate([clip4[:, 1], ground_clip4[:, 1]])
    pad = 2.0
    ax.set_xlim(all_x.min() - pad, all_x.max() + pad)
    ax.set_ylim(all_y.min() - pad, all_y.max() + pad)
    ax.set_aspect("equal")
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.3)
    ax.set_xlabel("clip x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("clip y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "4. Clip Space",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_clip_space.png")


# ── Diagram 5: NDC ───────────────────────────────────────────────────────


def diagram_coord_ndc():
    """House shown from the camera's perspective in NDC x-y space.

    Uses a 2D front-facing view so learners see what the camera sees,
    with coordinates normalised to [-1, 1].  The NDC boundary rectangle
    makes the visible region obvious and bridges naturally into the
    screen-space diagram that follows.
    """
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    _, _, _, ndc_verts, _, _ = _coord_pipeline()

    # NDC boundary rectangle: x ∈ [-1, 1], y ∈ [-1, 1]
    ax.add_patch(
        Rectangle(
            (-1, -1),
            2,
            2,
            linewidth=2,
            edgecolor=STYLE["warn"],
            facecolor=STYLE["surface"],
            alpha=0.25,
            zorder=1,
        )
    )

    # --- Scenery (ground + road) projected into NDC ---
    scenery = _scenery_through_pipeline()
    ground_ndc = scenery["ground"]["ndc"]
    road_ndc = scenery["road"]["ndc"]
    _add_scenery_2d(
        ax,
        np.column_stack([ground_ndc[:, 0], ground_ndc[:, 1]]),
        np.column_stack([road_ndc[:, 0], road_ndc[:, 1]]),
    )

    # Depth for painter's algorithm (NDC z — higher = farther)
    ndc_z = ndc_verts[:, 2]

    # Sort faces back-to-front so nearer faces draw on top
    face_depths = []
    for face_idx, idx_list in enumerate(_HOUSE_FACES):
        avg_z = np.mean(ndc_z[idx_list])
        face_depths.append((avg_z, face_idx, idx_list))
    face_depths.sort(key=lambda t: t[0], reverse=True)  # farthest first

    for draw_order, (_, face_idx, idx_list) in enumerate(face_depths):
        fx = ndc_verts[idx_list, 0]
        fy = ndc_verts[idx_list, 1]
        poly = np.column_stack([fx, fy])
        ax.add_patch(
            Polygon(
                poly,
                closed=True,
                facecolor=_HOUSE_COLORS[face_idx],
                edgecolor=STYLE["text_dim"],
                linewidth=0.6,
                alpha=0.85,
                zorder=3 + draw_order,
            )
        )

    # Boundary label
    ax.text(
        0.0,
        -1.0 - 0.08,
        "visible region  [-1, 1] \u00d7 [-1, 1]",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Z-range annotation
    ax.text(
        1.0,
        1.0 + 0.06,
        "z \u2208 [0, 1]  (depth)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="right",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlim(-1.35, 1.35)
    ax.set_ylim(-1.35, 1.35)
    ax.set_aspect("equal")
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.3)
    ax.set_xlabel("ndc x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("ndc y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "5. Normalized Device Coordinates",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_ndc.png")


# ── Diagram 6: Screen Space ──────────────────────────────────────────────


def diagram_coord_screen_space():
    """House mapped to final pixel coordinates on a 1920 × 1080 screen."""
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    _, _, clip4, ndc_verts, screen_x, screen_y = _coord_pipeline()

    # Per-vertex depth in view space (GPU z) for painter's algorithm
    screen_z = ndc_verts[:, 2]

    # Screen boundary (drawn before scenery so it sits behind everything)
    ax.add_patch(
        Rectangle(
            (0, 0),
            _SCR_W,
            _SCR_H,
            linewidth=2,
            edgecolor=STYLE["text_dim"],
            facecolor=STYLE["surface"],
            alpha=0.25,
            zorder=1,
        )
    )

    # --- Scenery (ground + road) projected into screen space ---
    scenery = _scenery_through_pipeline()
    _add_scenery_2d(
        ax,
        np.column_stack([scenery["ground"]["screen_x"], scenery["ground"]["screen_y"]]),
        np.column_stack([scenery["road"]["screen_x"], scenery["road"]["screen_y"]]),
    )

    # Sort faces back-to-front (painter's algorithm) so nearer faces
    # draw on top of farther ones.
    face_depths = []
    for face_idx, idx_list in enumerate(_HOUSE_FACES):
        avg_z = np.mean(screen_z[idx_list])
        face_depths.append((avg_z, face_idx, idx_list))
    face_depths.sort(key=lambda t: t[0], reverse=True)  # farthest first

    for draw_order, (_, face_idx, idx_list) in enumerate(face_depths):
        fx = screen_x[idx_list]
        fy = screen_y[idx_list]
        poly = np.column_stack([fx, fy])
        ax.add_patch(
            Polygon(
                poly,
                closed=True,
                facecolor=_HOUSE_COLORS[face_idx],
                edgecolor=STYLE["text_dim"],
                linewidth=0.6,
                alpha=0.85,
                zorder=3 + draw_order,
            )
        )

    ax.text(
        _SCR_W / 2,
        _SCR_H + 40,
        "1920 \u00d7 1080",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlim(-80, 2000)
    ax.set_ylim(_SCR_H + 80, -60)
    ax.set_aspect("equal")
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.3)
    ax.set_xlabel("pixel x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("pixel y  (0 = top)", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "6. Screen Space",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_screen_space.png")
