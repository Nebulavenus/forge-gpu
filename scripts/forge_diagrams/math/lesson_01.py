"""Diagrams for math/01."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, draw_vector, save, setup_axes

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
