"""Diagrams for Math Lesson 16 — Density Functions."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# math/16-density-functions — histogram_comparison.png
# ---------------------------------------------------------------------------


def diagram_histogram_comparison():
    """Compare count histogram vs density histogram for the same data."""
    rng = np.random.default_rng(42)
    data = rng.normal(loc=4.0, scale=1.5, size=200)
    data = data[(data >= 0) & (data < 8)]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 5), facecolor=STYLE["bg"])

    bins = np.arange(0, 9, 1.0)
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Count histogram (left) ---
    setup_axes(ax1, xlim=(-0.5, 8.5), ylim=(0, 60), grid=True, aspect=None)
    counts, _, _bars = ax1.hist(
        data,
        bins=bins,
        color=STYLE["accent1"],
        alpha=0.85,
        edgecolor=STYLE["bg"],
        linewidth=1.2,
    )
    ax1.set_title(
        "Count Histogram",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax1.set_xlabel("Value", color=STYLE["axis"], fontsize=10)
    ax1.set_ylabel("Count", color=STYLE["axis"], fontsize=10)

    # Annotate key bar
    peak_idx = int(np.argmax(counts))
    ax1.text(
        bins[peak_idx] + 0.5,
        counts[peak_idx] + 2,
        f"N={int(counts[peak_idx])}",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
    )

    ax1.text(
        4.0,
        52,
        "Shape depends on N and bin width",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    # --- Density histogram (right) ---
    setup_axes(ax2, xlim=(-0.5, 8.5), ylim=(0, 0.35), grid=True, aspect=None)
    ax2.hist(
        data,
        bins=bins,
        density=True,
        color=STYLE["accent2"],
        alpha=0.85,
        edgecolor=STYLE["bg"],
        linewidth=1.2,
    )
    ax2.set_title(
        "Density Histogram",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax2.set_xlabel("Value", color=STYLE["axis"], fontsize=10)
    ax2.set_ylabel("Density (probability / unit)", color=STYLE["axis"], fontsize=10)

    ax2.text(
        4.0,
        0.31,
        "Area under bars = 1.0 (always)",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
    )
    ax2.text(
        4.0,
        0.28,
        "Area = 1.0 regardless of N or bin width",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout(w_pad=3)
    save(fig, "math/16-density-functions", "histogram_comparison.png")


# ---------------------------------------------------------------------------
# math/16-density-functions — pdf_curves.png
# ---------------------------------------------------------------------------


def diagram_pdf_curves():
    """Show Gaussian and uniform probability density functions."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Gaussian PDF (left) ---
    setup_axes(ax1, xlim=(-1, 9), ylim=(0, 0.35), grid=True, aspect=None)
    x = np.linspace(-1, 9, 300)
    mu, sigma = 4.0, 1.5
    y = (1.0 / (sigma * np.sqrt(2 * np.pi))) * np.exp(-0.5 * ((x - mu) / sigma) ** 2)

    ax1.fill_between(x, y, alpha=0.3, color=STYLE["accent1"])
    ax1.plot(x, y, color=STYLE["accent1"], linewidth=2.5)

    # Mark mean and +/- 1 sigma
    ax1.axvline(mu, color=STYLE["warn"], linewidth=1, linestyle="--", alpha=0.7)
    ax1.axvline(
        mu - sigma, color=STYLE["text_dim"], linewidth=1, linestyle=":", alpha=0.6
    )
    ax1.axvline(
        mu + sigma, color=STYLE["text_dim"], linewidth=1, linestyle=":", alpha=0.6
    )

    peak_y = 1.0 / (sigma * np.sqrt(2 * np.pi))
    ax1.text(
        mu + 0.15,
        peak_y + 0.01,
        f"peak = {peak_y:.3f}",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=stroke,
    )
    ax1.text(
        mu,
        -0.025,
        r"$\mu$",
        color=STYLE["warn"],
        fontsize=11,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
    )

    ax1.set_title(
        r"Gaussian PDF ($\mu=4, \sigma=1.5$)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax1.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax1.set_ylabel("f(x)", color=STYLE["axis"], fontsize=10)

    # --- Uniform PDF (right) ---
    setup_axes(ax2, xlim=(-1, 9), ylim=(0, 0.45), grid=True, aspect=None)
    a, b = 2.0, 6.0
    height = 1.0 / (b - a)

    # Draw the rectangle
    ax2.fill_between([a, b], [height, height], alpha=0.3, color=STYLE["accent2"])
    ax2.plot([a, a], [0, height], color=STYLE["accent2"], linewidth=2.5)
    ax2.plot([b, b], [0, height], color=STYLE["accent2"], linewidth=2.5)
    ax2.plot([a, b], [height, height], color=STYLE["accent2"], linewidth=2.5)
    ax2.plot([-1, a], [0, 0], color=STYLE["accent2"], linewidth=2.5)
    ax2.plot([b, 9], [0, 0], color=STYLE["accent2"], linewidth=2.5)

    # Annotate height and area
    ax2.text(
        4.0,
        height + 0.02,
        f"f(x) = {height:.2f}",
        color=STYLE["accent2"],
        fontsize=11,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
    )
    ax2.text(
        4.0,
        height / 2,
        f"Area = {height:.2f} x {b - a:.0f} = 1.0",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
    )

    ax2.set_title(
        "Uniform PDF on [2, 6]",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax2.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax2.set_ylabel("f(x)", color=STYLE["axis"], fontsize=10)

    fig.tight_layout(w_pad=3)
    save(fig, "math/16-density-functions", "pdf_curves.png")


# ---------------------------------------------------------------------------
# math/16-density-functions — integration_area.png
# ---------------------------------------------------------------------------


def diagram_integration_area():
    """Show integration of a Gaussian — shaded area under the curve."""
    fig = plt.figure(figsize=(8, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 9), ylim=(0, 0.32), grid=True, aspect=None)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    x = np.linspace(-1, 9, 500)
    mu, sigma = 4.0, 1.5
    y = (1.0 / (sigma * np.sqrt(2 * np.pi))) * np.exp(-0.5 * ((x - mu) / sigma) ** 2)

    # Full curve
    ax.plot(x, y, color=STYLE["text_dim"], linewidth=1.5, alpha=0.5)

    # Shade mu +/- 1 sigma region
    lo1, hi1 = mu - sigma, mu + sigma
    mask1 = (x >= lo1) & (x <= hi1)
    ax.fill_between(
        x[mask1],
        y[mask1],
        alpha=0.5,
        color=STYLE["accent1"],
        label=r"$\mu \pm 1\sigma$: ~68%",
    )

    # Shade mu +/- 2 sigma region (outer parts only)
    lo2, hi2 = mu - 2 * sigma, mu + 2 * sigma
    mask_left = (x >= lo2) & (x < lo1)
    mask_right = (x > hi1) & (x <= hi2)
    ax.fill_between(
        x[mask_left],
        y[mask_left],
        alpha=0.35,
        color=STYLE["accent2"],
        label=r"$\mu \pm 2\sigma$: ~95%",
    )
    ax.fill_between(x[mask_right], y[mask_right], alpha=0.35, color=STYLE["accent2"])

    # Bold curve on top
    ax.plot(x, y, color=STYLE["accent1"], linewidth=2.5)

    # Vertical lines at sigma boundaries
    for boundary in [lo1, hi1]:
        ax.axvline(
            boundary, color=STYLE["accent1"], linewidth=1, linestyle="--", alpha=0.6
        )
    for boundary in [lo2, hi2]:
        ax.axvline(
            boundary, color=STYLE["accent2"], linewidth=1, linestyle=":", alpha=0.5
        )
    ax.axvline(mu, color=STYLE["warn"], linewidth=1.2, linestyle="--", alpha=0.7)

    # Annotations
    ax.text(
        mu,
        0.285,
        "68%",
        color=STYLE["accent1"],
        fontsize=14,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
    )
    ax.text(
        mu,
        0.05,
        r"95% within $\mu \pm 2\sigma$",
        color=STYLE["accent2"],
        fontsize=11,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
    )

    ax.set_title(
        "Integrating a Gaussian PDF — Area Under the Curve",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=14,
    )
    ax.set_xlabel("x", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("f(x) — probability density", color=STYLE["axis"], fontsize=11)

    leg = ax.legend(
        loc="upper right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    fig.tight_layout()
    save(fig, "math/16-density-functions", "integration_area.png")
