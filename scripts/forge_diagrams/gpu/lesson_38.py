"""Diagrams for gpu/38 — Indirect Drawing."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, FancyBboxPatch, Rectangle

from .._common import STYLE, save, setup_axes

LESSON_PATH = "gpu/38-indirect-drawing"


# ---------------------------------------------------------------------------
# gpu/38-indirect-drawing — indirect_draw_concept.png
# ---------------------------------------------------------------------------


def diagram_indirect_draw_concept():
    """Side-by-side: traditional N draw calls vs one indirect call."""
    fig = plt.figure(figsize=(14, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 15), ylim=(-1.5, 6.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ── Left panel: Traditional ──────────────────────────────────────
    left_x, panel_w, panel_h = 0.3, 6.0, 6.0
    box_left = FancyBboxPatch(
        (left_x, -0.5),
        panel_w,
        panel_h,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(box_left)
    ax.text(
        left_x + panel_w / 2,
        panel_h - 0.2,
        "Traditional: N Draw Calls",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # CPU box
    cpu_x, cpu_w = 0.8, 2.0
    cpu_box = FancyBboxPatch(
        (cpu_x, 0.2),
        cpu_w,
        4.0,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
        zorder=3,
    )
    ax.add_patch(cpu_box)
    ax.text(
        cpu_x + cpu_w / 2,
        4.4,
        "CPU",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # GPU box
    gpu_x, gpu_w = 4.0, 2.0
    gpu_box = FancyBboxPatch(
        (gpu_x, 0.2),
        gpu_w,
        4.0,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
        zorder=3,
    )
    ax.add_patch(gpu_box)
    ax.text(
        gpu_x + gpu_w / 2,
        4.4,
        "GPU",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Draw call arrows
    n_draws = 4
    for i in range(n_draws):
        y = 3.4 - i * 0.9
        label = f"Draw {i}"
        ax.text(
            cpu_x + cpu_w / 2,
            y,
            label,
            color=STYLE["text"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        ax.annotate(
            "",
            xy=(gpu_x + 0.05, y),
            xytext=(cpu_x + cpu_w - 0.05, y),
            arrowprops=dict(
                arrowstyle="->,head_width=0.15,head_length=0.08",
                color=STYLE["accent2"],
                lw=1.5,
            ),
            zorder=4,
        )

    # O(N) label
    ax.text(
        left_x + panel_w / 2,
        -1.0,
        "CPU work: O(N)",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Right panel: Indirect ────────────────────────────────────────
    right_x = 7.5
    box_right = FancyBboxPatch(
        (right_x, -0.5),
        panel_w + 1.0,
        panel_h,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(box_right)
    ax.text(
        right_x + (panel_w + 1.0) / 2,
        panel_h - 0.2,
        "Indirect: 1 Draw Call",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # CPU box (right)
    cpu2_x, cpu2_w = 8.0, 2.0
    cpu2_box = FancyBboxPatch(
        (cpu2_x, 2.0),
        cpu2_w,
        2.2,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
        zorder=3,
    )
    ax.add_patch(cpu2_box)
    ax.text(
        cpu2_x + cpu2_w / 2,
        4.4,
        "CPU",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        cpu2_x + cpu2_w / 2,
        3.1,
        "DrawIndexed\nIndirect(N)",
        color=STYLE["text"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # One arrow from CPU to GPU
    ax.annotate(
        "",
        xy=(10.7, 3.1),
        xytext=(cpu2_x + cpu2_w - 0.05, 3.1),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["accent3"],
            lw=2.5,
        ),
        zorder=4,
    )

    # GPU + buffer box (right)
    gpu2_x, gpu2_w = 10.7, 3.5
    gpu2_box = FancyBboxPatch(
        (gpu2_x, 0.2),
        gpu2_w,
        4.0,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
        zorder=3,
    )
    ax.add_patch(gpu2_box)
    ax.text(
        gpu2_x + gpu2_w / 2,
        4.4,
        "GPU",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Indirect buffer entries inside GPU box
    ax.text(
        gpu2_x + gpu2_w / 2,
        3.6,
        "Indirect Buffer",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    fields = [
        "num_idx, num_inst, ...",
        "num_idx, num_inst, ...",
        "num_idx, num_inst, ...",
        "num_idx, num_inst, ...",
    ]
    for i, f in enumerate(fields):
        y = 2.9 - i * 0.6
        r = Rectangle(
            (gpu2_x + 0.2, y - 0.2),
            gpu2_w - 0.4,
            0.45,
            facecolor=STYLE["grid"],
            edgecolor=STYLE["accent3"],
            linewidth=0.8,
            zorder=4,
        )
        ax.add_patch(r)
        ax.text(
            gpu2_x + gpu2_w / 2,
            y,
            f"cmd[{i}]: {f}",
            color=STYLE["text"],
            fontsize=6,
            ha="center",
            va="center",
            fontfamily="monospace",
            path_effects=stroke,
            zorder=5,
        )

    # O(1) label
    ax.text(
        right_x + (panel_w + 1.0) / 2,
        -1.0,
        "CPU work: O(1)",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Traditional Draw Calls vs Indirect Drawing",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "indirect_draw_concept.png")


# ---------------------------------------------------------------------------
# gpu/38-indirect-drawing — frustum_culling_diagram.png
# ---------------------------------------------------------------------------


def diagram_frustum_culling():
    """Top-down 2D view of frustum culling with bounding spheres."""
    fig = plt.figure(figsize=(10, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-5, 12), ylim=(-2, 12), grid=False, aspect="equal")
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Camera position
    cam_x, cam_y = 0.0, 0.0
    ax.plot(cam_x, cam_y, "o", color=STYLE["accent1"], markersize=10, zorder=6)
    ax.text(
        cam_x - 0.5,
        cam_y - 0.7,
        "Camera",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Frustum as trapezoid (top-down view)
    near_half = 1.0
    far_half = 5.0
    near_dist = 1.5
    far_dist = 10.0

    # Frustum corners (top-down: x=left/right, y=depth)
    frustum_pts = np.array(
        [
            [cam_x - near_half, cam_y + near_dist],  # near-left
            [cam_x + near_half, cam_y + near_dist],  # near-right
            [cam_x + far_half, cam_y + far_dist],  # far-right
            [cam_x - far_half, cam_y + far_dist],  # far-left
            [cam_x - near_half, cam_y + near_dist],  # close polygon
        ]
    )

    # Fill frustum area
    from matplotlib.patches import Polygon

    frust_poly = Polygon(
        frustum_pts[:-1],
        closed=True,
        facecolor=STYLE["accent1"] + "18",
        edgecolor=STYLE["accent1"],
        linewidth=2,
        linestyle="-",
        zorder=2,
    )
    ax.add_patch(frust_poly)

    # Label frustum planes
    # Near plane
    ax.text(
        cam_x,
        near_dist - 0.5,
        "Near",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    # Far plane
    ax.text(
        cam_x,
        far_dist + 0.5,
        "Far",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    # Left plane
    mid_left_x = (cam_x - near_half + cam_x - far_half) / 2 - 0.7
    mid_left_y = (near_dist + far_dist) / 2
    ax.text(
        mid_left_x,
        mid_left_y,
        "Left",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=60,
        path_effects=stroke,
        zorder=5,
    )
    # Right plane
    mid_right_x = (cam_x + near_half + cam_x + far_half) / 2 + 0.7
    mid_right_y = (near_dist + far_dist) / 2
    ax.text(
        mid_right_x,
        mid_right_y,
        "Right",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=-60,
        path_effects=stroke,
        zorder=5,
    )

    # Objects inside frustum (green)
    inside_objs = [(0.0, 4.0, 0.6), (-1.5, 7.0, 0.8), (2.0, 5.5, 0.5)]
    for ox, oy, r in inside_objs:
        c = Circle(
            (ox, oy),
            r,
            facecolor=STYLE["accent3"] + "60",
            edgecolor=STYLE["accent3"],
            linewidth=2,
            zorder=4,
        )
        ax.add_patch(c)

    # Objects outside frustum (red)
    outside_objs = [
        (-4.0, 5.0, 0.7),
        (7.0, 3.0, 0.6),
        (3.0, 11.5, 0.5),
        (-2.0, -0.5, 0.4),
    ]
    for ox, oy, r in outside_objs:
        c = Circle(
            (ox, oy),
            r,
            facecolor="#cc444460",
            edgecolor="#cc4444",
            linewidth=2,
            zorder=4,
        )
        ax.add_patch(c)
        # X mark
        ax.plot(
            [ox - r * 0.5, ox + r * 0.5],
            [oy - r * 0.5, oy + r * 0.5],
            "-",
            color="#cc4444",
            lw=2.5,
            zorder=5,
        )
        ax.plot(
            [ox - r * 0.5, ox + r * 0.5],
            [oy + r * 0.5, oy - r * 0.5],
            "-",
            color="#cc4444",
            lw=2.5,
            zorder=5,
        )

    # Legend
    ax.plot([], [], "o", color=STYLE["accent3"], markersize=8, label="Inside (draw)")
    ax.plot([], [], "o", color="#cc4444", markersize=8, label="Outside (cull)")
    leg = ax.legend(
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    leg.get_frame().set_alpha(0.9)

    # Annotation
    ax.text(
        3.5,
        -1.2,
        "GPU compute shader tests each object's\nbounding sphere against all 6 planes",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Frustum Culling (Top-Down View)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "frustum_culling_diagram.png")


# ---------------------------------------------------------------------------
# gpu/38-indirect-drawing — indirect_buffer_layout.png
# ---------------------------------------------------------------------------


def diagram_indirect_buffer_layout():
    """Memory layout of the indirect draw buffer with per-command fields."""
    fig = plt.figure(figsize=(14, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 15), ylim=(-1.5, 6), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title
    ax.text(
        7.25,
        5.3,
        "Indirect Draw Buffer — N tightly-packed structs (20 bytes each)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    fields = [
        "num_indices",
        "num_instances",
        "first_index",
        "vertex_offset",
        "first_instance",
    ]
    field_w = 2.4
    row_h = 0.9
    n_commands = 4
    # Visibility status per command: True=visible, False=culled
    visible = [True, False, True, False]

    for cmd_i in range(n_commands):
        y = 4.0 - cmd_i * (row_h + 0.3)
        # Command label
        vis_label = "draw" if visible[cmd_i] else "culled"
        vis_color = STYLE["accent3"] if visible[cmd_i] else "#cc4444"
        ax.text(
            -0.2,
            y,
            f"cmd[{cmd_i}]",
            color=STYLE["text_dim"],
            fontsize=9,
            ha="right",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        for f_i, field in enumerate(fields):
            x = 0.0 + f_i * field_w
            is_instance_field = f_i == 1  # num_instances

            if is_instance_field:
                fc = STYLE["accent3"] + "40" if visible[cmd_i] else "#cc444440"
                ec = STYLE["accent3"] if visible[cmd_i] else "#cc4444"
                lw = 2.5
            else:
                fc = STYLE["grid"]
                ec = STYLE["text_dim"]
                lw = 1.0

            r = Rectangle(
                (x, y - row_h / 2),
                field_w,
                row_h,
                facecolor=fc,
                edgecolor=ec,
                linewidth=lw,
                zorder=3,
            )
            ax.add_patch(r)

            # Field name (only on first row)
            if cmd_i == 0:
                ax.text(
                    x + field_w / 2,
                    4.7,
                    field,
                    color=STYLE["accent1"],
                    fontsize=8,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    fontfamily="monospace",
                    path_effects=stroke,
                    zorder=5,
                )

            # Value
            if is_instance_field:
                val = "1" if visible[cmd_i] else "0"
                val_color = STYLE["accent3"] if visible[cmd_i] else "#cc4444"
                ax.text(
                    x + field_w / 2,
                    y,
                    val,
                    color=val_color,
                    fontsize=11,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=stroke,
                    zorder=5,
                )
            else:
                ax.text(
                    x + field_w / 2,
                    y,
                    "...",
                    color=STYLE["text_dim"],
                    fontsize=9,
                    ha="center",
                    va="center",
                    path_effects=stroke,
                    zorder=5,
                )

        # Status label
        ax.text(
            0.0 + len(fields) * field_w + 0.3,
            y,
            vis_label,
            color=vis_color,
            fontsize=9,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Annotation about num_instances
    ax.annotate(
        "Compute shader sets\nnum_instances = 1 (draw)\nor 0 (culled)",
        xy=(field_w * 1.5, 0.8),
        xytext=(field_w * 1.5, -1.0),
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["warn"],
            lw=1.5,
        ),
    )

    # Byte offset annotation
    ax.text(
        7.25,
        -1.2,
        "stride = 20 bytes    |    first_instance indexes into the instance vertex buffer",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Indirect Draw Buffer Layout",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "indirect_buffer_layout.png")


# ---------------------------------------------------------------------------
# gpu/38-indirect-drawing — compute_to_render_pipeline.png
# ---------------------------------------------------------------------------


def diagram_compute_to_render_pipeline():
    """Per-frame pipeline: CPU extract -> Compute cull -> Shadow -> Main render."""
    fig = plt.figure(figsize=(16, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 18), ylim=(-1.5, 5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # 4 stages matching the README's pipeline description
    stages = [
        ("CPU:\nExtract\nFrustum\nPlanes", STYLE["accent1"], 0.0, 2.5),
        ("Compute:\nFrustum Cull\n(sphere vs\n6 planes)", STYLE["accent2"], 4.0, 2.5),
        ("Shadow Pass:\nIndirect Draw\n(depth only)", STYLE["warn"], 8.5, 2.5),
        ("Main Render:\nIndirect Draw\n+ Debug View", STYLE["accent3"], 13.0, 2.5),
    ]

    box_w, box_h = 3.5, 3.0

    for label, col, sx, sy in stages:
        box = FancyBboxPatch(
            (sx, sy - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)
        ax.text(
            sx + box_w / 2,
            sy,
            label,
            color=col,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Arrows between stages
    positions = [s[2] for s in stages]
    for i in range(len(positions) - 1):
        x1 = positions[i] + box_w
        x2 = positions[i + 1]
        ax.annotate(
            "",
            xy=(x2, 2.5),
            xytext=(x1, 2.5),
            arrowprops=dict(
                arrowstyle="->,head_width=0.25,head_length=0.12",
                color=STYLE["text_dim"],
                lw=2,
            ),
            zorder=3,
        )

    # Data flow labels below each arrow
    ax.text(
        2.2,
        0.4,
        "6 plane equations\n(vec4 each)",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        6.75,
        0.4,
        "indirect buffer\n+ visibility flags",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        11.25,
        0.4,
        "shadow map\n(shadow → main render)",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Auto-sync note (README says no explicit barriers needed)
    ax.text(
        8.5,
        4.5,
        "SDL GPU auto-synchronizes between passes",
        color=STYLE["text_dim"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="center",
        fontstyle="italic",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Compute-to-Render Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "compute_to_render_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/38-indirect-drawing — split_screen_layout.png
# ---------------------------------------------------------------------------


def diagram_split_screen_layout():
    """Annotated split-screen layout: main camera (left) + debug (right)."""
    fig = plt.figure(figsize=(13, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 13.5), ylim=(-1.5, 7), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Overall viewport
    vp_x, vp_y, vp_w, vp_h = 0.5, 0.5, 12.0, 5.5
    vp_rect = Rectangle(
        (vp_x, vp_y),
        vp_w,
        vp_h,
        facecolor=STYLE["bg"],
        edgecolor=STYLE["text"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(vp_rect)

    # Divider
    mid_x = vp_x + vp_w / 2
    ax.plot(
        [mid_x, mid_x],
        [vp_y, vp_y + vp_h],
        "-",
        color=STYLE["warn"],
        lw=3,
        zorder=5,
    )

    # ── Left half: Main Camera ──────────────────────────────────────
    left_bg = Rectangle(
        (vp_x, vp_y),
        vp_w / 2,
        vp_h,
        facecolor="#1a1a2e",
        edgecolor="none",
        zorder=2,
    )
    ax.add_patch(left_bg)

    ax.text(
        vp_x + vp_w / 4,
        vp_y + vp_h - 0.4,
        "Main Camera",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )
    ax.text(
        vp_x + vp_w / 4,
        vp_y + vp_h - 0.9,
        "Indirect drawn, frustum culled",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Visible objects (green) in left viewport
    left_objs = [(2.5, 3.0, 0.4), (4.0, 2.5, 0.35), (2.0, 4.2, 0.3), (4.8, 3.8, 0.25)]
    for ox, oy, r in left_objs:
        c = Circle(
            (ox, oy),
            r,
            facecolor=STYLE["accent3"] + "80",
            edgecolor=STYLE["accent3"],
            linewidth=1.5,
            zorder=4,
        )
        ax.add_patch(c)

    # ── Right half: Debug Camera ────────────────────────────────────
    right_bg = Rectangle(
        (mid_x, vp_y),
        vp_w / 2,
        vp_h,
        facecolor="#1e1e30",
        edgecolor="none",
        zorder=2,
    )
    ax.add_patch(right_bg)

    ax.text(
        mid_x + vp_w / 4,
        vp_y + vp_h - 0.4,
        "Debug Camera",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )
    ax.text(
        mid_x + vp_w / 4,
        vp_y + vp_h - 0.9,
        "All objects, visibility coloring",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Green = visible, red = culled objects in right viewport
    right_visible = [
        (8.0, 3.0, 0.35),
        (9.2, 2.8, 0.3),
        (7.8, 4.0, 0.25),
        (10.0, 3.5, 0.2),
    ]
    right_culled = [(10.5, 1.5, 0.3), (7.5, 1.4, 0.35), (10.5, 4.2, 0.25)]

    for ox, oy, r in right_visible:
        c = Circle(
            (ox, oy),
            r,
            facecolor=STYLE["accent3"] + "80",
            edgecolor=STYLE["accent3"],
            linewidth=1.5,
            zorder=4,
        )
        ax.add_patch(c)

    for ox, oy, r in right_culled:
        c = Circle(
            (ox, oy),
            r,
            facecolor="#cc444480",
            edgecolor="#cc4444",
            linewidth=1.5,
            zorder=4,
        )
        ax.add_patch(c)

    # Frustum wireframe in debug view (yellow trapezoid)
    from matplotlib.patches import Polygon

    frust_pts = np.array(
        [
            [8.6, 2.0],
            [9.4, 2.0],  # near
            [10.5, 4.5],
            [7.8, 4.5],  # far
        ]
    )
    frust_poly = Polygon(
        frust_pts,
        closed=True,
        facecolor="none",
        edgecolor=STYLE["warn"],
        linewidth=2,
        linestyle="--",
        zorder=5,
    )
    ax.add_patch(frust_poly)
    ax.text(
        9.2,
        4.7,
        "frustum",
        color=STYLE["warn"],
        fontsize=7,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Legend below
    ax.plot([], [], "o", color=STYLE["accent3"], markersize=8, label="Visible")
    ax.plot([], [], "o", color="#cc4444", markersize=8, label="Culled")
    ax.plot([], [], "--", color=STYLE["warn"], lw=2, label="Frustum wireframe")
    leg = ax.legend(
        loc="lower center",
        ncol=3,
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    leg.get_frame().set_alpha(0.9)

    ax.set_title(
        "Split-Screen Layout",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "split_screen_layout.png")


# ---------------------------------------------------------------------------
# gpu/38-indirect-drawing — sphere_frustum_test.png
# ---------------------------------------------------------------------------


def diagram_sphere_frustum_test():
    """Three cases of sphere-vs-plane testing with signed distance."""
    fig = plt.figure(figsize=(14, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 14.5), ylim=(-1, 9), grid=False, aspect="equal")
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    cases = [
        ("Pass: d > r", STYLE["accent3"], 1.5, 6.5, 0.8, 2.0),
        ("Pass: d >= -r", STYLE["accent1"], 1.5, 3.5, 0.8, 0.4),
        ("Cull: d < -r", "#cc4444", 1.5, 0.5, 0.8, -2.0),
    ]
    # Each case: (label, color, plane_y, sphere_cy, sphere_r, signed_dist)

    section_w = 3.5
    for i, (label, col, _, _, radius, _) in enumerate(  # noqa: B007
        cases
    ):
        sx = i * 4.5 + 0.5
        # Section background
        box = FancyBboxPatch(
            (sx - 0.3, 0.0),
            section_w + 0.6,
            8.0,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
            zorder=1,
        )
        ax.add_patch(box)

        # Label at top
        ax.text(
            sx + section_w / 2,
            7.5,
            label,
            color=col,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Frustum plane (horizontal line)
        plane_y = 3.5
        ax.plot(
            [sx, sx + section_w],
            [plane_y, plane_y],
            "-",
            color=STYLE["text_dim"],
            lw=2.5,
            zorder=3,
        )
        ax.text(
            sx + section_w + 0.1,
            plane_y,
            "plane",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # "Inside" label
        ax.text(
            sx + 0.15,
            plane_y + 1.5,
            "inside",
            color=STYLE["accent3"],
            fontsize=7,
            ha="left",
            va="center",
            fontstyle="italic",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            sx + 0.15,
            plane_y - 1.5,
            "outside",
            color="#cc4444",
            fontsize=7,
            ha="left",
            va="center",
            fontstyle="italic",
            path_effects=stroke,
            zorder=5,
        )

        # Sphere position based on case
        if i == 0:
            # Fully inside
            sphere_cy = plane_y + 2.2
        elif i == 1:
            # Intersecting
            sphere_cy = plane_y + 0.3
        else:
            # Fully outside
            sphere_cy = plane_y - 2.2

        sphere_cx = sx + section_w / 2

        # Draw sphere
        c = Circle(
            (sphere_cx, sphere_cy),
            radius,
            facecolor=col + "40",
            edgecolor=col,
            linewidth=2,
            zorder=4,
        )
        ax.add_patch(c)

        # Center dot
        ax.plot(sphere_cx, sphere_cy, "o", color=col, markersize=4, zorder=5)

        # Distance line (center to plane)
        ax.plot(
            [sphere_cx, sphere_cx],
            [sphere_cy, plane_y],
            "--",
            color=STYLE["warn"],
            lw=1.5,
            zorder=3,
        )
        # d label
        d_label_y = (sphere_cy + plane_y) / 2
        ax.text(
            sphere_cx + 0.4,
            d_label_y,
            "d",
            color=STYLE["warn"],
            fontsize=10,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Radius line
        ax.plot(
            [sphere_cx, sphere_cx + radius],
            [sphere_cy, sphere_cy],
            "-",
            color=col,
            lw=1.5,
            zorder=5,
        )
        ax.text(
            sphere_cx + radius / 2,
            sphere_cy + 0.35,
            "r",
            color=col,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Formula at bottom
    ax.text(
        6.5,
        -0.5,
        "distance = dot(center, plane.xyz) + plane.w",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        fontfamily="monospace",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Sphere-Frustum Plane Test",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "sphere_frustum_test.png")
