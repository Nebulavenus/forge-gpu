"""Diagrams for gpu/37."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/37-3d-picking — color_id_picking.png
# ---------------------------------------------------------------------------


def diagram_color_id_picking():
    """Color-ID picking pipeline: lit scene, ID pass, readback, result."""
    fig = plt.figure(figsize=(14, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 16), ylim=(-1, 5.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    stage_x = [0.5, 4.5, 8.5, 12.5]
    stage_w, stage_h = 3.0, 3.5
    titles = ["Lit Scene", "ID Pass", "Readback", "Result"]
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["warn"], STYLE["accent3"]]

    # Object colors for lit scene
    obj_colors = ["#4488cc", "#cc6644", "#66aa66"]
    # ID colors (flat unique)
    id_colors = ["#010000", "#020000", "#030000"]
    id_labels = ["R=1", "R=2", "R=3"]

    for i, (sx, title, col) in enumerate(zip(stage_x, titles, colors, strict=True)):
        # Stage background box
        box = FancyBboxPatch(
            (sx, 0.3),
            stage_w,
            stage_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)
        ax.text(
            sx + stage_w / 2,
            stage_h + 0.65,
            title,
            color=col,
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        if i == 0:
            # Lit scene: colored shapes
            for j, oc in enumerate(obj_colors):
                r = Rectangle(
                    (sx + 0.3 + j * 0.9, 0.8 + j * 0.6),
                    0.7,
                    0.7,
                    facecolor=oc,
                    edgecolor=STYLE["text_dim"],
                    linewidth=1,
                    zorder=4,
                )
                ax.add_patch(r)
        elif i == 1:
            # ID pass: flat unique ID colors
            for j, (ic, lbl) in enumerate(zip(id_colors, id_labels, strict=True)):
                r = Rectangle(
                    (sx + 0.3 + j * 0.9, 0.8 + j * 0.6),
                    0.7,
                    0.7,
                    facecolor=ic,
                    edgecolor=STYLE["text_dim"],
                    linewidth=1,
                    zorder=4,
                )
                ax.add_patch(r)
                ax.text(
                    sx + 0.65 + j * 0.9,
                    0.75 + j * 0.6,
                    lbl,
                    color=STYLE["warn"],
                    fontsize=7,
                    ha="center",
                    va="top",
                    path_effects=stroke,
                    zorder=6,
                )
        elif i == 2:
            # Readback: magnified pixel
            ax.text(
                sx + stage_w / 2,
                2.4,
                "cursor\npixel",
                color=STYLE["text_dim"],
                fontsize=9,
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )
            mag = Rectangle(
                (sx + 0.8, 0.9),
                1.4,
                1.0,
                facecolor="#020000",
                edgecolor=STYLE["warn"],
                linewidth=2,
                zorder=4,
            )
            ax.add_patch(mag)
            ax.text(
                sx + 1.5,
                1.4,
                "R=2, G=0",
                color=STYLE["warn"],
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=6,
            )
        elif i == 3:
            # Result: selected object highlighted
            for j, oc in enumerate(obj_colors):
                lw = 3 if j == 1 else 1
                ec = STYLE["accent3"] if j == 1 else STYLE["text_dim"]
                r = Rectangle(
                    (sx + 0.3 + j * 0.9, 0.8 + j * 0.6),
                    0.7,
                    0.7,
                    facecolor=oc,
                    edgecolor=ec,
                    linewidth=lw,
                    zorder=4,
                )
                ax.add_patch(r)
            ax.text(
                sx + 1.55,
                0.5,
                "selected!",
                color=STYLE["accent3"],
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=6,
            )

    # Arrows between stages
    for i in range(3):
        ax.annotate(
            "",
            xy=(stage_x[i + 1] - 0.1, 2.0),
            xytext=(stage_x[i] + stage_w + 0.1, 2.0),
            arrowprops=dict(
                arrowstyle="->,head_width=0.25,head_length=0.12",
                color=STYLE["text_dim"],
                lw=2,
            ),
            zorder=3,
        )

    ax.set_title(
        "Color-ID Picking Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/37-3d-picking", "color_id_picking.png")


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — stencil_id_picking.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — stencil_id_picking.png
# ---------------------------------------------------------------------------


def diagram_stencil_id_picking():
    """Stencil-ID picking: stencil writes, buffer visualization, readback."""
    fig = plt.figure(figsize=(13, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-0.5, 5.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    sections = [
        (0.5, "Render + Stencil Write", STYLE["accent1"]),
        (5.0, "Stencil Buffer", STYLE["accent2"]),
        (9.5, "Readback", STYLE["warn"]),
    ]
    sec_w, sec_h = 4.0, 3.8

    for sx, title, col in sections:
        box = FancyBboxPatch(
            (sx, 0.3),
            sec_w,
            sec_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)
        ax.text(
            sx + sec_w / 2,
            sec_h + 0.65,
            title,
            color=col,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Section 1: Objects with stencil values
    obj_colors = ["#4488cc", "#cc6644", "#66aa66"]
    stencil_vals = ["1", "2", "3"]
    for j in range(3):
        r = Rectangle(
            (0.8 + j * 1.1, 0.8 + j * 0.7),
            0.9,
            0.8,
            facecolor=obj_colors[j],
            edgecolor=STYLE["text"],
            linewidth=1,
            zorder=4,
        )
        ax.add_patch(r)
        ax.text(
            1.25 + j * 1.1,
            1.2 + j * 0.7,
            f"S={stencil_vals[j]}",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=6,
        )

    # Section 2: Stencil buffer visualization (colored regions)
    stencil_colors = [STYLE["bg"], STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]
    grid_w, grid_h = 3.2, 2.8
    gx0, gy0 = 5.4, 0.7
    # Draw a 4x4 simplified stencil grid
    cw, ch = grid_w / 4, grid_h / 4
    stencil_grid = [
        [0, 0, 0, 0],
        [0, 1, 0, 3],
        [0, 1, 2, 3],
        [0, 0, 2, 0],
    ]
    for r in range(4):
        for c in range(4):
            v = stencil_grid[r][c]
            cell = Rectangle(
                (gx0 + c * cw, gy0 + (3 - r) * ch),
                cw,
                ch,
                facecolor=stencil_colors[v],
                edgecolor=STYLE["grid"],
                linewidth=0.8,
                zorder=4,
            )
            ax.add_patch(cell)
            ax.text(
                gx0 + c * cw + cw / 2,
                gy0 + (3 - r) * ch + ch / 2,
                str(v),
                color=STYLE["text"],
                fontsize=8,
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=6,
            )

    # Section 3: Readback
    ax.text(
        11.5,
        2.8,
        "Read D24S8\nat cursor",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        11.5,
        1.6,
        "stencil byte = 2\nobject_index = 1",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Arrows
    ax.annotate(
        "",
        xy=(4.8, 2.2),
        xytext=(4.6, 2.2),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["text_dim"],
            lw=2,
        ),
        zorder=3,
    )
    ax.annotate(
        "",
        xy=(9.3, 2.2),
        xytext=(9.1, 2.2),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["text_dim"],
            lw=2,
        ),
        zorder=3,
    )

    ax.set_title(
        "Stencil-ID Picking",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/37-3d-picking", "stencil_id_picking.png")


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — gpu_readback_pipeline.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — gpu_readback_pipeline.png
# ---------------------------------------------------------------------------


def diagram_gpu_readback_pipeline():
    """GPU readback flow: render, copy, submit, wait, map, read, decode."""
    fig = plt.figure(figsize=(15, 4.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 18), ylim=(-1.5, 4), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    steps = [
        ("Render to\nTexture", STYLE["accent1"]),
        ("Copy Pass\n(Download)", STYLE["accent2"]),
        ("Submit\nCmd Buffer", STYLE["accent3"]),
        ("WaitFor\nGPUIdle", STYLE["warn"]),
        ("Map\nTransfer Buf", STYLE["accent4"]),
        ("Read\nPixel", STYLE["accent1"]),
        ("Decode\nObject ID", STYLE["accent2"]),
    ]

    box_w, box_h = 2.0, 2.0
    gap = 0.4
    y0 = 0.5

    for i, (label, col) in enumerate(steps):
        x = i * (box_w + gap)
        box = FancyBboxPatch(
            (x, y0),
            box_w,
            box_h,
            boxstyle="round,pad=0.12",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)
        ax.text(
            x + box_w / 2,
            y0 + box_h / 2,
            label,
            color=col,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Arrow to next step
        if i < len(steps) - 1:
            ax.annotate(
                "",
                xy=(x + box_w + gap - 0.05, y0 + box_h / 2),
                xytext=(x + box_w + 0.05, y0 + box_h / 2),
                arrowprops=dict(
                    arrowstyle="->,head_width=0.2,head_length=0.1",
                    color=STYLE["text_dim"],
                    lw=1.8,
                ),
                zorder=3,
            )

    # CPU/GPU boundary line (between Submit and WaitForGPUIdle)
    boundary_x = 3 * (box_w + gap) - gap / 2
    ax.axvline(
        boundary_x,
        color=STYLE["warn"],
        linestyle="--",
        linewidth=1.5,
        ymin=0.0,
        ymax=1.0,
        zorder=1,
    )
    ax.text(
        boundary_x - 0.3,
        -0.6,
        "GPU side",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        boundary_x + 0.3,
        -0.6,
        "CPU side",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        boundary_x,
        3.2,
        "sync point",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        fontstyle="italic",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "GPU Readback Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/37-3d-picking", "gpu_readback_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — picking_method_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — picking_method_comparison.png
# ---------------------------------------------------------------------------


def diagram_picking_method_comparison():
    """Comparison table: Color-ID vs Stencil-ID picking."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 10), ylim=(-0.5, 6), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Table layout
    col_x = [0, 3.5, 6.8]
    col_w = [3.3, 3.1, 3.1]
    row_h = 1.0
    headers = ["Feature", "Color-ID", "Stencil-ID"]
    header_colors = [STYLE["text"], STYLE["accent1"], STYLE["accent2"]]

    rows = [
        ("Max objects", "65,535", "255"),
        ("Extra resources", "Offscreen texture", "None (reuses D/S)"),
        ("Readback format", "RGBA color", "D24S8 byte"),
        ("Portability", "High", "Backend-dependent"),
    ]

    # Headers
    for _j, (hx, hw, hdr, hcol) in enumerate(
        zip(col_x, col_w, headers, header_colors, strict=True)
    ):
        r = FancyBboxPatch(
            (hx, 4.5),
            hw,
            row_h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["grid"],
            edgecolor=hcol,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(r)
        ax.text(
            hx + hw / 2,
            5.0,
            hdr,
            color=hcol,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Data rows
    for i, (feat, cid, sid) in enumerate(rows):
        y = 3.3 - i * row_h
        vals = [feat, cid, sid]
        txt_colors = [STYLE["text"], STYLE["accent1"], STYLE["accent2"]]
        for j, (hx, hw) in enumerate(zip(col_x, col_w, strict=True)):
            r = Rectangle(
                (hx, y),
                hw,
                row_h,
                facecolor=STYLE["surface"] if i % 2 == 0 else STYLE["bg"],
                edgecolor=STYLE["grid"],
                linewidth=0.8,
                zorder=2,
            )
            ax.add_patch(r)
            ax.text(
                hx + hw / 2,
                y + row_h / 2,
                vals[j],
                color=txt_colors[j],
                fontsize=10,
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    ax.set_title(
        "Picking Method Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/37-3d-picking", "picking_method_comparison.png")


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — outline_selection.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — outline_selection.png
# ---------------------------------------------------------------------------


def diagram_outline_selection():
    """Two-pass stencil outline for selection highlighting."""
    fig = plt.figure(figsize=(13, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-0.5, 5.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    sections = [
        (0.3, "Pass 1: Draw + Stencil", STYLE["accent1"]),
        (5.0, "Pass 2: Scaled + NOT_EQUAL", STYLE["accent2"]),
        (9.7, "Final Result", STYLE["accent3"]),
    ]
    sec_w, sec_h = 4.2, 3.8

    for sx, title, col in sections:
        box = FancyBboxPatch(
            (sx, 0.3),
            sec_w,
            sec_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)
        ax.text(
            sx + sec_w / 2,
            sec_h + 0.65,
            title,
            color=col,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Pass 1: Object drawn, stencil REPLACE (ref=200)
    obj1 = Rectangle(
        (1.3, 1.2),
        2.0,
        1.8,
        facecolor="#4488cc",
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=4,
    )
    ax.add_patch(obj1)
    ax.text(
        2.3,
        2.1,
        "stencil\nREPLACE\nref=200",
        color=STYLE["text"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Pass 2: Scaled-up object, only border passes stencil NOT_EQUAL
    obj_outer = Rectangle(
        (5.7, 0.7),
        2.8,
        2.6,
        facecolor="none",
        edgecolor=STYLE["accent2"],
        linewidth=3,
        linestyle="--",
        zorder=4,
    )
    ax.add_patch(obj_outer)
    obj_inner = Rectangle(
        (6.1, 1.0),
        2.0,
        1.8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1,
        zorder=4,
    )
    ax.add_patch(obj_inner)
    ax.text(
        7.1,
        1.9,
        "stencil=200\nfails NOT_EQUAL\n(rejected)",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )
    ax.text(
        8.7,
        1.4,
        "border\npasses",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Final result: object with outline (scale exaggerated for visibility)
    # Lesson uses OUTLINE_SCALE = 1.04; here we expand more so the border
    # is readable at diagram resolution.
    obj_final = Rectangle(
        (10.7, 0.7),
        2.8,
        2.6,
        facecolor="none",
        edgecolor=STYLE["accent3"],
        linewidth=4,
        zorder=4,
    )
    ax.add_patch(obj_final)
    obj_fill = Rectangle(
        (11.0, 1.0),
        2.2,
        2.0,
        facecolor="#4488cc",
        edgecolor=STYLE["text_dim"],
        linewidth=1,
        zorder=5,
    )
    ax.add_patch(obj_fill)
    ax.text(
        12.1,
        0.35,
        "(outline exaggerated\nfor visibility)",
        color=STYLE["text_dim"],
        fontsize=6,
        fontstyle="italic",
        ha="center",
        va="center",
        zorder=6,
    )
    ax.text(
        12.1,
        2.0,
        "selected",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=7,
    )

    # Arrows between sections
    for i in range(2):
        x_from = sections[i][0] + sec_w + 0.05
        x_to = sections[i + 1][0] - 0.05
        ax.annotate(
            "",
            xy=(x_to, 2.2),
            xytext=(x_from, 2.2),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=STYLE["text_dim"],
                lw=2,
            ),
            zorder=3,
        )

    ax.text(
        7.0,
        4.8,
        "Cross-ref: Lesson 34 (Stencil Testing)",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Stencil Outline Selection Technique",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/37-3d-picking", "outline_selection.png")


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — pick_event_timing.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — pick_event_timing.png
# ---------------------------------------------------------------------------


def diagram_pick_event_timing():
    """Timeline showing pick events: every-frame vs click-only work."""
    fig = plt.figure(figsize=(14, 4.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 16), ylim=(-1.5, 4.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Timeline base
    ax.plot([0, 15.5], [1.5, 1.5], "-", color=STYLE["grid"], lw=2, zorder=1)

    # Frame markers
    frame_x = [0.5, 2.5, 4.5, 6.5, 8.5, 10.5, 12.5, 14.5]
    click_frame = 4  # index of the click frame

    for i, fx in enumerate(frame_x):
        is_click = i == click_frame
        col = STYLE["warn"] if is_click else STYLE["text_dim"]
        # Frame tick
        ax.plot([fx, fx], [1.2, 1.8], "-", color=col, lw=2, zorder=3)
        ax.text(
            fx,
            1.0,
            f"F{i}",
            color=col,
            fontsize=8,
            ha="center",
            va="top",
            path_effects=stroke,
            zorder=5,
        )

    # "Every Frame" region (scene rendering)
    every_rect = Rectangle(
        (0.2, 2.2),
        15.0,
        1.0,
        facecolor=STYLE["accent1"] + "30",
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
        zorder=2,
    )
    ax.add_patch(every_rect)
    ax.text(
        7.5,
        2.7,
        "Every Frame: Render Lit Scene",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # "Click Frame Only" region — constrained to a single frame interval
    click_x = frame_x[click_frame]
    next_x = (
        frame_x[click_frame + 1] if click_frame + 1 < len(frame_x) else click_x + 2.0
    )
    frame_w = next_x - click_x
    click_steps = [
        "ID Pass",
        "Copy",
        "Wait +\nDecode",
    ]
    step_w = frame_w / len(click_steps)
    total_w = frame_w
    start_x = click_x

    click_rect = Rectangle(
        (start_x - 0.15, -1.2),
        total_w + 0.3,
        1.8,
        facecolor=STYLE["warn"] + "20",
        edgecolor=STYLE["warn"],
        linewidth=1.5,
        linestyle="--",
        zorder=2,
    )
    ax.add_patch(click_rect)
    ax.text(
        start_x + total_w / 2,
        -1.4,
        "Click Frame Only",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=5,
    )

    for j, step in enumerate(click_steps):
        sx = start_x + j * step_w
        box = FancyBboxPatch(
            (sx + 0.05, -0.9),
            step_w - 0.15,
            1.2,
            boxstyle="round,pad=0.06",
            facecolor=STYLE["surface"],
            edgecolor=STYLE["warn"],
            linewidth=1.2,
            zorder=4,
        )
        ax.add_patch(box)
        ax.text(
            sx + step_w / 2,
            -0.3,
            step,
            color=STYLE["warn"],
            fontsize=7,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=6,
        )

    # Click marker
    ax.annotate(
        "click!",
        xy=(click_x, 1.2),
        xytext=(click_x, 0.2),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["warn"],
            lw=2,
        ),
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=6,
    )

    ax.set_title(
        "Pick Event Timing",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/37-3d-picking", "pick_event_timing.png")


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — id_color_encoding.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/37-3d-picking — id_color_encoding.png
# ---------------------------------------------------------------------------


def diagram_id_color_encoding():
    """How object index maps to R,G channels for color-ID picking."""
    fig = plt.figure(figsize=(12, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 13), ylim=(-1, 6), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title block: formula
    ax.text(
        6.5,
        5.2,
        "id = object_index + 1    (0 = no object)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        6.5,
        4.5,
        "R = id & 0xFF        G = (id >> 8) & 0xFF",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        fontfamily="monospace",
        path_effects=stroke,
        zorder=5,
    )

    # Example rows
    examples = [
        ("Background", 0, 0, 0, STYLE["text_dim"]),
        ("Object 0", 1, 1, 0, STYLE["accent1"]),
        ("Object 1", 2, 2, 0, STYLE["accent2"]),
        ("Object 254", 255, 255, 0, STYLE["accent3"]),
        ("Object 255", 256, 0, 1, STYLE["warn"]),
        ("Object 511", 512, 0, 2, STYLE["accent4"]),
    ]

    col_headers = ["Name", "ID", "R", "G"]
    col_x = [0.5, 4.0, 6.5, 9.0]
    col_w = [3.2, 2.2, 2.2, 2.2]
    row_h = 0.6

    # Headers
    for _j, (cx, cw, hdr) in enumerate(zip(col_x, col_w, col_headers, strict=True)):
        ax.text(
            cx + cw / 2,
            3.5,
            hdr,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Header underline
    ax.plot([0.3, 11.4], [3.15, 3.15], "-", color=STYLE["grid"], lw=1.5, zorder=2)

    # Data rows
    for i, (name, id_val, r_val, g_val, col) in enumerate(examples):
        y = 2.6 - i * row_h
        vals = [name, str(id_val), str(r_val), str(g_val)]
        for j, (cx, cw) in enumerate(zip(col_x, col_w, strict=True)):
            tc = col if j == 0 else STYLE["text"]
            fs = 10 if j == 0 else 10
            fw = "bold" if j == 0 else "normal"
            ax.text(
                cx + cw / 2,
                y,
                vals[j],
                color=tc,
                fontsize=fs,
                fontweight=fw,
                ha="center",
                va="center",
                fontfamily="monospace" if j > 0 else None,
                path_effects=stroke,
                zorder=5,
            )

        # Color swatch
        swatch_r = min(r_val, 255) / 255.0
        swatch_g = min(g_val, 255) / 255.0
        swatch = Rectangle(
            (11.5, y - 0.22),
            0.8,
            0.44,
            facecolor=(swatch_r, swatch_g, 0.0),
            edgecolor=STYLE["text_dim"],
            linewidth=1,
            zorder=4,
        )
        ax.add_patch(swatch)

    ax.text(
        11.9,
        3.5,
        "Color",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Object ID to Color Encoding",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/37-3d-picking", "id_color_encoding.png")
