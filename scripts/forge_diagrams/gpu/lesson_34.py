"""Diagrams for gpu/34."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — stencil_buffer_concept.png
# ---------------------------------------------------------------------------


def diagram_stencil_buffer_concept():
    """Three framebuffer attachments: color, depth, and stencil in a shared depth-stencil texture."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    titles = ["Color Buffer", "Depth Buffer", "Stencil Buffer"]
    colors = [STYLE["accent1"], STYLE["text_dim"], STYLE["accent2"]]

    for ax in axes:
        setup_axes(ax, xlim=(0, 8), ylim=(0, 8), grid=False, aspect="equal")
        ax.set_xticks([])
        ax.set_yticks([])

    # --- Color buffer: colored rectangles representing rendered objects ---
    ax_c = axes[0]
    ax_c.set_title(titles[0], color=colors[0], fontsize=13, fontweight="bold", pad=12)

    bg_rect = Rectangle(
        (0.5, 0.5),
        7,
        7,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=1,
    )
    ax_c.add_patch(bg_rect)
    # Simplified scene objects
    obj1 = Rectangle(
        (1.5, 4.0),
        2.5,
        2.0,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.85,
        zorder=2,
    )
    obj2 = Rectangle(
        (4.0, 2.0),
        2.0,
        3.0,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.85,
        zorder=2,
    )
    obj3 = Rectangle(
        (2.0, 1.0),
        4.0,
        1.5,
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.85,
        zorder=2,
    )
    for p in (obj1, obj2, obj3):
        ax_c.add_patch(p)
    ax_c.text(
        4.0,
        0.15,
        "RGBA8",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # --- Depth buffer: grayscale gradient ---
    ax_d = axes[1]
    ax_d.set_title(titles[1], color=colors[1], fontsize=13, fontweight="bold", pad=12)

    grad = np.linspace(0.2, 0.9, 64).reshape(1, -1)
    grad = np.tile(grad, (64, 1))
    # Add some depth variation
    y_grad = np.linspace(0.3, 0.8, 64).reshape(-1, 1)
    depth_img = grad * 0.6 + y_grad * 0.4
    ax_d.imshow(
        depth_img,
        extent=(0.5, 7.5, 0.5, 7.5),
        cmap="gray",
        vmin=0,
        vmax=1,
        aspect="auto",
        zorder=1,
    )
    border = Rectangle(
        (0.5, 0.5),
        7,
        7,
        facecolor="none",
        edgecolor=STYLE["text_dim"],
        linewidth=2,
        zorder=3,
    )
    ax_d.add_patch(border)
    ax_d.text(
        4.0,
        7.8,
        "Near = 0.0",
        color=STYLE["text"],
        fontsize=8,
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )
    ax_d.text(
        4.0,
        0.15,
        "Far = 1.0",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # --- Stencil buffer: integer values shown as colored cells ---
    ax_s = axes[2]
    ax_s.set_title(titles[2], color=colors[2], fontsize=13, fontweight="bold", pad=12)
    stencil_bg = Rectangle(
        (0.5, 0.5),
        7,
        7,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=1,
    )
    ax_s.add_patch(stencil_bg)

    stencil_colors = {"0": STYLE["bg"], "1": STYLE["accent2"], "2": STYLE["accent3"]}
    stencil_labels = {"0": STYLE["text_dim"], "1": STYLE["warn"], "2": STYLE["text"]}

    # Grid of stencil values
    grid_data = [
        [0, 0, 0, 0, 0, 0, 0],
        [0, 0, 1, 1, 1, 0, 0],
        [0, 0, 1, 1, 1, 0, 0],
        [0, 0, 1, 2, 2, 2, 0],
        [0, 0, 0, 2, 2, 2, 0],
        [0, 0, 0, 2, 2, 2, 0],
        [0, 0, 0, 0, 0, 0, 0],
    ]
    cell_size = 1.0
    for row_i, row in enumerate(grid_data):
        for col_i, val in enumerate(row):
            x = 0.5 + col_i * cell_size
            y = 6.5 - row_i * cell_size
            c = stencil_colors[str(val)]
            cell = Rectangle(
                (x, y),
                cell_size,
                cell_size,
                facecolor=c,
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                zorder=2,
            )
            ax_s.add_patch(cell)
            ax_s.text(
                x + cell_size / 2,
                y + cell_size / 2,
                str(val),
                color=stencil_labels[str(val)],
                fontsize=7,
                fontweight="bold",
                ha="center",
                va="center",
                zorder=5,
            )

    # Legend for stencil values
    ax_s.text(
        4.0,
        0.15,
        "8-bit integer (0-255)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # Annotation linking depth and stencil
    fig.text(
        0.65,
        0.02,
        "Depth + Stencil share one texture: D24_UNORM_S8_UINT or "
        "D32_FLOAT_S8_UINT (platform-dependent depth + 8-bit stencil)",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    fig.suptitle(
        "Framebuffer Attachments",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=[0, 0.06, 1, 0.94])
    save(fig, "gpu/34-stencil-testing", "stencil_buffer_concept.png")


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — stencil_test_pipeline.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — stencil_test_pipeline.png
# ---------------------------------------------------------------------------


def diagram_stencil_test_pipeline():
    """Fragment pipeline flow with stencil test branching and comparison formula."""

    fig, ax = plt.subplots(1, 1, figsize=(14, 7), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 14), ylim=(0, 8), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    def draw_box(cx, cy, w, h, label, color, fontsize=10, fc=None):
        facecolor = fc if fc else STYLE["surface"]
        rect = FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=facecolor,
            edgecolor=color,
            linewidth=2.5,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy,
            label,
            color=color,
            fontsize=fontsize,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    def draw_arrow(x1, y1, x2, y2, color, label=None, label_pos=None):
        ax.annotate(
            "",
            xy=(x2, y2),
            xytext=(x1, y1),
            arrowprops={
                "arrowstyle": "->,head_width=0.35,head_length=0.18",
                "color": color,
                "lw": 2.2,
            },
            zorder=3,
        )
        if label and label_pos:
            ax.text(
                label_pos[0],
                label_pos[1],
                label,
                color=color,
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    # Main pipeline boxes
    draw_box(1.5, 6.0, 2.2, 1.0, "Fragment", STYLE["accent1"])
    draw_box(4.8, 6.0, 2.4, 1.0, "Stencil Test", STYLE["accent2"])
    draw_box(8.5, 6.0, 2.2, 1.0, "Depth Test", STYLE["accent3"])
    draw_box(12.0, 6.0, 2.2, 1.0, "Color Write", STYLE["warn"])

    # Main flow arrows
    draw_arrow(2.6, 6.0, 3.6, 6.0, STYLE["text_dim"])
    draw_arrow(6.0, 6.0, 7.4, 6.0, STYLE["text_dim"], "PASS", (6.7, 6.3))
    draw_arrow(9.6, 6.0, 10.9, 6.0, STYLE["text_dim"], "PASS", (10.25, 6.3))

    # Stencil test branches
    # Fail branch (down-left)
    draw_arrow(4.0, 5.5, 2.5, 4.0, STYLE["accent2"], "FAIL", (3.0, 4.9))
    draw_box(2.5, 3.3, 2.0, 0.9, "fail_op", STYLE["accent2"], fontsize=9)

    # Depth fail branch (down-center)
    draw_arrow(8.5, 5.5, 7.0, 4.0, STYLE["accent3"], "DEPTH\nFAIL", (7.5, 4.8))
    draw_box(7.0, 3.3, 2.2, 0.9, "depth_fail_op", STYLE["accent3"], fontsize=9)

    # Both pass branch (down-right)
    draw_arrow(9.6, 5.5, 11.0, 4.0, STYLE["warn"], "BOTH\nPASS", (10.5, 4.8))
    draw_box(11.0, 3.3, 2.0, 0.9, "pass_op", STYLE["warn"], fontsize=9)

    # Comparison formula
    formula_y = 1.6
    ax.text(
        7.0,
        formula_y + 0.6,
        "Comparison Formula:",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        7.0,
        formula_y,
        "(stencil_buffer & compare_mask)  OP  (ref & compare_mask)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        fontfamily="monospace",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Operations list
    ops_text = (
        "Operations:  KEEP | ZERO | REPLACE | INCREMENT_AND_CLAMP | "
        "DECREMENT_AND_CLAMP | INVERT | INCREMENT_AND_WRAP | DECREMENT_AND_WRAP"
    )
    ax.text(
        7.0,
        0.5,
        ops_text,
        color=STYLE["text_dim"],
        fontsize=7.5,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.suptitle(
        "Stencil Test Pipeline",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, "gpu/34-stencil-testing", "stencil_test_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — portal_technique.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — portal_technique.png
# ---------------------------------------------------------------------------


def diagram_portal_technique():
    """4-panel portal rendering steps using stencil masking."""

    fig, axes = plt.subplots(2, 2, figsize=(12, 10), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    panel_info = [
        ("1. Draw Main World", "Depth-writing occluders first"),
        ("2. Write Stencil Mask", "Portal rect = stencil 1 (depth blocks mask)"),
        ("3. Draw Portal World (stencil==1)", "Objects inside portal only"),
        ("4. Final Composite", "Portal frame + outlines"),
    ]

    for idx, ax in enumerate(axes.flat):
        setup_axes(ax, xlim=(0, 10), ylim=(0, 8), grid=False, aspect="equal")
        ax.set_xticks([])
        ax.set_yticks([])
        title, subtitle = panel_info[idx]
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold", pad=12)
        ax.text(
            5.0,
            0.3,
            subtitle,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Panel 1: Stencil mask — dark bg with red portal region
    ax1 = axes[0, 0]
    bg = Rectangle(
        (0.2, 0.6),
        9.6,
        7.0,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        zorder=1,
    )
    ax1.add_patch(bg)
    portal_mask = Rectangle(
        (3.0, 2.5),
        4.0,
        3.5,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        alpha=0.8,
        zorder=2,
    )
    ax1.add_patch(portal_mask)
    ax1.text(
        5.0,
        4.25,
        "stencil = 1",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax1.text(
        1.5,
        6.5,
        "0",
        color=STYLE["text_dim"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Panel 2: Portal world — objects only inside portal rect
    ax2 = axes[0, 1]
    bg2 = Rectangle(
        (0.2, 0.6),
        9.6,
        7.0,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        zorder=1,
    )
    ax2.add_patch(bg2)
    portal_view = Rectangle(
        (3.0, 2.5),
        4.0,
        3.5,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
        alpha=0.6,
        zorder=2,
    )
    ax2.add_patch(portal_view)
    # Objects in portal world
    p_obj1 = Rectangle(
        (3.5, 3.0),
        1.5,
        1.2,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    p_obj2 = Rectangle(
        (5.2, 4.0),
        1.2,
        1.5,
        facecolor=STYLE["accent4"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    ax2.add_patch(p_obj1)
    ax2.add_patch(p_obj2)

    # Panel 3: Main world — objects only outside portal
    ax3 = axes[1, 0]
    bg3 = Rectangle(
        (0.2, 0.6),
        9.6,
        7.0,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        zorder=1,
    )
    ax3.add_patch(bg3)
    # Blocked portal region
    portal_block = Rectangle(
        (3.0, 2.5),
        4.0,
        3.5,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
        linestyle="--",
        alpha=0.6,
        zorder=2,
    )
    ax3.add_patch(portal_block)
    ax3.text(
        5.0,
        4.25,
        "blocked",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    # Main world objects outside portal
    m_obj1 = Rectangle(
        (0.8, 5.0),
        1.8,
        1.5,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    m_obj2 = Rectangle(
        (7.5, 1.5),
        1.8,
        2.0,
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    ax3.add_patch(m_obj1)
    ax3.add_patch(m_obj2)

    # Panel 4: Final composite
    ax4 = axes[1, 1]
    bg4 = Rectangle(
        (0.2, 0.6),
        9.6,
        7.0,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        zorder=1,
    )
    ax4.add_patch(bg4)
    # Portal with inner world
    portal_inner = Rectangle(
        (3.0, 2.5),
        4.0,
        3.5,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        linewidth=3,
        alpha=0.6,
        zorder=2,
    )
    ax4.add_patch(portal_inner)
    # Portal world objects
    f_p1 = Rectangle(
        (3.5, 3.0),
        1.5,
        1.2,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    f_p2 = Rectangle(
        (5.2, 4.0),
        1.2,
        1.5,
        facecolor=STYLE["accent4"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    ax4.add_patch(f_p1)
    ax4.add_patch(f_p2)
    # Main world objects
    f_m1 = Rectangle(
        (0.8, 5.0),
        1.8,
        1.5,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    f_m2 = Rectangle(
        (7.5, 1.5),
        1.8,
        2.0,
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["text"],
        linewidth=1,
        alpha=0.9,
        zorder=3,
    )
    ax4.add_patch(f_m1)
    ax4.add_patch(f_m2)
    # Portal frame
    portal_frame = Rectangle(
        (2.9, 2.4),
        4.2,
        3.7,
        facecolor="none",
        edgecolor=STYLE["warn"],
        linewidth=3.5,
        zorder=4,
    )
    ax4.add_patch(portal_frame)
    ax4.text(
        5.0,
        6.5,
        "portal frame",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    fig.suptitle(
        "Portal Rendering with Stencil Masking",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    save(fig, "gpu/34-stencil-testing", "portal_technique.png")


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — outline_technique.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — outline_technique.png
# ---------------------------------------------------------------------------


def diagram_outline_technique():
    """3-panel outline rendering: draw with stencil, scaled draw, final result."""

    fig, axes = plt.subplots(1, 3, figsize=(15, 5.5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    panel_info = [
        "1. Draw Object, Stencil=2",
        "2. Draw Scaled, Stencil!=2",
        "3. Final Result",
    ]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(0, 10), ylim=(0, 8), grid=False, aspect="equal")
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_title(
            panel_info[idx], color=STYLE["text"], fontsize=11, fontweight="bold", pad=12
        )

    # Panel 1: Object drawn, stencil written
    ax1 = axes[0]
    bg1 = Rectangle(
        (0.2, 0.2),
        9.6,
        7.6,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        zorder=1,
    )
    ax1.add_patch(bg1)
    # Cube-like shape
    cube1 = FancyBboxPatch(
        (3.0, 2.5),
        4.0,
        3.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.9,
        zorder=2,
    )
    ax1.add_patch(cube1)
    ax1.text(
        5.0,
        4.25,
        "Object",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    # Stencil annotation
    ax1.text(
        5.0,
        1.2,
        "stencil = 2 where drawn",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Panel 2: Scaled object, only border passes
    ax2 = axes[1]
    bg2 = Rectangle(
        (0.2, 0.2),
        9.6,
        7.6,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        zorder=1,
    )
    ax2.add_patch(bg2)
    # Larger outline shape (scaled)
    outer = FancyBboxPatch(
        (2.2, 1.7),
        5.6,
        5.1,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        alpha=0.4,
        zorder=2,
    )
    ax2.add_patch(outer)
    # Inner blocked area (stencil==1)
    inner = FancyBboxPatch(
        (3.0, 2.5),
        4.0,
        3.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        linestyle="--",
        alpha=0.8,
        zorder=3,
    )
    ax2.add_patch(inner)
    ax2.text(
        5.0,
        4.25,
        "blocked\n(stencil==2)",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax2.text(
        5.0,
        1.0,
        "only border ring passes",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Panel 3: Final result — object with outline
    ax3 = axes[2]
    bg3 = Rectangle(
        (0.2, 0.2),
        9.6,
        7.6,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        zorder=1,
    )
    ax3.add_patch(bg3)
    # Outline ring
    outline = FancyBboxPatch(
        (2.2, 1.7),
        5.6,
        5.1,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        alpha=0.5,
        zorder=2,
    )
    ax3.add_patch(outline)
    # Object on top
    cube3 = FancyBboxPatch(
        (3.0, 2.5),
        4.0,
        3.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.95,
        zorder=3,
    )
    ax3.add_patch(cube3)
    ax3.text(
        5.0,
        4.25,
        "Object",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax3.text(
        5.0,
        1.0,
        "object + colored outline",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    fig.suptitle(
        "Object Outline via Stencil Testing",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, "gpu/34-stencil-testing", "outline_technique.png")


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — stencil_operations.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — stencil_operations.png
# ---------------------------------------------------------------------------


def diagram_stencil_operations():
    """Visual reference of all 8 stencil operations with before/after values."""

    fig, ax = plt.subplots(1, 1, figsize=(14, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 14), ylim=(0, 9.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    operations = [
        ("KEEP", "42", "42", "Value unchanged"),
        ("ZERO", "42", "0", "Always zero"),
        ("REPLACE", "42", "ref", "Set to reference (ref=42)"),
        ("INCREMENT_AND_CLAMP", "254", "255", "254+1=255; 255+1=255 (clamped)"),
        ("DECREMENT_AND_CLAMP", "1", "0", "1-1=0; 0-1=0 (clamped)"),
        ("INVERT", "0x0F", "0xF0", "Bitwise NOT"),
        ("INCREMENT_AND_WRAP", "255", "0", "255+1=0 (wraps)"),
        ("DECREMENT_AND_WRAP", "0", "255", "0-1=255 (wraps)"),
    ]

    colors_cycle = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
        STYLE["accent4"],
        STYLE["warn"],
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
    ]

    # Header
    ax.text(
        1.5,
        9.0,
        "Operation",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        6.5,
        9.0,
        "Before",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        8.5,
        9.0,
        "",
        color=STYLE["text_dim"],
        fontsize=11,
        ha="center",
        va="center",
        zorder=5,
    )
    ax.text(
        10.5,
        9.0,
        "After",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        13.0,
        9.0,
        "Notes",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Divider line
    ax.plot([0.3, 13.7], [8.5, 8.5], color=STYLE["grid"], linewidth=1.5, zorder=2)

    row_height = 1.0
    for i, (name, before, after, note) in enumerate(operations):
        y = 8.0 - i * row_height
        color = colors_cycle[i]

        # Operation name
        ax.text(
            1.5,
            y,
            name,
            color=color,
            fontsize=9,
            fontweight="bold",
            fontfamily="monospace",
            ha="left",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Before box
        before_rect = FancyBboxPatch(
            (5.7, y - 0.3),
            1.6,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(before_rect)
        ax.text(
            6.5,
            y,
            before,
            color=color,
            fontsize=10,
            fontweight="bold",
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Arrow
        ax.annotate(
            "",
            xy=(9.7, y),
            xytext=(7.5, y),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.12",
                "color": STYLE["text_dim"],
                "lw": 1.8,
            },
            zorder=3,
        )

        # After box
        after_rect = FancyBboxPatch(
            (9.7, y - 0.3),
            1.6,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(after_rect)
        ax.text(
            10.5,
            y,
            after,
            color=color,
            fontsize=10,
            fontweight="bold",
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Note
        ax.text(
            13.0,
            y,
            note,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
            style="italic",
        )

    # Reference value annotation
    ax.text(
        7.0,
        0.3,
        "Reference value (ref) = 42 for REPLACE examples",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    fig.suptitle(
        "Stencil Operations Reference",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, "gpu/34-stencil-testing", "stencil_operations.png")


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — draw_order_stencil.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — draw_order_stencil.png
# ---------------------------------------------------------------------------


def diagram_draw_order_stencil():
    """Timeline showing complete frame draw order with stencil state annotations."""

    fig, ax = plt.subplots(1, 1, figsize=(16, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 18), ylim=(0, 5.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Timeline axis
    ax.plot([0.5, 17.5], [2.8, 2.8], color=STYLE["grid"], linewidth=2, zorder=1)
    ax.annotate(
        "",
        xy=(17.8, 2.8),
        xytext=(17.2, 2.8),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["grid"],
            "lw": 2,
        },
        zorder=1,
    )
    ax.text(
        17.8,
        2.4,
        "time",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="right",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )

    passes = [
        ("Shadow\nPass", STYLE["text_dim"], "depth only", 1.1),
        ("Main\nCubes", STYLE["accent1"], "color+depth\nno stencil", 3.0),
        ("Portal\nMask", STYLE["accent2"], "stencil=1\nno color", 4.9),
        ("Portal\nWorld", STYLE["accent2"], "stencil==1\ncolor+depth", 6.8),
        ("Grid\nFloor", STYLE["accent3"], "stencil!=1\nthen ==1", 8.7),
        ("Portal\nFrame", STYLE["accent4"], "color+depth", 10.6),
        ("Outline\nWrite", STYLE["warn"], "stencil=2\ncolor+depth", 12.5),
        ("Outline\nDraw", STYLE["warn"], "stencil!=2\ncolor only", 14.4),
        ("Debug\nOverlay", STYLE["text_dim"], "if toggled", 16.3),
    ]

    box_w = 1.7
    box_h = 1.6
    for label, color, annotation, x in passes:
        rect = FancyBboxPatch(
            (x - box_w / 2, 2.8 + 0.3),
            box_w,
            box_h,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.2,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            x,
            2.8 + 0.3 + box_h / 2,
            label,
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        # Stencil annotation below timeline
        ax.text(
            x,
            2.3,
            annotation,
            color=color,
            fontsize=6.5,
            ha="center",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
            fontstyle="italic",
        )
        # Tick mark on timeline
        ax.plot([x, x], [2.65, 2.95], color=color, linewidth=2, zorder=2)

    # Pass number labels
    for i, (_, color, _, x) in enumerate(passes):
        ax.text(
            x,
            4.9,
            str(i + 1),
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    ax.text(
        0.5,
        4.9,
        "Pass #",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.suptitle(
        "Frame Draw Order with Stencil State",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.92])
    save(fig, "gpu/34-stencil-testing", "draw_order_stencil.png")


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — phase_ordering.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — phase_ordering.png
# ---------------------------------------------------------------------------


def diagram_phase_ordering():
    """Compare wrong vs correct stencil/depth phase ordering for portals."""

    fig, (ax_bad, ax_good) = plt.subplots(2, 1, figsize=(14, 9), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    def draw_scenario(ax, title, title_color, steps, depth_bar, stencil_bar):
        """Draw one scenario (wrong or correct) as a 3-step side view."""
        setup_axes(ax, xlim=(-0.5, 15.5), ylim=(-1.8, 4.5), grid=False, aspect=None)
        ax.set_xticks([])
        ax.set_yticks([])

        # Scenario title
        ax.text(
            7.5,
            4.2,
            title,
            color=title_color,
            fontsize=14,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke,
            zorder=10,
        )

        step_width = 4.5
        step_gap = 0.5

        for i, step in enumerate(steps):
            x_base = i * (step_width + step_gap) + 0.3
            x_mid = x_base + step_width / 2

            # Step label
            ax.text(
                x_mid,
                3.5,
                step["label"],
                color=step["label_color"],
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="top",
                path_effects=stroke_thin,
                zorder=10,
            )

            # --- Scene side view (camera on right, portal on left) ---
            scene_y = 1.2
            scene_h = 1.8

            # Z axis arrow (depth direction)
            ax.annotate(
                "",
                xy=(x_base + 0.2, scene_y - 0.6),
                xytext=(x_base + step_width - 0.2, scene_y - 0.6),
                arrowprops={
                    "arrowstyle": "->,head_width=0.15,head_length=0.08",
                    "color": STYLE["text_dim"],
                    "lw": 1.2,
                },
                zorder=2,
            )
            ax.text(
                x_base + 0.1,
                scene_y - 0.9,
                "far (z=-5)",
                color=STYLE["text_dim"],
                fontsize=6,
                ha="left",
                va="top",
                path_effects=stroke_thin,
                zorder=5,
            )
            ax.text(
                x_base + step_width - 0.1,
                scene_y - 0.9,
                "near (camera)",
                color=STYLE["text_dim"],
                fontsize=6,
                ha="right",
                va="top",
                path_effects=stroke_thin,
                zorder=5,
            )

            # Portal frame (always at left side — far Z)
            portal_x = x_base + 0.5
            portal_w = 0.35
            portal_rect = Rectangle(
                (portal_x, scene_y - 0.1),
                portal_w,
                scene_h + 0.2,
                facecolor=STYLE["text_dim"],
                edgecolor=STYLE["axis"],
                linewidth=1.5,
                alpha=step.get("portal_alpha", 0.7),
                zorder=step.get("portal_z", 3),
            )
            ax.add_patch(portal_rect)
            if i == 0:
                ax.text(
                    portal_x + portal_w / 2,
                    scene_y + scene_h + 0.35,
                    "Frame\n(z=-5)",
                    color=STYLE["text_dim"],
                    fontsize=6.5,
                    ha="center",
                    va="bottom",
                    path_effects=stroke_thin,
                    zorder=5,
                )

            # Portal mask quad (thin line at portal)
            if step.get("show_mask", False):
                mask_color = step.get("mask_color", STYLE["accent2"])
                ax.plot(
                    [portal_x + portal_w + 0.05, portal_x + portal_w + 0.05],
                    [scene_y, scene_y + scene_h],
                    color=mask_color,
                    linewidth=3,
                    alpha=0.8,
                    zorder=4,
                )
                ax.text(
                    portal_x + portal_w + 0.15,
                    scene_y + scene_h + 0.35,
                    "Mask",
                    color=mask_color,
                    fontsize=6.5,
                    ha="center",
                    va="bottom",
                    path_effects=stroke_thin,
                    zorder=5,
                )

            # Stencil region projection (cone from mask to screen)
            if step.get("show_stencil_cone", False):
                cone_color = step.get("cone_color", STYLE["accent2"])
                cone_alpha = step.get("cone_alpha", 0.12)
                cone_verts = [
                    (portal_x + portal_w + 0.05, scene_y),
                    (portal_x + portal_w + 0.05, scene_y + scene_h),
                    (x_base + step_width - 0.3, scene_y + scene_h + 0.3),
                    (x_base + step_width - 0.3, scene_y - 0.3),
                ]
                cone = Polygon(
                    cone_verts,
                    facecolor=cone_color,
                    alpha=cone_alpha,
                    edgecolor="none",
                    zorder=1,
                )
                ax.add_patch(cone)

            # Cube (in the middle — closer to camera)
            cube_x = x_base + 2.0
            cube_w = 1.2
            cube_color = step.get("cube_color", STYLE["accent1"])
            cube_alpha = step.get("cube_alpha", 0.85)
            cube_style = step.get("cube_style", "solid")

            cube_rect = FancyBboxPatch(
                (cube_x, scene_y + 0.2),
                cube_w,
                scene_h - 0.4,
                boxstyle="round,pad=0.05",
                facecolor=cube_color if cube_style == "solid" else "none",
                edgecolor=cube_color,
                linewidth=2,
                alpha=cube_alpha,
                linestyle="-" if cube_style == "solid" else "--",
                zorder=step.get("cube_z", 4),
            )
            ax.add_patch(cube_rect)
            if i == 0:
                ax.text(
                    cube_x + cube_w / 2,
                    scene_y + scene_h + 0.35,
                    "Cube\n(z=-3)",
                    color=STYLE["accent1"],
                    fontsize=6.5,
                    ha="center",
                    va="bottom",
                    path_effects=stroke_thin,
                    zorder=5,
                )

            # Rejected region overlay
            if step.get("show_rejected", False):
                rej_x = cube_x
                rej_w = cube_w
                rej_rect = Rectangle(
                    (rej_x, scene_y + 0.2),
                    rej_w,
                    scene_h - 0.4,
                    facecolor=STYLE["accent2"],
                    alpha=0.25,
                    edgecolor=STYLE["accent2"],
                    linewidth=1,
                    linestyle="--",
                    zorder=5,
                )
                ax.add_patch(rej_rect)
                ax.text(
                    cube_x + cube_w / 2,
                    scene_y + scene_h / 2,
                    "REJECTED\nby stencil",
                    color=STYLE["accent2"],
                    fontsize=7,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=stroke,
                    zorder=6,
                )

            # Depth write indicator
            if step.get("show_depth_write", False):
                dw_color = step.get("depth_write_color", STYLE["accent3"])
                ax.text(
                    cube_x + cube_w / 2,
                    scene_y - 0.2,
                    step.get("depth_write_label", "depth written"),
                    color=dw_color,
                    fontsize=6.5,
                    ha="center",
                    va="top",
                    path_effects=stroke_thin,
                    zorder=5,
                )

            # Mask blocked indicator
            if step.get("show_mask_blocked", False):
                ax.text(
                    portal_x + portal_w + 0.4,
                    scene_y + scene_h / 2,
                    step.get("mask_blocked_label", "mask FAILS\ndepth test"),
                    color=STYLE["accent3"],
                    fontsize=7,
                    fontweight="bold",
                    ha="left",
                    va="center",
                    path_effects=stroke,
                    zorder=6,
                )

            # Frame result indicator
            if step.get("show_frame_result", False):
                fr_color = step.get("frame_result_color", STYLE["accent2"])
                fr_text = step.get("frame_result_text", "")
                ax.text(
                    portal_x + portal_w / 2,
                    scene_y + scene_h / 2,
                    fr_text,
                    color=fr_color,
                    fontsize=7,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    rotation=90,
                    path_effects=stroke,
                    zorder=6,
                )

            # Camera icon
            cam_x = x_base + step_width - 0.4
            cam_y = scene_y + scene_h / 2
            cam_verts = [
                (cam_x, cam_y + 0.3),
                (cam_x + 0.3, cam_y),
                (cam_x, cam_y - 0.3),
            ]
            cam = Polygon(
                cam_verts,
                facecolor=STYLE["text_dim"],
                edgecolor=STYLE["axis"],
                linewidth=1,
                zorder=5,
            )
            ax.add_patch(cam)

        # Depth buffer bar visualization
        bar_y = -1.4
        ax.text(
            0.3,
            bar_y + 0.3,
            "Depth buffer:",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="left",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        for i, (label, color) in enumerate(depth_bar):
            bx = 3.5 + i * 3.8
            ax.text(
                bx,
                bar_y + 0.3,
                label,
                color=color,
                fontsize=7,
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

        ax.text(
            0.3,
            bar_y - 0.15,
            "Stencil buffer:",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="left",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        for i, (label, color) in enumerate(stencil_bar):
            bx = 3.5 + i * 3.8
            ax.text(
                bx,
                bar_y - 0.15,
                label,
                color=color,
                fontsize=7,
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    # ── Wrong order scenario ──
    draw_scenario(
        ax_bad,
        "WRONG: Mask before cubes — portal bleeds through closer geometry",
        STYLE["accent2"],
        steps=[
            {
                "label": "Phase A: Mask writes stencil",
                "label_color": STYLE["accent2"],
                "show_mask": True,
                "show_stencil_cone": True,
                "cube_color": STYLE["accent1"],
                "cube_alpha": 0.3,
                "cube_style": "dashed",
                "cube_z": 2,
            },
            {
                "label": "Phase B: Cube never wrote depth",
                "label_color": STYLE["accent2"],
                "show_mask": True,
                "cube_style": "dashed",
                "cube_alpha": 0.3,
                "cube_z": 2,
            },
            {
                "label": "Phase C: Frame passes depth (no cube depth!)",
                "label_color": STYLE["accent2"],
                "show_mask": True,
                "show_stencil_cone": True,
                "cube_style": "dashed",
                "cube_alpha": 0.2,
                "cube_z": 2,
                "portal_alpha": 1.0,
                "portal_z": 6,
                "show_frame_result": True,
                "frame_result_color": STYLE["accent2"],
                "frame_result_text": "RENDERS\nON TOP",
            },
        ],
        depth_bar=[
            ("cleared (1.0)", STYLE["text_dim"]),
            ("still 1.0 (no cube wrote!)", STYLE["accent2"]),
            ("frame passes LESS vs 1.0", STYLE["accent2"]),
        ],
        stencil_bar=[
            ("portal region = 1", STYLE["accent2"]),
            ("cube depth still missing", STYLE["accent2"]),
            ("frame ignores stencil", STYLE["text_dim"]),
        ],
    )

    # ── Correct order scenario ──
    draw_scenario(
        ax_good,
        "CORRECT: Cubes before mask — depth buffer prevents stencil behind closer objects",
        STYLE["accent3"],
        steps=[
            {
                "label": "Phase A: Cube draws first, writes depth",
                "label_color": STYLE["accent3"],
                "cube_color": STYLE["accent1"],
                "cube_alpha": 0.85,
                "cube_style": "solid",
                "show_depth_write": True,
                "depth_write_color": STYLE["accent3"],
                "depth_write_label": "depth written \u2713",
            },
            {
                "label": "Phase B: Mask fails depth where cube is closer",
                "label_color": STYLE["accent3"],
                "show_mask": True,
                "mask_color": STYLE["accent2"],
                "cube_color": STYLE["accent1"],
                "cube_alpha": 0.85,
                "cube_style": "solid",
                "show_mask_blocked": True,
                "mask_blocked_label": "mask FAILS\ndepth test\n\u2717",
            },
            {
                "label": "Phase C: Frame fails depth against cube",
                "label_color": STYLE["accent3"],
                "show_mask": True,
                "mask_color": STYLE["text_dim"],
                "cube_color": STYLE["accent1"],
                "cube_alpha": 0.85,
                "cube_style": "solid",
                "portal_alpha": 0.3,
                "portal_z": 2,
                "show_frame_result": True,
                "frame_result_color": STYLE["accent3"],
                "frame_result_text": "OCCLUDED\n\u2713",
            },
        ],
        depth_bar=[
            ("cube writes depth", STYLE["accent3"]),
            ("mask depth > cube depth \u2192 FAIL", STYLE["accent3"]),
            ("frame depth > cube depth \u2192 FAIL", STYLE["accent3"]),
        ],
        stencil_bar=[
            ("all zeros", STYLE["text_dim"]),
            ("stencil NOT written behind cube", STYLE["accent3"]),
            ("frame has no stencil test", STYLE["text_dim"]),
        ],
    )

    fig.suptitle(
        "Stencil Phase Ordering: Why Draw Order Matters for Depth Correctness",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    save(fig, "gpu/34-stencil-testing", "phase_ordering.png")


# ---------------------------------------------------------------------------
# gpu/35-decals — decal_box_projection.png
# ---------------------------------------------------------------------------
