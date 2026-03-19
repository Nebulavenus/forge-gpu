"""Diagrams for Audio Lesson 05 — Music and Streaming."""

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Rectangle

from .._common import STYLE, save, setup_axes

_LESSON = "audio/05-music-streaming"


# ---------------------------------------------------------------------------
# audio/05-music-streaming — streaming_architecture.png
# ---------------------------------------------------------------------------


def diagram_streaming_architecture():
    """WAV file -> chunk read -> SDL_AudioStream -> ring buffer -> mix output."""
    fig, ax = plt.subplots(figsize=(10, 5), facecolor=STYLE["bg"])
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-0.5, 5.5)
    ax.set_aspect("equal")
    ax.axis("off")

    box_style = "round,pad=0.15"
    box_color = STYLE["surface"]
    edge_color = STYLE["accent2"]
    text_color = STYLE["text"]
    arrow_color = STYLE["accent1"]

    # Boxes: WAV File, Chunk Read, SDL_AudioStream, Ring Buffer, Mix Output
    boxes = [
        (0.5, 3.0, 1.8, 1.0, "WAV File\n(on disk)"),
        (3.0, 3.0, 1.8, 1.0, "Chunk Read\n(4096 frames)"),
        (5.5, 3.0, 2.0, 1.0, "SDL_Audio\nStream"),
        (8.0, 3.0, 2.0, 1.0, "Ring Buffer\n(16384 frames)"),
        (8.0, 0.8, 2.0, 1.0, "Mix Output\n(to device)"),
    ]

    for x, y, w, h, label in boxes:
        rect = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle=box_style,
            facecolor=box_color,
            edgecolor=edge_color,
            linewidth=1.5,
        )
        ax.add_patch(rect)
        ax.text(
            x + w / 2,
            y + h / 2,
            label,
            ha="center",
            va="center",
            fontsize=9,
            color=text_color,
            fontweight="bold",
        )

    # Arrows between boxes
    arrows = [
        ((2.3, 3.5), (3.0, 3.5)),
        ((4.8, 3.5), (5.5, 3.5)),
        ((7.5, 3.5), (8.0, 3.5)),
        ((9.0, 3.0), (9.0, 1.8)),
    ]
    for start, end in arrows:
        ax.annotate(
            "",
            xy=end,
            xytext=start,
            arrowprops=dict(arrowstyle="->", color=arrow_color, lw=2),
        )

    # Arrow labels
    ax.text(
        2.65, 3.85, "seek +\nread", fontsize=7, color=STYLE["text_dim"], ha="center"
    )
    ax.text(
        5.15, 3.85, "convert\nformat", fontsize=7, color=STYLE["text_dim"], ha="center"
    )
    ax.text(7.75, 3.85, "fill", fontsize=7, color=STYLE["text_dim"], ha="center")
    ax.text(9.35, 2.4, "drain", fontsize=7, color=STYLE["text_dim"], ha="center")

    # Ring buffer detail below
    ring_y = 1.6
    ring_x = 1.0
    chunk_w = 1.2
    fills = [STYLE["accent1"], STYLE["accent1"], STYLE["surface"], STYLE["surface"]]
    alphas = [0.4, 0.4, 0.15, 0.15]
    labels_r = [
        "chunk 0\n(filled)",
        "chunk 1\n(filled)",
        "chunk 2\n(empty)",
        "chunk 3\n(empty)",
    ]
    for i in range(4):
        rect = Rectangle(
            (ring_x + i * chunk_w, ring_y),
            chunk_w,
            0.6,
            facecolor=fills[i],
            edgecolor=edge_color,
            linewidth=1,
            alpha=alphas[i],
        )
        ax.add_patch(rect)
        ax.text(
            ring_x + i * chunk_w + chunk_w / 2,
            ring_y + 0.3,
            labels_r[i],
            ha="center",
            va="center",
            fontsize=7,
            color=text_color,
        )

    # Read/write cursors
    ax.annotate(
        "read",
        xy=(ring_x, ring_y),
        xytext=(ring_x, ring_y - 0.4),
        fontsize=8,
        color=STYLE["accent2"],
        ha="center",
        arrowprops=dict(arrowstyle="->", color=STYLE["accent2"], lw=1.5),
    )
    ax.annotate(
        "write",
        xy=(ring_x + 2 * chunk_w, ring_y + 0.6),
        xytext=(ring_x + 2 * chunk_w, ring_y + 1.0),
        fontsize=8,
        color=STYLE["accent3"],
        ha="center",
        arrowprops=dict(arrowstyle="->", color=STYLE["accent3"], lw=1.5),
    )

    ax.text(
        ring_x + 2 * chunk_w,
        ring_y - 0.4,
        "Ring Buffer Detail (4 chunks, wrapping)",
        fontsize=8,
        color=STYLE["text_dim"],
        ha="center",
    )

    save(fig, _LESSON, "streaming_architecture.png")


