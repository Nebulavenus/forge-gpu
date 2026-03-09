"""Diagrams for gpu/03."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/03-uniforms-and-motion — unit_circle.png
# ---------------------------------------------------------------------------


def diagram_unit_circle():
    """Unit circle showing cos(t) and sin(t) as coordinates of a point.

    Shows the unit circle centered at the origin with a point at angle t,
    projections onto the x and y axes demonstrating cos and sin, and a
    dashed radius line from origin to the point.
    """
    fig = plt.figure(figsize=(7, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.65, 1.65), ylim=(-1.65, 1.65))

    # --- Unit circle ---
    theta = np.linspace(0, 2 * np.pi, 200)
    ax.plot(np.cos(theta), np.sin(theta), "-", color=STYLE["grid"], lw=2, zorder=2)

    # --- Axes through origin ---
    ax.axhline(0, color=STYLE["axis"], lw=0.8, zorder=1)
    ax.axvline(0, color=STYLE["axis"], lw=0.8, zorder=1)

    # --- Axis labels ---
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    ax.text(
        1.5,
        -0.15,
        "x",
        color=STYLE["axis"],
        fontsize=11,
        ha="center",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        -0.15,
        1.5,
        "y",
        color=STYLE["axis"],
        fontsize=11,
        ha="right",
        va="center",
        path_effects=stroke,
    )

    # --- Cardinal labels on the circle ---
    cardinal_style = dict(
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(1.0, -0.18, "(1, 0)", **cardinal_style)  # type: ignore[arg-type]
    ax.text(-1.0, -0.18, "(\u22121, 0)", **cardinal_style)  # type: ignore[arg-type]
    ax.text(0.22, 1.12, "(0, 1)", **cardinal_style)  # type: ignore[arg-type]
    ax.text(0.22, -1.12, "(0, \u22121)", **cardinal_style)  # type: ignore[arg-type]

    # --- Point on circle at angle t ---
    t = np.radians(40)  # 40° for a clear visual
    px, py = np.cos(t), np.sin(t)

    # Radius line from origin to point
    ax.plot([0, px], [0, py], "-", color=STYLE["accent1"], lw=2.5, zorder=4)
    ax.plot(px, py, "o", color=STYLE["accent1"], markersize=10, zorder=6)

    # Point label
    ax.text(
        px + 0.12,
        py + 0.12,
        "(cos t, sin t)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- Projection lines (dashed) ---
    # Vertical drop from point to x-axis (shows cos t)
    ax.plot(
        [px, px],
        [0, py],
        "--",
        color=STYLE["accent2"],
        lw=1.5,
        alpha=0.8,
        zorder=3,
    )
    # Horizontal line from point to y-axis (shows sin t)
    ax.plot(
        [0, px],
        [py, py],
        "--",
        color=STYLE["accent3"],
        lw=1.5,
        alpha=0.8,
        zorder=3,
    )

    # --- cos(t) label on x-axis ---
    ax.annotate(
        "",
        xy=(px, -0.08),
        xytext=(0, -0.08),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent2"],
            "lw": 2,
        },
        zorder=5,
    )
    ax.text(
        px / 2,
        -0.22,
        "cos t",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- sin(t) label on y-axis ---
    ax.annotate(
        "",
        xy=(-0.08, py),
        xytext=(-0.08, 0),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent3"],
            "lw": 2,
        },
        zorder=5,
    )
    ax.text(
        -0.25,
        py / 2,
        "sin t",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- Angle arc from +x axis to the radius ---
    arc_r = 0.3
    arc_t = np.linspace(0, t, 40)
    ax.plot(
        arc_r * np.cos(arc_t),
        arc_r * np.sin(arc_t),
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=5,
    )
    # Angle label
    arc_mid = t / 2
    ax.text(
        0.45 * np.cos(arc_mid),
        0.45 * np.sin(arc_mid),
        "t",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- Hide ticks, keep minimal appearance ---
    ax.set_xticks([-1, 0, 1])
    ax.set_yticks([-1, 0, 1])
    ax.tick_params(labelsize=9, colors=STYLE["axis"])

    ax.set_title(
        "The Unit Circle: cos and sin",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/03-uniforms-and-motion", "unit_circle.png")


# ---------------------------------------------------------------------------
# gpu/03-uniforms-and-motion — aspect_ratio.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/03-uniforms-and-motion — aspect_ratio.png
# ---------------------------------------------------------------------------


def diagram_aspect_ratio():
    """Side-by-side comparison of NDC square vs. stretched window.

    Left panel shows a circle in the square NDC space (-1 to +1 on both axes).
    Right panel shows how the same NDC circle appears stretched into an ellipse
    on a 16:9 window.  Demonstrates why aspect ratio correction is needed.
    """
    fig = plt.figure(figsize=(11, 5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    theta = np.linspace(0, 2 * np.pi, 100)
    r = 0.6
    aspect = 16.0 / 9.0  # 1280 / 720

    # --- Left panel: NDC space (square, equal aspect) ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-1.55, 1.55), ylim=(-1.55, 1.55))

    # NDC boundary
    ndc_box = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=1.5,
        edgecolor=STYLE["axis"],
        facecolor=STYLE["surface"],
        alpha=0.3,
        zorder=1,
    )
    ax1.add_patch(ndc_box)

    # Circle
    ax1.fill(
        r * np.cos(theta),
        r * np.sin(theta),
        color=STYLE["accent1"],
        alpha=0.25,
        zorder=2,
    )
    ax1.plot(
        r * np.cos(theta),
        r * np.sin(theta),
        "-",
        color=STYLE["accent1"],
        lw=2.5,
        zorder=4,
    )
    ax1.text(
        0,
        0,
        "circle",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Corner labels
    ax1.text(
        1.0,
        -1.2,
        "+1",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax1.text(
        -1.0,
        -1.2,
        "\u22121",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax1.text(
        -1.25,
        1.0,
        "+1",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax1.text(
        -1.25,
        -1.0,
        "\u22121",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    ax1.set_xticks([])
    ax1.set_yticks([])
    ax1.set_title(
        "NDC Space (square \u22121 to +1)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    # --- Arrow between panels ---
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
        0.40,
        "maps to\nwindow",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # --- Right panel: Window (16:9) ---
    # Use a non-equal aspect so the axes are wider than tall, showing stretch.
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-aspect - 0.3, aspect + 0.3), ylim=(-1.55, 1.55), aspect=None)

    # Window boundary (wider rectangle)
    win_box = Rectangle(
        (-aspect, -1),
        2 * aspect,
        2,
        linewidth=1.5,
        edgecolor=STYLE["axis"],
        facecolor=STYLE["surface"],
        alpha=0.3,
        zorder=1,
    )
    ax2.add_patch(win_box)

    # Same NDC circle mapped to the window — stretched horizontally
    ax2.fill(
        r * np.cos(theta) * aspect,
        r * np.sin(theta),
        color=STYLE["accent2"],
        alpha=0.25,
        zorder=2,
    )
    ax2.plot(
        r * np.cos(theta) * aspect,
        r * np.sin(theta),
        "-",
        color=STYLE["accent2"],
        lw=2.5,
        zorder=4,
    )
    ax2.text(
        0,
        0,
        "ellipse",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Dimension labels
    ax2.text(
        0,
        -1.3,
        "1280 px",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax2.text(
        aspect + 0.15,
        0,
        "720\npx",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    ax2.set_xticks([])
    ax2.set_yticks([])
    ax2.set_title(
        "Window (1280\u00d7720, stretched)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/03-uniforms-and-motion", "aspect_ratio.png")


# ---------------------------------------------------------------------------
# gpu/04-textures-and-samplers — uv_mapping.png
# ---------------------------------------------------------------------------
