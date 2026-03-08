"""Diagrams for gpu/35."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/35-decals — decal_box_projection.png
# ---------------------------------------------------------------------------


def diagram_decal_box_projection():
    """Oriented bounding box projecting a decal texture onto a curved surface.

    Shows a parabolic surface, a wireframe OBB around it, projection rays
    from the top of the box down through the surface, and the resulting
    UV-mapped decal region.
    """
    fig, ax = plt.subplots(1, 1, figsize=(10, 6), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-3, 3), ylim=(-2.5, 2.8), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # --- Curved surface (parabolic) ---
    t_surf = np.linspace(-2.2, 2.2, 200)
    y_surf = -0.12 * t_surf**2 - 0.8
    ax.fill_between(
        t_surf,
        y_surf - 0.3,
        y_surf,
        color=STYLE["surface"],
        alpha=0.6,
        zorder=1,
    )
    ax.plot(t_surf, y_surf, color=STYLE["text_dim"], linewidth=2.5, zorder=2)
    ax.text(
        2.4,
        -1.6,
        "scene surface",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="right",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Wireframe OBB (oriented bounding box) ---
    box_hw, box_hh = 1.2, 1.4  # half-width, half-height
    box_cy = 0.5
    # Rotate the box to show it is oriented (not axis-aligned)
    obb_theta = 0.2  # radians (~11 degrees)
    cos_t, sin_t = np.cos(obb_theta), np.sin(obb_theta)
    box_corners_local = [
        (-box_hw, -box_hh),
        (box_hw, -box_hh),
        (box_hw, box_hh),
        (-box_hw, box_hh),
    ]
    box_corners = [
        (cos_t * x - sin_t * y, sin_t * x + cos_t * y + box_cy)
        for x, y in box_corners_local
    ]
    # Draw wireframe box
    for i in range(4):
        x0, y0 = box_corners[i]
        x1, y1 = box_corners[(i + 1) % 4]
        ax.plot(
            [x0, x1],
            [y0, y1],
            color=STYLE["accent1"],
            linewidth=2,
            linestyle="--",
            zorder=4,
        )
    # Label at top-right corner
    label_x, label_y = box_corners[2]
    ax.text(
        label_x + 0.15,
        label_y,
        "OBB",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="top",
        path_effects=stroke,
        zorder=5,
    )

    # --- Projection rays along OBB local Y axis down to surface ---
    n_rays = 7
    ray_xs = np.linspace(-box_hw + 0.15, box_hw - 0.15, n_rays)
    # OBB local Y direction (projection axis) rotated into world space
    proj_dx = sin_t  # world X component of local -Y direction
    proj_dy = -cos_t  # world Y component of local -Y direction

    def project_to_surface(start_x, start_y):
        """March along projection direction until hitting the curved surface."""
        hx, hy = start_x, start_y
        for _ in range(200):
            sy = -0.12 * hx**2 - 0.8
            if hy <= sy:
                break
            hx += proj_dx * 0.02
            hy += proj_dy * 0.02
        return hx, hy

    for rx in ray_xs:
        # Ray origin at top of box in OBB local coords, rotated to world
        rx_top = cos_t * rx - sin_t * box_hh
        ry_top = sin_t * rx + cos_t * box_hh + box_cy
        hit_x, hit_y = project_to_surface(rx_top, ry_top)
        ax.plot(
            [rx_top, hit_x],
            [ry_top, hit_y],
            color=STYLE["accent4"],
            linewidth=1,
            alpha=0.5,
            zorder=3,
        )
        ax.plot(hit_x, hit_y, "o", color=STYLE["accent2"], markersize=4, zorder=5)

    # Arrow label for projection direction (rotated with OBB)
    arrow_offset = box_hw + 0.6
    arrow_top_x = cos_t * arrow_offset - sin_t * (box_hh - 0.2)
    arrow_top_y = sin_t * arrow_offset + cos_t * (box_hh - 0.2) + box_cy
    arrow_bot_x = cos_t * arrow_offset - sin_t * (-box_hh + 0.2)
    arrow_bot_y = sin_t * arrow_offset + cos_t * (-box_hh + 0.2) + box_cy
    ax.annotate(
        "",
        xy=(arrow_bot_x, arrow_bot_y),
        xytext=(arrow_top_x, arrow_top_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": STYLE["accent4"],
            "lw": 2,
        },
        zorder=5,
    )
    label_x = (arrow_top_x + arrow_bot_x) / 2 + 0.15
    label_y = (arrow_top_y + arrow_bot_y) / 2
    ax.text(
        label_x,
        label_y,
        "projection\ndirection",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Decal region on the surface (highlighted, follows OBB projection) ---
    # Sample local X positions across the OBB width, project along OBB Y to surface
    t_local = np.linspace(-box_hw, box_hw, 100)
    decal_xs = []
    decal_ys = []
    for lx in t_local:
        wx = cos_t * lx - sin_t * box_hh
        wy = sin_t * lx + cos_t * box_hh + box_cy
        hx, hy = project_to_surface(wx, wy)
        decal_xs.append(hx)
        decal_ys.append(hy)
    ax.plot(decal_xs, decal_ys, color=STYLE["accent2"], linewidth=4, zorder=3)
    mid_idx = len(decal_xs) // 2
    ax.text(
        decal_xs[mid_idx],
        decal_ys[mid_idx] - 0.35,
        "decal mapped via UV",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=5,
    )

    # --- UV coordinate labels at box edges ---
    ax.text(
        decal_xs[0],
        decal_ys[0] - 0.25,
        "U=0",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )
    ax.text(
        decal_xs[-1],
        decal_ys[-1] - 0.25,
        "U=1",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.suptitle(
        "Decal Box Projection: OBB Projects Texture onto Scene Geometry",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "gpu/35-decals", "decal_box_projection.png")


# ---------------------------------------------------------------------------
# gpu/35-decals — depth_reconstruction.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/35-decals — depth_reconstruction.png
# ---------------------------------------------------------------------------


def diagram_decal_depth_reconstruction():
    """Pipeline diagram showing how world position is reconstructed from depth.

    Flow: Screen UV -> Depth Sample -> NDC Position (with Y-flip)
    -> Clip Position (float4 lift) -> inv(VP) Multiply -> World Position.
    """
    fig, ax = plt.subplots(1, 1, figsize=(14, 4), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 18), ylim=(0.5, 4.8), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    def draw_box(cx, cy, w, h, label, color, sublabel=None, fontsize=9):
        rect = FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.12",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.2,
            zorder=2,
        )
        ax.add_patch(rect)
        text_y = cy + 0.15 if sublabel else cy
        ax.text(
            cx,
            text_y,
            label,
            color=color,
            fontsize=fontsize,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        if sublabel:
            ax.text(
                cx,
                cy - 0.35,
                sublabel,
                color=STYLE["text_dim"],
                fontsize=7,
                fontfamily="monospace",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    def draw_arrow(x1, x2, y, color):
        ax.annotate(
            "",
            xy=(x2, y),
            xytext=(x1, y),
            arrowprops={
                "arrowstyle": "->,head_width=0.3,head_length=0.15",
                "color": color,
                "lw": 2.5,
            },
            zorder=3,
        )

    # Pipeline stages
    stages = [
        (1.5, "Screen UV", STYLE["text_dim"], "(frag_pos / resolution)"),
        (4.5, "Depth Sample", STYLE["accent4"], "texture(depth_tex, uv)"),
        (7.5, "NDC Position", STYLE["accent1"], "xy: uv*2-1, y-flip\nz: depth"),
        (10.5, "Clip Position", STYLE["accent1"], "float4(ndc_xy,\n depth, 1.0)"),
        (13.5, "inv(VP) Multiply", STYLE["accent2"], "inv_vp * clip_pos"),
        (16.5, "World Position", STYLE["accent3"], "result.xyz / result.w"),
    ]

    y_center = 2.8
    box_w = 2.8
    box_h = 1.8

    for cx, label, color, sublabel in stages:
        draw_box(cx, y_center, box_w, box_h, label, color, sublabel)

    # Arrows between stages — computed from stage positions
    arrow_y = y_center
    arrow_colors = [
        STYLE["text_dim"],
        STYLE["accent4"],
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
    ]
    for i, color in enumerate(arrow_colors):
        src_x = stages[i][0] + box_w / 2
        dst_x = stages[i + 1][0] - box_w / 2
        draw_arrow(src_x, dst_x, arrow_y, color)

    # Y-flip annotation
    ax.text(
        9.0,
        1.5,
        "Y-flip: NDC.y = -(uv.y * 2 - 1)\n(Vulkan/Metal have flipped Y vs OpenGL)",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
        fontstyle="italic",
    )

    # Perspective divide annotation
    ax.text(
        16.2,
        1.5,
        "Perspective divide:\nworld_h = inv_vp * clip\nworld = world_h.xyz / world_h.w",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
        fontstyle="italic",
    )

    fig.suptitle(
        "Depth Reconstruction Pipeline: Screen Space to World Position",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.92])
    save(fig, "gpu/35-decals", "depth_reconstruction.png")


# ---------------------------------------------------------------------------
# gpu/35-decals — decal_local_space.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/35-decals — decal_local_space.png
# ---------------------------------------------------------------------------


def diagram_decal_local_space():
    """Unit cube [-0.5, 0.5]^3 with XZ plane as UV mapping plane.

    Shows the UV = local.xz + 0.5 mapping, the unit cube bounds,
    and points inside vs outside the volume.
    """
    fig, ax = plt.subplots(1, 1, figsize=(9, 9), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1.5, 2.0), ylim=(-1.5, 2.0), grid=True)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # --- Unit cube face (XZ plane, shown as 2D square [-0.5, 0.5]) ---
    cube_rect = Rectangle(
        (-0.5, -0.5),
        1.0,
        1.0,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        alpha=0.15,
        zorder=2,
    )
    ax.add_patch(cube_rect)

    # Cube border (solid)
    for x0, y0, x1, y1 in [
        (-0.5, -0.5, 0.5, -0.5),
        (0.5, -0.5, 0.5, 0.5),
        (0.5, 0.5, -0.5, 0.5),
        (-0.5, 0.5, -0.5, -0.5),
    ]:
        ax.plot(
            [x0, x1],
            [y0, y1],
            color=STYLE["accent1"],
            linewidth=2.5,
            zorder=3,
        )

    # --- Axis labels ---
    ax.annotate(
        "",
        xy=(1.6, 0),
        xytext=(-1.0, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["axis"],
            "lw": 1.5,
        },
        zorder=1,
    )
    ax.text(
        1.7,
        -0.1,
        "local X",
        color=STYLE["axis"],
        fontsize=10,
        ha="left",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )
    ax.annotate(
        "",
        xy=(0, 1.6),
        xytext=(0, -1.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["axis"],
            "lw": 1.5,
        },
        zorder=1,
    )
    ax.text(
        -0.1,
        1.7,
        "local Z",
        color=STYLE["axis"],
        fontsize=10,
        ha="right",
        va="bottom",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Corner labels for the unit cube ---
    corners = [
        (-0.5, -0.5, "(-0.5, -0.5)"),
        (0.5, -0.5, "(0.5, -0.5)"),
        (0.5, 0.5, "(0.5, 0.5)"),
        (-0.5, 0.5, "(-0.5, 0.5)"),
    ]
    for cx, cy, clabel in corners:
        ax.plot(cx, cy, "s", color=STYLE["accent1"], markersize=7, zorder=6)
        offset_x = -0.12 if cx < 0 else 0.12
        offset_y = -0.12 if cy < 0 else 0.12
        ha = "right" if cx < 0 else "left"
        va = "top" if cy < 0 else "bottom"
        ax.text(
            cx + offset_x,
            cy + offset_y,
            clabel,
            color=STYLE["accent1"],
            fontsize=8,
            fontfamily="monospace",
            ha=ha,
            va=va,
            path_effects=stroke_thin,
            zorder=5,
        )

    # --- UV mapping annotation ---
    ax.text(
        0.0,
        -0.85,
        "UV = local.xz + 0.5",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        0.0,
        -1.1,
        "maps [-0.5, 0.5] \u2192 [0, 1]",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Points inside the cube (accepted) ---
    inside_pts = [(0.1, 0.2), (-0.3, -0.1), (0.25, -0.35), (-0.15, 0.4)]
    for px, py in inside_pts:
        ax.plot(px, py, "o", color=STYLE["accent3"], markersize=8, zorder=6)

    ax.text(
        0.45,
        0.15,
        "INSIDE\n(rendered)",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Points outside the cube (rejected / clipped) ---
    outside_pts = [(0.9, 0.7), (-0.8, -0.9), (1.1, -0.3), (-0.7, 1.1)]
    for px, py in outside_pts:
        ax.plot(
            px,
            py,
            "x",
            color=STYLE["accent2"],
            markersize=10,
            markeredgewidth=2.5,
            zorder=6,
        )

    ax.text(
        1.15,
        0.85,
        "OUTSIDE\n(discarded)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Bounds check formula ---
    ax.text(
        0.0,
        1.85,
        "Bounds check:  abs(local.x) < 0.5  &&  abs(local.y) < 0.5  &&  abs(local.z) < 0.5",
        color=STYLE["text"],
        fontsize=10,
        fontfamily="monospace",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    fig.suptitle(
        "Decal Local Space: Unit Cube and UV Mapping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "gpu/35-decals", "decal_local_space.png")


# ---------------------------------------------------------------------------
# gpu/35-decals — render_pass_architecture.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/35-decals — render_pass_architecture.png
# ---------------------------------------------------------------------------


def diagram_decal_render_pipeline():
    """3-pass rendering pipeline for decal rendering.

    Pass 1: Shadow (D32_FLOAT), Pass 2: Scene (Swapchain + D24S8),
    Pass 3: Decals (Swapchain LOAD, reads scene_depth as sampler).
    Shows the depth texture flowing from Pass 2 output to Pass 3 input.
    """
    fig, ax = plt.subplots(1, 1, figsize=(15, 7), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 16), ylim=(0, 8), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    def draw_pass_box(cx, cy, w, h, title, color, outputs):
        """Draw a render pass box with title and output labels."""
        rect = FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.5,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy + h / 2 - 0.4,
            title,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke,
            zorder=5,
        )
        for i, (olabel, ocolor) in enumerate(outputs):
            ax.text(
                cx,
                cy + 0.1 - i * 0.55,
                olabel,
                color=ocolor,
                fontsize=8,
                fontfamily="monospace",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    def draw_arrow_h(x1, x2, y, color, label=None):
        ax.annotate(
            "",
            xy=(x2, y),
            xytext=(x1, y),
            arrowprops={
                "arrowstyle": "->,head_width=0.3,head_length=0.15",
                "color": color,
                "lw": 2.5,
            },
            zorder=3,
        )
        if label:
            mid_x = (x1 + x2) / 2
            ax.text(
                mid_x,
                y + 0.3,
                label,
                color=color,
                fontsize=7.5,
                ha="center",
                va="bottom",
                path_effects=stroke_thin,
                zorder=5,
            )

    # --- Pass 1: Shadow ---
    draw_pass_box(
        2.5,
        5.5,
        4.0,
        2.5,
        "Pass 1: Shadow",
        STYLE["text_dim"],
        [
            ("depth-only render", STYLE["text_dim"]),
            ("output: D32_FLOAT", STYLE["text_dim"]),
        ],
    )

    # --- Pass 2: Scene ---
    draw_pass_box(
        8.0,
        5.5,
        4.5,
        2.5,
        "Pass 2: Scene",
        STYLE["accent1"],
        [
            ("color: Swapchain (CLEAR)", STYLE["accent1"]),
            ("depth: scene_depth (D24S8)", STYLE["accent3"]),
        ],
    )

    # --- Pass 3: Decals ---
    draw_pass_box(
        13.5,
        5.5,
        4.0,
        2.5,
        "Pass 3: Decals",
        STYLE["accent2"],
        [
            ("color: Swapchain (LOAD)", STYLE["accent2"]),
            ("reads: scene_depth sampler", STYLE["accent3"]),
        ],
    )

    # Arrows between passes
    draw_arrow_h(4.6, 5.7, 5.5, STYLE["text_dim"])
    draw_arrow_h(10.3, 11.4, 5.5, STYLE["accent1"])

    # --- Depth texture flow (key connection: Pass 2 depth -> Pass 3 input) ---
    # Draw a curved arrow from Pass 2 depth output down and over to Pass 3 input
    depth_flow_x = [8.0, 8.0, 10.8, 10.8, 13.5, 13.5]
    depth_flow_y = [4.2, 2.5, 2.5, 2.5, 2.5, 4.2]
    ax.plot(
        depth_flow_x[:3],
        depth_flow_y[:3],
        color=STYLE["accent3"],
        linewidth=2.5,
        linestyle="-",
        zorder=3,
    )
    ax.plot(
        depth_flow_x[2:5],
        depth_flow_y[2:5],
        color=STYLE["accent3"],
        linewidth=2.5,
        linestyle="-",
        zorder=3,
    )
    ax.annotate(
        "",
        xy=(13.5, 4.2),
        xytext=(13.5, 2.5),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
        zorder=3,
    )

    # Label the depth flow
    ax.text(
        10.8,
        2.15,
        "scene_depth texture",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        10.8,
        1.7,
        "Pass 2 writes depth \u2192 Pass 3 samples it as texture",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
        fontstyle="italic",
    )

    # --- Format annotations ---
    formats = [
        (2.5, 3.5, "D32_FLOAT\n(shadow map)", STYLE["text_dim"]),
        (8.0, 3.5, "D24_UNORM_S8_UINT\n(scene depth-stencil)", STYLE["accent1"]),
        (13.5, 1.2, "Sampled as\nfloat depth", STYLE["accent2"]),
    ]
    for fx, fy, flabel, fcolor in formats:
        ax.text(
            fx,
            fy,
            flabel,
            color=fcolor,
            fontsize=8,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
            fontstyle="italic",
        )

    fig.suptitle(
        "Decal Render Pipeline: 3-Pass Architecture with Depth Sharing",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "gpu/35-decals", "render_pass_architecture.png")


# ---------------------------------------------------------------------------
# gpu/35-decals — back_face_culling_decals.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/35-decals — back_face_culling_decals.png
# ---------------------------------------------------------------------------


def diagram_back_face_culling_decals():
    """Side-view comparison of CULL_BACK vs CULL_FRONT for decal box rendering.

    Left: CULL_BACK (front faces rendered) causes decal to disappear when
    camera enters the box. Right: CULL_FRONT (back faces rendered) keeps
    the decal visible from inside.
    """
    fig, (ax_left, ax_right) = plt.subplots(
        1, 2, figsize=(14, 6), facecolor=STYLE["bg"]
    )

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    for ax in (ax_left, ax_right):
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-0.5, 6), grid=False, aspect=None)
        ax.set_xticks([])
        ax.set_yticks([])

    def draw_scene(
        ax,
        title,
        title_color,
        cull_label,
        camera_inside,
        result_text,
        result_color,
        face_label,
        face_color,
    ):
        """Draw one culling scenario."""
        # Title
        ax.text(
            4.0,
            5.7,
            title,
            color=title_color,
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke,
            zorder=10,
        )

        # Ground surface
        ax.plot([0, 7.5], [1.0, 1.0], color=STYLE["text_dim"], linewidth=2, zorder=1)
        ax.text(
            7.5,
            0.7,
            "surface",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Decal box (side view — rectangle)
        box_x, box_y, box_w, box_h = 2.0, 0.5, 3.5, 3.0
        box_rect = Rectangle(
            (box_x, box_y),
            box_w,
            box_h,
            facecolor=STYLE["accent1"],
            edgecolor=STYLE["accent1"],
            linewidth=2.5,
            alpha=0.12,
            zorder=2,
        )
        ax.add_patch(box_rect)

        # Determine which face to highlight based on the label
        front_face_x = box_x + box_w
        back_face_x = box_x
        is_back_face = "back face" in face_label
        highlighted_x = back_face_x if is_back_face else front_face_x
        other_x = front_face_x if is_back_face else back_face_x
        ax.plot(
            [highlighted_x, highlighted_x],
            [box_y, box_y + box_h],
            color=face_color,
            linewidth=3.5,
            zorder=4,
        )
        ax.text(
            highlighted_x + (-0.1 if is_back_face else 0.1),
            box_y + box_h + 0.15,
            face_label,
            color=face_color,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Other face (dimmed)
        other_color = (
            STYLE["accent3"] if face_color != STYLE["accent3"] else STYLE["text_dim"]
        )
        ax.plot(
            [other_x, other_x],
            [box_y, box_y + box_h],
            color=other_color,
            linewidth=2,
            linestyle="--",
            zorder=3,
        )

        # Box label
        ax.text(
            box_x + box_w / 2,
            box_y + box_h + 0.15,
            "decal OBB",
            color=STYLE["accent1"],
            fontsize=9,
            ha="center",
            va="bottom",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Camera (triangle icon)
        cam_x = box_x + box_w - 0.8 if camera_inside else box_x + box_w + 1.5
        cam_y = 2.0
        cam_verts = [
            (cam_x + 0.4, cam_y),
            (cam_x - 0.25, cam_y + 0.3),
            (cam_x - 0.25, cam_y - 0.3),
        ]
        cam = Polygon(
            cam_verts,
            facecolor=STYLE["warn"],
            edgecolor=STYLE["text"],
            linewidth=1.5,
            zorder=6,
        )
        ax.add_patch(cam)
        ax.text(
            cam_x,
            cam_y - 0.55,
            "camera\n(INSIDE)" if camera_inside else "camera",
            color=STYLE["warn"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
        )

        # View ray from camera
        ray_end_x = box_x - 0.3
        ax.annotate(
            "",
            xy=(ray_end_x, cam_y),
            xytext=(cam_x - 0.3, cam_y),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.1",
                "color": STYLE["warn"],
                "lw": 1.5,
                "linestyle": "--",
            },
            zorder=4,
        )

        # Cull mode label
        ax.text(
            4.0,
            4.8,
            cull_label,
            color=title_color,
            fontsize=10,
            fontfamily="monospace",
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Result
        result_rect = FancyBboxPatch(
            (0.5, -0.2),
            6.5,
            0.8,
            boxstyle="round,pad=0.08",
            facecolor=result_color,
            edgecolor=result_color,
            linewidth=1.5,
            alpha=0.2,
            zorder=2,
        )
        ax.add_patch(result_rect)
        ax.text(
            4.0,
            0.2,
            result_text,
            color=result_color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    # Left: CULL_BACK — camera inside, front faces rendered but face away
    draw_scene(
        ax_left,
        "CULL_BACK (default)",
        STYLE["accent2"],
        "SDL_GPU_CULLMODE_BACK",
        camera_inside=True,
        result_text="Back faces are culled when camera is inside \u2192 decal DISAPPEARS",
        result_color=STYLE["accent2"],
        face_label="back face\n(culled by\nCULL_BACK)",
        face_color=STYLE["accent2"],
    )

    # Right: CULL_FRONT — camera inside, back faces rendered and face toward camera
    draw_scene(
        ax_right,
        "CULL_FRONT (correct for decals)",
        STYLE["accent3"],
        "SDL_GPU_CULLMODE_FRONT",
        camera_inside=True,
        result_text="Back faces face TOWARD camera inside \u2192 decal VISIBLE",
        result_color=STYLE["accent3"],
        face_label="back face\n(rendered,\nfaces camera)",
        face_color=STYLE["accent3"],
    )

    fig.suptitle(
        "Back-Face Culling for Decal Boxes: Why CULL_FRONT Is Required",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, "gpu/35-decals", "back_face_culling_decals.png")


# ---------------------------------------------------------------------------
# gpu/35-decals — decal_layering.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/35-decals — decal_layering.png
# ---------------------------------------------------------------------------


def diagram_decal_layering():
    """Overlapping decal boxes and stencil increment to prevent double-blending.

    Left side shows artifacts from naive blending (double-blend regions),
    right side shows clean overlap using stencil increment technique.
    """
    fig, (ax_left, ax_right) = plt.subplots(
        1, 2, figsize=(14, 7), facecolor=STYLE["bg"]
    )

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    def draw_overlap_scene(ax, title, title_color, has_stencil):
        """Draw a top-down view of overlapping decal boxes on a surface."""
        setup_axes(ax, xlim=(-0.5, 8), ylim=(-1, 7), grid=False, aspect=None)
        ax.set_xticks([])
        ax.set_yticks([])

        # Title
        ax.text(
            4.0,
            6.7,
            title,
            color=title_color,
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke,
            zorder=10,
        )

        # Ground plane
        ground = Rectangle(
            (0.3, 0.8),
            7.0,
            5.0,
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            linewidth=1.5,
            zorder=1,
        )
        ax.add_patch(ground)
        ax.text(
            3.8,
            0.5,
            "scene surface (top-down view)",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Decal A (left)
        decal_a = Rectangle(
            (1.0, 1.8),
            3.5,
            3.0,
            facecolor=STYLE["accent1"],
            edgecolor=STYLE["accent1"],
            linewidth=2.5,
            alpha=0.25,
            zorder=2,
        )
        ax.add_patch(decal_a)
        ax.text(
            2.0,
            4.5,
            "Decal A",
            color=STYLE["accent1"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Decal B (right, overlapping)
        decal_b_x, decal_b_y, decal_b_w, decal_b_h = 3.0, 2.3, 3.5, 2.5

        # Overlap region
        overlap_x = decal_b_x
        overlap_y = decal_b_y
        overlap_w = 1.5  # intersection width
        overlap_h = decal_b_h

        # When stencil is active, only draw B outside the overlap region
        decal_b_regions = (
            [(decal_b_x + overlap_w, decal_b_y, decal_b_w - overlap_w, decal_b_h)]
            if has_stencil
            else [(decal_b_x, decal_b_y, decal_b_w, decal_b_h)]
        )
        for bx, by, bw, bh in decal_b_regions:
            decal_b = Rectangle(
                (bx, by),
                bw,
                bh,
                facecolor=STYLE["accent2"],
                edgecolor=STYLE["accent2"],
                linewidth=2.5,
                alpha=0.25,
                zorder=3,
            )
            ax.add_patch(decal_b)
        ax.text(
            5.5,
            4.5,
            "Decal B",
            color=STYLE["accent2"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        if has_stencil:
            # Clean overlap — stencil rejects B, keeps A's appearance
            overlap = Rectangle(
                (overlap_x, overlap_y),
                overlap_w,
                overlap_h,
                facecolor="none",
                edgecolor=STYLE["accent3"],
                linewidth=2,
                linestyle="--",
                hatch="//",
                alpha=0.8,
                zorder=4,
            )
            ax.add_patch(overlap)
            ax.text(
                overlap_x + overlap_w / 2,
                overlap_y + overlap_h / 2,
                "B rejected\nstencil keeps A",
                color=STYLE["accent3"],
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=6,
            )
        else:
            # Artifact: double-blended region is darker / over-saturated
            overlap = Rectangle(
                (overlap_x, overlap_y),
                overlap_w,
                overlap_h,
                facecolor=STYLE["accent2"],
                edgecolor=STYLE["warn"],
                linewidth=3,
                alpha=0.7,
                linestyle="--",
                zorder=4,
            )
            ax.add_patch(overlap)
            ax.text(
                overlap_x + overlap_w / 2,
                overlap_y + overlap_h / 2,
                "DOUBLE\nBLEND\nartifact",
                color=STYLE["warn"],
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=6,
            )

        # Stencil technique annotation
        if has_stencil:
            technique_lines = [
                "1. Draw decal A, increment stencil",
                "2. Draw decal B, test stencil == 0",
                "3. Overlap pixels already marked \u2192 reject",
            ]
            for i, line in enumerate(technique_lines):
                ax.text(
                    4.0,
                    -0.0 - i * 0.35,
                    line,
                    color=STYLE["accent3"],
                    fontsize=8,
                    ha="center",
                    va="top",
                    path_effects=stroke_thin,
                    zorder=5,
                )
        else:
            ax.text(
                4.0,
                -0.0,
                "Both decals blend at full opacity\nin the overlap region",
                color=STYLE["accent2"],
                fontsize=8,
                ha="center",
                va="top",
                path_effects=stroke_thin,
                zorder=5,
                fontstyle="italic",
            )

    # Left: Without stencil (artifacts)
    draw_overlap_scene(
        ax_left,
        "Without Stencil: Double-Blend Artifacts",
        STYLE["accent2"],
        has_stencil=False,
    )

    # Right: With stencil increment (clean)
    draw_overlap_scene(
        ax_right,
        "With Stencil Increment: Clean Overlap",
        STYLE["accent3"],
        has_stencil=True,
    )

    fig.suptitle(
        "Decal Layering: Stencil Increment Prevents Double-Blending",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, "gpu/35-decals", "decal_layering.png")


# ---------------------------------------------------------------------------
# gpu/36-edge-detection — sobel_kernels.png
# ---------------------------------------------------------------------------