# ---------------------------------------------------------------------------
# audio/05-music-streaming — crossfade_curves.png
# ---------------------------------------------------------------------------


def diagram_crossfade_curves():
    """Equal-power vs linear crossfade gain curves."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4), facecolor=STYLE["bg"])

    t = np.linspace(0, 1, 200)

    for ax, title, gain_out, gain_in in [
        (ax1, "Linear Crossfade", 1 - t, t),
        (ax2, "Equal-Power Crossfade", np.sqrt(1 - t), np.sqrt(t)),
    ]:
        setup_axes(ax, xlim=(0, 1), ylim=(0, 1.15), grid=True, aspect=None)
        ax.plot(t, gain_out, color=STYLE["accent2"], linewidth=2, label="Outgoing")
        ax.plot(t, gain_in, color=STYLE["accent3"], linewidth=2, label="Incoming")
        ax.plot(
            t,
            gain_out**2 + gain_in**2,
            color=STYLE["accent1"],
            linewidth=1.5,
            linestyle="--",
            label="Total power",
        )

        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold")
        ax.set_xlabel("Crossfade progress (t)", color=STYLE["text_dim"], fontsize=9)
        ax.set_ylabel("Gain / Power", color=STYLE["text_dim"], fontsize=9)
        ax.legend(
            fontsize=8,
            loc="center right",
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            labelcolor=STYLE["text"],
        )

    # Mark the power dip on linear: at t=0.5, power = 0.5² + 0.5² = 0.5
    ax1.annotate(
        "3 dB dip\n(power = 0.50)",
        xy=(0.5, 0.5),
        xytext=(0.3, 0.2),
        fontsize=8,
        color=STYLE["accent1"],
        arrowprops=dict(arrowstyle="->", color=STYLE["accent1"], lw=1),
    )

    save(fig, _LESSON, "crossfade_curves.png")


# ---------------------------------------------------------------------------
# audio/05-music-streaming — adaptive_layers.png
# ---------------------------------------------------------------------------


def diagram_adaptive_layers():
    """Stacked stems visualization showing weight-based mixing."""
    fig, ax = plt.subplots(figsize=(10, 5), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])

    # Simulated waveforms for each layer
    t = np.linspace(0, 4, 1000)
    rng = np.random.default_rng(42)

    layers = [
        ("Layer 1 (Base)", 1.0, STYLE["accent2"]),
        ("Layer 2 (Mid)", 0.7, STYLE["accent3"]),
        ("Layer 3 (Lead)", 0.3, STYLE["accent1"]),
    ]

    y_offset = 0
    spacing = 1.8

    for i, (name, weight, color) in enumerate(layers):
        freq = 2 + i * 0.7
        wave = np.sin(2 * np.pi * freq * t) * 0.3
        wave += np.sin(2 * np.pi * freq * 2.1 * t) * 0.15
        wave += rng.standard_normal(len(t)) * 0.05

        y = y_offset - i * spacing

        alpha = max(weight, 0.15)
        ax.fill_between(
            t,
            y + wave * weight,
            y - wave * weight,
            alpha=alpha,
            color=color,
            linewidth=0,
        )
        ax.plot(t, y + wave * weight, color=color, linewidth=0.5, alpha=alpha)
        ax.plot(t, y - wave * weight, color=color, linewidth=0.5, alpha=alpha)

        label = f"{name}  (w={weight:.1f})"
        ax.text(
            -0.3,
            y,
            label,
            fontsize=9,
            color=STYLE["text"],
            ha="right",
            va="center",
            fontweight="bold",
        )

    ax.set_xlim(-0.2, 4.2)
    ax.set_ylim(-4.5, 1.0)
    ax.set_xlabel("Time (seconds)", color=STYLE["text_dim"], fontsize=9)
    ax.set_title(
        "Adaptive Music Layers \u2014 Synchronized Stems",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
    )
    ax.tick_params(colors=STYLE["axis"])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])

    ax.text(3.5, 0.7, "weight = 1.0: full volume", fontsize=8, color=STYLE["accent2"])
    ax.text(3.5, 0.3, "weight = 0.0: silent", fontsize=8, color=STYLE["text_dim"])

    save(fig, _LESSON, "adaptive_layers.png")
