"""Diagrams for Asset Lesson 17 — Texture Atlas Packing."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# Diagram 1: Guillotine packing step-by-step
# ---------------------------------------------------------------------------


def diagram_guillotine_packing():
    """Show the guillotine bin packing algorithm placing rects step by step."""
    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])

    atlas_w, atlas_h = 8, 8

    # Pre-computed placements for the step-by-step illustration.
    # Each step shows which rects have been placed and the free regions.
    steps = [
        # Step 1: Place A at (0,0), free: right (4,0,4,8) and bottom (0,3,4,5)
        {
            "placed": [("A", 0, 0, 4, 3, STYLE["accent1"])],
            "free": [(4, 0, 4, 8), (0, 3, 4, 5)],
            "title": "Step 1: Place A (4x3)",
        },
        # Step 2: Place B at (4,0), split right free region
        {
            "placed": [
                ("A", 0, 0, 4, 3, STYLE["accent1"]),
                ("B", 4, 0, 3, 2, STYLE["accent2"]),
            ],
            "free": [(7, 0, 1, 8), (4, 2, 3, 6), (0, 3, 4, 5)],
            "title": "Step 2: Place B (3x2)",
        },
        # Step 3: Place D at (4,2) — area 6, placed before C (area 4)
        {
            "placed": [
                ("A", 0, 0, 4, 3, STYLE["accent1"]),
                ("B", 4, 0, 3, 2, STYLE["accent2"]),
                ("D", 4, 2, 2, 3, STYLE["accent4"]),
            ],
            "free": [(7, 0, 1, 8), (6, 2, 1, 6), (4, 5, 2, 3), (0, 3, 4, 5)],
            "title": "Step 3: Place D (2x3)",
        },
        # Step 4: Place C at (4,5) — area 4, smallest rect last
        {
            "placed": [
                ("A", 0, 0, 4, 3, STYLE["accent1"]),
                ("B", 4, 0, 3, 2, STYLE["accent2"]),
                ("D", 4, 2, 2, 3, STYLE["accent4"]),
                ("C", 4, 5, 2, 2, STYLE["accent3"]),
            ],
            "free": [(7, 0, 1, 8), (6, 2, 1, 6), (4, 7, 2, 1), (0, 3, 4, 5)],
            "title": "Step 4: Place C (2x2)",
        },
    ]

    for ax, step in zip(axes, steps, strict=True):
        setup_axes(ax, grid=False)
        ax.set_xlim(-0.3, atlas_w + 0.3)
        ax.set_ylim(-0.3, atlas_h + 0.3)
        ax.set_aspect("equal")
        ax.axis("off")

        # Draw atlas boundary
        border = mpatches.Rectangle(
            (0, 0),
            atlas_w,
            atlas_h,
            linewidth=1.5,
            edgecolor=STYLE["text_dim"],
            facecolor="none",
            linestyle="--",
        )
        ax.add_patch(border)

        # Draw free regions (subtle)
        for fx, fy, fw, fh in step["free"]:
            free_rect = mpatches.Rectangle(
                (fx, fy),
                fw,
                fh,
                linewidth=0.8,
                edgecolor=STYLE["grid"],
                facecolor=STYLE["surface"],
                alpha=0.4,
                linestyle=":",
            )
            ax.add_patch(free_rect)

        # Draw placed rects
        for name, rx, ry, rw, rh, color in step["placed"]:
            rect = mpatches.FancyBboxPatch(
                (rx + 0.05, ry + 0.05),
                rw - 0.1,
                rh - 0.1,
                boxstyle="round,pad=0.05",
                facecolor=color,
                edgecolor="white",
                linewidth=1.2,
                alpha=0.85,
            )
            ax.add_patch(rect)
            ax.text(
                rx + rw / 2,
                ry + rh / 2,
                f"{name}\n{rw}x{rh}",
                color="white",
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
            )

        ax.set_title(step["title"], color=STYLE["text"], fontsize=9, pad=8)

    fig.suptitle(
        "Guillotine Bin Packing",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=1.02,
    )

    save(fig, "assets/17-texture-atlas", "lesson_17_guillotine_packing.png")


# ---------------------------------------------------------------------------
# Diagram 2: Atlas UV transform
# ---------------------------------------------------------------------------


def diagram_atlas_uv_transform():
    """Show how pixel coordinates become shader UV offset/scale."""
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.patch.set_facecolor(STYLE["bg"])

    # Left panel: original UVs (0-1 range per material)
    ax = axes[0]
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.1, 1.1)
    ax.set_ylim(-0.1, 1.1)
    ax.set_aspect("equal")
    ax.set_title(
        "Original UVs\n(per material)", color=STYLE["text"], fontsize=10, pad=8
    )

    uv_rect = mpatches.Rectangle(
        (0, 0),
        1,
        1,
        linewidth=2,
        edgecolor=STYLE["accent1"],
        facecolor=STYLE["accent1"],
        alpha=0.2,
    )
    ax.add_patch(uv_rect)
    ax.text(
        0.5,
        0.5,
        "UV: (0,0) → (1,1)",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        va="center",
        fontweight="bold",
    )
    ax.set_xlabel("U", color=STYLE["text_dim"], fontsize=9)
    ax.set_ylabel("V", color=STYLE["text_dim"], fontsize=9)
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])

    # Middle panel: atlas with multiple rects
    ax = axes[1]
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.05, 1.05)
    ax.set_ylim(-0.05, 1.05)
    ax.set_aspect("equal")
    ax.set_title("Atlas Layout\n(2048x2048)", color=STYLE["text"], fontsize=10, pad=8)

    atlas_rects = [
        ("Brick", 0.0, 0.0, 0.5, 0.5, STYLE["accent1"]),
        ("Metal", 0.5, 0.0, 0.25, 0.25, STYLE["accent2"]),
        ("Wood", 0.5, 0.25, 0.5, 0.25, STYLE["accent3"]),
        ("Tile", 0.0, 0.5, 0.25, 0.25, STYLE["accent4"]),
    ]

    for name, rx, ry, rw, rh, color in atlas_rects:
        rect = mpatches.Rectangle(
            (rx, ry),
            rw,
            rh,
            linewidth=1.5,
            edgecolor=color,
            facecolor=color,
            alpha=0.25,
        )
        ax.add_patch(rect)
        ax.text(
            rx + rw / 2,
            ry + rh / 2,
            name,
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
        )

    # Atlas boundary
    border = mpatches.Rectangle(
        (0, 0),
        1,
        1,
        linewidth=2,
        edgecolor=STYLE["text_dim"],
        facecolor="none",
    )
    ax.add_patch(border)
    ax.set_xlabel("U", color=STYLE["text_dim"], fontsize=9)
    ax.set_ylabel("V", color=STYLE["text_dim"], fontsize=9)
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])

    # Right panel: the UV transform formula
    ax = axes[2]
    setup_axes(ax, grid=False)
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 8)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Shader UV Remap", color=STYLE["text"], fontsize=10, pad=8)

    # Formula box
    formula_rect = mpatches.FancyBboxPatch(
        (0.5, 4.5),
        9,
        2.5,
        boxstyle="round,pad=0.2",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(formula_rect)
    ax.text(
        5,
        5.7,
        "uv_atlas = uv * scale + offset",
        color=STYLE["warn"],
        fontsize=12,
        fontfamily="monospace",
        fontweight="bold",
        ha="center",
        va="center",
    )

    # Example values
    examples = [
        ("Brick:", "offset=(0.0, 0.0)  scale=(0.5, 0.5)", STYLE["accent1"]),
        ("Metal:", "offset=(0.5, 0.0)  scale=(0.25, 0.25)", STYLE["accent2"]),
        ("Wood:", "offset=(0.5, 0.25)  scale=(0.5, 0.25)", STYLE["accent3"]),
    ]
    for i, (name, values, color) in enumerate(examples):
        y = 3.2 - i * 1.1
        ax.text(1.0, y, name, color=color, fontsize=9, fontweight="bold", va="center")
        ax.text(
            3.0,
            y,
            values,
            color=STYLE["text"],
            fontsize=8.5,
            fontfamily="monospace",
            va="center",
        )

    save(fig, "assets/17-texture-atlas", "lesson_17_atlas_uv_transform.png")


# ---------------------------------------------------------------------------
# Diagram 3: Mipmap bleeding across padding
# ---------------------------------------------------------------------------


def diagram_mipmap_bleeding():
    """Show how padding shrinks at each mip level, causing bleeding."""
    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])

    mip_levels = [
        {
            "label": "Mip 0 (2048²)",
            "size": 64,
            "padding": 4,
            "safe": True,
            "status": "Safe",
        },
        {
            "label": "Mip 1 (1024²)",
            "size": 32,
            "padding": 2,
            "safe": True,
            "status": "Safe",
        },
        {
            "label": "Mip 2 (512²)",
            "size": 16,
            "padding": 1,
            "safe": False,
            "status": "Minor bleed",
        },
        {
            "label": "Mip 3 (256²)",
            "size": 8,
            "padding": 0,
            "safe": False,
            "status": "Bleeding",
        },
    ]

    for ax, mip in zip(axes, mip_levels, strict=True):
        setup_axes(ax, grid=False)
        s = mip["size"]
        ax.set_xlim(-2, s + 2)
        ax.set_ylim(-2, s + 2)
        ax.set_aspect("equal")
        ax.axis("off")

        p = mip["padding"]

        # Two adjacent material rects
        half = s // 2

        # Material A (left half)
        rect_a = mpatches.Rectangle(
            (0, 0),
            half,
            s,
            facecolor=STYLE["accent1"],
            alpha=0.6,
            edgecolor="none",
        )
        ax.add_patch(rect_a)

        # Material B (right half)
        rect_b = mpatches.Rectangle(
            (half, 0),
            half,
            s,
            facecolor=STYLE["accent2"],
            alpha=0.6,
            edgecolor="none",
        )
        ax.add_patch(rect_b)

        # Padding zone (border between materials)
        if p > 0:
            pad_rect = mpatches.Rectangle(
                (half - p, 0),
                p * 2,
                s,
                facecolor=STYLE["surface"],
                alpha=0.8,
                edgecolor=STYLE["warn"],
                linewidth=1.5,
                linestyle="--",
            )
            ax.add_patch(pad_rect)
            ax.text(
                half,
                s + 0.8,
                f"{p}px pad",
                color=STYLE["warn"],
                fontsize=8,
                ha="center",
                va="bottom",
            )
        else:
            # No padding — show bleeding zone
            bleed = mpatches.Rectangle(
                (half - 1, 0),
                2,
                s,
                facecolor=STYLE["warn"],
                alpha=0.3,
                edgecolor=STYLE["warn"],
                linewidth=1.5,
            )
            ax.add_patch(bleed)
            ax.text(
                half,
                s + 0.8,
                "BLEED",
                color=STYLE["warn"],
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="bottom",
            )

        # Border
        border = mpatches.Rectangle(
            (0, 0),
            s,
            s,
            linewidth=1.2,
            edgecolor=STYLE["text_dim"],
            facecolor="none",
        )
        ax.add_patch(border)

        # Labels
        status_color = STYLE["accent3"] if mip["safe"] else STYLE["warn"]
        status_text = mip.get("status", "Safe" if mip["safe"] else "Bleeding")
        ax.set_title(
            f"{mip['label']}\n{status_text}",
            color=status_color,
            fontsize=9,
            pad=8,
        )

        ax.text(
            half / 2,
            s / 2,
            "A",
            color="white",
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
        )
        ax.text(
            half + half / 2,
            s / 2,
            "B",
            color="white",
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
        )

    fig.suptitle(
        "Mipmap Bleeding — Padding Halves at Each Mip Level",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=1.02,
    )
    fig.text(
        0.5,
        -0.02,
        "Padding width exaggerated for readability.",
        ha="center",
        color=STYLE["text_dim"],
        fontsize=8,
    )

    save(fig, "assets/17-texture-atlas", "lesson_17_mipmap_bleeding.png")


DIAGRAMS = [
    diagram_guillotine_packing,
    diagram_atlas_uv_transform,
    diagram_mipmap_bleeding,
]
