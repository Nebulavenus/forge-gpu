"""Diagrams for math/13."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

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


# ---------------------------------------------------------------------------
# math/12-hash-functions — hash_pipeline.png
# ---------------------------------------------------------------------------


def _hash3d(x, y, z):
    """Cascaded 3D hash: hash(x ^ hash(y ^ hash(z)))."""
    return _hash_wang(
        np.asarray(x, dtype=np.uint32)
        ^ _hash_wang(np.asarray(y, dtype=np.uint32) ^ _hash_wang(z))
    )


def _noise_fade(t):
    """Perlin's quintic fade curve."""
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def _noise_grad2d(hash_val, dx, dy):
    """2D gradient dot product (4-gradient set)."""
    h = np.asarray(hash_val, dtype=np.uint32) & np.uint32(3)
    # gx: +1 for cases 0,2; -1 for cases 1,3 (bit 0 controls sign)
    gx = np.where(h & np.uint32(1), -1.0, 1.0)
    # gy: +1 for cases 0,1; -1 for cases 2,3 (bit 1 controls sign)
    gy = np.where(h & np.uint32(2), -1.0, 1.0)
    return gx * dx + gy * dy


def _simplex2d(x, y, seed):
    """2D simplex noise (vectorised NumPy)."""
    x = np.asarray(x, dtype=np.float64)
    y = np.asarray(y, dtype=np.float64)

    F2 = 0.36602540378
    G2 = 0.21132486540

    s = (x + y) * F2
    i = np.floor(x + s).astype(np.int32)
    j = np.floor(y + s).astype(np.int32)

    t = (i + j).astype(np.float64) * G2
    x0 = x - (i.astype(np.float64) - t)
    y0 = y - (j.astype(np.float64) - t)

    i1 = np.where(x0 > y0, 1, 0).astype(np.int32)
    j1 = np.where(x0 > y0, 0, 1).astype(np.int32)

    x1 = x0 - i1.astype(np.float64) + G2
    y1 = y0 - j1.astype(np.float64) + G2
    x2 = x0 - 1.0 + 2.0 * G2
    y2 = y0 - 1.0 + 2.0 * G2

    ui = i.astype(np.uint32)
    uj = j.astype(np.uint32)
    ss = np.uint32(seed)

    h0 = _hash3d(ui, uj, ss)
    h1 = _hash3d(ui + i1.astype(np.uint32), uj + j1.astype(np.uint32), ss)
    h2 = _hash3d(ui + np.uint32(1), uj + np.uint32(1), ss)

    t0 = np.maximum(0.5 - x0 * x0 - y0 * y0, 0.0)
    t1 = np.maximum(0.5 - x1 * x1 - y1 * y1, 0.0)
    t2 = np.maximum(0.5 - x2 * x2 - y2 * y2, 0.0)

    n0 = t0 * t0 * t0 * t0 * _noise_grad2d(h0, x0, y0)
    n1 = t1 * t1 * t1 * t1 * _noise_grad2d(h1, x1, y1)
    n2 = t2 * t2 * t2 * t2 * _noise_grad2d(h2, x2, y2)

    return 70.0 * (n0 + n1 + n2)


def _perlin2d(x, y, seed):
    """2D Perlin gradient noise (vectorised NumPy)."""
    x = np.asarray(x, dtype=np.float64)
    y = np.asarray(y, dtype=np.float64)

    ix = np.floor(x).astype(np.int32)
    iy = np.floor(y).astype(np.int32)
    fx = x - ix.astype(np.float64)
    fy = y - iy.astype(np.float64)

    u = _noise_fade(fx)
    v = _noise_fade(fy)

    uix = ix.astype(np.uint32)
    uiy = iy.astype(np.uint32)
    s = np.uint32(seed)

    h00 = _hash3d(uix, uiy, s)
    h10 = _hash3d(uix + np.uint32(1), uiy, s)
    h01 = _hash3d(uix, uiy + np.uint32(1), s)
    h11 = _hash3d(uix + np.uint32(1), uiy + np.uint32(1), s)

    g00 = _noise_grad2d(h00, fx, fy)
    g10 = _noise_grad2d(h10, fx - 1.0, fy)
    g01 = _noise_grad2d(h01, fx, fy - 1.0)
    g11 = _noise_grad2d(h11, fx - 1.0, fy - 1.0)

    x0 = g00 + u * (g10 - g00)
    x1 = g01 + u * (g11 - g01)
    return x0 + v * (x1 - x0)


