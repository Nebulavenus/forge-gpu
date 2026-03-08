"""Diagrams for gpu/15."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

from .._common import STYLE, save, setup_axes


def diagram_cascaded_shadow_maps():
    """Cascaded shadow maps: how the view frustum is split into cascades.

    Shows a side view of the camera frustum divided into 3 cascades at
    logarithmic-linear split distances.  Each cascade has its own
    orthographic projection from the light, covering a progressively
    larger area with the same shadow map resolution.
    """
    fig = plt.figure(figsize=(12, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 14), ylim=(-4, 4.5), grid=False)

    # Camera position
    cam_x, cam_y = 0.0, 0.0
    ax.plot(cam_x, cam_y, "o", color=STYLE["text"], markersize=8, zorder=10)
    ax.text(
        cam_x - 0.5,
        cam_y - 0.6,
        "Camera",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
    )

    # Frustum parameters
    near = 0.5
    far = 12.0
    half_fov = 0.35  # radians, for visual purposes

    # Cascade splits (logarithmic-linear blend, lambda=0.5)
    splits = [near]
    for i in range(1, 4):
        p = i / 3.0
        log_s = near * (far / near) ** p
        lin_s = near + (far - near) * p
        splits.append(0.5 * log_s + 0.5 * lin_s)

    cascade_colors = [STYLE["accent1"], STYLE["accent3"], STYLE["accent2"]]
    cascade_labels = ["Cascade 0\n(nearest)", "Cascade 1\n(mid)", "Cascade 2\n(far)"]
    cascade_alphas = [0.35, 0.25, 0.18]

    # Draw each cascade as a trapezoid
    for ci in range(3):
        d_near = splits[ci]
        d_far = splits[ci + 1]
        h_near = d_near * np.tan(half_fov)
        h_far = d_far * np.tan(half_fov)

        verts = [
            (d_near, -h_near),
            (d_far, -h_far),
            (d_far, h_far),
            (d_near, h_near),
        ]
        poly = Polygon(
            verts,
            closed=True,
            facecolor=cascade_colors[ci],
            edgecolor=cascade_colors[ci],
            alpha=cascade_alphas[ci],
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(poly)

        # Cascade label at center
        cx = (d_near + d_far) / 2.0
        ax.text(
            cx,
            0.0,
            cascade_labels[ci],
            color=cascade_colors[ci],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
        )

        # Split distance annotation at bottom
        if ci < 2:
            ax.axvline(
                x=d_far,
                color=STYLE["text_dim"],
                linestyle="--",
                linewidth=1,
                alpha=0.6,
                zorder=1,
            )
            ax.text(
                d_far,
                -h_far - 0.5,
                f"split {ci}",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
            )

    # Draw frustum outline
    h_near_full = near * np.tan(half_fov)
    h_far_full = far * np.tan(half_fov)
    ax.plot([cam_x, far], [0, h_far_full], "-", color=STYLE["axis"], linewidth=1.5)
    ax.plot([cam_x, far], [0, -h_far_full], "-", color=STYLE["axis"], linewidth=1.5)
    ax.plot(
        [far, far], [-h_far_full, h_far_full], "-", color=STYLE["axis"], linewidth=1.5
    )

    # Light direction arrow
    ax.annotate(
        "",
        xy=(9.0, 3.0),
        xytext=(12.0, 4.2),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
    )
    ax.text(
        12.2,
        4.2,
        "Light\ndirection",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        va="center",
    )

    # Near/far labels
    ax.text(
        near,
        h_near_full + 0.4,
        "near",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )
    ax.text(
        far,
        h_far_full + 0.4,
        "far",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    # Resolution note
    ax.text(
        6.0,
        -3.5,
        "Each cascade uses the same shadow map resolution (2048\u00b2)\n"
        "Near cascades cover less area \u2192 higher effective resolution",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        style="italic",
    )

    ax.set_title(
        "Cascaded Shadow Maps \u2014 Frustum Partitioning",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/15-cascaded-shadow-maps", "cascaded_shadow_maps.png")


def diagram_cascade_ortho_projections():
    """Side view of cascade orthographic projections aligned to the light.

    Shows the camera frustum from the side (camera looks right, light shines
    from the upper-left at an angle — matching the lesson's LIGHT_DIR).  Each
    cascade slice gets its own orthographic projection, drawn as a rotated
    rectangle aligned to the light direction (because the AABB is computed in
    light view space, not camera space).  Near cascades produce small, tight
    boxes; far cascades produce larger ones.
    """
    fig = plt.figure(figsize=(12, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-2, 15), ylim=(-6.5, 8.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])

    # Camera position
    cam_x, cam_y = 0.0, 0.0
    ax.plot(cam_x, cam_y, "o", color=STYLE["text"], markersize=8, zorder=10)
    ax.text(
        cam_x - 0.5,
        cam_y - 0.6,
        "Camera",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
    )

    # Frustum parameters (visual scale)
    near = 0.5
    far = 12.0
    half_fov = 0.35  # radians

    # Cascade splits — same logarithmic-linear blend as main.c
    splits = [near]
    for i in range(1, 4):
        p = i / 3.0
        log_s = near * (far / near) ** p
        lin_s = near + (far - near) * p
        splits.append(0.5 * log_s + 0.5 * lin_s)

    cascade_colors = [STYLE["accent1"], STYLE["accent3"], STYLE["accent2"]]
    cascade_labels = ["Cascade 0\n(nearest)", "Cascade 1\n(mid)", "Cascade 2\n(far)"]

    # Light direction in the 2D side-view plane.
    # main.c uses LIGHT_DIR (1, 1, 0.5) — this points TOWARD the light.
    # In our side view (x = camera forward, y = up), the light comes from
    # the upper-right and shines toward the lower-left.  We use the direction
    # the light travels (negated): roughly (-1, -1) normalized, but we tilt
    # it to ~60° from horizontal so the angle is visually clear.
    light_angle = np.radians(240)  # 240° from +X = lower-left direction
    light_dx = np.cos(light_angle)  # light travel direction x
    light_dy = np.sin(light_angle)  # light travel direction y

    # Light's local axes (for computing oriented bounding boxes)
    # light_forward = direction light travels (into the scene)
    # light_right = perpendicular (the "width" axis of the ortho box)
    lf = np.array([light_dx, light_dy])
    lr = np.array([-light_dy, light_dx])  # 90° CCW rotation

    # Draw the frustum outline (faint)
    h_near_full = near * np.tan(half_fov)
    h_far_full = far * np.tan(half_fov)
    ax.plot(
        [cam_x, far],
        [0, h_far_full],
        "-",
        color=STYLE["axis"],
        linewidth=1.2,
        alpha=0.5,
    )
    ax.plot(
        [cam_x, far],
        [0, -h_far_full],
        "-",
        color=STYLE["axis"],
        linewidth=1.2,
        alpha=0.5,
    )
    ax.plot(
        [far, far],
        [-h_far_full, h_far_full],
        "-",
        color=STYLE["axis"],
        linewidth=1.2,
        alpha=0.5,
    )

    # Draw each cascade slice and its light-aligned orthographic box
    for ci in range(3):
        d_near = splits[ci]
        d_far = splits[ci + 1]
        h_near_c = d_near * np.tan(half_fov)
        h_far_c = d_far * np.tan(half_fov)

        # The 4 corners of this cascade slice (trapezoid in side view)
        corners = np.array(
            [
                [d_near, h_near_c],  # near-top
                [d_near, -h_near_c],  # near-bottom
                [d_far, h_far_c],  # far-top
                [d_far, -h_far_c],  # far-bottom
            ]
        )

        # Cascade frustum slice (trapezoid, lightly filled)
        trap_verts = [
            (d_near, -h_near_c),
            (d_far, -h_far_c),
            (d_far, h_far_c),
            (d_near, h_near_c),
        ]
        poly = Polygon(
            trap_verts,
            closed=True,
            facecolor=cascade_colors[ci],
            edgecolor=cascade_colors[ci],
            alpha=0.15,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(poly)

        # Project cascade corners onto the light's axes to find the
        # oriented bounding box (OBB) — this is what the code does when
        # it transforms corners into light view space and computes AABB.
        proj_fwd = corners @ lf  # projection onto light forward axis
        proj_rgt = corners @ lr  # projection onto light right axis

        fwd_min, fwd_max = proj_fwd.min(), proj_fwd.max()
        rgt_min, rgt_max = proj_rgt.min(), proj_rgt.max()

        # Reconstruct the 4 OBB corners in world (diagram) space
        obb_corners = np.array(
            [
                lf * fwd_min + lr * rgt_min,
                lf * fwd_max + lr * rgt_min,
                lf * fwd_max + lr * rgt_max,
                lf * fwd_min + lr * rgt_max,
            ]
        )

        obb_poly = Polygon(
            obb_corners,
            closed=True,
            facecolor="none",
            edgecolor=cascade_colors[ci],
            linewidth=2.5,
            linestyle="-",
            zorder=5,
        )
        ax.add_patch(obb_poly)

        # Cascade label at the center of the trapezoid
        cx = (d_near + d_far) / 2.0
        ax.text(
            cx,
            0.0,
            cascade_labels[ci],
            color=cascade_colors[ci],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Resolution annotation along the top edge of each OBB
        top_mid = lf * (fwd_min + fwd_max) / 2 + lr * rgt_max
        # Offset outward along the light-right axis
        label_pos = top_mid + lr * 0.4
        ax.text(
            label_pos[0],
            label_pos[1],
            "2048\u00b2",
            color=cascade_colors[ci],
            fontsize=7,
            ha="center",
            va="bottom",
            style="italic",
            rotation=np.degrees(light_angle),
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Light direction arrow — show the light coming from the upper-left
    arrow_start = np.array([8.0, 7.5])
    arrow_end = arrow_start + np.array([light_dx, light_dy]) * 2.0
    ax.annotate(
        "",
        xy=(arrow_end[0], arrow_end[1]),
        xytext=(arrow_start[0], arrow_start[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
    )
    ax.text(
        arrow_start[0] + 0.8,
        arrow_start[1] + 0.3,
        "Light\ndirection",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        va="center",
    )

    # Faint light rays through the scene showing the light angle
    for x_anchor in [2.0, 6.0, 10.0]:
        ray_start = np.array([x_anchor, 6.5])
        ray_end = ray_start + np.array([light_dx, light_dy]) * 10.0
        ax.plot(
            [ray_start[0], ray_end[0]],
            [ray_start[1], ray_end[1]],
            ":",
            color=STYLE["warn"],
            linewidth=0.6,
            alpha=0.25,
        )

    # Near/far labels
    ax.text(
        near,
        -h_near_full - 0.7,
        "near",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )
    ax.text(
        far,
        -h_far_full - 0.7,
        "far",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    # View direction arrow along the bottom
    ax.annotate(
        "",
        xy=(8.5, -5.0),
        xytext=(4.0, -5.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.15",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )
    ax.text(
        6.25,
        -4.6,
        "Camera view direction",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    # Explanation note at bottom
    ax.text(
        6.25,
        -6.0,
        "Each rectangle is one cascade\u2019s orthographic projection \u2014 a box aligned to the light direction.\n"
        "The AABB is computed in light view space, so the boxes tilt with the light, not the camera.",
        color=STYLE["text_dim"],
        fontsize=7.5,
        ha="center",
        style="italic",
    )

    ax.set_title(
        "Cascade Orthographic Projections \u2014 Side View",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/15-cascaded-shadow-maps", "cascade_ortho_projections.png")


def diagram_pcf_kernel():
    """PCF (Percentage Closer Filtering) 3x3 kernel visualization.

    Shows a 3x3 grid of shadow map texels centered on the fragment's
    projected position.  Each cell shows its offset, whether it passes
    the depth test (lit) or fails (shadowed), and the final averaged
    shadow factor.  Illustrates how the [unroll] loop iterates over
    the 9 sample positions.
    """
    fig, axes = plt.subplots(
        1,
        2,
        figsize=(10, 5.0),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [1, 1], "wspace": 0.08},
    )

    ax_grid = axes[0]
    ax_result = axes[1]

    # --- Left panel: the 3x3 sample grid ---------------------------------
    ax_grid.set_facecolor(STYLE["bg"])
    ax_grid.set_xlim(-2.0, 2.0)
    ax_grid.set_ylim(-2.3, 2.3)
    ax_grid.set_aspect("equal")
    ax_grid.axis("off")

    # Depth test results for this example: an edge case where the shadow
    # boundary cuts diagonally across the kernel
    #   1 = lit (map_depth >= fragment_depth - bias)
    #   0 = shadowed
    results = [
        [1, 1, 0],  # top row    (y = -1)
        [1, 1, 0],  # middle row (y =  0)
        [0, 0, 0],  # bottom row (y = +1)
    ]

    lit_color = STYLE["accent3"]  # green
    shadow_color = STYLE["accent2"]  # orange
    cell_size = 1.1

    for row_idx, row in enumerate(results):
        for col_idx, lit in enumerate(row):
            # Offsets: x from -1 to +1, y from -1 to +1
            ox = col_idx - 1
            oy = row_idx - 1

            cx = ox * cell_size
            cy = -oy * cell_size  # flip so y=-1 is top

            color = lit_color if lit else shadow_color
            fill_alpha = 0.45 if lit else 0.30

            # Cell background
            rect = Rectangle(
                (cx - cell_size / 2, cy - cell_size / 2),
                cell_size,
                cell_size,
                facecolor=color,
                edgecolor=STYLE["text_dim"],
                alpha=fill_alpha,
                linewidth=1.2,
            )
            ax_grid.add_patch(rect)

            # Cell border (drawn separately for full opacity)
            border = Rectangle(
                (cx - cell_size / 2, cy - cell_size / 2),
                cell_size,
                cell_size,
                facecolor="none",
                edgecolor=STYLE["text_dim"],
                linewidth=1.0,
            )
            ax_grid.add_patch(border)

            # Offset label
            label = f"({ox:+d},{oy:+d})"
            ax_grid.text(
                cx,
                cy + 0.15,
                label,
                ha="center",
                va="center",
                fontsize=8,
                fontfamily="monospace",
                color=STYLE["text"],
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

            # Lit/shadow indicator
            indicator = "lit" if lit else "shadow"
            ax_grid.text(
                cx,
                cy - 0.22,
                indicator,
                ha="center",
                va="center",
                fontsize=7,
                fontfamily="monospace",
                color=color,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

    # Center crosshair marking the fragment's projected UV
    ax_grid.plot(
        0, 0, "+", color=STYLE["warn"], markersize=14, markeredgewidth=2.0, zorder=10
    )

    # Annotation for the center
    ax_grid.text(
        0,
        -2.05,
        "fragment\u2019s shadow UV",
        ha="center",
        va="center",
        fontsize=8,
        color=STYLE["warn"],
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax_grid.text(
        0,
        2.15,
        "3\u00d73 Sample Grid",
        ha="center",
        va="center",
        fontsize=12,
        fontweight="bold",
        color=STYLE["text"],
    )

    # --- Right panel: averaging result ------------------------------------
    ax_result.set_facecolor(STYLE["bg"])
    ax_result.set_xlim(-2.0, 2.0)
    ax_result.set_ylim(-2.3, 2.3)
    ax_result.set_aspect("equal")
    ax_result.axis("off")

    lit_count = sum(cell for row in results for cell in row)
    shadow_count = 9 - lit_count
    factor = lit_count / 9.0

    # Draw a bar/equation summary
    y_top = 1.5

    ax_result.text(
        0,
        y_top,
        "Depth test per sample:",
        ha="center",
        va="center",
        fontsize=10,
        color=STYLE["text"],
        fontweight="bold",
    )

    ax_result.text(
        0,
        y_top - 0.55,
        "map_depth \u2265 frag_depth \u2212 bias  \u2192  1 (lit)\n"
        "map_depth < frag_depth \u2212 bias  \u2192  0 (shadow)",
        ha="center",
        va="center",
        fontsize=8,
        fontfamily="monospace",
        color=STYLE["text_dim"],
        linespacing=1.6,
    )

    # Tally
    ax_result.text(
        0,
        y_top - 1.55,
        f"{lit_count} lit  +  {shadow_count} shadowed  =  9 samples",
        ha="center",
        va="center",
        fontsize=10,
        color=STYLE["text"],
    )

    # Result
    ax_result.text(
        0,
        y_top - 2.25,
        f"shadow_factor = {lit_count} / 9 = {factor:.3f}",
        ha="center",
        va="center",
        fontsize=13,
        fontfamily="monospace",
        fontweight="bold",
        color=STYLE["accent1"],
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Gradient bar showing the result visually
    bar_w = 2.8
    bar_h = 0.35
    bar_y = y_top - 3.1

    # Background bar (full shadow)
    bg_bar = Rectangle(
        (-bar_w / 2, bar_y - bar_h / 2),
        bar_w,
        bar_h,
        facecolor=shadow_color,
        alpha=0.3,
        edgecolor=STYLE["text_dim"],
        linewidth=1.0,
    )
    ax_result.add_patch(bg_bar)

    # Lit portion
    lit_bar = Rectangle(
        (-bar_w / 2, bar_y - bar_h / 2),
        bar_w * factor,
        bar_h,
        facecolor=lit_color,
        alpha=0.6,
        edgecolor="none",
    )
    ax_result.add_patch(lit_bar)

    # Border
    border_bar = Rectangle(
        (-bar_w / 2, bar_y - bar_h / 2),
        bar_w,
        bar_h,
        facecolor="none",
        edgecolor=STYLE["text_dim"],
        linewidth=1.0,
    )
    ax_result.add_patch(border_bar)

    ax_result.text(
        -bar_w / 2 - 0.1,
        bar_y,
        "0",
        ha="right",
        va="center",
        fontsize=8,
        color=STYLE["text_dim"],
    )
    ax_result.text(
        bar_w / 2 + 0.1,
        bar_y,
        "1",
        ha="left",
        va="center",
        fontsize=8,
        color=STYLE["text_dim"],
    )

    # Marker at the factor position
    marker_x = -bar_w / 2 + bar_w * factor
    ax_result.plot(
        marker_x,
        bar_y + bar_h / 2 + 0.08,
        "v",
        color=STYLE["accent1"],
        markersize=8,
    )

    ax_result.text(
        0,
        2.15,
        "Averaging",
        ha="center",
        va="center",
        fontsize=12,
        fontweight="bold",
        color=STYLE["text"],
    )

    # Overall title
    fig.suptitle(
        "PCF (Percentage Closer Filtering) \u2014 3\u00d73 Kernel",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.90))
    save(fig, "gpu/15-cascaded-shadow-maps", "pcf_kernel.png")


def diagram_peter_panning():
    """Peter panning: shadow detachment caused by excessive depth bias.

    Shows a side-view cross-section of an object on a surface, with light
    rays casting a shadow.  The left panel shows correct shadows (touching
    the object base), and the right panel shows peter panning where too
    much depth bias shifts the shadow map surface, creating a visible gap
    between the object and its shadow.
    """
    fig, (ax_good, ax_bad) = plt.subplots(
        1, 2, figsize=(13, 5.5), facecolor=STYLE["bg"]
    )

    for ax, title, show_bias in [
        (ax_good, "Correct shadow", False),
        (ax_bad, "Peter panning (too much bias)", True),
    ]:
        setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-1.5, 7.5), grid=False)
        ax.set_xticks([])
        ax.set_yticks([])

        # -- Ground plane --
        ground_y = 1.0
        ax.fill_between(
            [-0.5, 10.5],
            [-1.5, -1.5],
            [ground_y, ground_y],
            color=STYLE["surface"],
            alpha=0.6,
        )
        ax.plot(
            [-0.5, 10.5],
            [ground_y, ground_y],
            color=STYLE["axis"],
            linewidth=1.5,
        )
        ax.text(
            0.0,
            0.3,
            "Ground",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="left",
        )

        # -- Object (box) --
        box_left = 3.0
        box_right = 5.0
        box_top = 4.5
        box = Rectangle(
            (box_left, ground_y),
            box_right - box_left,
            box_top - ground_y,
            facecolor=STYLE["accent1"],
            edgecolor=STYLE["text"],
            linewidth=1.5,
            alpha=0.7,
            zorder=5,
        )
        ax.add_patch(box)
        ax.text(
            (box_left + box_right) / 2,
            (ground_y + box_top) / 2,
            "Object",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=6,
        )

        # -- Light rays (coming from upper-left) --
        light_angle = 0.6  # radians from vertical
        for lx_start in [2.0, 4.0, 6.0, 8.0]:
            ly_start = 7.5
            lx_end = lx_start + 3.0 * np.sin(light_angle)
            ly_end = ly_start - 3.0 * np.cos(light_angle)
            ax.annotate(
                "",
                xy=(lx_end, ly_end),
                xytext=(lx_start, ly_start),
                arrowprops={
                    "arrowstyle": "->,head_width=0.15,head_length=0.12",
                    "color": STYLE["warn"],
                    "lw": 1.2,
                    "alpha": 0.5,
                },
                zorder=1,
            )

        # -- Shadow on the ground --
        # The shadow starts where the light ray from the object's base hits ground
        # With bias, the effective shadow surface shifts, pushing the shadow start
        # further from the object
        shadow_start = box_right  # light from left, shadow on right side
        shadow_end = 8.5

        bias_offset = 1.3 if show_bias else 0.0
        actual_shadow_start = shadow_start + bias_offset

        # Shadow region
        ax.fill_between(
            [actual_shadow_start, shadow_end],
            [ground_y, ground_y],
            [ground_y - 0.01, ground_y - 0.01],
            color=STYLE["bg"],
            alpha=1.0,
            zorder=3,
        )
        ax.fill_between(
            [actual_shadow_start, shadow_end],
            [ground_y - 0.25, ground_y - 0.25],
            [ground_y, ground_y],
            color="#0a0a1a",
            alpha=0.85,
            zorder=3,
        )
        ax.text(
            (actual_shadow_start + shadow_end) / 2,
            ground_y - 0.65,
            "Shadow",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            zorder=4,
        )

        if show_bias:
            # -- Highlight the gap --
            gap_color = STYLE["accent2"]
            ax.fill_between(
                [shadow_start, actual_shadow_start],
                [ground_y - 0.25, ground_y - 0.25],
                [ground_y, ground_y],
                color=gap_color,
                alpha=0.35,
                hatch="//",
                zorder=3,
            )
            ax.annotate(
                "Gap!\nShadow\ndetached",
                xy=((shadow_start + actual_shadow_start) / 2, ground_y - 0.12),
                xytext=((shadow_start + actual_shadow_start) / 2, -0.8),
                color=gap_color,
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="top",
                arrowprops={
                    "arrowstyle": "->,head_width=0.2,head_length=0.15",
                    "color": gap_color,
                    "lw": 1.5,
                },
                zorder=7,
            )

            # -- Show the biased depth surface above the actual surface --
            bias_y = ground_y + 0.5
            ax.plot(
                [1.0, 9.5],
                [bias_y, bias_y],
                "--",
                color=STYLE["accent2"],
                linewidth=1.5,
                alpha=0.7,
                zorder=2,
            )
            ax.text(
                9.6,
                bias_y,
                "Biased\ndepth\nsurface",
                color=STYLE["accent2"],
                fontsize=7,
                fontweight="bold",
                va="center",
                ha="left",
                alpha=0.9,
            )

            # Bias offset arrow
            ax.annotate(
                "",
                xy=(1.5, ground_y),
                xytext=(1.5, bias_y),
                arrowprops={
                    "arrowstyle": "<->,head_width=0.15,head_length=0.1",
                    "color": STYLE["accent2"],
                    "lw": 1.2,
                },
                zorder=4,
            )
            ax.text(
                1.1,
                (ground_y + bias_y) / 2,
                "bias",
                color=STYLE["accent2"],
                fontsize=8,
                ha="right",
                va="center",
                fontstyle="italic",
            )
        else:
            # Correct case: shadow touches the object base
            ax.annotate(
                "Shadow meets\nobject base",
                xy=(shadow_start, ground_y - 0.12),
                xytext=(shadow_start + 0.3, -0.7),
                color=STYLE["accent3"],
                fontsize=8,
                fontweight="bold",
                ha="left",
                va="top",
                arrowprops={
                    "arrowstyle": "->,head_width=0.15,head_length=0.1",
                    "color": STYLE["accent3"],
                    "lw": 1.2,
                },
                zorder=7,
            )

        ax.set_title(
            title,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=10,
        )

    fig.suptitle(
        "Peter Panning \u2014 Shadow Detachment from Excessive Depth Bias",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/15-cascaded-shadow-maps", "peter_panning.png")
