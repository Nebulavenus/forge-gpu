"""Diagrams for physics/02 — Springs and Constraints."""

import matplotlib.lines as mlines
import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — hookes_law.png
# ---------------------------------------------------------------------------


def diagram_hookes_law():
    """Plot F vs displacement for k=10, 50, 100."""
    fig = plt.figure(figsize=(9, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, grid=True, aspect=None)

    x = np.linspace(-2.0, 2.0, 400)
    k_values = [10, 50, 100]
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    for k, color in zip(k_values, colors, strict=True):
        f = -k * x
        ax.plot(x, f, color=color, lw=2.5, label=f"k = {k} N/m")

    ax.axhline(0, color=STYLE["grid"], lw=1, alpha=0.6)
    ax.axvline(0, color=STYLE["grid"], lw=1, alpha=0.6)

    ax.set_xlabel("Displacement x (m)", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Restoring Force F (N)", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Hooke\u2019s Law: F = \u2212kx",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    legend = ax.legend(
        loc="upper left",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    legend.get_frame().set_alpha(0.9)

    fig.tight_layout()
    save(fig, "physics/02-springs-and-constraints", "hookes_law.png")


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — damped_spring_comparison.png
# ---------------------------------------------------------------------------


def diagram_damped_spring_comparison():
    """Compare underdamped, critically-damped, and overdamped spring systems."""
    fig = plt.figure(figsize=(9, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, grid=True, aspect=None)

    m = 1.0
    k = 100.0
    dt = 0.001
    steps = 3000
    t = np.arange(steps) * dt

    cases = [
        (2.0, "Underdamped (b=2)", STYLE["accent1"]),
        (20.0, "Critically damped (b=20)", STYLE["accent2"]),
        (40.0, "Overdamped (b=40)", STYLE["accent3"]),
    ]

    for b, label, color in cases:
        x, v = 1.0, 0.0
        positions = np.empty(steps)
        for i in range(steps):
            positions[i] = x
            a = (-k * x - b * v) / m
            v += a * dt
            x += v * dt
        ax.plot(t, positions, color=color, lw=2, label=label)

    ax.axhline(0, color=STYLE["grid"], lw=1, alpha=0.6)

    ax.set_xlabel("Time (s)", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Displacement (m)", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Damped Spring Response",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    legend = ax.legend(
        loc="upper right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    legend.get_frame().set_alpha(0.9)

    fig.tight_layout()
    save(fig, "physics/02-springs-and-constraints", "damped_spring_comparison.png")


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — spring_damping_components.png
# ---------------------------------------------------------------------------


def diagram_spring_damping_components():
    """Vector diagram: spring force, damping force, and total force on a particle."""
    fig = plt.figure(figsize=(9, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 11), ylim=(-3, 5), grid=False)

    # Two particles connected by a spring
    p_a = np.array([2.0, 1.0])
    p_b = np.array([8.0, 1.0])

    # Draw spring (zigzag)
    n_zigs = 12
    spring_pts_x = np.linspace(p_a[0] + 0.5, p_b[0] - 0.5, n_zigs)
    spring_pts_y = np.ones(n_zigs) * 1.0
    for i in range(1, n_zigs - 1):
        spring_pts_y[i] += 0.4 * ((-1) ** i)
    ax.plot(spring_pts_x, spring_pts_y, color=STYLE["text_dim"], lw=1.5)

    # Particles
    circle_a = mpatches.Circle(
        p_a,
        0.35,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        lw=2,
    )
    circle_b = mpatches.Circle(
        p_b,
        0.35,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["text"],
        lw=2,
    )
    ax.add_patch(circle_a)
    ax.add_patch(circle_b)

    ax.text(
        p_a[0],
        p_a[1] + 0.8,
        "A",
        color=STYLE["accent1"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )
    ax.text(
        p_b[0],
        p_b[1] + 0.8,
        "B",
        color=STYLE["accent2"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Velocity arrow for B (moving right = stretching)
    arrow_kw = {"arrowstyle": "->,head_width=0.25,head_length=0.12"}
    ax.annotate(
        "",
        xy=(p_b[0] + 2.0, p_b[1]),
        xytext=(p_b[0] + 0.5, p_b[1]),
        arrowprops={
            **arrow_kw,
            "color": STYLE["text_dim"],
            "lw": 1.8,
            "linestyle": "--",
        },
    )
    ax.text(
        p_b[0] + 1.5,
        p_b[1] + 0.5,
        "velocity",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # F_spring arrow (on B, pointing toward A)
    ax.annotate(
        "",
        xy=(p_b[0] - 2.5, p_b[1] - 1.5),
        xytext=(p_b[0], p_b[1] - 1.5),
        arrowprops={**arrow_kw, "color": STYLE["accent3"], "lw": 2.5},
    )
    ax.text(
        p_b[0] - 1.3,
        p_b[1] - 2.1,
        "F_spring = \u2212k\u00b7\u0394x",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # F_damp arrow (on B, opposing velocity = pointing left)
    ax.annotate(
        "",
        xy=(p_b[0] - 1.5, p_b[1] - 2.8),
        xytext=(p_b[0], p_b[1] - 2.8),
        arrowprops={**arrow_kw, "color": STYLE["accent4"], "lw": 2.5},
    )
    ax.text(
        p_b[0] - 0.8,
        p_b[1] - 3.4,
        "F_damp = \u2212b\u00b7v_rel\u00b7\u00ea",
        color=STYLE["accent4"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # F_total arrow (on B, sum)
    ax.annotate(
        "",
        xy=(p_b[0] - 3.5, 3.5),
        xytext=(p_b[0], 3.5),
        arrowprops={**arrow_kw, "color": STYLE["warn"], "lw": 3},
    )
    ax.text(
        p_b[0] - 1.8,
        4.1,
        "F_total = F_spring + F_damp",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Spring and Damping Force Components",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "physics/02-springs-and-constraints", "spring_damping_components.png")


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — distance_constraint_projection.png
# ---------------------------------------------------------------------------


def diagram_distance_constraint_projection():
    """Before/after: two particles projected to satisfy a distance constraint."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-2, 4.5), grid=False)

    rest_length = 3.0

    # --- Before (left side) ---
    a_before = np.array([1.5, 2.0])
    b_before = np.array([6.0, 2.0])
    current_dist = np.linalg.norm(b_before - a_before)

    # Dashed circles (original)
    for pt, label, color in [
        (a_before, "A", STYLE["accent1"]),
        (b_before, "B", STYLE["accent2"]),
    ]:
        c = mpatches.Circle(pt, 0.25, fill=False, edgecolor=color, lw=2, linestyle="--")
        ax.add_patch(c)
        ax.text(
            pt[0],
            pt[1] + 0.6,
            label,
            color=color,
            fontsize=12,
            fontweight="bold",
            ha="center",
            path_effects=_STROKE,
        )

    # Dashed line between before
    ax.plot(
        [a_before[0], b_before[0]],
        [a_before[1], b_before[1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.5,
        alpha=0.6,
    )
    ax.text(
        (a_before[0] + b_before[0]) / 2,
        2.7,
        f"d = {current_dist:.1f}",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # --- After (corrected positions) ---
    direction = (b_before - a_before) / current_dist
    error = current_dist - rest_length
    a_after = a_before + direction * (error * 0.5)
    b_after = b_before - direction * (error * 0.5)

    for pt, color in [(a_after, STYLE["accent1"]), (b_after, STYLE["accent2"])]:
        c = mpatches.Circle(
            pt, 0.25, facecolor=color, edgecolor=STYLE["text"], lw=2, alpha=0.9
        )
        ax.add_patch(c)

    # Solid line between after
    ax.plot(
        [a_after[0], b_after[0]],
        [a_after[1], b_after[1]],
        color=STYLE["accent3"],
        lw=2.5,
    )
    ax.text(
        (a_after[0] + b_after[0]) / 2,
        1.3,
        f"rest = {rest_length:.1f}",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Correction arrows
    arrow_kw = {"arrowstyle": "->,head_width=0.2,head_length=0.1", "lw": 2}
    ax.annotate(
        "",
        xy=a_after,
        xytext=a_before,
        arrowprops={**arrow_kw, "color": STYLE["accent1"]},
    )
    ax.annotate(
        "",
        xy=b_after,
        xytext=b_before,
        arrowprops={**arrow_kw, "color": STYLE["accent2"]},
    )

    # Labels
    ax.text(
        0.5,
        4.0,
        "Before",
        color=STYLE["text_dim"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        0.5,
        -1.5,
        "Correction: each moves half the error",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Right side: formula ---
    ax.text(
        8.5,
        3.5,
        "Distance Constraint",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )
    formulas = [
        "\u0394 = |B \u2212 A| \u2212 rest",
        "n = (B \u2212 A) / |B \u2212 A|",
        "A += n \u00b7 \u0394/2",
        "B \u2212= n \u00b7 \u0394/2",
    ]
    for i, f in enumerate(formulas):
        ax.text(
            8.5,
            2.5 - i * 0.7,
            f,
            color=STYLE["text"],
            fontsize=10,
            ha="center",
            fontfamily="monospace",
            path_effects=_STROKE,
        )

    ax.set_title(
        "Distance Constraint Projection",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(
        fig, "physics/02-springs-and-constraints", "distance_constraint_projection.png"
    )


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — constraint_mass_weighting.png
# ---------------------------------------------------------------------------


def diagram_constraint_mass_weighting():
    """Show unequal-mass constraint: lighter particle moves more."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-2, 5), grid=False)

    rest_length = 3.0
    m_a, m_b = 1.0, 3.0
    w_a = 1.0 / m_a
    w_b = 1.0 / m_b
    w_sum = w_a + w_b

    # Before positions (too far apart)
    a_pos = np.array([2.0, 2.0])
    b_pos = np.array([7.0, 2.0])
    dist = np.linalg.norm(b_pos - a_pos)
    direction = (b_pos - a_pos) / dist
    error = dist - rest_length

    # After positions (mass-weighted correction)
    a_after = a_pos + direction * error * (w_a / w_sum)
    b_after = b_pos - direction * error * (w_b / w_sum)

    # Draw before (dashed)
    for pt, label, color, mass in [
        (a_pos, "A", STYLE["accent1"], m_a),
        (b_pos, "B", STYLE["accent2"], m_b),
    ]:
        r = 0.2 + mass * 0.1
        c = mpatches.Circle(pt, r, fill=False, edgecolor=color, lw=2, linestyle="--")
        ax.add_patch(c)
        ax.text(
            pt[0],
            pt[1] + 0.8,
            f"{label} (m={mass:.0f})",
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            path_effects=_STROKE,
        )

    ax.plot(
        [a_pos[0], b_pos[0]],
        [a_pos[1], b_pos[1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.5,
        alpha=0.5,
    )

    # Draw after (solid)
    for pt, color, mass in [
        (a_after, STYLE["accent1"], m_a),
        (b_after, STYLE["accent2"], m_b),
    ]:
        r = 0.2 + mass * 0.1
        c = mpatches.Circle(
            pt, r, facecolor=color, edgecolor=STYLE["text"], lw=2, alpha=0.9
        )
        ax.add_patch(c)

    ax.plot(
        [a_after[0], b_after[0]],
        [a_after[1], b_after[1]],
        color=STYLE["accent3"],
        lw=2.5,
    )

    # Correction arrows
    arrow_kw = {"arrowstyle": "->,head_width=0.2,head_length=0.1", "lw": 2.5}
    ax.annotate(
        "", xy=a_after, xytext=a_pos, arrowprops={**arrow_kw, "color": STYLE["accent1"]}
    )
    ax.annotate(
        "", xy=b_after, xytext=b_pos, arrowprops={**arrow_kw, "color": STYLE["accent2"]}
    )

    # Labels for correction magnitude
    a_move = np.linalg.norm(a_after - a_pos)
    b_move = np.linalg.norm(b_after - b_pos)
    ax.text(
        a_pos[0] + (a_after[0] - a_pos[0]) / 2,
        1.0,
        f"\u0394A = {a_move:.2f}",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )
    ax.text(
        b_pos[0] + (b_after[0] - b_pos[0]) / 2,
        1.0,
        f"\u0394B = {b_move:.2f}",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Key insight
    ax.text(
        5.0,
        4.2,
        "Lighter particle moves more",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )
    ax.text(
        5.0,
        -1.2,
        "weight_i = 1/m_i    correction_i = \u0394 \u00b7 w_i / (w_a + w_b)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        fontfamily="monospace",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Mass-Weighted Constraint Projection",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "physics/02-springs-and-constraints", "constraint_mass_weighting.png")


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — gauss_seidel_convergence.png
# ---------------------------------------------------------------------------


def diagram_gauss_seidel_convergence():
    """Plot constraint error vs iteration count for a chain of 5 particles."""
    fig = plt.figure(figsize=(9, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, grid=True, aspect=None)

    # Chain of 5 particles, rest length = 1.0 between each
    n_particles = 5
    rest_length = 1.0
    max_iterations = 20

    # Initial positions: spread out unevenly (not satisfying constraints)
    positions = np.array([0.0, 2.5, 3.0, 5.5, 7.0])

    errors = []
    for _ in range(max_iterations):
        # Compute total distance error
        total_error = 0.0
        for i in range(n_particles - 1):
            d = abs(positions[i + 1] - positions[i])
            total_error += abs(d - rest_length)
        errors.append(total_error)

        # Gauss-Seidel: project each constraint sequentially
        for i in range(n_particles - 1):
            diff = positions[i + 1] - positions[i]
            d = abs(diff)
            if d < 1e-8:
                continue
            direction = diff / d
            err = d - rest_length
            positions[i] += direction * err * 0.5
            positions[i + 1] -= direction * err * 0.5

    iters = np.arange(max_iterations)
    ax.plot(
        iters,
        errors,
        color=STYLE["accent1"],
        lw=2.5,
        marker="o",
        markersize=5,
        markerfacecolor=STYLE["accent1"],
        markeredgecolor=STYLE["text"],
    )

    ax.set_xlabel("Iteration", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Total Constraint Error", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Gauss\u2013Seidel Convergence (5-Particle Chain)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    # Annotation
    ax.text(
        max_iterations * 0.5,
        errors[0] * 0.6,
        "Error decreases\neach iteration",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    fig.tight_layout()
    save(fig, "physics/02-springs-and-constraints", "gauss_seidel_convergence.png")


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — cloth_topology.png
# ---------------------------------------------------------------------------


def diagram_cloth_topology():
    """5x5 cloth grid: structural (H+V) and shear (diagonal) springs."""
    fig = plt.figure(figsize=(8, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.8, 4.8), ylim=(-0.8, 4.8), grid=False)

    rows, cols = 5, 5

    # Draw shear springs (diagonals) first — behind structural
    for r in range(rows - 1):
        for c in range(cols - 1):
            ax.plot([c, c + 1], [r, r + 1], color=STYLE["accent2"], lw=1.2, alpha=0.6)
            ax.plot([c + 1, c], [r, r + 1], color=STYLE["accent2"], lw=1.2, alpha=0.6)

    # Draw structural springs (horizontal + vertical)
    for r in range(rows):
        for c in range(cols):
            # Horizontal
            if c < cols - 1:
                ax.plot([c, c + 1], [r, r], color=STYLE["accent1"], lw=2, alpha=0.9)
            # Vertical
            if r < rows - 1:
                ax.plot([c, c], [r, r + 1], color=STYLE["accent1"], lw=2, alpha=0.9)

    # Draw nodes
    for r in range(rows):
        for c in range(cols):
            ax.plot(c, r, "o", color=STYLE["text"], markersize=8, zorder=5)

    # Pinned top row markers
    for c in range(cols):
        ax.plot(
            c,
            rows - 1,
            "s",
            color=STYLE["warn"],
            markersize=10,
            zorder=6,
            markeredgecolor=STYLE["text"],
            markeredgewidth=1.5,
        )

    # Legend entries
    legend_elements = [
        mlines.Line2D(
            [0], [0], color=STYLE["accent1"], lw=2.5, label="Structural (H + V)"
        ),
        mlines.Line2D(
            [0],
            [0],
            color=STYLE["accent2"],
            lw=1.5,
            alpha=0.7,
            label="Shear (diagonal)",
        ),
        mlines.Line2D(
            [0],
            [0],
            color=STYLE["warn"],
            marker="s",
            lw=0,
            markersize=8,
            label="Pinned nodes",
        ),
    ]
    legend = ax.legend(
        handles=legend_elements,
        loc="lower right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    legend.get_frame().set_alpha(0.9)

    ax.set_title(
        "Cloth Spring Topology (5\u00d75 Grid)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "physics/02-springs-and-constraints", "cloth_topology.png")


# ---------------------------------------------------------------------------
# physics/02-springs-and-constraints — spring_vs_constraint.png
# ---------------------------------------------------------------------------


def diagram_spring_vs_constraint():
    """Compare spring oscillation vs rigid constraint over 200 steps."""
    fig = plt.figure(figsize=(9, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, grid=True, aspect=None)

    dt = 1.0 / 60.0
    steps = 200
    rest = 2.0

    # --- Spring simulation (two particles, 1D) ---
    # A fixed at 0, B starts at 3.0 (stretched)
    k = 200.0
    b_damp = 2.0
    b_pos = 3.0
    b_vel = 0.0
    spring_disp = np.empty(steps)
    for i in range(steps):
        spring_disp[i] = b_pos - rest
        f = -k * (b_pos - rest) - b_damp * b_vel
        b_vel += f * dt
        b_pos += b_vel * dt

    # --- Constraint simulation (position-based, Gauss-Seidel) ---
    # A fixed at 0, B starts at 3.0
    c_pos = 3.0
    c_vel = 0.0
    constraint_disp = np.empty(steps)
    for i in range(steps):
        # Apply velocity (no force, just inertia)
        c_pos += c_vel * dt
        # Project constraint: snap to rest length
        error = c_pos - rest
        c_pos -= error  # Full correction (A is fixed)
        # Update velocity to reflect the correction
        c_vel = 0.0  # Constraint kills overshoot
        constraint_disp[i] = c_pos - rest

    t = np.arange(steps)
    ax.plot(t, spring_disp, color=STYLE["accent1"], lw=2.5, label="Spring (k=200, b=2)")
    ax.plot(
        t, constraint_disp, color=STYLE["accent2"], lw=2.5, label="Rigid constraint"
    )
    ax.axhline(0, color=STYLE["grid"], lw=1, alpha=0.6)

    ax.set_xlabel("Time step", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Displacement from rest length", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Spring Oscillation vs Rigid Constraint",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    # Annotations
    ax.text(
        steps * 0.4,
        spring_disp[20] * 0.8,
        "Oscillates",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        steps * 0.4,
        0.15,
        "Instantly satisfied",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    legend = ax.legend(
        loc="upper right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    legend.get_frame().set_alpha(0.9)

    fig.tight_layout()
    save(fig, "physics/02-springs-and-constraints", "spring_vs_constraint.png")