def _hash2d(x, y, hash_fn=_hash_wang):
    """Cascaded 2D hash: hash(x ^ hash(y))."""
    return hash_fn(np.asarray(x, dtype=np.uint32) ^ hash_fn(y))


def _fbm2d(x, y, seed, octaves, lacunarity=2.0, persistence=0.5):
    """2D fractal Brownian motion (vectorised)."""
    if octaves <= 0:
        return np.zeros_like(x, dtype=np.float64)
    total = np.zeros_like(x, dtype=np.float64)
    amplitude = 1.0
    frequency = 1.0
    max_amp = 0.0
    for i in range(octaves):
        total += amplitude * _perlin2d(x * frequency, y * frequency, seed + i)
        max_amp += amplitude
        frequency *= lacunarity
        amplitude *= persistence
    return total / max_amp


def _hash_to_float(h):
    """Convert uint32 hash to float in [0, 1)."""
    return (h >> np.uint32(8)).astype(np.float64) / 16777216.0


def _domain_warp2d(x, y, seed, strength):
    """2D domain warping via fBm (vectorised)."""
    wx = _fbm2d(x, y, seed, 4)
    wy = _fbm2d(x, y, seed + 1, 4)
    return _fbm2d(x + strength * wx, y + strength * wy, seed + 2, 4)


# ---------------------------------------------------------------------------
# math/13-gradient-noise — gradient_noise_concept.png
# ---------------------------------------------------------------------------


