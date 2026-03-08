"""Diagrams for math/11."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# math/11-color-spaces — cie_chromaticity.png
# ---------------------------------------------------------------------------


def diagram_cie_chromaticity():
    """CIE 1931 xy chromaticity diagram with sRGB, DCI-P3, Rec.2020 gamuts."""
    fig = plt.figure(figsize=(8, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.02, 0.82), ylim=(-0.02, 0.92), grid=False)

    # CIE 1931 spectral locus (2-degree standard observer, 380-700 nm)
    # These are the (x, y) chromaticity coordinates of monochromatic light
    # at each wavelength. Data from CIE tables.
    spectral_xy = np.array(
        [
            [0.1741, 0.0050],
            [0.1740, 0.0050],
            [0.1738, 0.0049],
            [0.1736, 0.0049],
            [0.1733, 0.0048],
            [0.1730, 0.0048],
            [0.1726, 0.0048],
            [0.1721, 0.0048],
            [0.1714, 0.0051],
            [0.1703, 0.0058],
            [0.1689, 0.0069],
            [0.1669, 0.0086],
            [0.1644, 0.0109],
            [0.1611, 0.0138],
            [0.1566, 0.0177],
            [0.1510, 0.0227],
            [0.1440, 0.0297],
            [0.1355, 0.0399],
            [0.1241, 0.0578],
            [0.1096, 0.0868],
            [0.0913, 0.1327],
            [0.0687, 0.2007],
            [0.0454, 0.2950],
            [0.0235, 0.4127],
            [0.0082, 0.5384],
            [0.0039, 0.6548],
            [0.0139, 0.7502],
            [0.0389, 0.8120],
            [0.0743, 0.8338],
            [0.1142, 0.8262],
            [0.1547, 0.8059],
            [0.1929, 0.7816],
            [0.2296, 0.7543],
            [0.2658, 0.7243],
            [0.3016, 0.6923],
            [0.3373, 0.6589],
            [0.3731, 0.6245],
            [0.4087, 0.5896],
            [0.4441, 0.5547],
            [0.4788, 0.5202],
            [0.5125, 0.4866],
            [0.5448, 0.4544],
            [0.5752, 0.4242],
            [0.6029, 0.3965],
            [0.6270, 0.3725],
            [0.6482, 0.3514],
            [0.6658, 0.3340],
            [0.6801, 0.3197],
            [0.6915, 0.3083],
            [0.7006, 0.2993],
            [0.7079, 0.2920],
            [0.7140, 0.2859],
            [0.7190, 0.2809],
            [0.7230, 0.2770],
            [0.7260, 0.2740],
            [0.7283, 0.2717],
            [0.7300, 0.2700],
            [0.7311, 0.2689],
            [0.7320, 0.2680],
            [0.7327, 0.2673],
            [0.7334, 0.2666],
            [0.7340, 0.2660],
            [0.7344, 0.2656],
            [0.7346, 0.2654],
            [0.7347, 0.2653],
        ]
    )

    # Fill the spectral locus with a faint tinted region
    locus_closed = np.vstack([spectral_xy, spectral_xy[0]])
    locus_poly = Polygon(
        locus_closed,
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor="none",
        alpha=0.4,
    )
    ax.add_patch(locus_poly)

    # Draw the spectral locus outline
    ax.plot(
        spectral_xy[:, 0],
        spectral_xy[:, 1],
        color=STYLE["text"],
        lw=1.5,
        alpha=0.8,
    )
    # Purple line closing the locus
    ax.plot(
        [spectral_xy[0, 0], spectral_xy[-1, 0]],
        [spectral_xy[0, 1], spectral_xy[-1, 1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.0,
        alpha=0.5,
    )

    # Label select wavelengths
    label_nms = [460, 480, 500, 520, 540, 560, 580, 600, 620, 700]
    for nm in label_nms:
        idx = (nm - 380) // 5
        if 0 <= idx < len(spectral_xy):
            x, y = spectral_xy[idx]
            # Offset labels outward
            cx, cy = 0.33, 0.33  # center of diagram
            dx, dy = x - cx, y - cy
            dist = np.sqrt(dx * dx + dy * dy)
            if dist > 0:
                ox = dx / dist * 0.04
                oy = dy / dist * 0.04
            else:
                ox, oy = 0.04, 0.04
            ax.annotate(
                f"{nm}",
                xy=(x, y),
                xytext=(x + ox, y + oy),
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="center",
                arrowprops={"arrowstyle": "-", "color": STYLE["grid"], "lw": 0.5},
            )

    # Gamut triangles
    gamuts = [
        (
            "sRGB",
            [(0.6400, 0.3300), (0.3000, 0.6000), (0.1500, 0.0600)],
            STYLE["accent1"],
            2.0,
            "-",
        ),
        (
            "DCI-P3",
            [(0.6800, 0.3200), (0.2650, 0.6900), (0.1500, 0.0600)],
            STYLE["accent2"],
            1.5,
            "--",
        ),
        (
            "Rec.2020",
            [(0.7080, 0.2920), (0.1700, 0.7970), (0.1310, 0.0460)],
            STYLE["accent3"],
            1.5,
            ":",
        ),
    ]

    for name, pts, color, lw, ls in gamuts:
        tri = Polygon(
            pts,
            closed=True,
            facecolor="none",
            edgecolor=color,
            lw=lw,
            ls=ls,
        )
        ax.add_patch(tri)

        # Label near the centroid
        cx = sum(p[0] for p in pts) / 3
        cy = sum(p[1] for p in pts) / 3
        ax.text(
            cx,
            cy - 0.03,
            name,
            color=color,
            fontsize=10,
            ha="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # D65 white point
    ax.plot(0.3127, 0.3290, "o", color=STYLE["warn"], markersize=8, zorder=5)
    ax.text(
        0.3127 + 0.02,
        0.3290 - 0.03,
        "D65",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "CIE 1931 Chromaticity Diagram",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "math/11-color-spaces", "cie_chromaticity.png")


# ---------------------------------------------------------------------------
# math/11-color-spaces — gamma_perception.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/11-color-spaces — gamma_perception.png
# ---------------------------------------------------------------------------


def diagram_gamma_perception():
    """Show how sRGB gamma allocates more precision to dark values."""
    fig, (ax1, ax2) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
    )

    x = np.linspace(0, 1, 500)

    # --- Left panel: transfer functions ---
    setup_axes(ax1, xlim=(-0.02, 1.02), ylim=(-0.02, 1.02), grid=True, aspect=None)

    # sRGB transfer function (linear -> encoded)
    srgb = np.where(
        x <= 0.0031308,
        x * 12.92,
        1.055 * np.power(x, 1 / 2.4) - 0.055,
    )
    ax1.plot(x, srgb, color=STYLE["accent1"], lw=2.5, label="sRGB (piecewise)")
    ax1.plot(
        x,
        np.power(x, 1 / 2.2),
        "--",
        color=STYLE["accent2"],
        lw=1.5,
        alpha=0.7,
        label="Simple pow(x, 1/2.2)",
    )
    ax1.plot(x, x, ":", color=STYLE["text_dim"], lw=1.0, label="Linear (no correction)")

    ax1.set_xlabel("Linear light intensity", color=STYLE["axis"], fontsize=10)
    ax1.set_ylabel(
        "Encoded value (for storage/display)", color=STYLE["axis"], fontsize=10
    )
    ax1.set_title(
        "Gamma Encoding: Linear to sRGB",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax1.legend(
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    # Annotate the midpoint
    mid_srgb = 1.055 * (0.5 ** (1 / 2.4)) - 0.055
    ax1.plot(0.5, mid_srgb, "o", color=STYLE["warn"], markersize=8, zorder=5)
    ax1.annotate(
        f"50% light -> sRGB {mid_srgb:.2f}",
        xy=(0.5, mid_srgb),
        xytext=(0.15, 0.9),
        color=STYLE["warn"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 1.5},
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Right panel: perceptual spacing ---
    setup_axes(ax2, grid=False, aspect=None)

    # Show gradient bars: linear vs sRGB
    n_steps = 32
    linear_levels = np.linspace(0, 1, n_steps)

    # Decode sRGB to show what equally-spaced sRGB values look like in linear
    srgb_levels = np.linspace(0, 1, n_steps)
    srgb_decoded = np.where(
        srgb_levels <= 0.04045,
        srgb_levels / 12.92,
        np.power((srgb_levels + 0.055) / 1.055, 2.4),
    )

    bar_height = 0.3
    for i in range(n_steps):
        # Linear encoding (top bar) — equally-spaced light levels
        ax2.add_patch(
            Rectangle(
                (i / n_steps, 0.6),
                1 / n_steps,
                bar_height,
                facecolor=(linear_levels[i],) * 3,
                edgecolor="none",
            )
        )
        # sRGB encoding (bottom bar) — perceptually-spaced
        ax2.add_patch(
            Rectangle(
                (i / n_steps, 0.1),
                1 / n_steps,
                bar_height,
                facecolor=(srgb_decoded[i],) * 3,
                edgecolor="none",
            )
        )

    ax2.set_xlim(0, 1)
    ax2.set_ylim(0, 1.1)
    ax2.text(
        0.5,
        0.97,
        "Linear encoding (wastes bits on brights)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        0.5,
        0.47,
        "sRGB encoding (more steps in darks)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.set_title(
        "Perceptual Spacing: 32 Equal Steps",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])
    for spine in ax2.spines.values():
        spine.set_visible(False)

    fig.suptitle(
        "Gamma Correction: Matching Human Perception",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    save(fig, "math/11-color-spaces", "gamma_perception.png")


# ---------------------------------------------------------------------------
# math/11-color-spaces — tone_mapping_curves.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# math/11-color-spaces — tone_mapping_curves.png
# ---------------------------------------------------------------------------


def diagram_tone_mapping_curves():
    """Compare Reinhard, ACES, and linear clamp tone mapping curves."""
    fig = plt.figure(figsize=(8, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.2, 10.2), ylim=(-0.02, 1.15), grid=True, aspect=None)

    x = np.linspace(0, 10, 500)

    # Linear clamp
    linear = np.clip(x, 0, 1)
    ax.plot(x, linear, ":", color=STYLE["text_dim"], lw=1.5, label="Linear clamp")

    # Reinhard: x / (x + 1)
    reinhard = x / (x + 1)
    ax.plot(x, reinhard, color=STYLE["accent2"], lw=2.5, label="Reinhard")

    # ACES (Narkowicz 2015 fit)
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    aces = np.clip((x * (a * x + b)) / (x * (c * x + d) + e), 0, 1)
    ax.plot(x, aces, color=STYLE["accent1"], lw=2.5, label="ACES (Narkowicz)")

    # Mark the 1.0 input line
    ax.axvline(x=1.0, color=STYLE["grid"], lw=0.8, ls="--", alpha=0.5)
    ax.text(
        1.05,
        1.08,
        "SDR range boundary",
        color=STYLE["text_dim"],
        fontsize=8,
        rotation=0,
    )

    # Highlight area
    ax.fill_between(
        x,
        0,
        1,
        where=(x <= 1),  # type: ignore[arg-type]
        alpha=0.06,
        color=STYLE["accent1"],
    )

    ax.set_xlabel("HDR input intensity", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Display output (0-1)", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Tone Mapping: Compressing HDR to Display Range",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.legend(
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        loc="lower right",
    )

    fig.tight_layout()
    save(fig, "math/11-color-spaces", "tone_mapping_curves.png")


# ---------------------------------------------------------------------------
# math/12-hash-functions — white_noise_comparison.png
# ---------------------------------------------------------------------------

# Python implementations of the three hash functions from forge_math.h.
# These use np.uint32 arrays for correct 32-bit unsigned overflow.
