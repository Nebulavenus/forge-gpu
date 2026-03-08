"""Diagrams for math/14."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save

# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — sampling_comparison.png
# ---------------------------------------------------------------------------


def _halton(index, base):
    """Radical inverse in the given base."""
    result = 0.0
    fraction = 1.0 / base
    i = index
    while i > 0:
        result += (i % base) * fraction
        i //= base
        fraction /= base
    return result


def _star_discrepancy(xs, ys):
    """Brute-force star discrepancy (Python version for diagrams)."""
    n = len(xs)
    max_disc = 0.0
    # Test rectangles [0,u]×[0,v] at each sample point (inclusive comparisons)
    for i in range(n):
        u, v = xs[i], ys[i]
        inside = np.sum((xs <= u) & (ys <= v))
        disc = abs(inside / n - u * v)
        if disc > max_disc:
            max_disc = disc
    # Test boundary rectangles with u=1 (full width)
    for i in range(n):
        inside = np.sum(ys <= ys[i])
        disc = abs(inside / n - ys[i])
        if disc > max_disc:
            max_disc = disc
    # Test boundary rectangles with v=1 (full height)
    for i in range(n):
        inside = np.sum(xs <= xs[i])
        disc = abs(inside / n - xs[i])
        if disc > max_disc:
            max_disc = disc
    # Test corner rectangle [0,1]×[0,1] — should be 0 but include for completeness
    disc = abs(1.0 - 1.0)  # all n points inside, area = 1
    if disc > max_disc:
        max_disc = disc
    return max_disc


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — radical_inverse.png
# ---------------------------------------------------------------------------


def _wang_hash(key):
    """Thomas Wang integer hash (Python version for diagram generation)."""
    key = key & 0xFFFFFFFF
    key = (~key + (key << 21)) & 0xFFFFFFFF
    key = key ^ (key >> 24)
    key = ((key + (key << 3)) + (key << 8)) & 0xFFFFFFFF
    key = key ^ (key >> 14)
    key = ((key + (key << 2)) + (key << 4)) & 0xFFFFFFFF
    key = key ^ (key >> 28)
    key = (key + (key << 31)) & 0xFFFFFFFF
    return key


def _hash_to_float_low24(h):
    """Map low 24 bits of a scalar hash to [0, 1)."""
    return (h & 0x00FFFFFF) / 16777216.0


def _r2(index):
    """R2 quasi-random sequence point."""
    alpha1 = 0.7548776662466927
    alpha2 = 0.5698402909980532
    x = (0.5 + index * alpha1) % 1.0
    y = (0.5 + index * alpha2) % 1.0
    return x, y


def _blue_noise_2d(count, candidates, seed):
    """Mitchell's best candidate blue noise."""
    xs = [_hash_to_float_low24(_wang_hash(seed))]
    ys = [_hash_to_float_low24(_wang_hash(seed ^ 0x9E3779B9))]

    for i in range(1, count):
        best_x, best_y, best_dist = 0, 0, -1
        for c in range(candidates):
            h1 = _wang_hash((seed + (i * candidates + c) * 2654435761) & 0xFFFFFFFF)
            h2 = _wang_hash(h1)
            cx, cy = _hash_to_float_low24(h1), _hash_to_float_low24(h2)

            min_dist = 1e30
            for j in range(len(xs)):
                dx = cx - xs[j]
                dy = cy - ys[j]
                if dx > 0.5:
                    dx -= 1.0
                if dx < -0.5:
                    dx += 1.0
                if dy > 0.5:
                    dy -= 1.0
                if dy < -0.5:
                    dy += 1.0
                d2 = dx * dx + dy * dy
                if d2 < min_dist:
                    min_dist = d2

            if min_dist > best_dist:
                best_dist = min_dist
                best_x, best_y = cx, cy

        xs.append(best_x)
        ys.append(best_y)

    return np.array(xs), np.array(ys)


