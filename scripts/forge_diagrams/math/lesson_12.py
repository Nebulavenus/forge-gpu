"""Diagrams for math/12."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# math/12-hash-functions — white_noise_comparison.png
# ---------------------------------------------------------------------------

# Python implementations of the three hash functions from forge_math.h.
# These use np.uint32 arrays for correct 32-bit unsigned overflow.


def _hash_wang(key):
    """Wang hash (Thomas Wang 2007) — vectorised uint32."""
    k = np.asarray(key, dtype=np.uint32)
    k = (k ^ np.uint32(61)) ^ (k >> np.uint32(16))
    k = k * np.uint32(9)
    k = k ^ (k >> np.uint32(4))
    k = k * np.uint32(0x27D4EB2D)
    k = k ^ (k >> np.uint32(15))
    return k


def _hash2d(x, y, hash_fn=_hash_wang):
    """Cascaded 2D hash: hash(x ^ hash(y))."""
    return hash_fn(np.asarray(x, dtype=np.uint32) ^ hash_fn(y))


def _hash_xxhash32(h):
    """xxHash32 avalanche finaliser — vectorised uint32."""
    h = np.asarray(h, dtype=np.uint32)
    h = h ^ (h >> np.uint32(15))
    h = h * np.uint32(0x85EBCA77)
    h = h ^ (h >> np.uint32(13))
    h = h * np.uint32(0xC2B2AE3D)
    h = h ^ (h >> np.uint32(16))
    return h


def _hash_to_float(h):
    """Convert uint32 hash to float in [0, 1)."""
    return (h >> np.uint32(8)).astype(np.float64) / 16777216.0


def _hash_pcg(inp):
    """PCG output permutation hash — vectorised uint32."""
    s = np.asarray(inp, dtype=np.uint32)
    s = s * np.uint32(747796405) + np.uint32(2891336453)
    shift = (s >> np.uint32(28)) + np.uint32(4)
    # Element-wise variable shift using NumPy's right_shift ufunc
    word = np.right_shift(s, shift) ^ s
    word = word * np.uint32(277803737)
    return (word >> np.uint32(22)) ^ word


def diagram_white_noise_comparison():
    """Side-by-side white noise grids for Wang, PCG, and xxHash32."""
    w, h = 256, 256

    # Create 2D coordinate grids
    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.uint32), np.arange(w, dtype=np.uint32), indexing="ij"
    )

    noise_wang = _hash_to_float(_hash2d(xx, yy, _hash_wang))
    noise_pcg = _hash_to_float(_hash2d(xx, yy, _hash_pcg))
    noise_xx = _hash_to_float(_hash2d(xx, yy, _hash_xxhash32))

    fig, axes = plt.subplots(1, 3, figsize=(12, 4.5), facecolor=STYLE["bg"])
    titles = ["Wang Hash", "PCG Hash", "xxHash32"]
    noises = [noise_wang, noise_pcg, noise_xx]

    for ax, title, noise in zip(axes, titles, noises, strict=True):
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(noise, cmap="gray", vmin=0, vmax=1, interpolation="nearest")
        ax.set_title(title, color=STYLE["text"], fontsize=12, fontweight="bold", pad=8)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "White Noise: hash2d(x, y) for Each Hash Function",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/12-hash-functions", "white_noise_comparison.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — avalanche_matrix.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/12-hash-functions — avalanche_matrix.png
# ---------------------------------------------------------------------------


def diagram_avalanche_matrix():
    """32x32 avalanche matrix heatmap: P(output bit j flips | input bit i flips)."""
    n_samples = 4096
    inputs = np.arange(n_samples, dtype=np.uint32)

    # Build 32x32 matrix: for each input bit i, measure flip probability of each
    # output bit j over many sample inputs
    matrix = np.zeros((32, 32), dtype=np.float64)

    base_hashes = _hash_wang(inputs)
    for i in range(32):
        flipped_inputs = inputs ^ np.uint32(1 << i)
        flipped_hashes = _hash_wang(flipped_inputs)
        diff = base_hashes ^ flipped_hashes
        for j in range(32):
            bit_changed = (diff >> np.uint32(j)) & np.uint32(1)
            matrix[i, j] = np.mean(bit_changed.astype(np.float64))

    fig = plt.figure(figsize=(7, 6.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    # Use a diverging colormap centered on 0.5 (the ideal value)
    im = ax.imshow(
        matrix,
        cmap="RdYlGn",
        vmin=0.35,
        vmax=0.65,
        interpolation="nearest",
        aspect="equal",
    )

    ax.set_xlabel("Output bit", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Input bit flipped", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Avalanche Matrix: Wang Hash\nP(output bit j flips | input bit i flips)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax.tick_params(colors=STYLE["axis"], labelsize=7)

    # Tick every 4 bits
    ticks = list(range(0, 32, 4))
    ax.set_xticks(ticks)
    ax.set_yticks(ticks)

    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_label("Flip probability (ideal = 0.50)", color=STYLE["axis"], fontsize=10)
    cbar.ax.tick_params(colors=STYLE["axis"], labelsize=8)
    cbar.outline.set_edgecolor(STYLE["grid"])  # type: ignore[reportCallIssue]

    # Annotate the ideal line
    avg = np.mean(matrix)
    ax.text(
        0.02,
        0.02,
        f"Mean flip probability: {avg:.3f}  (ideal: 0.500)",
        transform=ax.transAxes,
        color=STYLE["text"],
        fontsize=9,
        ha="left",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "math/12-hash-functions", "avalanche_matrix.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — distribution_histogram.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/12-hash-functions — distribution_histogram.png
# ---------------------------------------------------------------------------


def diagram_distribution_histogram():
    """Histogram comparing output distribution of three hash functions."""
    n = 100000
    inputs = np.arange(n, dtype=np.uint32)
    n_bins = 50

    values_wang = _hash_to_float(_hash_wang(inputs))
    values_pcg = _hash_to_float(_hash_pcg(inputs))
    values_xx = _hash_to_float(_hash_xxhash32(inputs))

    fig, axes = plt.subplots(1, 3, figsize=(13, 4), facecolor=STYLE["bg"])
    datasets = [
        ("Wang Hash", values_wang, STYLE["accent1"]),
        ("PCG Hash", values_pcg, STYLE["accent2"]),
        ("xxHash32", values_xx, STYLE["accent3"]),
    ]

    expected = n / n_bins

    for ax, (title, data, color) in zip(axes, datasets, strict=True):
        ax.set_facecolor(STYLE["bg"])
        counts, edges, _ = ax.hist(
            data, bins=n_bins, range=(0, 1), color=color, alpha=0.75, edgecolor="none"
        )
        ax.axhline(
            y=expected,
            color=STYLE["warn"],
            lw=1.5,
            ls="--",
            label=f"Expected ({int(expected)})",
        )
        ax.set_title(title, color=STYLE["text"], fontsize=12, fontweight="bold", pad=8)
        ax.set_xlabel("Hash output [0, 1)", color=STYLE["axis"], fontsize=9)
        ax.set_ylabel("Count", color=STYLE["axis"], fontsize=9)
        ax.tick_params(colors=STYLE["axis"], labelsize=8)
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

        # Stats
        deviation = np.std(counts - expected)
        ax.text(
            0.98,
            0.95,
            f"Std dev: {deviation:.1f}",
            transform=ax.transAxes,
            color=STYLE["text"],
            fontsize=8,
            ha="right",
            va="top",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )
        ax.legend(
            fontsize=8,
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            labelcolor=STYLE["text"],
            loc="upper left",
        )

    fig.suptitle(
        f"Distribution Uniformity: {n:,} Sequential Inputs",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/12-hash-functions", "distribution_histogram.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — hash_pipeline.png
# ---------------------------------------------------------------------------


def diagram_hash_pipeline():
    """Visual flow of the Wang hash pipeline showing each mixing step."""
    fig = plt.figure(figsize=(10, 4), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.8, 10.5), ylim=(-1.5, 2.5), grid=False)
    ax.axis("off")

    steps = [
        ("Input\nkey", STYLE["text"]),
        ("XOR\nkey^61\n^(key>>16)", STYLE["accent1"]),
        ("MUL\nkey*9", STYLE["accent2"]),
        ("XOR\nkey^=\nkey>>4", STYLE["accent1"]),
        ("MUL\nkey*=\n0x27d4eb2d", STYLE["accent2"]),
        ("XOR\nkey^=\nkey>>15", STYLE["accent1"]),
        ("Output\nhash", STYLE["accent3"]),
    ]

    box_w = 1.2
    spacing = 1.5
    y = 0.5

    for i, (label, color) in enumerate(steps):
        x = i * spacing
        rect = Rectangle(
            (x - box_w / 2, y - 0.6),
            box_w,
            1.2,
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            x,
            y,
            label,
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=3,
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Arrow to next step
        if i < len(steps) - 1:
            ax.annotate(
                "",
                xy=((i + 1) * spacing - box_w / 2 - 0.05, y),
                xytext=(x + box_w / 2 + 0.05, y),
                arrowprops={
                    "arrowstyle": "->,head_width=0.2,head_length=0.1",
                    "color": STYLE["text_dim"],
                    "lw": 1.5,
                },
                zorder=1,
            )

    # Purpose labels below
    purposes = [
        "",
        "fold upper\ninto lower",
        "spread via\ncarry chain",
        "fold multiply\nresult",
        "full-width\nbit spread",
        "final\navalanche",
        "",
    ]
    for i, purpose in enumerate(purposes):
        if purpose:
            ax.text(
                i * spacing,
                y - 1.1,
                purpose,
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="top",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

    ax.set_title(
        "Wang Hash Pipeline: How Each Step Mixes Bits",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=15,
    )

    fig.tight_layout()
    save(fig, "math/12-hash-functions", "hash_pipeline.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — gradient_noise_concept.png
# ---------------------------------------------------------------------------
