"""Diagrams for physics/03 — Particle Collisions."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


# ---------------------------------------------------------------------------
# physics/03-particle-collisions — sphere_sphere_collision.png
# ---------------------------------------------------------------------------


def diagram_sphere_sphere_collision():
    """Two overlapping spheres with contact normal, penetration depth, and contact point."""
    fig = plt.figure(figsize=(10, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 11), ylim=(-2, 6), grid=False)

    # Particle centers and radii
    center_a = np.array([3.5, 2.5])
    center_b = np.array([6.5, 2.5])
    r_a = 2.0
    r_b = 1.5

    dist = np.linalg.norm(center_b - center_a)
    penetration = (r_a + r_b) - dist
    normal = (center_a - center_b) / dist  # B toward A
    # Contact point sits at the midpoint of the overlap region along the axis
    contact_pt = center_a + (center_b - center_a) / dist * (r_a - penetration * 0.5)

    # Draw particle circles
    circle_a = mpatches.Circle(
        center_a,
        r_a,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        lw=2,
        alpha=0.25,
    )
    circle_b = mpatches.Circle(
        center_b,
        r_b,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["text"],
        lw=2,
        alpha=0.25,
    )
    ax.add_patch(circle_a)
    ax.add_patch(circle_b)

    # Draw circle outlines more visibly
    outline_a = mpatches.Circle(
        center_a, r_a, fill=False, edgecolor=STYLE["accent1"], lw=2.5
    )
    outline_b = mpatches.Circle(
        center_b, r_b, fill=False, edgecolor=STYLE["accent2"], lw=2.5
    )
    ax.add_patch(outline_a)
    ax.add_patch(outline_b)

    # Center dots
    ax.plot(*center_a, "o", color=STYLE["accent1"], markersize=6, zorder=5)
    ax.plot(*center_b, "o", color=STYLE["accent2"], markersize=6, zorder=5)

    # Labels A and B
    ax.text(
        center_a[0],
        center_a[1] + r_a + 0.4,
        "A",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )
    ax.text(
        center_b[0],
        center_b[1] + r_b + 0.4,
        "B",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Radius lines
    # r_a: from center_a upward-left
    r_a_end = center_a + np.array([-r_a * 0.707, r_a * 0.707])
    ax.plot(
        [center_a[0], r_a_end[0]],
        [center_a[1], r_a_end[1]],
        color=STYLE["accent1"],
        lw=1.5,
        linestyle="--",
    )
    ax.text(
        (center_a[0] + r_a_end[0]) / 2 - 0.3,
        (center_a[1] + r_a_end[1]) / 2 + 0.2,
        f"r\u2090 = {r_a:.1f}",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # r_b: from center_b upward-right
    r_b_end = center_b + np.array([r_b * 0.707, r_b * 0.707])
    ax.plot(
        [center_b[0], r_b_end[0]],
        [center_b[1], r_b_end[1]],
        color=STYLE["accent2"],
        lw=1.5,
        linestyle="--",
    )
    ax.text(
        (center_b[0] + r_b_end[0]) / 2 + 0.3,
        (center_b[1] + r_b_end[1]) / 2 + 0.2,
        f"r\u1d47 = {r_b:.1f}",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Distance line (below centers)
    y_dist = center_a[1] - 0.3
    ax.annotate(
        "",
        xy=(center_b[0], y_dist),
        xytext=(center_a[0], y_dist),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )
    ax.text(
        (center_a[0] + center_b[0]) / 2,
        y_dist - 0.5,
        f"d = {dist:.1f}",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # Contact normal arrow (from B toward A)
    arrow_kw = {"arrowstyle": "->,head_width=0.25,head_length=0.12"}
    normal_start = contact_pt
    normal_end = contact_pt + normal * 1.8
    ax.annotate(
        "",
        xy=normal_end,
        xytext=normal_start,
        arrowprops={**arrow_kw, "color": STYLE["accent3"], "lw": 2.5},
    )
    ax.text(
        normal_end[0] - 0.2,
        normal_end[1] + 0.5,
        "n (contact normal)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Contact point dot
    ax.plot(*contact_pt, "o", color=STYLE["warn"], markersize=10, zorder=6)
    ax.text(
        contact_pt[0] + 0.1,
        contact_pt[1] - 0.7,
        "contact point",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Penetration depth annotation
    y_pen = center_a[1] + r_a + 0.9
    ax.annotate(
        "",
        xy=(center_b[0] - r_b, y_pen),
        xytext=(center_a[0] + r_a, y_pen),
        arrowprops={"arrowstyle": "<->", "color": STYLE["warn"], "lw": 2},
    )
    ax.text(
        (center_a[0] + r_a + center_b[0] - r_b) / 2,
        y_pen + 0.4,
        f"penetration = {penetration:.1f}",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Sphere\u2013Sphere Collision Detection",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "physics/03-particle-collisions", "sphere_sphere_collision.png")


# ---------------------------------------------------------------------------
# physics/03-particle-collisions — impulse_response.png
# ---------------------------------------------------------------------------


def diagram_impulse_response():
    """Before/after collision showing velocity changes and impulse formula."""
    fig = plt.figure(figsize=(12, 5), facecolor=STYLE["bg"])

    arrow_kw = {"arrowstyle": "->,head_width=0.25,head_length=0.12"}

    # --- Before panel (left) ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-1, 9), ylim=(-2, 5), grid=False)

    ca = np.array([2.5, 2.0])
    cb = np.array([6.0, 2.0])
    r = 0.8

    # Particles
    for center, color, label in [
        (ca, STYLE["accent1"], "A"),
        (cb, STYLE["accent2"], "B"),
    ]:
        c = mpatches.Circle(
            center, r, facecolor=color, edgecolor=STYLE["text"], lw=2, alpha=0.35
        )
        ax1.add_patch(c)
        ax1.add_patch(mpatches.Circle(center, r, fill=False, edgecolor=color, lw=2))
        ax1.text(
            center[0],
            center[1],
            label,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

    # Velocity arrows before
    ax1.annotate(
        "",
        xy=(ca[0] + 2.5, ca[1] + 0.5),
        xytext=(ca[0] + r + 0.1, ca[1] + 0.2),
        arrowprops={**arrow_kw, "color": STYLE["accent1"], "lw": 2.5},
    )
    ax1.text(
        ca[0] + 2.0,
        ca[1] + 1.2,
        "v\u2090",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax1.annotate(
        "",
        xy=(cb[0] - 2.0, cb[1] - 0.3),
        xytext=(cb[0] - r - 0.1, cb[1] - 0.1),
        arrowprops={**arrow_kw, "color": STYLE["accent2"], "lw": 2.5},
    )
    ax1.text(
        cb[0] - 2.0,
        cb[1] - 1.0,
        "v\u1d47",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax1.set_title(
        "Before Collision",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # --- After panel (right) ---
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-1, 9), ylim=(-2, 5), grid=False)

    # Particles (same positions, post-collision)
    for center, color, label in [
        (ca, STYLE["accent1"], "A"),
        (cb, STYLE["accent2"], "B"),
    ]:
        c = mpatches.Circle(
            center, r, facecolor=color, edgecolor=STYLE["text"], lw=2, alpha=0.35
        )
        ax2.add_patch(c)
        ax2.add_patch(mpatches.Circle(center, r, fill=False, edgecolor=color, lw=2))
        ax2.text(
            center[0],
            center[1],
            label,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

    # Velocity arrows after (reversed/modified)
    ax2.annotate(
        "",
        xy=(ca[0] - 1.5, ca[1] + 0.3),
        xytext=(ca[0] - r - 0.1, ca[1] + 0.1),
        arrowprops={**arrow_kw, "color": STYLE["accent1"], "lw": 2.5},
    )
    ax2.text(
        ca[0] - 1.8,
        ca[1] + 1.0,
        "v\u2090\u2032",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax2.annotate(
        "",
        xy=(cb[0] + 2.5, cb[1] - 0.2),
        xytext=(cb[0] + r + 0.1, cb[1] - 0.1),
        arrowprops={**arrow_kw, "color": STYLE["accent2"], "lw": 2.5},
    )
    ax2.text(
        cb[0] + 2.2,
        cb[1] - 0.9,
        "v\u1d47\u2032",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax2.set_title(
        "After Collision",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # Impulse formula centered below both panels
    fig.text(
        0.5,
        0.02,
        "j = \u2212(1 + e) \u00b7 v_closing  /  (1/m\u2090 + 1/m\u1d47)",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="bottom",
        fontfamily="monospace",
        path_effects=_STROKE,
    )

    fig.tight_layout(rect=[0, 0.08, 1, 1])
    save(fig, "physics/03-particle-collisions", "impulse_response.png")


# ---------------------------------------------------------------------------
# physics/03-particle-collisions — restitution_comparison.png
# ---------------------------------------------------------------------------


def diagram_restitution_comparison():
    """Three panels comparing e=0, e=0.5, e=1.0 collision outcomes."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 5), facecolor=STYLE["bg"])

    arrow_kw = {"arrowstyle": "->,head_width=0.2,head_length=0.1"}
    r = 0.6

    e_values = [0.0, 0.5, 1.0]
    labels = [
        "Perfectly Inelastic",
        "Partial Bounce",
        "Perfectly Elastic",
    ]

    # Incoming velocities (1D along x)
    v_a_in = 3.0
    v_b_in = -1.0
    m_a, m_b = 1.0, 1.0

    for ax, e, label in zip(axes, e_values, labels, strict=True):
        setup_axes(ax, xlim=(-1, 9), ylim=(-3, 4), grid=False)

        ca = np.array([2.5, 1.5])
        cb = np.array([5.5, 1.5])

        # Compute post-collision velocities
        v_closing = v_a_in - v_b_in
        j = -(1 + e) * v_closing / (1 / m_a + 1 / m_b)
        v_a_out = v_a_in + j / m_a
        v_b_out = v_b_in - j / m_b

        # --- Before (dim, upper region) ---
        y_before = 2.8
        for center_x, color in [(ca[0], STYLE["accent1"]), (cb[0], STYLE["accent2"])]:
            c = mpatches.Circle(
                (center_x, y_before),
                r * 0.7,
                fill=False,
                edgecolor=color,
                lw=1.5,
                linestyle="--",
                alpha=0.5,
            )
            ax.add_patch(c)

        # Before velocity arrows (dim)
        v_scale = 0.5
        ax.annotate(
            "",
            xy=(ca[0] + v_a_in * v_scale, y_before),
            xytext=(ca[0] + r * 0.7, y_before),
            arrowprops={**arrow_kw, "color": STYLE["accent1"], "lw": 1.5, "alpha": 0.5},
        )
        ax.annotate(
            "",
            xy=(cb[0] + v_b_in * v_scale, y_before),
            xytext=(cb[0] - r * 0.7, y_before),
            arrowprops={**arrow_kw, "color": STYLE["accent2"], "lw": 1.5, "alpha": 0.5},
        )

        ax.text(
            4.0,
            3.7,
            "before",
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            path_effects=_STROKE,
        )

        # --- After (solid, lower region) ---
        y_after = 0.0
        for center_x, color in [(ca[0], STYLE["accent1"]), (cb[0], STYLE["accent2"])]:
            c = mpatches.Circle(
                (center_x, y_after),
                r,
                facecolor=color,
                edgecolor=STYLE["text"],
                lw=2,
                alpha=0.35,
            )
            ax.add_patch(c)
            ax.add_patch(
                mpatches.Circle(
                    (center_x, y_after), r, fill=False, edgecolor=color, lw=2
                )
            )

        # After velocity arrows
        if abs(v_a_out) > 0.05:
            ax.annotate(
                "",
                xy=(ca[0] + v_a_out * v_scale, y_after),
                xytext=(ca[0] + (r if v_a_out > 0 else -r), y_after),
                arrowprops={**arrow_kw, "color": STYLE["accent1"], "lw": 2.5},
            )
        if abs(v_b_out) > 0.05:
            ax.annotate(
                "",
                xy=(cb[0] + v_b_out * v_scale, y_after),
                xytext=(cb[0] + (r if v_b_out > 0 else -r), y_after),
                arrowprops={**arrow_kw, "color": STYLE["accent2"], "lw": 2.5},
            )

        ax.text(
            4.0,
            -1.3,
            "after",
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            fontweight="bold",
            path_effects=_STROKE,
        )

        # Velocity values
        ax.text(
            4.0,
            -2.2,
            f"v\u2090\u2032={v_a_out:+.1f}  v\u1d47\u2032={v_b_out:+.1f}",
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            fontfamily="monospace",
            path_effects=_STROKE,
        )

        ax.set_title(
            f"e = {e:.1f}\n{label}",
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=12,
        )

    fig.tight_layout()
    save(fig, "physics/03-particle-collisions", "restitution_comparison.png")


