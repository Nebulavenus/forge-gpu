"""Diagrams for gpu/23."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, Polygon, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — cube_face_layout.png
# ---------------------------------------------------------------------------


def diagram_cube_face_layout():
    """Cube map face layout for omnidirectional shadow mapping.

    Shows a point light at the center with six camera frustums pointing in
    the +X, -X, +Y, -Y, +Z, -Z directions.  Each face is labeled with its
    axis and colored distinctly.  A 2D top-down view (XZ plane) shows the
    four horizontal faces, and two inset arrows show +Y and -Y.
    """
    fig = plt.figure(figsize=(9, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-5.5, 5.5), ylim=(-5.5, 5.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Central light ---
    light = Circle(
        (0, 0),
        0.3,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=10,
    )
    ax.add_patch(light)
    ax.text(
        0,
        -0.7,
        "Point Light",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Face definitions: (label, direction_xy, color, label_pos) ---
    # Top-down view: X is right, Z is up (representing the XZ ground plane)
    face_len = 3.5
    face_half = 1.5  # half-width of the frustum at the far end

    faces = [
        ("+X", (1, 0), STYLE["accent1"], (4.8, 0)),
        ("\u2212X", (-1, 0), STYLE["accent2"], (-4.8, 0)),
        ("+Z", (0, 1), STYLE["accent3"], (0, 4.8)),
        ("\u2212Z", (0, -1), STYLE["accent4"], (0, -4.8)),
    ]

    for label, direction, color, label_pos in faces:
        dx, dy = direction
        # Frustum tip is at origin, far end is at face_len along direction
        far_center = np.array([dx * face_len, dy * face_len])

        # Perpendicular direction for frustum width
        perp = np.array([-dy, dx])
        far_left = far_center - perp * face_half
        far_right = far_center + perp * face_half

        # Draw frustum trapezoid
        trap = Polygon(
            [(0, 0), (far_left[0], far_left[1]), (far_right[0], far_right[1])],
            facecolor=color,
            alpha=0.15,
            edgecolor=color,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(trap)

        # Draw far edge
        ax.plot(
            [far_left[0], far_right[0]],
            [far_left[1], far_right[1]],
            color=color,
            linewidth=2,
            zorder=3,
        )

        # Direction arrow
        ax.annotate(
            "",
            xy=(dx * 2.5, dy * 2.5),
            xytext=(dx * 0.5, dy * 0.5),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.15",
                "color": color,
                "lw": 2.5,
            },
            zorder=8,
        )

        # Face label
        ax.text(
            label_pos[0],
            label_pos[1],
            label,
            color=color,
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- +Y and -Y indicators (up/down out of plane) ---
    # Show as circles with arrows since they point out of the 2D view
    for label, y_offset, color in [
        ("+Y \u2299", 3.0, STYLE["warn"]),  # out of screen (dot)
        ("\u2212Y \u2297", -3.0, STYLE["text_dim"]),  # into screen (cross)
    ]:
        ax.text(
            4.0,
            y_offset,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            bbox={
                "facecolor": STYLE["surface"],
                "edgecolor": color,
                "linewidth": 1.5,
                "boxstyle": "round,pad=0.3",
                "alpha": 0.9,
            },
            zorder=9,
        )

    # --- Annotations ---
    ax.text(
        -4.8,
        4.8,
        "90\u00b0 FOV per face\n6 faces = full sphere",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        -4.8,
        -4.3,
        "Top-down view (XZ plane)\nY-axis faces shown as insets",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="left",
        va="top",
        style="italic",
    )

    ax.set_title(
        "Cube Map Face Layout \u2014 6 Cameras Around a Point Light",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/23-point-light-shadows", "cube_face_layout.png")


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — linear_vs_hardware_depth.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — linear_vs_hardware_depth.png
# ---------------------------------------------------------------------------


def diagram_linear_vs_hardware_depth():
    """Comparison of linear depth vs hardware (z/w) depth distribution.

    Shows two plots: hardware depth (non-linear, precision near camera) and
    linear depth (uniform precision).  Illustrates why R32_FLOAT with
    distance/far_plane gives better shadow comparison results.
    """
    fig, (ax_hw, ax_lin) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
    )
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    near = 0.1
    far = 25.0
    distances = np.linspace(near, far, 500)

    # --- Hardware depth: z_ndc = (f * (d - n)) / (d * (f - n)) ---
    # For a [0,1] depth range (Vulkan/D3D12 convention)
    hw_depth = (far * (distances - near)) / (distances * (far - near))

    # --- Linear depth: d / far ---
    lin_depth = distances / far

    for ax, depth_values, title, color, description in [
        (
            ax_hw,
            hw_depth,
            "Hardware Depth (z/w)",
            STYLE["accent2"],
            "Precision concentrated near camera\n\u2192 coarse at distance",
        ),
        (
            ax_lin,
            lin_depth,
            "Linear Depth (distance / far)",
            STYLE["accent1"],
            "Uniform precision across range\n\u2192 consistent shadow tests",
        ),
    ]:
        setup_axes(ax, grid=True, aspect=None)
        ax.set_xlim(0, far)
        ax.set_ylim(-0.05, 1.1)
        ax.set_xlabel("World Distance", color=STYLE["axis"], fontsize=10)
        ax.set_ylabel("Stored Depth [0, 1]", color=STYLE["axis"], fontsize=10)

        ax.plot(distances, depth_values, color=color, linewidth=2.5, zorder=5)

        # Show equal-distance markers to visualize spacing
        marker_distances = np.array([1, 5, 10, 15, 20, 25])
        for d in marker_distances:
            if d <= far:
                idx = np.argmin(np.abs(distances - d))
                dv = depth_values[idx]
                ax.plot(d, dv, "o", color=color, markersize=5, zorder=6)
                ax.plot(
                    [d, d],
                    [0, dv],
                    "--",
                    color=STYLE["grid"],
                    linewidth=0.8,
                    alpha=0.6,
                    zorder=2,
                )

        ax.set_title(
            title,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=12,
        )

        ax.text(
            far * 0.55,
            0.15,
            description,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="bottom",
            path_effects=stroke,
        )

    fig.suptitle(
        "Linear Depth vs Hardware Depth \u2014 Why R32_FLOAT Stores distance / far",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/23-point-light-shadows", "linear_vs_hardware_depth.png")


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — shadow_lookup.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — shadow_lookup.png
# ---------------------------------------------------------------------------


def diagram_shadow_lookup():
    """Shadow cube map lookup: light-to-fragment direction samples the cube map.

    Shows a point light, a fragment point, and the direction vector between
    them.  The direction vector selects a cube map face and the stored depth
    is compared against the actual distance.
    """
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 11), ylim=(-2, 8), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Light source ---
    light_x, light_y = 1.5, 4.0
    light_circle = Circle(
        (light_x, light_y),
        0.25,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=10,
    )
    ax.add_patch(light_circle)
    ax.text(
        light_x,
        light_y + 0.6,
        "Light",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Occluder (box between light and fragment) ---
    occ_x, occ_y = 4.5, 2.5
    occ_w, occ_h = 1.2, 2.5
    occluder = Rectangle(
        (occ_x, occ_y),
        occ_w,
        occ_h,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.7,
        zorder=6,
    )
    ax.add_patch(occluder)
    ax.text(
        occ_x + occ_w / 2,
        occ_y + occ_h - 0.4,
        "Occluder",
        color=STYLE["text"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        zorder=7,
    )

    # --- Fragment point (in shadow) ---
    frag_x, frag_y = 8.5, 1.5
    ax.plot(frag_x, frag_y, "o", color=STYLE["accent2"], markersize=8, zorder=10)
    ax.text(
        frag_x + 0.4,
        frag_y - 0.4,
        "Fragment P",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=stroke,
    )

    # --- Direction vector from light to fragment ---
    dir_dx = frag_x - light_x
    dir_dy = frag_y - light_y
    ax.annotate(
        "",
        xy=(frag_x, frag_y),
        xytext=(light_x, light_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
            "linestyle": "dashed",
        },
        zorder=4,
    )
    mid_x = light_x + dir_dx * 0.18
    mid_y = light_y + dir_dy * 0.18 + 1.4
    ax.text(
        mid_x,
        mid_y,
        "light_to_frag",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Stored depth point (where the ray first hits the occluder) ---
    # Find intersection along the ray
    t_hit = (occ_x - light_x) / dir_dx  # left face of occluder
    hit_y = light_y + t_hit * dir_dy
    hit_x = occ_x

    ax.plot(hit_x, hit_y, "D", color=STYLE["warn"], markersize=8, zorder=10)
    ax.text(
        hit_x - 1.2,
        hit_y - 0.6,
        "stored\ndepth",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )
    # Leader line from label to diamond marker
    ax.plot(
        [hit_x - 1.2, hit_x],
        [hit_y - 0.5, hit_y],
        color=STYLE["warn"],
        lw=0.8,
        ls=":",
        zorder=5,
    )

    # --- Depth comparison annotations ---
    # Actual distance line
    actual_dist = np.sqrt(dir_dx**2 + dir_dy**2)
    stored_dist = np.sqrt((hit_x - light_x) ** 2 + (hit_y - light_y) ** 2)

    # Stored distance bracket
    ax.annotate(
        "",
        xy=(hit_x, -1.0),
        xytext=(light_x, -1.0),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
        zorder=5,
    )
    ax.text(
        (light_x + hit_x) / 2,
        -1.5,
        f"stored = {stored_dist / 25:.2f}",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Actual distance bracket
    ax.annotate(
        "",
        xy=(frag_x, -0.2),
        xytext=(light_x, -0.2),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
        zorder=5,
    )
    ax.text(
        (light_x + frag_x) / 2,
        -0.7,
        f"current = {actual_dist / 25:.2f}",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Result annotation ---
    ax.text(
        8.5,
        6.5,
        "current > stored\n\u2192 IN SHADOW",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        bbox={
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["accent2"],
            "linewidth": 2,
            "boxstyle": "round,pad=0.5",
            "alpha": 0.9,
        },
        zorder=11,
    )

    # --- Cube map face indicator ---
    ax.text(
        8.5,
        7.5,
        "TextureCube.Sample(smp, light_to_frag)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke,
    )

    ax.set_title(
        "Shadow Cube Map Lookup \u2014 Direction-Based Depth Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/23-point-light-shadows", "shadow_lookup.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — atmosphere_layers.png
# ---------------------------------------------------------------------------
