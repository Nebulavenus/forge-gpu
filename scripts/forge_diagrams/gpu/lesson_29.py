"""Diagrams for gpu/29."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Rectangle

from .._common import STYLE, save


def _ssr_draw_arrow(ax, x_start, y_start, x_end, y_end, color=None):
    """Draw a connecting arrow between elements."""
    if color is None:
        color = STYLE["text_dim"]
    ax.annotate(
        "",
        xy=(x_end, y_end),
        xytext=(x_start, y_start),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": color,
            "lw": 2,
        },
        zorder=2,
    )


def _ssr_draw_texture_tag(ax, x, y, label, fmt, color, stroke_thin):
    """Draw a small texture output tag (label + format)."""
    tag_w = 1.55
    tag_h = 0.55
    rect = Rectangle(
        (x - tag_w / 2, y - tag_h / 2),
        tag_w,
        tag_h,
        linewidth=1.2,
        edgecolor=color,
        facecolor=STYLE["bg"],
        alpha=0.85,
        zorder=5,
    )
    ax.add_patch(rect)
    ax.text(
        x,
        y + 0.07,
        label,
        color=color,
        fontsize=7.5,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=6,
    )
    ax.text(
        x,
        y - 0.17,
        fmt,
        color=STYLE["text_dim"],
        fontsize=6,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=6,
    )


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — render_pipeline.png
# ---------------------------------------------------------------------------


def _ssr_draw_pass_box(ax, x, y, w, h, title, number, color, stroke, fontsize=12):
    """Draw a rounded pass box with a numbered title."""
    rect = FancyBboxPatch(
        (x, y),
        w,
        h,
        boxstyle="round,pad=0.12",
        linewidth=2.5,
        edgecolor=color,
        facecolor=STYLE["surface"],
        alpha=0.9,
        zorder=3,
    )
    ax.add_patch(rect)
    ax.text(
        x + w / 2,
        y + h * 0.65,
        f"{number}. {title}",
        color=STYLE["text"],
        fontsize=fontsize,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=4,
    )


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — ray_march.png
# ---------------------------------------------------------------------------


def _ssr_depth_profile(x):
    """Synthetic depth-buffer profile (side view).

    Returns height values representing the scene surface from the side.
    The profile has a floor, a reflective surface (where P sits), a gap,
    and a tall wall that the reflected ray will hit.
    """
    y = np.ones_like(x, dtype=float) * 0.4
    # Reflective floor / platform where P sits
    mask_plat = (x >= 1.0) & (x < 4.0)
    y[mask_plat] = 1.5
    # Drop to floor after platform
    mask_gap = (x >= 4.0) & (x < 7.0)
    y[mask_gap] = 0.4
    # Small step
    mask_step = (x >= 5.2) & (x < 6.2)
    y[mask_step] = 0.75
    # Tall wall — the reflection target
    mask_wall = (x >= 8.0) & (x < 10.5)
    y[mask_wall] = 2.8
    # Trailing floor
    mask_trail = x >= 10.5
    y[mask_trail] = 0.4
    return y


def diagram_ssr_render_pipeline():
    """SSR render pipeline: 4 passes with texture outputs flowing between them."""
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(16, 7.5), facecolor=STYLE["bg"])
    ax.set_xlim(-0.5, 16)
    ax.set_ylim(-4.5, 4.5)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.axis("off")

    # --- Pass box dimensions and positions ---
    box_w = 2.8
    box_h = 1.8
    box_y = 0.8
    gap = 1.2

    x1 = 0.3
    x2 = x1 + box_w + gap
    x3 = x2 + box_w + gap
    x4 = x3 + box_w + gap

    pass_colors = [
        STYLE["accent4"],  # Shadow — purple
        STYLE["accent1"],  # G-Buffer — cyan
        STYLE["accent2"],  # SSR Ray March — orange
        STYLE["accent3"],  # Composite — green
    ]

    # --- Draw the four pass boxes ---
    _ssr_draw_pass_box(
        ax, x1, box_y, box_w, box_h, "Shadow\nPass", 1, pass_colors[0], stroke
    )
    _ssr_draw_pass_box(
        ax, x2, box_y, box_w, box_h, "G-Buffer\nPass", 2, pass_colors[1], stroke
    )
    _ssr_draw_pass_box(
        ax, x3, box_y, box_w, box_h, "SSR Ray\nMarch", 3, pass_colors[2], stroke
    )
    _ssr_draw_pass_box(
        ax, x4, box_y, box_w, box_h, "Composite", 4, pass_colors[3], stroke
    )

    # --- Arrows between pass boxes ---
    arrow_y = box_y + box_h / 2
    for src_x, dst_x in [(x1, x2), (x2, x3), (x3, x4)]:
        _ssr_draw_arrow(ax, src_x + box_w + 0.08, arrow_y, dst_x - 0.08, arrow_y)

    # --- Shadow Pass output: single Depth texture ---
    shadow_out_y = box_y - 0.9
    shadow_cx = x1 + box_w / 2
    _ssr_draw_texture_tag(
        ax, shadow_cx, shadow_out_y, "Shadow Depth", "D32F", pass_colors[0], stroke_thin
    )
    _ssr_draw_arrow(
        ax, shadow_cx, box_y - 0.02, shadow_cx, shadow_out_y + 0.3, pass_colors[0]
    )

    # --- G-Buffer Pass outputs: 4 textures fanning out below ---
    gbuf_textures = [
        ("Color", "RGBA8"),
        ("Normals", "RGBA16F"),
        ("World Pos", "RGBA16F"),
        ("Depth", "D32F"),
    ]
    gbuf_start_x = x2 - 0.6
    gbuf_spacing = 1.7
    gbuf_out_y = box_y - 1.2
    gbuf_tag_y = gbuf_out_y - 0.7

    gbuf_cx = x2 + box_w / 2

    for i, (label, fmt) in enumerate(gbuf_textures):
        tag_x = gbuf_start_x + i * gbuf_spacing
        _ssr_draw_texture_tag(
            ax, tag_x, gbuf_tag_y, label, fmt, pass_colors[1], stroke_thin
        )
        _ssr_draw_arrow(
            ax, gbuf_cx, box_y - 0.15, tag_x, gbuf_tag_y + 0.35, pass_colors[1]
        )

    # --- Curved arrows from G-Buffer textures up into SSR Ray March ---
    ssr_cx = x3 + box_w / 2

    ax.text(
        (gbuf_start_x + gbuf_start_x + 3 * gbuf_spacing) / 2,
        gbuf_tag_y - 0.7,
        "All G-Buffer textures feed into SSR",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=4,
    )

    for i in range(4):
        tag_x = gbuf_start_x + i * gbuf_spacing
        ax.annotate(
            "",
            xy=(ssr_cx - 0.8 + i * 0.55, box_y + 0.05),
            xytext=(tag_x, gbuf_tag_y - 0.3),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.08",
                "color": pass_colors[1],
                "lw": 1.2,
                "alpha": 0.5,
                "connectionstyle": "arc3,rad=-0.25",
            },
            zorder=2,
        )

    # --- Shadow map also feeds into G-Buffer pass (for shadow testing) ---
    ax.annotate(
        "",
        xy=(x2, box_y + box_h * 0.3),
        xytext=(shadow_cx + 0.6, shadow_out_y + 0.05),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": pass_colors[0],
            "lw": 1.2,
            "alpha": 0.5,
            "connectionstyle": "arc3,rad=0.3",
        },
        zorder=2,
    )

    # --- SSR Ray March output ---
    ssr_out_y = box_y - 0.9
    ssr_out_cx = x3 + box_w / 2
    _ssr_draw_texture_tag(
        ax, ssr_out_cx, ssr_out_y, "SSR Result", "RGBA8", pass_colors[2], stroke_thin
    )
    _ssr_draw_arrow(
        ax, ssr_out_cx, box_y - 0.02, ssr_out_cx, ssr_out_y + 0.3, pass_colors[2]
    )

    # --- Composite inputs: Scene Color (from G-Buffer Color) + SSR Result ---
    # Arrow from SSR Result to Composite
    ax.annotate(
        "",
        xy=(x4, box_y + box_h * 0.3),
        xytext=(ssr_out_cx + 0.6, ssr_out_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.18,head_length=0.10",
            "color": pass_colors[2],
            "lw": 1.5,
            "alpha": 0.7,
            "connectionstyle": "arc3,rad=-0.3",
        },
        zorder=2,
    )

    # Arrow from G-Buffer Color tag to Composite
    color_tag_x = gbuf_start_x
    ax.annotate(
        "",
        xy=(x4, box_y + box_h * 0.6),
        xytext=(color_tag_x + 0.5, gbuf_tag_y - 0.3),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": pass_colors[3],
            "lw": 1.2,
            "alpha": 0.5,
            "connectionstyle": "arc3,rad=-0.35",
        },
        zorder=2,
    )

    # --- Composite output: Swapchain ---
    swap_out_y = box_y - 0.9
    swap_cx = x4 + box_w / 2
    _ssr_draw_texture_tag(
        ax, swap_cx, swap_out_y, "Swapchain", "Present", pass_colors[3], stroke_thin
    )
    _ssr_draw_arrow(
        ax, swap_cx, box_y - 0.02, swap_cx, swap_out_y + 0.3, pass_colors[3]
    )

    # --- Input labels above the first pass ---
    ax.text(
        x1 + box_w / 2,
        box_y + box_h + 0.55,
        "Scene geometry",
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=4,
    )
    _ssr_draw_arrow(
        ax,
        x1 + box_w / 2,
        box_y + box_h + 0.3,
        x1 + box_w / 2,
        box_y + box_h + 0.05,
        STYLE["text_dim"],
    )

    # --- Title ---
    fig.suptitle(
        "SSR Render Pipeline  (4 Passes)",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.97,
    )

    # --- Subtitle line ---
    ax.text(
        8.0,
        4.2,
        "Shadow  \u2192  G-Buffer  \u2192  SSR Ray March  \u2192  Composite  \u2192  Display",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=4,
    )

    # --- Legend ---
    legend_y = -3.8
    legend_items = [
        (pass_colors[0], "Shadow Pass", "Depth-only render from light view"),
        (
            pass_colors[1],
            "G-Buffer Pass",
            "Geometry attributes to multiple render targets",
        ),
        (pass_colors[2], "SSR Ray March", "Screen-space ray marching for reflections"),
        (pass_colors[3], "Composite", "Blend scene color with SSR result"),
    ]
    legend_x_start = 0.5
    legend_spacing = 3.9

    for i, (color, name, desc) in enumerate(legend_items):
        lx = legend_x_start + i * legend_spacing
        ax.plot(lx, legend_y, "s", color=color, markersize=8, zorder=5)
        ax.text(
            lx + 0.3,
            legend_y + 0.05,
            name,
            color=STYLE["text"],
            fontsize=8,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        ax.text(
            lx + 0.3,
            legend_y - 0.45,
            desc,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="left",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    fig.tight_layout(rect=(0, 0.02, 1, 0.93))
    save(fig, "gpu/29-screen-space-reflections", "render_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — ray_march.png
# ---------------------------------------------------------------------------


def diagram_ssr_ray_march():
    """SSR ray march: side-view cross-section showing reflected ray through depth buffer."""
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig = plt.figure(figsize=(14, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)

    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-2.5, 13.0)
    ax.set_ylim(-0.5, 4.8)
    ax.set_aspect("equal")
    ax.axis("off")

    # -----------------------------------------------------------------
    # 1. Depth buffer surface profile (the "scene" seen from the side)
    # -----------------------------------------------------------------
    xs = np.linspace(-1.5, 12.5, 3000)
    ys = _ssr_depth_profile(xs)

    ax.fill_between(
        xs,
        -0.5,
        ys,
        color=STYLE["surface"],
        alpha=0.6,
        zorder=1,
    )
    ax.plot(xs, ys, color=STYLE["axis"], lw=2.0, zorder=3)

    ax.text(
        11.5,
        0.15,
        "Depth buffer\n(scene surface)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 2. Camera eye
    # -----------------------------------------------------------------
    cam = np.array([-1.8, 2.5])
    ax.plot(
        cam[0],
        cam[1],
        "D",
        color=STYLE["warn"],
        markersize=11,
        zorder=8,
    )
    ax.text(
        cam[0],
        cam[1] + 0.25,
        "Camera",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 3. Surface point P — top of the reflective platform
    # -----------------------------------------------------------------
    p_point = np.array([2.5, 1.5])
    ax.plot(
        p_point[0],
        p_point[1],
        "o",
        color=STYLE["accent1"],
        markersize=10,
        zorder=8,
    )
    ax.text(
        p_point[0] - 0.3,
        p_point[1] + 0.2,
        "P",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        ha="right",
        va="bottom",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 4. View direction (camera -> P)
    # -----------------------------------------------------------------
    view_dir = p_point - cam
    view_len = np.linalg.norm(view_dir)
    view_unit = view_dir / view_len

    arrow_start = cam + view_unit * 0.5
    arrow_end = p_point - view_unit * 0.4
    ax.annotate(
        "",
        xy=arrow_end,
        xytext=arrow_start,
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.13",
            "color": STYLE["warn"],
            "lw": 2.0,
            "linestyle": "--",
        },
        zorder=5,
    )
    lbl_pos = cam + view_dir * 0.35
    view_angle = np.degrees(np.arctan2(view_unit[1], view_unit[0]))
    ax.text(
        lbl_pos[0] - 0.1,
        lbl_pos[1] + 0.35,
        "View direction",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        rotation=view_angle,
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 5. Surface normal at P (upward)
    # -----------------------------------------------------------------
    normal = np.array([0.0, 1.0])
    normal_len = 0.9
    ax.annotate(
        "",
        xy=(p_point[0], p_point[1] + normal_len),
        xytext=p_point,
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.13",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
        zorder=7,
    )
    ax.text(
        p_point[0] + 0.25,
        p_point[1] + normal_len * 0.6,
        "Normal",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 6. Reflected ray direction
    # -----------------------------------------------------------------
    v_dir = view_unit
    r_dir = v_dir - 2.0 * np.dot(v_dir, normal) * normal
    r_dir = r_dir / np.linalg.norm(r_dir)

    num_steps = 9
    step_size = 0.95
    step_positions = [p_point + r_dir * step_size * (i + 1) for i in range(num_steps)]

    hit_index = None
    thickness = 0.25
    source_depth = _ssr_depth_profile(np.array([p_point[0]]))[0]
    for i, sp in enumerate(step_positions):
        if sp[0] < -1.5 or sp[0] > 12.5:
            continue
        depth_val = _ssr_depth_profile(np.array([sp[0]]))[0]
        # Skip self-intersection with the originating reflective surface.
        if abs(depth_val - source_depth) < 1e-4:
            continue
        if abs(sp[1] - depth_val) <= thickness and hit_index is None:
            hit_index = i
            break

    last_step = hit_index if hit_index is not None else num_steps - 1
    ray_end = step_positions[last_step]
    ax.plot(
        [p_point[0], ray_end[0]],
        [p_point[1], ray_end[1]],
        "--",
        color=STYLE["accent1"],
        lw=1.2,
        alpha=0.4,
        zorder=4,
    )

    label_t = min(2.0, last_step * 0.5)
    label_pos = p_point + r_dir * step_size * label_t
    angle_deg = np.degrees(np.arctan2(r_dir[1], r_dir[0]))
    ax.text(
        label_pos[0],
        label_pos[1] + 0.28,
        "Reflected ray",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        rotation=angle_deg,
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 7. Ray march steps — dots along the ray, depth comparisons
    # -----------------------------------------------------------------
    for i, sp in enumerate(step_positions):
        if sp[0] < -1.5 or sp[0] > 12.5:
            continue

        depth_val = _ssr_depth_profile(np.array([sp[0]]))[0]
        is_hit = i == hit_index

        if hit_index is not None and i > hit_index:
            break

        dot_color = STYLE["accent2"] if is_hit else STYLE["accent1"]
        dot_size = 10 if is_hit else 6
        ax.plot(
            sp[0],
            sp[1],
            "o",
            color=dot_color,
            markersize=dot_size,
            zorder=8,
        )

        line_color = STYLE["accent2"] if is_hit else STYLE["text_dim"]
        line_alpha = 0.85 if is_hit else 0.35
        ax.plot(
            [sp[0], sp[0]],
            [sp[1], depth_val],
            ":",
            color=line_color,
            lw=1.3,
            alpha=line_alpha,
            zorder=4,
        )

        ax.plot(
            sp[0],
            depth_val,
            "s",
            color=line_color,
            markersize=4,
            alpha=line_alpha,
            zorder=5,
        )

        if not is_hit:
            step_labels = {0: "Step 1", 1: "Step 2", 2: "Step 3"}
            if i in step_labels:
                ax.text(
                    sp[0] + 0.2,
                    sp[1] + 0.18,
                    step_labels[i],
                    color=STYLE["text_dim"],
                    fontsize=8,
                    ha="left",
                    va="bottom",
                    path_effects=stroke,
                    zorder=10,
                )

    # -----------------------------------------------------------------
    # 8. Hit point annotation
    # -----------------------------------------------------------------
    if hit_index is not None:
        hp = step_positions[hit_index]
        depth_at_hit = _ssr_depth_profile(np.array([hp[0]]))[0]

        ax.text(
            hp[0] + 0.4,
            hp[1] + 0.25,
            "Hit!",
            color=STYLE["accent2"],
            fontsize=15,
            fontweight="bold",
            ha="left",
            va="bottom",
            path_effects=stroke,
            zorder=10,
        )

        ax.plot(
            hp[0],
            depth_at_hit,
            "o",
            color=STYLE["accent2"],
            markersize=13,
            markerfacecolor="none",
            markeredgewidth=2.5,
            zorder=9,
        )

        # -----------------------------------------------------------------
        # 9. Thickness threshold bracket
        # -----------------------------------------------------------------
        bracket_x = hp[0] + 0.65
        bracket_top = depth_at_hit + thickness
        bracket_bot = depth_at_hit - thickness

        cap_hw = 0.12
        for by in (bracket_top, bracket_bot):
            ax.plot(
                [bracket_x - cap_hw, bracket_x + cap_hw],
                [by, by],
                "-",
                color=STYLE["accent4"],
                lw=1.5,
                zorder=7,
            )
        ax.annotate(
            "",
            xy=(bracket_x, bracket_top),
            xytext=(bracket_x, bracket_bot),
            arrowprops={
                "arrowstyle": "<->",
                "color": STYLE["accent4"],
                "lw": 1.5,
            },
            zorder=7,
        )
        ax.text(
            bracket_x + 0.25,
            depth_at_hit,
            "Thickness\nthreshold",
            color=STYLE["accent4"],
            fontsize=9,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=10,
        )

    # -----------------------------------------------------------------
    # 10. Legend
    # -----------------------------------------------------------------
    legend_x, legend_y = 10.2, 4.3
    ax.plot(legend_x, legend_y, "o", color=STYLE["accent1"], markersize=6, zorder=8)
    ax.text(
        legend_x + 0.3,
        legend_y,
        "Ray steps",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )
    ax.plot(
        legend_x, legend_y - 0.4, "o", color=STYLE["accent2"], markersize=6, zorder=8
    )
    ax.text(
        legend_x + 0.3,
        legend_y - 0.4,
        "Intersection (hit)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )
    ax.plot(
        [legend_x - 0.15, legend_x + 0.15],
        [legend_y - 0.8, legend_y - 0.8],
        ":",
        color=STYLE["text_dim"],
        lw=1.3,
        alpha=0.6,
        zorder=8,
    )
    ax.text(
        legend_x + 0.3,
        legend_y - 0.8,
        "Depth comparison",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # Title
    # -----------------------------------------------------------------
    ax.set_title(
        "Screen-Space Reflections: Ray Marching Through the Depth Buffer",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=16,
    )

    save(fig, "gpu/29-screen-space-reflections", "ray_march.png")


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — gbuffer_layout.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — gbuffer_layout.png
# ---------------------------------------------------------------------------


def diagram_ssr_gbuffer_layout():
    """G-buffer layout: 4 stacked render targets for screen-space reflections."""
    from matplotlib.patches import FancyBboxPatch

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-0.8, 8.0)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("auto")
    ax.axis("off")

    # --- G-buffer render targets (stacked bottom to top) ---
    targets = [
        (
            0.0,
            "Depth",
            "D32_FLOAT",
            "Hardware depth buffer",
            STYLE["text_dim"],
        ),
        (
            1.8,
            "World Position",
            "R16G16B16A16_FLOAT",
            "World-space position  (alpha = reflectivity)",
            STYLE["accent3"],
        ),
        (
            3.6,
            "View Normals",
            "R16G16B16A16_FLOAT",
            "View-space surface normals",
            STYLE["accent2"],
        ),
        (
            5.4,
            "Scene Color",
            "R8G8B8A8_UNORM",
            "Lit color with Blinn-Phong + shadows",
            STYLE["accent1"],
        ),
    ]

    bar_width = 7.0
    bar_height = 1.3
    bar_x = 1.5

    for y, name, fmt, desc, color in targets:
        box = FancyBboxPatch(
            (bar_x, y),
            bar_width,
            bar_height,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.2,
            zorder=3,
        )
        ax.add_patch(box)

        ax.text(
            bar_x + 0.35,
            y + bar_height / 2 + 0.15,
            name,
            color=color,
            fontsize=12,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        ax.text(
            bar_x + 0.35,
            y + bar_height / 2 - 0.25,
            desc,
            color=STYLE["text"],
            fontsize=8.5,
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        ax.text(
            bar_x + bar_width - 0.35,
            y + bar_height / 2,
            fmt,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="right",
            va="center",
            family="monospace",
            path_effects=stroke,
            zorder=5,
        )

    # --- Bracket and label on the left ---
    bracket_x = 0.9
    bracket_top = 5.4 + bar_height
    bracket_bot = 0.0

    ax.plot(
        [bracket_x, bracket_x],
        [bracket_bot + 0.15, bracket_top - 0.15],
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=4,
    )
    ax.plot(
        [bracket_x, bracket_x + 0.25],
        [bracket_top - 0.15, bracket_top - 0.15],
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=4,
    )
    ax.plot(
        [bracket_x, bracket_x + 0.25],
        [bracket_bot + 0.15, bracket_bot + 0.15],
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=4,
    )

    mid_y = (bracket_top + bracket_bot) / 2
    ax.text(
        bracket_x - 0.15,
        mid_y,
        "G-Buffer",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=90,
        path_effects=stroke,
        zorder=5,
    )

    # --- SV_Target labels on the right ---
    sv_labels = [
        (5.4, "SV_Target0"),
        (3.6, "SV_Target1"),
        (1.8, "SV_Target2"),
        (0.0, "Depth attachment"),
    ]
    label_x = bar_x + bar_width + 0.5

    for y, label in sv_labels:
        ax.text(
            label_x,
            y + bar_height / 2,
            label,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="left",
            va="center",
            family="monospace",
            style="italic",
            path_effects=stroke,
            zorder=5,
        )

    # --- Title ---
    ax.set_title(
        "G-Buffer Layout \u2014 Screen-Space Reflections",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/29-screen-space-reflections", "gbuffer_layout.png")


# ===================================================================
# GPU Lesson 29 — SSR Self-Intersection Guard
# ===================================================================


# ===================================================================
# GPU Lesson 29 — SSR Self-Intersection Guard
# ===================================================================


def diagram_ssr_self_intersection():
    """Visualise why SSR needs a minimum-travel guard to avoid self-hits."""
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig, axes = plt.subplots(
        1,
        2,
        figsize=(12, 4.5),
        facecolor=STYLE["bg"],
        gridspec_kw={"wspace": 0.35},
    )

    for ax in axes:
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-0.5, 10.5)
        ax.set_ylim(-1.0, 5.5)
        ax.set_aspect("equal")
        ax.axis("off")

    # -----------------------------------------------------------------
    # Shared geometry: a flat floor surface and a vertical wall
    # -----------------------------------------------------------------
    def _draw_scene(ax):
        # Floor surface
        ax.plot([-0.5, 10.5], [0.0, 0.0], color=STYLE["axis"], lw=2.5, zorder=3)
        ax.fill_between(
            [-0.5, 10.5], -1.0, 0.0, color=STYLE["surface"], alpha=0.6, zorder=1
        )
        ax.text(
            1.0,
            -0.5,
            "Reflective floor (depth buffer)",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="left",
            path_effects=stroke,
            zorder=10,
        )

        # Wall / object
        wall_x = 7.0
        ax.plot([wall_x, wall_x], [0.0, 4.5], color=STYLE["axis"], lw=2.5, zorder=3)
        ax.fill_betweenx(
            [0.0, 4.5], wall_x, 10.5, color=STYLE["surface"], alpha=0.6, zorder=1
        )
        ax.text(
            8.5,
            2.5,
            "Object",
            color=STYLE["text_dim"],
            fontsize=10,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=10,
        )

    # -----------------------------------------------------------------
    # Panel A: WITHOUT guard — self-hit on first step
    # -----------------------------------------------------------------
    ax_a = axes[0]
    _draw_scene(ax_a)
    ax_a.set_title(
        "Without self-intersection guard",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Ray origin on the floor
    origin = np.array([2.0, 0.0])
    # Reflected direction (shallow angle upward)
    r_dir = np.array([0.85, 0.25])
    r_dir = r_dir / np.linalg.norm(r_dir)

    # Surface normal
    ax_a.annotate(
        "",
        xy=(origin[0], origin[1] + 1.2),
        xytext=origin,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 2.0,
        },
        zorder=6,
    )
    ax_a.text(
        origin[0] - 0.35,
        origin[1] + 0.7,
        "N",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        zorder=10,
    )

    # Draw ray steps — first one is very close to surface and false-hits
    step_size = 0.6
    wall_x = 7.0
    travel_to_wall = (wall_x - origin[0]) / r_dir[0]
    n_steps = int(np.ceil(travel_to_wall / step_size)) + 1
    steps = [origin + r_dir * step_size * (i + 1) for i in range(n_steps)]

    # Step 1 is close to floor — false hit
    false_hit = steps[0]
    # Draw dashed ray up to false hit only
    ax_a.plot(
        [origin[0], false_hit[0]],
        [origin[1], false_hit[1]],
        "--",
        color=STYLE["accent1"],
        lw=1.2,
        alpha=0.5,
        zorder=4,
    )

    # Step dots — only first one shown (it's a false hit)
    ax_a.plot(
        false_hit[0], false_hit[1], "o", color=STYLE["accent2"], markersize=10, zorder=8
    )

    # Depth comparison line from step to floor
    ax_a.plot(
        [false_hit[0], false_hit[0]],
        [false_hit[1], 0.0],
        ":",
        color=STYLE["accent2"],
        lw=1.5,
        alpha=0.8,
        zorder=4,
    )
    ax_a.plot(false_hit[0], 0.0, "s", color=STYLE["accent2"], markersize=5, zorder=5)

    # Label: false hit
    ax_a.text(
        false_hit[0] + 0.3,
        false_hit[1] + 0.3,
        "False hit!",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        path_effects=stroke,
        zorder=10,
    )

    # Small annotation showing the tiny depth diff
    ax_a.annotate(
        "",
        xy=(false_hit[0] + 0.15, false_hit[1]),
        xytext=(false_hit[0] + 0.15, 0.0),
        arrowprops={"arrowstyle": "<->", "color": STYLE["warn"], "lw": 1.2},
        zorder=7,
    )
    ax_a.text(
        false_hit[0] + 0.5,
        false_hit[1] / 2,
        "tiny\ndepth\ndiff",
        color=STYLE["warn"],
        fontsize=7,
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # Show where the ray SHOULD have gone (ghosted)
    for sp in steps[1:5]:
        ax_a.plot(
            sp[0],
            sp[1],
            "o",
            color=STYLE["text_dim"],
            markersize=4,
            alpha=0.3,
            zorder=4,
        )
    ax_a.text(
        5.5,
        2.5,
        "Missed\n(ray stopped\ntoo early)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # Panel B: WITH guard — skips early steps, finds correct hit
    # -----------------------------------------------------------------
    ax_b = axes[1]
    _draw_scene(ax_b)
    ax_b.set_title(
        "With self-intersection guard",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Same origin and direction
    ax_b.annotate(
        "",
        xy=(origin[0], origin[1] + 1.2),
        xytext=origin,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 2.0,
        },
        zorder=6,
    )
    ax_b.text(
        origin[0] - 0.35,
        origin[1] + 0.7,
        "N",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        zorder=10,
    )

    # Draw all steps — first 2 are skipped (guard zone), then marching
    guard_steps = 2  # steps within SSR_MIN_TRAVEL
    hit_step = None

    for i, sp in enumerate(steps):
        if sp[0] >= 7.0:
            hit_step = i
            break

        if i < guard_steps:
            # Skipped steps — dimmed, with "skip" marker
            ax_b.plot(
                sp[0],
                sp[1],
                "o",
                color=STYLE["text_dim"],
                markersize=6,
                alpha=0.4,
                zorder=6,
            )
            ax_b.plot(
                sp[0],
                sp[1],
                "x",
                color=STYLE["accent2"],
                markersize=8,
                markeredgewidth=2.0,
                alpha=0.7,
                zorder=7,
            )
        else:
            # Active steps — normal color
            ax_b.plot(sp[0], sp[1], "o", color=STYLE["accent1"], markersize=6, zorder=8)
            # Depth comparison line
            ax_b.plot(
                [sp[0], sp[0]],
                [sp[1], 0.0],
                ":",
                color=STYLE["text_dim"],
                lw=1.0,
                alpha=0.3,
                zorder=4,
            )

    # Draw ray line up to hit point (only show wall hit if a step reached it)
    if hit_step is not None:
        t_wall = (wall_x - origin[0]) / r_dir[0]
        hit_on_wall = origin + r_dir * t_wall

        ax_b.plot(
            [origin[0], hit_on_wall[0]],
            [origin[1], hit_on_wall[1]],
            "--",
            color=STYLE["accent1"],
            lw=1.2,
            alpha=0.5,
            zorder=4,
        )

        # Hit marker on wall
        ax_b.plot(
            hit_on_wall[0],
            hit_on_wall[1],
            "o",
            color=STYLE["accent2"],
            markersize=10,
            zorder=8,
        )
        ax_b.plot(
            hit_on_wall[0],
            hit_on_wall[1],
            "o",
            color=STYLE["accent2"],
            markersize=16,
            markerfacecolor="none",
            markeredgewidth=2.5,
            zorder=9,
        )
        ax_b.text(
            hit_on_wall[0] - 0.4,
            hit_on_wall[1] + 0.4,
            "Correct hit!",
            color=STYLE["accent2"],
            fontsize=11,
            fontweight="bold",
            ha="right",
            path_effects=stroke,
            zorder=10,
        )
    else:
        # No step reached the wall — draw the full ray with a "miss" label
        last = steps[-1]
        ax_b.plot(
            [origin[0], last[0]],
            [origin[1], last[1]],
            "--",
            color=STYLE["accent1"],
            lw=1.2,
            alpha=0.5,
            zorder=4,
        )
        ax_b.text(
            last[0] + 0.3,
            last[1],
            "No hit",
            color=STYLE["text_dim"],
            fontsize=9,
            style="italic",
            path_effects=stroke,
            zorder=10,
        )

    # Guard zone bracket
    guard_end = origin + r_dir * step_size * guard_steps
    ax_b.annotate(
        "",
        xy=(guard_end[0], -0.6),
        xytext=(origin[0], -0.6),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent4"], "lw": 1.5},
        zorder=7,
    )
    ax_b.text(
        (origin[0] + guard_end[0]) / 2,
        -0.85,
        "Guard zone\n(no hit test)",
        color=STYLE["accent4"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=10,
    )

    # Legend
    legend_items = [
        ("x", STYLE["accent2"], "Skipped (guard)"),
        ("o", STYLE["accent1"], "Active step"),
        ("o", STYLE["accent2"], "Hit detected"),
    ]
    lx, ly = 0.0, 5.2
    for marker, color, label in legend_items:
        ax_b.plot(lx, ly, marker, color=color, markersize=6, zorder=8)
        ax_b.text(
            lx + 0.3,
            ly,
            label,
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=10,
        )
        ly -= 0.5

    fig.tight_layout()
    save(fig, "gpu/29-screen-space-reflections", "self_intersection.png")


# ===========================================================================
# gpu/30-planar-reflections
# ===========================================================================


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — reflection_camera.png
# ---------------------------------------------------------------------------
