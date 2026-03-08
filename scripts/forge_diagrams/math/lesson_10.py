"""Diagrams for math/10."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# math/10-anisotropy — pixel_footprint.png
# ---------------------------------------------------------------------------


def diagram_pixel_footprint():
    """Two-panel: Jacobian maps circle to ellipse, isotropic vs anisotropic sampling."""
    fig = plt.figure(figsize=(11, 5.5), facecolor=STYLE["bg"])

    # --- Left panel: pixel footprint at different tilt angles ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-2.5, 3.0), ylim=(-2.8, 2.8), grid=False)

    # Faint reference axes
    ax1.axhline(0, color=STYLE["grid"], lw=0.5, alpha=0.4)
    ax1.axvline(0, color=STYLE["grid"], lw=0.5, alpha=0.4)

    theta = np.linspace(0, 2 * np.pi, 100)

    # Draw ellipses at different tilt angles (footprints in texture space)
    tilts = [
        (0, STYLE["accent3"], "0\u00b0 (isotropic)", 1.12),
        (45, STYLE["accent1"], "45\u00b0", 1.12),
        (75, STYLE["accent2"], "75\u00b0 (anisotropic)", 1.12),
    ]

    for tilt_deg, color, label, label_x in tilts:
        tilt_rad = np.radians(tilt_deg)
        cos_t = np.cos(tilt_rad)
        sigma_u = 1.0
        sigma_v = 1.0 / cos_t if cos_t > 0.01 else 100.0
        sigma_v = min(sigma_v, 2.3)

        x = sigma_u * np.cos(theta)
        y = sigma_v * np.sin(theta)
        ax1.plot(x, y, "-", color=color, lw=2.2, alpha=0.85)

        # Label to the right of each ellipse (at the widest point)
        ax1.text(
            label_x,
            sigma_v * 0.7,
            label,
            color=color,
            fontsize=9,
            ha="left",
            va="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Label the singular value axes on the 75-degree ellipse
    tilt75 = np.radians(75)
    sv_major = min(1.0 / np.cos(tilt75), 2.3)

    # Major axis arrow (vertical, center to edge = singular value)
    ax1.annotate(
        "",
        xy=(0, sv_major),
        xytext=(0, 0),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
    )
    ax1.text(
        -0.35,
        sv_major / 2,
        "\u03c3\u2081",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    # Minor axis arrow (horizontal, center to edge = singular value)
    ax1.annotate(
        "",
        xy=(1.0, 0),
        xytext=(0, 0),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
    )
    ax1.text(
        0.5,
        -0.35,
        "\u03c3\u2082",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax1.plot(0, 0, "o", color=STYLE["text"], markersize=4, zorder=5)
    ax1.set_title(
        "Pixel Footprint in Texture Space",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax1.set_xlabel("U (texels)", color=STYLE["axis"], fontsize=10)
    ax1.set_ylabel("V (texels)", color=STYLE["axis"], fontsize=10)

    # --- Right panel: isotropic vs anisotropic sampling ---
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-4.2, 4.2), ylim=(-3.2, 3.2), grid=False)

    # Draw the same 75-degree ellipse in both halves
    sigma_u = 1.0
    sigma_v = min(1.0 / np.cos(tilt75), 2.3)
    ex = sigma_u * np.cos(theta)
    ey = sigma_v * np.sin(theta)

    # Push the two examples further apart to avoid overlap with divider
    offset_l = -2.1
    offset_r = 2.1

    # Left half: isotropic (single large circle covering the ellipse)
    ax2.plot(ex + offset_l, ey, "-", color=STYLE["accent2"], lw=2, alpha=0.7)
    # Isotropic: dashed circle sized to the major axis, clipped to panel
    iso_radius = sigma_v
    ax2.plot(
        iso_radius * np.cos(theta) + offset_l,
        iso_radius * np.sin(theta),
        "--",
        color=STYLE["text_dim"],
        lw=1.2,
        alpha=0.5,
        clip_on=True,
    )
    ax2.plot(
        offset_l,
        0,
        "o",
        color=STYLE["accent2"],
        markersize=10,
        zorder=5,
        alpha=0.8,
    )
    mip_iso = np.log2(sigma_v)
    ax2.text(
        offset_l,
        -sigma_v - 0.45,
        f"Isotropic\n1 sample, mip {mip_iso:.1f}",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        offset_l,
        sigma_v + 0.35,
        "BLURRY",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        alpha=0.8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Right half: anisotropic (multiple small samples along major axis)
    ax2.plot(ex + offset_r, ey, "-", color=STYLE["accent3"], lw=2, alpha=0.7)
    n_samples = max(1, int(np.ceil(sigma_v / sigma_u)))
    for i in range(n_samples):
        t = (i + 0.5) / n_samples
        sy_pos = -sigma_v + 2 * sigma_v * t
        ax2.plot(
            sigma_u * np.cos(theta) * 0.85 + offset_r,
            sigma_u * np.sin(theta) * 0.85 + sy_pos,
            "-",
            color=STYLE["text_dim"],
            lw=0.7,
            alpha=0.4,
        )
        ax2.plot(
            offset_r,
            sy_pos,
            "o",
            color=STYLE["accent3"],
            markersize=5,
            zorder=5,
            alpha=0.7,
        )
    mip_aniso = np.log2(max(sigma_u, 1e-6))
    ax2.text(
        offset_r,
        -sigma_v - 0.45,
        f"Anisotropic\n{n_samples} samples, mip {mip_aniso:.1f}",
        color=STYLE["accent3"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        offset_r,
        sigma_v + 0.35,
        "SHARP",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        alpha=0.8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Divider line
    ax2.plot([0, 0], [-3.0, 3.0], "-", color=STYLE["grid"], lw=0.8, alpha=0.5)
    ax2.text(
        0,
        3.0,
        "vs",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="bottom",
        style="italic",
    )

    ax2.set_title(
        "Isotropic vs Anisotropic Filtering",
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
        "Anisotropy: How the Pixel Footprint Drives Texture Filtering",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "math/10-anisotropy", "pixel_footprint.png")


# ---------------------------------------------------------------------------
# math/11-color-spaces — cie_chromaticity.png
# ---------------------------------------------------------------------------