# ---------------------------------------------------------------------------
# physics/03-particle-collisions — collision_pipeline.png
# ---------------------------------------------------------------------------


def diagram_collision_pipeline():
    """Horizontal flowchart showing the four collision pipeline phases."""
    fig = plt.figure(figsize=(14, 3.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 10), ylim=(-1.5, 2.5), grid=False)
    ax.set_axis_off()

    # Four phases matching the README description
    steps = [
        "All-Pairs\nDetection\n(O(n\u00b2))",
        "Contact\nGeneration",
        "Impulse\nResolution",
        "Ground Plane\nCollision",
    ]

    colors = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
        STYLE["accent4"],
    ]

    box_w = 2.0
    box_h = 1.4
    gap = 0.6
    y_center = 0.5

    for i, (step, color) in enumerate(zip(steps, colors, strict=True)):
        x = i * (box_w + gap)

        # Rounded rectangle
        rect = mpatches.FancyBboxPatch(
            (x, y_center - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            facecolor=color,
            edgecolor=STYLE["text"],
            lw=2,
            alpha=0.25,
        )
        ax.add_patch(rect)

        # Border with full opacity
        border = mpatches.FancyBboxPatch(
            (x, y_center - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            fill=False,
            edgecolor=color,
            lw=2,
        )
        ax.add_patch(border)

        # Step label
        ax.text(
            x + box_w / 2,
            y_center,
            step,
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

        # Phase number
        ax.text(
            x + box_w / 2,
            y_center - box_h / 2 - 0.35,
            f"Phase {i + 1}",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            path_effects=_STROKE,
        )

        # Arrow to next box
        if i < len(steps) - 1:
            arrow_x = x + box_w + 0.05
            ax.annotate(
                "",
                xy=(arrow_x + gap - 0.1, y_center),
                xytext=(arrow_x, y_center),
                arrowprops={
                    "arrowstyle": "->,head_width=0.2,head_length=0.1",
                    "color": STYLE["warn"],
                    "lw": 2,
                },
            )

    ax.set_title(
        "Collision Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "physics/03-particle-collisions", "collision_pipeline.png")


# ---------------------------------------------------------------------------
# physics/03-particle-collisions — momentum_conservation.png
# ---------------------------------------------------------------------------


def diagram_momentum_conservation():
    """Bar chart showing momentum conservation: total before = total after."""
    fig = plt.figure(figsize=(10, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, grid=True, aspect=None)

    # Collision parameters
    m_a, m_b = 2.0, 1.0
    v_a_before, v_b_before = 3.0, -1.0
    e = 0.8

    # Compute post-collision velocities
    v_closing = v_a_before - v_b_before
    j = -(1 + e) * v_closing / (1 / m_a + 1 / m_b)
    v_a_after = v_a_before + j / m_a
    v_b_after = v_b_before - j / m_b

    # Momenta
    p_a_before = m_a * v_a_before
    p_b_before = m_b * v_b_before
    p_total_before = p_a_before + p_b_before

    p_a_after = m_a * v_a_after
    p_b_after = m_b * v_b_after
    p_total_after = p_a_after + p_b_after

    # Bar positions
    x_before = np.array([0.5, 1.5, 2.8])
    x_after = np.array([4.5, 5.5, 6.8])
    bar_width = 0.7

    # Before bars
    before_vals = [p_a_before, p_b_before, p_total_before]
    before_colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]
    before_labels = ["p\u2090", "p\u1d47", "p_total"]

    bars1 = ax.bar(
        x_before,
        before_vals,
        bar_width,
        color=before_colors,
        edgecolor=STYLE["text"],
        lw=1.5,
        alpha=0.8,
    )

    # After bars
    after_vals = [p_a_after, p_b_after, p_total_after]
    after_colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]
    after_labels = ["p\u2090\u2032", "p\u1d47\u2032", "p_total\u2032"]

    bars2 = ax.bar(
        x_after,
        after_vals,
        bar_width,
        color=after_colors,
        edgecolor=STYLE["text"],
        lw=1.5,
        alpha=0.8,
    )

    # Value labels on bars
    for bars, labels in [(bars1, before_labels), (bars2, after_labels)]:
        for bar, label in zip(bars, labels, strict=True):
            height = bar.get_height()
            y_pos = height + 0.15 if height >= 0 else height - 0.4
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                y_pos,
                f"{label}\n{height:.1f}",
                color=STYLE["text"],
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="bottom" if height >= 0 else "top",
                path_effects=_STROKE,
            )

    # Group labels
    ax.text(
        1.5,
        -1.8,
        "Before",
        color=STYLE["text_dim"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )
    ax.text(
        5.5,
        -1.8,
        "After",
        color=STYLE["text_dim"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Horizontal line connecting totals
    ax.plot(
        [2.8, 6.8],
        [p_total_before + 0.8, p_total_after + 0.8],
        "--",
        color=STYLE["warn"],
        lw=2,
    )
    ax.text(
        4.8,
        p_total_before + 1.2,
        "conserved",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Zero line
    ax.axhline(0, color=STYLE["grid"], lw=1, alpha=0.6)

    # Clean up x-axis
    ax.set_xticks([])
    ax.set_ylabel("Momentum (kg\u00b7m/s)", color=STYLE["axis"], fontsize=11)

    ax.set_title(
        "Momentum Conservation: p_before = p_after",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    # Parameters note
    ax.text(
        3.8,
        -2.5,
        f"m\u2090={m_a:.0f}  m\u1d47={m_b:.0f}  e={e}",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        fontfamily="monospace",
        path_effects=_STROKE,
    )

    fig.tight_layout()
    save(fig, "physics/03-particle-collisions", "momentum_conservation.png")
