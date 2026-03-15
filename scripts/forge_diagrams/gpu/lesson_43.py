"""Diagrams for gpu/43."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

from .._common import STYLE, save, setup_axes

LESSON_PATH = "gpu/43-pipeline-skinned-animations"

# ---------------------------------------------------------------------------
# gpu/43-pipeline-skinned-animations — skinning_pipeline_flow.png
# ---------------------------------------------------------------------------


def diagram_skinning_pipeline_flow():
    """Animation evaluation pipeline: from keyframes to GPU vertex skinning."""
    fig = plt.figure(figsize=(12, 14), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.0, 13.0), ylim=(-1.0, 17.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Colors for the three pipeline stages
    color_cpu_anim = STYLE["accent1"]  # Blue — CPU animation evaluation
    color_joint = STYLE["accent2"]  # Orange — joint matrix computation
    color_gpu = STYLE["accent3"]  # Green — GPU upload / shader

    # --- Box definitions ---
    # (x_center, y_center, width, height, label, color, sublabel)
    box_w = 4.0
    box_h = 1.3
    small_box_w = 3.4
    small_box_h = 1.1

    # Two input sources at the top
    input_left_x = 2.0
    input_right_x = 10.0
    top_y = 16.0

    boxes = [
        # Row 0: inputs (two parallel sources)
        (
            input_left_x,
            top_y,
            small_box_w,
            small_box_h,
            ".fanim keyframes",
            color_cpu_anim,
            None,
        ),
        (
            input_right_x,
            top_y,
            small_box_w,
            small_box_h,
            ".fskin hierarchy",
            color_cpu_anim,
            None,
        ),
        # Row 1: evaluation steps (two parallel)
        (
            input_left_x,
            13.5,
            small_box_w,
            small_box_h,
            "Binary search\n+ lerp / slerp",
            color_cpu_anim,
            None,
        ),
        (
            input_right_x,
            13.5,
            small_box_w,
            small_box_h,
            "Parent-child\ntraversal",
            color_cpu_anim,
            None,
        ),
        # Row 2: intermediate results (two parallel)
        (
            input_left_x,
            11.0,
            small_box_w,
            small_box_h,
            "Local TRS",
            color_cpu_anim,
            "per joint",
        ),
        (
            input_right_x,
            11.0,
            small_box_w,
            small_box_h,
            "World Transforms",
            color_cpu_anim,
            "per joint",
        ),
        # Row 3: merge point
        (
            6.0,
            8.8,
            box_w,
            box_h,
            "World Transforms",
            color_joint,
            "local TRS composed via hierarchy",
        ),
        # Row 4: split into mesh_world and joint_world
        (2.5, 6.5, small_box_w, small_box_h, "mesh_world", color_joint, None),
        (9.5, 6.5, small_box_w, small_box_h, "joint_world[i]", color_joint, None),
        # Row 5: joint matrix formula
        (
            6.0,
            4.3,
            box_w + 1.5,
            box_h + 0.2,
            "Joint Matrix[i]",
            color_joint,
            "inv(mesh_world) \u00d7 joint_world \u00d7 IBM[i]",
        ),
        # Row 6: GPU storage buffer
        (6.0, 2.2, box_w, box_h, "GPU Storage Buffer", color_gpu, None),
        # Row 7: vertex shader
        (
            6.0,
            0.2,
            box_w + 1.5,
            box_h + 0.2,
            "Vertex Shader Skinning",
            color_gpu,
            "skin = \u03a3 weight[k] \u00d7 J[joint[k]]",
        ),
    ]

    def draw_box(cx, cy, w, h, label, color, sublabel):
        """Draw a rounded box centered at (cx, cy)."""
        rect = FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.8,
            zorder=2,
        )
        ax.add_patch(rect)
        if sublabel:
            # Main label slightly above center, sublabel below
            ax.text(
                cx,
                cy + 0.18,
                label,
                color=color,
                fontsize=10.5,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )
            ax.text(
                cx,
                cy - 0.28,
                sublabel,
                color=STYLE["text_dim"],
                fontsize=7.5,
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )
        else:
            ax.text(
                cx,
                cy,
                label,
                color=color,
                fontsize=10.5,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    for bx, by, bw, bh, label, color, sublabel in boxes:
        draw_box(bx, by, bw, bh, label, color, sublabel)

    # --- Arrows ---
    arrow_kw = {
        "arrowstyle": "->,head_width=0.2,head_length=0.12",
        "lw": 2.0,
        "connectionstyle": "arc3,rad=0",
    }

    def draw_arrow(x0, y0, x1, y1, color):
        ax.annotate(
            "",
            xy=(x1, y1),
            xytext=(x0, y0),
            arrowprops={**arrow_kw, "color": color},
            zorder=3,
        )

    # Row 0 -> Row 1 (inputs to evaluation)
    draw_arrow(
        input_left_x,
        top_y - small_box_h / 2,
        input_left_x,
        13.5 + small_box_h / 2,
        color_cpu_anim,
    )
    draw_arrow(
        input_right_x,
        top_y - small_box_h / 2,
        input_right_x,
        13.5 + small_box_h / 2,
        color_cpu_anim,
    )

    # Row 1 -> Row 2 (evaluation to results)
    draw_arrow(
        input_left_x,
        13.5 - small_box_h / 2,
        input_left_x,
        11.0 + small_box_h / 2,
        color_cpu_anim,
    )
    draw_arrow(
        input_right_x,
        13.5 - small_box_h / 2,
        input_right_x,
        11.0 + small_box_h / 2,
        color_cpu_anim,
    )

    # Row 2 -> Row 3 (local TRS + hierarchy -> world transforms)
    # Left merges right into the center box
    draw_arrow(
        input_left_x + small_box_w / 2 - 0.3,
        11.0 - small_box_h / 2,
        6.0 - box_w / 2 + 0.3,
        8.8 + box_h / 2,
        color_cpu_anim,
    )
    draw_arrow(
        input_right_x - small_box_w / 2 + 0.3,
        11.0 - small_box_h / 2,
        6.0 + box_w / 2 - 0.3,
        8.8 + box_h / 2,
        color_cpu_anim,
    )

    # Row 3 -> Row 4 (world transforms splits into mesh_world and joint_world)
    draw_arrow(
        6.0 - box_w / 2 + 0.3,
        8.8 - box_h / 2,
        2.5 + small_box_w / 2 - 0.3,
        6.5 + small_box_h / 2,
        color_joint,
    )
    draw_arrow(
        6.0 + box_w / 2 - 0.3,
        8.8 - box_h / 2,
        9.5 - small_box_w / 2 + 0.3,
        6.5 + small_box_h / 2,
        color_joint,
    )

    # Row 4 -> Row 5 (mesh_world and joint_world merge into joint matrix)
    draw_arrow(
        2.5 + small_box_w / 2 - 0.3,
        6.5 - small_box_h / 2,
        6.0 - (box_w + 1.5) / 2 + 0.5,
        4.3 + (box_h + 0.2) / 2,
        color_joint,
    )
    draw_arrow(
        9.5 - small_box_w / 2 + 0.3,
        6.5 - small_box_h / 2,
        6.0 + (box_w + 1.5) / 2 - 0.5,
        4.3 + (box_h + 0.2) / 2,
        color_joint,
    )

    # Row 5 -> Row 6 (joint matrix -> GPU buffer)
    draw_arrow(6.0, 4.3 - (box_h + 0.2) / 2, 6.0, 2.2 + box_h / 2, color_gpu)

    # Row 6 -> Row 7 (GPU buffer -> vertex shader)
    draw_arrow(6.0, 2.2 - box_h / 2, 6.0, 0.2 + (box_h + 0.2) / 2, color_gpu)

    # --- Stage labels on the side ---
    label_x = -0.5
    ax.text(
        label_x,
        13.5,
        "CPU",
        color=color_cpu_anim,
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=90,
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        label_x,
        6.5,
        "CPU",
        color=color_joint,
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=90,
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        label_x,
        1.2,
        "GPU",
        color=color_gpu,
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=90,
        path_effects=stroke,
        zorder=5,
    )

    # --- Title ---
    ax.set_title(
        "Animation Evaluation Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, LESSON_PATH, "skinning_pipeline_flow.png")
