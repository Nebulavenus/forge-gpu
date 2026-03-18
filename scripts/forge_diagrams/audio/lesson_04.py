"""Diagrams for Audio Lesson 04 — Spatial Audio."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


# ---------------------------------------------------------------------------
# audio/04-spatial-audio — attenuation_curves.png
# ---------------------------------------------------------------------------


def diagram_attenuation_curves():
    """Plot all three distance attenuation models on the same axes."""
    fig, ax = plt.subplots(figsize=(10, 6), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 55), ylim=(-0.05, 1.15), grid=True, aspect=None)

    min_d = 1.0
    max_d = 50.0
    rolloff = 1.0

    d = np.linspace(min_d, max_d, 500)

    # Linear: gain = 1 - rolloff * (d - min) / (max - min)
    linear = 1.0 - rolloff * (d - min_d) / (max_d - min_d)
    linear = np.clip(linear, 0.0, 1.0)

    # Inverse: gain = min / (min + rolloff * (d - min))
    inverse = min_d / (min_d + rolloff * (d - min_d))

    # Exponential: gain = pow(d / min, -rolloff)
    exponential = np.power(d / min_d, -rolloff)

    ax.plot(
        d,
        linear,
        color=STYLE["accent1"],
        linewidth=2.5,
        label="Linear",
        path_effects=_STROKE,
    )
    ax.plot(
        d,
        inverse,
        color=STYLE["accent2"],
        linewidth=2.5,
        label="Inverse",
        path_effects=_STROKE,
    )
    ax.plot(
        d,
        exponential,
        color=STYLE["accent3"],
        linewidth=2.5,
        label="Exponential",
        path_effects=_STROKE,
    )

    # Mark min and max distance
    ax.axvline(min_d, color=STYLE["warn"], linewidth=1, linestyle="--", alpha=0.6)
    ax.axvline(max_d, color=STYLE["warn"], linewidth=1, linestyle="--", alpha=0.6)
    ax.text(
        min_d + 0.5, 1.08, "min", color=STYLE["warn"], fontsize=10, path_effects=_STROKE
    )
    ax.text(
        max_d - 3.0, 1.08, "max", color=STYLE["warn"], fontsize=10, path_effects=_STROKE
    )

    ax.set_xlabel("Distance", color=STYLE["text"], fontsize=12)
    ax.set_ylabel("Gain", color=STYLE["text"], fontsize=12)
    ax.set_title(
        "Distance Attenuation Models", color=STYLE["text"], fontsize=14, pad=12
    )
    ax.legend(
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=11,
        loc="upper right",
    )

    return save(fig, "audio/04-spatial-audio", "attenuation_curves.png")


# ---------------------------------------------------------------------------
# audio/04-spatial-audio — stereo_pan_diagram.png
# ---------------------------------------------------------------------------


def diagram_stereo_pan():
    """Top-down view of listener orientation and pan projection."""
    fig, ax = plt.subplots(figsize=(8, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-5, 5), ylim=(-5, 5), grid=False, aspect="equal")
    ax.axis("off")

    # Listener at origin
    listener = np.array([0.0, 0.0])

    # Forward direction (up in the diagram = -Z in 3D, but we draw as +Y)
    fwd = np.array([0.0, 1.0])
    right = np.array([1.0, 0.0])

    # Draw listener as a circle
    circle = mpatches.Circle(
        listener,
        0.25,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=5,
    )
    ax.add_patch(circle)
    ax.text(
        0.0,
        -0.7,
        "Listener",
        color=STYLE["text"],
        fontsize=11,
        ha="center",
        path_effects=_STROKE,
    )

    # Draw forward arrow
    ax.annotate(
        "",
        xy=fwd * 3.5,
        xytext=listener,
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["accent3"],
            "linewidth": 2,
        },
    )
    ax.text(
        fwd[0] * 3.5 + 0.3,
        fwd[1] * 3.5,
        "forward",
        color=STYLE["accent3"],
        fontsize=11,
        path_effects=_STROKE,
    )

    # Draw right arrow
    ax.annotate(
        "",
        xy=right * 3.5,
        xytext=listener,
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["accent2"],
            "linewidth": 2,
        },
    )
    ax.text(
        right[0] * 3.5 + 0.2,
        right[1] * 3.5 - 0.4,
        "right",
        color=STYLE["accent2"],
        fontsize=11,
        path_effects=_STROKE,
    )

    # Source position (upper-right)
    source = np.array([3.0, 2.0])
    source_circle = mpatches.Circle(
        source,
        0.2,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=5,
    )
    ax.add_patch(source_circle)
    ax.text(
        source[0] + 0.4,
        source[1] + 0.2,
        "Source",
        color=STYLE["warn"],
        fontsize=11,
        path_effects=_STROKE,
    )

    # Direction vector from listener to source
    to_source = source - listener
    dist = np.linalg.norm(to_source)
    direction = to_source / dist

    ax.annotate(
        "",
        xy=source,
        xytext=listener,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.15",
            "color": STYLE["text_dim"],
            "linewidth": 1.5,
            "linestyle": "--",
        },
    )

    # Project onto right axis (dashed line from source down to right axis)
    proj_len = np.dot(direction, right)
    proj_point = right * proj_len * dist
    ax.plot(
        [source[0], proj_point[0]],
        [source[1], proj_point[1]],
        color=STYLE["accent4"],
        linewidth=1.5,
        linestyle=":",
        zorder=3,
    )

    # Pan value annotation
    pan_val = np.dot(direction, right)
    ax.text(
        2.0,
        -1.5,
        f"pan = dot(dir, right) = {pan_val:.2f}",
        color=STYLE["text"],
        fontsize=12,
        ha="center",
        bbox={
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["grid"],
            "boxstyle": "round,pad=0.4",
        },
        path_effects=_STROKE,
    )

    # Left/Right labels
    ax.text(
        -4.0,
        0.0,
        "Left\npan = \u22121",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=_STROKE,
    )
    ax.text(
        4.0,
        0.0,
        "Right\npan = +1",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Stereo Pan from 3D Position", color=STYLE["text"], fontsize=14, pad=12
    )

    return save(fig, "audio/04-spatial-audio", "stereo_pan_diagram.png")


# ---------------------------------------------------------------------------
# audio/04-spatial-audio — doppler_effect.png
# ---------------------------------------------------------------------------


def diagram_doppler_effect():
    """Visualization of Doppler effect: approaching vs receding source."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 6), facecolor=STYLE["bg"])

    for ax in axes:
        setup_axes(ax, xlim=(-6, 6), ylim=(-4, 4), grid=False, aspect="equal")
        ax.axis("off")

    # -- Left panel: approaching source --
    ax = axes[0]
    ax.set_title("Approaching Source", color=STYLE["text"], fontsize=13, pad=10)

    # Listener on the right
    l_circle = mpatches.Circle(
        (3.5, 0),
        0.3,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=5,
    )
    ax.add_patch(l_circle)
    ax.text(
        3.5,
        -0.8,
        "Listener",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # Source on the left, moving right
    s_circle = mpatches.Circle(
        (-3.0, 0),
        0.25,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=5,
    )
    ax.add_patch(s_circle)
    ax.text(
        -3.0,
        -0.8,
        "Source",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # Velocity arrow
    ax.annotate(
        "",
        xy=(-1.5, 0),
        xytext=(-2.7, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["accent3"],
            "linewidth": 2.5,
        },
    )

    # Compressed wavefronts (closer spacing ahead of source)
    for i, r in enumerate([0.6, 1.1, 1.5, 1.8]):
        theta = np.linspace(-np.pi / 2, np.pi / 2, 100)
        x = -3.0 + r * 0.7 + r * np.cos(theta)
        y = r * np.sin(theta)
        alpha = 0.7 - i * 0.12
        ax.plot(x, y, color=STYLE["accent2"], linewidth=1.5, alpha=alpha)

    # Stretched wavefronts behind
    for i, r in enumerate([0.8, 1.6, 2.5]):
        theta = np.linspace(np.pi / 2, 3 * np.pi / 2, 100)
        x = -3.0 + r * np.cos(theta)
        y = r * np.sin(theta)
        alpha = 0.5 - i * 0.12
        ax.plot(x, y, color=STYLE["accent2"], linewidth=1.0, alpha=alpha)

    ax.text(
        0.0,
        2.8,
        "Compressed wavelengths\n\u2192 higher pitch",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # -- Right panel: receding source --
    ax = axes[1]
    ax.set_title("Receding Source", color=STYLE["text"], fontsize=13, pad=10)

    # Listener on the left
    l_circle2 = mpatches.Circle(
        (-3.5, 0),
        0.3,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=5,
    )
    ax.add_patch(l_circle2)
    ax.text(
        -3.5,
        -0.8,
        "Listener",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # Source on the right, moving right
    s_circle2 = mpatches.Circle(
        (3.0, 0),
        0.25,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=5,
    )
    ax.add_patch(s_circle2)
    ax.text(
        3.0,
        -0.8,
        "Source",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # Velocity arrow
    ax.annotate(
        "",
        xy=(4.5, 0),
        xytext=(3.3, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["accent3"],
            "linewidth": 2.5,
        },
    )

    # Stretched wavefronts behind (toward listener)
    for i, r in enumerate([0.8, 1.6, 2.5]):
        theta = np.linspace(np.pi / 2, 3 * np.pi / 2, 100)
        x = 3.0 + r * np.cos(theta)
        y = r * np.sin(theta)
        alpha = 0.5 - i * 0.12
        ax.plot(x, y, color=STYLE["accent1"], linewidth=1.0, alpha=alpha)

    # Compressed wavefronts ahead
    for i, r in enumerate([0.6, 1.1, 1.5, 1.8]):
        theta = np.linspace(-np.pi / 2, np.pi / 2, 100)
        x = 3.0 + r * 0.7 + r * np.cos(theta)
        y = r * np.sin(theta)
        alpha = 0.7 - i * 0.12
        ax.plot(x, y, color=STYLE["accent1"], linewidth=1.5, alpha=alpha)

    ax.text(
        0.0,
        2.8,
        "Stretched wavelengths\n\u2192 lower pitch",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    fig.suptitle("Doppler Effect", color=STYLE["text"], fontsize=15, y=0.98)
    fig.tight_layout(rect=[0, 0, 1, 0.93])

    return save(fig, "audio/04-spatial-audio", "doppler_effect.png")


# ---------------------------------------------------------------------------
# audio/04-spatial-audio — spatial_setup_flow.png
# ---------------------------------------------------------------------------


def diagram_spatial_setup_flow():
    """Data-flow diagram: game objects → spatial sources → mixer → output."""
    fig, ax = plt.subplots(figsize=(14, 7), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-1.5, 6.5), grid=False, aspect=None)
    ax.axis("off")

    # ── Helper to draw a rounded box with label ──────────────────────────
    def box(x, y, w, h, label, color, fontsize=10):
        rect = mpatches.FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            zorder=3,
        )
        ax.add_patch(rect)
        ax.text(
            x + w / 2,
            y + h / 2,
            label,
            color=STYLE["text"],
            fontsize=fontsize,
            ha="center",
            va="center",
            fontweight="bold",
            path_effects=_STROKE,
            zorder=4,
        )

    def arrow(x0, y0, x1, y1, color):
        ax.annotate(
            "",
            xy=(x1, y1),
            xytext=(x0, y0),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.15",
                "color": color,
                "linewidth": 2,
            },
            zorder=2,
        )

    def note(x, y, text, color=STYLE["text_dim"], fontsize=9):
        ax.text(
            x,
            y,
            text,
            color=color,
            fontsize=fontsize,
            ha="center",
            va="center",
            path_effects=_STROKE,
            zorder=4,
        )

    # ── Row 1: Game objects (top) ────────────────────────────────────────
    row1_y = 5.0
    bh = 0.8

    box(0.0, row1_y, 2.5, bh, "Game Entity\n(position, velocity)", STYLE["warn"])
    box(3.5, row1_y, 2.5, bh, "Game Entity\n(position, velocity)", STYLE["warn"])
    box(7.0, row1_y, 2.5, bh, "Game Entity\n(position, velocity)", STYLE["warn"])

    note(1.25, row1_y + bh + 0.3, "car", STYLE["warn"])
    note(4.75, row1_y + bh + 0.3, "alarm", STYLE["warn"])
    note(8.25, row1_y + bh + 0.3, "wind", STYLE["warn"])

    # ── Row 2: Spatial sources ───────────────────────────────────────────
    row2_y = 3.2

    box(0.0, row2_y, 2.5, bh, "SpatialSource\n+ AudioSource", STYLE["accent2"])
    box(3.5, row2_y, 2.5, bh, "SpatialSource\n+ AudioSource", STYLE["accent2"])
    box(7.0, row2_y, 2.5, bh, "SpatialSource\n+ AudioSource", STYLE["accent2"])

    # Arrows: entities → spatial sources (position updates)
    for x_off in [1.25, 4.75, 8.25]:
        arrow(x_off, row1_y, x_off, row2_y + bh + 0.05, STYLE["warn"])

    note(
        10.2,
        row2_y + bh / 2,
        ".position = entity.pos\n.velocity = entity.vel",
        STYLE["text_dim"],
        9,
    )

    # ── Camera / Listener (side) ─────────────────────────────────────────
    box(11.0, row2_y, 2.5, bh, "Camera\n→ Listener", STYLE["accent1"])

    note(
        12.25, row2_y + bh + 0.3, "listener_from_camera(pos, quat)", STYLE["accent1"], 8
    )

    # Arrow from listener to the spatial_apply zone
    arrow(11.0, row2_y + bh / 2, 9.8, row2_y + bh / 2, STYLE["accent1"])

    # ── spatial_apply zone ───────────────────────────────────────────────
    apply_y = 1.6
    apply_rect = mpatches.FancyBboxPatch(
        (0.0, apply_y),
        9.5,
        0.9,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
        linestyle="--",
        zorder=2,
    )
    ax.add_patch(apply_rect)
    ax.text(
        4.75,
        apply_y + 0.45,
        "forge_audio_spatial_apply(&listener, &spatial[i])   ×  N sources",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=_STROKE,
        zorder=4,
    )

    # Arrows: spatial sources → apply zone
    for x_off in [1.25, 4.75, 8.25]:
        arrow(x_off, row2_y, x_off, apply_y + 0.95, STYLE["accent2"])

    # ── Row 3: Mixer ─────────────────────────────────────────────────────
    mixer_y = 0.0

    box(
        2.5,
        mixer_y,
        4.5,
        bh,
        "ForgeAudioMixer\n(channels, master, peaks)",
        STYLE["accent4"],
        11,
    )

    # Arrow: apply → mixer
    arrow(4.75, apply_y, 4.75, mixer_y + bh + 0.05, STYLE["accent3"])

    note(
        8.0,
        mixer_y + bh / 2,
        "writes channel.volume\nwrites channel.pan",
        STYLE["accent3"],
        9,
    )

    # ── Row 4: Output ────────────────────────────────────────────────────
    box(2.5, -1.2, 4.5, 0.7, "SDL Audio Stream → Speakers", STYLE["text_dim"], 10)

    arrow(4.75, mixer_y, 4.75, -1.2 + 0.75, STYLE["accent4"])

    note(8.0, -0.85, "mixer_mix() → PutAudioStreamData()", STYLE["text_dim"], 9)

    # ── Title ────────────────────────────────────────────────────────────
    ax.set_title(
        "Spatial Audio Setup Flow",
        color=STYLE["text"],
        fontsize=14,
        pad=12,
    )

    return save(fig, "audio/04-spatial-audio", "spatial_setup_flow.png")
