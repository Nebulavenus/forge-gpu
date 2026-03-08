"""Diagrams for gpu/31."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/31-transform-animations — keyframe_interpolation.png
# ---------------------------------------------------------------------------


def diagram_keyframe_interpolation():
    """Keyframe interpolation modes: STEP, LINEAR, and Catmull-Rom cubic."""
    fig = plt.figure(figsize=(10, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.1, 1.65), ylim=(-15, 195), grid=True, aspect=None)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Keyframe data
    kf_times = np.array([0.0, 0.3, 0.6, 1.0, 1.25])
    kf_values = np.array([0.0, 90.0, 45.0, 135.0, 180.0])

    # --- STEP interpolation ---
    step_x = []
    step_y = []
    for i in range(len(kf_times) - 1):
        step_x.extend([kf_times[i], kf_times[i + 1]])
        step_y.extend([kf_values[i], kf_values[i]])
    step_x.append(kf_times[-1])
    step_y.append(kf_values[-1])
    ax.plot(
        step_x,
        step_y,
        color=STYLE["accent1"],
        lw=2.0,
        label="STEP",
        alpha=0.85,
        zorder=3,
    )

    # --- LINEAR interpolation ---
    ax.plot(
        kf_times,
        kf_values,
        color=STYLE["accent2"],
        lw=2.0,
        label="LINEAR",
        alpha=0.85,
        zorder=3,
    )

    # --- CUBIC interpolation (Catmull-Rom spline, no scipy needed) ---
    def _catmull_rom_segment(p0, p1, p2, p3, t_arr):
        """Evaluate a Catmull-Rom segment for parameter values in [0, 1]."""
        return 0.5 * (
            (2.0 * p1)
            + (-p0 + p2) * t_arr
            + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t_arr**2
            + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t_arr**3
        )

    t_segments = []
    v_segments = []
    for seg_i in range(len(kf_times) - 1):
        # Clamp boundary control points
        i0 = max(seg_i - 1, 0)
        i3 = min(seg_i + 2, len(kf_values) - 1)
        p0, p1, p2, p3 = (
            kf_values[i0],
            kf_values[seg_i],
            kf_values[seg_i + 1],
            kf_values[i3],
        )
        n_pts = 60
        u = np.linspace(0, 1, n_pts, endpoint=(seg_i == len(kf_times) - 2))
        seg_t = kf_times[seg_i] + u * (kf_times[seg_i + 1] - kf_times[seg_i])
        seg_v = _catmull_rom_segment(p0, p1, p2, p3, u)
        t_segments.append(seg_t)
        v_segments.append(seg_v)

    t_smooth = np.concatenate(t_segments)
    v_smooth = np.concatenate(v_segments)

    ax.plot(
        t_smooth,
        v_smooth,
        color=STYLE["accent3"],
        lw=2.0,
        label="CUBIC (Catmull-Rom)",
        alpha=0.85,
        zorder=3,
    )

    # Keyframe dots
    for t_kf, v_kf in zip(kf_times, kf_values, strict=True):
        ax.plot(t_kf, v_kf, "o", color=STYLE["warn"], ms=8, zorder=5)

    # --- Binary search highlight ---
    query_t = 0.78
    # Bracketing keyframes via binary search
    idx_hi = int(np.searchsorted(kf_times, query_t, side="right"))
    idx_hi = min(max(idx_hi, 1), len(kf_times) - 1)
    idx_lo = idx_hi - 1
    t_lo, t_hi = kf_times[idx_lo], kf_times[idx_hi]
    v_lo, v_hi = kf_values[idx_lo], kf_values[idx_hi]

    # Vertical query line
    ax.axvline(query_t, color=STYLE["warn"], lw=1.5, ls="--", alpha=0.7, zorder=2)
    ax.text(
        query_t,
        190,
        f"t = {query_t}",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=6,
    )

    # Arrows pointing to bracketing keyframes
    arrow_kw = {
        "arrowstyle": "->,head_width=0.25,head_length=0.12",
        "color": STYLE["warn"],
        "lw": 1.5,
    }
    ax.annotate(
        "",
        xy=(t_lo, v_lo - 6),
        xytext=(query_t, v_lo - 6),
        arrowprops=arrow_kw,
        zorder=5,
    )
    ax.annotate(
        "",
        xy=(t_hi, v_hi + 6),
        xytext=(query_t, v_hi + 6),
        arrowprops=arrow_kw,
        zorder=5,
    )

    # Alpha annotation between the two bracketing keyframes
    alpha_val = (query_t - t_lo) / (t_hi - t_lo)
    ax.text(
        (t_lo + t_hi) / 2,
        (v_lo + v_hi) / 2 + 18,
        f"\u03b1 = (t \u2212 t\u2080) / (t\u2081 \u2212 t\u2080) = {alpha_val:.2f}",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=6,
    )

    # Bracket labels
    ax.text(
        t_lo,
        v_lo - 16,
        f"t\u2080 = {t_lo}",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=6,
    )
    ax.text(
        t_hi,
        v_hi + 16,
        f"t\u2081 = {t_hi}",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
        path_effects=stroke_thin,
        zorder=6,
    )

    # Binary search annotation box
    ax.text(
        1.42,
        12,
        "Binary search\nfinds [t\u2080, t\u2081]",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
        bbox={
            "boxstyle": "round,pad=0.3",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["warn"],
            "alpha": 0.9,
        },
    )

    ax.set_xlabel("Time (s)", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Rotation angle (\u00b0)", color=STYLE["text"], fontsize=11)
    ax.legend(
        loc="upper left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    fig.suptitle(
        "Keyframe Interpolation \u2014 STEP, LINEAR, and CUBIC Modes",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/31-transform-animations", "keyframe_interpolation.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — quaternion_slerp.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — quaternion_slerp.png
# ---------------------------------------------------------------------------


def diagram_quaternion_slerp():
    """SLERP vs NLERP on a unit circle with interpolated positions."""
    fig = plt.figure(figsize=(8, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.55, 1.55), ylim=(-1.55, 1.55))

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # --- Unit circle ---
    theta_full = np.linspace(0, 2 * np.pi, 300)
    ax.plot(
        np.cos(theta_full),
        np.sin(theta_full),
        color=STYLE["grid"],
        lw=1.5,
        zorder=1,
    )

    # Quaternion endpoints as angles on the circle
    angle_q0 = np.radians(20)
    angle_q1 = np.radians(110)
    q0 = np.array([np.cos(angle_q0), np.sin(angle_q0)])
    q1 = np.array([np.cos(angle_q1), np.sin(angle_q1)])

    # --- SLERP arc (along the circle) ---
    theta_arc = np.linspace(angle_q0, angle_q1, 200)
    ax.plot(
        np.cos(theta_arc),
        np.sin(theta_arc),
        color=STYLE["accent1"],
        lw=3,
        label="SLERP (great arc)",
        zorder=3,
    )

    # --- NLERP chord (straight line through interior, then normalize) ---
    ax.plot(
        [q0[0], q1[0]],
        [q0[1], q1[1]],
        color=STYLE["accent2"],
        lw=2,
        ls="--",
        label="NLERP (chord)",
        zorder=3,
    )

    # --- Interpolated positions on the slerp arc ---
    total_angle = angle_q1 - angle_q0
    t_values = [0.0, 0.25, 0.5, 0.75, 1.0]
    for t_val in t_values:
        angle_t = angle_q0 + t_val * total_angle
        px = np.cos(angle_t)
        py = np.sin(angle_t)
        ax.plot(px, py, "o", color=STYLE["accent1"], ms=8, zorder=5)
        # Skip endpoint labels — q₀ and q₁ already mark those positions
        if t_val in (0.0, 1.0):
            continue
        offset_x = 0.14 * np.cos(angle_t)
        offset_y = 0.14 * np.sin(angle_t)
        ax.text(
            px + offset_x,
            py + offset_y,
            f"t={t_val}",
            color=STYLE["text"],
            fontsize=8,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=6,
        )

    # --- q0 and q1 labels ---
    ax.plot(q0[0], q0[1], "o", color=STYLE["accent3"], ms=12, zorder=6)
    ax.text(
        q0[0] + 0.12,
        q0[1] - 0.12,
        "q\u2080",
        color=STYLE["accent3"],
        fontsize=14,
        fontweight="bold",
        ha="left",
        va="top",
        path_effects=stroke,
        zorder=7,
    )
    ax.plot(q1[0], q1[1], "o", color=STYLE["accent4"], ms=12, zorder=6)
    ax.text(
        q1[0] - 0.08,
        q1[1] + 0.12,
        "q\u2081",
        color=STYLE["accent4"],
        fontsize=14,
        fontweight="bold",
        ha="right",
        va="bottom",
        path_effects=stroke,
        zorder=7,
    )

    # --- Radius lines from origin ---
    ax.plot([0, q0[0]], [0, q0[1]], color=STYLE["accent3"], lw=1, ls=":", zorder=2)
    ax.plot([0, q1[0]], [0, q1[1]], color=STYLE["accent4"], lw=1, ls=":", zorder=2)

    # --- Angle arc and theta label ---
    angle_arc_r = 0.3
    theta_label = np.linspace(angle_q0, angle_q1, 60)
    ax.plot(
        angle_arc_r * np.cos(theta_label),
        angle_arc_r * np.sin(theta_label),
        color=STYLE["warn"],
        lw=1.5,
        zorder=4,
    )
    mid_angle = (angle_q0 + angle_q1) / 2
    ax.text(
        0.42 * np.cos(mid_angle),
        0.42 * np.sin(mid_angle),
        "\u03b8",
        color=STYLE["warn"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # --- Formula annotation ---
    formula = (
        "slerp(q\u2080, q\u2081, t) = "
        "q\u2080 \u00b7 sin((1\u2212t)\u03b8) / sin\u03b8  +  "
        "q\u2081 \u00b7 sin(t\u03b8) / sin\u03b8"
    )
    ax.text(
        0.0,
        -1.35,
        formula,
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=6,
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["grid"],
            "alpha": 0.9,
        },
    )

    ax.legend(
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    fig.suptitle(
        "Quaternion SLERP vs NLERP on a Unit Circle",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/31-transform-animations", "quaternion_slerp.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — animation_clip_structure.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — animation_clip_structure.png
# ---------------------------------------------------------------------------


def diagram_animation_clip_structure():
    """glTF animation data layout: clips, channels, samplers, accessors."""
    from matplotlib.patches import FancyArrowPatch

    fig = plt.figure(figsize=(12, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(-6.5, 4.5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # --- Timeline bar ---
    timeline_y = 3.5
    bar_left, bar_right = 1.0, 10.0
    ax.plot(
        [bar_left, bar_right],
        [timeline_y, timeline_y],
        color=STYLE["accent1"],
        lw=4,
        solid_capstyle="round",
        zorder=3,
    )
    ax.text(
        (bar_left + bar_right) / 2,
        timeline_y + 0.5,
        'Animation Clip: "Wheels"',
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )
    # Time labels
    duration = 1.25
    n_ticks = 6
    for i in range(n_ticks):
        t = i * duration / (n_ticks - 1)
        x = bar_left + (bar_right - bar_left) * (t / duration)
        ax.plot(x, timeline_y, "|", color=STYLE["text"], ms=10, mew=2, zorder=4)
        ax.text(
            x,
            timeline_y - 0.35,
            f"{t:.2f}s",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
        )

    # --- Channel tracks ---
    channels = [
        ("Channel 0: Node 0 (Front Wheels) \u2014 rotation", 2.0, STYLE["accent2"]),
        ("Channel 1: Node 2 (Rear Wheels) \u2014 rotation", 1.0, STYLE["accent3"]),
    ]
    kf_positions = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25]

    for label, track_y, color in channels:
        ax.text(
            0.8,
            track_y,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="right",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        ax.plot(
            [bar_left, bar_right],
            [track_y, track_y],
            color=color,
            lw=1.5,
            alpha=0.4,
            zorder=2,
        )
        for kf_t in kf_positions:
            x = bar_left + (bar_right - bar_left) * (kf_t / duration)
            ax.plot(x, track_y, "o", color=color, ms=7, zorder=4)

    # --- Data flow diagram ---
    box_h = 0.7
    flow_y = -2.0
    boxes = [
        ("Animation", 0.5, 2.0, STYLE["text"]),
        ("Channels", 3.0, 1.8, STYLE["text"]),
        ("Samplers", 5.3, 1.8, STYLE["text"]),
        ("Accessors", 7.6, 1.8, STYLE["text"]),
        ("BufferView", 9.9, 1.9, STYLE["text_dim"]),
    ]

    box_patches = []
    for label, bx, bw, text_color in boxes:
        patch = FancyBboxPatch(
            (bx, flow_y - box_h / 2),
            bw,
            box_h,
            boxstyle="round,pad=0.12",
            facecolor=STYLE["surface"],
            edgecolor=STYLE["accent1"],
            lw=1.5,
            zorder=3,
        )
        ax.add_patch(patch)
        ax.text(
            bx + bw / 2,
            flow_y,
            label,
            color=text_color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        box_patches.append((bx, bw))

    # Arrows between boxes
    for i in range(len(box_patches) - 1):
        x_start = box_patches[i][0] + box_patches[i][1]
        x_end = box_patches[i + 1][0]
        arrow = FancyArrowPatch(
            (x_start + 0.05, flow_y),
            (x_end - 0.05, flow_y),
            arrowstyle="->,head_width=0.15,head_length=0.1",
            color=STYLE["accent1"],
            lw=1.5,
            zorder=4,
        )
        ax.add_patch(arrow)

    # --- Accessor type annotations ---
    acc_y = flow_y - 1.2
    ax.text(
        8.5,
        acc_y,
        "timestamps (float)",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )
    ax.text(
        8.5,
        acc_y - 0.5,
        "quaternions (vec4)",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Color-coded dots beside accessor labels
    ax.plot(7.6, acc_y, "s", color=STYLE["accent1"], ms=6, zorder=5)
    ax.plot(7.6, acc_y - 0.5, "s", color=STYLE["accent2"], ms=6, zorder=5)

    # --- Bottom buffer bar ---
    buf_y = -5.0
    buf_w = 10.0
    buf_patch = FancyBboxPatch(
        (1.0, buf_y - 0.35),
        buf_w,
        0.7,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        lw=1,
        zorder=3,
    )
    ax.add_patch(buf_patch)
    ax.text(
        1.0 + buf_w / 2,
        buf_y,
        "Binary Buffer (.bin)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Arrow from BufferView to Buffer
    arrow_buf = FancyArrowPatch(
        (10.85, flow_y - box_h / 2 - 0.05),
        (6.0, buf_y + 0.4),
        arrowstyle="->,head_width=0.15,head_length=0.1",
        color=STYLE["text_dim"],
        lw=1.2,
        connectionstyle="arc3,rad=-0.2",
        zorder=4,
    )
    ax.add_patch(arrow_buf)

    fig.suptitle(
        "glTF Animation Clip Structure \u2014 Data Flow",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/31-transform-animations", "animation_clip_structure.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — transform_hierarchy.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — transform_hierarchy.png
# ---------------------------------------------------------------------------


def diagram_transform_hierarchy():
    """CesiumMilkTruck node tree with animated wheel nodes."""
    from matplotlib.patches import FancyArrowPatch

    fig = plt.figure(figsize=(10, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 11), ylim=(-1.5, 9.5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Node definitions: (label, detail, x, y, is_animated)
    nodes = {
        "yup2zup": ("Node 5: Yup2Zup", "R: 90\u00b0 X-axis", 5.0, 8.0, False),
        "truck": ("Node 4: Cesium_Milk_Truck", "mesh (3 primitives)", 5.0, 6.2, False),
        "front_axle": ("Node 1: Front Axle", "T: (1.0, 0.37, 0.0)", 2.5, 4.4, False),
        "front_wheels": ("Node 0: Wheels", "ANIMATED", 2.5, 2.6, True),
        "rear_axle": (
            "Node 3: Rear Axle",
            "T: (\u22121.0, 0.37, 0.0)",
            7.5,
            4.4,
            False,
        ),
        "rear_wheels": ("Node 2: Wheels.001", "ANIMATED", 7.5, 2.6, True),
    }

    # Draw nodes as rounded boxes
    node_positions = {}
    box_w, box_h = 3.2, 1.0
    for key, (label, detail, nx, ny, animated) in nodes.items():
        edge_color = STYLE["accent2"] if animated else STYLE["accent1"]
        lw = 2.5 if animated else 1.5
        patch = FancyBboxPatch(
            (nx - box_w / 2, ny - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=edge_color,
            lw=lw,
            zorder=3,
        )
        ax.add_patch(patch)
        ax.text(
            nx,
            ny + 0.15,
            label,
            color=STYLE["text"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        detail_color = STYLE["accent2"] if animated else STYLE["text_dim"]
        ax.text(
            nx,
            ny - 0.25,
            detail,
            color=detail_color,
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        node_positions[key] = (nx, ny)

    # Draw edges (parent -> child)
    edges = [
        ("yup2zup", "truck"),
        ("truck", "front_axle"),
        ("truck", "rear_axle"),
        ("front_axle", "front_wheels"),
        ("rear_axle", "rear_wheels"),
    ]
    for parent, child in edges:
        px, py = node_positions[parent]
        cx, cy = node_positions[child]
        arrow = FancyArrowPatch(
            (px, py - box_h / 2),
            (cx, cy + box_h / 2),
            arrowstyle="->,head_width=0.15,head_length=0.1",
            color=STYLE["grid"],
            lw=1.5,
            connectionstyle="arc3,rad=0",
            zorder=2,
        )
        ax.add_patch(arrow)

    # --- Input arrows ---
    # Path animation -> root
    ax.annotate(
        "",
        xy=(5.0 - box_w / 2, 8.0),
        xytext=(-0.5, 8.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 2,
        },
        zorder=4,
    )
    ax.text(
        -0.7,
        8.0,
        "Path Animation\n\u2192 Root Transform",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # glTF keyframes -> wheel nodes
    for wheel_key in ["front_wheels", "rear_wheels"]:
        wx, wy = node_positions[wheel_key]
        ax.annotate(
            "",
            xy=(wx, wy - box_h / 2),
            xytext=(wx, wy - box_h / 2 - 0.8),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.1",
                "color": STYLE["accent2"],
                "lw": 1.8,
            },
            zorder=4,
        )

    ax.text(
        5.0,
        1.3,
        "glTF Keyframes \u2192 Wheel Rotation",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # --- Transform composition formula ---
    ax.text(
        5.0,
        0.2,
        "World = Parent \u00d7 T \u00d7 R \u00d7 S",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
        bbox={
            "boxstyle": "round,pad=0.35",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["warn"],
            "alpha": 0.9,
        },
    )

    fig.suptitle(
        "Transform Hierarchy \u2014 CesiumMilkTruck Scene Graph",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/31-transform-animations", "transform_hierarchy.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — path_following.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — path_following.png
# ---------------------------------------------------------------------------


def diagram_path_following():
    """Top-down view of the truck track with waypoints and forward vector."""
    fig = plt.figure(figsize=(10, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-6.5, 6.5), ylim=(-5.0, 5.0))

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Ellipse parameters
    a, b = 5.0, 3.5  # semi-major, semi-minor

    # --- Track outline (two concentric ellipses for "road" look) ---
    theta_full = np.linspace(0, 2 * np.pi, 400)
    road_half_width = 0.35
    for sign, alpha_val in [(1, 0.3), (-1, 0.3)]:
        r_offset = sign * road_half_width
        # approximate offset ellipse
        x_off = (a + r_offset) * np.cos(theta_full)
        z_off = (b + r_offset) * np.sin(theta_full)
        ax.plot(
            x_off, z_off, color=STYLE["text_dim"], lw=0.8, alpha=alpha_val, zorder=1
        )

    # Main ellipse path
    ex = a * np.cos(theta_full)
    ez = b * np.sin(theta_full)
    ax.plot(ex, ez, color=STYLE["accent1"], lw=2.5, zorder=2, label="Path (ellipse)")

    # --- Waypoints ---
    n_waypoints = 16
    wp_angles = np.linspace(0, 2 * np.pi, n_waypoints, endpoint=False)
    wp_x = a * np.cos(wp_angles)
    wp_z = b * np.sin(wp_angles)

    for i, (wx, wz) in enumerate(zip(wp_x, wp_z, strict=True)):
        ax.plot(wx, wz, "o", color=STYLE["text_dim"], ms=5, zorder=4)
        # Label every 4th waypoint
        if i % 4 == 0:
            offset_angle = wp_angles[i]
            ax.text(
                wx + 0.4 * np.cos(offset_angle),
                wz + 0.4 * np.sin(offset_angle),
                str(i),
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    # --- Truck position (between waypoints 3 and 4) ---
    idx_lo, idx_hi = 3, 4
    alpha_t = 0.6
    truck_x = wp_x[idx_lo] + alpha_t * (wp_x[idx_hi] - wp_x[idx_lo])
    truck_z = wp_z[idx_lo] + alpha_t * (wp_z[idx_hi] - wp_z[idx_lo])

    # Forward direction: tangent to ellipse at this point
    truck_angle = wp_angles[idx_lo] + alpha_t * (wp_angles[idx_hi] - wp_angles[idx_lo])
    # Tangent vector to ellipse: (-a*sin(t), b*cos(t)), normalized
    tx = -a * np.sin(truck_angle)
    tz = b * np.cos(truck_angle)
    t_len = np.sqrt(tx**2 + tz**2)
    tx /= t_len
    tz /= t_len

    # Draw truck as a small rotated rectangle
    truck_w, truck_h = 0.8, 0.4
    angle_deg = np.degrees(np.arctan2(tz, tx))
    from matplotlib.transforms import Affine2D

    truck_rect = Rectangle(
        (-truck_w / 2, -truck_h / 2),
        truck_w,
        truck_h,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["text"],
        lw=1.5,
        zorder=6,
    )
    transform = (
        Affine2D().rotate_deg(angle_deg).translate(truck_x, truck_z) + ax.transData
    )
    truck_rect.set_transform(transform)
    ax.add_patch(truck_rect)

    # Forward direction arrow
    arrow_len = 1.2
    ax.annotate(
        "",
        xy=(truck_x + arrow_len * tx, truck_z + arrow_len * tz),
        xytext=(truck_x, truck_z),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
        zorder=7,
    )
    ax.text(
        truck_x + (arrow_len + 0.3) * tx,
        truck_z + (arrow_len + 0.3) * tz,
        "forward",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=8,
    )

    # Highlight bracketing waypoints
    for idx, lbl in [(idx_lo, f"wp[{idx_lo}]"), (idx_hi, f"wp[{idx_hi}]")]:
        ax.plot(wp_x[idx], wp_z[idx], "o", color=STYLE["warn"], ms=10, zorder=5)
        offset_angle = wp_angles[idx]
        ax.text(
            wp_x[idx] + 0.5 * np.cos(offset_angle),
            wp_z[idx] + 0.5 * np.sin(offset_angle),
            lbl,
            color=STYLE["warn"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=6,
        )

    # Alpha annotation
    mid_x = (wp_x[idx_lo] + truck_x) / 2
    mid_z = (wp_z[idx_lo] + truck_z) / 2
    ax.text(
        mid_x + 0.5,
        mid_z + 0.6,
        f"\u03b1 = {alpha_t}",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=8,
    )

    # --- Labels for interpolation methods ---
    ax.text(
        -5.5,
        -4.2,
        "Position: lerp(wp[i], wp[i+1], \u03b1)",
        color=STYLE["accent1"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )
    ax.text(
        -5.5,
        -4.6,
        "Orientation: slerp(q[i], q[i+1], \u03b1)",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax.set_xlabel("X", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Z", color=STYLE["text"], fontsize=11)

    fig.suptitle(
        "Path Following \u2014 Top-Down View of Elliptical Track",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/31-transform-animations", "path_following.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — animation_timeline.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — animation_timeline.png
# ---------------------------------------------------------------------------


def diagram_animation_timeline():
    """Stacked timelines showing path and wheel animation looping independently."""
    fig = plt.figure(figsize=(12, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 28), ylim=(-2.5, 5.5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    bar_h = 0.6
    current_t = 5.0

    # --- Path animation bar (top) ---
    path_y = 3.5
    lap_dur = 25.0
    path_bar_end = lap_dur

    # Background bar
    path_bg = FancyBboxPatch(
        (0, path_y - bar_h / 2),
        path_bar_end,
        bar_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        lw=1.5,
        zorder=2,
    )
    ax.add_patch(path_bg)

    # Progress fill
    path_wrapped = current_t % lap_dur
    path_fill = FancyBboxPatch(
        (0, path_y - bar_h / 2),
        path_wrapped,
        bar_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent3"],
        edgecolor="none",
        alpha=0.3,
        zorder=3,
    )
    ax.add_patch(path_fill)

    ax.text(
        -0.5,
        path_y,
        "Path\nAnimation",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Duration label
    ax.text(
        lap_dur / 2,
        path_y + 0.8,
        f"duration = {lap_dur:.0f}s (one lap)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Loop arrow at end
    ax.annotate(
        "",
        xy=(0.3, path_y + 0.5),
        xytext=(lap_dur - 0.3, path_y + 0.5),
        arrowprops={
            "arrowstyle": "<->,head_width=0.15,head_length=0.1",
            "color": STYLE["text_dim"],
            "lw": 1,
            "connectionstyle": "arc3,rad=0.15",
        },
        zorder=4,
    )

    # Tick marks at endpoints
    for t_mark, lbl in [(0, "0s"), (lap_dur, f"{lap_dur:.0f}s")]:
        ax.plot(
            t_mark,
            path_y - bar_h / 2 - 0.15,
            "|",
            color=STYLE["text_dim"],
            ms=8,
            mew=1,
            zorder=4,
        )
        ax.text(
            t_mark,
            path_y - bar_h / 2 - 0.4,
            lbl,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
        )

    # --- Wheel animation bar (bottom, repeating) ---
    wheel_y = 1.5
    wheel_dur = 1.25
    n_repeats = int(np.ceil(lap_dur / wheel_dur))

    for i in range(n_repeats):
        x_start = i * wheel_dur
        if x_start >= lap_dur:
            break
        w = min(wheel_dur, lap_dur - x_start)
        color = STYLE["accent2"] if i % 2 == 0 else STYLE["accent4"]
        bar = FancyBboxPatch(
            (x_start, wheel_y - bar_h / 2),
            w,
            bar_h,
            boxstyle="round,pad=0.02",
            facecolor=color,
            edgecolor=STYLE["surface"],
            alpha=0.35,
            lw=0.5,
            zorder=2,
        )
        ax.add_patch(bar)

    # Outline
    wheel_bg = FancyBboxPatch(
        (0, wheel_y - bar_h / 2),
        lap_dur,
        bar_h,
        boxstyle="round,pad=0.05",
        facecolor="none",
        edgecolor=STYLE["accent2"],
        lw=1.5,
        zorder=3,
    )
    ax.add_patch(wheel_bg)

    ax.text(
        -0.5,
        wheel_y,
        "Wheel\nAnimation",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Duration label
    ax.text(
        wheel_dur / 2,
        wheel_y + 0.7,
        f"{wheel_dur}s",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Current time vertical line ---
    ax.plot(
        [current_t, current_t],
        [wheel_y - 1.0, path_y + 1.2],
        color=STYLE["warn"],
        lw=2,
        ls="--",
        zorder=6,
    )
    ax.text(
        current_t,
        path_y + 1.5,
        f"t = {current_t:.1f}s",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=7,
    )

    # --- Wrapping formulas ---
    t_wheel_wrapped = current_t % wheel_dur
    t_path_wrapped = current_t % lap_dur

    formula_x = 14.0
    formula_y = -0.5
    formulas = [
        (
            f"t_path  = fmod({current_t:.1f}, {lap_dur:.0f}) = {t_path_wrapped:.1f}s",
            STYLE["accent3"],
        ),
        (
            f"t_wheel = fmod({current_t:.1f}, {wheel_dur}) = {t_wheel_wrapped:.2f}s",
            STYLE["accent2"],
        ),
    ]
    for i, (text, color) in enumerate(formulas):
        ax.text(
            formula_x,
            formula_y - i * 0.55,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
            fontfamily="monospace",
        )

    # --- "Both run simultaneously" annotation ---
    ax.text(
        14.0,
        -1.8,
        "Both animations loop independently at their own rates",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.suptitle(
        "Animation Timeline \u2014 Path vs Wheel Looping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/31-transform-animations", "animation_timeline.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — forward_driven_movement.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — forward_driven_movement.png
# ---------------------------------------------------------------------------


def diagram_forward_driven_movement():
    """Side-by-side: position interpolation cuts corners vs forward-driven follows the road."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # -- Road geometry: L-shaped 90-degree right turn -----------------------
    road_cx, road_cy = 2.0, 2.0  # road centerline corner
    road_hw = 0.8  # half-width

    # Waypoints on the road centerline
    wp_a = np.array([-2.0, road_cy])  # heading east
    wp_b = np.array([road_cx, -2.0])  # heading south
    yaw_a = 0.0  # east
    yaw_b = -np.pi / 2  # south

    def draw_road_and_waypoints(ax):
        """Draw the L-shaped road and waypoint markers."""
        # Horizontal arm
        rect_h = Rectangle(
            (-4.5, road_cy - road_hw),
            4.5 + road_cx + road_hw,
            2 * road_hw,
            facecolor=STYLE["surface"],
            alpha=0.4,
            zorder=0,
        )
        ax.add_patch(rect_h)
        # Vertical arm
        rect_v = Rectangle(
            (road_cx - road_hw, -4.5),
            2 * road_hw,
            4.5 + road_cy + road_hw,
            facecolor=STYLE["surface"],
            alpha=0.4,
            zorder=0,
        )
        ax.add_patch(rect_v)
        # Road edge lines
        edges = [
            ([-4.5, road_cx - road_hw], [road_cy + road_hw, road_cy + road_hw]),
            ([-4.5, road_cx - road_hw], [road_cy - road_hw, road_cy - road_hw]),
            ([road_cx + road_hw, road_cx + road_hw], [-4.5, road_cy - road_hw]),
            ([road_cx - road_hw, road_cx - road_hw], [-4.5, road_cy - road_hw]),
            (
                [road_cx + road_hw, road_cx + road_hw],
                [road_cy + road_hw, road_cy + road_hw],
            ),
            (
                [road_cx - road_hw, road_cx + road_hw],
                [road_cy + road_hw, road_cy + road_hw],
            ),
        ]
        for ex, ey in edges:
            ax.plot(ex, ey, color=STYLE["grid"], lw=1, alpha=0.7, zorder=1)
        # Waypoint markers
        for wp, label in [(wp_a, "A"), (wp_b, "B")]:
            ax.plot(*wp, "o", color=STYLE["warn"], ms=8, zorder=6)
            lx = -0.5 if wp[0] < 0 else 0.5
            ly = 0.5 if wp[1] > 0 else -0.5
            ax.text(
                wp[0] + lx,
                wp[1] + ly,
                label,
                color=STYLE["warn"],
                fontsize=11,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=7,
            )

    def draw_truck(ax, pos, yaw, color):
        """Draw the truck as a small triangle pointing in the yaw direction."""
        size = 0.45
        tri = np.array(
            [
                [size, 0],
                [-size * 0.5, size * 0.35],
                [-size * 0.5, -size * 0.35],
            ]
        )
        c, s = np.cos(yaw), np.sin(yaw)
        rot = np.array([[c, -s], [s, c]])
        tri_rot = tri @ rot.T + pos
        triangle = Polygon(
            tri_rot,
            closed=True,
            facecolor=color,
            edgecolor=STYLE["text"],
            lw=1.5,
            zorder=8,
        )
        ax.add_patch(triangle)

    # -- Left panel: Position Interpolation ----------------------------------
    setup_axes(ax1, xlim=(-4.5, 4.5), ylim=(-4.5, 4.5))
    draw_road_and_waypoints(ax1)

    # Dashed interpolation line A → B
    ax1.plot(
        [wp_a[0], wp_b[0]],
        [wp_a[1], wp_b[1]],
        ls="--",
        color=STYLE["accent1"],
        lw=2,
        zorder=3,
    )

    # Truck at t=0.35 along the interpolation line
    t_lerp = 0.35
    interp_pos = wp_a + t_lerp * (wp_b - wp_a)
    interp_yaw = yaw_a + t_lerp * (yaw_b - yaw_a)
    draw_truck(ax1, interp_pos, interp_yaw, STYLE["accent1"])

    # Movement direction (along A→B line)
    move_dir = wp_b - wp_a
    move_dir = move_dir / np.linalg.norm(move_dir)

    # Heading direction (where the truck faces)
    head_dir = np.array([np.cos(interp_yaw), np.sin(interp_yaw)])

    # Lateral drift = component of movement perpendicular to heading
    lateral = move_dir - np.dot(move_dir, head_dir) * head_dir
    drift_scale = 2.0

    ax1.annotate(
        "",
        xy=(
            interp_pos[0] + lateral[0] * drift_scale,
            interp_pos[1] + lateral[1] * drift_scale,
        ),
        xytext=interp_pos,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent2"],
            "lw": 2.5,
            "ls": "--",
        },
        zorder=9,
    )
    ax1.text(
        interp_pos[0] + lateral[0] * drift_scale * 1.1 - 0.7,
        interp_pos[1] + lateral[1] * drift_scale * 1.1 + 0.1,
        "lateral\ndrift",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # Heading arrow
    ax1.annotate(
        "",
        xy=(interp_pos[0] + head_dir[0] * 1.3, interp_pos[1] + head_dir[1] * 1.3),
        xytext=interp_pos,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 2,
        },
        zorder=9,
    )
    ax1.text(
        interp_pos[0] + head_dir[0] * 1.7,
        interp_pos[1] + head_dir[1] * 1.7 + 0.3,
        "heading",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=10,
    )

    ax1.text(
        -2.5,
        -3.5,
        "lerp(A, B, t) cuts\nthrough the corner",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax1.set_title(
        "Position Interpolation",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # -- Right panel: Forward-Driven Movement --------------------------------
    setup_axes(ax2, xlim=(-4.5, 4.5), ylim=(-4.5, 4.5))
    draw_road_and_waypoints(ax2)

    # Simulate forward-driven path
    n_steps = 200
    total_dist = np.linalg.norm(wp_b - wp_a)
    step_dist = total_dist / n_steps
    fwd_path = [wp_a.copy()]
    pos = wp_a.copy()

    for i in range(1, n_steps + 1):
        t = i / n_steps
        yaw = yaw_a + t * (yaw_b - yaw_a)
        hd = np.array([np.cos(yaw), np.sin(yaw)])
        pos = pos + hd * step_dist
        fwd_path.append(pos.copy())

    fwd_path = np.array(fwd_path)
    ax2.plot(
        fwd_path[:, 0],
        fwd_path[:, 1],
        color=STYLE["accent3"],
        lw=2.5,
        zorder=3,
    )

    # Truck at similar progress along the forward-driven path
    t_idx = int(0.35 * n_steps)
    truck_pos = fwd_path[t_idx]
    truck_yaw = yaw_a + 0.35 * (yaw_b - yaw_a)
    draw_truck(ax2, truck_pos, truck_yaw, STYLE["accent3"])

    # Forward arrow (movement = heading)
    fwd_dir = np.array([np.cos(truck_yaw), np.sin(truck_yaw)])
    ax2.annotate(
        "",
        xy=(truck_pos[0] + fwd_dir[0] * 1.3, truck_pos[1] + fwd_dir[1] * 1.3),
        xytext=truck_pos,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
        zorder=9,
    )
    ax2.text(
        truck_pos[0] + fwd_dir[0] * 1.8,
        truck_pos[1] + fwd_dir[1] * 1.8 + 0.3,
        "forward",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    ax2.text(
        -2.5,
        -3.5,
        "Always moves in\nthe heading direction",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax2.set_title(
        "Forward-Driven Movement",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "gpu/31-transform-animations", "forward_driven_movement.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — arc_length_parameterization.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — arc_length_parameterization.png
# ---------------------------------------------------------------------------


def diagram_arc_length_parameterization():
    """Side-by-side: uniform parameterization vs arc-length parameterization.

    Shows how uniform parameter spacing misaligns yaw changes with actual
    position, while arc-length spacing keeps them synchronized.
    """
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Simplified path: 3 segments of different lengths
    # Short corner arc → Long straight → Short corner arc
    waypoints = np.array(
        [
            [-3.5, 3.0],  # A (start of corner)
            [-1.0, 3.5],  # B (end of corner, start of long straight)
            [3.5, 3.5],  # C (end of straight, start of corner)
            [3.5, 0.5],  # D (end of corner)
        ]
    )
    labels = ["A", "B", "C", "D"]
    seg_lengths = np.array(
        [np.linalg.norm(waypoints[i + 1] - waypoints[i]) for i in range(3)]
    )
    total_len = seg_lengths.sum()

    # Road path (polyline)
    def draw_path_and_waypoints(ax):
        ax.plot(
            waypoints[:, 0],
            waypoints[:, 1],
            color=STYLE["text_dim"],
            lw=1.5,
            ls="--",
            alpha=0.4,
            zorder=1,
        )
        for i, (wp, lbl) in enumerate(zip(waypoints, labels, strict=True)):
            ax.plot(*wp, "o", color=STYLE["warn"], ms=7, zorder=6)
            oy = 0.5 if i < 2 else -0.5
            ax.text(
                wp[0],
                wp[1] + oy,
                lbl,
                color=STYLE["warn"],
                fontsize=10,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=7,
            )

    def point_on_path(dist):
        """Return (x, y) at the given distance along the polyline."""
        d = 0.0
        for i in range(len(waypoints) - 1):
            seg_len = seg_lengths[i]
            if d + seg_len >= dist:
                t = (dist - d) / seg_len if seg_len > 1e-9 else 0.0
                return waypoints[i] + t * (waypoints[i + 1] - waypoints[i])
            d += seg_len
        return waypoints[-1].copy()

    # Number of sample dots to show
    n_dots = 9

    # -- Left panel: Uniform Parameterization --------------------------------
    setup_axes(ax1, xlim=(-4.5, 4.5), ylim=(-1.5, 5.5), aspect="equal")
    draw_path_and_waypoints(ax1)

    # Uniform: equal parameter intervals → equal fractions of segment count
    # Each segment gets equal parameter range regardless of length
    seg_count = len(waypoints) - 1
    for i in range(n_dots):
        # Uniform parameter: evenly space across the parameter range
        u = i / (n_dots - 1)  # 0..1
        # Map to segment + local t (each segment gets 1/seg_count of parameter)
        seg_param = u * seg_count
        seg_idx = min(int(seg_param), seg_count - 1)
        local_t = seg_param - seg_idx
        pos = waypoints[seg_idx] + local_t * (
            waypoints[seg_idx + 1] - waypoints[seg_idx]
        )
        color = STYLE["accent1"] if i % 2 == 0 else STYLE["accent4"]
        ax1.plot(*pos, "o", color=color, ms=6, zorder=5)

    # Show segment length annotations
    for i in range(seg_count):
        mid = (waypoints[i] + waypoints[i + 1]) / 2
        ax1.text(
            mid[0],
            mid[1] - 0.7,
            f"L={seg_lengths[i]:.1f}",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    # Annotation: bunched on short, spread on long
    ax1.text(
        0.0,
        -0.8,
        "Equal parameter intervals\n→ bunched on short segments,\n  spread on long segments",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax1.set_title(
        "Uniform Parameterization",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # -- Right panel: Arc-Length Parameterization -----------------------------
    setup_axes(ax2, xlim=(-4.5, 4.5), ylim=(-1.5, 5.5), aspect="equal")
    draw_path_and_waypoints(ax2)

    # Arc-length: equal distance intervals along the actual path
    for i in range(n_dots):
        dist = i / (n_dots - 1) * total_len
        pos = point_on_path(dist)
        color = STYLE["accent3"] if i % 2 == 0 else STYLE["accent4"]
        ax2.plot(*pos, "o", color=color, ms=6, zorder=5)

    # Show segment length annotations
    for i in range(seg_count):
        mid = (waypoints[i] + waypoints[i + 1]) / 2
        ax2.text(
            mid[0],
            mid[1] - 0.7,
            f"L={seg_lengths[i]:.1f}",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    # Annotation: evenly spaced
    ax2.text(
        0.0,
        -0.8,
        "Equal distance intervals\n→ evenly spaced regardless\n  of segment length",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax2.set_title(
        "Arc-Length Parameterization",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, "gpu/31-transform-animations", "arc_length_parameterization.png")


# ---------------------------------------------------------------------------
# gpu/32-skinning-animations — joint_matrix_pipeline.png
# ---------------------------------------------------------------------------
