"""Diagrams for ui/03."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 03 — Font Atlas Packing
# ---------------------------------------------------------------------------


def diagram_shelf_packing():
    """Show shelf packing step-by-step: glyphs placed left-to-right in rows,
    sorted by height (tallest first), with wasted space highlighted."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Shelf Packing — Height-Sorted Glyph Placement",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    atlas_w, atlas_h = 16, 12

    # Simulated glyph sizes — sorted by height descending
    glyphs_sorted = [
        (3, 5),
        (2, 5),
        (3, 5),
        (2, 5),
        (3, 5),
        (2, 5),
        (2, 4),
        (3, 4),
        (2, 4),
        (3, 4),
        (2, 4),
        (2, 4),
        (2, 3),
        (1, 3),
        (2, 3),
        (2, 3),
        (1, 3),
        (2, 3),
        (2, 3),
        (2, 2),
        (1, 2),
        (2, 2),
        (1, 2),
        (2, 2),
        (1, 2),
    ]

    # Unsorted (shuffled) version
    glyphs_unsorted = [
        (2, 3),
        (3, 5),
        (1, 2),
        (2, 4),
        (2, 5),
        (1, 3),
        (2, 2),
        (3, 4),
        (2, 3),
        (2, 5),
        (2, 4),
        (2, 3),
        (3, 5),
        (1, 2),
        (2, 4),
        (2, 2),
        (1, 3),
        (2, 5),
        (2, 3),
        (3, 4),
        (2, 2),
        (2, 4),
        (1, 3),
        (2, 3),
        (2, 5),
    ]

    colors_by_height = {
        5: STYLE["accent1"],
        4: STYLE["accent2"],
        3: STYLE["accent3"],
        2: STYLE["accent4"],
    }

    def pack_and_draw(ax, glyphs, title, padding=0):
        setup_axes(
            ax,
            xlim=(-0.5, atlas_w + 0.5),
            ylim=(-1.5, atlas_h + 0.5),
            grid=False,
            aspect="equal",
        )
        ax.axis("off")

        # Draw atlas border
        border = mpatches.Rectangle(
            (0, 0),
            atlas_w,
            atlas_h,
            facecolor=STYLE["surface"],
            edgecolor=STYLE["text_dim"],
            linewidth=1.5,
            zorder=1,
        )
        ax.add_patch(border)

        # Shelf pack
        cx, cy = 0, atlas_h
        row_h = 0
        total_used = 0
        rows = []

        for gw, gh in glyphs:
            pw = gw + padding
            ph = gh + padding
            if cx + pw > atlas_w:
                rows.append((cy, row_h))
                cy -= row_h
                cx = 0
                row_h = 0
            if cy - ph < 0:
                break

            color = colors_by_height.get(gh, STYLE["text_dim"])
            rect = mpatches.Rectangle(
                (cx, cy - gh),
                gw,
                gh,
                facecolor=color + "50",
                edgecolor=color,
                linewidth=1.0,
                zorder=3,
            )
            ax.add_patch(rect)
            total_used += gw * gh

            cx += pw
            if ph > row_h:
                row_h = ph

        if row_h > 0:
            rows.append((cy, row_h))

        # Draw shelf dividers
        for shelf_top, shelf_h in rows:
            shelf_bottom = shelf_top - shelf_h
            ax.plot(
                [0, atlas_w],
                [shelf_bottom, shelf_bottom],
                color=STYLE["warn"],
                linewidth=0.8,
                linestyle=":",
                zorder=2,
            )

        # Utilization
        total_area = atlas_w * atlas_h
        util = total_used / total_area * 100
        ax.set_title(
            f"{title}\n{util:.0f}% utilization",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            pad=8,
        )

    pack_and_draw(axes[0], glyphs_unsorted, "Unsorted (random order)")
    pack_and_draw(axes[1], glyphs_sorted, "Sorted by height (tallest first)")

    # Legend
    legend_items = [
        mpatches.Patch(
            facecolor=STYLE["accent1"] + "50",
            edgecolor=STYLE["accent1"],
            label="Tall glyphs (5px)",
        ),
        mpatches.Patch(
            facecolor=STYLE["accent2"] + "50",
            edgecolor=STYLE["accent2"],
            label="Medium glyphs (4px)",
        ),
        mpatches.Patch(
            facecolor=STYLE["accent3"] + "50",
            edgecolor=STYLE["accent3"],
            label="Short glyphs (3px)",
        ),
        mpatches.Patch(
            facecolor=STYLE["accent4"] + "50",
            edgecolor=STYLE["accent4"],
            label="Small glyphs (2px)",
        ),
    ]
    fig.legend(
        handles=legend_items,
        loc="lower center",
        ncol=4,
        fontsize=8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        framealpha=0.9,
        bbox_to_anchor=(0.5, -0.01),
    )

    fig.tight_layout(rect=[0, 0.06, 1, 0.93])
    save(fig, "ui/03-font-atlas", "shelf_packing.png")


