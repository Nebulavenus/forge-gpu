"""Diagrams for gpu/21."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Rectangle

from .._common import STYLE, save, setup_axes

# ACES filmic tone mapping coefficients (Narkowicz approximation).
# Shared across diagram functions to keep the curve definition in one place.
ACES_COEFFS = (2.51, 0.03, 2.43, 0.59, 0.14)

# ---------------------------------------------------------------------------
# gpu/03-uniforms-and-motion — unit_circle.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — hdr_pipeline.png
# ---------------------------------------------------------------------------


def diagram_hdr_pipeline():
    """Two-pass HDR rendering pipeline with sample pixel values."""
    fig, ax = plt.subplots(figsize=(12, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(-2.5, 3.5), grid=False, aspect=None)
    ax.axis("off")

    # Pipeline stages as boxes
    stages = [
        (0.5, 0, 2.5, 2, "Scene\nShaders", "Blinn-Phong\nlighting"),
        (4.0, 0, 2.5, 2, "HDR Buffer", "R16G16B16A16\nFLOAT"),
        (7.5, 0, 2.5, 2, "Tone Map\nPass", "Reinhard\nor ACES"),
        (11.0, 0, 1.5, 2, "sRGB\nSwapchain", "Auto\ngamma"),
    ]

    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"], STYLE["accent4"]]

    for (x, y, w, h, title, subtitle), color in zip(stages, colors, strict=True):
        rect = Rectangle(
            (x, y),
            w,
            h,
            linewidth=2,
            edgecolor=color,
            facecolor=STYLE["surface"],
            alpha=0.8,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            x + w / 2,
            y + h * 0.65,
            title,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )
        ax.text(
            x + w / 2,
            y + h * 0.25,
            subtitle,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
        )

    # Arrows between stages
    arrow_props = {
        "arrowstyle": "->,head_width=0.3,head_length=0.15",
        "color": STYLE["text_dim"],
        "lw": 2,
    }
    ax.annotate("", xy=(4.0, 1), xytext=(3.0, 1), arrowprops=arrow_props)
    ax.annotate("", xy=(7.5, 1), xytext=(6.5, 1), arrowprops=arrow_props)
    ax.annotate("", xy=(11.0, 1), xytext=(10.0, 1), arrowprops=arrow_props)

    # Sample pixel values below each stage
    samples = [
        (1.75, -0.7, "(0.8, 0.3, 0.1)\nSpecular: 4.2"),
        (5.25, -0.7, "Stored as-is:\n(0.8, 0.3, 0.1)\n(4.2, 4.2, 4.2)"),
        (8.75, -0.7, "Compressed:\n(0.57, 0.23, 0.09)\n(0.81, 0.81, 0.81)"),
        (11.75, -0.7, "Gamma\napplied"),
    ]
    for x, y, text in samples:
        ax.text(
            x,
            y,
            text,
            color=STYLE["warn"],
            fontsize=7.5,
            ha="center",
            va="top",
            family="monospace",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    fig.suptitle(
        "HDR Rendering Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/21-hdr-tone-mapping", "hdr_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — ldr_clipping.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — ldr_clipping.png
# ---------------------------------------------------------------------------


def diagram_ldr_clipping():
    """Show how LDR clamps values above 1.0, losing highlight detail."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), facecolor=STYLE["bg"])

    for ax in axes:
        setup_axes(ax, grid=False, aspect=None)
        ax.set_xlim(-0.2, 5.5)
        ax.set_ylim(-0.1, 1.4)

    # Generate a gradient of HDR values from 0 to 5
    x = np.linspace(0, 5, 200)

    # Left: LDR (clamped)
    ax = axes[0]
    ldr = np.clip(x, 0, 1)
    ax.fill_between(x, 0, ldr, color=STYLE["accent1"], alpha=0.3)
    ax.plot(x, ldr, color=STYLE["accent1"], lw=2.5, label="LDR output")
    ax.axhline(y=1.0, color=STYLE["warn"], lw=1, ls="--", alpha=0.7)
    ax.fill_between(x[x > 1], 1, 1.3, color="#ff3333", alpha=0.15)
    ax.text(
        3.0,
        1.15,
        "Lost detail",
        color="#ff6666",
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.set_xlabel("HDR input value", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Output value", color=STYLE["text"], fontsize=10)
    ax.set_title(
        "LDR (clamp to 1.0)", color=STYLE["text"], fontsize=12, fontweight="bold"
    )

    # Right: HDR with tone mapping (Reinhard)
    ax = axes[1]
    reinhard = x / (x + 1)
    ax.fill_between(x, 0, reinhard, color=STYLE["accent3"], alpha=0.3)
    ax.plot(x, reinhard, color=STYLE["accent3"], lw=2.5, label="Reinhard")
    ax.axhline(y=1.0, color=STYLE["warn"], lw=1, ls="--", alpha=0.7)

    # Show that different HDR values produce different outputs
    for hdr_val in [1.0, 2.0, 3.0, 4.0]:
        mapped = hdr_val / (hdr_val + 1)
        ax.plot(
            [hdr_val, hdr_val],
            [0, mapped],
            "--",
            color=STYLE["text_dim"],
            lw=0.8,
            alpha=0.5,
        )
        ax.plot(hdr_val, mapped, "o", color=STYLE["accent2"], markersize=5)
    ax.text(
        3.5,
        0.6,
        "Detail\npreserved",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.set_xlabel("HDR input value", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Output value", color=STYLE["text"], fontsize=10)
    ax.set_title(
        "HDR + Tone Mapping (Reinhard)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
    )

    fig.suptitle(
        "Why Tone Mapping Matters",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "gpu/21-hdr-tone-mapping", "ldr_clipping.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — tone_map_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — tone_map_comparison.png
# ---------------------------------------------------------------------------


def diagram_tone_map_comparison():
    """Compare Reinhard, ACES, and linear clamp tone mapping operators."""
    fig, ax = plt.subplots(figsize=(8, 5), facecolor=STYLE["bg"])
    setup_axes(ax, grid=True, aspect=None)

    x = np.linspace(0, 8, 400)

    # Linear clamp
    clamp = np.clip(x, 0, 1)
    ax.plot(
        x,
        clamp,
        color=STYLE["text_dim"],
        lw=2,
        ls="--",
        label="Linear clamp",
        alpha=0.8,
    )

    # Reinhard: x / (x + 1)
    reinhard = x / (x + 1)
    ax.plot(x, reinhard, color=STYLE["accent1"], lw=2.5, label="Reinhard")

    # ACES (Narkowicz approximation)
    a, b, c, d, e = ACES_COEFFS
    aces = np.clip((x * (a * x + b)) / (x * (c * x + d) + e), 0, 1)
    ax.plot(x, aces, color=STYLE["accent2"], lw=2.5, label="ACES filmic")

    ax.set_xlim(0, 8)
    ax.set_ylim(0, 1.1)
    ax.set_xlabel("HDR input value", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("LDR output value", color=STYLE["text"], fontsize=11)

    # Annotations
    ax.annotate(
        "Highlights\ncompressed",
        xy=(4, reinhard[np.searchsorted(x, 4)]),
        xytext=(5.5, 0.55),
        color=STYLE["accent1"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 1.5},
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.annotate(
        "S-curve\ncontrast",
        xy=(0.5, aces[np.searchsorted(x, 0.5)]),
        xytext=(2.0, 0.15),
        color=STYLE["accent2"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 1.5},
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    legend = ax.legend(
        loc="lower right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in legend.get_texts():
        text.set_color(STYLE["text"])

    fig.suptitle(
        "Tone Mapping Operators",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/21-hdr-tone-mapping", "tone_map_comparison.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — exposure_effect.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — exposure_effect.png
# ---------------------------------------------------------------------------


def diagram_exposure_effect():
    """Show how exposure multiplier shifts the tone curve."""
    fig, ax = plt.subplots(figsize=(8, 5), facecolor=STYLE["bg"])
    setup_axes(ax, grid=True, aspect=None)

    x = np.linspace(0, 5, 300)

    # ACES at different exposures
    exposures = [0.5, 1.0, 2.0, 4.0]
    colors_list = [STYLE["accent4"], STYLE["accent1"], STYLE["accent2"], STYLE["warn"]]

    a, b, c, d, e = ACES_COEFFS

    for exp, col in zip(exposures, colors_list, strict=True):
        hdr = x * exp
        aces = np.clip((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e), 0, 1)
        ax.plot(x, aces, color=col, lw=2.5, label=f"Exposure {exp:.1f}")

    ax.set_xlim(0, 5)
    ax.set_ylim(0, 1.1)
    ax.set_xlabel("Scene HDR value", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Output after ACES", color=STYLE["text"], fontsize=11)

    # Annotations
    ax.text(
        1.0,
        0.95,
        "\u2191 Brighter",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        3.5,
        0.25,
        "\u2193 Darker",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    legend = ax.legend(
        loc="center right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in legend.get_texts():
        text.set_color(STYLE["text"])

    fig.suptitle(
        "Exposure Control \u2014 Pre-Tone-Map Brightness",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/21-hdr-tone-mapping", "exposure_effect.png")


# ---------------------------------------------------------------------------
# gpu/22-bloom — Jimenez dual-filter bloom diagrams
# ---------------------------------------------------------------------------