def diagram_gradient_noise_concept():
    """Core concept: grid with gradient arrows, sample point, dot products."""
    fig = plt.figure(figsize=(9, 9), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.8, 3.8), ylim=(-0.8, 3.8))

    # Draw the integer grid
    for i in range(4):
        ax.axhline(i, color=STYLE["grid"], lw=0.6, alpha=0.5)
        ax.axvline(i, color=STYLE["grid"], lw=0.6, alpha=0.5)

    # Define gradient directions at each grid point (seeded deterministically)
    rng = np.random.default_rng(42)
    gradients_4 = [(1, 1), (-1, 1), (1, -1), (-1, -1)]
    grid_grads = {}
    for gx in range(4):
        for gy in range(4):
            idx = rng.integers(0, 4)
            grid_grads[(gx, gy)] = gradients_4[idx]

    # Draw gradient arrows at all grid points (dimmed)
    arrow_len = 0.3
    for (gx, gy), (dx, dy) in grid_grads.items():
        mag = np.sqrt(dx * dx + dy * dy)
        ndx, ndy = dx / mag * arrow_len, dy / mag * arrow_len
        ax.annotate(
            "",
            xy=(gx + ndx, gy + ndy),
            xytext=(gx, gy),
            arrowprops={
                "arrowstyle": "->,head_width=0.12,head_length=0.06",
                "color": STYLE["text_dim"],
                "lw": 1.5,
            },
        )
        ax.plot(gx, gy, "o", color=STYLE["text_dim"], markersize=5, zorder=4)

    # Highlight the cell containing the sample point
    sample_x, sample_y = 1.65, 1.35
    cell_x, cell_y = 1, 1

    # Highlight cell
    cell_rect = Rectangle(
        (cell_x, cell_y),
        1,
        1,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        alpha=0.6,
        zorder=1,
    )
    ax.add_patch(cell_rect)

    # Draw highlighted gradient arrows at the 4 corners
    corners = [(0, 0), (1, 0), (0, 1), (1, 1)]
    corner_colors = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent4"],
        STYLE["accent3"],
    ]
    corner_labels = ["g00", "g10", "g01", "g11"]

    for (cx, cy), color, label in zip(
        corners, corner_colors, corner_labels, strict=True
    ):
        gx_abs = cell_x + cx
        gy_abs = cell_y + cy
        dx, dy = grid_grads[(gx_abs, gy_abs)]
        mag = np.sqrt(dx * dx + dy * dy)
        ndx, ndy = dx / mag * 0.4, dy / mag * 0.4

        ax.annotate(
            "",
            xy=(gx_abs + ndx, gy_abs + ndy),
            xytext=(gx_abs, gy_abs),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.08",
                "color": color,
                "lw": 2.5,
            },
            zorder=6,
        )
        ax.plot(gx_abs, gy_abs, "o", color=color, markersize=8, zorder=7)

        # Label
        ox = -0.25 if cx == 0 else 0.12
        oy = -0.18 if cy == 0 else 0.12
        ax.text(
            gx_abs + ox,
            gy_abs + oy,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Sample point
    ax.plot(
        sample_x,
        sample_y,
        "*",
        color=STYLE["warn"],
        markersize=18,
        zorder=8,
        markeredgecolor="white",
        markeredgewidth=0.5,
    )
    ax.text(
        sample_x + 0.12,
        sample_y + 0.15,
        f"P ({sample_x:.2f}, {sample_y:.2f})",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Distance vectors from each corner to sample point (dashed)
    for (cx, cy), color in zip(corners, corner_colors, strict=True):
        gx_abs = cell_x + cx
        gy_abs = cell_y + cy
        ax.plot(
            [gx_abs, sample_x],
            [gy_abs, sample_y],
            "--",
            color=color,
            lw=1.0,
            alpha=0.6,
            zorder=3,
        )

    # Annotation: explain the process
    ax.text(
        3.7,
        -0.5,
        "1. Hash each corner to get gradient\n"
        "2. Dot(gradient, distance-to-P)\n"
        "3. Interpolate with fade curve",
        color=STYLE["text"],
        fontsize=9,
        ha="right",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Gradient Noise: Lattice Gradients + Dot Products + Interpolation",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "gradient_noise_concept.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — fade_curves.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/13-gradient-noise — fade_curves.png
# ---------------------------------------------------------------------------


def diagram_fade_curves():
    """Compare linear, smoothstep, and Perlin's quintic fade curves."""
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.05, 1.05), ylim=(-0.05, 1.15), grid=True, aspect=None)

    t = np.linspace(0, 1, 500)

    # Linear
    ax.plot(t, t, ":", color=STYLE["text_dim"], lw=1.5, label="Linear: t")

    # Smoothstep (Hermite): 3t^2 - 2t^3
    smoothstep = 3 * t**2 - 2 * t**3
    ax.plot(
        t,
        smoothstep,
        "--",
        color=STYLE["accent2"],
        lw=2.0,
        label="Smoothstep: $3t^2 - 2t^3$ (C1)",
    )

    # Quintic (Perlin improved): 6t^5 - 15t^4 + 10t^3
    quintic = 6 * t**5 - 15 * t**4 + 10 * t**3
    ax.plot(
        t,
        quintic,
        "-",
        color=STYLE["accent1"],
        lw=2.5,
        label="Quintic: $6t^5 - 15t^4 + 10t^3$ (C2)",
    )

    # Mark the endpoints
    for color in [STYLE["text_dim"], STYLE["accent2"], STYLE["accent1"]]:
        ax.plot(0, 0, "o", color=color, markersize=5, zorder=5)
        ax.plot(1, 1, "o", color=color, markersize=5, zorder=5)

    # Annotate derivatives
    ax.annotate(
        "Zero 1st & 2nd derivative\nat both endpoints",
        xy=(0.08, quintic[40]),
        xytext=(0.25, 0.15),
        color=STYLE["accent1"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 1.2},
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel(
        "t (fractional position within grid cell)", color=STYLE["axis"], fontsize=10
    )
    ax.set_ylabel("fade(t) (interpolation weight)", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Fade Curves: Why Perlin's Quintic Eliminates Grid Artifacts",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax.legend(
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        loc="upper left",
    )

    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "fade_curves.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — perlin_vs_simplex_grid.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/13-gradient-noise — perlin_vs_simplex_grid.png
# ---------------------------------------------------------------------------


def diagram_perlin_vs_simplex_grid():
    """Side-by-side: square grid (Perlin) vs triangular grid (simplex)."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5.5), facecolor=STYLE["bg"])

    # --- Left panel: Square grid (Perlin) ---
    setup_axes(ax1, xlim=(-0.5, 4.5), ylim=(-0.5, 4.5), grid=False)

    # Draw square grid
    for i in range(5):
        ax1.plot([-0.3, 4.3], [i, i], color=STYLE["grid"], lw=0.8, alpha=0.6)
        ax1.plot([i, i], [-0.3, 4.3], color=STYLE["grid"], lw=0.8, alpha=0.6)

    # Grid points
    for x in range(5):
        for y in range(5):
            ax1.plot(x, y, "o", color=STYLE["text_dim"], markersize=5, zorder=4)

    # Highlight one cell and its 4 corners
    cell_rect = Rectangle(
        (1, 1),
        1,
        1,
        facecolor=STYLE["accent1"],
        alpha=0.15,
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        zorder=2,
    )
    ax1.add_patch(cell_rect)

    sample = (1.6, 1.4)
    ax1.plot(*sample, "*", color=STYLE["warn"], markersize=14, zorder=8)

    for cx, cy in [(1, 1), (2, 1), (1, 2), (2, 2)]:
        ax1.plot(cx, cy, "o", color=STYLE["accent1"], markersize=8, zorder=6)
        ax1.plot(
            [cx, sample[0]],
            [cy, sample[1]],
            "--",
            color=STYLE["accent1"],
            lw=1,
            alpha=0.5,
            zorder=3,
        )

    ax1.text(
        2.0,
        -0.3,
        "4 corners per sample\n(bilinear interpolation)",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax1.set_title(
        "Square Grid (Perlin)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )
    ax1.set_xticks([])
    ax1.set_yticks([])

    # --- Right panel: Simplex (triangular) grid ---
    setup_axes(ax2, xlim=(-0.5, 4.5), ylim=(-0.5, 4.5), grid=False)

    # Draw triangular grid (equilateral triangles via skewing)
    G2 = (3 - np.sqrt(3)) / 6

    # Generate grid points in simplex space and unskew to display
    simplex_pts = []
    for si in range(-1, 7):
        for sj in range(-1, 7):
            # Unskew from simplex to Cartesian
            t = (si + sj) * G2
            px = si - t
            py = sj - t
            if -0.3 <= px <= 4.3 and -0.3 <= py <= 4.3:
                simplex_pts.append((px, py, si, sj))

    # Draw edges
    for px, py, si, sj in simplex_pts:
        for dsi, dsj in [(1, 0), (0, 1), (1, -1)]:
            ni, nj = si + dsi, sj + dsj
            nt = (ni + nj) * G2
            nx, ny = ni - nt, nj - nt
            if -0.3 <= nx <= 4.3 and -0.3 <= ny <= 4.3:
                ax2.plot(
                    [px, nx],
                    [py, ny],
                    color=STYLE["grid"],
                    lw=0.8,
                    alpha=0.6,
                )

    # Grid points
    for px, py, _, _ in simplex_pts:
        ax2.plot(px, py, "o", color=STYLE["text_dim"], markersize=5, zorder=4)

    # Highlight a triangle and its 3 corners
    # Pick a specific triangle
    tri_i, tri_j = 2, 2
    t_val = (tri_i + tri_j) * G2
    p0 = (tri_i - t_val, tri_j - t_val)

    t_val1 = (tri_i + 1 + tri_j) * G2
    p1 = (tri_i + 1 - t_val1, tri_j - t_val1)

    t_val2 = (tri_i + 1 + tri_j + 1) * G2
    p2 = (tri_i + 1 - t_val2, tri_j + 1 - t_val2)

    tri = Polygon(
        [p0, p1, p2],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.15,
        edgecolor=STYLE["accent3"],
        linewidth=2.5,
        zorder=2,
    )
    ax2.add_patch(tri)

    sample_s = ((p0[0] + p1[0] + p2[0]) / 3, (p0[1] + p1[1] + p2[1]) / 3)
    ax2.plot(*sample_s, "*", color=STYLE["warn"], markersize=14, zorder=8)

    for px, py in [p0, p1, p2]:
        ax2.plot(px, py, "o", color=STYLE["accent3"], markersize=8, zorder=6)
        ax2.plot(
            [px, sample_s[0]],
            [py, sample_s[1]],
            "--",
            color=STYLE["accent3"],
            lw=1,
            alpha=0.5,
            zorder=3,
        )

    ax2.text(
        2.0,
        -0.3,
        "3 corners per sample\n(better scaling to higher dims)",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.set_title(
        "Simplex Grid (Triangular)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])

    fig.suptitle(
        "Perlin vs Simplex: Grid Structure",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "math/13-gradient-noise", "perlin_vs_simplex_grid.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — noise_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/13-gradient-noise — noise_comparison.png
# ---------------------------------------------------------------------------


def diagram_noise_comparison():
    """Side-by-side 2D noise renders: white, Perlin, simplex."""
    w, h = 256, 256
    scale = 0.03

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    # White noise
    white = (
        _hash_to_float(_hash2d(xx.astype(np.uint32), yy.astype(np.uint32))) * 2.0 - 1.0
    )

    # Perlin noise
    perlin = _perlin2d(xx * scale, yy * scale, 42)

    # Simplex noise
    simplex = _simplex2d(xx * scale, yy * scale, 42)

    fig, axes = plt.subplots(1, 3, figsize=(14, 5), facecolor=STYLE["bg"])
    titles = [
        "White Noise (uncorrelated)",
        "Perlin Noise (square grid)",
        "Simplex Noise (triangular grid)",
    ]
    noises = [white, perlin * 2.5, simplex * 1.8]
    cmaps = ["gray", "gray", "gray"]

    for ax, title, noise, cmap in zip(axes, titles, noises, cmaps, strict=True):
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            noise,
            cmap=cmap,
            vmin=-1,
            vmax=1,
            interpolation="nearest",
            aspect="equal",
        )
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold", pad=8)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Noise Types: From Random Static to Smooth Coherent Patterns",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "noise_comparison.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — fbm_octaves.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/13-gradient-noise — fbm_octaves.png
# ---------------------------------------------------------------------------


def diagram_fbm_octaves():
    """fBm with 1, 2, 4, and 8 octaves showing progressive detail."""
    w, h = 256, 256
    scale = 0.02

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    octave_counts = [1, 2, 4, 8]

    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5), facecolor=STYLE["bg"])

    for ax, octaves in zip(axes, octave_counts, strict=True):
        noise = _fbm2d(xx * scale, yy * scale, 42, octaves)
        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            noise,
            cmap="gray",
            vmin=-0.8,
            vmax=0.8,
            interpolation="bilinear",
            aspect="equal",
        )
        ax.set_title(
            f"{octaves} octave{'s' if octaves > 1 else ''}",
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=8,
        )
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "fBm (Fractal Brownian Motion): Adding Octaves Builds Detail",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "fbm_octaves.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — lacunarity_persistence.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/13-gradient-noise — lacunarity_persistence.png
# ---------------------------------------------------------------------------


def diagram_lacunarity_persistence():
    """Grid showing fBm with varying lacunarity and persistence."""
    w, h = 192, 192
    scale = 0.02
    octaves = 6

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    lacunarities = [1.5, 2.0, 3.0]
    persistences = [0.3, 0.5, 0.7]

    fig, axes = plt.subplots(
        3,
        3,
        figsize=(12, 12),
        facecolor=STYLE["bg"],
    )

    for row, persistence in enumerate(persistences):
        for col, lacunarity in enumerate(lacunarities):
            ax = axes[row][col]
            noise = _fbm2d(xx * scale, yy * scale, 42, octaves, lacunarity, persistence)
            ax.set_facecolor(STYLE["bg"])
            ax.imshow(
                noise,
                cmap="gray",
                vmin=-0.8,
                vmax=0.8,
                interpolation="bilinear",
                aspect="equal",
            )
            ax.set_xticks([])
            ax.set_yticks([])
            for spine in ax.spines.values():
                spine.set_color(STYLE["grid"])
                spine.set_linewidth(0.5)

            if row == 0:
                ax.set_title(
                    f"lacunarity = {lacunarity}",
                    color=STYLE["accent1"],
                    fontsize=11,
                    fontweight="bold",
                    pad=8,
                )
            if col == 0:
                ax.set_ylabel(
                    f"persistence = {persistence}",
                    color=STYLE["accent2"],
                    fontsize=11,
                    fontweight="bold",
                    labelpad=8,
                )

    fig.suptitle(
        "fBm Parameters: Lacunarity (columns) vs Persistence (rows)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.01,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "lacunarity_persistence.png")


# ---------------------------------------------------------------------------
# math/13-gradient-noise — domain_warping.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/13-gradient-noise — domain_warping.png
# ---------------------------------------------------------------------------


def diagram_domain_warping():
    """Side-by-side: plain fBm vs domain-warped noise at various strengths."""
    w, h = 256, 256
    scale = 0.02

    yy, xx = np.meshgrid(
        np.arange(h, dtype=np.float64),
        np.arange(w, dtype=np.float64),
        indexing="ij",
    )

    strengths = [0.0, 1.5, 3.0, 5.0]
    labels = [
        "No warping (strength=0)",
        "Mild (strength=1.5)",
        "Moderate (strength=3.0)",
        "Strong (strength=5.0)",
    ]

    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5), facecolor=STYLE["bg"])

    for ax, strength, label in zip(axes, strengths, labels, strict=True):
        if strength == 0.0:
            noise = _fbm2d(xx * scale, yy * scale, 42, 4)
        else:
            noise = _domain_warp2d(xx * scale, yy * scale, 42, strength)

        ax.set_facecolor(STYLE["bg"])
        ax.imshow(
            noise,
            cmap="gray",
            vmin=-0.8,
            vmax=0.8,
            interpolation="bilinear",
            aspect="equal",
        )
        ax.set_title(label, color=STYLE["text"], fontsize=10, fontweight="bold", pad=8)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Domain Warping: Distorting Coordinates for Organic Patterns",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "math/13-gradient-noise", "domain_warping.png")


# ---------------------------------------------------------------------------
# math/14-blue-noise-sequences — sampling_comparison.png
# ---------------------------------------------------------------------------
