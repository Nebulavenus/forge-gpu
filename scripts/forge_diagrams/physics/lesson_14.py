"""Diagrams for Physics Lesson 14 — Stacking Stability."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# physics/14-stacking-stability — baumgarte_bias.png
# ---------------------------------------------------------------------------


def diagram_baumgarte_bias():
    """Baumgarte velocity bias: penetration correction over time for different β values."""
    fig = plt.figure(figsize=(9, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(0, 1.0), ylim=(0, 0.06), grid=True, aspect="auto")

    dt = 1.0 / 60.0
    slop = 0.01
    steps = 60
    t = np.linspace(0, steps * dt, steps)

    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]
    betas = [0.1, 0.2, 0.4]
    labels = ["β = 0.1 (gentle)", "β = 0.2 (default)", "β = 0.4 (aggressive)"]

    for beta, color, label in zip(betas, colors, labels, strict=True):
        # Simulate penetration decay: each frame, bias reduces penetration
        # by bias * dt where bias = (beta/dt) * max(pen - slop, 0)
        pen = np.zeros(steps)
        pen[0] = 0.05  # initial 5 cm penetration
        for i in range(1, steps):
            excess = max(pen[i - 1] - slop, 0.0)
            bias = (beta / dt) * excess
            # Penetration decreases by bias * dt (simplified model)
            pen[i] = pen[i - 1] - bias * dt
            if pen[i] < 0:
                pen[i] = 0

        ax.plot(
            t,
            pen,
            color=color,
            linewidth=2.5,
            label=label,
            path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
        )

    # Slop threshold line
    ax.axhline(y=slop, color=STYLE["warn"], linewidth=1.5, linestyle="--", alpha=0.8)
    ax.text(
        0.85,
        slop + 0.002,
        "slop threshold",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("Time (s)", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Penetration depth (m)", color=STYLE["axis"], fontsize=11)

    ax.set_title(
        "Baumgarte Bias: Penetration Correction Rate",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    leg = ax.legend(
        loc="upper right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    fig.tight_layout()
    save(fig, "physics/14-stacking-stability", "baumgarte_bias.png")


# ---------------------------------------------------------------------------
# physics/14-stacking-stability — warm_start_convergence.png
# ---------------------------------------------------------------------------


def diagram_warm_start_convergence():
    """Solver convergence with and without warm-starting."""
    fig = plt.figure(figsize=(9, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(0, 30), ylim=(1e-4, 100), grid=True, aspect="auto")

    iterations = np.arange(1, 31)

    # Cold start: exponential decay from high residual
    cold_residual = 80.0 * np.exp(-0.15 * iterations)

    # Warm start: starts much lower (near previous solution), converges fast
    warm_residual = 3.0 * np.exp(-0.35 * iterations)

    ax.semilogy(
        iterations,
        cold_residual,
        color=STYLE["accent2"],
        linewidth=2.5,
        label="Cold start (no warm-starting)",
        path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
    )
    ax.semilogy(
        iterations,
        warm_residual,
        color=STYLE["accent3"],
        linewidth=2.5,
        label="Warm start (cached impulses)",
        path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
    )

    # Mark the "good enough" threshold
    ax.axhline(y=0.1, color=STYLE["warn"], linewidth=1.5, linestyle="--", alpha=0.7)
    ax.text(
        25,
        0.15,
        "convergence threshold",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Annotate where each crosses the threshold
    cold_cross = int(np.ceil(np.log(80.0 / 0.1) / 0.15))
    warm_cross = int(np.ceil(np.log(3.0 / 0.1) / 0.35))
    x_max = int(iterations[-1])

    # Cold-start threshold is beyond the plot range (~45 > 30), so annotate
    # at the right edge with a clipped indicator
    if cold_cross <= x_max:
        ax.annotate(
            f"~{cold_cross} iterations",
            xy=(cold_cross, 0.1),
            xytext=(cold_cross + 3, 2),
            color=STYLE["accent2"],
            fontsize=10,
            arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 1.5},
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )
    else:
        ax.annotate(
            f">{x_max} iterations",
            xy=(x_max, cold_residual[-1]),
            xytext=(x_max - 10, 2),
            color=STYLE["accent2"],
            fontsize=10,
            arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 1.5},
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )
    ax.annotate(
        f"~{warm_cross} iterations",
        xy=(warm_cross, 0.1),
        xytext=(warm_cross + 5, 0.5),
        color=STYLE["accent3"],
        fontsize=10,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent3"], "lw": 1.5},
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("Solver iterations", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Residual impulse error", color=STYLE["axis"], fontsize=11)

    ax.set_title(
        "Warm-Starting: Solver Convergence Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    leg = ax.legend(
        loc="upper right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    fig.tight_layout()
    save(fig, "physics/14-stacking-stability", "warm_start_convergence.png")
