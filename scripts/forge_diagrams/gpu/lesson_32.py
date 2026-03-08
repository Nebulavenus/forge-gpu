"""Diagrams for gpu/32."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, FancyBboxPatch

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/32-skinning-animations — joint_matrix_pipeline.png
# ---------------------------------------------------------------------------


def diagram_joint_matrix_pipeline():
    """Joint-matrix pipeline across model, joint-local, world, and mesh-local spaces."""
    fig = plt.figure(figsize=(14, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 15.0), ylim=(-1.5, 4.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # --- Space boxes (evenly spaced) ---
    box_w, box_h = 2.6, 2.4
    gap = 1.0
    x0 = 0.4
    boxes = [
        (x0 + 0 * (box_w + gap), 1.0, "Model Space\n(bind pose)", STYLE["accent1"]),
        (x0 + 1 * (box_w + gap), 1.0, "Joint-Local\nSpace", STYLE["accent3"]),
        (x0 + 2 * (box_w + gap), 1.0, "Animated\nWorld Space", STYLE["accent2"]),
        (x0 + 3 * (box_w + gap), 1.0, "Mesh-Local\nSpace", STYLE["accent4"]),
    ]

    for bx, by, label, color in boxes:
        rect = FancyBboxPatch(
            (bx, by),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.8,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            bx + box_w / 2,
            by + box_h / 2,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # --- Vertex dot in model space ---
    vx, vy = 1.7, 1.4
    ax.plot(vx, vy, "o", color=STYLE["warn"], ms=10, zorder=6)
    ax.text(
        vx + 0.25,
        vy - 0.3,
        "v",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=7,
    )

    # --- Transform arrows between boxes ---
    arrow_y = 2.2
    arrows = [
        (3.0, 4.0, "$B_j^{-1}$", STYLE["accent3"], "Inverse bind\nmatrix"),
        (6.6, 7.6, "$W_j$", STYLE["accent2"], "World\ntransform"),
        (10.2, 11.2, "$M^{-1}$", STYLE["accent4"], "Inverse mesh\nworld"),
    ]

    for x_start, x_end, math_label, color, desc_label in arrows:
        ax.annotate(
            "",
            xy=(x_end, arrow_y),
            xytext=(x_start, arrow_y),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.15",
                "color": color,
                "lw": 2.5,
                "connectionstyle": "arc3,rad=0",
            },
            zorder=4,
        )
        mid_x = (x_start + x_end) / 2
        # Math label above arrow
        ax.text(
            mid_x,
            arrow_y + 0.45,
            math_label,
            color=color,
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        # Description below arrow
        ax.text(
            mid_x,
            arrow_y - 0.55,
            desc_label,
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    # --- Full formula at bottom ---
    ax.text(
        7.25,
        -0.3,
        r"$\mathrm{jointMatrix}_j = M_{mesh}^{-1} \times W_j \times B_j^{-1}$",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        7.25,
        -0.9,
        "Transforms a bind-pose vertex to the mesh node's local space\n"
        "where the skin matrix is applied in the vertex shader",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax.set_title(
        "Joint Matrix Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/32-skinning-animations", "joint_matrix_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/32-skinning-animations — skinned_normal_transform.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/32-skinning-animations — skinned_normal_transform.png
# ---------------------------------------------------------------------------


def diagram_skinned_normal_transform():
    """Show why skinned normals must be transformed by the model matrix.

    The character faces away from the light (back turned).  With the correct
    transform the back is dark (N·L ≈ 0).  With the broken transform the
    normal is stuck in the rest-pose direction and falsely reads as lit.
    """

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6.5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    for ax in (ax1, ax2):
        setup_axes(ax, xlim=(-3.8, 3.8), ylim=(-3.5, 4.2), grid=False, aspect="equal")
        ax.set_xticks([])
        ax.set_yticks([])

    # --- Geometry ---
    # Use standard 2D counterclockwise parameterization (cos, sin) so the
    # circle direction in the diagram matches the scene's CCW walk path.
    theta = np.linspace(0, 2 * np.pi, 80)
    radius = 2.0
    cx, cy = radius * np.cos(theta), radius * np.sin(theta)

    # Character has walked partway around — positioned so it faces away
    # from the light (back turned).  Rest-pose forward is +Y in the diagram.
    walk_angle = 4.069
    char_x = radius * np.cos(walk_angle)
    char_z = radius * np.sin(walk_angle)

    # Facing direction = CCW tangent to circle: (-sin, cos)
    face_x = -np.sin(walk_angle)
    face_z = np.cos(walk_angle)

    # World-space light comes from top-right of diagram (toward -X, -Z).
    # L = normalize(-light_dir) points *toward* the light.
    light_dx, light_dz = -0.6, -0.8
    l_len = np.sqrt(light_dx**2 + light_dz**2)
    light_dx /= l_len
    light_dz /= l_len
    # L vector (toward light)
    L_x, L_z = -light_dx, -light_dz

    def draw_panel(ax, title, is_correct):
        # Walk path
        ax.plot(cx, cy, color=STYLE["grid"], lw=1.2, ls="--", alpha=0.4, zorder=1)

        # Origin
        ax.plot(0, 0, "+", color=STYLE["text_dim"], ms=10, mew=1.5, zorder=2)
        ax.text(
            0.0,
            -0.4,
            "origin",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=3,
        )

        # Character body
        body = Circle(
            (char_x, char_z),
            0.4,
            facecolor=STYLE["surface"],
            edgecolor=STYLE["accent1"],
            lw=2.0,
            zorder=5,
        )
        ax.add_patch(body)

        # Facing arrow (drawn after normal so it renders on top)
        f_len = 0.8

        # Light direction arrow (top-right corner, pointing into scene)
        lsx, lsz = 2.2, 3.2
        la_len = 1.8
        ax.annotate(
            "",
            xy=(lsx + light_dx * la_len, lsz + light_dz * la_len),
            xytext=(lsx, lsz),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.12",
                "color": STYLE["warn"],
                "lw": 2.5,
            },
            zorder=6,
        )
        ax.text(
            lsx - 0.1,
            lsz + 0.45,
            "light dir (world)",
            color=STYLE["warn"],
            fontsize=7.5,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=7,
        )

        # Normal arrow
        if is_correct:
            # Correct: normal follows facing direction (model matrix applied)
            nrm_dx, nrm_dz = face_x, face_z
            nrm_color = STYLE["accent3"]
            nrm_label = "normal\n(world space)"
        else:
            # Broken: normal stuck in rest-pose direction (+Z = up in diagram)
            nrm_dx, nrm_dz = 0.0, 1.0
            nrm_color = STYLE["accent2"]
            nrm_label = "normal\n(mesh-local!)"

        nrm_len = 1.2
        nrm_end_x = char_x + nrm_dx * nrm_len
        nrm_end_z = char_z + nrm_dz * nrm_len
        ax.annotate(
            "",
            xy=(nrm_end_x, nrm_end_z),
            xytext=(char_x, char_z),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.12",
                "color": nrm_color,
                "lw": 2.5,
            },
            zorder=8,
        )
        # Place label perpendicular to the arrow to avoid overlapping
        # the facing arrow or the NdotL badge below.
        perp_x, perp_z = -nrm_dz, nrm_dx  # 90° CCW from normal direction
        ax.text(
            nrm_end_x + perp_x * 0.7,
            nrm_end_z + perp_z * 0.7,
            nrm_label,
            color=nrm_color,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=9,
        )

        # Facing arrow — drawn after normal so it's visible on top
        ax.annotate(
            "",
            xy=(char_x + face_x * f_len, char_z + face_z * f_len),
            xytext=(char_x, char_z),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.1",
                "color": STYLE["accent1"],
                "lw": 2.0,
            },
            zorder=10,
        )
        # Place facing label above the arrow tip to avoid badge collision
        ax.text(
            char_x + face_x * 1.1,
            char_z + face_z * 1.1 + 0.3,
            "facing",
            color=STYLE["accent1"],
            fontsize=7.5,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=11,
        )

        # Compute NdotL
        ndotl = nrm_dx * L_x + nrm_dz * L_z
        ndotl_display = max(ndotl, 0.0)

        # Shade body to indicate lighting result
        if ndotl_display > 0.2:
            shade_color = STYLE["warn"]
            shade_alpha = 0.35
        else:
            shade_color = STYLE["text_dim"]
            shade_alpha = 0.12

        shade = Circle(
            (char_x, char_z),
            0.4,
            facecolor=shade_color,
            alpha=shade_alpha,
            edgecolor="none",
            zorder=4,
        )
        ax.add_patch(shade)

        # NdotL badge
        ndotl_text = f"N·L = {ndotl_display:.2f}"
        badge_color = STYLE["accent3"] if is_correct else STYLE["accent2"]
        # Determine if this is a wrong result — lit when should be dark, or vice versa
        if not is_correct and ndotl_display > 0.2:
            verdict = "  (falsely lit!)"
            verdict_color = STYLE["accent2"]
        elif is_correct and ndotl_display < 0.1:
            verdict = "  (correctly dark)"
            verdict_color = STYLE["accent3"]
        else:
            verdict = ""
            verdict_color = badge_color

        ax.text(
            char_x,
            char_z - 1.1,
            ndotl_text + verdict,
            color=verdict_color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=10,
            bbox={
                "boxstyle": "round,pad=0.3",
                "facecolor": STYLE["surface"],
                "edgecolor": badge_color,
                "alpha": 0.9,
            },
        )

        # Formula at bottom
        if is_correct:
            formula = "world_nrm = model × (skin_mat × n)"
            formula_color = STYLE["accent3"]
        else:
            formula = "world_nrm = skin_mat × n"
            formula_color = STYLE["accent2"]

        ax.text(
            0.0,
            -3.2,
            formula,
            color=formula_color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
            zorder=10,
        )

        ax.set_title(
            title,
            color=STYLE["text"],
            fontsize=13,
            fontweight="bold",
            pad=12,
        )

    draw_panel(ax1, "Broken: normal ignores path rotation", is_correct=False)
    draw_panel(ax2, "Correct: normal follows path rotation", is_correct=True)

    # Verdict marks
    ax1.text(
        3.0,
        -3.2,
        "✗",
        color=STYLE["accent2"],
        fontsize=20,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=10,
    )
    ax2.text(
        3.0,
        -3.2,
        "✓",
        color=STYLE["accent3"],
        fontsize=20,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    fig.tight_layout()
    save(fig, "gpu/32-skinning-animations", "skinned_normal_transform.png")


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — traditional_vs_pulled_pipeline.png
# ---------------------------------------------------------------------------
