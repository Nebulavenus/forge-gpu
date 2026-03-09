"""Diagrams for gpu/20."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_falloff_curves.png
# ---------------------------------------------------------------------------


def diagram_fog_falloff_curves():
    """Three fog falloff modes: linear, exponential, exponential-squared."""
    fig, ax = plt.subplots(figsize=(10, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 25), ylim=(-0.05, 1.1), grid=True, aspect=None)

    d = np.linspace(0, 25, 500)

    # Linear fog: ramps from 1 at start to 0 at end
    start, end = 2.0, 18.0
    linear = np.clip((end - d) / (end - start), 0.0, 1.0)

    # Exponential: e^(-density * d)
    exp_density = 0.12
    exponential = np.clip(np.exp(-exp_density * d), 0.0, 1.0)

    # Exp-squared: e^(-(density * d)^2)
    exp2_density = 0.08
    exp_squared = np.clip(np.exp(-((exp2_density * d) ** 2)), 0.0, 1.0)

    ax.plot(d, linear, color=STYLE["accent1"], lw=2.5, label="Linear")
    ax.plot(d, exponential, color=STYLE["accent2"], lw=2.5, label="Exponential")
    ax.plot(d, exp_squared, color=STYLE["accent3"], lw=2.5, label="Exp-squared")

    # Annotate regions
    ax.axhspan(0.95, 1.1, color=STYLE["accent1"], alpha=0.06)
    ax.text(
        1.0,
        1.05,
        "Fully visible",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
    )
    ax.axhspan(-0.05, 0.05, color=STYLE["accent2"], alpha=0.06)
    ax.text(
        20,
        -0.02,
        "Fully fogged",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
    )

    # Mark linear start/end
    ax.axvline(start, color=STYLE["accent1"], ls="--", lw=0.8, alpha=0.5)
    ax.axvline(end, color=STYLE["accent1"], ls="--", lw=0.8, alpha=0.5)
    ax.text(
        start,
        -0.02,
        f"start={start:.0f}",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="top",
    )
    ax.text(
        end,
        -0.02,
        f"end={end:.0f}",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="top",
    )

    ax.set_xlabel("Distance from camera", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Fog factor (visibility)", color=STYLE["text"], fontsize=11)

    leg = ax.legend(
        loc="upper right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    fig.suptitle(
        "Fog Falloff Modes",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/20-linear-fog", "fog_falloff_curves.png")


# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_blending.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_blending.png
# ---------------------------------------------------------------------------


def diagram_fog_blending():
    """Horizontal gradient bars showing fog blending for each mode."""
    fig, axes = plt.subplots(3, 1, figsize=(10, 4), facecolor=STYLE["bg"])

    d = np.linspace(0, 25, 256)

    # Surface color (warm truck-like tone) and fog color (medium gray)
    surface = np.array([0.75, 0.60, 0.22])
    fog_col = np.array([0.5, 0.5, 0.5])

    modes = [
        ("Linear", np.clip((18.0 - d) / (18.0 - 2.0), 0.0, 1.0)),
        ("Exponential", np.clip(np.exp(-0.12 * d), 0.0, 1.0)),
        ("Exp-squared", np.clip(np.exp(-((0.08 * d) ** 2)), 0.0, 1.0)),
    ]
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    for ax, (name, factor), col in zip(axes, modes, colors, strict=True):
        # Build a 2D image: 1 row x 256 columns x 3 channels
        gradient = np.zeros((1, 256, 3))
        for i in range(256):
            f = factor[i]
            gradient[0, i] = f * surface + (1.0 - f) * fog_col

        ax.imshow(gradient, aspect="auto", extent=(0, 25, 0, 1))
        ax.set_facecolor(STYLE["bg"])
        ax.set_yticks([])
        ax.set_xlim(0, 25)
        ax.tick_params(colors=STYLE["axis"], labelsize=8)
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)
        ax.set_ylabel(
            name,
            color=col,
            fontsize=10,
            fontweight="bold",
            rotation=0,
            labelpad=80,
            va="center",
        )

    axes[0].set_title(
        "Surface Color \u2192 Fog Color Blend over Distance",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    axes[-1].set_xlabel("Distance from camera", color=STYLE["text"], fontsize=10)

    # Labels
    axes[0].text(
        0.5,
        0.5,
        "Near\n(visible)",
        color=STYLE["text"],
        fontsize=8,
        ha="left",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    axes[0].text(
        24,
        0.5,
        "Far\n(fogged)",
        color=STYLE["text"],
        fontsize=8,
        ha="right",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "gpu/20-linear-fog", "fog_blending.png")


# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_scene_layout.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_scene_layout.png
# ---------------------------------------------------------------------------


def diagram_fog_scene_layout():
    """Top-down view of the scene with camera, truck, boxes, and fog gradient."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-10, 10), ylim=(-10, 10), grid=False, aspect="equal")

    # Fog gradient background (radial — denser farther from center)
    for r_i in range(50):
        radius = 10.0 - r_i * 0.2
        alpha = 0.02 + 0.15 * (r_i / 50.0)
        circle = Circle(
            (0, 0), radius, color=STYLE["text"], alpha=alpha, fill=True, zorder=0
        )
        ax.add_patch(circle)

    # Camera position (at -6, 6 in world XZ)
    ax.plot(-6, 6, "^", color=STYLE["warn"], markersize=14, zorder=5)
    ax.text(
        -6,
        7.2,
        "Camera",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Truck at origin
    truck_rect = Rectangle((-1.2, -0.6), 2.4, 1.2, color="#cc6633", alpha=0.9, zorder=4)
    ax.add_patch(truck_rect)
    ax.text(
        0,
        0,
        "Truck",
        color=STYLE["bg"],
        fontsize=9,
        ha="center",
        va="center",
        fontweight="bold",
        zorder=5,
    )

    # 8 ground boxes in a ring at radius 5
    box_radius = 5.0
    box_color = "#b8860b"
    for i in range(8):
        angle = i * (2.0 * np.pi / 8.0)
        bx = np.cos(angle) * box_radius
        bz = np.sin(angle) * box_radius
        box = Rectangle(
            (bx - 0.5, bz - 0.5), 1.0, 1.0, color=box_color, alpha=0.8, zorder=4
        )
        ax.add_patch(box)

        # Mark stacked boxes (every other)
        if i % 2 == 0:
            stack = Rectangle(
                (bx - 0.4, bz - 0.4),
                0.8,
                0.8,
                color="#daa520",
                alpha=0.6,
                zorder=4,
                linestyle="--",
                linewidth=1.5,
                edgecolor=STYLE["text"],
                fill=False,
            )
            ax.add_patch(stack)

    # Ring radius annotation
    ax.annotate(
        "",
        xy=(box_radius, 0),
        xytext=(0, 0),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent1"], "lw": 1.5},
        zorder=6,
    )
    ax.text(
        2.5,
        -0.7,
        "r = 5",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Distance from camera to nearest/farthest objects
    ax.annotate(
        "",
        xy=(0, 0),
        xytext=(-6, 6),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent2"],
            "lw": 1.2,
            "ls": "--",
        },
        zorder=6,
    )
    ax.text(
        -4.0,
        3.5,
        "d \u2248 8.5",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Legend
    ax.text(
        7.5,
        -8.5,
        "Fog density\nincreases\nwith distance",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("X (world)", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Z (world)", color=STYLE["text"], fontsize=10)

    fig.suptitle(
        "Scene Layout \u2014 Top-Down View",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/20-linear-fog", "fog_scene_layout.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — hdr_pipeline.png
# ---------------------------------------------------------------------------
