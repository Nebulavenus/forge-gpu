"""Diagrams for Audio Lesson 06 — DSP Effects."""

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, FancyBboxPatch

from .._common import STYLE, save, setup_axes

_LESSON = "audio/06-dsp-effects"


# ---------------------------------------------------------------------------
# audio/06-dsp-effects — effect_chain_signal_flow.png
# ---------------------------------------------------------------------------


def diagram_effect_chain_signal_flow():
    """Mixer signal chain showing channel and master effect chain insertion points."""
    fig, ax = plt.subplots(figsize=(12, 5), facecolor=STYLE["bg"])
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 5.0)
    ax.set_aspect("equal")
    ax.axis("off")

    box_style = "round,pad=0.12"
    box_color = STYLE["surface"]
    edge_color = STYLE["accent2"]
    text_color = STYLE["text"]
    arrow_color = STYLE["accent1"]
    fx_color = STYLE["accent4"]  # purple for effect boxes

    # Main signal chain boxes (top row)
    boxes = [
        (0.2, 3.0, 1.6, 0.9, "Source\nMix", box_color, edge_color),
        (2.3, 3.0, 1.8, 0.9, "Channel\nEffects", fx_color, STYLE["accent4"]),
        (4.6, 3.0, 1.6, 0.9, "Volume\n& Pan", box_color, edge_color),
        (6.7, 3.0, 1.2, 0.9, "Sum", box_color, edge_color),
        (8.4, 3.0, 1.8, 0.9, "Master\nEffects", fx_color, STYLE["accent4"]),
        (10.7, 3.0, 1.6, 0.9, "Master Vol\n& Soft Clip", box_color, edge_color),
    ]

    for x, y, w, h, label, fc, ec in boxes:
        rect = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle=box_style,
            facecolor=fc,
            edgecolor=ec,
            linewidth=1.5,
            alpha=0.85,
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
    arrow_y = 3.45
    arrow_pairs = [
        (1.8, 2.3),
        (4.1, 4.6),
        (6.2, 6.7),
        (7.9, 8.4),
        (10.2, 10.7),
    ]
    for x1, x2 in arrow_pairs:
        ax.annotate(
            "",
            xy=(x2, arrow_y),
            xytext=(x1, arrow_y),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.12",
                "color": arrow_color,
                "lw": 2,
            },
        )

    # Labels above effect boxes
    ax.text(
        3.2,
        4.15,
        "per-channel\n(before volume/pan)",
        ha="center",
        va="bottom",
        fontsize=7.5,
        color=STYLE["accent4"],
        fontstyle="italic",
    )
    ax.text(
        9.3,
        4.15,
        "master bus\n(before soft clip)",
        ha="center",
        va="bottom",
        fontsize=7.5,
        color=STYLE["accent4"],
        fontstyle="italic",
    )

    # Output arrow
    ax.annotate(
        "",
        xy=(12.4, arrow_y),
        xytext=(12.3, arrow_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": arrow_color,
            "lw": 2,
        },
    )
    ax.text(
        12.45,
        arrow_y,
        "Output",
        ha="left",
        va="center",
        fontsize=9,
        color=text_color,
        fontweight="bold",
    )

    # Effect chain detail (bottom)
    ax.text(
        3.2,
        1.6,
        "ForgeAudioEffectChain",
        ha="center",
        va="center",
        fontsize=8.5,
        color=STYLE["text_dim"],
    )

    chain_x = [1.0, 2.8, 4.6]
    chain_labels = ["Effect 0", "Effect 1", "Effect N"]
    for i, (cx, label) in enumerate(zip(chain_x, chain_labels, strict=True)):
        rect = FancyBboxPatch(
            (cx, 0.5),
            1.4,
            0.7,
            boxstyle=box_style,
            facecolor=fx_color,
            edgecolor=STYLE["accent4"],
            linewidth=1.0,
            alpha=0.6,
        )
        ax.add_patch(rect)
        ax.text(
            cx + 0.7,
            0.85,
            label,
            ha="center",
            va="center",
            fontsize=8,
            color=text_color,
        )
        # Wet/dry label
        ax.text(
            cx + 0.7,
            0.55,
            "wet/dry",
            ha="center",
            va="center",
            fontsize=6.5,
            color=STYLE["text_dim"],
        )
        if i < len(chain_labels) - 1:
            ax.annotate(
                "",
                xy=(chain_x[i + 1], 0.85),
                xytext=(cx + 1.4, 0.85),
                arrowprops={"arrowstyle": "->", "color": arrow_color, "lw": 1.5},
            )

    # Dots between effect 1 and N
    ax.text(
        4.35,
        0.85,
        "...",
        ha="center",
        va="center",
        fontsize=12,
        color=STYLE["text_dim"],
    )

    fig.suptitle(
        "Mixer Signal Chain with Effect Chains",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    save(fig, _LESSON, "effect_chain_signal_flow.png")


# ---------------------------------------------------------------------------
# audio/06-dsp-effects — biquad_frequency_response.png
# ---------------------------------------------------------------------------


def diagram_biquad_frequency_response():
    """Frequency response curves for LP, HP, and BP biquad filters at 1 kHz cutoff."""
    fig, ax = plt.subplots(figsize=(9, 5), facecolor=STYLE["bg"])
    setup_axes(ax, grid=True, aspect=None)

    sample_rate = 44100
    cutoff = 1000.0
    q = 0.707  # Butterworth

    freqs = np.logspace(np.log10(20), np.log10(20000), 1000)
    w = 2.0 * np.pi * freqs / sample_rate

    def biquad_response(b0, b1, b2, a1, a2, w):
        """Compute magnitude response of a biquad filter."""
        z = np.exp(1j * w)
        num = b0 + b1 * z**-1 + b2 * z**-2
        den = 1.0 + a1 * z**-1 + a2 * z**-2
        h = num / den
        return 20.0 * np.log10(np.maximum(np.abs(h), 1e-10))

    w0 = 2.0 * np.pi * cutoff / sample_rate
    sin_w = np.sin(w0)
    cos_w = np.cos(w0)
    alpha = sin_w / (2.0 * q)

    # Low-pass
    b1_lp = 1.0 - cos_w
    b0_lp = b1_lp / 2.0
    b2_lp = b0_lp
    a0_lp = 1.0 + alpha
    a1_lp = -2.0 * cos_w
    a2_lp = 1.0 - alpha
    mag_lp = biquad_response(
        b0_lp / a0_lp,
        b1_lp / a0_lp,
        b2_lp / a0_lp,
        a1_lp / a0_lp,
        a2_lp / a0_lp,
        w,
    )

    # High-pass
    b0_hp = (1.0 + cos_w) / 2.0
    b1_hp = -(1.0 + cos_w)
    b2_hp = b0_hp
    a0_hp = 1.0 + alpha
    a1_hp = -2.0 * cos_w
    a2_hp = 1.0 - alpha
    mag_hp = biquad_response(
        b0_hp / a0_hp,
        b1_hp / a0_hp,
        b2_hp / a0_hp,
        a1_hp / a0_hp,
        a2_hp / a0_hp,
        w,
    )

    # Band-pass
    b0_bp = alpha
    b1_bp = 0.0
    b2_bp = -alpha
    a0_bp = 1.0 + alpha
    a1_bp = -2.0 * cos_w
    a2_bp = 1.0 - alpha
    mag_bp = biquad_response(
        b0_bp / a0_bp,
        b1_bp / a0_bp,
        b2_bp / a0_bp,
        a1_bp / a0_bp,
        a2_bp / a0_bp,
        w,
    )

    ax.semilogx(freqs, mag_lp, color=STYLE["accent1"], linewidth=2.5, label="Low-Pass")
    ax.semilogx(freqs, mag_hp, color=STYLE["accent2"], linewidth=2.5, label="High-Pass")
    ax.semilogx(freqs, mag_bp, color=STYLE["accent3"], linewidth=2.5, label="Band-Pass")

    # Cutoff marker
    ax.axvline(x=cutoff, color=STYLE["warn"], linewidth=1.0, linestyle="--", alpha=0.7)
    ax.text(
        cutoff * 1.15,
        -2,
        f"fc = {int(cutoff)} Hz",
        color=STYLE["warn"],
        fontsize=9,
        va="top",
    )

    # -3 dB line
    ax.axhline(y=-3, color=STYLE["text_dim"], linewidth=0.8, linestyle=":", alpha=0.5)
    ax.text(25, -2.5, "-3 dB", color=STYLE["text_dim"], fontsize=8)

    ax.set_xlim(20, 20000)
    ax.set_ylim(-40, 5)
    ax.set_xlabel("Frequency (Hz)", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Magnitude (dB)", color=STYLE["text"], fontsize=10)
    ax.legend(
        loc="lower left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    fig.suptitle(
        "Biquad Filter Frequency Response (fc=1 kHz, Q=0.707)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    save(fig, _LESSON, "biquad_frequency_response.png")


# ---------------------------------------------------------------------------
# audio/06-dsp-effects — schroeder_reverb_architecture.png
# ---------------------------------------------------------------------------


def diagram_schroeder_reverb_architecture():
    """Schroeder reverb: 4 parallel comb filters feeding 2 series allpass filters."""
    fig, ax = plt.subplots(figsize=(11, 6), facecolor=STYLE["bg"])
    ax.set_xlim(-0.5, 11.5)
    ax.set_ylim(-0.5, 6.5)
    ax.set_aspect("equal")
    ax.axis("off")

    box_style = "round,pad=0.1"
    text_color = STYLE["text"]
    arrow_color = STYLE["accent1"]
    comb_color = STYLE["accent2"]
    ap_color = STYLE["accent3"]
    sum_color = STYLE["accent4"]

    # Input
    ax.text(
        0.0,
        3.25,
        "Input",
        ha="center",
        va="center",
        fontsize=10,
        color=text_color,
        fontweight="bold",
    )

    # Split arrows to 4 comb filters
    comb_lengths = [1557, 1617, 1491, 1422]
    comb_y = [5.0, 3.8, 2.6, 1.4]

    for i, (length, cy) in enumerate(zip(comb_lengths, comb_y, strict=True)):
        # Arrow from input to comb
        ax.annotate(
            "",
            xy=(1.5, cy + 0.35),
            xytext=(0.5, 3.25),
            arrowprops={
                "arrowstyle": "->",
                "color": arrow_color,
                "lw": 1.5,
                "connectionstyle": "arc3,rad=0.0",
            },
        )

        # Comb filter box
        rect = FancyBboxPatch(
            (1.5, cy),
            2.5,
            0.7,
            boxstyle=box_style,
            facecolor=STYLE["surface"],
            edgecolor=comb_color,
            linewidth=1.5,
        )
        ax.add_patch(rect)
        ax.text(
            2.75,
            cy + 0.35,
            f"Comb {i + 1}\n{length} samples",
            ha="center",
            va="center",
            fontsize=8,
            color=text_color,
            fontweight="bold",
        )

        # LP damping label
        ax.text(
            2.75,
            cy - 0.15,
            "LP damping in feedback",
            ha="center",
            va="top",
            fontsize=6,
            color=STYLE["text_dim"],
            fontstyle="italic",
        )

        # Arrow from comb to sum
        ax.annotate(
            "",
            xy=(5.0, 3.25),
            xytext=(4.0, cy + 0.35),
            arrowprops={"arrowstyle": "->", "color": arrow_color, "lw": 1.5},
        )

    # Sum node
    circle = Circle(
        (5.25, 3.25), 0.3, facecolor=STYLE["surface"], edgecolor=sum_color, linewidth=2
    )
    ax.add_patch(circle)
    ax.text(
        5.25,
        3.25,
        "+",
        ha="center",
        va="center",
        fontsize=14,
        color=sum_color,
        fontweight="bold",
    )

    ax.text(
        5.25,
        2.6,
        "parallel\nsum",
        ha="center",
        va="top",
        fontsize=7,
        color=STYLE["text_dim"],
    )

    # Arrow to allpass 1
    ax.annotate(
        "",
        xy=(6.3, 3.25),
        xytext=(5.55, 3.25),
        arrowprops={"arrowstyle": "->", "color": arrow_color, "lw": 2},
    )

    # Allpass filters
    ap_lengths = [556, 441]
    ap_x = [6.3, 8.5]

    for i, (length, apx) in enumerate(zip(ap_lengths, ap_x, strict=True)):
        rect = FancyBboxPatch(
            (apx, 2.9),
            1.8,
            0.7,
            boxstyle=box_style,
            facecolor=STYLE["surface"],
            edgecolor=ap_color,
            linewidth=1.5,
        )
        ax.add_patch(rect)
        ax.text(
            apx + 0.9,
            3.25,
            f"Allpass {i + 1}\n{length} samples",
            ha="center",
            va="center",
            fontsize=8,
            color=text_color,
            fontweight="bold",
        )

        if i < len(ap_lengths) - 1:
            ax.annotate(
                "",
                xy=(ap_x[i + 1], 3.25),
                xytext=(apx + 1.8, 3.25),
                arrowprops={"arrowstyle": "->", "color": arrow_color, "lw": 2},
            )

    # Output arrow
    ax.annotate(
        "",
        xy=(11.0, 3.25),
        xytext=(10.3, 3.25),
        arrowprops={"arrowstyle": "->", "color": arrow_color, "lw": 2},
    )
    ax.text(
        11.1,
        3.25,
        "Output",
        ha="left",
        va="center",
        fontsize=10,
        color=text_color,
        fontweight="bold",
    )

    # Legend
    ax.text(
        0.5,
        6.0,
        "Schroeder Reverb Architecture",
        fontsize=13,
        color=text_color,
        fontweight="bold",
    )
    ax.text(
        0.5,
        5.5,
        "4 parallel comb filters (with LP damping) \u2192 2 series allpass filters",
        fontsize=9,
        color=STYLE["text_dim"],
    )

    # Stereo note
    ax.text(
        5.5,
        0.3,
        "Stereo width: right channel comb lengths offset by +23 samples",
        ha="center",
        va="center",
        fontsize=8,
        color=STYLE["warn"],
        fontstyle="italic",
    )

    save(fig, _LESSON, "schroeder_reverb_architecture.png")
