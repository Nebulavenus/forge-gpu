"""Diagrams for gpu/22."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

from .._common import FORGE_CMAP, STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/22-bloom — Jimenez dual-filter bloom diagrams
# ---------------------------------------------------------------------------


def diagram_bloom_pipeline():
    """Full bloom render pipeline: scene → downsample chain → upsample chain → tonemap."""
    fig, ax = plt.subplots(figsize=(14, 7), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-4.5, 5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # --- Shared arrow style ---
    arrow_kw = {
        "arrowstyle": "->,head_width=0.25,head_length=0.12",
        "color": STYLE["text_dim"],
        "lw": 2,
    }
    green_arrow_kw = {
        "arrowstyle": "->,head_width=0.25,head_length=0.12",
        "color": STYLE["accent3"],
        "lw": 2,
    }

    # --- Row 1: Scene pass ---
    scene_box = Rectangle(
        (0.5, 3.0),
        3.0,
        1.6,
        linewidth=2,
        edgecolor=STYLE["accent1"],
        facecolor=STYLE["surface"],
        alpha=0.85,
        zorder=2,
    )
    ax.add_patch(scene_box)
    ax.text(
        2.0,
        4.05,
        "1. Scene Pass",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        2.0,
        3.4,
        "Grid + Models + Emissive\n\u2192 HDR target (CLEAR)",
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Arrow from scene to downsample
    ax.annotate("", xy=(2.0, 2.2), xytext=(2.0, 2.9), arrowprops=arrow_kw)

    # --- Row 2: Downsample chain (left to right, shrinking boxes) ---
    ax.text(
        0.2,
        2.05,
        "2. Downsample (5 passes)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # HDR source box
    hdr_x, hdr_y, hdr_w, hdr_h = 0.3, 0.0, 1.8, 1.6
    hdr_box = Rectangle(
        (hdr_x, hdr_y),
        hdr_w,
        hdr_h,
        linewidth=2,
        edgecolor=STYLE["accent1"],
        facecolor=STYLE["surface"],
        alpha=0.7,
        zorder=2,
    )
    ax.add_patch(hdr_box)
    ax.text(
        hdr_x + hdr_w / 2,
        hdr_y + hdr_h * 0.6,
        "HDR",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        hdr_x + hdr_w / 2,
        hdr_y + hdr_h * 0.25,
        "1280\u00d7720",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    mip_sizes = [
        ("Mip 0", "640\u00d7360"),
        ("Mip 1", "320\u00d7180"),
        ("Mip 2", "160\u00d790"),
        ("Mip 3", "80\u00d745"),
        ("Mip 4", "40\u00d722"),
    ]
    mip_widths = [1.5, 1.3, 1.1, 0.9, 0.8]
    mip_heights = [1.4, 1.2, 1.0, 0.8, 0.7]
    mip_x_starts = []
    cx = hdr_x + hdr_w + 0.6
    for i, ((label, size), mw, mh) in enumerate(
        zip(mip_sizes, mip_widths, mip_heights, strict=True)
    ):
        my = hdr_y + (hdr_h - mh) / 2  # vertically center
        rect = Rectangle(
            (cx, my),
            mw,
            mh,
            linewidth=1.5,
            edgecolor=STYLE["accent2"],
            facecolor=STYLE["surface"],
            alpha=0.7,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx + mw / 2,
            my + mh * 0.6,
            label,
            color=STYLE["accent2"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            cx + mw / 2,
            my + mh * 0.25,
            size,
            color=STYLE["text_dim"],
            fontsize=6.5,
            ha="center",
            va="center",
            path_effects=stroke,
        )

        # Arrow from previous box
        if i == 0:
            ax.annotate(
                "",
                xy=(cx, hdr_y + hdr_h / 2),
                xytext=(hdr_x + hdr_w, hdr_y + hdr_h / 2),
                arrowprops=arrow_kw,
            )
        else:
            prev_x = mip_x_starts[-1] + mip_widths[i - 1]
            ax.annotate(
                "",
                xy=(cx, hdr_y + hdr_h / 2),
                xytext=(prev_x, hdr_y + hdr_h / 2),
                arrowprops=arrow_kw,
            )

        # First pass annotation
        if i == 0:
            ax.text(
                cx + mw / 2,
                my - 0.3,
                "Threshold\n+ Karis",
                color=STYLE["warn"],
                fontsize=7,
                fontweight="bold",
                ha="center",
                va="top",
                path_effects=stroke,
            )

        mip_x_starts.append(cx)
        cx += mw + 0.4

    # --- Row 3: Upsample chain (right to left, aligned below downsample) ---
    # Each upsample mip sits directly below its downsample counterpart.
    # Arrows flow right-to-left: Mip 4 → Mip 3 → ... → Mip 0.
    ax.text(
        0.2,
        -1.2,
        "3. Upsample (4 passes, additive blend)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
    )

    us_y_base = -3.6
    us_positions = []  # (x, y, w, h) indexed by mip level 0..4
    for i, ((label, size), mw, mh) in enumerate(
        zip(mip_sizes, mip_widths, mip_heights, strict=True)
    ):
        ux = mip_x_starts[i]
        uy = us_y_base + (hdr_h - mh) / 2
        rect = Rectangle(
            (ux, uy),
            mw,
            mh,
            linewidth=1.5,
            edgecolor=STYLE["accent3"],
            facecolor=STYLE["surface"],
            alpha=0.7,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            ux + mw / 2,
            uy + mh * 0.6,
            label,
            color=STYLE["accent3"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            ux + mw / 2,
            uy + mh * 0.25,
            size,
            color=STYLE["text_dim"],
            fontsize=6.5,
            ha="center",
            va="center",
            path_effects=stroke,
        )
        us_positions.append((ux, uy, mw, mh))

    # Arrows between upsample mips (right to left: Mip 4 → Mip 3 → ... → Mip 0)
    us_mid_y = us_y_base + hdr_h / 2
    for i in range(4, 0, -1):
        src_x = us_positions[i][0]
        dst_x, _, dst_w, _ = us_positions[i - 1]
        ax.annotate(
            "",
            xy=(dst_x + dst_w, us_mid_y),
            xytext=(src_x, us_mid_y),
            arrowprops=green_arrow_kw,
        )
        # "+" label between each pair
        gap_mid = (src_x + dst_x + dst_w) / 2
        ax.text(
            gap_mid,
            us_mid_y + 0.25,
            "+",
            color=STYLE["accent3"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Straight vertical arrow from downsample Mip 4 to upsample Mip 4
    mip4_cx = mip_x_starts[4] + mip_widths[4] / 2
    ds_mip4_bottom = hdr_y + (hdr_h - mip_heights[4]) / 2
    us_mip4_top = us_positions[4][1] + us_positions[4][3]
    ax.annotate(
        "",
        xy=(mip4_cx, us_mip4_top),
        xytext=(mip4_cx, ds_mip4_bottom),
        arrowprops=green_arrow_kw,
    )

    # --- Tone Map pass (below HDR, at the left end of the upsample row) ---
    tm_w, tm_h = 2.0, 1.4
    tm_x = hdr_x
    tm_y = us_y_base + (hdr_h - tm_h) / 2
    tm_box = Rectangle(
        (tm_x, tm_y),
        tm_w,
        tm_h,
        linewidth=2,
        edgecolor=STYLE["accent4"],
        facecolor=STYLE["surface"],
        alpha=0.85,
        zorder=2,
    )
    ax.add_patch(tm_box)
    ax.text(
        tm_x + tm_w / 2,
        tm_y + tm_h * 0.7,
        "4. Tone Map",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        tm_x + tm_w / 2,
        tm_y + tm_h * 0.3,
        "HDR + Bloom \u00d7 intensity\n\u2192 swapchain",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Arrow from upsample Mip 0 to Tone Map
    us_mip0_x = us_positions[0][0]
    ax.annotate(
        "",
        xy=(tm_x + tm_w, us_mid_y),
        xytext=(us_mip0_x, us_mid_y),
        arrowprops=green_arrow_kw,
    )

    # Straight dashed arrow from HDR down to Tone Map
    hdr_cx = hdr_x + hdr_w / 2
    ax.annotate(
        "",
        xy=(hdr_cx, tm_y + tm_h),
        xytext=(hdr_cx, hdr_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": STYLE["accent1"],
            "lw": 1.5,
            "linestyle": "--",
        },
    )
    ax.text(
        hdr_cx - 0.15,
        -0.5,
        "HDR input",
        color=STYLE["accent1"],
        fontsize=7.5,
        fontstyle="italic",
        ha="right",
        va="center",
        path_effects=stroke,
    )

    fig.suptitle(
        "Bloom Pipeline \u2014 Jimenez Dual-Filter Method",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "bloom_pipeline.png")


def diagram_downsample_13tap():
    """13-tap weighted downsample with 5 overlapping 2x2 box regions."""
    fig = plt.figure(figsize=(12, 6), facecolor=STYLE["bg"])

    # Left panel: sample positions on a 5x5 grid
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-0.8, 4.8), ylim=(-0.8, 4.8), grid=False)

    # Background grid
    for i in range(5):
        ax1.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)
        ax1.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)

    # 13 sample positions (mapped to 0..4 grid coordinates)
    # The shader samples at: (-2,-2), (0,-2), (2,-2), (-1,-1), (1,-1),
    # (-2,0), (0,0), (2,0), (-1,1), (1,1), (-2,2), (0,2), (2,2)
    # Map to grid: offset by 2 so center is at (2,2)
    samples = {
        "a": (0, 4),
        "b": (2, 4),
        "c": (4, 4),
        "d": (1, 3),
        "e": (3, 3),
        "f": (0, 2),
        "g": (2, 2),
        "h": (4, 2),
        "i": (1, 1),
        "j": (3, 1),
        "k": (0, 0),
        "l": (2, 0),
        "m": (4, 0),
    }

    # Draw all sample points
    for name, (sx, sy) in samples.items():
        ax1.plot(
            sx,
            sy,
            "o",
            color=STYLE["text"],
            markersize=10,
            zorder=5,
        )
        ax1.text(
            sx,
            sy + 0.3,
            name,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax1.set_title(
        "13 Sample Positions",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax1.set_xticks([])
    ax1.set_yticks([])

    # Right panel: the 5 overlapping boxes
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-0.8, 4.8), ylim=(-0.8, 4.8), grid=False)

    # Background grid
    for i in range(5):
        ax2.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)
        ax2.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)

    # Draw sample points (dimmed)
    for _name, (sx, sy) in samples.items():
        ax2.plot(
            sx,
            sy,
            "o",
            color=STYLE["text_dim"],
            markersize=6,
            zorder=5,
        )

    # 5 overlapping boxes with color coding
    boxes = [
        (
            "Top-Left",
            [(0, 2), (2, 2), (2, 4), (0, 4)],
            STYLE["accent1"],
            "a,b,f,g",
            0.125,
        ),
        (
            "Top-Right",
            [(2, 2), (4, 2), (4, 4), (2, 4)],
            STYLE["accent2"],
            "b,c,g,h",
            0.125,
        ),
        (
            "Bot-Left",
            [(0, 0), (2, 0), (2, 2), (0, 2)],
            STYLE["accent3"],
            "f,g,k,l",
            0.125,
        ),
        (
            "Bot-Right",
            [(2, 0), (4, 0), (4, 2), (2, 2)],
            STYLE["accent4"],
            "g,h,l,m",
            0.125,
        ),
        ("Center", [(1, 1), (3, 1), (3, 3), (1, 3)], STYLE["warn"], "d,e,i,j", 0.500),
    ]

    for _label, verts, color, _samp_names, _weight in boxes:
        poly = Polygon(
            verts,
            closed=True,
            facecolor=color,
            edgecolor=color,
            alpha=0.15,
            linewidth=2,
            zorder=1,
        )
        ax2.add_patch(poly)
        # Box outline
        poly_outline = Polygon(
            verts,
            closed=True,
            facecolor="none",
            edgecolor=color,
            alpha=0.7,
            linewidth=2,
            zorder=3,
        )
        ax2.add_patch(poly_outline)

    # Weight annotations below
    weight_text = (
        "Weights: corner boxes \u00d7 0.125  |  center box \u00d7 0.500  |  total = 1.0"
    )
    ax2.text(
        2.0,
        -0.6,
        weight_text,
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
    )

    # Legend entries
    for ci, (label, _, color, _, weight) in enumerate(boxes):
        ax2.text(
            4.8,
            4.6 - ci * 0.35,
            f"\u25a0 {label} (\u00d7{weight:.3f})",
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="right",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax2.set_title(
        "5 Overlapping 2\u00d72 Boxes",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])

    fig.suptitle(
        "13-Tap Downsample Filter \u2014 Jimenez (SIGGRAPH 2014)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "downsample_13tap.png")


def diagram_tent_filter():
    """Tent (bilinear) filter kernel: 2D heatmap + 3D surface plot."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    fig = plt.figure(figsize=(12, 5), facecolor=STYLE["bg"])

    # Kernel weights
    kernel = (
        np.array(
            [
                [1, 2, 1],
                [2, 4, 2],
                [1, 2, 1],
            ]
        )
        / 16.0
    )

    # --- Left: Annotated 2D grid ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, grid=False, aspect="equal")

    # Draw the kernel as colored squares
    for row in range(3):
        for col in range(3):
            w = kernel[row, col]
            # Color intensity based on weight
            intensity = w / kernel.max()
            r_bg = int(STYLE["bg"][1:3], 16)
            g_bg = int(STYLE["bg"][3:5], 16)
            b_bg = int(STYLE["bg"][5:7], 16)
            r_ac = int(STYLE["accent1"][1:3], 16)
            g_ac = int(STYLE["accent1"][3:5], 16)
            b_ac = int(STYLE["accent1"][5:7], 16)
            r = int(r_bg + (r_ac - r_bg) * intensity)
            g = int(g_bg + (g_ac - g_bg) * intensity)
            b = int(b_bg + (b_ac - b_bg) * intensity)
            cell_color = f"#{r:02x}{g:02x}{b:02x}"

            rect = Rectangle(
                (col - 0.45, 2 - row - 0.45),
                0.9,
                0.9,
                facecolor=cell_color,
                edgecolor=STYLE["accent1"],
                linewidth=1.5,
                alpha=0.85,
                zorder=2,
            )
            ax1.add_patch(rect)

            # Weight text
            frac_text = f"{int(w * 16)}/16"
            ax1.text(
                col,
                2 - row,
                frac_text,
                color=STYLE["text"],
                fontsize=13,
                fontweight="bold",
                ha="center",
                va="center",
                zorder=3,
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

    ax1.set_xlim(-0.8, 2.8)
    ax1.set_ylim(-0.8, 2.8)
    ax1.set_xticks([])
    ax1.set_yticks([])

    # Axis labels
    for ci, label in enumerate(["-1", "0", "+1"]):
        ax1.text(
            ci,
            -0.65,
            label,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
        )
        ax1.text(
            -0.65,
            ci,
            label,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
        )

    ax1.set_title(
        "9-Tap Tent Kernel Weights",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # --- Right: 3D surface showing the "tent" shape ---
    ax2: Axes3D = fig.add_subplot(122, projection="3d")  # type: ignore[assignment]
    ax2.set_facecolor(STYLE["bg"])

    # Higher-res tent for smooth surface
    x = np.linspace(-1, 1, 50)
    y = np.linspace(-1, 1, 50)
    X, Y = np.meshgrid(x, y)
    # 2D tent = (1 - |x|) * (1 - |y|)
    Z = np.maximum(0, 1 - np.abs(X)) * np.maximum(0, 1 - np.abs(Y))

    ax2.plot_surface(
        X,
        Y,
        Z,
        cmap=FORGE_CMAP,
        alpha=0.85,
        edgecolor="none",
        rstride=2,
        cstride=2,
    )

    # Mark the 9 sample points on the surface
    sample_coords = [
        (-1, -1),
        (0, -1),
        (1, -1),
        (-1, 0),
        (0, 0),
        (1, 0),
        (-1, 1),
        (0, 1),
        (1, 1),
    ]
    for sx, sy in sample_coords:
        sz = max(0, 1 - abs(sx)) * max(0, 1 - abs(sy))
        ax2.scatter(
            [sx],
            [sy],
            [sz + 0.02],  # type: ignore[arg-type]  # zs typed as int in stubs
            color=STYLE["text"],
            s=30,
            zorder=10,
            depthshade=False,
        )

    ax2.set_xlabel("x", color=STYLE["text_dim"], fontsize=9, labelpad=-2)
    ax2.set_ylabel("y", color=STYLE["text_dim"], fontsize=9, labelpad=-2)
    ax2.set_zlabel("Weight", color=STYLE["text_dim"], fontsize=9, labelpad=-2)
    ax2.tick_params(colors=STYLE["axis"], labelsize=7)
    ax2.set_title(
        '"Tent" Shape \u2014 Bilinear Falloff',
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax2.view_init(elev=30, azim=-50)
    # Style the 3D axes panes
    ax2.xaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax2.yaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax2.zaxis.pane.set_facecolor(STYLE["surface"])
    ax2.xaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax2.yaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax2.zaxis.pane.set_edgecolor(STYLE["grid"])

    fig.suptitle(
        "Tent Filter \u2014 9-Tap Bilinear Upsample Kernel",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "tent_filter.png")


def diagram_karis_averaging():
    """Karis averaging: how 1/(1+luminance) weighting suppresses fireflies."""
    fig, (ax1, ax2) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [1, 1]},
    )

    # --- Left: Weight curve ---
    setup_axes(ax1, grid=True, aspect=None)
    luma = np.linspace(0, 20, 500)
    weight = 1.0 / (1.0 + luma)

    ax1.plot(luma, weight, color=STYLE["accent1"], lw=2.5)
    ax1.fill_between(luma, weight, alpha=0.15, color=STYLE["accent1"])

    # Mark key points
    for lv, label in [(0.5, "Dim pixel"), (2.0, "Bright"), (10.0, "Firefly")]:
        w = 1.0 / (1.0 + lv)
        ax1.plot(lv, w, "o", color=STYLE["warn"], markersize=8, zorder=5)
        ax1.annotate(
            f"{label}\nluma={lv:.1f}\nw={w:.3f}",
            xy=(lv, w),
            xytext=(lv + 1.5, w + 0.12),
            color=STYLE["warn"],
            fontsize=8,
            fontweight="bold",
            arrowprops={
                "arrowstyle": "->",
                "color": STYLE["warn"],
                "lw": 1.5,
                "connectionstyle": "arc3,rad=0.2",
            },
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax1.set_xlim(0, 20)
    ax1.set_ylim(0, 1.1)
    ax1.set_xlabel("Luminance", color=STYLE["text"], fontsize=11)
    ax1.set_ylabel("Karis weight = 1 / (1 + luma)", color=STYLE["text"], fontsize=11)
    ax1.set_title(
        "Karis Weight Function",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # --- Right: Bar chart comparing uniform vs Karis-weighted box average ---
    setup_axes(ax2, grid=True, aspect=None)

    # Scenario: one firefly pixel among normal pixels in a 2x2 box
    pixels = [0.5, 0.6, 0.4, 50.0]  # last one is a firefly
    pixel_labels = ["0.5", "0.6", "0.4", "50.0"]

    # Uniform average
    uniform_avg = np.mean(pixels)

    # Karis-weighted average
    weights_k = [1.0 / (1.0 + p) for p in pixels]
    w_sum = sum(weights_k)
    karis_avg = sum(p * w / w_sum for p, w in zip(pixels, weights_k, strict=True))

    bar_x = np.arange(3)
    bar_labels = ["Uniform\naverage", "Karis\naverage", "Without\nfirefly"]
    bar_values = [uniform_avg, karis_avg, np.mean(pixels[:3])]
    bar_colors = [STYLE["accent2"], STYLE["accent1"], STYLE["accent3"]]

    bars = ax2.bar(bar_x, bar_values, color=bar_colors, width=0.6, alpha=0.85)

    # Value labels on bars
    for bar, val in zip(bars, bar_values, strict=True):
        ax2.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.4,
            f"{val:.2f}",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax2.set_xticks(bar_x)
    ax2.set_xticklabels(bar_labels, color=STYLE["text"], fontsize=9)
    ax2.set_ylim(0, max(bar_values) * 1.3)
    ax2.set_ylabel("Result", color=STYLE["text"], fontsize=11)

    # Input pixels annotation
    ax2.text(
        1.0,
        max(bar_values) * 1.2,
        f"Input pixels: {', '.join(pixel_labels)}",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax2.set_title(
        "Firefly Suppression \u2014 2\u00d72 Box",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    fig.suptitle(
        "Karis Averaging \u2014 Suppressing HDR Fireflies",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "karis_averaging.png")


def diagram_mip_chain_flow():
    """Mip-to-mip data flow for downsample (pull) and upsample (push) phases."""
    fig, (ax1, ax2) = plt.subplots(
        2,
        1,
        figsize=(12, 8),
        facecolor=STYLE["bg"],
        gridspec_kw={"height_ratios": [1, 1]},
    )

    for ax in (ax1, ax2):
        setup_axes(ax, grid=False, aspect=None)
        ax.axis("off")

    # Mip levels with proportional sizing
    mip_labels = [
        "HDR\n1280\u00d7720",
        "Mip 0\n640\u00d7360",
        "Mip 1\n320\u00d7180",
        "Mip 2\n160\u00d790",
        "Mip 3\n80\u00d745",
        "Mip 4\n40\u00d722",
    ]
    mip_scales = [1.0, 0.85, 0.7, 0.55, 0.42, 0.32]

    arrow_kw = {
        "arrowstyle": "->,head_width=0.3,head_length=0.15",
        "lw": 2.5,
    }

    # --- Top: Downsample (reading from larger, writing to smaller) ---
    ax1.set_xlim(-0.5, 12.5)
    ax1.set_ylim(-1.5, 2.5)

    ds_x_positions = [0.0, 2.2, 4.2, 6.0, 7.6, 9.0]
    ds_box_widths = [s * 1.8 for s in mip_scales]
    ds_box_heights = [s * 1.6 for s in mip_scales]

    for i, (x, label, bw, bh) in enumerate(
        zip(ds_x_positions, mip_labels, ds_box_widths, ds_box_heights, strict=True)
    ):
        color = STYLE["accent1"] if i == 0 else STYLE["accent2"]
        y = (1.6 - bh) / 2  # vertically center
        rect = Rectangle(
            (x, y),
            bw,
            bh,
            linewidth=2,
            edgecolor=color,
            facecolor=STYLE["surface"],
            alpha=0.8,
            zorder=2,
        )
        ax1.add_patch(rect)
        ax1.text(
            x + bw / 2,
            y + bh / 2,
            label,
            color=color,
            fontsize=8 if i > 0 else 9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Arrows and "read/write" labels
        if i > 0:
            prev_x = ds_x_positions[i - 1] + ds_box_widths[i - 1]
            ax1.annotate(
                "",
                xy=(x, 0.8),
                xytext=(prev_x, 0.8),
                arrowprops={**arrow_kw, "color": STYLE["accent2"]},
            )
            # "Read → Write" label
            mid_x = (x + prev_x) / 2
            ax1.text(
                mid_x,
                1.7,
                "read \u2192 write",
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="center",
            )

    # First pass special annotation
    ax1.text(
        ds_x_positions[1] + ds_box_widths[1] / 2,
        -0.7,
        "Pass 0: Threshold + Karis averaging",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
    )
    ax1.text(
        6.5,
        -0.7,
        "Passes 1\u20134: Standard 13-tap weighting",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
    )

    ax1.set_title(
        "Downsample: Pull from Larger Mip \u2192 Write to Smaller Mip",
        color=STYLE["accent2"],
        fontsize=13,
        fontweight="bold",
        pad=15,
    )

    # Direction label
    ax1.annotate(
        "",
        xy=(11.0, 2.2),
        xytext=(10.0, 2.2),
        arrowprops={**arrow_kw, "color": STYLE["accent2"]},
    )
    ax1.text(
        11.2,
        2.2,
        "Resolution shrinks",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
    )

    # --- Bottom: Upsample (reading from smaller, blending into larger) ---
    ax2.set_xlim(-0.5, 12.5)
    ax2.set_ylim(-1.5, 2.5)

    # Reverse order: start from smallest
    us_labels = list(reversed(mip_labels[1:]))  # Mip4 → Mip0 (no HDR)
    us_scales = list(reversed(mip_scales[1:]))
    us_x_positions = [0.0, 1.6, 3.4, 5.4, 7.6]
    us_box_widths = [s * 1.8 for s in us_scales]
    us_box_heights = [s * 1.6 for s in us_scales]

    for i, (x, label, bw, bh) in enumerate(
        zip(us_x_positions, us_labels, us_box_widths, us_box_heights, strict=True)
    ):
        y = (1.6 - bh) / 2
        rect = Rectangle(
            (x, y),
            bw,
            bh,
            linewidth=2,
            edgecolor=STYLE["accent3"],
            facecolor=STYLE["surface"],
            alpha=0.8,
            zorder=2,
        )
        ax2.add_patch(rect)
        ax2.text(
            x + bw / 2,
            y + bh / 2,
            label,
            color=STYLE["accent3"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        if i > 0:
            prev_x = us_x_positions[i - 1] + us_box_widths[i - 1]
            ax2.annotate(
                "",
                xy=(x, 0.8),
                xytext=(prev_x, 0.8),
                arrowprops={**arrow_kw, "color": STYLE["accent3"]},
            )
            mid_x = (x + prev_x) / 2
            ax2.text(
                mid_x,
                1.7,
                "read \u2192 ADD",
                color=STYLE["accent3"],
                fontsize=7.5,
                fontweight="bold",
                ha="center",
                va="center",
            )

    ax2.text(
        4.0,
        -0.7,
        "Each pass: tent-filter upsample from smaller mip, "
        "additively blend (ONE + ONE) into larger",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
    )

    ax2.set_title(
        "Upsample: Read from Smaller Mip \u2192 Additively Blend into Larger Mip",
        color=STYLE["accent3"],
        fontsize=13,
        fontweight="bold",
        pad=15,
    )

    # Direction label
    ax2.annotate(
        "",
        xy=(11.0, 2.2),
        xytext=(10.0, 2.2),
        arrowprops={**arrow_kw, "color": STYLE["accent3"]},
    )
    ax2.text(
        11.2,
        2.2,
        "Resolution grows",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
    )

    fig.suptitle(
        "Mip Chain Data Flow \u2014 Downsample vs. Upsample",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "gpu/22-bloom", "mip_chain_flow.png")


def diagram_brightness_threshold():
    """How brightness thresholding selects bloom-contributing pixels."""
    fig, (ax1, ax2) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [3, 2]},
    )

    # --- Left: Threshold function ---
    setup_axes(ax1, grid=True, aspect=None)

    luma = np.linspace(0, 5, 500)
    threshold = 1.0

    # contribution = max(luma - threshold, 0) / max(luma, 0.0001) * luma
    # Effectively: pixel * max(luma - threshold, 0) / max(luma, 0.0001)
    # For a white pixel (rgb all equal to luma), output = max(luma - thresh, 0)
    contribution = np.maximum(luma - threshold, 0)

    ax1.plot(
        luma,
        luma,
        "--",
        color=STYLE["text_dim"],
        lw=1.5,
        alpha=0.5,
        label="Original (no threshold)",
    )
    ax1.plot(
        luma,
        contribution,
        color=STYLE["accent1"],
        lw=2.5,
        label=f"After threshold = {threshold:.1f}",
    )
    ax1.fill_between(luma, contribution, alpha=0.12, color=STYLE["accent1"])

    # Mark the threshold point
    ax1.axvline(threshold, color=STYLE["warn"], ls="--", lw=1.5, alpha=0.7)
    ax1.text(
        threshold + 0.1,
        4.2,
        f"Threshold = {threshold:.1f}",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Shade the "no bloom" region
    ax1.axvspan(0, threshold, alpha=0.08, color=STYLE["accent2"])
    ax1.text(
        threshold / 2,
        0.5,
        "No bloom",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax1.text(
        threshold + 1.5,
        0.5,
        "Bloom",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax1.set_xlim(0, 5)
    ax1.set_ylim(0, 4.5)
    ax1.set_xlabel("Pixel luminance", color=STYLE["text"], fontsize=11)
    ax1.set_ylabel("Bloom contribution", color=STYLE["text"], fontsize=11)

    legend = ax1.legend(
        loc="upper left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in legend.get_texts():
        text.set_color(STYLE["text"])

    ax1.set_title(
        "Brightness Threshold Function",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # --- Right: Example scene values ---
    setup_axes(ax2, grid=True, aspect=None)

    # Example pixel luminances from different scene elements
    elements = [
        ("Shadow", 0.1, STYLE["text_dim"]),
        ("Diffuse", 0.4, STYLE["accent1"]),
        ("Specular", 1.8, STYLE["accent2"]),
        ("Emissive", 50.0, STYLE["warn"]),
    ]

    y_pos = np.arange(len(elements))
    bars = []
    for i, (_name, luma_val, color) in enumerate(elements):
        # Show log scale for the bars since emissive is so bright
        bar_val = np.log10(luma_val + 1) * 2  # log scale for visual
        b = ax2.barh(i, bar_val, color=color, height=0.6, alpha=0.85)
        bars.append(b)
        ax2.text(
            bar_val + 0.1,
            i,
            f"luma = {luma_val}",
            color=color,
            fontsize=9,
            fontweight="bold",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Threshold line
    thresh_bar = np.log10(threshold + 1) * 2
    ax2.axvline(thresh_bar, color=STYLE["warn"], ls="--", lw=2, alpha=0.7)

    # Labels for bloom/no bloom
    for i, (_name, luma_val, _color) in enumerate(elements):
        blooms = luma_val > threshold
        status = "\u2714 Blooms" if blooms else "\u2718 No bloom"
        status_color = STYLE["accent3"] if blooms else STYLE["accent2"]
        ax2.text(
            -0.1,
            i,
            status,
            color=status_color,
            fontsize=8,
            fontweight="bold",
            ha="right",
            va="center",
        )

    ax2.set_yticks(y_pos)
    ax2.set_yticklabels([e[0] for e in elements], color=STYLE["text"], fontsize=10)
    ax2.set_xlim(-1.5, 5)
    ax2.set_xticks([])
    ax2.set_title(
        "Scene Elements at Threshold = 1.0",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    fig.suptitle(
        "Brightness Thresholding \u2014 Selecting Bloom Pixels",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "brightness_threshold.png")


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — cube_face_layout.png
# ---------------------------------------------------------------------------
