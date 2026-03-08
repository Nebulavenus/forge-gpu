"""Diagrams for gpu/12."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# GPU Lesson 12 — Shader Grid: undersampling / Nyquist aliasing
# ---------------------------------------------------------------------------


def diagram_undersampling():
    """Three-panel diagram showing adequate sampling vs undersampling.

    Demonstrates the Nyquist-Shannon sampling theorem visually:
    - Left:   High-frequency signal sampled above the Nyquist rate (correct)
    - Centre: Same signal sampled BELOW the Nyquist rate (aliased)
    - Right:  Grid analogy — adequate vs inadequate pixel density

    This connects the mathematical concept (sampling theorem) to the practical
    problem (moire on a procedural grid) that fwidth()/smoothstep() solves.
    """
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # -- Shared signal: a sine wave representing a grid-like periodic pattern --
    t_fine = np.linspace(0, 2, 1000)
    freq = 5.0  # 5 Hz signal
    signal = np.sin(2 * np.pi * freq * t_fine)

    # ── Left panel: adequate sampling (above Nyquist rate) ────────────────
    ax = axes[0]
    setup_axes(ax, xlim=(-0.05, 2.05), ylim=(-1.6, 1.6), grid=False, aspect="auto")

    ax.set_title(
        "Adequate Sampling (fs > 2f)",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Continuous signal (faint)
    ax.plot(t_fine, signal, color=STYLE["accent1"], alpha=0.35, linewidth=1.5)

    # Sample points: 12 samples/sec >> 2*5 = 10 (Nyquist rate)
    fs_good = 25
    t_good = np.linspace(0, 2, fs_good * 2 + 1)
    s_good = np.sin(2 * np.pi * freq * t_good)
    ax.plot(
        t_good,
        s_good,
        "o",
        color=STYLE["accent1"],
        markersize=5,
        zorder=5,
    )

    # Reconstructed signal from samples
    ax.plot(t_good, s_good, color=STYLE["accent1"], linewidth=2.0, zorder=4)

    ax.set_xlabel(
        "Position (world space)",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.set_ylabel(
        "Grid pattern",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.text(
        1.0,
        -1.35,
        "Samples capture the signal faithfully",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # ── Centre panel: undersampling (below Nyquist rate → alias) ──────────
    ax = axes[1]
    setup_axes(ax, xlim=(-0.05, 2.05), ylim=(-1.6, 1.6), grid=False, aspect="auto")

    ax.set_title(
        "Undersampling (fs < 2f) → Aliasing",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Original signal (faint)
    ax.plot(t_fine, signal, color=STYLE["accent1"], alpha=0.25, linewidth=1.5)

    # Too few samples: 3 samples/sec < 2*5 = 10 (below Nyquist)
    fs_bad = 3
    t_bad = np.linspace(0, 2, fs_bad * 2 + 1)
    s_bad = np.sin(2 * np.pi * freq * t_bad)
    ax.plot(
        t_bad,
        s_bad,
        "o",
        color=STYLE["accent2"],
        markersize=7,
        zorder=5,
    )

    # The alias: connecting the sparse samples shows a WRONG low-frequency wave.
    # np.interp does piecewise linear — sufficient to show the alias clearly.
    t_interp = np.linspace(0, 2, 500)
    s_interp = np.interp(t_interp, t_bad, s_bad)
    ax.plot(
        t_interp,
        s_interp,
        color=STYLE["accent2"],
        linewidth=2.5,
        linestyle="-",
        zorder=4,
        label="Perceived (alias)",
    )

    ax.set_xlabel(
        "Position (world space)",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.text(
        1.0,
        -1.35,
        "Sparse samples reconstruct a false signal (moire)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # ── Right panel: grid analogy (pixels vs grid frequency) ──────────────
    ax = axes[2]
    setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-0.5, 10.5), grid=False, aspect="equal")

    ax.set_title(
        "Grid Cells vs Pixel Density",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Draw grid lines (the signal being sampled)
    for i in range(11):
        ax.axhline(y=i, color=STYLE["accent1"], linewidth=0.8, alpha=0.5, zorder=1)
        ax.axvline(x=i, color=STYLE["accent1"], linewidth=0.8, alpha=0.5, zorder=1)

    # Top half: dense pixel grid (well-sampled) — small dots
    for px in np.arange(0.25, 10.0, 0.5):
        for py in np.arange(5.5, 10.0, 0.5):
            ax.plot(
                px,
                py,
                "s",
                color=STYLE["accent3"],
                markersize=2,
                alpha=0.6,
                zorder=3,
            )

    # Bottom half: sparse pixel grid (undersampled) — large dots
    for px in np.arange(0.75, 10.0, 2.5):
        for py in np.arange(0.75, 5.0, 2.5):
            ax.plot(
                px,
                py,
                "s",
                color=STYLE["accent2"],
                markersize=6,
                alpha=0.8,
                zorder=3,
            )

    # Dividing line
    ax.axhline(y=5.0, color=STYLE["text"], linewidth=1.5, linestyle="--", zorder=4)

    ax.text(
        5.0,
        7.75,
        "Close: many pixels per cell\n(above Nyquist → crisp lines)",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        5.0,
        2.25,
        "Far: few pixels per cell\n(below Nyquist → moire)",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    ax.set_xlabel("World X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("World Z", color=STYLE["axis"], fontsize=9)

    fig.tight_layout(pad=1.5)
    save(fig, "gpu/12-shader-grid", "undersampling.png")


# ---------------------------------------------------------------------------
# gpu/14-environment-mapping — reflection_mapping.png
# ---------------------------------------------------------------------------
