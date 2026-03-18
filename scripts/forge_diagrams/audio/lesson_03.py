"""Diagrams for Audio Lesson 03 — Audio Mixing."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


# ---------------------------------------------------------------------------
# audio/03-audio-mixing — mixer_signal_chain.png
# ---------------------------------------------------------------------------


def diagram_mixer_signal_chain():
    """Horizontal data-flow diagram of the mixer's signal processing pipeline."""
    fig, ax = plt.subplots(figsize=(14, 6), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-2.5, 5.5), grid=False, aspect=None)
    ax.axis("off")

    # ── Channel boxes (left side, stacked vertically) ──────────────────────
    ch_x = 0.0
    ch_w = 2.2
    ch_h = 0.7
    ch_gap = 0.15
    ch_colors = [
        STYLE["accent2"],  # warm colors for individual channels
        STYLE["accent1"],
        STYLE["warn"],
        STYLE["accent4"],
        STYLE["accent3"],
    ]

    # Center the 5 channels vertically around y=2.5
    ch_total_h = 5 * ch_h + 4 * ch_gap
    ch_y_start = 2.5 + ch_total_h / 2

    for i in range(5):
        y = ch_y_start - (i + 1) * ch_h - i * ch_gap
        color = ch_colors[i]

        rect = mpatches.FancyBboxPatch(
            (ch_x, y),
            ch_w,
            ch_h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=color,
            lw=1.5,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            ch_x + ch_w / 2,
            y + ch_h / 2,
            f"Ch {i + 1}  Vol + Pan",
            color=STYLE["text"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

    # ── Per-channel peak tap (downward arrow from channels) ────────────────
    tap_x = ch_x + ch_w / 2
    tap_y_top = ch_y_start - 5 * ch_h - 4 * ch_gap - 0.15
    tap_y_bot = tap_y_top - 0.8

    ax.annotate(
        "",
        xy=(tap_x, tap_y_bot),
        xytext=(tap_x, tap_y_top),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent3"],
            "lw": 1.5,
            "linestyle": "dashed",
        },
    )
    ax.text(
        tap_x,
        tap_y_bot - 0.25,
        "Channel\nPeak Meters",
        color=STYLE["accent3"],
        fontsize=7.5,
        ha="center",
        va="top",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # ── Arrow from channels to mute/solo gate ──────────────────────────────
    gate_x = 3.4
    gate_w = 2.0
    gate_h = 1.4
    gate_y = 2.5 - gate_h / 2

    arrow_props = {
        "arrowstyle": "->,head_width=0.2,head_length=0.1",
        "color": STYLE["text_dim"],
        "lw": 2,
    }
    ax.annotate(
        "",
        xy=(gate_x, 2.5),
        xytext=(ch_x + ch_w + 0.1, 2.5),
        arrowprops=arrow_props,
    )

    # ── Mute/Solo Gate box ─────────────────────────────────────────────────
    gate_rect = mpatches.FancyBboxPatch(
        (gate_x, gate_y),
        gate_w,
        gate_h,
        boxstyle="round,pad=0.12",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        lw=2,
        zorder=2,
    )
    ax.add_patch(gate_rect)
    ax.text(
        gate_x + gate_w / 2,
        2.5 + 0.2,
        "Mute / Solo",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
    )
    ax.text(
        gate_x + gate_w / 2,
        2.5 - 0.25,
        "Gate",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    # ── Pipeline stages after the gate ─────────────────────────────────────
    stages = [
        (6.3, "Sum", "\u03a3 additive"),
        (8.5, "Master\nVolume", "gain"),
        (10.7, "tanh\nSoft Clip", "[-1, 1]"),
    ]
    stage_w = 1.8
    stage_h = 1.4
    stage_colors = [STYLE["accent1"], STYLE["warn"], STYLE["accent2"]]

    prev_x_end = gate_x + gate_w

    for (sx, title, subtitle), scolor in zip(stages, stage_colors, strict=True):
        sy = 2.5 - stage_h / 2

        # Arrow into this stage
        ax.annotate(
            "",
            xy=(sx, 2.5),
            xytext=(prev_x_end + 0.1, 2.5),
            arrowprops=arrow_props,
        )

        rect = mpatches.FancyBboxPatch(
            (sx, sy),
            stage_w,
            stage_h,
            boxstyle="round,pad=0.12",
            facecolor=STYLE["surface"],
            edgecolor=scolor,
            lw=2,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            sx + stage_w / 2,
            2.5 + 0.2,
            title,
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )
        ax.text(
            sx + stage_w / 2,
            2.5 - 0.35,
            subtitle,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

        prev_x_end = sx + stage_w

    # ── Output box ─────────────────────────────────────────────────────────
    out_x = 13.0
    out_w = 0.8
    out_h = 1.0
    out_y = 2.5 - out_h / 2

    ax.annotate(
        "",
        xy=(out_x, 2.5),
        xytext=(prev_x_end + 0.1, 2.5),
        arrowprops=arrow_props,
    )

    rect = mpatches.FancyBboxPatch(
        (out_x, out_y),
        out_w,
        out_h,
        boxstyle="round,pad=0.08",
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["accent3"],
        lw=2,
        alpha=0.3,
        zorder=2,
    )
    ax.add_patch(rect)
    border = mpatches.FancyBboxPatch(
        (out_x, out_y),
        out_w,
        out_h,
        boxstyle="round,pad=0.08",
        fill=False,
        edgecolor=STYLE["accent3"],
        lw=2,
        zorder=3,
    )
    ax.add_patch(border)
    ax.text(
        out_x + out_w / 2,
        2.5,
        "Out",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    # ── Master peak tap (downward from tanh box) ───────────────────────────
    tanh_x = 10.7
    tanh_cx = tanh_x + stage_w / 2
    tanh_bot = 2.5 - stage_h / 2 - 0.15

    ax.annotate(
        "",
        xy=(tanh_cx, tanh_bot - 0.8),
        xytext=(tanh_cx, tanh_bot),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent3"],
            "lw": 1.5,
            "linestyle": "dashed",
        },
    )
    ax.text(
        tanh_cx,
        tanh_bot - 1.1,
        "Master\nPeak Meter",
        color=STYLE["accent3"],
        fontsize=7.5,
        ha="center",
        va="top",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    fig.suptitle(
        "Mixer Signal Chain",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "audio/03-audio-mixing", "mixer_signal_chain.png")


# ---------------------------------------------------------------------------
# audio/03-audio-mixing — soft_clipping.png
# ---------------------------------------------------------------------------


def diagram_soft_clipping():
    """Compare tanh soft clipping with hard clipping (clamping)."""
    fig, ax = plt.subplots(figsize=(8, 6), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-3.5, 3.5), ylim=(-1.5, 1.5), grid=True, aspect=None)

    x = np.linspace(-3.5, 3.5, 500)

    # Hard clip: clamp to [-1, 1]
    hard = np.clip(x, -1.0, 1.0)

    # Soft clip: tanh
    soft = np.tanh(x)

    # Unity line (input = output)
    unity_range = np.linspace(-1.0, 1.0, 100)
    ax.plot(
        unity_range,
        unity_range,
        color=STYLE["grid"],
        lw=1,
        linestyle=":",
        alpha=0.6,
        zorder=1,
    )

    # Plot curves
    ax.plot(
        x, hard, color=STYLE["accent2"], lw=2.5, label="Hard clip (clamp)", zorder=3
    )
    ax.plot(x, soft, color=STYLE["accent1"], lw=2.5, label="Soft clip (tanh)", zorder=4)

    # Shade the near-identity region
    mask = np.abs(x) <= 0.3
    ax.fill_between(
        x[mask],
        soft[mask] - 0.08,
        soft[mask] + 0.08,
        alpha=0.15,
        color=STYLE["accent3"],
        zorder=2,
    )
    ax.text(
        0.0,
        -0.25,
        "near-identity\nregion",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="top",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Annotations for key properties
    ax.annotate(
        "smooth\nsaturation",
        xy=(2.0, np.tanh(2.0)),
        xytext=(2.8, 0.55),
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent1"],
            "lw": 1.2,
        },
        path_effects=_STROKE,
    )

    ax.annotate(
        "harsh\nedge",
        xy=(1.0, 1.0),
        xytext=(1.8, 1.3),
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent2"],
            "lw": 1.2,
        },
        path_effects=_STROKE,
    )

    # Axis labels
    ax.set_xlabel("Input amplitude", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("Output amplitude", color=STYLE["axis"], fontsize=10)

    # Clipping bounds
    ax.axhline(y=1.0, color=STYLE["text_dim"], lw=0.8, linestyle="--", alpha=0.5)
    ax.axhline(y=-1.0, color=STYLE["text_dim"], lw=0.8, linestyle="--", alpha=0.5)

    # Legend
    leg = ax.legend(
        loc="lower right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    ax.set_title(
        "Soft Clipping vs Hard Clipping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "audio/03-audio-mixing", "soft_clipping.png")


# ---------------------------------------------------------------------------
# audio/03-audio-mixing — peak_hold_behavior.png
# ---------------------------------------------------------------------------


def diagram_peak_hold_behavior():
    """Time-domain plot showing instantaneous peak, peak hold, and decay."""
    fig, ax = plt.subplots(figsize=(10, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 4.0), ylim=(-0.05, 1.15), grid=True, aspect=None)

    # Simulate a signal with bursts
    np.random.seed(42)
    t = np.linspace(0, 4.0, 2000)
    dt = t[1] - t[0]

    # Create a bursty envelope
    envelope = np.zeros_like(t)
    # Burst 1: t=0.2 to 0.6
    mask1 = (t >= 0.2) & (t <= 0.6)
    envelope[mask1] = 0.7 * np.sin(np.pi * (t[mask1] - 0.2) / 0.4)
    # Burst 2: t=0.8 to 1.4 (louder)
    mask2 = (t >= 0.8) & (t <= 1.4)
    envelope[mask2] = 0.95 * np.sin(np.pi * (t[mask2] - 0.8) / 0.6)
    # Burst 3: t=1.8 to 2.3
    mask3 = (t >= 1.8) & (t <= 2.3)
    envelope[mask3] = 0.6 * np.sin(np.pi * (t[mask3] - 1.8) / 0.5)
    # Burst 4: t=2.8 to 3.4 (moderate)
    mask4 = (t >= 2.8) & (t <= 3.4)
    envelope[mask4] = 0.75 * np.sin(np.pi * (t[mask4] - 2.8) / 0.6)

    # Add noise to make it look like real peak detection
    noise = np.random.uniform(0.0, 0.08, len(t))
    peak_instant = np.maximum(envelope + noise * (envelope > 0.05), 0.0)
    # Clamp to 1.0
    peak_instant = np.minimum(peak_instant, 1.0)

    # Simulate peak hold with decay
    # Must match FORGE_AUDIO_PEAK_HOLD_TIME in common/audio/forge_audio.h
    hold_time = 1.5  # seconds
    decay_rate = 1.0 / hold_time
    peak_hold = np.zeros_like(t)
    current_hold = 0.0
    hold_timer = 0.0

    for i in range(len(t)):
        if peak_instant[i] >= current_hold:
            current_hold = peak_instant[i]
            hold_timer = hold_time
        else:
            if hold_timer > 0:
                if dt >= hold_timer:
                    residual = dt - hold_timer
                    hold_timer = 0.0
                    current_hold -= decay_rate * residual
                    if current_hold < 0:
                        current_hold = 0.0
                else:
                    hold_timer -= dt
            else:
                current_hold -= decay_rate * dt
                if current_hold < 0:
                    current_hold = 0.0
        peak_hold[i] = current_hold

    # Plot
    ax.fill_between(
        t,
        0,
        peak_instant,
        alpha=0.25,
        color=STYLE["accent3"],
        zorder=2,
        label="Instantaneous peak",
    )
    ax.plot(
        t,
        peak_instant,
        color=STYLE["accent3"],
        lw=0.8,
        alpha=0.7,
        zorder=3,
    )
    ax.plot(
        t,
        peak_hold,
        color=STYLE["accent2"],
        lw=2.5,
        zorder=4,
        label="Peak hold (with decay)",
    )

    # Annotate the hold plateau and decay slope
    ax.annotate(
        "hold plateau\n(1.5s timer)",
        xy=(1.8, 0.95),
        xytext=(2.3, 1.08),
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent2"],
            "lw": 1.2,
        },
        path_effects=_STROKE,
    )

    # Find a point on the actual decay slope
    decay_idx = None
    for idx in range(len(t) - 1):
        if peak_hold[idx] > 0.05 and peak_hold[idx + 1] < peak_hold[idx] - 0.001:
            decay_idx = idx
            break
    if decay_idx is not None:
        ann_x = t[decay_idx]
        ann_y = peak_hold[decay_idx]
    else:
        ann_x = 2.6
        ann_y = 0.45

    ax.annotate(
        "linear decay",
        xy=(ann_x, ann_y),
        xytext=(ann_x + 0.4, ann_y - 0.25),
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.2,
        },
        path_effects=_STROKE,
    )

    # Axis labels
    ax.set_xlabel("Time (seconds)", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("Level", color=STYLE["axis"], fontsize=10)

    # Legend
    leg = ax.legend(
        loc="upper right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    ax.set_title(
        "Peak Metering: Hold and Decay",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "audio/03-audio-mixing", "peak_hold_behavior.png")