def diagram_padding_bleed():
    """Show what happens with and without padding between adjacent glyphs
    in an atlas — bilinear filtering samples into the neighbor."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Atlas Padding — Preventing Texture Bleed",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    cell_size = 1.0

    def draw_texels(ax, grid, xlim, ylim, title):
        setup_axes(ax, xlim=xlim, ylim=ylim, grid=False, aspect="equal")
        ax.axis("off")

        rows = len(grid)
        cols = len(grid[0])
        for r in range(rows):
            for c in range(cols):
                val = grid[r][c]
                if val == 0:
                    color = STYLE["bg"]
                elif val == 1:
                    color = STYLE["accent1"]
                elif val == 2:
                    color = STYLE["accent2"]
                else:
                    color = STYLE["warn"]

                alpha = 0.7 if val > 0 else 0.2
                rect = mpatches.Rectangle(
                    (c * cell_size, (rows - 1 - r) * cell_size),
                    cell_size,
                    cell_size,
                    facecolor=color,
                    alpha=alpha,
                    edgecolor=STYLE["grid"],
                    linewidth=0.5,
                    zorder=2,
                )
                ax.add_patch(rect)

        ax.set_title(title, color=STYLE["text"], fontsize=10, fontweight="bold", pad=8)

    # Without padding: two glyph bitmaps adjacent
    # Glyph A (cyan, left) and Glyph B (orange, right) touching
    grid_no_pad = [
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 1, 1, 1, 2, 2, 2, 0],
        [0, 0, 0, 0, 0, 0, 0, 0],
    ]

    draw_texels(
        axes[0], grid_no_pad, (-0.5, 8.5), (-1.5, 5.5), "Without Padding — Bleed Risk"
    )

    # Draw the sampling circle at the boundary
    circle = plt.Circle(
        (3.5, 2.5),
        0.8,
        facecolor="none",
        edgecolor=STYLE["warn"],
        linewidth=2.0,
        linestyle="--",
        zorder=5,
    )
    axes[0].add_patch(circle)
    axes[0].text(
        3.5,
        0.5,
        "Bilinear filter\nsamples BOTH glyphs",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
    )

    # With padding: 1px gap between glyphs
    grid_padded = [
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 1, 1, 1, 0, 2, 2, 2, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0],
    ]

    draw_texels(
        axes[1],
        grid_padded,
        (-0.5, 9.5),
        (-1.5, 5.5),
        "With 1px Padding — Clean Sampling",
    )

    # Draw the sampling circle in the padded case
    circle2 = plt.Circle(
        (3.5, 2.5),
        0.8,
        facecolor="none",
        edgecolor=STYLE["accent3"],
        linewidth=2.0,
        linestyle="--",
        zorder=5,
    )
    axes[1].add_patch(circle2)
    axes[1].text(
        3.5,
        0.5,
        "Filter only samples\nglyph + empty padding",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
    )

    # Legend
    legend_items = [
        mpatches.Patch(facecolor=STYLE["accent1"], alpha=0.7, label="Glyph A texels"),
        mpatches.Patch(facecolor=STYLE["accent2"], alpha=0.7, label="Glyph B texels"),
        mpatches.Patch(
            facecolor=STYLE["bg"], edgecolor=STYLE["grid"], label="Empty (padding)"
        ),
    ]
    fig.legend(
        handles=legend_items,
        loc="lower center",
        ncol=3,
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        framealpha=0.9,
        bbox_to_anchor=(0.5, -0.01),
    )

    fig.tight_layout(rect=[0, 0.06, 1, 0.93])
    save(fig, "ui/03-font-atlas", "padding_bleed.png")


def diagram_uv_coordinates():
    """Show how pixel positions in the atlas map to normalized UV coordinates,
    with one glyph highlighted and the formula overlaid."""

    fig, ax = plt.subplots(figsize=(8, 8))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-2.5, 11), grid=False, aspect="equal")
    ax.axis("off")

    atlas_size = 8  # visual size for the atlas square

    # Draw atlas background
    atlas_rect = mpatches.Rectangle(
        (0, 0),
        atlas_size,
        atlas_size,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        zorder=1,
    )
    ax.add_patch(atlas_rect)

    # Draw some "packed glyphs" as colored rectangles
    packed = [
        (0.5, 5.5, 2.0, 2.0, "H", STYLE["accent1"]),
        (3.0, 5.5, 1.5, 2.0, "e", STYLE["accent1"]),
        (5.0, 5.5, 1.8, 2.0, "l", STYLE["accent1"]),
        (0.5, 3.0, 2.0, 2.0, "o", STYLE["accent1"]),
        (3.0, 3.0, 1.5, 2.0, "W", STYLE["accent1"]),
    ]

    for gx, gy, gw, gh, label, color in packed:
        r = mpatches.Rectangle(
            (gx, gy),
            gw,
            gh,
            facecolor=color + "30",
            edgecolor=color,
            linewidth=1.0,
            zorder=2,
        )
        ax.add_patch(r)
        ax.text(
            gx + gw / 2,
            gy + gh / 2,
            label,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
            zorder=3,
        )

    # Highlight one glyph with full annotation
    hx, hy, hw, hh = 5.0, 3.0, 2.0, 2.0
    highlight = mpatches.Rectangle(
        (hx, hy),
        hw,
        hh,
        facecolor=STYLE["accent2"] + "40",
        edgecolor=STYLE["accent2"],
        linewidth=2.5,
        zorder=4,
    )
    ax.add_patch(highlight)
    ax.text(
        hx + hw / 2,
        hy + hh / 2,
        "A",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        zorder=5,
    )

    # UV coordinate labels — top-left origin convention:
    #   u0 = x / W,  v0 = y / H,  u1 = (x+w) / W,  v1 = (y+h) / H
    # The glyph positions in the diagram use matplotlib coordinates where
    # y increases upward, but the UV formula treats y as increasing downward
    # from the top-left origin.  For the highlighted glyph at (hx, hy) the
    # atlas-space y equals hy (the diagram values are chosen so this holds).
    inv = 1.0 / atlas_size
    u0 = hx * inv
    v0 = hy * inv
    u1 = (hx + hw) * inv
    v1 = (hy + hh) * inv

    # Corner labels (display UVs directly, no double-flip)
    ax.text(
        hx - 0.1,
        hy + hh + 0.1,
        f"({u0:.2f}, {v0:.2f})",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="right",
        va="bottom",
    )
    ax.text(
        hx + hw + 0.1,
        hy - 0.1,
        f"({u1:.2f}, {v1:.2f})",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="top",
    )

    # Axis labels
    ax.text(
        atlas_size / 2,
        -0.3,
        "u (0.0 to 1.0)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="top",
    )
    ax.text(
        -0.3,
        atlas_size / 2,
        "v (0.0 to 1.0)",
        color=STYLE["text"],
        fontsize=10,
        ha="right",
        va="center",
        rotation=90,
    )

    # Origin and extent labels — top-left UV convention:
    # (0,0) at top-left, (1,0) at top-right, (0,1) at bottom-left
    ax.text(
        0,
        atlas_size + 0.15,
        "(0,0)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
    )
    ax.text(
        atlas_size,
        atlas_size + 0.15,
        "(1,0)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
    )
    ax.text(
        -0.15,
        0,
        "(0,1)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="right",
        va="center",
    )

    # Formula
    ax.text(
        atlas_size / 2,
        -1.5,
        "UV = pixel_position / atlas_dimension\n"
        "u0 = x / W    v0 = y / H    u1 = (x+w) / W    v1 = (y+h) / H",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="top",
        family="monospace",
        bbox=dict(
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            boxstyle="round,pad=0.4",
            alpha=0.9,
        ),
    )

    # Title
    ax.text(
        atlas_size / 2,
        atlas_size + 0.8,
        "UV Coordinate Mapping",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
    )

    save(fig, "ui/03-font-atlas", "uv_coordinates.png")


# ---------------------------------------------------------------------------
# UI Lesson 04 — Text Layout
# ---------------------------------------------------------------------------