def diagram_sampling_comparison():
    """Four-panel scatter plot: white noise, Halton, R2, blue noise."""
    n = 256

    # White noise
    rng = np.random.default_rng(42)
    wx = rng.random(n)
    wy = rng.random(n)

    # Halton (base 2, 3)
    hx = np.array([_halton(i + 1, 2) for i in range(n)])
    hy = np.array([_halton(i + 1, 3) for i in range(n)])

    # R2
    r2x = np.array([_r2(i)[0] for i in range(n)])
    r2y = np.array([_r2(i)[1] for i in range(n)])

    # Blue noise (fewer points for speed)
    bn_count = 256
    bnx, bny = _blue_noise_2d(bn_count, 20, 42)

    datasets = [
        ("White Noise", wx, wy, STYLE["accent2"]),
        ("Halton (2, 3)", hx, hy, STYLE["accent1"]),
        ("R2 Sequence", r2x, r2y, STYLE["accent3"]),
        ("Blue Noise", bnx, bny, STYLE["accent4"]),
    ]

    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5), facecolor=STYLE["bg"])

    for ax, (title, xs, ys, color) in zip(axes, datasets, strict=True):
        ax.set_facecolor(STYLE["bg"])
        ax.scatter(xs, ys, s=6, c=color, alpha=0.8, edgecolors="none")
        ax.set_xlim(0, 1)
        ax.set_ylim(0, 1)
        ax.set_aspect("equal")
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold", pad=8)
        ax.tick_params(colors=STYLE["axis"], labelsize=7)
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)
        ax.grid(True, color=STYLE["grid"], linewidth=0.3, alpha=0.4)

    fig.suptitle(
        "Sampling Distributions: 256 Points in [0, 1)²",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "sampling_comparison.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — dithering_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — dithering_comparison.png
# ---------------------------------------------------------------------------


def diagram_dithering_comparison():
    """Gradient banding vs white noise vs R1 vs blue noise dithering."""
    width = 512
    levels = 8  # Quantize to this many levels for visible banding

    # Smooth gradient [0, 1]
    gradient = np.linspace(0, 1, width).reshape(1, -1)
    gradient = np.repeat(gradient, 64, axis=0)  # Make it 64 pixels tall

    # Quantize without dithering
    banded = np.floor(gradient * levels) / levels

    # White noise dithering
    rng = np.random.default_rng(42)
    white = rng.random(gradient.shape)
    dithered_white = np.floor((gradient + (white - 0.5) / levels) * levels) / levels
    dithered_white = np.clip(dithered_white, 0, 1)

    # R1 (golden ratio) dithering — use a different offset per row too
    inv_phi = 0.6180339887498949
    r1_vals = np.zeros_like(gradient)
    for row in range(r1_vals.shape[0]):
        for col in range(r1_vals.shape[1]):
            idx = row * r1_vals.shape[1] + col
            r1_vals[row, col] = (0.5 + idx * inv_phi) % 1.0
    dithered_r1 = np.floor((gradient + (r1_vals - 0.5) / levels) * levels) / levels
    dithered_r1 = np.clip(dithered_r1, 0, 1)

    # Blue noise dithering — build a 64x64 tile via 2D Mitchell's best candidate,
    # then tile across the gradient dimensions.
    # Use _blue_noise_2d to generate 2D sample positions with toroidal distance,
    # then build a rank map: each cell gets a threshold from insertion order.
    tile_size = 64
    tile_count = tile_size * tile_size
    bnx, bny = _blue_noise_2d(tile_count, 10, 7)
    # Build rank map: map each 2D position to a grid cell, assign rank by
    # insertion order (earlier samples get lower ranks → lower thresholds)
    bn_tile = np.zeros((tile_size, tile_size))
    occupied = np.full((tile_size, tile_size), -1, dtype=int)
    for rank in range(tile_count):
        col = int(bnx[rank] * tile_size) % tile_size
        row = int(bny[rank] * tile_size) % tile_size
        # Handle collisions: find nearest unoccupied cell
        if occupied[row, col] >= 0:
            found = False
            for radius in range(1, tile_size):
                for dr in range(-radius, radius + 1):
                    for dc in range(-radius, radius + 1):
                        if abs(dr) != radius and abs(dc) != radius:
                            continue
                        nr = (row + dr) % tile_size
                        nc = (col + dc) % tile_size
                        if occupied[nr, nc] < 0:
                            row, col = nr, nc
                            found = True
                            break
                    if found:
                        break
                if found:
                    break
        occupied[row, col] = rank
        bn_tile[row, col] = rank / tile_count  # Normalize to [0, 1)
    rows, cols = gradient.shape
    blue_noise = np.tile(bn_tile, (rows // tile_size + 1, cols // tile_size + 1))[
        :rows, :cols
    ]
    dithered_blue = np.floor((gradient + (blue_noise - 0.5) / levels) * levels) / levels
    dithered_blue = np.clip(dithered_blue, 0, 1)

    panels = [
        ("Original gradient", gradient),
        ("Quantized (banding)", banded),
        ("White noise dithered", dithered_white),
        ("R1 (golden ratio) dithered", dithered_r1),
        ("Blue noise dithered", dithered_blue),
    ]

    fig, axes = plt.subplots(5, 1, figsize=(12, 6.25), facecolor=STYLE["bg"])

    for ax, (title, data) in zip(axes, panels, strict=True):
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            data, cmap="gray", vmin=0, vmax=1, aspect="auto", interpolation="nearest"
        )
        ax.set_ylabel(
            title,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            rotation=0,
            ha="right",
            va="center",
            labelpad=10,
        )
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Dithering: Replacing Banding with Imperceptible Noise",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "dithering_comparison.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — power_spectrum.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — power_spectrum.png
# ---------------------------------------------------------------------------


def diagram_power_spectrum():
    """2D power spectrum comparison: white noise vs blue noise."""
    size = 128

    # White noise
    rng = np.random.default_rng(42)
    white = rng.random((size, size))

    # Blue noise approximation via scattered points on a grid
    blue = np.zeros((size, size))
    n_points = size * size // 4
    bnx, bny = _blue_noise_2d(min(n_points, 400), 25, 42)
    for px, py in zip(bnx, bny, strict=True):
        ix = int(px * (size - 1))
        iy = int(py * (size - 1))
        if 0 <= ix < size and 0 <= iy < size:
            blue[iy, ix] = 1.0

    # Compute 2D FFT magnitude (power spectrum)
    white_fft = np.abs(np.fft.fftshift(np.fft.fft2(white - white.mean()))) ** 2
    blue_fft = np.abs(np.fft.fftshift(np.fft.fft2(blue - blue.mean()))) ** 2

    # Normalize for display
    white_fft = np.log1p(white_fft)
    blue_fft = np.log1p(blue_fft)
    white_fft /= white_fft.max() if white_fft.max() > 0 else 1
    blue_fft /= blue_fft.max() if blue_fft.max() > 0 else 1

    # Radial average for 1D profile
    center = size // 2
    freqs_w = np.zeros(center)
    freqs_b = np.zeros(center)
    counts = np.zeros(center)

    for y in range(size):
        for x in range(size):
            r = int(np.sqrt((x - center) ** 2 + (y - center) ** 2))
            if r < center:
                freqs_w[r] += white_fft[y, x]
                freqs_b[r] += blue_fft[y, x]
                counts[r] += 1

    mask = counts > 0
    freqs_w[mask] /= counts[mask]
    freqs_b[mask] /= counts[mask]

    fig = plt.figure(figsize=(14, 5), facecolor=STYLE["bg"])

    # White noise spectrum
    ax1 = fig.add_subplot(131)
    ax1.set_facecolor(STYLE["bg"])
    ax1.imshow(white_fft, cmap="inferno", interpolation="bilinear")
    ax1.set_title(
        "White Noise Spectrum",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    ax1.set_xticks([])
    ax1.set_yticks([])
    for spine in ax1.spines.values():
        spine.set_color(STYLE["grid"])

    # Blue noise spectrum
    ax2 = fig.add_subplot(132)
    ax2.set_facecolor(STYLE["bg"])
    ax2.imshow(blue_fft, cmap="inferno", interpolation="bilinear")
    ax2.set_title(
        "Blue Noise Spectrum",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])
    for spine in ax2.spines.values():
        spine.set_color(STYLE["grid"])

    # Radial profile
    ax3 = fig.add_subplot(133)
    ax3.set_facecolor(STYLE["bg"])
    ax3.plot(
        freqs_w, color=STYLE["accent2"], linewidth=2, label="White noise", alpha=0.8
    )
    ax3.plot(
        freqs_b, color=STYLE["accent1"], linewidth=2, label="Blue noise", alpha=0.8
    )
    ax3.set_xlabel("Frequency", color=STYLE["axis"], fontsize=10)
    ax3.set_ylabel("Power", color=STYLE["axis"], fontsize=10)
    ax3.set_title(
        "Radial Power Profile",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    ax3.tick_params(colors=STYLE["axis"], labelsize=8)
    ax3.legend(
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=9,
    )
    for spine in ax3.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax3.grid(True, color=STYLE["grid"], linewidth=0.3, alpha=0.4)

    fig.suptitle(
        "Power Spectrum: White Noise vs Blue Noise",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "power_spectrum.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — discrepancy_convergence.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — discrepancy_convergence.png
# ---------------------------------------------------------------------------


def diagram_discrepancy_convergence():
    """Log-log plot of star discrepancy vs sample count."""
    sample_counts = [8, 16, 32, 64, 128, 256]

    d_random = []
    d_halton = []
    d_r2 = []

    rng = np.random.default_rng(42)

    for n in sample_counts:
        # Random
        rx = rng.random(n)
        ry = rng.random(n)
        d_random.append(_star_discrepancy(rx, ry))

        # Halton
        hx = np.array([_halton(i + 1, 2) for i in range(n)])
        hy = np.array([_halton(i + 1, 3) for i in range(n)])
        d_halton.append(_star_discrepancy(hx, hy))

        # R2
        pts = [_r2(i) for i in range(n)]
        r2x = np.array([p[0] for p in pts])
        r2y = np.array([p[1] for p in pts])
        d_r2.append(_star_discrepancy(r2x, r2y))

    fig, ax = plt.subplots(figsize=(8, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])

    ax.loglog(
        sample_counts,
        d_random,
        "o-",
        color=STYLE["accent2"],
        linewidth=2,
        markersize=6,
        label="White noise",
    )
    ax.loglog(
        sample_counts,
        d_halton,
        "s-",
        color=STYLE["accent1"],
        linewidth=2,
        markersize=6,
        label="Halton (2, 3)",
    )
    ax.loglog(
        sample_counts,
        d_r2,
        "D-",
        color=STYLE["accent3"],
        linewidth=2,
        markersize=6,
        label="R2",
    )

    # Reference lines
    ns = np.array(sample_counts, dtype=float)
    ax.loglog(
        ns,
        0.8 / np.sqrt(ns),
        "--",
        color=STYLE["text_dim"],
        linewidth=1,
        alpha=0.5,
        label=r"$O(1/\sqrt{N})$ (random)",
    )
    ax.loglog(
        ns,
        2.0 / ns,
        "--",
        color=STYLE["warn"],
        linewidth=1,
        alpha=0.5,
        label=r"$O(1/N)$ (optimal)",
    )

    ax.set_xlabel("Number of samples (N)", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Star discrepancy D*", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Discrepancy vs Sample Count",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.tick_params(colors=STYLE["axis"], labelsize=9)
    ax.legend(
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=9,
    )
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.3, alpha=0.4, which="both")

    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "discrepancy_convergence.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — radical_inverse.png
# ---------------------------------------------------------------------------


def diagram_radical_inverse():
    """Visualization of the radical inverse filling [0,1) progressively."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 4), facecolor=STYLE["bg"])

    counts = [4, 8, 16]
    for ax, n in zip(axes, counts, strict=True):
        ax.set_facecolor(STYLE["bg"])

        # Plot the Halton base-2 points on a number line
        vals = [_halton(i + 1, 2) for i in range(n)]

        # Show number line
        ax.axhline(0, color=STYLE["grid"], linewidth=1, alpha=0.6)

        # Plot points with labels for small n
        for i, v in enumerate(vals):
            ax.plot(v, 0, "o", color=STYLE["accent1"], markersize=8, zorder=5)
            if n <= 8:
                ax.annotate(
                    f"{i + 1}",
                    xy=(v, 0),
                    xytext=(0, 12),
                    textcoords="offset points",
                    ha="center",
                    color=STYLE["text"],
                    fontsize=8,
                    fontweight="bold",
                    path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
                )

        ax.set_xlim(-0.05, 1.05)
        ax.set_ylim(-0.3, 0.5)
        ax.set_title(
            f"n = {n} points",
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            pad=8,
        )
        ax.set_xlabel("[0, 1)", color=STYLE["axis"], fontsize=10)
        ax.tick_params(colors=STYLE["axis"], labelsize=8, left=False, labelleft=False)
        for spine in ["left", "top", "right"]:
            ax.spines[spine].set_visible(False)
        ax.spines["bottom"].set_color(STYLE["grid"])
        ax.spines["bottom"].set_linewidth(0.5)

    fig.suptitle(
        "Radical Inverse (Base 2): Progressive Gap-Filling",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/14-blue-noise-sequences", "radical_inverse.png")


# ---------------------------------------------------------------------------
# math/02-coordinate-spaces — 3-D coordinate space visualizations
# ---------------------------------------------------------------------------
#
# A simple 3-D house (box body + triangular-prism roof) is transformed
# through the six-stage rendering pipeline.  Each diagram renders the
# house with flat-shaded faces so learners can follow it from local
# space all the way to screen pixels.  World-space diagrams include a
# yard (ground plane) and road for spatial context.
